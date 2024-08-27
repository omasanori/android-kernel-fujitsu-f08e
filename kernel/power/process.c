/*
 * drivers/power/process.c - Functions for starting/stopping processes on 
 *                           suspend transitions.
 *
 * Originally from swsusp.
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/


#undef DEBUG

#include <linux/interrupt.h>
#include <linux/oom.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/kmod.h>
#include <linux/wakelock.h>
#include "power.h"

/* 
 * Timeout for stopping processes
 */
#define TIMEOUT	(20 * HZ)

/* FUJITSU:2013-03-22 freeze log add start */
extern void freeze_workqueue_busy_logging(void);
#define LOG_TIMEOUT	(1 * HZ)
#define LOG_SPAN	(5 * HZ)
static void print_freezing_list( bool user_only )
{
	struct task_struct *g, *p;

	read_lock(&tasklist_lock);
	do_each_thread(g, p)
	{
		if ( !frozen(p) && freezing(p) ){
#ifdef CONFIG_SMP
			printk( "Freezing wait PID[%06d] TGID[%06d] FLAGS[0x%08x] "
					"STATE[0x%08x] on_cpu[%d] comm[%s] \n", 
					p->pid, 
					p->tgid, 
					(unsigned int)p->flags, 
					(unsigned int)p->state, 
					p->on_cpu, 
					&p->comm[0] );
#else
			printk( "Freezing wait PID[%06d] TGID[%06d] FLAGS[0x%08x] "
					"STATE[0x%08x] comm[%s] \n", 
					p->pid, 
					p->tgid, 
					(unsigned int)p->flags, 
					(unsigned int)p->state, 
					&p->comm[0] );
#endif
		}
	}
	while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	if ( !user_only ) {
		freeze_workqueue_busy_logging();
	}
}
/* FUJITSU:2013-03-22 freeze log add end */

static int try_to_freeze_tasks(bool user_only)
{
	struct task_struct *g, *p;
	unsigned long end_time;
	unsigned int todo;
	bool wq_busy = false;
	struct timeval start, end;
	u64 elapsed_csecs64;
	unsigned int elapsed_csecs;
	bool wakeup = false;
/* FUJITSU:2013-03-22 freeze log add start */
	int tolv = 1;
	unsigned long log_time, start_time;
/* FUJITSU:2013-03-22 freeze log add end */

	do_gettimeofday(&start);

	end_time = jiffies + TIMEOUT;
/* FUJITSU:2013-03-22 freeze log add start */
	start_time = jiffies;
	log_time = start_time + LOG_TIMEOUT;
/* FUJITSU:2013-03-22 freeze log add end */

	if (!user_only)
		freeze_workqueues_begin();

	while (true) {
		todo = 0;
		read_lock(&tasklist_lock);
		do_each_thread(g, p) {
			if (p == current || !freeze_task(p))
				continue;

			/*
			 * Now that we've done set_freeze_flag, don't
			 * perturb a task in TASK_STOPPED or TASK_TRACED.
			 * It is "frozen enough".  If the task does wake
			 * up, it will immediately call try_to_freeze.
			 *
			 * Because freeze_task() goes through p's scheduler lock, it's
			 * guaranteed that TASK_STOPPED/TRACED -> TASK_RUNNING
			 * transition can't race with task state testing here.
			 */
			if (!task_is_stopped_or_traced(p) &&
			    !freezer_should_skip(p))
				todo++;
		} while_each_thread(g, p);
		read_unlock(&tasklist_lock);

		if (!user_only) {
			wq_busy = freeze_workqueues_busy();
			todo += wq_busy;
		}

		if (todo && has_wake_lock(WAKE_LOCK_SUSPEND)) {
/* FUJITSU:2013-03-22 freeze log add start */
			if ( time_after(jiffies, start_time + LOG_TIMEOUT) ){
				printk( "Freezer has todo and wake lock \n" );
				print_freezing_list( user_only );
			}
/* FUJITSU:2013-03-22 freeze log add end */
			wakeup = 1;
			break;
		}

/* FUJITSU:2013-03-22 freeze log mod start */
		if ( !todo ){
			if ( time_after(jiffies, start_time + LOG_TIMEOUT) ){
				printk( "Freezer not todo \n" );
				print_freezing_list( user_only );
			}
			break;
		}

		if ( time_after(jiffies, end_time) ){
			printk( "Freezer timeout \n" );
			print_freezing_list( user_only );
			break;
		}
//		if (!todo || time_after(jiffies, end_time))
//			break;
/* FUJITSU:2013-03-22 freeze log mod end */

		if (pm_wakeup_pending()) {
/* FUJITSU:2013-03-22 freeze log add start */
			if ( time_after(jiffies, start_time + LOG_TIMEOUT) ){
				printk( "Freezer wakeup pending \n" );
				print_freezing_list( user_only );
			}
/* FUJITSU:2013-03-22 freeze log add end */
			wakeup = true;
			break;
		}

/* FUJITSU:2013-03-22 freeze log add start */
		if ( time_after(jiffies, log_time) && (tolv < 4) ){
			printk( "[%d] Freezer wait too long ... \n", tolv );
			print_freezing_list( user_only );
			log_time = start_time + tolv * LOG_SPAN;
			tolv++;
		}
/* FUJITSU:2013-03-22 freeze log add end */
		/*
		 * We need to retry, but first give the freezing tasks some
		 * time to enter the regrigerator.
		 */
		msleep(10);
	}

	do_gettimeofday(&end);
	elapsed_csecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
	do_div(elapsed_csecs64, NSEC_PER_SEC / 100);
	elapsed_csecs = elapsed_csecs64;

	if (todo) {
		/* This does not unfreeze processes that are already frozen
		 * (we have slightly ugly calling convention in that respect,
		 * and caller must call thaw_processes() if something fails),
		 * but it cleans up leftover PF_FREEZE requests.
		 */
		if(wakeup) {
			printk("\n");
			printk(KERN_ERR "Freezing of %s aborted\n",
					user_only ? "user space " : "tasks ");
		}
		else {
			printk("\n");
			printk(KERN_ERR "Freezing of tasks %s after %d.%02d seconds "
			       "(%d tasks refusing to freeze, wq_busy=%d):\n",
			       wakeup ? "aborted" : "failed",
			       elapsed_csecs / 100, elapsed_csecs % 100,
			       todo - wq_busy, wq_busy);
		}

		if (!wakeup) {
			read_lock(&tasklist_lock);
			do_each_thread(g, p) {
				if (p != current && !freezer_should_skip(p)
				    && freezing(p) && !frozen(p) &&
				    elapsed_csecs > 100)
					sched_show_task(p);
			} while_each_thread(g, p);
			read_unlock(&tasklist_lock);
		}
	} else {
		printk("(elapsed %d.%02d seconds) ", elapsed_csecs / 100,
			elapsed_csecs % 100);
	}

	return todo ? -EBUSY : 0;
}

