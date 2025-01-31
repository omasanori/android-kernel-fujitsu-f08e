/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011-2013
/*----------------------------------------------------------------------------*/

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include "logger.h"

#include <asm/ioctls.h>
/* FUJITSU:2013-01-10 TOK LOG add start */
#include <linux/syscalls.h>
/* FUJITSU:2013-01-10 TOK LOG add end   */
#ifdef CONFIG_KERNEL_LOG        /* FUJITSU:2013-03-19 KERNEL LOG added. */
#include <linux/seqlock.h>
#include <linux/spinlock.h>
#define AID_ROOT             0  /* traditional unix root user */
#define AID_SYSTEM        1000  /* system server */
#endif

/*
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char		*buffer;/* the ring buffer itself */
	struct miscdevice	misc;	/* misc device representing the log */
	wait_queue_head_t	wq;	/* wait queue for readers */
	struct list_head	readers; /* this log's readers */
	struct mutex		mutex;	/* mutex protecting buffer */
	size_t			w_off;	/* current write head offset */
	size_t			head;	/* new readers start here */
	size_t			size;	/* size of the log */
};

/*
 * struct logger_reader - a logging device open for reading
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;	/* associated log */
	struct list_head	list;	/* entry in logger_log's list */
	size_t			r_off;	/* current read head offset */
	bool			r_all;	/* reader can read all entries */
	int			r_ver;	/* reader ABI version */
};

/* FUJITSU:2013-01-10 sequential log add start */
#ifdef CONFIG_SEQUENTIAL_LOG
#include <asm/siginfo.h>
#include <linux/signal.h>
extern int send_sig(int sig, struct task_struct *p, int priv);
static pid_t user_pid = -1;

#define check_wp(log, tmp)  (tmp > (log)->w_off)
#endif

/* Log type determination macro */
#define is_buff_kind_of(logger_log, buff_name) (strncmp(&((logger_log)->misc.name[4]), (#buff_name), (sizeof(#buff_name)-1) ) == 0)

/* FUJITSU:2013-01-10 sequential log add end   */
/* logger_offset - returns index 'n' into the log via (optimized) modulus */
size_t logger_offset(struct logger_log *log, size_t n)
{
	return n & (log->size-1);
}


/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 *	1) Need to quickly obtain the associated log during an I/O operation
 *	2) Readers need to maintain state (logger_reader)
 *	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		return reader->log;
	} else
		return file->private_data;
}

/*
 * get_entry_header - returns a pointer to the logger_entry header within
 * 'log' starting at offset 'off'. A temporary logger_entry 'scratch' must
 * be provided. Typically the return value will be a pointer within
 * 'logger->buf'.  However, a pointer to 'scratch' may be returned if
 * the log entry spans the end and beginning of the circular buffer.
 */
static struct logger_entry *get_entry_header(struct logger_log *log,
		size_t off, struct logger_entry *scratch)
{
	size_t len = min(sizeof(struct logger_entry), log->size - off);
	if (len != sizeof(struct logger_entry)) {
		memcpy(((void *) scratch), log->buffer + off, len);
		memcpy(((void *) scratch) + len, log->buffer,
			sizeof(struct logger_entry) - len);
		return scratch;
	}

	return (struct logger_entry *) (log->buffer + off);
}

