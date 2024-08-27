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
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

//#define dprintk2 printk
#define dprintk2(...)

/*
 * Read buffer
 * This buffer is for holding file read data
 * which is then passed to crashdump_write.
 * This can be any size.
 */
static char readbuf[READBUFSZ];

void crashdump_file(const char *filename)
{
	struct file *filp;
	int retlen;
	mm_segment_t fs;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s:%s\n", __FUNCTION__, filename);
#endif

	fs = get_fs();
	set_fs(KERNEL_DS);

	dprintk(KERN_EMERG "(%s:%s:%d) <%s>\n", __FILE__, __FUNCTION__, __LINE__, filename);
	crashdump_write_str("\n=== %s ===\n", filename);

	filp = filp_open(filename, O_RDONLY, 0);
	if ( IS_ERR(filp) ) {
		printk(KERN_EMERG "failed to open file: <%s>\n", filename);
		goto exit0;
	}

	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	dprintk2(KERN_EMERG "(%s:%s:%d) filp->f_op->read: 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, (unsigned int)(filp->f_op->read));
	if ( (filp->f_op == NULL) || (filp->f_op->read == NULL) ) {
		printk(KERN_EMERG "failed to read file: <%s>. read op not available\n", filename);
		goto exit1;
	}

	while (1) {
		dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
		retlen = filp->f_op->read(filp, readbuf, sizeof(readbuf), &filp->f_pos);
		dprintk2(KERN_EMERG "(%s:%s:%d) retlen = %d\n", __FILE__, __FUNCTION__, __LINE__, retlen);
		if (retlen < 0) {
			printk(KERN_EMERG "failed to read file: <%s>. read returned (%d)\n", filename, retlen);
			break;
		}

		if (retlen == 0) {
			break;
		}

		crashdump_write(readbuf, retlen);
	}

exit1:
	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	filp_close(filp, current->files);

exit0:
	set_fs(fs);
}

int crashdump_read_file(const char *filename, void *buf, size_t len)
{
	struct file *filp;
	int retlen;
	mm_segment_t fs;
	void *p;
	int len1;
	int myret;

	myret = 0;
#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s:%s\n", __FUNCTION__, filename);
#endif

	fs = get_fs();
	set_fs(KERNEL_DS);

	dprintk(KERN_EMERG "(%s:%s:%d) <%s>\n", __FILE__, __FUNCTION__, __LINE__, filename);

	filp = filp_open(filename, O_RDONLY, 0);
	if ( IS_ERR(filp) ) {
		printk(KERN_EMERG "failed to open file: <%s>\n", filename);
		goto exit0;
	}

	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	dprintk2(KERN_EMERG "(%s:%s:%d) filp->f_op->read: 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, (unsigned int)(filp->f_op->read));
	if ( (filp->f_op == NULL) || (filp->f_op->read == NULL) ) {
		printk(KERN_EMERG "failed to read file: <%s>. read op not available\n", filename);
		goto exit1;
	}

	p = buf;
	len1 = len;
	while (1) {
		dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
		retlen = filp->f_op->read(filp, p, len1, &filp->f_pos);
		dprintk2(KERN_EMERG "(%s:%s:%d) retlen = %d\n", __FILE__, __FUNCTION__, __LINE__, retlen);
		if (retlen < 0) {
			printk(KERN_EMERG "failed to read file: <%s>. read returned (%d)\n", filename, retlen);
			break;
		}

		if (retlen == 0) {
			break;
		}

		p += retlen;
		len1 -= retlen;
	}

	myret = len - len1;

exit1:
	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	filp_close(filp, current->files);

exit0:
	set_fs(fs);
	return myret;
}
