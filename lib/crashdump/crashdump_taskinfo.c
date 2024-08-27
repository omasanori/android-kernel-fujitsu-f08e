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

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/sections.h>
#include <asm/traps.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

//#define dprintk2 printk
#define dprintk2(...)

static const char *processor_modes[] = {
  "USER_26", "FIQ_26" , "IRQ_26" , "SVC_26" , "UK4_26" , "UK5_26" , "UK6_26" , "UK7_26" ,
  "UK8_26" , "UK9_26" , "UK10_26", "UK11_26", "UK12_26", "UK13_26", "UK14_26", "UK15_26",
  "USER_32", "FIQ_32" , "IRQ_32" , "SVC_32" , "UK4_32" , "UK5_32" , "UK6_32" , "ABT_32" ,
  "UK8_32" , "UK9_32" , "UK10_32", "UND_32" , "UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static const char *isa_modes[] = {
  "ARM" , "Thumb" , "Jazelle", "ThumbEE"
};

static void crashdump_print_symbol(const char *fmt, unsigned long address)
{
	char buffer[KSYM_SYMBOL_LEN];

	sprint_symbol(buffer, address);

	crashdump_write_str(fmt, buffer);
}

static void crashdump_mem(const char *str, unsigned long bottom, unsigned long top, int mode, struct task_struct *tsk)
{
	unsigned long p = bottom & ~31;
	mm_segment_t fs;
	int i;

	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	/*
	 * We need to switch to kernel mode so that we can
	 * safely read from kernel space.
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);

	dprintk2(KERN_EMERG "%s(0x%08lx to 0x%08lx)\n", str, bottom, top);
	crashdump_write_str("%s(0x%08lx to 0x%08lx)\n", str, bottom, top);

	for (p = bottom & ~31; p < top;) {
		crashdump_write_str("%04lx: ", p & 0xffff);

		for (i = 0; i < 8; i++, p += 4) {
			unsigned int val;

			if (p < bottom || p >= top)
				crashdump_write_str("         ");
			else {
				if (mode == 0) {
					access_process_vm(tsk, p, (void *)&val, 4, 0);
				}
				else {
					val = *(unsigned long *)p;
				}

				crashdump_write_str("%08x ", val);
			}
		}
		crashdump_write_str("\n");
	}

	set_fs(fs);
}

void crashdump_backtrace_entry(unsigned long where, unsigned long from, unsigned long frame)
{
#ifdef CONFIG_KALLSYMS
	crashdump_write_str("[<%08lx>] ", where);
	crashdump_print_symbol("(%s) ", where);
	crashdump_write_str("from [<%08lx>] ", from);
	crashdump_print_symbol("(%s)\n", from);
#else
	crashdump_write_str("Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
#endif

	if (in_exception_text(where))
		crashdump_mem("Exception stack", frame + 4, frame + 4 + sizeof(struct pt_regs), 1, NULL);
}

/*
 * Stack pointers should always be within the kernels view of
 * physical memory.  If it is not there, then we can't dump
 * out any information relating to the stack.
 */
static int verify_stack(unsigned long sp, int mode, struct task_struct *tsk)
{
	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	if (!sp) {
		dprintk2(KERN_EMERG "(%s:%s:%d) no frame pointer\n", __FILE__, __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (mode == 0) {

		/* no further check */

	}
	else {

		if (sp < PAGE_OFFSET || (sp > (unsigned long)high_memory && high_memory != 0)) {
			dprintk2(KERN_EMERG "invalid frame pointer 0x%08x\n", sp);
			return -EFAULT;
		}

		if (sp < (unsigned long)end_of_stack(tsk)) {
			dprintk2(KERN_EMERG "frame pointer underflow\n");
			return -EFAULT;
		}
	}

	return 0;
}

static void crashdump_backtrace(struct task_struct *tsk, struct pt_regs *regs, int mode)
{
	unsigned long fp;
	int ok = 1;

	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	crashdump_write_str("Backtrace: ");
	fp = regs->ARM_fp;

	if ( (mode != 0) && (tsk == current) ) {
		asm("mov %0, fp" : "=r" (fp) : : "cc");
	}

	if (verify_stack(fp, mode, tsk)) {
		crashdump_write_str("invalid frame pointer 0x%08x", fp);
		ok = 0;
	}
	crashdump_write_str("\n");

	if (ok) {
		if (mode == 0) {
			crashdump_c_backtrace(fp, processor_mode(regs));
		}
		else {
			/*
			 * here 0x10 means 32 bit mode.
			 * we do not have the saved CPSR for this case,
			 * but specifying 0x10 is good enough.
			 */
			crashdump_c_backtrace(fp, 0x10);
		}
	}

	barrier();
}

static void crashdump_regs(struct pt_regs *regs, int mode)
{
	unsigned long flags;
	char buf[64];

	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	crashdump_print_symbol("PC is at %s\n", instruction_pointer(regs));

	if (mode == 0) {
		crashdump_print_symbol("LR is at %s\n", regs->ARM_lr);

		crashdump_write_str("lr : [<%08lx>]    psr: %08lx    ip : %08lx\n",
							regs->ARM_lr, regs->ARM_cpsr, regs->ARM_ip);
	}

	crashdump_write_str("pc : [<%08lx>]    sp : %08lx  fp : %08lx\n",
		regs->ARM_pc, regs->ARM_sp, regs->ARM_fp);

	crashdump_write_str("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	crashdump_write_str("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);

	if (mode == 0) {
		crashdump_write_str("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
							regs->ARM_r3, regs->ARM_r2,
							regs->ARM_r1, regs->ARM_r0);
	}
	else {
		return;
	}

	flags = regs->ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

	crashdump_write_str("Flags: %s  IRQs o%s  FIQs o%s  Mode %s  ISA %s  Segment %s\n",
		buf, interrupts_enabled(regs) ? "n" : "ff",
		fast_interrupts_enabled(regs) ? "n" : "ff",
		processor_modes[processor_mode(regs)],
		isa_modes[isa_mode(regs)],
		get_fs() == get_ds() ? "kernel" : "user");
#ifdef CONFIG_CPU_CP15
	{
		unsigned int ctrl;

		buf[0] = '\0';
#ifdef CONFIG_CPU_CP15_MMU
		{
			unsigned int transbase, dac;
			asm("mrc p15, 0, %0, c2, c0\n\t"
			    "mrc p15, 0, %1, c3, c0\n"
			    : "=r" (transbase), "=r" (dac));
			snprintf(buf, sizeof(buf), "  Table: %08x  DAC: %08x",
				transbase, dac);
		}
#endif
		asm("mrc p15, 0, %0, c1, c0\n" : "=r" (ctrl));

		crashdump_write_str("Control: %08x%s\n", ctrl, buf);
	}
#endif
}

#define INIT_PID 1
#define KTHREADD_PID 2
pid_t save_pid;
char const *procname[]={
			"system_server"
		,	"surfaceflinger"
		,	"mediaserver"
		,	"sensors_server"
		,	NULL
};

static int crashdump_is_dump_proc(struct task_struct *p)
{
	int i;

	if (p==NULL) {
		return 0;
	}
	i=0;
	while (procname[i]!=NULL) {
		dprintk(KERN_EMERG "strnlen(%s) %d\n", procname[i], strlen(procname[i]));
		if ((!strncmp(procname[i], p->comm, strlen(procname[i])))) {
			return 1;
		}
		i++;
	}
	return 0;
}

static int crashdump_is_dump_thread(struct task_struct *p)
{
	if ((task_tgid_nr(p->real_parent) == INIT_PID)
		|| (task_tgid_nr(p->real_parent) == KTHREADD_PID)) {
		return 1;
	}
	if ((task_tgid_nr(p) == INIT_PID)
		|| (task_tgid_nr(p) == KTHREADD_PID)) {
		return 1;
	}
	return 0;
}

void crashdump_taskinfo(void)
{
	struct task_struct *task, *g;
	struct pt_regs ptregs;
	unsigned long wchan;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== Process info ===\n");

    do_each_thread(g,task) {

		if ( (task != kthreadd_task) && (task->real_parent != kthreadd_task) ) {
			crashdump_write_str("*** Pid: %d, Tid: %d, PPid: %d, comm: %20s *** user context\n"
			, task_tgid_nr(task), task_pid_nr(task), task_tgid_nr(task->real_parent), task->comm);

			if (crashdump_is_dump_proc(task) 
				|| crashdump_is_dump_thread(task) 
				|| (task_tgid_nr(task) == task_pid_nr(task))
				|| ((save_pid != 0)&&(save_pid==task_tgid_nr(task)))) {

				ptregs = *task_pt_regs(task);
				crashdump_regs(&ptregs, 0);
				crashdump_mem("Code: ", instruction_pointer(&ptregs), instruction_pointer(&ptregs) + 32, 0, task);
				crashdump_mem("Stack: ", (&ptregs)->ARM_sp, (&ptregs)->ARM_sp + 128, 0, task);
				if (crashdump_is_dump_proc(task)||crashdump_is_dump_thread(task)) { 
					save_pid = task_tgid_nr(task);
					dprintk("1 save pid(%d)\n", save_pid);
				}
			}

			/*
			 * we cannot output backtrace here because
			 * the (&ptregs)->ARM_fp is zero. zeroed by
			 * vector_swi etc.
			 */
		}

		crashdump_write_str("*** Pid: %d, Tid: %d, PPid: %d, comm: %20s *** kernel context\n"
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

		if (crashdump_is_dump_proc(task) 
			|| crashdump_is_dump_thread(task) 
			|| (task_tgid_nr(task) == task_pid_nr(task))
			|| ((save_pid != 0)&&(save_pid==task_tgid_nr(task)))) {

			crashdump_regs(&ptregs, 1);
			crashdump_mem("Code: ", instruction_pointer(&ptregs), instruction_pointer(&ptregs) + 32, 1, task);
			crashdump_mem("Stack: ", (&ptregs)->ARM_sp, THREAD_SIZE + (unsigned long)task_stack_page(task), 1, NULL);
			if (crashdump_is_dump_proc(task)||crashdump_is_dump_thread(task)) { 
				save_pid = task_tgid_nr(task);
				dprintk("2 save pid(%d)\n", save_pid);
			}
		}
		crashdump_backtrace(task, &ptregs, 1);

		if (crashdump_is_dump_proc(task) 
			|| crashdump_is_dump_thread(task) 
			|| (task_tgid_nr(task) == task_pid_nr(task))
			|| ((save_pid != 0)&&(save_pid==task_tgid_nr(task)))) {

			wchan = get_wchan(task);
			crashdump_write_str("wchan: 0x%08x\n", (unsigned int)wchan);
			if (!(crashdump_is_dump_proc(task)||crashdump_is_dump_thread(task)||(save_pid==task_tgid_nr(task)))) {
				dprintk("3 clear pid(%d)\n", save_pid);
				save_pid=0;
			}
		} else {
			dprintk("4 clear pid(%d)\n", save_pid);
			save_pid=0;
		}
	} while_each_thread(g,task);
}