/*
 * get_entry_msg_len - Grabs the length of the message of the entry
 * starting from from 'off'.
 *
 * An entry length is 2 bytes (16 bits) in host endian order.
 * In the log, the length does not include the size of the log entry structure.
 * This function returns the size including the log entry structure.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_msg_len(struct logger_log *log, size_t off)
{
	struct logger_entry scratch;
	struct logger_entry *entry;

	entry = get_entry_header(log, off, &scratch);
	return entry->len;
}

static size_t get_user_hdr_len(int ver)
{
	if (ver < 2)
		return sizeof(struct user_logger_entry_compat);
	else
		return sizeof(struct logger_entry);
}

static ssize_t copy_header_to_user(int ver, struct logger_entry *entry,
					 char __user *buf)
{
	void *hdr;
	size_t hdr_len;
	struct user_logger_entry_compat v1;

	if (ver < 2) {
		v1.len      = entry->len;
		v1.__pad    = 0;
		v1.pid      = entry->pid;
		v1.tid      = entry->tid;
		v1.sec      = entry->sec;
		v1.nsec     = entry->nsec;
		hdr         = &v1;
		hdr_len     = sizeof(struct user_logger_entry_compat);
	} else {
		hdr         = entry;
		hdr_len     = sizeof(struct logger_entry);
	}

	return copy_to_user(buf, hdr, hdr_len);
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	struct logger_entry scratch;
	struct logger_entry *entry;
	size_t len;
	size_t msg_start;

	/*
	 * First, copy the header to userspace, using the version of
	 * the header requested
	 */
	entry = get_entry_header(log, reader->r_off, &scratch);
	if (copy_header_to_user(reader->r_ver, entry, buf))
		return -EFAULT;

	count -= get_user_hdr_len(reader->r_ver);
	buf += get_user_hdr_len(reader->r_ver);
	msg_start = logger_offset(log,
		reader->r_off + sizeof(struct logger_entry));

	/*
	 * We read from the msg in two disjoint operations. First, we read from
	 * the current msg head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - msg_start);
	if (copy_to_user(buf, log->buffer + msg_start, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(log, reader->r_off +
		sizeof(struct logger_entry) + count);

	return count + get_user_hdr_len(reader->r_ver);
}

/*
 * get_next_entry_by_uid - Starting at 'off', returns an offset into
 * 'log->buffer' which contains the first entry readable by 'euid'
 */
static size_t get_next_entry_by_uid(struct logger_log *log,
		size_t off, uid_t euid)
{
	while (off != log->w_off) {
		struct logger_entry *entry;
		struct logger_entry scratch;
		size_t next_len;

		entry = get_entry_header(log, off, &scratch);

#ifdef CONFIG_FTA
		if ((entry->euid & 0x0000ffff) == euid) /* FUJITSU:2012-07-19 FTA mod */
#else
		if (entry->euid == euid)
#endif /* CONFIG_FTA */
			return off;

		next_len = sizeof(struct logger_entry) + entry->len;
		off = logger_offset(log, off + next_len);
	}

	return off;
}

/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 *	- O_NONBLOCK works
 *	- If there are no log entries to read, blocks until log is written to
 *	- Atomically reads exactly one log entry
 *
 * Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);

