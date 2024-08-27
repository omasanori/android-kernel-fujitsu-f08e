/*
  * Copyright(C) 2012-2013 FUJITSU LIMITED
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pagemap.h>
#include <linux/genhd.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/debugfs.h>

#include <asm/uaccess.h>
#include <mach/msm_smsm.h>

#include <mach/board.h>

#include "nonvolatile.h"

static int get_nonvolatile_sub(uint8_t *, unsigned long, unsigned long, unsigned long, unsigned long*);
static int set_nonvolatile_sub(uint8_t *, unsigned long, unsigned long, unsigned long, unsigned long*);
static int set_nonvolatile_no_lock(uint8_t* , unsigned long, unsigned long);
static void nonvolatile_debugfs_init(void);
static void nonvolatile_debugfs_remove(void);

struct mutex nv_lock;

#define DRIVER_NAME	"nonvolatile"
#define CLASS_NAME	"nonvolatile"
#define DEVICE_NAME	"nonvolatile"

#define NONVOLATILE_HEADER_SIZE	12
#define MAX_RETRY_COUNT				5
#define RETRY_WAIT					50	// 50ms
#define MAX_RECONSTRUCT_COUNT		2
#define SIZE_4KB					(sizeof(uint8_t) * 4096)
#define BACKUP_AREA_DATA_OFFSET		(sizeof(uint8_t) * 2 * 1024 * 1024)
#define SIZE_1MB					(sizeof(uint8_t) * 1 * 1024 * 1024)
#define SIZE_3MB					(sizeof(uint8_t) * 3 * 1024 * 1024)
#define SIZE_5MB					(sizeof(uint8_t) * 5 * 1024 * 1024)
#define MAX_ITEM_NUM				60000
#define MAX_ITEM_SIZE				SIZE_1MB
#define MAX_TOTAL_ITEM_SIZE			((5 * 1024 * 1024) - (sizeof(struct nv_info_area) + (sizeof(struct nv_item_info) * 5)))
#define MAX_DATA_AREA_OFFSET		(5 * 1024 * 1024)
#define BACKUP_AREA_MAX_ITEM_NUM	((BACKUP_AREA_DATA_OFFSET / 16) - 1)

#define NONVOLATILE_DEBUG	(1<<0)
#define NONVOLATILE_DEBUG2	(1<<1)
#define NONVOLATILE_DEBUG3	(1<<2)
#define MIN_DEBUG_LOGLEVEL	0
#define MAX_DEBUG_LOGLEVEL	(NONVOLATILE_DEBUG | NONVOLATILE_DEBUG2 | NONVOLATILE_DEBUG3)

#define nprintk(fmt, ...)	\
	if( nonvolatile_log_flg & NONVOLATILE_DEBUG) {	\
		printk(pr_fmt(fmt), ##__VA_ARGS__);	\
	}

#define n2printk(fmt, ...)	\
	if( nonvolatile_log_flg & NONVOLATILE_DEBUG2) {	\
		printk(pr_fmt(fmt), ##__VA_ARGS__);	\
	}

#define n3printk(fmt, ...)	\
	if( nonvolatile_log_flg & NONVOLATILE_DEBUG3) {	\
		printk(pr_fmt(fmt), ##__VA_ARGS__);	\
	}

static int nonvolatile_log_flg = MIN_DEBUG_LOGLEVEL;

#ifdef CONFIG_DEBUG_FS
static struct dentry *nonvolatile_debugfs = NULL;
#endif /* CONFIG_DEBUG_FS */


static unsigned int nonvolatile_devs	= 1; /* device count */
static unsigned int nonvolatile_major	= 0;
static unsigned int nonvolatile_minor	= 0;
static struct cdev nonvolatile_cdev;
static struct class	 *nonvolatile_class;


const uint8_t nv_signature[4]			= { 'N', 'V',  0,   0  };
const uint8_t reconstruct_signature[4]	= { 'R', 'C', 'S', 'T' };
const uint8_t backup_signature[4]		= { 'B', 'K', 'U', 'P' };


static struct nv_info_area	g_nv_master_info_area;
static struct nv_info_area	g_nv_emmc_info_area;


enum nv_data_kind
{
	nv_kind_normal = 0,
	nv_kind_secure_normal,
	nv_kind_backup,
	nv_kind_secure_backup,
	nv_kind_count
};


struct nv_data_table
{
	unsigned long			nv_data_area_offset;
	unsigned int			nv_item_num;
	unsigned long			nv_item_total_size;
	struct nv_item			*nv_item_data;
};
static struct nv_data_table g_nv_data_table[4] = {{0, 0, 0, NULL},
													{0 ,0, 0, NULL},
													{0, 0, 0, NULL},
													{0, 0, 0, NULL}};


static unsigned int		g_early_init_item_num	= 0;
static struct nv_item	*g_early_init_item_data	= NULL;


struct nonvolatile_device {
	struct mutex lock;
	bool initialized;
	bool early_initialized;
};
static struct nonvolatile_device *nonvolatile_device;


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

extern int makercmd_mode;

static bool is_mc_mode(void)
{
	if (makercmd_mode == 1) {
		return true;
	} else {
		return false;
	}
}


inline
static int encrypted_size(int size)
{
	return (size + 15) / 16 * 16;
}


inline
static int auto_resize(unsigned int flag, int size)
{
	return ((flag & nv_item_flag_secure) ? encrypted_size(size) : size);
}


static struct block_device * block_finddev(const char *name)
{
	struct class_dev_iter	diter;
	struct device				*dev;
	struct gendisk			*disk;
	struct disk_part_iter	piter;
	struct hd_struct			*part;
	struct block_device		*bdev = NULL;

	class_dev_iter_init(&diter, &block_class, NULL, NULL);
	while((bdev == NULL) &&
	      ((dev = class_dev_iter_next(&diter)) != NULL)) {

		if(strcmp(dev->type->name, "disk") != 0) {
			continue;
		}

		disk = dev_to_disk(dev);

		if(get_capacity(disk) == 0 ||
		   (disk->flags & GENHD_FL_SUPPRESS_PARTITION_INFO)) {
			continue;
		}

		disk_part_iter_init(&piter, disk, DISK_PITER_INCL_PART0);

		while((part = disk_part_iter_next(&piter)) != NULL) {
			if(part->info != NULL) {
				if(strcmp(part->info->volname, name) == 0) {
					bdev = bdget(part_devt(part));
					break;
				}
			}
		}

		disk_part_iter_exit(&piter);
	}
	class_dev_iter_exit(&diter);

	return bdev;
}


static int block_read(const char *devname, loff_t offset, uint8_t *buf, size_t len)
{
	int i;
	struct block_device *bdev;
	pgoff_t index;
	size_t start;
	size_t size = 0;
	struct page *page;
	fmode_t mode = FMODE_READ;
	int ret;


	if (!devname) {
		printk(KERN_ERR "APPNV_ERROR: devname is NULL: %s\n", __func__);
		return -1;
	}

	if (!buf) {
		printk(KERN_ERR "APPNV_ERROR: buf is NULL: %s\n", __func__);
		return -1;
	}
	
	if (offset < 0 || offset >= SIZE_5MB) {
		printk(KERN_ERR "APPNV_ERROR: input param(offset) error: %s\n", __func__);
		return -1;
	}

	if (len == 0 || len > SIZE_5MB) {
		printk(KERN_ERR "APPNV_ERROR: input param(len) error: %s\n", __func__);
		return -1;
	}

	if (strcmp(devname, NONVOLATILE_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_BACKUP_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_MASTER_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_BOOT_MODE_PARTITION_NAME) != 0) {
		printk(KERN_ERR "APPNV_ERROR: devname error(%s): %s\n", devname, __func__);
		return -1;
	}

	if (strcmp(devname, NONVOLATILE_BOOT_MODE_PARTITION_NAME) == 0 && offset >= SIZE_1MB) {
		printk(KERN_ERR "APPNV_ERROR: offset range error: %s\n", __func__);
		return -1;
	}

	mutex_lock(&nonvolatile_device->lock);

	bdev = block_finddev(devname);
	if(!bdev) {
		printk(KERN_ERR "APPNV_ERROR: block_finddev(): %s\n", __func__);
		mutex_unlock(&nonvolatile_device->lock);
		return -ENODEV;
	}

	ret = blkdev_get(bdev, mode, NULL);
	if(ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: blkdev_get(): %s\n", __func__);
		mutex_unlock(&nonvolatile_device->lock);
		return ret;
	}

	nprintk(KERN_INFO "APPNV_DEBUG: PAGE_SHIFT = %d, PAGE_SIZE = %ld\n", PAGE_SHIFT, PAGE_SIZE);

	index = offset >> PAGE_SHIFT;
	start = offset % PAGE_SIZE;

	for (i = 0; i < MAX_RETRY_COUNT; i++) {
		do {
			if((start + len) > PAGE_SIZE) {
				nprintk(KERN_INFO "APPNV: size = %d\n", size);
				size = PAGE_SIZE - start;
			} else {
				nprintk(KERN_INFO "APPNV: size = %d\n", size);
				size = len;
			}

			page = read_mapping_page(bdev->bd_inode->i_mapping, index, NULL);

			nprintk(KERN_INFO "APPNV: page = %p\n", page);
			if(!page) {
				printk(KERN_ERR "APPNV_ERROR: read_mapping_page(): %s\n", __func__);
				ret = -ENOMEM;
				break;
			}

			if(IS_ERR(page)) {
				printk(KERN_ERR "APPNV_ERROR: read_mapping_page(): %s\n", __func__);
				ret = PTR_ERR(page);
				break;
			}

			memcpy(buf, page_address(page) + start, size);
			page_cache_release(page);
			index++;
			start = 0;
			buf += size;
			len -= size;
			nprintk(KERN_INFO "APPNV: index = %ld, buf = %p, len = %u\n", index, buf, len);
		} while(len > 0);

		blkdev_put(bdev, mode);
		if(ret == 0){
			break;
		}
		msleep(RETRY_WAIT);
	}

	if (i == MAX_RETRY_COUNT) {
		ret = -1;
	}

	mutex_unlock(&nonvolatile_device->lock);

	return ret;
}


static int block_write(const char *devname, loff_t offset, const uint8_t *buf, size_t len)
{
	int i;
	struct block_device *bdev;
	pgoff_t index;
	size_t start;
	size_t size;
	struct page *page;
	fmode_t mode = FMODE_READ | FMODE_WRITE;

	int ret;


	if (strcmp(devname, NONVOLATILE_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_BACKUP_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_MASTER_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_BOOT_MODE_PARTITION_NAME) != 0) {
		printk(KERN_ERR "APPNV_ERROR: devname error(%s): %s\n", devname, __func__);
		return -1;
	}

	if (strcmp(devname, NONVOLATILE_BOOT_MODE_PARTITION_NAME) == 0 && offset >= SIZE_1MB) {
		printk(KERN_ERR "APPNV_ERROR: offset range error: %s\n", __func__);
		return -1;
	}

	mutex_lock(&nonvolatile_device->lock);

	bdev = block_finddev(devname);
	if(bdev == NULL) {
		printk(KERN_ERR "APPNV_ERROR: block_finddev(): %s\n", __func__);
		mutex_unlock(&nonvolatile_device->lock);
		return -ENODEV;
	}

	ret = blkdev_get(bdev, mode, NULL);
	if(ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: blkdev_get(): %s\n", __func__);
		mutex_unlock(&nonvolatile_device->lock);
		return ret;
	}

	nprintk(KERN_INFO "APPNV_DEBUG: PAGE_SHIFT = %d, PAGE_SIZE = %ld\n", PAGE_SHIFT, PAGE_SIZE);

		index = offset >> PAGE_SHIFT;
		start = offset % PAGE_SIZE;

	for (i = 0; i < MAX_RETRY_COUNT; i++) {
		do {
			if((start + len) > PAGE_SIZE) {
				size = PAGE_SIZE - start;
			}
			else {
				size = len;
			}

			page = read_mapping_page(bdev->bd_inode->i_mapping, index, NULL);
			if(page == NULL) {
				printk(KERN_ERR "APPNV_ERROR: read_mapping_page(): %s\n", __func__);
				ret = -ENOMEM;
				break;
			}
			if(IS_ERR(page)) {
				printk(KERN_ERR "APPNV_ERROR: read_mapping_page(): %s\n", __func__);
				ret = PTR_ERR(page);
				break;
			}

			lock_page(page);
			if(memcmp(page_address(page) + start, buf, size)) {	
				memcpy(page_address(page) + start, buf, size);
				set_page_dirty(page);
			}
			unlock_page(page);
			page_cache_release(page);

			index++;
			start = 0;
			buf += size;
			len -= size;
			nprintk(KERN_INFO "APPNV: index = %ld, buf = %p, len = %u\n", index, buf, len);
		} while(len > 0);

		sync_blockdev(bdev);
		blkdev_put(bdev, mode);

		if (ret == 0) {
			break;
		}
		msleep(RETRY_WAIT);
	}
	
	if (i == MAX_RETRY_COUNT) {
		ret = -1;
	}

	mutex_unlock(&nonvolatile_device->lock);

	return ret;
}


