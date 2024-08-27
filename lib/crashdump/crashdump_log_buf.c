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
#include <linux/string.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

void crashdump_log_buf(void)
{
	char *abuf;
	int alen;
	unsigned int log_start;
	unsigned int logged_chars;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_printk_register();

	crashdump_write_str("\n=== Kernel log ===\n");

	log_start = *(cctr.pctr.log_start);
	logged_chars = *(cctr.pctr.logged_chars);

	log_start &= *(cctr.pctr.log_buf_len) - 1;

	abuf = cctr.pctr.__log_buf + log_start;
	alen = logged_chars - log_start;
	crashdump_write(abuf, alen);

	if (log_start != 0) {
		abuf = cctr.pctr.__log_buf;
		alen = log_start;
		crashdump_write(abuf, alen);
	}
}
