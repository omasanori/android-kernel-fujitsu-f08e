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
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

//#define dprintk2 printk
#define dprintk2(...)

static size_t vss, rss, pss, uss;

static int pagemap_pte(pte_t *pte, unsigned long start, unsigned long end, struct mm_walk *walk)
{
	struct page *ppage;
	unsigned long pfn;
	int count;
	pte_t ptent;

	dprintk2(KERN_EMERG "(%s) 0x%08x: (0x%08lx - 0x%08lx)\n", __FUNCTION__, (u32)pte, start, end);

	ptent = *pte;
	ppage = NULL;
	count = 0;

	if ( (pte_present(ptent)) && !(is_swap_pte(ptent)) ) {
		vss += PAGE_SIZE;

		pfn = PM_PFRAME(pte_pfn(ptent));
		if (pfn_valid(pfn)) {
			ppage = pfn_to_page(pfn);
		}

		if (ppage) {
			count = page_mapcount(ppage);
		}

		rss += (count >= 1) ? (PAGE_SIZE) : (0);
		pss += (count >= 1) ? (PAGE_SIZE / count) : (0);
		uss += (count == 1) ? (PAGE_SIZE) : (0);
	}

	return 0;
}

void crashdump_dolibrank(struct task_struct *task)
{
	struct mm_walk pagemap_walk = {};
	struct mm_struct *mm;
	char *mapname;
	struct file *file;
	unsigned long ino;
	dev_t dev;
	int flags;
	struct vm_area_struct *vma;

	dprintk(" %6d (%s)\n", task_pid_nr(task), task->comm);
	crashdump_write_str(" %6d (%s)\n", task_pid_nr(task), task->comm);

	mm = get_task_mm(task);
	if (!mm) {
		goto exit0;
	}

	pagemap_walk.pte_entry = pagemap_pte;
	pagemap_walk.mm = mm;
	pagemap_walk.private = NULL;

	for(vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

		vss = rss = pss = uss = 0;
		ino = 0;
		dev = 0;
		mapname = "[heap]/[stack]/[vdso]";

		file = vma->vm_file;
		flags = vma->vm_flags;

		if (file && file->f_path.dentry) {
			struct inode *inode = file->f_path.dentry->d_inode;
			dev = inode->i_sb->s_dev;
			ino = inode->i_ino;
			mapname = (char *)(file->f_path.dentry->d_name.name);
		}

		dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

		dprintk2("%08lx-%08lx %c%c%c%c %08llx %02x:%02x %lu %s\n",
				vma->vm_start,
				vma->vm_end,
				flags & VM_READ ? 'r' : '-',
				flags & VM_WRITE ? 'w' : '-',
				flags & VM_EXEC ? 'x' : '-',
				flags & VM_MAYSHARE ? 's' : 'p',
				((loff_t)vma->vm_pgoff) << PAGE_SHIFT,
				MAJOR(dev), MINOR(dev), ino, mapname);

		dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

		if (!file || !file->f_path.dentry) {
			continue;
		}

		walk_page_range(vma->vm_start, vma->vm_end, &pagemap_walk);

		dprintk(" %6s  %6dK  %6dK  %6dK  %6dK    %s\n", "",
				vss / 1024,
				rss / 1024,
				pss / 1024,
				uss / 1024,
				mapname
			);
		crashdump_write_str(" %6s  %6dK  %6dK  %6dK  %6dK    %s\n", "",
				vss / 1024,
				rss / 1024,
				pss / 1024,
				uss / 1024,
				mapname
			);
	}

	mmput(mm);

exit0:
	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return;
}

void crashdump_librank(void)
{
	struct task_struct *task;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== librank ===\n");

	dprintk(" %6s   %6s   %6s   %6s   %6s    %s\n", "Proc", "VSS", "RSS", "PSS", "USS", "Library");
	crashdump_write_str(" %6s   %6s   %6s   %6s   %6s    %s\n", "Proc", "VSS", "RSS", "PSS", "USS", "Library");

	for_each_process(task) {
		crashdump_dolibrank(task);
	}
}