start:
	while (1) {
		mutex_lock(&log->mutex);

		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		ret = (log->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

	mutex_lock(&log->mutex);

	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		mutex_unlock(&log->mutex);
		goto start;
	}

	/* get the size of the next entry */
	ret = get_user_hdr_len(reader->r_ver) +
		get_entry_msg_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

out:
	mutex_unlock(&log->mutex);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = sizeof(struct logger_entry) +
			get_entry_msg_len(log, off);
		off = logger_offset(log, off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * is_between - is a < c < b, accounting for wrapping of a, b, and c
 *    positions in the buffer
 *
 * That is, if a<b, check for c between a and b
 * and if a>b, check for c outside (not between) a and b
 *
 * |------- a xxxxxxxx b --------|
 *               c^
 *
 * |xxxxx b --------- a xxxxxxxxx|
 *    c^
 *  or                    c^
 */
static inline int is_between(size_t a, size_t b, size_t c)
{
	if (a < b) {
		/* is c between a and b? */
		if (a < c && c <= b)
			return 1;
	} else {
		/* is c outside of b through a? */
		if (c <= b || a < c)
			return 1;
	}

	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(log, old + len);
	struct logger_reader *reader;

	if (is_between(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
		if (is_between(old, new, reader->r_off))
			reader->r_off = get_next_entry(log, reader->r_off, len);
}

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->mutex.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

	log->w_off = logger_offset(log, log->w_off + count);

}

/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->mutex.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log,
				      const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			/*
			 * Note that by not updating w_off, this abandons the
			 * portion of the new entry that *was* successfully
			 * copied, just above.  This is intentional to avoid
			 * message corruption from missing fragments.
			 */
			return -EFAULT;

	log->w_off = logger_offset(log, log->w_off + count);

	return count;
}

#ifdef CONFIG_FTA
/* FUJITSU:2011-09-05 FTA add start */
extern int ftadrv_cpu(void);
/* FUJITSU:2011-09-05 FTA add end */
#endif /* CONFIG_FTA */
/*
 * logger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
#ifdef CONFIG_FTA
	/* FUJITSU:2011-09-05 FTA add start */
	int cpuid = 0;
	/* FUJITSU:2011-09-05 FTA add end */
#endif /* CONFIG_FTA */
	struct logger_log *log = file_get_log(iocb->ki_filp);
	size_t orig = log->w_off;
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;
/* FUJITSU:2013-01-10 sequential log add start */
#ifdef CONFIG_SEQUENTIAL_LOG
    int is_wraparound = 0;
	size_t tmp = 0;
#endif
/* FUJITSU:2013-01-10 sequential log add end   */

#ifdef CONFIG_FTA
	/* FUJITSU:2011-09-05 FTA add start */
	{
		unsigned long flags;
		preempt_disable();
		raw_local_irq_save(flags);
		getnstimeofday(&now);
		cpuid = ftadrv_cpu();
		raw_local_irq_restore(flags);
		preempt_enable();
	}
	/* FUJITSU:2011-09-05 FTA add end */
#else
	now = current_kernel_time();
#endif /* CONFIG_FTA */

	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
#ifdef CONFIG_FTA
	header.euid = (cpuid << 16) | current_euid();
#else
	header.euid = current_euid();
#endif /* CONFIG_FTA */
	header.len = min_t(size_t, iocb->ki_left, LOGGER_ENTRY_MAX_PAYLOAD);
	header.hdr_size = sizeof(struct logger_entry);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	mutex_lock(&log->mutex);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

/* FUJITSU:2013-01-10 sequential log add start */
#ifdef CONFIG_SEQUENTIAL_LOG
    tmp = log->w_off;
#endif
/* FUJITSU:2013-01-10 sequential log add end   */

	do_write_log(log, &header, sizeof(struct logger_entry));

/* FUJITSU:2013-01-10 sequential log add start */
#ifdef CONFIG_SEQUENTIAL_LOG
    is_wraparound |= check_wp(log, tmp);
#endif
/* FUJITSU:2013-01-10 sequential log add end   */

	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

/* FUJITSU:2013-01-10 sequential log add start */
#ifdef CONFIG_SEQUENTIAL_LOG
        tmp = log->w_off;
#endif
/* FUJITSU:2013-01-10 sequential log add end   */

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);

/* FUJITSU:2013-01-10 sequential log add start */
#ifdef CONFIG_SEQUENTIAL_LOG
        is_wraparound |= check_wp(log, tmp);
#endif
/* FUJITSU:2013-01-10 sequential log add end   */

		if (unlikely(nr < 0)) {
			log->w_off = orig;
			mutex_unlock(&log->mutex);
/* FUJITSU:2013-01-10 sequential log add start */
#ifdef CONFIG_SEQUENTIAL_LOG
            ret = nr;
            goto FAIL;
#else
			return nr;
#endif
/* FUJITSU:2013-01-10 sequential log add end   */
		}

		iov++;
		ret += nr;
	}

	mutex_unlock(&log->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

/* FUJITSU:2013-01-10 sequential log add start */
#ifdef CONFIG_SEQUENTIAL_LOG
FAIL:
    /* kick crashdump_app */
    if (is_wraparound) {
        if (is_buff_kind_of(log, main) || is_buff_kind_of(log, system)) {
            if(user_pid > 1) {
                struct task_struct *t;
                rcu_read_lock();
                t = find_task_by_vpid(user_pid);
                rcu_read_unlock();
                if(t != NULL) {
                    send_sig(SIGUSR1, t, 1);
                }
            }
        }
    }
#endif
/* FUJITSU:2013-01-10 sequential log add end   */
	return ret;
}

static struct logger_log *get_log_from_minor(int);

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		reader->r_ver = 1;
		reader->r_all = in_egroup_p(inode->i_gid) ||
			capable(CAP_SYSLOG);

		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		struct logger_log *log = reader->log;

		mutex_lock(&log->mutex);
		list_del(&reader->list);
		mutex_unlock(&log->mutex);

		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	mutex_unlock(&log->mutex);

	return ret;
}