static int block_write_verify(const char *devname, const loff_t offset, const uint8_t *buf, const size_t len)
{
	uint8_t *read_buf		= NULL;
	uint8_t *org_data_buf	= NULL;

	bool error_flag			= false;
	int result				= 0;


	if (!devname) {
		printk(KERN_ERR "APPNV_ERROR: devname is NULL: %s\n", __func__);
		return -1;
	}

	if (!buf) {
		printk(KERN_ERR "APPNV_ERROR: buf is NULL: %s\n", __func__);
		return -1;
	}
	
	if (offset < 0 || offset >= SIZE_5MB) {
		printk(KERN_ERR "APPNV_ERROR: offset range error (MAX 5MB): %s\n", __func__);
		return -1;
	}

	if (len == 0) {
		printk(KERN_ERR "APPNV_ERROR: input param(len) error: %s\n", __func__);
		return -1;
	}

	if (strcmp(devname, NONVOLATILE_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_BACKUP_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_MASTER_PARTITION_NAME) != 0
		&& strcmp(devname, NONVOLATILE_BOOT_MODE_PARTITION_NAME) != 0) {
		printk(KERN_ERR "APPNV_ERROR: devname error(%s): %s\n", devname, __func__);
		return -1;
	}

	if (strcmp(devname, NONVOLATILE_BOOT_MODE_PARTITION_NAME) == 0 && offset >= SIZE_1MB) {
		printk(KERN_ERR "APPNV_ERROR: offset range error (MAX 1MB: ): %s\n", __func__);
		return -1;
	}

	read_buf = kzalloc(sizeof(uint8_t) * len, GFP_KERNEL);
	if (!read_buf) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
		return -EFAULT;
	}

	org_data_buf = kzalloc(sizeof(uint8_t) * len, GFP_KERNEL);
	if (!org_data_buf) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
		result = -EFAULT;
		goto read_buf_free;
	}

	result = block_read(devname, offset, org_data_buf, len);
	if(result < 0){
		printk(KERN_ERR "APPNV_INFO: eMMC read error.\n");
		goto org_data_buf_free;
	}

	result = block_write(devname, offset, buf, len);
	if(result < 0){
		printk(KERN_ERR "APPNV_INFO: eMMC write error.\n");
		error_flag = true;
	}
	
	if (!error_flag) {
		result = block_read(devname, offset, read_buf, len);
		if(result < 0){
			printk(KERN_ERR "APPNV_INFO: eMMC read check error.\n");
			error_flag = true;
		}
	}

	if (!error_flag) {
		if(memcmp(buf, read_buf, len) == 0){
			result = 0;
			goto org_data_buf_free;
		} else {
			printk(KERN_ERR "APPNV_INFO: write data check error.\n");
			error_flag = true;
		}
	}
	
	if (error_flag) {
		result = block_write(devname, offset, org_data_buf, len);
		if(result < 0){
			printk(KERN_ERR "APPNV_ERROR: recovery error(block_write): %s\n", __func__);
			result = -2;
			goto org_data_buf_free;
		}
		
		result = block_read(devname, offset, read_buf, len);
		if(result < 0){
			printk(KERN_ERR "APPNV_ERROR: recovery error(block_read): %s\n", __func__);
			result = -2;
			goto org_data_buf_free;
		}

		if (memcmp(org_data_buf, read_buf, len) == 0) {
			printk(KERN_ERR "APPNV_ERROR: recovery error(check error): %s\n", __func__);
			result = -1;
			goto org_data_buf_free;
		}
	}

org_data_buf_free:
	if(org_data_buf){
		kfree(org_data_buf);
		org_data_buf = NULL;
	}

read_buf_free:
	if(read_buf){
		kfree(read_buf);
		read_buf = NULL;
	}

	return result;
}


void free_backup_item_list(struct backup_item_list **list)
{
	int i;


	if (*list) {
		if ((*list)->backup_item) {
			for(i = 0; i < (*list)->backup_item_num; i++){
				if ((*list)->backup_item[i].item_data) {
					kfree((*list)->backup_item[i].item_data);
					(*list)->backup_item[i].item_data = NULL;
				}
			}
			kfree((*list)->backup_item);
			(*list)->backup_item = NULL;
		}
		kfree(*list);
		(*list) = NULL;
	}

	return;
}


int get_backup_item_list(struct backup_item_list **valid_backup_item_list)
{
	int i = 0;
	int j = 0;


	struct nv_backup_area_info	backup_item_area_info;
	struct nv_item_info			*item_info_buffer		= NULL;
	uint8_t						*item_data_buffer		= NULL;

	int valid_backup_item_num	= 0;
	int id_check_list_num		= 0;
	int *id_check_list			= NULL;

	int ret = 0;

	(*valid_backup_item_list) = kzalloc(sizeof(struct backup_item_list), GFP_KERNEL);
	if (!(*valid_backup_item_list)) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(valid_backup_item_list): %s\n", __func__);
		return -EFAULT;
	}

	(*valid_backup_item_list)->backup_item_num = 0;
	(*valid_backup_item_list)->backup_item = NULL;

	ret = block_read(NONVOLATILE_BACKUP_PARTITION_NAME,
						0,
						(uint8_t *)&backup_item_area_info,
						sizeof(struct nv_backup_area_info));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto backup_item_list_free;
	}

	if (memcmp((backup_item_area_info.backup_area_state), backup_signature, sizeof(backup_signature)) == 0) {
		if (backup_item_area_info.item_num < 0 || backup_item_area_info.item_num > MAX_ITEM_NUM
				|| backup_item_area_info.item_total_size < 0 || backup_item_area_info.item_total_size > MAX_TOTAL_ITEM_SIZE) {
			printk(KERN_INFO "APPNV_INFO: backup item num error(%d): %s\n", backup_item_area_info.item_num, __func__);
			ret = -1;
			goto backup_item_list_free;
		}

		if (backup_item_area_info.item_num == 0) {
			printk(KERN_INFO "APPNV_INFO: backup item num = 0: %s\n", __func__);
			return 0;
		}

		item_info_buffer = kzalloc(sizeof(struct nv_item_info) * backup_item_area_info.item_num, GFP_KERNEL);
		if (!item_info_buffer) {
			printk(KERN_ERR "APPNV_ERROR: kzalloc(item_info_buffer): %s\n", __func__);
			ret = -EFAULT;
			goto backup_item_list_free;
		}
		
		ret = block_read(NONVOLATILE_BACKUP_PARTITION_NAME,
							sizeof(struct nv_item_area_info),
							(uint8_t *)item_info_buffer,
							sizeof(struct nv_item_info) * backup_item_area_info.item_num);
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
			goto item_info_buffer_free;
		}

		for(i = 0; i < backup_item_area_info.item_num; i++){
			if(item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_id <= 0
				|| item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_offset < 0
				|| item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_offset > MAX_TOTAL_ITEM_SIZE -1
				|| item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_size <= 0
				|| item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_size > MAX_ITEM_SIZE){
				printk(KERN_ERR "APPNV_ERROR: item info buffer param error: %s\n", __func__);
				ret = -1;
				goto item_info_buffer_free;
			}
		}

		item_data_buffer = kzalloc(sizeof(uint8_t) * backup_item_area_info.item_total_size, GFP_KERNEL);
		if (!item_data_buffer) {
			printk(KERN_ERR "APPNV_ERROR: kzalloc(item_data_buffer): %s\n", __func__);
			ret = -EFAULT;
			goto item_info_buffer_free;
		}

		ret = block_read(NONVOLATILE_BACKUP_PARTITION_NAME,
							BACKUP_AREA_DATA_OFFSET,
							(uint8_t *)item_data_buffer,
							sizeof(uint8_t) * backup_item_area_info.item_total_size);
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
			goto free_item_data_buffer;
		}

		id_check_list = kzalloc(sizeof(int) * backup_item_area_info.item_num, GFP_KERNEL);
		if(!id_check_list){
			printk(KERN_ERR "APPNV_ERROR: kzalloc(id_check_list): %s\n", __func__);
			ret = -EFAULT;
			goto free_item_data_buffer;
		}
		id_check_list_num = 0;

		for(i = 0; i < backup_item_area_info.item_num; i++){
			for(j = 0; j < id_check_list_num; j++){
				if (item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_id == id_check_list[j]) {
					break;
				}
			}

			if(j == id_check_list_num){
				if(item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_flag & nv_item_flag_enable){
					valid_backup_item_num++;
				}

				id_check_list[id_check_list_num] = item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_id;
				id_check_list_num++;
			}
		}

		if(valid_backup_item_num == 0){
			(*valid_backup_item_list)->backup_item_num = 0;
			(*valid_backup_item_list)->backup_item = NULL;
			return 0;
		}

		(*valid_backup_item_list)->backup_item_num = valid_backup_item_num;
		(*valid_backup_item_list)->backup_item = kzalloc(sizeof(struct nv_item) * valid_backup_item_num, GFP_KERNEL);

		for (j = 0; j < id_check_list_num; j++) {
			id_check_list[j] = 0;
		}
		id_check_list_num = 0;
		valid_backup_item_num = 0;

		for (i = 0; i < backup_item_area_info.item_num; i++) {
			for (j = 0; j < id_check_list_num; j++) {
				if (item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_id == id_check_list[j]) {
					break;
				}
			}

			if(j == id_check_list_num){
				if(item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_flag & nv_item_flag_enable){
					memcpy(&((*valid_backup_item_list)->backup_item[valid_backup_item_num].item_info), &(item_info_buffer[backup_item_area_info.item_num - (i + 1)]), sizeof(struct nv_item_info));

					(*valid_backup_item_list)->backup_item[valid_backup_item_num].item_data = kzalloc(sizeof(uint8_t) * item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_size, GFP_KERNEL);
					if(!(*valid_backup_item_list)){
						printk(KERN_ERR "APPNV_ERROR: kzalloc(valid_backup_item_list): %s\n", __func__);
						ret = -EFAULT;
						goto free_id_check_list;
					}
					memcpy(((*valid_backup_item_list)->backup_item[valid_backup_item_num].item_data),
							item_data_buffer + item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_offset,
							sizeof(uint8_t) * (*valid_backup_item_list)->backup_item[valid_backup_item_num].item_info.item_size);

					valid_backup_item_num++;

				}
				id_check_list[id_check_list_num] = item_info_buffer[backup_item_area_info.item_num - (i + 1)].item_id;
				id_check_list_num++;
			}
		}
	}

free_id_check_list:
	if(id_check_list){
		kfree(id_check_list);
		id_check_list = NULL;
	}

free_item_data_buffer:
	if (item_data_buffer) {
		kfree(item_data_buffer);
		item_data_buffer = NULL;
	}

item_info_buffer_free:
	if (item_info_buffer) {
		kfree(item_info_buffer);
		item_info_buffer = NULL;
	}

backup_item_list_free:
	if (ret != 0) {
		free_backup_item_list(valid_backup_item_list);
	}

	return ret;
}


int add_backup_item(const struct nv_item_info *item_info, const uint8_t *buf, unsigned long buf_len)
{
	struct nv_backup_area_info	backup_item_area_info;
	struct nv_item_info			backup_item_info;
	unsigned long				current_itm_num			= 0;
	unsigned long				current_item_total_size	= 0;

	int ret = 0;

	if (!item_info) {
		printk(KERN_ERR "APPNV_ERROR: item_info is NULL: %s\n", __func__);
		return -1;
	}

	if (!buf) {
		printk(KERN_ERR "APPNV_ERROR: buf is NULL: %s\n", __func__);
		return -1;
	}

	if (buf_len > MAX_ITEM_SIZE) {
		printk(KERN_ERR "APPNV_ERROR: buf_len error(%ld): %s\n", buf_len, __func__);
		return -1;
	}

	ret = block_read(NONVOLATILE_BACKUP_PARTITION_NAME,
						0,
						(uint8_t *)&backup_item_area_info,
						sizeof(struct nv_backup_area_info));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		return ret;
	}

	if(memcmp((backup_item_area_info.backup_area_state), backup_signature, sizeof(backup_signature)) != 0){
		backup_item_area_info.data_area_offset	= 0;
		backup_item_area_info.item_num			= 0;
		backup_item_area_info.item_total_size	= 0;
		memcpy((backup_item_area_info.backup_area_state), backup_signature, sizeof(backup_signature));
	}

	/* appsst2(backup partition) total number check */
	if (backup_item_area_info.item_num >= BACKUP_AREA_MAX_ITEM_NUM) {
		printk(KERN_ERR "APPNV_ERROR: Backup area item total number over : %s\n", __func__);
		return -1;
	}

	/* appsst2(backup partition) free space check */
	if(backup_item_area_info.item_total_size + buf_len > SIZE_3MB){
		printk(KERN_ERR "APPNV_ERROR: item_total_size over : %s\n", __func__);
		return -1;
	}

	backup_item_info.item_id		= item_info->item_id;
	backup_item_info.item_flag		= item_info->item_flag | nv_item_flag_active;
	backup_item_info.item_offset	= backup_item_area_info.item_total_size;
	backup_item_info.item_size		= item_info->item_size;

	current_itm_num				= backup_item_area_info.item_num;
	current_item_total_size		= backup_item_area_info.item_total_size;

	backup_item_area_info.item_num += 1;
	backup_item_area_info.item_total_size += buf_len;

	ret = block_write_verify(NONVOLATILE_BACKUP_PARTITION_NAME,
								BACKUP_AREA_DATA_OFFSET + current_item_total_size,
								buf,
								buf_len);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
		return ret;
	}

	ret = block_write_verify(NONVOLATILE_BACKUP_PARTITION_NAME,
								sizeof(struct nv_item_area_info) + ((sizeof(struct nv_item_info) * current_itm_num)),
								(uint8_t *)&backup_item_info,
								sizeof(struct nv_item_info));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
		return ret;
	}

	ret = block_write_verify(NONVOLATILE_BACKUP_PARTITION_NAME,
								0,
								(uint8_t *)&backup_item_area_info,
								sizeof(struct nv_backup_area_info));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
		return ret;
	}

	return ret;
}


