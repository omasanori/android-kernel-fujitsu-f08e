/*
 * Copyright(C) 2013 FUJITSU LIMITED
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>					// create_proc_entry
#include <linux/vmalloc.h>
/* FUJITSU:2013-04-08 MUTEX start */
#include <linux/mutex.h>
/* FUJITSU:2013-04-08 MUTEX end */
#include <asm/uaccess.h>					// copy_from_user
#include <asm/traps.h>
#include "backtrace.h"


/*	make interface /proc/PROCNAME 	*/
#define PROCNAME "driver/backtrace"
#define MAX_BACKTRACE_BUF_SIZE (1*1024*1024)

#define STRBUFSZ 512												/*	once buffer size on backtrace_write_str	*/

static char* backtrace_buf = NULL;
static int backtrace_buf_size = 0;
static int backtrace_buf_write_index = 0;
static int backtrace_buf_read_index  = 0;
static int backtrace_buf_nonbuf_index  = 0;
static struct proc_dir_entry* backtrace_entry;
static char backtrace_nonbuf[] = "backtrace:vmalloc err\n";
/* FUJITSU:2013-04-08 MUTEX start */
DEFINE_MUTEX(backtrace_mutex_lock);
/* FUJITSU:2013-04-08 MUTEX end */


void backtrace_write_str(const char *fmt, ...)
{
	char strbuf[STRBUFSZ];
	va_list va;
	int retlen;

	va_start(va, fmt);
	retlen = vsnprintf(strbuf, sizeof(strbuf), fmt, va);
	va_end(va);

	if (retlen <= (backtrace_buf_size - backtrace_buf_write_index)) {
		memcpy((backtrace_buf+ backtrace_buf_write_index),strbuf,retlen);
		
		backtrace_buf_write_index += retlen;
		dprintk(LogTypeDEBUG, "success retlen=%d backtrace_buf_write_index=%d",retlen,backtrace_buf_write_index);
	} else {
		dprintk(LogTypeERR ,"faild retlen=%d backtrace_buf_write_index=%d",retlen,backtrace_buf_write_index);
	}
}

static void backtrace_write_symbol(const char *fmt, unsigned long address)
{
	char buffer[KSYM_SYMBOL_LEN];

	sprint_symbol(buffer, address);

	backtrace_write_str(fmt, buffer);
}

static void get_backtrace_mem(const char *str, unsigned long bottom, unsigned long top, int mode, struct task_struct *tsk)
{
	unsigned long p = bottom & ~31;
	mm_segment_t fs;
	int i;

	dprintk(LogTypeINFO, "[IN]");

	/*
	 * We need to switch to kernel mode so that we can
	 * safely read from kernel space.
	 */
	fs = get_fs();
	dprintk(LogTypeINFO, "fs=%d %d",(int)fs,(int)KERNEL_DS);
	/* change kernel mode */
	set_fs(KERNEL_DS);

	backtrace_write_str("%s(0x%08lx to 0x%08lx)\n", str, bottom, top);

	for (p = bottom & ~31; p < top;) {
		backtrace_write_str("%04lx: ", p & 0xffff);

		for (i = 0; i < 8; i++, p += 4) {
			unsigned int val;

			if (p < bottom || p >= top) {
				backtrace_write_str("         ");
			} else {
				if (mode == 0) {
					access_process_vm(tsk, p, (void *)&val, 4, 0);
				} else {
					val = *(unsigned long *)p;
				}

				backtrace_write_str("%08x ", val);
			}
		}
		backtrace_write_str("\n");
	}

	/* Change is returned. */
	set_fs(fs);
	dprintk(LogTypeINFO, "[OUT]");
}