static long logger_set_version(struct logger_reader *reader, void __user *arg)
{
	int version;
	if (copy_from_user(&version, arg, sizeof(int)))
		return -EFAULT;

	if ((version < 1) || (version > 2))
		return -EINVAL;

	reader->r_ver = version;
	return 0;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -EINVAL;
	void __user *argp = (void __user *) arg;

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;

		if (!reader->r_all)
			reader->r_off = get_next_entry_by_uid(log,
				reader->r_off, current_euid());

		if (log->w_off != reader->r_off)
			ret = get_user_hdr_len(reader->r_ver) +
				get_entry_msg_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;
		ret = 0;
		break;
	case LOGGER_GET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = reader->r_ver;
		break;
	case LOGGER_SET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = logger_set_version(reader, argp);
		break;
    /* FUJITSU:2013-01-10 TOK LOG add start */
#ifdef CONFIG_SEQUENTIAL_LOG
	case LOGGER_SET_PID:
		ret = 0;
        if(current_uid() == 0) {
            if(copy_from_user(&user_pid, (char __user *)arg, sizeof(pid_t))) {
                ret= -EFAULT;
            }
        }
		break;
#endif
    /* FUJITSU:2013-01-10 TOK LOG add end   */
	}

	mutex_unlock(&log->mutex);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.aio_write = logger_aio_write,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};
#ifdef CONFIG_KERNEL_LOG        /* FUJITSU:2013-03-19 KERNEL LOG added. */
static DEFINE_RAW_SPINLOCK(printk_spin_lock);
static seqcount_t printk_seq_lock = SEQCNT_ZERO;
int pk_buff_size;

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll_kernel(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;
	unsigned long flags;
	unsigned long seq;
	size_t      tmp_r_off;
    int is_retry = 1;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	do {
		seq = read_seqcount_begin(&printk_seq_lock);
        tmp_r_off = reader->r_off;
        if (!reader->r_all)
            tmp_r_off = get_next_entry_by_uid(log,
                tmp_r_off, current_euid());

        if (log->w_off != tmp_r_off)
            ret |= POLLIN | POLLRDNORM;

        raw_spin_lock_irqsave(&printk_spin_lock, flags);

        if(!read_seqcount_retry(&printk_seq_lock, seq)) {
            reader->r_off = tmp_r_off;
            is_retry = 0;
        }

        raw_spin_unlock_irqrestore(&printk_spin_lock, flags);

	} while (is_retry);
	mutex_unlock(&log->mutex);

	return ret;
}


static ssize_t k_copy_header_to_user(int ver, struct logger_entry *entry,
					 char *buf)
{
	void *hdr;
	size_t hdr_len;
	struct user_logger_entry_compat v1;

	if (ver < 2) {
		v1.len      = entry->len;
		v1.__pad    = 0;
		v1.pid      = entry->pid;
		v1.tid      = entry->tid;
		v1.sec      = entry->sec;
		v1.nsec     = entry->nsec;
		hdr         = &v1;
		hdr_len     = sizeof(struct user_logger_entry_compat);
	} else {
		hdr         = entry;
		hdr_len     = sizeof(struct logger_entry);
	}

    memcpy(buf, hdr, hdr_len);
    return 0;
}