int remove_last_backup_item(void)
{
	struct nv_backup_area_info	backup_item_area_info;
	struct nv_item_info			backup_item_info;	
	unsigned long delete_item_size		= 0;
	unsigned long delete_item_offset	= 0;
	uint8_t *zero_fill_data_buf			= 0;

	int ret = 0;


	ret = block_read(NONVOLATILE_BACKUP_PARTITION_NAME,
						0,
						(uint8_t *)&backup_item_area_info,
						sizeof(struct nv_backup_area_info));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: remove_last_backup_item() end: backup area info read error.\n");
		return -1;
	}
	
	ret = block_read(NONVOLATILE_BACKUP_PARTITION_NAME,
						sizeof(struct nv_item_area_info) + (sizeof(struct nv_item_info) * (backup_item_area_info.item_num - 1)),
						(uint8_t *)&backup_item_info, 
						sizeof(struct nv_item_info));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: remove_last_backup_item() end: backup item info area read error\n");
		return ret;
	}
	
	if(backup_item_info.item_size > MAX_ITEM_SIZE){
		ret = -1;
		printk(KERN_ERR "APPNV_ERROR: remove_last_backup_item() end: item size error.(item_size = %ld)\n", backup_item_info.item_size);
		return ret;
	}

	if(backup_item_info.item_offset > (SIZE_3MB - 1)){
		ret = -1;
		printk(KERN_ERR "APPNV_ERROR: remove_last_backup_item() end: item offset error.(item_offset = %ld)\n", backup_item_info.item_offset);
		return ret;
	}

	if (backup_item_area_info.item_num == 0) {
		// Nothing to do.
		return 0;
	} 
	
	delete_item_size = backup_item_info.item_size;
	delete_item_offset = backup_item_info.item_offset;
	
	backup_item_area_info.item_num -= 1;
	backup_item_area_info.item_total_size -= backup_item_info.item_size;

	ret = block_write_verify(NONVOLATILE_BACKUP_PARTITION_NAME,
							0,
							(uint8_t *)&backup_item_area_info,
							sizeof(struct nv_backup_area_info));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: remove_last_backup_item() end: update backup item area info error.\n");
		return ret;
	}

	memset(&backup_item_info, 0, sizeof(struct nv_item_info));

	ret = block_write_verify(NONVOLATILE_BACKUP_PARTITION_NAME,
						sizeof(struct nv_item_area_info) + (sizeof(struct nv_item_info) * (backup_item_area_info.item_num - 1)),
						(uint8_t *)&backup_item_info, 
						sizeof(struct nv_item_info) * backup_item_area_info.item_num);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: remove_last_backup_item() end: update backup item info area error.(remove success)\n");
		return 0;
	}
	
	zero_fill_data_buf = kzalloc(sizeof(uint8_t) * delete_item_size, GFP_KERNEL);
	if (!zero_fill_data_buf) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(zero_fill_data_buf): %s\n", __func__);
		return -EFAULT;
	}
	ret = block_write_verify(NONVOLATILE_BACKUP_PARTITION_NAME,
						BACKUP_AREA_DATA_OFFSET + delete_item_offset,
						zero_fill_data_buf, 
						delete_item_size);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: remove_last_backup_item() end: update backup item data error.(remove success)\n");
		ret = 0;
	}

	if(zero_fill_data_buf != NULL){
		kfree(zero_fill_data_buf);
		zero_fill_data_buf = NULL;
	}

	return ret;
}


void free_nv_cache(void)
{
	int i = 0;
	int j = 0;

	memset(&g_nv_emmc_info_area, 0, sizeof(struct nv_info_area));

	for (i = 0; i < nv_kind_count; i++) {
		for (j = 0; j < g_nv_data_table[i].nv_item_num; j++) {
			if (g_nv_data_table[i].nv_item_data != NULL && g_nv_data_table[i].nv_item_data[j].item_data) {
				kfree(g_nv_data_table[i].nv_item_data[j].item_data);
				g_nv_data_table[i].nv_item_data[j].item_data = NULL;
			}
		}

		if (g_nv_data_table[i].nv_item_data) {
			kfree(g_nv_data_table[i].nv_item_data);
			g_nv_data_table[i].nv_item_data	= NULL;
		}
		g_nv_data_table[i].nv_item_total_size	= 0;
		g_nv_data_table[i].nv_data_area_offset	= 0;
		g_nv_data_table[i].nv_item_num	 		= 0;
	}

	return;
}


int create_nv_cache(void)
{
	unsigned int i = 0;
	unsigned int j = 0;

	struct nv_item_info	*item_info_buffer		= NULL;
	uint8_t				*item_data_buffer		= NULL;
	uint8_t				*item_data_buffer_temp	= NULL;

	unsigned int item_offset = 0;

	int ret = 0;


	free_nv_cache();

	ret = block_read(NONVOLATILE_PARTITION_NAME, 0, (uint8_t *)(&g_nv_emmc_info_area), sizeof(struct nv_info_area));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		return ret;
	}

	if (g_nv_emmc_info_area.nv_manage_area.total_item_num > MAX_ITEM_NUM) {
		printk(KERN_ERR "APPNV_ERROR: item num error(%d): %s\n", g_nv_emmc_info_area.nv_manage_area.total_item_num, __func__);
		return -1;
	}

	if (g_nv_emmc_info_area.nv_manage_area.total_item_num == 0) {
		for (i = 0; i < nv_kind_count; i++) {
			g_nv_data_table[i].nv_data_area_offset		= 0;
			g_nv_data_table[i].nv_item_num				= 0;
			g_nv_data_table[i].nv_item_total_size		= 0;
			g_nv_data_table[i].nv_item_data				= NULL;
		}		

		return 0;
	}

	item_info_buffer = kzalloc((sizeof(struct nv_item_info) * g_nv_emmc_info_area.nv_manage_area.total_item_num), GFP_KERNEL);
	if (!item_info_buffer) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(item_info_buffer): %s\n", __func__);
		return -EFAULT;
	}

	ret = block_read(NONVOLATILE_PARTITION_NAME,
						g_nv_emmc_info_area.nv_manage_area.header_size,
						(uint8_t*)(item_info_buffer),
						sizeof(struct nv_item_info) * g_nv_emmc_info_area.nv_manage_area.total_item_num);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	}

	for (i = 0; i < nv_kind_count; i++) {
		if (g_nv_emmc_info_area.item_area_info[i].data_area_offset > MAX_DATA_AREA_OFFSET
			|| g_nv_emmc_info_area.item_area_info[i].item_num > MAX_ITEM_NUM
			|| g_nv_emmc_info_area.item_area_info[i].item_total_size > MAX_TOTAL_ITEM_SIZE) {

			printk(KERN_ERR "APPNV_ERROR: eMMC read data error: %s\n", __func__);
			ret = -1;
			goto error;
		}

		g_nv_data_table[i].nv_data_area_offset	= g_nv_emmc_info_area.item_area_info[i].data_area_offset;
		g_nv_data_table[i].nv_item_num			= g_nv_emmc_info_area.item_area_info[i].item_num;
		g_nv_data_table[i].nv_item_total_size	= g_nv_emmc_info_area.item_area_info[i].item_total_size;
	}

	for (i = 0; i < nv_kind_count; i++) {
		if (g_nv_data_table[i].nv_item_num == 0
			|| g_nv_data_table[i].nv_item_total_size == 0) {
			continue;
		}

		item_data_buffer = kzalloc(g_nv_data_table[i].nv_item_total_size, GFP_KERNEL);
		if (!item_data_buffer) {
			printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
			ret = -EFAULT;
			goto error;
		}

		ret = block_read(NONVOLATILE_PARTITION_NAME,
							g_nv_data_table[i].nv_data_area_offset,
							(uint8_t *)item_data_buffer,
							g_nv_data_table[i].nv_item_total_size);
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
			goto error;
		}

		item_data_buffer_temp = item_data_buffer;

		g_nv_data_table[i].nv_item_data = kzalloc(sizeof(struct nv_item) * g_nv_data_table[i].nv_item_num, GFP_KERNEL);
		if (g_nv_data_table[i].nv_item_data == NULL) {
			printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
			ret = -EFAULT;
			goto error;
		}

		for (j = 0; j < g_nv_data_table[i].nv_item_num; j++) {
			unsigned int buffer_size = auto_resize(item_info_buffer[j + item_offset].item_flag, item_info_buffer[j + item_offset].item_size);

			if (buffer_size == 0 || buffer_size > MAX_ITEM_SIZE) {
				printk(KERN_ERR "APPNV_ERROR: invalid item size.: %s\n", __func__);
				ret = -nv_invalid_size;
				goto error;
			}

			g_nv_data_table[i].nv_item_data[j].item_data = kzalloc(buffer_size, GFP_KERNEL);
			if (g_nv_data_table[i].nv_item_data[j].item_data == NULL) {
				printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
				ret = -EFAULT;
				goto error;
			}

			memcpy(&(g_nv_data_table[i].nv_item_data[j].item_info), &(item_info_buffer[j + item_offset]), sizeof(struct nv_item_info));
			memcpy((g_nv_data_table[i].nv_item_data[j].item_data), item_data_buffer_temp, buffer_size);

			item_data_buffer_temp += buffer_size;
		}

		item_offset += g_nv_data_table[i].nv_item_num;

		if (item_data_buffer) {
			kfree(item_data_buffer);
			item_data_buffer = NULL;
		}
	}

	if (item_info_buffer != NULL) {
		kfree(item_info_buffer);
		item_info_buffer = NULL;
	}	

	return ret;

error:
	free_nv_cache();

	if (item_data_buffer != NULL) {
		kfree(item_data_buffer);
		item_data_buffer = NULL;
	}	

	if (item_info_buffer != NULL) {
		kfree(item_info_buffer);
		item_info_buffer = NULL;
	}

	return ret;
}


int set_boot_mode_item(unsigned long item_id, uint8_t *buffer, unsigned long buffer_size)
{
	unsigned long item_offset	= 0;
	unsigned long item_size		= 0;
	unsigned long set_size		= 0;

	int ret = 0;


	if(!buffer){
		printk(KERN_ERR "APPNV_ERROR: buffer is NULL: %s\n", __func__);
		return -1;
	}
	
	if(buffer_size > SIZE_1MB){
		printk(KERN_ERR "APPNV_ERROR: buffer size over 1048576(1MB).(buffer_size = %ld): %s\n", buffer_size, __func__);
		return -1;
	}	
	
	switch (item_id) {
	case MC_CMD_BOOT_MODE_ID:
		item_offset = KOUTEI_BOOT_MODE_OFFSET;
		item_size   = KOUTEI_BOOT_MODE_SIZE;
		break;
	case MC_CMD_KOUTEI_MODE_ID:
		item_offset = KOUTEI_MODE_OFFSET;
		item_size   = KOUTEI_MODE_SIZE;
		break;
	case MC_CMD_MASETER_CLEAR_ID:
		item_offset = KOUTEI_MASETER_CLEAR_OFFSET;
		item_size   = KOUTEI_MASETER_CLEAR_SIZE;
		break;
	case KERNEL_POWER_ON_REASON_ID:
		item_offset = KERNEL_POWER_ON_REASON_OFFSET;
		item_size   = KERNEL_POWER_ON_REASON_SIZE;
		break;
	case MC_CMD_USBDL_BOOT_ID:
		item_offset = KOUTEI_USBDL_BOOT_OFFSET;
		item_size   = KOUTEI_USBDL_BOOT_SIZE;
		break;
	case APNV_CAMERA_SDDL_ID:
		item_offset = APNV_CAMERA_SDDL_OFFSET;
		item_size   = APNV_CAMERA_SDDL_SIZE;
		break;
	case MC_CMD_FIRST_START_FLAG_ID:
		item_offset = KOUTEI_FIRST_START_FLAG_OFFSET;
		item_size   = KOUTEI_FIRST_START_FLAG_SIZE;
		break;
	case MC_CMD_FUSE_BLOW_1_ID:
		item_offset = KOUTEI_FUSE_BLOW_1_OFFSET;
		item_size   = KOUTEI_FUSE_BLOW_1_SIZE;
		break;
	case MC_CMD_FUSE_BLOW_2_ID:
		item_offset = KOUTEI_FUSE_BLOW_2_OFFSET;
		item_size   = KOUTEI_FUSE_BLOW_2_SIZE;
		break;
	case MC_CMD_FUSE_BLOW_3_ID:
		item_offset = KOUTEI_FUSE_BLOW_3_OFFSET;
		item_size   = KOUTEI_FUSE_BLOW_3_SIZE;
		break;
	case MC_CMD_FUSE_BLOW_4_ID:
		item_offset = KOUTEI_FUSE_BLOW_4_OFFSET;
		item_size   = KOUTEI_FUSE_BLOW_4_SIZE;
		break;
	default:
		return 0;
	}

	if (buffer_size > item_size) {
		set_size = item_size;
	} else {
		set_size = buffer_size;
	}

	ret = block_write_verify(NONVOLATILE_BOOT_MODE_PARTITION_NAME, item_offset, buffer, set_size);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: boot mode item write error.(item id = %ld): %s\n", item_id, __func__);
		return -1;
	}

	return 0;
}


