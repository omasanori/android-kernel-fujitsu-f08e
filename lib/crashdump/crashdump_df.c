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
#include <linux/nsproxy.h>
#include <linux/mnt_namespace.h>
#include <linux/fs.h>
#include <linux/statfs.h>
#include <linux/mount.h>
#include "mount.h"
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

void crashdump_df(void)
{
	struct nsproxy *nsp;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== df ===\n");

	rcu_read_lock();

	nsp = task_nsproxy(current);
	if( nsp ){

		struct mnt_namespace *ns;
		struct list_head *lh;

		ns = nsp->mnt_ns;

		// walk through mnt tree
		list_for_each(lh, &ns->list){

			struct mount *r;
			struct vfsmount *mnt;
			struct path mnt_path;
			char buf[128];
			char *str_path;
			struct kstatfs stat_buf;
			int err = -EINVAL;
			struct path path;

			// get the 'struct vfsmount' pointer from list_head
			r = list_entry(lh, struct mount, mnt_list);
			mnt = &r->mnt;

			// get stat
			path.mnt = mnt;
			path.dentry = mnt->mnt_root;
			err = vfs_statfs(&path, &stat_buf);
			if( err || stat_buf.f_blocks == 0 ){
				continue;
			}

			// get abs path
			mnt_path.dentry = mnt->mnt_root;
			mnt_path.mnt = mnt;
			memset(buf, 0, 128);
			str_path = d_path(&mnt_path, buf, 128);

			// print
			crashdump_write_str(
				"%s: %lldK total, %lldK used, %lldK available (block size %ld)\n",
				str_path,
				((long long)stat_buf.f_bsize * (long long)stat_buf.f_blocks) / 1024,
				((long long)stat_buf.f_bsize * (long long)(stat_buf.f_blocks - (long long)stat_buf.f_bfree) ) / 1024,
				((long long)stat_buf.f_bsize * (long long)stat_buf.f_bfree) / 1024,
				stat_buf.f_bsize );
		}

	}

	rcu_read_unlock();
}