/*
 * pk_do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t pk_do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char *buf,
				   size_t count)
{
	struct logger_entry scratch;
	struct logger_entry *entry;
	size_t len;
	size_t msg_start;

	/*
	 * First, copy the header to userspace, using the version of
	 * the header requested
	 */
	entry = get_entry_header(log, reader->r_off, &scratch);
	if (k_copy_header_to_user(reader->r_ver, entry, buf))
		return -EFAULT;

	count -= get_user_hdr_len(reader->r_ver);
	buf += get_user_hdr_len(reader->r_ver);
	msg_start = logger_offset(log,
		reader->r_off + sizeof(struct logger_entry));

	/*
	 * We read from the msg in two disjoint operations. First, we read from
	 * the current msg head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - msg_start);
    memcpy(buf, log->buffer + msg_start, min_t(size_t, len, pk_buff_size) );

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
        memcpy(buf + len, log->buffer, min_t(size_t, (count-len), pk_buff_size) );

	reader->r_off = logger_offset(log, reader->r_off +
		sizeof(struct logger_entry) + count);

	return count + get_user_hdr_len(reader->r_ver);
}
extern int get_printk_buff_size(void);
static ssize_t logger_read_kernel(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);
	unsigned long flags;
	unsigned long seq;
	size_t tmp_r_off;
    char *tmp_buff = NULL;
    int is_retry = 1;


start:
	while (1) {
		mutex_lock(&log->mutex);

		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

        raw_spin_lock_irqsave(&printk_spin_lock, flags);
		ret = (log->w_off == reader->r_off);
        raw_spin_unlock_irqrestore(&printk_spin_lock, flags);

		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		goto out;

    if (tmp_buff == NULL) {
        tmp_buff = kmalloc(pk_buff_size, GFP_KERNEL);
        if (tmp_buff == NULL) {
            return ret;
        }
    }

	mutex_lock(&log->mutex);
    do {
        seq = read_seqcount_begin(&printk_seq_lock);
        tmp_r_off = reader->r_off;
        if (!reader->r_all)
            tmp_r_off = get_next_entry_by_uid(log,
                tmp_r_off, current_euid());

        /* is there still something to read or did we race? */
        if (unlikely(log->w_off == tmp_r_off)) {
            if (!read_seqcount_retry(&printk_seq_lock, seq)) {
                mutex_unlock(&log->mutex);
                goto start;
            }
            continue;
        }

        /* get the size of the next entry */
        ret = get_user_hdr_len(reader->r_ver) +
            get_entry_msg_len(log, tmp_r_off);
        if (count < ret) {
            if (!read_seqcount_retry(&printk_seq_lock, seq)) {
                ret = -EINVAL;
                mutex_unlock(&log->mutex);
                goto out;
            }
            continue;
        }

        raw_spin_lock_irqsave(&printk_spin_lock, flags);
        if(!read_seqcount_retry(&printk_seq_lock, seq)) {
            /* get exactly one entry from the log */
            ret = pk_do_read_log_to_user(log, reader, tmp_buff, ret);
            is_retry = 0;
        }
        raw_spin_unlock_irqrestore(&printk_spin_lock, flags);
	} while (is_retry);
	mutex_unlock(&log->mutex);
    if (ret > 0) {
        if ( copy_to_user(buf, tmp_buff, min(ret, pk_buff_size)) ) {
			ret =  -EFAULT;
        }
    }
out:
    if (tmp_buff) {
        kfree(tmp_buff);
    }

	return ret;
}

static long logger_ioctl_kernel(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -EINVAL;
	void __user *argp = (void __user *) arg;
	unsigned long flags;
	unsigned long seq;
	size_t      tmp_r_off;
    int is_retry = 1;

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;

        raw_spin_lock_irqsave(&printk_spin_lock, flags);
		if (log->w_off >= reader->r_off) {
			ret = log->w_off - reader->r_off;
        } else {
			ret = (log->size - reader->r_off) + log->w_off;
        }
        raw_spin_unlock_irqrestore(&printk_spin_lock, flags);

		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;

        do {
            seq = read_seqcount_begin(&printk_seq_lock);
            tmp_r_off = reader->r_off;
            if (!reader->r_all)
                tmp_r_off = get_next_entry_by_uid(log,
                    tmp_r_off, current_euid());
            
            if (log->w_off != tmp_r_off) {
                ret = get_user_hdr_len(reader->r_ver) +
                    get_entry_msg_len(log, tmp_r_off);
            } else {
                ret = 0;
            }
            raw_spin_lock_irqsave(&printk_spin_lock, flags);
            if (!read_seqcount_retry(&printk_seq_lock, seq)) {
                reader->r_off = tmp_r_off;
                is_retry = 0;
            }
            raw_spin_unlock_irqrestore(&printk_spin_lock, flags);
        } while (is_retry);

		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
        raw_spin_lock_irqsave(&printk_spin_lock, flags);
		list_for_each_entry(reader, &log->readers, list)
            reader->r_off = log->w_off;
		log->head = log->w_off;
        raw_spin_unlock_irqrestore(&printk_spin_lock, flags);

		ret = 0;
		break;
	case LOGGER_GET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = reader->r_ver;
		break;
	case LOGGER_SET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = logger_set_version(reader, argp);
		break;
	}
	mutex_unlock(&log->mutex);

	return ret;
}