int activate_nv_item(void)
{
	int i = 0;
	int j = 0;
	int k = 0;

	struct nv_info_area 	*master_data					= NULL;
	struct nv_item_info 	*master_data_info_buffer		= NULL;

	int ret = 0;


	ret = create_nv_cache();
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: create_nv_cache(): %s\n", __func__);
		return ret;
	}

	master_data_info_buffer = kzalloc((sizeof(struct nv_item_info) * g_nv_emmc_info_area.nv_manage_area.total_item_num), GFP_KERNEL);
	if (!master_data_info_buffer) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
		return -EFAULT;
	}
	ret = block_read(NONVOLATILE_MASTER_PARTITION_NAME,
				sizeof(struct nv_info_area),
				(uint8_t *)master_data_info_buffer,
				sizeof(struct nv_item_info) * g_nv_master_info_area.nv_manage_area.total_item_num);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto master_data_info_buffer_free;
	}

	for (i = 0; i < nv_kind_count; i++) {
		for (j = 0; j < g_nv_data_table[i].nv_item_num; j++) {
			if (!(g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_active)) {
				for (k = 0; k < g_nv_master_info_area.nv_manage_area.total_item_num; k++) {
					if (g_nv_data_table[i].nv_item_data[j].item_info.item_id == master_data_info_buffer[k].item_id) {
						break;
					}
				}

				if (k == g_nv_master_info_area.nv_manage_area.total_item_num) {
					printk(KERN_ERR "APPNV: item(id = %ld) not found.: %s\n",
							g_nv_data_table[i].nv_item_data[j].item_info.item_id, __func__);
					continue;
				}

				if (master_data_info_buffer[k].item_flag & nv_item_flag_active) {
					master_data = kzalloc(master_data_info_buffer[k].item_size, GFP_KERNEL);
					if (!master_data) {
						printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
						ret = -EFAULT;
						goto master_data_info_buffer_free;						
					}

					ret = block_read(NONVOLATILE_MASTER_PARTITION_NAME,
								g_nv_master_info_area.item_area_info[i].data_area_offset + master_data_info_buffer[k].item_offset,
								(uint8_t *)master_data,
								master_data_info_buffer[k].item_size);
					if (ret != 0) {
						printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
						goto master_data_free;
					}

					ret = set_nonvolatile_no_lock((uint8_t* )master_data, master_data_info_buffer[k].item_id, master_data_info_buffer[k].item_size);
					if (ret <= 0) {
						// nothing to do.
					}

					ret = set_boot_mode_item(g_nv_data_table[i].nv_item_data[j].item_info.item_id,
												(uint8_t* )master_data,
												master_data_info_buffer[k].item_size);
					if (ret != 0) {
						printk(KERN_ERR "APPNV_ERROR: boot mode item.(id = %ld): %s\n",
									g_nv_data_table[i].nv_item_data[j].item_info.item_id, __func__);
						goto master_data_free;
					}

					if (master_data) {
						kfree(master_data);
						master_data = NULL;
					}
				}
			}
		}
	}

	if (master_data_info_buffer) {
		kfree(master_data_info_buffer);
		master_data_info_buffer = NULL;
	}

	return 0;

master_data_free:
	if (master_data) {
		kfree(master_data);
		master_data = NULL;
	}

master_data_info_buffer_free:
	if (master_data_info_buffer) {
		kfree(master_data_info_buffer);
		master_data_info_buffer = NULL;
	}

	return ret;
}


static int carry_over_old_nv_item(
	const struct nv_item_area_info	*dst_area_info,
	struct		 nv_item_info		*dst_item_index,
	uint8_t							*dst_item_data_header,
	
	const struct nv_item_area_info	*src_area_info,
	const struct nv_item_info		*src_item_index,
	const uint8_t					*src_item_data_header,
	
	enum nv_data_kind kind)
{
	int err = 0;
	int src_idx = 0;
	int dst_idx = 0;

	const uint8_t *source = NULL;
	uint8_t *target = NULL;


	if (dst_area_info && 
		dst_item_index && 
		dst_item_data_header &&
		src_area_info &&
		src_item_index &&
		src_item_data_header &&
		(kind == nv_kind_backup || kind == nv_kind_secure_backup)
	) {
		n2printk(KERN_INFO "APPNV: ---> %s", __func__);
	} else {
		printk(KERN_ERR "APPNV: %s skipped (src = 0 byte)", __func__);
		return err;
	}

	for (dst_idx = 0; dst_idx < dst_area_info->item_num; ++dst_idx) {
		src_idx = 0;
		for (src_idx = 0; src_idx < src_area_info->item_num; ++src_idx) {
			n2printk(KERN_INFO "APPNV: [%d] (%d, %d)", kind, dst_idx, src_idx);

			if (dst_item_index[dst_idx].item_id == src_item_index[src_idx].item_id) {
				n2printk(KERN_INFO "APPNV: [MATCH] itemid=(%04x)",
					(unsigned int)dst_item_index[dst_idx].item_id);

				if ((src_item_index[src_idx].item_flag & nv_item_flag_enable) &&
					(src_item_index[src_idx].item_flag & nv_item_flag_active)) {
					source = src_item_data_header + src_item_index[src_idx].item_offset;
					target = dst_item_data_header + dst_item_index[dst_idx].item_offset;

					n2printk(KERN_INFO "APPNV: src=[ACTIVE]");
					
					if ( !(dst_item_index[dst_idx].item_flag & nv_item_flag_secure) &&
						dst_item_index[dst_idx].item_size >= src_item_index[src_idx].item_size) {
						n2printk(KERN_INFO "APPNV: [raw data] %p <-- %p (%04x >= %04x)", target, source,
							(unsigned int)dst_item_index[dst_idx].item_size,
							(unsigned int)src_item_index[src_idx].item_size);
						
						memcpy(target, source, src_item_index[src_idx].item_size);
						
						dst_item_index[dst_idx].item_flag |= nv_item_flag_active;
					} else if ( (dst_item_index[dst_idx].item_flag & nv_item_flag_secure) && 
						encrypted_size(dst_item_index[dst_idx].item_size) == encrypted_size(src_item_index[src_idx].item_size)) {
						n2printk(KERN_INFO "APPNV: [encrypted] %p <-- %p (%04x)", target, source, encrypted_size(src_item_index[src_idx].item_size));
						
						memcpy(target, source, encrypted_size(src_item_index[src_idx].item_size));

						dst_item_index[dst_idx].item_flag |= nv_item_flag_active;
					} else {
						n2printk(KERN_INFO "APPNV: [otherwise]");
					}
				} else {
					n2printk(KERN_INFO "APPNV: src=[NOT ACTIVE]");
				}
				break;
			} else {
				n2printk(KERN_INFO "APPNV: [UNMATCH] itemid=(%04x, %04x)",
					(unsigned int)dst_item_index[dst_idx].item_id,
					(unsigned int)src_item_index[src_idx].item_id);
			}
		}
	}

	return err;
}


int get_nv_item_data(unsigned int id, unsigned int *flag, unsigned long *size, uint8_t **data)
{
	int i;
	int j;


	if(!size){
		printk(KERN_ERR "APPNV_ERROR: size is NULL: %s\n", __func__);
		return -1;
	}

	if(!(data)){
		printk(KERN_ERR "APPNV_ERROR: data is NULL: %s\n", __func__);
		return -1;
	}

	for (i = 0; i < nv_kind_count; i++) {
		if (i == nv_kind_normal || i == nv_kind_secure_normal) {
			continue;
		}
		for (j = 0; j < g_nv_data_table[i].nv_item_num; j++) {
			if (g_nv_data_table[i].nv_item_data[j].item_info.item_id == id) {
					*flag = g_nv_data_table[i].nv_item_data[j].item_info.item_flag;
					*size = g_nv_data_table[i].nv_item_data[j].item_info.item_size;
					*data = g_nv_data_table[i].nv_item_data[j].item_data;
					return 0;
			}
		}
	}
	printk(KERN_ERR "APPNV_ERROR: target id not found: %s\n", __func__);

	return -1;
}


