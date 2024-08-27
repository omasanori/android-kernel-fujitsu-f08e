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

void crashdump_doprocrank(struct task_struct *task)
{
	struct mm_walk pagemap_walk = {};
	struct mm_struct *mm;

	vss = rss = pss = uss = 0;

	dprintk2("%5d (%s)\n", task_pid_nr(task), task->comm);

	mm = get_task_mm(task);
	if (!mm) {
		goto exit0;
	}

	pagemap_walk.pte_entry = pagemap_pte;
	pagemap_walk.mm = mm;
	pagemap_walk.private = NULL;
	walk_page_range(0, TASK_SIZE_OF(task), &pagemap_walk);

	dprintk("%5d  %6dK  %6dK  %6dK  %6dK  %s\n",
			task_pid_nr(task),
			vss / 1024,
			rss / 1024,
			pss / 1024,
			uss / 1024,
			task->comm
		);
	crashdump_write_str("%5d  %6dK  %6dK  %6dK  %6dK  %s\n",
			task_pid_nr(task),
			vss / 1024,
			rss / 1024,
			pss / 1024,
			uss / 1024,
			task->comm
		);

	mmput(mm);

exit0:
	return;
}

void crashdump_procrank(void)
{
	struct task_struct *task;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== procrank ===\n");

	dprintk("%5s  %7s  %7s  %7s  %7s  %s\n", "PID", "Vss", "Rss", "Pss", "Uss", "cmdline");
	crashdump_write_str("%5s  %7s  %7s  %7s  %7s  %s\n", "PID", "Vss", "Rss", "Pss", "Uss", "cmdline");

	for_each_process(task) {
		crashdump_doprocrank(task);
	}
}