/*
 * logger_open_kernel - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open_kernel(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;
	unsigned long flags;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		reader->r_ver = 1;

        if ((current_euid() == AID_ROOT) || (current_euid() == AID_SYSTEM)) {
            reader->r_all = 1;
        } else {
            kfree(reader);
            return -EPERM;
        }

		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
        raw_spin_lock_irqsave(&printk_spin_lock, flags);

		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);

        raw_spin_unlock_irqrestore(&printk_spin_lock, flags);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release_kernel - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release_kernel(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		struct logger_log *log = reader->log;
        unsigned long flags;

		mutex_lock(&log->mutex);
        raw_spin_lock_irqsave(&printk_spin_lock, flags);

		list_del(&reader->list);

        raw_spin_unlock_irqrestore(&printk_spin_lock, flags);
		mutex_unlock(&log->mutex);

		kfree(reader);
	}

	return 0;
}

static const struct file_operations logger_kernel_fops = {
	.owner = THIS_MODULE,
	.read           = logger_read_kernel,
//	.aio_write = logger_aio_write,
	.poll           = logger_poll_kernel,
	.unlocked_ioctl = logger_ioctl_kernel,
	.compat_ioctl   = logger_ioctl_kernel,
	.open           = logger_open_kernel,
	.release        = logger_release_kernel,
};
#define DEFINE_CONFIG_KERNEL_LOGGER_DEVICE(VAR, NAME, SIZE) \
static unsigned char _buf_ ## VAR[SIZE]; \
static struct logger_log VAR = { \
	.buffer = _buf_ ## VAR, \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = &logger_kernel_fops, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = SIZE, \
};

#define LOG_KERNEL_SIZE  (256*1024)

DEFINE_CONFIG_KERNEL_LOGGER_DEVICE(log_kernel, LOGGER_LOG_KERNEL, LOG_KERNEL_SIZE)
static int __init init_kernel_log(struct logger_log *log)
{
	int ret;

	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		printk(KERN_ERR "logger: failed to register misc "
		       "device for log '%s'!\n", log->misc.name);
		return ret;
	}

	printk(KERN_INFO "logger: created %luK log '%s'\n",
	       (unsigned long) log->size >> 10, log->misc.name);

	return 0;
}
#endif

/*
 * Defines a log structure with name 'NAME' and a size of 'SIZE' bytes, which
 * must be a power of two, and greater than
 * (LOGGER_ENTRY_MAX_PAYLOAD + sizeof(struct logger_entry)).
 */
/* FUJITSU:2011-09-05 FTA del start */
#ifndef CONFIG_FTA
#define DEFINE_LOGGER_DEVICE(VAR, NAME, SIZE) \
static unsigned char _buf_ ## VAR[SIZE]; \
static struct logger_log VAR = { \
	.buffer = _buf_ ## VAR, \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = &logger_fops, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = SIZE, \
};

DEFINE_LOGGER_DEVICE(log_main, LOGGER_LOG_MAIN, 256*1024)
DEFINE_LOGGER_DEVICE(log_events, LOGGER_LOG_EVENTS, 256*1024)
DEFINE_LOGGER_DEVICE(log_radio, LOGGER_LOG_RADIO, 256*1024)
DEFINE_LOGGER_DEVICE(log_system, LOGGER_LOG_SYSTEM, 256*1024)

static struct logger_log *get_log_from_minor(int minor)
{
	if (log_main.misc.minor == minor)
		return &log_main;
	if (log_events.misc.minor == minor)
		return &log_events;
	if (log_radio.misc.minor == minor)
		return &log_radio;
	if (log_system.misc.minor == minor)
		return &log_system;
#ifdef CONFIG_KERNEL_LOG        /* FUJITSU:2013-03-19 KERNEL LOG added. */
	if (log_kernel.misc.minor == minor)
		return &log_kernel;
#endif
	return NULL;
}

static int __init init_log(struct logger_log *log)
{
	int ret;

	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		printk(KERN_ERR "logger: failed to register misc "
		       "device for log '%s'!\n", log->misc.name);
		return ret;
	}

	printk(KERN_INFO "logger: created %luK log '%s'\n",
	       (unsigned long) log->size >> 10, log->misc.name);

	return 0;
}

#else
/* FUJITSU:2011-09-05 FTA del end */

/* FUJITSU:2011-09-05 FTA add start */
#define FTA_LOG_MAIN    0
#define FTA_LOG_EVENTS  1
#define FTA_LOG_RADIO   2
#define FTA_LOG_SYSTEM  3
#define FTA_LOG_USER0   4
#define FTA_LOG_USER1   5
#define FTA_LOG_NUM     6

struct fta_logger_type {
	unsigned long totalSize;
	unsigned long offset;
	struct logger_log    logger;
};