int reconstruct_nv_area(void)
{
	int i = 0;

	struct nv_item_info			*item_info_buffer			= NULL;
	uint8_t						*item_data_buffer			= NULL;

	struct nv_item_info			*master_item_index[nv_kind_count];
	struct nv_item_info			*cur_item_index[nv_kind_count];
	uint8_t						*cur_item_data[nv_kind_count];
	
	struct nv_item_info			*cur_item_info_buffer		= NULL;
	uint8_t						*cur_item_data_buffer		= NULL;

	struct nv_item_info			*master_index_iter			= NULL;
	struct nv_item_info			*cur_index_iter				= NULL;
	uint8_t						*cur_data_iter				= NULL;
	
	int							cur_item_data_size	= 0;
	int							kind				= 0;

	unsigned long				backup_item_total_size	= 0;
	unsigned long				data_offset				= 0;
	struct nv_item_area_info	backup_area_info;
	struct nv_item_area_info	carry_over_item_area_info;

	bool						restore_backup_item_flag	= false;
	struct backup_item_list		*backup_item_list			= NULL;
	struct nv_info_area			info_area_reconstruct;
	struct nv_item_info			*carry_over_item_index		= NULL;
	uint8_t						*carry_over_item_data		= NULL;

	int ret = 0;


	backup_area_info.data_area_offset	= 0;
	backup_area_info.item_num			= 0;
	backup_area_info.item_total_size	= 0;
	backup_area_info.reserved			= 0;

	ret = block_read(NONVOLATILE_MASTER_PARTITION_NAME,
						0,
						(uint8_t*)(&g_nv_master_info_area),
						sizeof(struct nv_info_area));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		return ret;
	}

	if (g_nv_master_info_area.nv_manage_area.total_item_num == 0
			|| g_nv_master_info_area.nv_manage_area.total_item_num > MAX_ITEM_NUM) {
		printk(KERN_ERR "APPNV_ERROR: item num error(%d): %s\n", g_nv_master_info_area.nv_manage_area.total_item_num, __func__);
		return -1;
	}

	if (g_nv_master_info_area.nv_manage_area.total_item_num == 0) {
		printk(KERN_INFO "APPNV_INFO: APPNV item none.: %s\n", __func__);
		return 0;
	}

	item_info_buffer = kzalloc((sizeof(struct nv_item_info) * g_nv_master_info_area.nv_manage_area.total_item_num), GFP_KERNEL);
	if(!item_info_buffer){
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
		return -EFAULT;
	}
	ret = block_read(NONVOLATILE_MASTER_PARTITION_NAME,
				sizeof(struct nv_info_area),
				(uint8_t *)item_info_buffer,
				sizeof(struct nv_item_info) * g_nv_master_info_area.nv_manage_area.total_item_num);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	}

	for (i = 0; i < nv_kind_count; i++) {
		if(g_nv_master_info_area.item_area_info[i].data_area_offset > SIZE_5MB - sizeof(struct nv_info_area) - sizeof(struct nv_item_info)
			|| g_nv_master_info_area.item_area_info[i].item_num > MAX_ITEM_NUM
			|| g_nv_master_info_area.item_area_info[i].item_total_size > MAX_TOTAL_ITEM_SIZE){
			printk(KERN_ERR "APPNV_ERROR: master info area check error: %s\n", __func__);
			ret = -1;
			goto error;
		}

		g_nv_data_table[i].nv_data_area_offset	= g_nv_master_info_area.item_area_info[i].data_area_offset;
		g_nv_data_table[i].nv_item_num			= g_nv_master_info_area.item_area_info[i].item_num;
		g_nv_data_table[i].nv_item_total_size	= g_nv_master_info_area.item_area_info[i].item_total_size;
	}

	ret = block_read(NONVOLATILE_PARTITION_NAME,
						0,
						(uint8_t*)(&g_nv_emmc_info_area),
						sizeof(struct nv_info_area));
	if (ret != 0) {
		printk(KERN_ERR "APPNV: block_read[current INFO HEADER] err=%d: %s\n", ret, __func__);
		goto error;
	}

	if ((g_nv_emmc_info_area.nv_manage_area.nv_state[0] == 'N') &&
		(g_nv_emmc_info_area.nv_manage_area.nv_state[1] == 'V') &&
		(g_nv_emmc_info_area.nv_manage_area.total_item_num <= MAX_ITEM_NUM)) {

		master_index_iter	= NULL;
		cur_index_iter		= NULL;
		cur_data_iter		= NULL;
		
		cur_item_data_size = 0;
		kind = 0;

		cur_item_info_buffer = kzalloc((sizeof(struct nv_item_info) * g_nv_emmc_info_area.nv_manage_area.total_item_num), GFP_KERNEL);
		if(!cur_item_info_buffer){
			printk(KERN_ERR "APPNV: cur_item_info_buffer = kzalloc error: %s\n", __func__);
			ret = -EFAULT;
			goto error;
		}
		ret = block_read(NONVOLATILE_PARTITION_NAME,
					sizeof(struct nv_info_area),
					(uint8_t *)cur_item_info_buffer,
					sizeof(struct nv_item_info) * g_nv_emmc_info_area.nv_manage_area.total_item_num);
		if (ret != 0) {
			printk(KERN_ERR "APPNV: block_read[current ITEM INFO[%d]] err=%d: %s\n", g_nv_emmc_info_area.nv_manage_area.total_item_num, ret, __func__);
			goto error;
		}

		for (kind = 0; kind < nv_kind_count; ++kind) {
			cur_item_data_size += g_nv_emmc_info_area.item_area_info[kind].item_total_size;
		}

		if (cur_item_data_size <= 0 || cur_item_data_size > MAX_TOTAL_ITEM_SIZE){
				printk(KERN_ERR "APPNV_ERROR: total data size = zero: %s\n", __func__);
				ret = -1;
				goto error;
		}

		cur_item_data_buffer = kzalloc(cur_item_data_size, GFP_KERNEL);
		if (!cur_item_data_buffer) {
			printk(KERN_ERR "APPNV: cur_item_data_buffer = kzalloc: %s\n", __func__);
			ret = -EFAULT;
			goto error;
		}
		ret = block_read(NONVOLATILE_PARTITION_NAME,
							g_nv_emmc_info_area.item_area_info[nv_kind_normal].data_area_offset,
							(uint8_t *)cur_item_data_buffer,
							cur_item_data_size);
		if (ret != 0) {
			printk(KERN_ERR "APPNV: block_read[current DATA[%d]] err=%d: %s\n", cur_item_data_size, ret, __func__);
			goto error;
		}
		
		master_index_iter	= item_info_buffer;
		cur_index_iter		= cur_item_info_buffer;
		cur_data_iter		= cur_item_data_buffer;
		
		for (kind = 0; kind < nv_kind_count; ++kind) {
			master_item_index[kind] = master_index_iter;
			master_index_iter += g_nv_master_info_area.item_area_info[kind].item_num;
			
			cur_item_index[kind] = cur_index_iter;
			cur_index_iter += g_nv_emmc_info_area.item_area_info[kind].item_num;
			
			cur_item_data[kind] = cur_data_iter;
			cur_data_iter += g_nv_emmc_info_area.item_area_info[kind].item_total_size;
		}
	} else {
		restore_backup_item_flag = true;

		master_index_iter = item_info_buffer;
		for (kind = 0; kind < nv_kind_count; ++kind) {
			master_item_index[kind] = master_index_iter;
			master_index_iter += g_nv_master_info_area.item_area_info[kind].item_num;
		}

		ret = get_backup_item_list(&backup_item_list);
		if(ret != 0){
			// nothing to do.
		} else if (backup_item_list->backup_item_num != 0) {
			cur_item_info_buffer = kzalloc((sizeof(struct nv_item_info) * backup_item_list->backup_item_num), GFP_KERNEL);
			if(!cur_item_info_buffer){
				printk(KERN_ERR "APPNV: cur_item_info_buffer = kzalloc error: %s\n", __func__);
				ret = -EFAULT;
				goto error;
			}

			for (i = 0; i < backup_item_list->backup_item_num; i++) {
				backup_item_total_size += backup_item_list->backup_item[i].item_info.item_size;
			}

			cur_item_data_buffer = kzalloc(backup_item_total_size, GFP_KERNEL);
			if (!cur_item_data_buffer) {
				printk(KERN_ERR "APPNV: cur_item_data_buffer = kzalloc: %s\n", __func__);
				ret = -EFAULT;
				goto error;
			}

			data_offset = 0;

			for (i = 0; i < backup_item_list->backup_item_num; i++) {
				memcpy(&(cur_item_info_buffer[i]), &(backup_item_list->backup_item[i].item_info), sizeof(struct nv_item_info));
				memcpy(cur_item_data_buffer + data_offset,  backup_item_list->backup_item[i].item_data, backup_item_list->backup_item[i].item_info.item_size);
				cur_item_info_buffer[i].item_offset = data_offset;
				data_offset += backup_item_list->backup_item[i].item_info.item_size;
			}

			backup_area_info.data_area_offset	= 0;
			backup_area_info.item_num			= backup_item_list->backup_item_num;
			backup_area_info.item_total_size	= backup_item_total_size;
			backup_area_info.reserved			= 0;
		}
		free_backup_item_list(&backup_item_list);
	}

	ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
						0,
						(uint8_t *)reconstruct_signature,
						sizeof(reconstruct_signature));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
		goto error;
	}

	for (i = 0; i < nv_kind_count; i++) {
		if (g_nv_data_table[i].nv_item_total_size > MAX_TOTAL_ITEM_SIZE) {
			printk(KERN_ERR "APPNV_ERROR: item total size error(%ld): %s\n", g_nv_data_table[i].nv_item_total_size, __func__);
			ret = -1;
			goto error;
		}

		if (g_nv_data_table[i].nv_item_total_size == 0) {
			continue;
		}

		item_data_buffer = kzalloc(g_nv_data_table[i].nv_item_total_size, GFP_KERNEL);
		if (item_data_buffer == NULL) {
			printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
			ret = -EFAULT;
			goto error;
		}

		ret = block_read(NONVOLATILE_MASTER_PARTITION_NAME,
							g_nv_data_table[i].nv_data_area_offset,
							(uint8_t *)item_data_buffer,
							g_nv_data_table[i].nv_item_total_size);
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
			goto error;
		}

		switch (i) {
		case nv_kind_backup:	/* FALL THRU */
		case nv_kind_secure_backup:
			n2printk(KERN_INFO "APPNV: ---> switch (%d)", i);
			if (restore_backup_item_flag) {
				// backup & master
				memcpy(&carry_over_item_area_info, &backup_area_info, sizeof(struct nv_item_area_info));
				if (backup_area_info.item_num != 0) {
					carry_over_item_index	= cur_item_info_buffer;
					carry_over_item_data	= cur_item_data_buffer;
				}
			} else {
				// master & nv
				memcpy(&carry_over_item_area_info, &(g_nv_emmc_info_area.item_area_info[i]), sizeof(struct nv_item_area_info));
				carry_over_item_index	= cur_item_index[i];
				carry_over_item_data	= cur_item_data[i];
			}

			ret = carry_over_old_nv_item(
				
				&g_nv_master_info_area.item_area_info[i],
				master_item_index[i],
				item_data_buffer,
				
				&carry_over_item_area_info,
				carry_over_item_index,
				carry_over_item_data,

				i
			);
			n2printk(KERN_INFO "APPNV: <--- switch (%d) err=%d", i, ret);
			break;
		default:
			// nothing to do.
			break;
		}

		ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
							g_nv_data_table[i].nv_data_area_offset,
							(uint8_t *)item_data_buffer,
							g_nv_data_table[i].nv_item_total_size);
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
			goto error;
		}

		if (item_data_buffer) {
			kfree(item_data_buffer);
			item_data_buffer = NULL;
		}
	}

	ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
				sizeof(struct nv_info_area),
				(uint8_t *)item_info_buffer,
				sizeof(struct nv_item_info) * g_nv_master_info_area.nv_manage_area.total_item_num);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
		goto error;
	}
	
	if (item_info_buffer) {
		kfree(item_info_buffer);
		item_info_buffer = NULL;
	}

	if (cur_item_info_buffer) {
		kfree(cur_item_info_buffer);
		cur_item_info_buffer = NULL;
	}

	if (cur_item_data_buffer) {
		kfree(cur_item_data_buffer);
		cur_item_data_buffer = NULL;
	}

	memcpy(&info_area_reconstruct, &g_nv_master_info_area, sizeof(struct nv_info_area));
	memcpy(&info_area_reconstruct, reconstruct_signature, sizeof(reconstruct_signature));
	
	ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
							0,
							(uint8_t*)(&info_area_reconstruct),
							sizeof(struct nv_info_area));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
		goto error;
	}

	ret = create_nv_cache();
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: create_nv_cache(): %s\n", __func__);
		goto error;
	}

error:
	if (cur_item_data_buffer) {
		kfree(cur_item_data_buffer);
		cur_item_data_buffer = NULL;
	}
	
	if (cur_item_info_buffer) {
		kfree(cur_item_info_buffer);
		cur_item_info_buffer = NULL;
	}

	if (item_info_buffer) {
		kfree(item_info_buffer);
		item_info_buffer = NULL;
	}

	if (item_data_buffer) {
		kfree(item_data_buffer);
		item_data_buffer = NULL;
	}

	free_backup_item_list(&backup_item_list);

	return ret;
}


void free_early_init_item(void)
{
	unsigned int i = 0;


	if (!nonvolatile_device->early_initialized) {
		// nothing to do.
		return;
	}

	if(!g_early_init_item_data){
		// nothing to do.
		return;
	}
	
	for (i = 0; i < g_early_init_item_num; i++) {
		if (g_early_init_item_data[i].item_data) {
			kfree(g_early_init_item_data[i].item_data);
			g_early_init_item_data[i].item_data = NULL;
		}
	}

	if (g_early_init_item_data) {
		kfree(g_early_init_item_data);
		g_early_init_item_data = NULL;
	}

	g_early_init_item_num = 0;
	nonvolatile_device->early_initialized = false;
}


static int init_nv_item(void)
{
	unsigned int i = 0;
	uint8_t		*workBuff	= NULL;

	struct backup_item_list *backup_item_list;
	unsigned int	flag;
	unsigned long	size;
	uint8_t			*data;	
	bool reconstruct_check_flag = false;

	int ret = 0;

	printk(KERN_INFO "APPNV: init_nv_item() start\n");

	ret = block_read(NONVOLATILE_MASTER_PARTITION_NAME, 0, (uint8_t*)(&g_nv_master_info_area), sizeof(struct nv_info_area));
	if (ret != 0) {
		printk(KERN_INFO "APPNV_INFO: eMMC device not ready.\n");
		return ret;
	}

	if (g_nv_master_info_area.nv_manage_area.nv_state[0] != 'N'
			|| g_nv_master_info_area.nv_manage_area.nv_state[1] != 'V') {
		printk(KERN_ERR "APPNV_ERROR: nv master data error: %s\n", __func__);
		return -1;
	}

	ret = block_read(NONVOLATILE_PARTITION_NAME, 0, (uint8_t *)(&g_nv_emmc_info_area), sizeof(struct nv_info_area));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		return ret;
	}

	// NV data state check.
	if (g_nv_emmc_info_area.nv_manage_area.nv_state[0] != 'N'
			|| g_nv_emmc_info_area.nv_manage_area.nv_state[1] != 'V') {
		// NV binary not found(recovery from "master data" & "backup data".)
		printk(KERN_INFO "APPNV_INFO: APPNV data not found: %s\n", __func__);
	} else {
		// NV binary version check.
		for (i = 0; i < 12; i++) {
			if (g_nv_master_info_area.nv_version_info[i] != g_nv_emmc_info_area.nv_version_info[i]) {
				// binary version up/down
				reconstruct_check_flag = true;
				printk(KERN_INFO "APPNV: init_nv_item() ver update\n");
				break;
			}
		}
	}

	if (i == 12) {
		// NV version check OK.
		ret = create_nv_cache();
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: initialize nv item error: %s\n", __func__);
			return ret;
		}
	} else {
		for (i = 0; i < MAX_RECONSTRUCT_COUNT; i++) {
			ret = reconstruct_nv_area();
			if(ret == 0){
				break;
			}
		}

		if(i == MAX_RECONSTRUCT_COUNT){
			// reconstruct error.
			printk(KERN_ERR "APPNV_ERROR: APPNV initialize error(retyr count = %d): %s\n", i, __func__);
			return -1;
		}

		if (reconstruct_check_flag) {
			ret = get_backup_item_list(&backup_item_list);
			if (ret == 0 && backup_item_list->backup_item_num != 0) {
				for (i = 0; i < backup_item_list->backup_item_num; i++) {
					ret = get_nv_item_data(backup_item_list->backup_item[i].item_info.item_id, &flag, &size, &data);
					if (ret != 0) {
						printk(KERN_INFO "APPNV_INFO: item not found(id = %ld): %s\n", backup_item_list->backup_item[i].item_info.item_id, __func__);
						continue;
					}

					if (size == backup_item_list->backup_item[i].item_info.item_size) {
						if (memcmp(data, backup_item_list->backup_item[i].item_data, size) == 0) {
							continue;
						} else {
							printk(KERN_ERR "APPNV_INFO: reconstruct data mismatch(id = %ld): %s\n", backup_item_list->backup_item[i].item_info.item_id, __func__);
								ret = set_nonvolatile_no_lock(backup_item_list->backup_item[i].item_data, backup_item_list->backup_item[i].item_info.item_id, size);
							if (ret <= 0) {
								printk(KERN_ERR "APPNV_ERROR: data recovery error(id = %ld): %s\n", backup_item_list->backup_item[i].item_info.item_id, __func__);
							} else {
								printk(KERN_INFO "APPNV_INFO: data recovery from backup data(id = %ld): %s\n", backup_item_list->backup_item[i].item_info.item_id, __func__);
							}
							continue;
						}
					} else {
						continue;
					}
				}
			}
			free_backup_item_list(&backup_item_list);
		}
	}

	// sync aboot item.
	workBuff = kzalloc(sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE, GFP_KERNEL);
	if (!workBuff) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
		return -EFAULT;			
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_BOOT_MODE_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_BOOT_MODE_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_BOOT_MODE_ID, sizeof(uint8_t) * KOUTEI_BOOT_MODE_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: BOOT MODE sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_MODE_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_MODE_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_KOUTEI_MODE_ID, sizeof(uint8_t) * KOUTEI_MODE_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: MC MODE sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_MASETER_CLEAR_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_MASETER_CLEAR_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_MASETER_CLEAR_ID, sizeof(uint8_t) * KOUTEI_MASETER_CLEAR_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: MASTER CLEAR FLAG sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KERNEL_POWER_ON_REASON_OFFSET,
						workBuff,
						sizeof(uint8_t) * KERNEL_POWER_ON_REASON_SIZE);

	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		printk(KERN_INFO "[BOOT]boot_reason=0x%08x\n", *((int*)workBuff));
		ret = set_nonvolatile_no_lock(workBuff, KERNEL_POWER_ON_REASON_ID, sizeof(uint8_t) * KERNEL_POWER_ON_REASON_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: POWER ON REASON sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_USBDL_BOOT_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_USBDL_BOOT_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_USBDL_BOOT_ID, sizeof(uint8_t) * KOUTEI_USBDL_BOOT_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: USBDL BOOT sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						APNV_CAMERA_SDDL_OFFSET,
						workBuff,
						sizeof(uint8_t) * APNV_CAMERA_SDDL_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, APNV_CAMERA_SDDL_ID, sizeof(uint8_t) * APNV_CAMERA_SDDL_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: CAMERA SDDL sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_FIRST_START_FLAG_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_FIRST_START_FLAG_OFFSET);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_FIRST_START_FLAG_ID, sizeof(uint8_t) * KOUTEI_FIRST_START_FLAG_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: FIRST START FLAG sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_FUSE_BLOW_1_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_FUSE_BLOW_1_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_FUSE_BLOW_1_ID, sizeof(uint8_t) * KOUTEI_FUSE_BLOW_1_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: FUSE BLOW 1 sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_FUSE_BLOW_2_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_FUSE_BLOW_2_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_FUSE_BLOW_2_ID, sizeof(uint8_t) * KOUTEI_FUSE_BLOW_2_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: FUSE BLOW 2 sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_FUSE_BLOW_3_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_FUSE_BLOW_3_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_FUSE_BLOW_3_ID, sizeof(uint8_t) * KOUTEI_FUSE_BLOW_3_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: FUSE BLOW 3 sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	memset(workBuff, 0, sizeof(uint8_t) * KOUTEI_ITEM_MAX_SIZE);
	ret = block_read(NONVOLATILE_BOOT_MODE_PARTITION_NAME,
						KOUTEI_FUSE_BLOW_4_OFFSET,
						workBuff,
						sizeof(uint8_t) * KOUTEI_FUSE_BLOW_4_SIZE);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	} else {
		ret = set_nonvolatile_no_lock(workBuff, MC_CMD_FUSE_BLOW_4_ID, sizeof(uint8_t) * KOUTEI_FUSE_BLOW_4_SIZE);
		if (ret <= 0) {
			printk(KERN_ERR "APPNV_ERROR: FUSE BLOW 4 sync error.: %s\n", __func__);
		}
		ret = 0;
	}

	ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
						0,
						(uint8_t *)nv_signature,
						sizeof(nv_signature));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
		goto error;
	}

	nonvolatile_device->initialized = true;
	free_early_init_item();