void get_backtrace_entry(unsigned long where, unsigned long from, unsigned long frame)
{
#ifdef CONFIG_KALLSYMS
	backtrace_write_str("[<%08lx>] ", where);
	backtrace_write_symbol("(%s) ", where);
	backtrace_write_str("from [<%08lx>] ", from);
	backtrace_write_symbol("(%s)\n", from);
#else
	backtrace_write_str("Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
#endif

	if (in_exception_text(where)) {
		get_backtrace_mem("Exception stack", frame + 4, frame + 4 + sizeof(struct pt_regs), 1, NULL);
	}
}


/*
 * Stack pointers should always be within the kernels view of
 * physical memory.  If it is not there, then we can't dump
 * out any information relating to the stack.
 */
static int verify_stack(unsigned long sp, struct task_struct *tsk)
{
	dprintk(LogTypeINFO, "[IN]");

	if (!sp) {
		dprintk(LogTypeERR, "no frame pointer");
		return -EFAULT;
	}

	if ((sp < PAGE_OFFSET) || ((sp > (unsigned long)high_memory) && (high_memory != 0))) {
		dprintk(LogTypeERR, "invalid frame pointer 0x%04x", (int)sp);
		return -EFAULT;
	}

	if (sp < (unsigned long)end_of_stack(tsk)) {
		dprintk(LogTypeERR, "frame pointer underflow");
		return -EFAULT;
	}
	dprintk(LogTypeINFO, "[OUT]");
	return 0;
}

static void backtrace_thread_info(struct task_struct *tsk, struct pt_regs *regs)
{
	unsigned long fp;
	int ok = 1;

	dprintk(LogTypeINFO ,"[IN]");

	backtrace_write_str("Backtrace: ");
	fp = regs->ARM_fp;

	if (tsk == current) {
		asm("mov %0, fp" : "=r" (fp) : : "cc");
	}

	if (verify_stack(fp, tsk)) {
		backtrace_write_str("invalid frame pointer 0x%08x", fp);
		ok = 0;
	}
	backtrace_write_str("\n");

	if (ok) {
		/*
		 * here 0x10 means 32 bit mode.
		 * we do not have the saved CPSR for this case,
		 * but specifying 0x10 is good enough.
		 */
		get_asm_backtrace(fp, 0x10);
	}

	barrier();
	dprintk(LogTypeINFO ,"[OUT]");
}

static	void backtrace_thread_search(void)
{
	struct task_struct *task, *g;
	struct pt_regs ptregs;

	dprintk(LogTypeNOTICE ,"[IN]");
	backtrace_write_str("\n=== Process info START ===\n");

	do_each_thread(g,task) {
		backtrace_write_str("*** Pid: %d, Tid: %d, PPid: %d, comm: %20s *** kernel context\n"
		, task_tgid_nr(task), task_pid_nr(task), task_tgid_nr(task->real_parent), task->comm);

		ptregs.ARM_ORIG_r0 = 0;
		ptregs.ARM_cpsr = 0;
		ptregs.ARM_pc = task_thread_info(task)->cpu_context.pc;
		ptregs.ARM_lr = 0;
		ptregs.ARM_sp = task_thread_info(task)->cpu_context.sp;
		ptregs.ARM_ip = 0;
		ptregs.ARM_fp = task_thread_info(task)->cpu_context.fp;
		ptregs.ARM_r10 = task_thread_info(task)->cpu_context.sl;
		ptregs.ARM_r9 = task_thread_info(task)->cpu_context.r9;
		ptregs.ARM_r8 = task_thread_info(task)->cpu_context.r8;
		ptregs.ARM_r7 = task_thread_info(task)->cpu_context.r7;
		ptregs.ARM_r6 = task_thread_info(task)->cpu_context.r6;
		ptregs.ARM_r5 = task_thread_info(task)->cpu_context.r5;
		ptregs.ARM_r4 = task_thread_info(task)->cpu_context.r4;
		ptregs.ARM_r3 = 0;
		ptregs.ARM_r2 = 0;
		ptregs.ARM_r1 = 0;
		ptregs.ARM_r0 = 0;

		backtrace_thread_info(task, &ptregs);

	} while_each_thread(g,task);
	backtrace_write_str("\n=== Process info END ===\n");
	dprintk(LogTypeNOTICE ,"[OUT]");
}



static void* backtrace_malloc(size_t size,int* pget_size)
{
	void *	ptr = NULL;
	size_t	get_size = size;
	
	dprintk(LogTypeNOTICE,"[IN]size[%d]",size);
	ptr = vmalloc(size);
	if (ptr == NULL) {
		dprintk(LogTypeERR,"[OUT]ptr[%p]get_size=%d",ptr,get_size);
		return	ptr;
	}
	if (pget_size) {
		*pget_size = get_size;
	}
	dprintk(LogTypeWARNING,"[OUT]ptr[%p]get_size=%d",ptr,get_size);
	return	ptr;
}

static void backtrace_free(void** ptr)
{
	dprintk(LogTypeNOTICE,"[IN]ptr[%p]",ptr);
	if ((ptr == NULL)||(*ptr == NULL)) {
		dprintk(LogTypeNOTICE,"[OUT]ptr[%p]",ptr);
		return	;
	}
	vfree(*ptr);
	*ptr = NULL;
	dprintk(LogTypeWARNING,"[OUT]ptr[%p]",*ptr);
}


/* FUJITSU:2013-04-08 MUTEX start */
static inline int backtrace_write_proc(struct file *filp, const char *buf, unsigned long len, void *data)
/* FUJITSU:2013-04-08 MUTEX end */
{
	dprintk(LogTypeNOTICE,"[IN]buf[%s]len=%lu",buf,len);
	if (memcmp(buf,"SaveBackTrace",strlen("SaveBackTrace")) != 0) {
		/* Unlawful access */
		backtrace_free((void**)&backtrace_buf);
		dprintk(LogTypeNOTICE,"[OUT]ret=[%lu]",len);
		backtrace_buf_write_index = 0;
		backtrace_buf_read_index  = 0;
		backtrace_buf_nonbuf_index  = 0;
		return -EIO;
	}
	
	if (backtrace_buf) {
		/* Before read-out has finished yet, it ends without doing anything. */
		dprintk(LogTypeNOTICE,"[OUT]ret=[%d]",-EIO);
		return -EIO;
	}
	
	backtrace_buf_write_index = 0;
	backtrace_buf_read_index  = 0;
	backtrace_buf_nonbuf_index  = 0;
	backtrace_buf = backtrace_malloc(MAX_BACKTRACE_BUF_SIZE,&backtrace_buf_size);
	
	if (backtrace_buf) {
		backtrace_thread_search();
		dprintk(LogTypeNOTICE,"backtrace_thread_search[OUT]backtrace_buf_write_index=%d",backtrace_buf_write_index);
	} else {
		/* Memory secured failure */
		dprintk(LogTypeNOTICE,"[OUT]ret=[%d]",-EIO);
		return -EIO;
	}
	
	dprintk(LogTypeNOTICE,"[OUT]ret=[%lu]",len);
	return (int)len;
}


/* FUJITSU:2013-04-08 MUTEX start */
static inline int backtrace_read_proc(char *page, char **start, off_t offset, int count, int *eof, void *data)
/* FUJITSU:2013-04-08 MUTEX end */
{
	int outbyte;
	int remainder_byte;
	char * read_ptr;


	if (offset > 0) {
		dprintk(LogTypeDEBUG, "offset=%lld",(long long int)offset);
		*eof = 1;
		return 0;
	}

	if (backtrace_buf) {
		read_ptr = backtrace_buf+ backtrace_buf_read_index;
		remainder_byte = backtrace_buf_write_index - backtrace_buf_read_index;
	} else {
		dprintk(LogTypeINFO, "backtrace_buf=NULL");
		read_ptr = backtrace_nonbuf+ backtrace_buf_nonbuf_index;
		remainder_byte = sizeof(backtrace_nonbuf) - 1 - backtrace_buf_nonbuf_index;
	}
	
	dprintk(LogTypeNOTICE, "len = %d size=%d page=%p %p offset=%lld", count, remainder_byte,page,read_ptr,(long long int)offset);
	if (remainder_byte <= 0) {
		/* It will clear, if it comes to a termination. */
		backtrace_free((void**)&backtrace_buf);
		backtrace_buf_read_index  = 0;
		backtrace_buf_nonbuf_index  = 0;
		*eof = 1;
		dprintk(LogTypeNOTICE, "remainder_byte=%d",remainder_byte);
		return -1;
	}

	if (count <= remainder_byte) {
		outbyte = count ;
	} else {
		outbyte = remainder_byte;
	}
	
	memcpy(page,read_ptr,outbyte);
	backtrace_buf_read_index += outbyte;
	backtrace_buf_nonbuf_index += outbyte;
	/* It is always considered as a termination. */
	*eof = 1;
	dprintk(LogTypeNOTICE ,"[OUT]size=%d eof=%d",outbyte,*eof);
	return outbyte;
}

/* FUJITSU:2013-04-08 MUTEX start */
static int backtrace_write(struct file *filp, const char *buf, unsigned long len, void *data)
{
	int ret;
	mutex_lock(&backtrace_mutex_lock);
	ret = backtrace_write_proc(filp, buf, len, data);
	mutex_unlock(&backtrace_mutex_lock);
	return ret;
}
/* FUJITSU:2013-04-08 MUTEX end */

/* FUJITSU:2013-04-08 MUTEX start */
static int backtrace_read(char *page, char **start, off_t offset, int count, int *eof, void *data)
{
	int ret;
	mutex_lock(&backtrace_mutex_lock);
	ret = backtrace_read_proc(page, start, offset, count, eof, data);
	mutex_unlock(&backtrace_mutex_lock);
	return ret;
}
/* FUJITSU:2013-04-08 MUTEX end */

static int __init backtrace_module_init(void)
{
	// make interface /proc/PROCNAME (system authority is performed by init.rc) 
	backtrace_entry = create_proc_entry(PROCNAME, 0666, NULL);
	if (backtrace_entry) {
		backtrace_entry->write_proc  = backtrace_write;
		backtrace_entry->read_proc   = backtrace_read;
		
		dprintk(LogTypeNOTICE, "backtrace is loaded");
		backtrace_buf_write_index = 0;
		backtrace_buf_read_index  = 0;
		backtrace_buf_nonbuf_index  = 0;

		return 0;
	}
	dprintk(LogTypeEMERG,"create_proc_entry failed");
	return -EBUSY;
}


static void __exit backtrace_module_exit(void)
{
	remove_proc_entry(PROCNAME, NULL);

	dprintk(LogTypeNOTICE, "backtrace is removed");
}


module_init(backtrace_module_init);
module_exit(backtrace_module_exit);

MODULE_DESCRIPTION("backtrace");
