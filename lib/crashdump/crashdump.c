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
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/reboot.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

struct crashdump_ctr cctr;

int crashdump_inprogress = 0;
static int crashdump_prepare_done = 0;

static void crashdump_finish(void);
static void crashdump_restart(void);

static void crashdump_checkrecursion(void)
{
	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	crashdump_inprogress++;

	/* here crashdump_inprogress can have the following values
	 *  4: 4th time here. just give up (infinite loop)
	 *  3: 3rd time here. try to restart
	 *  2: 2nd time here. try to do some finishing
	 *  1: 1st time here. no recursion yet
	 */

	if (crashdump_inprogress > 3) {
		while(1) {};
	}

	if (crashdump_inprogress > 2) {
		printk(KERN_EMERG "Recursive panic. Giving up!\n");
		crashdump_restart();
		return;
	}

	if (crashdump_inprogress > 1) {
		printk(KERN_EMERG "Recursive panic.\n");

		if (crashdump_prepare_done) {
			printk(KERN_EMERG "Trying to do finishing.\n");
			crashdump_write_str("Recursive panic.\n");
			crashdump_finish();
		}

		crashdump_restart();
		return;
	}
}

static int crashdump_prepare(void)
{
	int retval __attribute__((unused));

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	retval = crashdump_findpartition();
	if (retval == 0) {
		printk(KERN_EMERG "Failed to find crashdump partition\n");
		return -1;
	}

	retval = crashdump_erasepartition();
	if (retval < 0) {
		printk(KERN_EMERG "Failed to erase crashdump partition\n");
		return -1;
	}
#endif

	crashdump_prepare_done = 1;
	return 0;
}

static void crashdump_finish(void)
{
	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	crashdump_flush();
}

static void crashdump_restart(void)
{
	printk(KERN_EMERG "Going to restart\n");

	emergency_restart();
}

extern pid_t save_pid;

void crashdump(void)
{
	int retval;

	save_pid = 0;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	printk(KERN_EMERG "Crashdump in progress\n");

	crashdump_checkrecursion();

	retval = crashdump_prepare();
	if (retval == -1) {
		printk(KERN_EMERG "Aborting crashdump\n");
		crashdump_restart();
		return;
	}

	local_irq_enable();

	crashdump_log_buf();

	crashdump_time();

	crashdump_logcat(LOGCAT_MAIN);
	crashdump_logcat(LOGCAT_SYSTEM);
	crashdump_logcat(LOGCAT_EVENTS);
	crashdump_logcat(LOGCAT_RADIO);

	crashdump_system_properties();

	crashdump_file("/proc/meminfo");
	crashdump_file("/proc/vmstat");
	crashdump_file("/proc/slabinfo");
	crashdump_file("/proc/zoneinfo");

	crashdump_taskinfo();
	crashdump_top();
	crashdump_procrank();
	crashdump_librank();

	crashdump_file("/proc/loadavg");
	crashdump_file("/proc/uptime");
	crashdump_file("/proc/version");
	crashdump_file("/proc/cmdline");
	crashdump_file("/proc/execdomains");

	crashdump_file("/proc/wakelocks");

	crashdump_file(CONFIG_CRASHDUMP_BINDER_DIR"/binder/failed_transaction_log");
	crashdump_file(CONFIG_CRASHDUMP_BINDER_DIR"/binder/transaction_log");
	crashdump_file(CONFIG_CRASHDUMP_BINDER_DIR"/binder/transactions");
	crashdump_file(CONFIG_CRASHDUMP_BINDER_DIR"/binder/stats");
	crashdump_file(CONFIG_CRASHDUMP_BINDER_DIR"/binder/state");

	crashdump_file("/proc/last_kmsg");

	crashdump_file("/data/anr/traces.txt");
	crashdump_file("/etc/event-log-tags");
	crashdump_file("/data/system/packages.xml");
	crashdump_file("/data/system/uiderrors.txt");

	crashdump_df();
	crashdump_file("/proc/filesystems");

	crashdump_netcfg();
	crashdump_file("/proc/net/route");

	crashdump_service();

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	crashdump_finish();

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	crashdump_restart();

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
}