extern unsigned char *ftadrv_alloc(int area,int size);
struct fta_logger_type *fta_logger[FTA_LOG_NUM];

/*!
 @brief get_log_from_minor

 get the log struct from minor no

 @param [in] minor  minor device no

 @retval   <>0:  pointer of the log struct
             0:  failed
*/
static struct logger_log *get_log_from_minor(int minor)
{
	int i;
#ifdef CONFIG_KERNEL_LOG        /* FUJITSU:2013-03-19 KERNEL LOG added. */
	if (log_kernel.misc.minor == minor) {
		return &log_kernel;
    }
#endif
	for(i=0;i<FTA_LOG_NUM;i++) {
		if(fta_logger[i]->logger.misc.minor == minor)
			return &(fta_logger[i]->logger);
	}
	return NULL;
}

/*!
 @brief init_log

 initialize android log

 @param [in] tag    android log area id
 @param [in] name   device name

 @retval     0:  output message length
 @retval    -1:  failed
*/
static int __init init_log(int tag,char *name)
{
	struct fta_logger_type *fp;
	int ret;

	fp = (struct fta_logger_type *)ftadrv_alloc(tag,sizeof(struct fta_logger_type));

	if(fp == 0) {
		printk(KERN_ERR "logger: failed to allocate area %s\n",name);
		return -1;
	}
	fta_logger[tag] = fp;
	fp->offset = offsetof(struct logger_log,w_off);
	fp->logger.buffer = ((unsigned char *)fp) + sizeof(struct fta_logger_type);
	fp->logger.misc.minor = MISC_DYNAMIC_MINOR;
	fp->logger.misc.name = name;
	fp->logger.misc.fops = &logger_fops;
	fp->logger.misc.parent = NULL;
	init_waitqueue_head(&fp->logger.wq);
	INIT_LIST_HEAD(&fp->logger.readers);
	mutex_init(&fp->logger.mutex);
	fp->logger.w_off = 0;
	fp->logger.head = 0;
	fp->logger.size = 0;
	fp->logger.size = fp->totalSize - sizeof(struct fta_logger_type);
	fp->totalSize = sizeof(struct logger_log);
	if(name) {
		ret = misc_register(&fp->logger.misc);

		if (unlikely(ret)) {
			printk(KERN_ERR "logger: failed to register misc "
			       "device for log '%s'!\n", name);
			return ret;
		}
	}

	printk(KERN_INFO "logger: created %luK log '%s'\n",
	       (unsigned long) fp->logger.size >> 10, name);

	return 0;
}
/* FUJITSU:2011-09-05 FTA add end */
#endif /* CONFIG_FTA */

static int __init logger_init(void)
{
	int ret;

#ifdef CONFIG_FTA
	ret = init_log(FTA_LOG_MAIN, LOGGER_LOG_MAIN);  /* FUJITSU:2011-09-05 FTA mod */
#else
	ret = init_log(&log_main);
#endif /* CONFIG_FTA */
	if (unlikely(ret))
		goto out;

#ifdef CONFIG_FTA
	ret = init_log(FTA_LOG_EVENTS, LOGGER_LOG_EVENTS);  /* FUJITSU:2011-09-05 FTA mod */
#else
	ret = init_log(&log_events);
#endif /* CONFIG_FTA */
	if (unlikely(ret))
		goto out;

#ifdef CONFIG_FTA
	ret = init_log(FTA_LOG_RADIO, LOGGER_LOG_RADIO);  /* FUJITSU:2011-09-05 FTA mod */
#else
	ret = init_log(&log_radio);
#endif /* CONFIG_FTA */
	if (unlikely(ret))
		goto out;

#ifdef CONFIG_FTA
	ret = init_log(FTA_LOG_SYSTEM, LOGGER_LOG_SYSTEM); /* FUJITSU:2011-09-05 FTA mod */
#else
	ret = init_log(&log_system);
#endif /* CONFIG_FTA */
	if (unlikely(ret))
		goto out;

#ifdef CONFIG_FTA
	/* FUJITSU:2011-09-05 FTA add start */
	ret = init_log(FTA_LOG_USER0, NULL);
	if (unlikely(ret))
		goto out;
	ret = init_log(FTA_LOG_USER1, NULL);
	if (unlikely(ret))
		goto out;
	/* FUJITSU:2011-09-05 FTA add end */
#endif /* CONFIG_FTA */
#ifdef CONFIG_KERNEL_LOG        /* FUJITSU:2013-03-19 KERNEL LOG added. */
	ret = init_kernel_log(&log_kernel);
    if (unlikely(ret))
        goto out;
    pk_buff_size = get_printk_buff_size() 
                   + sizeof(struct logger_entry);
#endif
out:
	return ret;
}
device_initcall(logger_init);

