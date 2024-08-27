/*
 * Copyright(C) 2009-2013 FUJITSU LIMITED
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __LINUX_CRASHDUMP_H
#define __LINUX_CRASHDUMP_H

#include <linux/miscdevice.h>
#include <linux/mtd/mtd.h>

#define CRASHDUMP_PARTITION_NAME CONFIG_CRASHDUMP_PART_NAME

extern int crashdump_inprogress;

/*
 * Size of string buffer
 * This buffer is for holding formatted string for
 * crashdump_write_str internal use.
 * This buffer can be any size larger enough to hold formatted string.
 * Caller of crashdump_write_str must not specify strings larger than
 * this size. If so, the extra part will be ignored.
 */
#define STRBUFSZ 512

/*
 * Size of buffer "to-be-flushed".
 * This buffer is for buffering logs less than page size.
 * This buffer is actually written to the flash by
 * crashdump_flush call.
 * This buffer should be the size of one page of the flash.
 */
#define TBFLUSHBUFSZ 4096

/*
 * Size of read buffer
 * This buffer is for holding file read data
 * which is then passed to crashdump_write.
 * /proc/binder/ requires this size to be PAGE_SIZE.
 * Otherwise it could have been any size.
 */
#define READBUFSZ PAGE_SIZE

/*
 * File name to dump sysfs output
 */
#define DUMPSYSFILENAME "/data/crashdump_dumpsys.txt"

/* copied from fs/proc/task_mmu.c for procrank and librank */
#define PM_ENTRY_BYTES      sizeof(u64)
#define PM_STATUS_BITS      3
#define PM_STATUS_OFFSET    (64 - PM_STATUS_BITS)
#define PM_STATUS_MASK      (((1LL << PM_STATUS_BITS) - 1) << PM_STATUS_OFFSET)
#define PM_STATUS(nr)       (((nr) << PM_STATUS_OFFSET) & PM_STATUS_MASK)
#define PM_PSHIFT_BITS      6
#define PM_PSHIFT_OFFSET    (PM_STATUS_OFFSET - PM_PSHIFT_BITS)
#define PM_PSHIFT_MASK      (((1LL << PM_PSHIFT_BITS) - 1) << PM_PSHIFT_OFFSET)
#define PM_PSHIFT(x)        (((u64) (x) << PM_PSHIFT_OFFSET) & PM_PSHIFT_MASK)
#define PM_PFRAME_MASK      ((1LL << PM_PSHIFT_OFFSET) - 1)
#define PM_PFRAME(x)        ((x) & PM_PFRAME_MASK)

#define PM_PRESENT          PM_STATUS(4LL)
#define PM_SWAP             PM_STATUS(2LL)
#define PM_NOT_PRESENT      PM_PSHIFT(PAGE_SHIFT)
#define PM_END_OF_BUFFER    1

enum {
	LOGCAT_MAIN = 0,
	LOGCAT_EVENTS,
	LOGCAT_RADIO,
	LOGCAT_SYSTEM,
};

struct printk_ctr {
	char *__log_buf;
	unsigned *logged_chars;
	unsigned *log_start;
	unsigned *con_start;
	unsigned *log_end;
	int *log_buf_len;
};

#ifndef LOGGER_C
/* structure copied from drivers/staging/android/logger.c */
struct logger_log {
	unsigned char 		*buffer;/* the ring buffer itself */
	struct miscdevice	misc;	/* misc device representing the log */
	wait_queue_head_t	wq;	/* wait queue for readers */
	struct list_head	readers; /* this log's readers */
	struct mutex		mutex;	/* mutex protecting buffer */
	size_t			w_off;	/* current write head offset */
	size_t			head;	/* new readers start here */
	size_t			size;	/* size of the log */
};
#endif

struct logcat_ctr {
	struct logger_log *log_main;
	struct logger_log *log_events;
	struct logger_log *log_radio;
	struct logger_log *log_system;
};

struct crashdump_ctr {
	struct printk_ctr pctr;
	struct logcat_ctr lctr;
};

extern void crashdump(void);
extern int crashdump_findpartition(void);
extern int crashdump_erasepartition(void);
extern void crashdump_flush(void);
extern void crashdump_log_buf(void);
extern void crashdump_g3(void);
extern void crashdump_logcat(int type);
extern void crashdump_system_properties(void);
extern void crashdump_taskinfo(void);
extern void crashdump_top(void);
extern void crashdump_procrank(void);
extern void crashdump_librank(void);
extern void crashdump_file(const char *filename);
extern void crashdump_time(void);
extern void crashdump_df(void);
extern void crashdump_netcfg(void);
extern void crashdump_service(void);

extern void crashdump_write(char *abuf, int alen);
extern void crashdump_write_str(const char *fmt, ...);
extern void crashdump_write_bin(char *abuf, int alen);
extern void crashdump_timetr(struct timeval *tv, int *year0, int *month0, int *monthday0, int *hour0, int *minute0, int *second0, int *weekday0, char *weekdaystring0, char *monthstring0);
extern int crashdump_read_file(const char *filename, void *buf, size_t len);

extern void modify_reg16(unsigned short clear, unsigned short set, unsigned long reg);
extern void crashdump_printk_register(void);
extern void crashdump_logcat_register(void);
extern unsigned long system_properties_addr[];
extern asmlinkage void crashdump_c_backtrace(unsigned long fp, int pmode);
extern struct list_head* get_mtd_partitions(void);
extern const char *crashdump_get_task_state(struct task_struct *tsk);
extern int crashdump_pid_cmdline(struct task_struct *task, char *buffer, unsigned int buffer_len);
extern unsigned long task_vsize(struct mm_struct *);

extern char gbuf[];
extern struct crashdump_ctr cctr;
extern struct mtd_info *master_mtd;

#endif // __LINUX_CRASHDUMP_H
