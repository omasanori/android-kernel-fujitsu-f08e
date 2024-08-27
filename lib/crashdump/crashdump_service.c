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
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/kmod.h>
#include <asm/uaccess.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

void crashdump_service(void)
{
	int retval;
	int lretval;
	mm_segment_t fs;

	char *envp[] = {
		"HOME=/",
		"TERM=vt102",
		"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
		NULL };

	char *argv[] = {
		"/sbin/dumpsys.sh",
		DUMPSYSFILENAME,
		NULL };

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== Services ===\n");

	dprintk(KERN_EMERG "(%s:%s:%d) calling helper '%s'\n", __FILE__, __FUNCTION__, __LINE__, argv[0]);

	retval = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);

	dprintk(KERN_EMERG "(%s:%s:%d) helper returned = %d\n", __FILE__, __FUNCTION__, __LINE__, retval);

	if (retval < 0) {
		printk(KERN_EMERG "Failed to run helper (%s) %d\n", argv[0], retval);
		goto exit0;
	}

	crashdump_file(DUMPSYSFILENAME);

	fs = get_fs();
	set_fs(KERNEL_DS);
	lretval = sys_unlink(DUMPSYSFILENAME);
	set_fs(fs);

	dprintk(KERN_EMERG "(%s:%s:%d) sys_unlink returned = %d\n", __FILE__, __FUNCTION__, __LINE__, lretval);

	sys_sync();

exit0:
	return;
}
