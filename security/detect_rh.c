/*
 * Copyright(C) 2011-2012 FUJITSU LIMITED
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
#include <linux/kmod.h>

//#define dprintk printk
#define dprintk(...)

void set_rhflag(void)
{
	int retval;

	char *envp[] = {
		"HOME=/",
		"PATH=/system/bin",
		NULL };

	char *argv[] = {
		"/system/bin/set_rhflag",
		NULL };

	printk(KERN_INFO "calling helper '%s'\n", argv[0]);
	
	retval = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	
	printk(KERN_INFO "helper '%s' returned %d\n", argv[0], retval);

	if (retval < 0) {
		printk(KERN_ERR "Failed to run helper (%s) %d\n", argv[0], retval);
	}
}
