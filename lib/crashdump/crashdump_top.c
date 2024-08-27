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
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kernel_stat.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

struct cpu_info {
	long unsigned utime, ntime, stime, itime;
	long unsigned iowtime, irqtime, sirqtime;
};

struct proc_info {
	long unsigned utime;
	long unsigned stime;
};

static char cmdline[PAGE_SIZE];

/* get time statistics */
void crashdump_gettimestat(struct cpu_info *cpui, struct proc_info *proci)
{
	int i;
	u64 user, nice, system, idle, iowait, irq, softirq;
	struct task_struct *task_grp;
	struct task_struct *task_thr;

	// initialize to zero
	user = nice = system = idle = iowait = irq = softirq = 0;

	// add up the values of each CPU to get the total
	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += kcpustat_cpu(i).cpustat[CPUTIME_IDLE];
		iowait += kcpustat_cpu(i).cpustat[CPUTIME_IOWAIT];
		irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
	}

	// set return values
	cpui->utime = (long unsigned)cputime64_to_clock_t(user);
	cpui->ntime = (long unsigned)cputime64_to_clock_t(nice);
	cpui->stime = (long unsigned)cputime64_to_clock_t(system);
	cpui->itime = (long unsigned)cputime64_to_clock_t(idle);
	cpui->iowtime = (long unsigned)cputime64_to_clock_t(iowait);
	cpui->irqtime = (long unsigned)cputime64_to_clock_t(irq);
	cpui->sirqtime = (long unsigned)cputime64_to_clock_t(softirq);

	// get time statistics of the tasks
	i = 0;
	do_each_thread(task_grp, task_thr) {
		cputime_t utime, stime;
		task_times(task_thr, &utime, &stime);
		proci[i].utime = (long unsigned)cputime_to_clock_t(utime);
		proci[i].stime = (long unsigned)cputime_to_clock_t(stime);
		i++;
	} while_each_thread(task_grp, task_thr);
}

void crashdump_top(void)
{
	struct task_struct *task_grp;
	struct task_struct *task_thr;
	struct mm_struct *mm;
	unsigned long vsize;
	unsigned long rss;
	char state;
	struct cpu_info old_cpu, new_cpu;
	struct proc_info *old_proc, *new_proc, *proc_info_buff;
	long unsigned total_delta_time;
	long unsigned proc_delta_time;
	int i;
	int tasknum;
	int retval;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== top ===\n");

	//read_lock(&tasklist_lock);

	// count number of tasks
	tasknum = 0;
	do_each_thread(task_grp, task_thr) {
		tasknum++;
	} while_each_thread(task_grp, task_thr);

	// memory to hold old time stat
	proc_info_buff = kmalloc(sizeof(struct proc_info) * tasknum * 2, GFP_KERNEL);
	if (!proc_info_buff) {
		printk(KERN_EMERG "failed to allocate memory for proc info\n");
		goto exit;
	}

	old_proc = proc_info_buff;
	new_proc = old_proc + tasknum;

	// get time stat
	crashdump_gettimestat(&old_cpu, old_proc);

	// wait
	mdelay(1000);

	// get time stat again
	crashdump_gettimestat(&new_cpu, new_proc);

	// difference between old and new
	total_delta_time = (new_cpu.utime + new_cpu.ntime + new_cpu.stime + new_cpu.itime
						+ new_cpu.iowtime + new_cpu.irqtime + new_cpu.sirqtime)
		- (old_cpu.utime + old_cpu.ntime + old_cpu.stime + old_cpu.itime
		   + old_cpu.iowtime + old_cpu.irqtime + old_cpu.sirqtime);

	crashdump_write_str("User %ld%%, System %ld%%, IOW %ld%%, IRQ %ld%%\n",
		   ((new_cpu.utime + new_cpu.ntime) - (old_cpu.utime + old_cpu.ntime)) * 100  / total_delta_time,
		   ((new_cpu.stime ) - (old_cpu.stime)) * 100 / total_delta_time,
		   ((new_cpu.iowtime) - (old_cpu.iowtime)) * 100 / total_delta_time,
		   ((new_cpu.irqtime + new_cpu.sirqtime)
			- (old_cpu.irqtime + old_cpu.sirqtime)) * 100 / total_delta_time);

	crashdump_write_str("User %ld + Nice %ld + Sys %ld + Idle %ld + IOW %ld + IRQ %ld + SIRQ %ld = %ld\n\n",
		   new_cpu.utime - old_cpu.utime,
		   new_cpu.ntime - old_cpu.ntime,
		   new_cpu.stime - old_cpu.stime,
		   new_cpu.itime - old_cpu.itime,
		   new_cpu.iowtime - old_cpu.iowtime,
		   new_cpu.irqtime - old_cpu.irqtime,
		   new_cpu.sirqtime - old_cpu.sirqtime,
		   total_delta_time);

	crashdump_write_str("%5s %5s %4s %1s %7s %7s %-8s %-15s %s\n", "PID", "TID", "CPU%", "S", "VSS", "RSS", "UID", "Thread", "Proc");

	i = 0;
	do_each_thread(task_grp, task_thr) {
		mm = get_task_mm(task_thr);
		if (!mm) {
			continue;
		}

		vsize = task_vsize(mm);
		rss = get_mm_rss(mm);
		state = *crashdump_get_task_state(task_thr);

		// difference between old and new (for process time stat)
		proc_delta_time = (new_proc[i].utime + new_proc[i].stime)
			- (old_proc[i].utime + old_proc[i].stime);

		// get commandline
		retval = crashdump_pid_cmdline(task_grp, cmdline, sizeof(cmdline));
		if (!retval) {
			cmdline[0] = '\0';
		}

		crashdump_write_str("%5d %5d %3ld%% %c %6ldK %6ldK %-8.8d %-15s %s\n", task_pid_nr(task_grp), task_pid_nr(task_thr), proc_delta_time * 100 / total_delta_time, state,
			   vsize / 1024, rss * PAGE_SIZE / 1024, task_grp->real_cred->uid, task_thr->comm, cmdline);
		i++;
		mmput(mm);
	} while_each_thread(task_grp, task_thr);

	kfree(proc_info_buff);

exit:
	//read_unlock(&tasklist_lock);

	return;
}