error:
	if (workBuff) {
		kfree(workBuff);
		workBuff = NULL;
	}

	printk(KERN_INFO "APPNV: init_nv_item() end ret = %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(init_nv_item);


int get_early_init_item(uint8_t* buffer, unsigned long id, unsigned long buffer_size)
{
	int i = 0;


	if(!buffer){
		printk(KERN_ERR "APPNV_ERROR: buffer is NULL: %s\n", __func__);
		return -nv_invalid_buffer;
	}
	
	if(buffer_size == 0){
		printk(KERN_ERR "APPNV_ERROR: input param(buffer_size) error: %s\n", __func__);
		return -nv_invalid_size;
	}

	if (!nonvolatile_device->early_initialized) {
		printk(KERN_ERR "APPNV_ERROR: appnv not initialized.: %s\n", __func__);
		return -nv_not_initialized;
	}

	for (i = 0; i < g_early_init_item_num; i++) {
		if (g_early_init_item_data[i].item_info.item_id == id){
			if (!(g_early_init_item_data[i].item_info.item_flag & nv_item_flag_active)) {
				return -nv_not_active;
			} else {
				if (g_early_init_item_data[i].item_info.item_size < buffer_size) {
					buffer_size = g_early_init_item_data[i].item_info.item_size;
				}
				memcpy(buffer, g_early_init_item_data[i].item_data, buffer_size);
				// success.
				return buffer_size;
			}
		}
	}

	printk(KERN_ERR "APPNV_ERROR: appnv not initialized.: %s\n", __func__);
	return -nv_not_initialized;
}


int get_nonvolatile(uint8_t* buffer, unsigned long id, unsigned long buffer_size)
{
	unsigned long item_size = 0;
	int ret = 0;

	if (!buffer) {
		printk(KERN_ERR "APPNV_ERROR: buffer is NULL: %s\n", __func__);
		return -nv_invalid_buffer;
	} else if (buffer_size == 0 || buffer_size > MAX_ITEM_SIZE) {
		printk(KERN_ERR "APPNV_ERROR: input param(buffer_size) error(buffer_size = %ld): %s\n", buffer_size, __func__);
		return -nv_invalid_size;
	}

	ret = get_nonvolatile_sub(buffer, id, buffer_size, 0, &item_size);
	n3printk(KERN_INFO "APPNV: get_nonvolatile() id = %d, buffer_size = %d, ret = %d\n",
			(int)id, (int)buffer_size, ret);
	return ret;
}
EXPORT_SYMBOL(get_nonvolatile);


static int get_nonvolatile_sub(uint8_t* buffer, unsigned long id, unsigned long buffer_size, unsigned long aEncrypted, unsigned long* aOutItemSize)
{
	int i = 0;
	int j = 0;

	unsigned long result_buffer_size = 0;

	int ret = 0;


	mutex_lock(&nv_lock);

	if (!buffer) {
		printk(KERN_ERR "APPNV_ERROR: buffer is NULL: (id = %ld): %s\n", id, __func__);
		mutex_unlock(&nv_lock);
		return -nv_invalid_buffer;
	} else if (buffer_size > MAX_ITEM_SIZE) {
		printk(KERN_ERR "APPNV_ERROR: buffer size over 1048576(1MB): (id = %ld, buffer_size = %ld): %s\n", id, buffer_size, __func__);
		mutex_unlock(&nv_lock);
		return -nv_invalid_size;
	}

	if (!aOutItemSize) {
		printk(KERN_ERR "APPNV_ERROR: aOutItemSize is NULL: (id = %ld)%s\n", id, __func__);
		mutex_unlock(&nv_lock);
		return -nv_invalid_buffer;
	}

	if (!nonvolatile_device->initialized) {
		ret = init_nv_item();
		if (ret != 0) {
			if (nonvolatile_device->early_initialized) {
				ret = get_early_init_item(buffer, id, buffer_size);
				printk(KERN_INFO "APPNV: get_nonvolatile_sub() end:(id = %ld, result = %d)\n", id, ret);
				mutex_unlock(&nv_lock);
				return ret;
			}

			printk(KERN_INFO "APPNV: get_nonvolatile_sub() end: not initialized.(id = %ld, result = %d)\n", id, ret);

			mutex_unlock(&nv_lock);
			return ret;
		}
	}

	for (i = 0; i < nv_kind_count; i++) {
		for (j = 0; j < g_nv_data_table[i].nv_item_num; j++) {
			if(g_nv_data_table[i].nv_item_data[j].item_info.item_id == id) {
				if (!(g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_active)) {
					printk(KERN_INFO "APPNV: get_nonvolatile_sub() end: not active.(id = %ld, result = %d)\n", id, ret);
					mutex_unlock(&nv_lock);
					return -nv_not_active;
				}

				if (!aEncrypted && g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_secure) {
					*aOutItemSize = g_nv_data_table[i].nv_item_data[j].item_info.item_size;
					printk(KERN_INFO "APPNV: get_nonvolatile_sub() end: encrypted item.(id = %ld, result = %d)\n", id, ret);
					mutex_unlock(&nv_lock);
					return -nv_encrypted;
				} else if (aEncrypted) {
					result_buffer_size = buffer_size;
				} else if (g_nv_data_table[i].nv_item_data[j].item_info.item_size < buffer_size)	{
					result_buffer_size = g_nv_data_table[i].nv_item_data[j].item_info.item_size;
				} else {
					result_buffer_size = buffer_size;
				}

				memcpy(buffer, g_nv_data_table[i].nv_item_data[j].item_data, result_buffer_size);
				mutex_unlock(&nv_lock);
				return result_buffer_size;
			}
		}
	}
	printk(KERN_INFO "APPNV: get_nonvolatile_sub() end: invalid id.(id = %ld, result = %d)\n", id, ret);
	mutex_unlock(&nv_lock);

	return -nv_invalid_id;
}


int set_nonvolatile(uint8_t* buffer, unsigned long id, unsigned long buffer_size)
{
	unsigned long item_size;
	int ret = 0;

	ret = set_nonvolatile_sub(buffer, id, buffer_size, 0, &item_size);
	n3printk(KERN_INFO "APPNV: set_nonvolatile() id = %d, buffer_size = %d, ret = %d\n",
			(int)id, (int)buffer_size, ret);
	return ret;
}
EXPORT_SYMBOL(set_nonvolatile);


static int set_nonvolatile_sub(uint8_t* buffer, unsigned long id, unsigned long buffer_size, unsigned long  aEncrypted, unsigned long* aOutItemSize)
{
	unsigned int i = 0;
	unsigned int j = 0;
	
	bool			buffer_updated		= false;
	uint8_t*		buffer_temp			= NULL;
	unsigned long	buffer_temp_size	= 0;
	int				temp_flag			= 0;
	int				item_num			= 0;

	unsigned long	result_buffer_size	= 0;

	int ret = 0;

	mutex_lock(&nv_lock);

	if (!buffer) {
		printk(KERN_ERR "APPNV_ERROR: buffer is NULL: (id = %ld)%s\n", id, __func__);
		mutex_unlock(&nv_lock);
		return -nv_invalid_buffer;
	} else if (buffer_size == 0 || buffer_size > MAX_ITEM_SIZE) {
		printk(KERN_ERR "APPNV_ERROR: input param(buffer_size) error(id = %ld, buffer_size = %ld): %s\n", id, buffer_size, __func__);
		mutex_unlock(&nv_lock);
		return -nv_invalid_size;
	}

	if (!aOutItemSize) {
		printk(KERN_ERR "APPNV_ERROR: aOutItemSize is NULL: (id = %ld)%s\n", id, __func__);
		mutex_unlock(&nv_lock);
		return -nv_invalid_buffer;
	}

	if (!nonvolatile_device->initialized) {
		ret = init_nv_item();
		if (ret != 0) {
			printk(KERN_INFO "APPNV: set_nonvolatile_sub() end: not initialized.(id = %ld)\n", id);
			mutex_unlock(&nv_lock);
			return -nv_not_initialized;
		}
	}

	for (i = 0; i < nv_kind_count; i++) {
		for (j = 0; j < g_nv_data_table[i].nv_item_num; j++) {
			if(g_nv_data_table[i].nv_item_data[j].item_info.item_id == id) {
				if (!aEncrypted && g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_secure) {
					*aOutItemSize = g_nv_data_table[i].nv_item_data[j].item_info.item_size;
					printk(KERN_INFO "APPNV: set_nonvolatile_sub() end: encrypted item.(id = %ld)\n", id);
					mutex_unlock(&nv_lock);
					return -nv_encrypted;
				}
				else if (aEncrypted) {
					result_buffer_size = encrypted_size(g_nv_data_table[i].nv_item_data[j].item_info.item_size);
				} else if (g_nv_data_table[i].nv_item_data[j].item_info.item_size < buffer_size)	{
					result_buffer_size = g_nv_data_table[i].nv_item_data[j].item_info.item_size;
				} else {
					result_buffer_size = buffer_size;
				}

				buffer_temp_size = auto_resize(g_nv_data_table[i].nv_item_data[j].item_info.item_flag,
													g_nv_data_table[i].nv_item_data[j].item_info.item_size);
				buffer_temp = kzalloc(buffer_temp_size, GFP_KERNEL);
				if (!buffer_temp) {
					printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_sub() end: kzalloc buffer_temp error.(id = %ld)\n", id);
					mutex_unlock(&nv_lock);
					return -EFAULT;
				}

				memcpy(buffer_temp, buffer, result_buffer_size);

				ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
										(g_nv_data_table[i].nv_data_area_offset) + (g_nv_data_table[i].nv_item_data[j].item_info.item_offset),
										buffer_temp,
										buffer_temp_size);
				if (ret != 0) {
					printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_sub() end: block_write_verify(buffer_temp) error.(id = %ld)\n", id);
					goto error;
				}
				
				if (is_mc_mode()
						&& (g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_backup)
						&& !(g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_master_clear)) {
					ret = add_backup_item(&(g_nv_data_table[i].nv_item_data[j].item_info), buffer_temp, buffer_temp_size);
					if (ret != 0) {
						printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_sub(): backup item add error.(id = %ld)\n", id);
						ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
												(g_nv_data_table[i].nv_data_area_offset) + (g_nv_data_table[i].nv_item_data[j].item_info.item_offset),
												g_nv_data_table[i].nv_item_data[j].item_data,
												buffer_temp_size);
						if (ret != 0) {
							printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_sub() end: nv data write back error.(data mismatch)(id = %ld)\n", id);
							goto error;
						}
						printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_sub() end: nv data write back success.(data mismatch)(id = %ld)\n", id);
						ret = -1;
						goto error;
					}
				}

				if (!(g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_active)) {
					temp_flag = g_nv_data_table[i].nv_item_data[j].item_info.item_flag | nv_item_flag_active;
					ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
											sizeof(struct nv_info_area) + (sizeof(struct nv_item_info) * item_num) + sizeof(int),
											(uint8_t *)(&temp_flag),
											sizeof(int));
					if (ret != 0) {
						printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
						if (is_mc_mode()
								&& (g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_backup)
								&& !(g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_master_clear)) {
							ret = remove_last_backup_item();
							if (ret != 0) {
								printk(KERN_ERR "APPNV_ERROR: remove_last_backup_item(): %s\n", __func__);
								goto error;
							}
						}
						ret = -1;
						goto error;
					}
				}

				memcpy(g_nv_data_table[i].nv_item_data[j].item_data,
							buffer_temp,
							buffer_temp_size);

				g_nv_data_table[i].nv_item_data[j].item_info.item_flag |= nv_item_flag_active;

				buffer_updated = true;
				break;
			}
			item_num++;
		}

		if(buffer_updated){
			break;
		}
	}

	if (!buffer_updated) {
		ret = -nv_invalid_id;
		printk(KERN_INFO "APPNV: set_nonvolatile_sub() end: invalid id.(id = %ld, result = %d)\n", id, ret);

		goto error;
	}