#ifdef CONFIG_CRASHDUMP
#define LOGGER_C
#include <linux/crashdump.h>

void crashdump_logcat_register(void)
{
	cctr.lctr.log_main = &fta_logger[FTA_LOG_MAIN]->logger;
	cctr.lctr.log_events = &fta_logger[FTA_LOG_EVENTS]->logger;
	cctr.lctr.log_radio = &fta_logger[FTA_LOG_RADIO]->logger;
	cctr.lctr.log_system = &fta_logger[FTA_LOG_SYSTEM]->logger;
}
#endif
#ifdef CONFIG_KERNEL_LOG        /* FUJITSU:2013-03-19 KERNEL LOG added. */
#define ARRAY_OF(ary)   ( (sizeof(ary)) / (sizeof(ary[0])) )
int strlen_null_or_lf(const char *s)
{
	const char *sc;

    for (sc = s; *sc && (*sc != '\n'); sc++);
        /* nothing */
	return (sc - s) + (*sc == '\n' ? 1 : 0); /* The line-feed character is counted. */
}
int logger_kernel_write(unsigned long lv,  const unsigned char *uptime, const char *s)
{
    struct logger_entry header;
    struct timespec now;
    struct logger_log *log  = &log_kernel;
    struct iovec vec[5];
    struct iovec *iov = vec;
    int vec_count     = ARRAY_OF(vec);
    const char *ptag  = "kernel";
    int prio    = 7;    /* ANDROID_LOG_FATAL */
    unsigned int msg_len;
    char lv_buff[3];
    ssize_t sum = 0;
    int ret     = 0;
    size_t len;
    unsigned long flags;

    now = current_kernel_time();

    if(current->comm[0] != '\0') {
        ptag = current->comm;
    }

    lv_buff[0] = '<';
    lv_buff[1] = '0' | (lv & 0xff);
    lv_buff[2] = '>';

    /* format: <priority:1><tag:N>\0<message:N>\0 */
    vec[0].iov_base = (unsigned char *) &prio;
    vec[0].iov_len  = 1;
    vec[1].iov_base = (void *)ptag;
    vec[1].iov_len  = strlen(ptag) + 1;
    vec[2].iov_base = (void *)lv_buff;
    vec[2].iov_len  = sizeof(lv_buff);
    vec[3].iov_base = (void *)uptime;
    vec[3].iov_len  = strlen(uptime);        /* The NULL terminal is included in the buffer "s".  */
    vec[4].iov_base = (void *)s;
    ret = strlen_null_or_lf(s);
    vec[4].iov_len  = min_t(size_t, ret+1, LOGGER_ENTRY_MAX_PAYLOAD);

    msg_len = vec[0].iov_len
            + vec[1].iov_len
            + vec[2].iov_len
            + vec[3].iov_len
            + vec[4].iov_len;

    header.pid = current->tgid;
    header.tid = current->pid;
    header.sec = now.tv_sec;
    header.nsec= now.tv_nsec;
    header.euid = current_euid();
    header.len = min_t(size_t, msg_len, LOGGER_ENTRY_MAX_PAYLOAD);
	header.hdr_size = sizeof(struct logger_entry);

    raw_spin_lock_irqsave(&printk_spin_lock, flags);
    write_seqcount_begin(&printk_seq_lock);

    fix_up_readers(log, sizeof(struct logger_entry) + header.len);
    do_write_log(log, &header, sizeof(struct logger_entry));

    while (vec_count-- > 0) {
        /* figure out how much of this vector we can keep */
        len = min_t(size_t, iov->iov_len, header.len - sum);

        /* write out this segment's payload */
        do_write_log(log, iov->iov_base, len);

        iov++;
        sum += len;
    }

    write_seqcount_end(&printk_seq_lock);
    raw_spin_unlock_irqrestore(&printk_spin_lock, flags);

    /* wake up any blocked readers */
    wake_up_interruptible(&log->wq);

    return ret;
}

#endif