/**
 * freeze_processes - Signal user space processes to enter the refrigerator.
 *
 * On success, returns 0.  On failure, -errno and system is fully thawed.
 */
int freeze_processes(void)
{
	int error;

	error = __usermodehelper_disable(UMH_FREEZING);
	if (error)
		return error;

	if (!pm_freezing)
		atomic_inc(&system_freezing_cnt);

	printk("Freezing user space processes ... ");
	pm_freezing = true;
	error = try_to_freeze_tasks(true);
	if (!error) {
		printk("done.");
		__usermodehelper_set_disable_depth(UMH_DISABLED);
		oom_killer_disable();
	}
	printk("\n");
	BUG_ON(in_atomic());

	if (error)
		thaw_processes();
	return error;
}

/**
 * freeze_kernel_threads - Make freezable kernel threads go to the refrigerator.
 *
 * On success, returns 0.  On failure, -errno and only the kernel threads are
 * thawed, so as to give a chance to the caller to do additional cleanups
 * (if any) before thawing the userspace tasks. So, it is the responsibility
 * of the caller to thaw the userspace tasks, when the time is right.
 */
int freeze_kernel_threads(void)
{
	int error;

	error = suspend_sys_sync_wait();
	if (error)
		return error;

	printk("Freezing remaining freezable tasks ... ");
	pm_nosig_freezing = true;
	error = try_to_freeze_tasks(false);
	if (!error)
		printk("done.");

	printk("\n");
	BUG_ON(in_atomic());

	if (error)
		thaw_kernel_threads();
	return error;
}

void thaw_processes(void)
{
	struct task_struct *g, *p;

	if (pm_freezing)
		atomic_dec(&system_freezing_cnt);
	pm_freezing = false;
	pm_nosig_freezing = false;

	oom_killer_enable();

	printk("Restarting tasks ... ");

	thaw_workqueues();

	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		__thaw_task(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	usermodehelper_enable();

	schedule();
	printk("done.\n");
}

void thaw_kernel_threads(void)
{
	struct task_struct *g, *p;

	pm_nosig_freezing = false;
	printk("Restarting kernel threads ... ");

	thaw_workqueues();

	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		if (p->flags & (PF_KTHREAD | PF_WQ_WORKER))
			__thaw_task(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	schedule();
	printk("done.\n");
}