error:
	if (buffer_temp) {
		kfree(buffer_temp);
		buffer_temp = NULL;
	}

	if (ret == 0) {
		ret = result_buffer_size;
	}

	mutex_unlock(&nv_lock);

	return ret;
}


static int set_nonvolatile_no_lock(uint8_t* buffer, unsigned long id, unsigned long buffer_size)
{
	unsigned int i = 0;
	unsigned int j = 0;

	bool			buffer_updated		= false;
	uint8_t*		buffer_temp			= NULL;
	unsigned long	buffer_temp_size	= 0;
	int				temp_flag			= 0;
	int				item_num			= 0;
	unsigned long	result_buffer_size	= 0;

	int ret = 0;


	if (!buffer) {
		printk(KERN_ERR "APPNV_ERROR: buffer is NULL: (id = %ld): %s\n", id, __func__);
		return -nv_invalid_buffer;
	} else if (buffer_size <= 0 || buffer_size > MAX_ITEM_SIZE) {
		printk(KERN_ERR "APPNV_ERROR: input param(buffer_size) error(id = %ld, buffer_size = %ld): %s\n", id, buffer_size, __func__);
		return -nv_invalid_size;
	}
	
	for (i = 0; i < nv_kind_count; i++) {
		for (j = 0; j < g_nv_data_table[i].nv_item_num; j++) {

			if(g_nv_data_table[i].nv_item_data[j].item_info.item_id == id) {
				if(g_nv_data_table[i].nv_item_data[j].item_info.item_size < buffer_size) {
					result_buffer_size = g_nv_data_table[i].nv_item_data[j].item_info.item_size;
				} else {
					result_buffer_size = buffer_size;
				}

				buffer_temp_size = auto_resize(g_nv_data_table[i].nv_item_data[j].item_info.item_flag,
													g_nv_data_table[i].nv_item_data[j].item_info.item_size);
				buffer_temp = kzalloc(buffer_temp_size, GFP_KERNEL);
				if (!buffer_temp) {
					printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_no_lock() end: kzalloc buffer_temp error.(id = %ld)\n", id);
					return -EFAULT;
				}
				memcpy(buffer_temp, buffer, result_buffer_size);

				ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
										(g_nv_data_table[i].nv_data_area_offset) + (g_nv_data_table[i].nv_item_data[j].item_info.item_offset),
										buffer_temp,
										buffer_temp_size);
				if (ret != 0) {
					printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_no_lock() end: block_write_verify(buffer_temp) error.(id = %ld)\n", id);
					goto error;
				}

				if (!(g_nv_data_table[i].nv_item_data[j].item_info.item_flag & nv_item_flag_active)) {

					temp_flag = g_nv_data_table[i].nv_item_data[j].item_info.item_flag | nv_item_flag_active;
					ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
											sizeof(struct nv_info_area) + (sizeof(struct nv_item_info) * item_num) + sizeof(int),
											(uint8_t *)(&temp_flag),
											sizeof(int));
					if (ret != 0) {
						printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_no_lock() end: block_read(temp_flag) error.(id = %ld)\n", id);
						goto error;
					}
				}

				memcpy(g_nv_data_table[i].nv_item_data[j].item_data,
							buffer_temp,
							buffer_temp_size);

				g_nv_data_table[i].nv_item_data[j].item_info.item_flag |= nv_item_flag_active;

				buffer_updated = true;
				break;
			}
			item_num++;
		}

		if(buffer_updated){
			break;
		}
	}

	if (!buffer_updated) {
		ret = -nv_invalid_id;
		printk(KERN_INFO "APPNV: set_nonvolatile_no_lock() end: invalid id.(id = %ld)\n", id);
		goto error;
	}


error:
	if (buffer_temp) {
		kfree(buffer_temp);
		buffer_temp = NULL;
	}

	if (ret == 0) {
		ret = result_buffer_size;
	}
	
	return ret;
}


int clear_backup_area(enum nv_mc_clear_mode clear_mode)
{
	int i = 0;

	struct nv_backup_area_info	backup_item_area_info;
	uint8_t						*zero_fill_buf;

	int ret = 0;


	if(clear_mode < 0 || clear_mode >= nv_mc_clear_mode_count){
		printk(KERN_ERR "APPNV_ERROR: input param(clear_mode) error: %s\n", __func__);
		return -1;
	}

	switch(clear_mode){
	case nv_mc_clear_master:
	case nv_mc_clear_type_2:	/* FALL THRU */
		// nothing to do.
		break;
	case nv_mc_clear_type_1:
		ret = block_read(NONVOLATILE_BACKUP_PARTITION_NAME,
							0,
							(uint8_t *)&backup_item_area_info,
							sizeof(struct nv_item_area_info));
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
			return ret;
		}

		if(memcmp((backup_item_area_info.backup_area_state), backup_signature, 4) != 0){
			return 0;
		}

		zero_fill_buf = kzalloc(sizeof(struct nv_item_info) * SIZE_4KB, GFP_KERNEL);
		if(!zero_fill_buf){
			printk(KERN_INFO "APPNV_INFO: kzalloc() zero_fill_buf error.: %s\n", __func__);
			return -EFAULT;
		}
		
		for (i = 0; i < (sizeof(struct nv_item_area_info) + (sizeof(struct nv_item_info) * backup_item_area_info.item_num) + SIZE_4KB - 1) / SIZE_4KB; i++) {
			ret = block_write_verify(NONVOLATILE_BACKUP_PARTITION_NAME,
										i * SIZE_4KB,
										zero_fill_buf,
										sizeof(uint8_t) * SIZE_4KB);
			if (ret != 0) {
				printk(KERN_ERR "APPNV_ERROR: block_write_verify() INFO AREA: %s\n", __func__);
				goto free_zerofill_buf;
			}
		}
		
		for (i = 0; i < (backup_item_area_info.item_total_size + SIZE_4KB - 1) / SIZE_4KB; i++) {
			ret = block_write_verify(NONVOLATILE_BACKUP_PARTITION_NAME,
										BACKUP_AREA_DATA_OFFSET + (i * SIZE_4KB),
										zero_fill_buf,
										sizeof(uint8_t) * SIZE_4KB);
			if (ret != 0) {
				printk(KERN_ERR "APPNV_ERROR: block_write_verify() DATA AREA: %s\n", __func__);
				ret = 0;
				goto free_zerofill_buf;
			}
		}

free_zerofill_buf:
		if(zero_fill_buf){
			kfree(zero_fill_buf);
			zero_fill_buf = NULL;
		}

		break;
	default:
		// nothing to do.

		break;
	}

	return ret;
}


int clear_nonvolatile(enum nv_mc_clear_mode clear_mode)
{
	int i = 0;
	int j = 0;

	int item_count = 0;
	bool activate_flag = false;
	struct nv_item_info	*item_info_buffer = NULL;
	struct nv_backup_area_info	backup_item_area_info;
	struct backup_item_list 	*backup_item_list				= NULL;

	unsigned int	flag;
	unsigned long	size;
	uint8_t			*data;

	int ret = 0;

	printk(KERN_INFO "APPNV: clear_nonvolatile() start mode:%d\n", clear_mode);

	if(clear_mode < 0 || clear_mode >= nv_mc_clear_mode_count){
		printk(KERN_ERR "APPNV_ERROR: input param(clear_mode) error(%d): %s\n", clear_mode, __func__);
		return -1;
	}

	if(clear_mode == nv_mc_clear_type_1 && !is_mc_mode()){
		printk(KERN_ERR "APPNV_ERROR: clear type 1 works in mc mode: %s\n", __func__);
		return -1;
	}

	mutex_lock(&nv_lock);

	if (!nonvolatile_device->initialized) {
		ret = init_nv_item();
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: init_nv_item(): %s\n", __func__);
			mutex_unlock(&nv_lock);
			return -nv_not_initialized;
		}
	}

	item_info_buffer = kzalloc((sizeof(struct nv_item_info) * g_nv_emmc_info_area.nv_manage_area.total_item_num), GFP_KERNEL);
	if (!item_info_buffer) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
		mutex_unlock(&nv_lock);
		return -EFAULT;
	}

	if (clear_mode == nv_mc_clear_master) {
		for (i = 0; i < nv_kind_count; i++) {
			for (j = 0; j < g_nv_data_table[i].nv_item_num; j++){
				memcpy(&(item_info_buffer[item_count]), &(g_nv_data_table[i].nv_item_data[j].item_info), sizeof(struct nv_item_info));
				if (item_info_buffer[item_count].item_flag & nv_item_flag_master_clear){
					item_info_buffer[item_count].item_flag &= ~nv_item_flag_active;
				}
				item_count++;
			}
		}
	} else {
		for (i = 0; i < nv_kind_count; i++) {
			for (j = 0; j < g_nv_data_table[i].nv_item_num; j++){
				memcpy(&(item_info_buffer[item_count]), &(g_nv_data_table[i].nv_item_data[j].item_info), sizeof(struct nv_item_info));

				activate_flag = false;
				switch(clear_mode){
					case nv_mc_clear_type_1:
						activate_flag = true;
						break;
					case nv_mc_clear_type_2:
						if (item_info_buffer[item_count].item_flag & nv_item_flag_clear_type_2) {
							activate_flag = true;
						}
						break;
					default:
						break;
				}
				
				if (activate_flag){
					item_info_buffer[item_count].item_flag &= ~nv_item_flag_active;
				}
				item_count++;
			}
		}
	}

	if (g_nv_emmc_info_area.nv_manage_area.total_item_num != 0) {

		ret = block_write_verify(NONVOLATILE_PARTITION_NAME,
					sizeof(struct nv_info_area),
					(uint8_t *)item_info_buffer,
					sizeof(struct nv_item_info) * g_nv_emmc_info_area.nv_manage_area.total_item_num);
		if (ret != 0) {
			printk(KERN_ERR "APPNV_ERROR: block_write_verify(): %s\n", __func__);
			goto error;
		}
	}

	ret = activate_nv_item();
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: activate_nv_item(): %s\n", __func__);
		goto error;
	}

	ret = block_read(NONVOLATILE_BACKUP_PARTITION_NAME,
						0,
						(uint8_t *)&backup_item_area_info,
						sizeof(struct nv_backup_area_info));
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: block_read(): %s\n", __func__);
		goto error;
	}

	if(memcmp((backup_item_area_info.backup_area_state), backup_signature, sizeof(uint8_t) * 4) == 0){
		switch (clear_mode) {
		case nv_mc_clear_master:
			ret = get_backup_item_list(&backup_item_list);
			if(ret != 0){
				printk(KERN_ERR "APPNV_ERROR: get_backup_item_list(): %s\n", __func__);
				goto error;
			}

			for (i = 0; i < backup_item_list->backup_item_num; i++) {
				ret = get_nv_item_data(backup_item_list->backup_item[i].item_info.item_id, &flag, &size, &data);
				if (ret != 0) {
					continue;
				}

				if ((size == backup_item_list->backup_item[i].item_info.item_size)
						&& (memcmp(data, backup_item_list->backup_item[i].item_data, size) != 0)) {
					ret = set_nonvolatile_no_lock(backup_item_list->backup_item[i].item_data,
														backup_item_list->backup_item[i].item_info.item_id,
														backup_item_list->backup_item[i].item_info.item_size);
					if (ret <= 0) {
						printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_no_lock(): %s\n", __func__);
						goto error2;
					}
					ret = 0;
				}
			}	

			free_backup_item_list(&backup_item_list);

			break;
		case nv_mc_clear_type_1:
			ret = clear_backup_area(clear_mode);
			if(ret != 0){
				printk(KERN_ERR "APPNV_ERROR: clear_backup_area()(nv_mc_clear_type_1): %s\n", __func__);
				goto error;
			}

			break;
		case nv_mc_clear_type_2:
			// nothing to do.
			break;
		default:
			// nothing to do.
			break;
		}
	}


error2:
	free_backup_item_list(&backup_item_list);

error:
	if (item_info_buffer) {
		kfree(item_info_buffer);
		item_info_buffer = NULL;
	}

	mutex_unlock(&nv_lock);

	printk(KERN_INFO "APPNV: clear_nonvolatile() end ret:%d\n", ret);

	return ret;
}
//EXPORT_SYMBOL(clear_nonvolatile);


int set_boot_nonvoltaile(loff_t offset_from_bootarea_begin, const uint8_t *buf, size_t len)
{
	if(!buf){
		printk(KERN_ERR "APPNV_ERROR: buf is NULL: %s\n", __func__);
		return -1;
	}

	if(len == 0 || len > MAX_ITEM_SIZE){
		printk(KERN_ERR "APPNV_ERROR: input param(len) error: %s\n", __func__);
		return -1;
	}

	return block_write_verify(NONVOLATILE_BOOT_MODE_PARTITION_NAME, offset_from_bootarea_begin, buf, len);
}
EXPORT_SYMBOL(set_boot_nonvoltaile);


static long nonvolatile_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
	enum nv_mc_clear_mode clear_mode;
	nonvolatile_data_t data;
	char *workBuff = NULL;
	int result = 0;
	int ret = 0;


	if (cmd == IOCTL_CLEAR_NONVOLATILE) {
		if (copy_from_user(&clear_mode, (enum nv_mc_clear_mode *)arg, sizeof(enum nv_mc_clear_mode)) != 0) {
			printk(KERN_ERR "APPNV_ERROR: copy_from_user(): %s\n", __func__);
			ret = -nv_invalid_buffer;
		} else {
			ret = clear_nonvolatile(clear_mode);
			if (ret != 0) {
				printk(KERN_ERR "APPNV_WARNING: clear_nonvolatile() ret(%d): %s\n", ret, __func__);
			}
		}

		return ret;
	}

	if (copy_from_user(&data, (nonvolatile_data_t *)arg, sizeof(nonvolatile_data_t)) != 0) {
		printk(KERN_ERR "APPNV_ERROR: copy_from_user(): %s\n", __func__);
		return -nv_invalid_buffer;
	}

	if(data.iSize <= 0){
		printk(KERN_ERR "APPNV_ERROR: data buffer size error(data.iSize = %ld): %s\n", data.iSize, __func__);
		return -nv_invalid_size;
	}
	
	workBuff = kzalloc(data.iSize, GFP_KERNEL);

	if (!workBuff) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);

		return -nv_invalid_buffer;
	}

	if (copy_from_user(workBuff, data.iBuff, data.iSize) != 0) {
		printk(KERN_ERR "APPNV_ERROR: copy_from_user(): %s\n", __func__);
		ret = -nv_invalid_buffer;
		goto error;
	}

	switch (cmd) {
	case IOCTL_SET_NONVOLATILE:
		result = set_nonvolatile_sub(workBuff, data.iId, data.iSize, data.iEncrypted, &data.iSize);
		n3printk(KERN_INFO "APPNV: IOCTL_SET_NONVOLATILE id = %d, buffer_size = %d, ret = %d\n",
				(int)data.iId, (int)data.iSize, result);
		if (result <= 0) {
			printk(KERN_ERR "APPNV_ERROR: set_nonvolatile_sub(): %s\n", __func__);
			goto error;
		}

		ret = set_boot_mode_item(data.iId, workBuff, data.iSize);
		if (ret != 0) {
			printk(KERN_ERR "error: can't update boot mode item.(id = %ld)\n", data.iId);
			goto error;
		}
		break;
	case IOCTL_GET_NONVOLATILE:
		result = get_nonvolatile_sub(workBuff, data.iId, data.iSize, data.iEncrypted, &data.iSize);
		n3printk(KERN_INFO "APPNV: IOCTL_GET_NONVOLATILE id = %d, buffer_size = %d, ret = %d\n",
				(int)data.iId, (int)data.iSize, result);

		if (result > 0) {
			if (copy_to_user(data.iBuff, workBuff, data.iSize) != 0){
				printk(KERN_ERR "APPNV_ERROR: copy_to_user(): %s\n", __func__);
				ret = -nv_invalid_buffer;
				goto error;
			}
		} else {
			printk(KERN_ERR "APPNV_ERROR: get_nonvolatile_sub() error(ret = %d): %s\n", ret, __func__);
			
			ret = -1;
			goto error;
		}
		break;
	default:
		printk(KERN_ERR "APPNV_ERROR: switch(): default: %s\n", __func__);
		ret = -ENOTTY;
		goto error;
	}

	data.iResult = result;
	if (copy_to_user((nonvolatile_data_t *)arg, &data, sizeof(nonvolatile_data_t)) != 0){
		printk(KERN_ERR "APPNV_ERROR: copy_to_user(): %s\n", __func__);
		ret = -nv_invalid_buffer;
		goto error;
	}


error:
	if (workBuff != NULL) {
		kfree(workBuff);
		workBuff = NULL;
	}

	return ret;
}


static int nonvolatile_open(struct inode *inode, struct file *filp)
{
	return 0;
}


static int nonvolatile_close(struct inode *inode, struct file *filp)
{
	return 0;
}


struct file_operations nonvolatile_fops = {
	.owner   = THIS_MODULE,
	.open    = nonvolatile_open,
	.release = nonvolatile_close,
	.unlocked_ioctl   = nonvolatile_ioctl,
};


static int init_early_init_item(void)
{
	unsigned int i = 0;

	unsigned int	*smem_early_init_item_num	= NULL;
	uint8_t			*smem_early_init_item_data	= NULL;
	unsigned int offset = 0;

	int ret = 0;

	printk(KERN_INFO "APPNV: init_early_init_item() start\n");

	smem_early_init_item_num = smem_alloc_vendor0(SMEM_OEM_V0_001);
	if(!smem_early_init_item_num)	{
		printk(KERN_ERR "APPNV_ERROR: smem_alloc_vendor0(): %s\n", __func__);
		return -ENOMEM;
	}

	if ((*smem_early_init_item_num) >= 0) {
		g_early_init_item_num = *smem_early_init_item_num;
	} else {
		g_early_init_item_num = 0;
	}

	if (g_early_init_item_num < 0 || g_early_init_item_num > MAX_ITEM_NUM) {
		printk(KERN_ERR "APPNV_ERROR: item num error(%d): %s\n", g_early_init_item_num, __func__);
		return -1;
	}

	if (g_early_init_item_num == 0) {
		nonvolatile_device->early_initialized = true;
		printk(KERN_INFO "APPNV_INFO: NV early initialized item none.: %s\n", __func__);
		return 0;
	}

	g_early_init_item_data = kzalloc(sizeof(struct nv_item) * g_early_init_item_num, GFP_KERNEL);
	if (!g_early_init_item_data) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
		return -EFAULT;
	}

	smem_early_init_item_data = smem_alloc_vendor0(SMEM_OEM_V0_002);
	if(!smem_early_init_item_data)	{
		printk(KERN_ERR "APPNV_ERROR: smem_alloc_vendor0(): %s\n", __func__);

		g_early_init_item_num = 0;
		nonvolatile_device->early_initialized = true;

		ret = -ENOMEM;
		goto early_init_item_free;
	}

	offset = 0;
	for (i = 0; i < g_early_init_item_num; i++) {
		if (offset + sizeof(struct nv_item_info) + g_early_init_item_data[i].item_info.item_size > SMEM_OEM_V0_002_FUNC_SIZE) {
			printk(KERN_WARNING "APPNV_WARNING: ShareMem full, Some items were skipped.\n");
			g_early_init_item_num = i;
			break;
		}

		memcpy(&(g_early_init_item_data[i].item_info), (uint8_t *)(smem_early_init_item_data + offset), sizeof(struct nv_item_info));
		offset += sizeof(struct nv_item_info);

		if (g_early_init_item_data[i].item_info.item_size > 0) {
			g_early_init_item_data[i].item_data = kzalloc((sizeof(uint8_t) * g_early_init_item_data[i].item_info.item_size), GFP_KERNEL);
			if (!g_early_init_item_data[i].item_data) {
				printk(KERN_ERR	 "APPNV_ERROR: kzalloc(): %s\n", __func__);
				ret = -EFAULT;
				goto early_init_item_free;
			}

			memcpy(g_early_init_item_data[i].item_data,
					(uint8_t *)(smem_early_init_item_data + offset),
					sizeof(uint8_t) * g_early_init_item_data[i].item_info.item_size);

			offset += g_early_init_item_data[i].item_info.item_size;
		}
	}

	nonvolatile_device->early_initialized = true;

	smem_early_init_item_num	= NULL;
	smem_early_init_item_data	= NULL;

	printk(KERN_INFO "APPNV: init_early_init_item() num = %d size = %d\n", g_early_init_item_num, offset);
	return ret;

early_init_item_free:
	smem_early_init_item_num	= NULL;
	smem_early_init_item_data	= NULL;

	free_early_init_item();

	printk(KERN_INFO "APPNV: init_early_init_item() fail ret = %d\n", ret);
	return ret;
}


static int Initialize(void)
{
	return init_early_init_item();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static int nonvolatile_drv_probe(struct platform_device *pdev)
{
	dev_t dev = MKDEV(0, 0);
	int ret;
	struct device *class_dev = NULL;


	nonvolatile_device = kzalloc(sizeof(struct nonvolatile_device), GFP_KERNEL);
	if (nonvolatile_device == NULL) {
		printk(KERN_ERR "APPNV_ERROR: kzalloc(): %s\n", __func__);
		return -EFAULT;
	}

    ret = alloc_chrdev_region(&dev, 0, nonvolatile_devs, DRIVER_NAME);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: alloc_chrdev_region(): %s\n", __func__);
		return ret;
	}

    nonvolatile_major = MAJOR(dev);

    cdev_init(&nonvolatile_cdev, &nonvolatile_fops);

	nonvolatile_cdev.owner = THIS_MODULE;
	nonvolatile_cdev.ops   = &nonvolatile_fops;

	ret = cdev_add(&nonvolatile_cdev, MKDEV(nonvolatile_major, nonvolatile_minor), 1);
	if (ret != 0) {
		printk(KERN_ERR "APPNV_ERROR: cdev_add(): %s\n", __func__);
		return ret;
	}

	nonvolatile_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(nonvolatile_class)) {
		printk(KERN_ERR "APPNV_ERROR: class_create(): %s\n", __func__);
		return PTR_ERR(nonvolatile_class);
	}

	class_dev = device_create(nonvolatile_class,
					NULL,
					MKDEV(nonvolatile_major, nonvolatile_minor),
					NULL,
					"%s",
					DEVICE_NAME);

	mutex_init(&nonvolatile_device->lock);
	mutex_init(&nv_lock);

	nonvolatile_device->initialized			= false;
	nonvolatile_device->early_initialized	= false;

	ret = Initialize();
	if (ret) {
		printk(KERN_ERR "APPNV_ERROR: Initialize(): %s\n", __func__);
		return ret;
	}
	
	nonvolatile_debugfs_init();

	return 0;
}


static int nonvolatile_drv_remove(struct platform_device *pdev)
{
	free_nv_cache();

	if (nonvolatile_device) {
		kfree(nonvolatile_device);
		nonvolatile_device = NULL;
	}

	nonvolatile_debugfs_remove();
	
	return 0;
}


static struct platform_device nonvolatile_devices = {
	.name					= "nonvolatile",
	.id						= -1,
	.dev					= {
	.dma_mask				= NULL,
	.coherent_dma_mask	= 0xffffffff,
	},
};


static struct platform_device *devices[] __initdata = {
	&nonvolatile_devices,
};


static struct platform_driver nonvolatile_driver = {
	.probe    = nonvolatile_drv_probe,
	.remove   = nonvolatile_drv_remove,
	.driver   = {
	.name = DRIVER_NAME,
	},
};


static int __init nonvolatile_init(void)
{
	platform_add_devices(devices, ARRAY_SIZE(devices));

	return platform_driver_register(&nonvolatile_driver);
}


static void __exit nonvolatile_exit(void)
{
	platform_driver_unregister(&nonvolatile_driver);
}


#ifdef CONFIG_DEBUG_FS
static ssize_t nonvolatile_debug_logset(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char charBuf;
	int log_level;

	if (count != 2) {
		printk(KERN_ERR "APPNV_ERROR:  count failed: %s\n", __func__);
		return -EINVAL;
	}
	
	if (copy_from_user(&charBuf, buf, sizeof(charBuf)) != 0) {
		printk(KERN_ERR "APPNV_ERROR:  copy_from_user failed: %s\n", __func__);
		return -EFAULT;
	}

	log_level = (int)(charBuf - '0');
	
	if ((log_level < MIN_DEBUG_LOGLEVEL) || (MAX_DEBUG_LOGLEVEL < log_level)) {
		printk(KERN_ERR "APPNV_EROR: debugfs loglevel setting error: %s\n", __func__);
		return -EINVAL;
	}

	nonvolatile_log_flg = log_level;

	return count;
}

static const struct file_operations nonvolatile_debugfs_operations = {
	.write	= nonvolatile_debug_logset,
};
#endif /* CONFIG_DEBUG_FS */


static void nonvolatile_debugfs_init(void)
{
#ifdef CONFIG_DEBUG_FS
	/* /sys/kernel/debug/nonvolatile */
	nonvolatile_debugfs = debugfs_create_file("nonvolatile", 
											  S_IWUSR,
											  NULL, 
											  NULL, 
											  &nonvolatile_debugfs_operations);
											  
	if (nonvolatile_debugfs == NULL) {
		printk(KERN_ERR "APPNV_ERROR: Failed to create nonvolatile debug file: %s\n", __func__);
	}
#endif	/* CONFIG_DEBUG_FS */

	return;
}


static void nonvolatile_debugfs_remove(void)
{
#ifdef CONFIG_DEBUG_FS
	if (nonvolatile_debugfs != NULL) {
		debugfs_remove(nonvolatile_debugfs);
	}
#endif	/* CONFIG_DEBUG_FS */
	
	return;
}



subsys_initcall(nonvolatile_init);
module_exit(nonvolatile_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("nonvolatile device");
MODULE_LICENSE("GPL");
