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
#include <linux/mtd/mtd.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <asm/uaccess.h>
#include <linux/crashdump.h>

#define FINDBLKDEV_BYNAME

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV
#include <linux/fsr_if.h>
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
#include <linux/genhd.h>
#include <linux/msdos_fs.h>
#endif

//#define dprintk printk
#define dprintk(...)

//#define dprintk2 printk
#define dprintk2(...)

//#define dprintk3 printk
#define dprintk3(...)

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
/* partition node structure copied from drivers/mtd/mtdpart.c */
struct mtd_part {
	struct mtd_info mtd;
	struct mtd_info *master;
	uint64_t offset;
	struct list_head list;
};
#endif

/*
 * String buffer
 * This buffer is for holding formatted string for
 * crashdump_write_str internal use.
 * This buffer can be any size larger enough to hold formatted string.
 * Caller of crashdump_write_str must not specify strings larger than
 * this size. If so, the extra part will be ignored.
 */
static char strbuf[STRBUFSZ];

/*
 * Buffer "to-be-flushed".
 * This buffer is for buffering logs less than page size.
 * This buffer is actually written to the flash by
 * crashdump_flush call.
 * This buffer should be the size of one page of the flash.
 */
static char tbflushbuf[TBFLUSHBUFSZ]; // one page size
static int tbflushbuf_index = 0; // index in the buffer where next byte goes
static unsigned int pagesize = sizeof(tbflushbuf);

#ifdef CONFIG_CRASHDUMP_TO_FLASH

static uint64_t crashdump_size;
static loff_t crashdump_off = 0;

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
static struct mtd_part *crashdump_part = NULL;
static unsigned int subpagesize;
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV
static const char *crashdump_blkdev_name = "/dev/block/bml"CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV_NUM;
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
static struct block_device *crashdump_part = NULL;
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
static int crashdump_findmtd(void)
{
	int retval;

	struct list_head* mtd_partitions;
	struct mtd_part *slave, *next;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	retval = 0;

	mtd_partitions = get_mtd_partitions();
	list_for_each_entry_safe(slave, next, mtd_partitions, list) {
		dprintk2(KERN_EMERG "<%s> 0x%012llx 0x%012llx\n", slave->mtd.name, (unsigned long long)slave->mtd.size, (unsigned long long)slave->offset);

		if ( strcmp(slave->mtd.name, CRASHDUMP_PARTITION_NAME) == 0 ) {
			crashdump_part = slave;
			break;
		}
	}

	if (crashdump_part == NULL) {
		printk(KERN_EMERG "crashdump partition is not available\n");
	}
	else {
		printk(KERN_EMERG "Crashdump to <%s>/<%s>\n", crashdump_part->master->name, crashdump_part->mtd.name);

		crashdump_size = crashdump_part->mtd.size;

		pagesize = crashdump_part->mtd.writesize;
		subpagesize = crashdump_part->mtd.writesize >> crashdump_part->mtd.subpage_sft;

		printk(KERN_EMERG "pagesize 0x%08x\n", pagesize);
		printk(KERN_EMERG "subpagesize 0x%08x\n", subpagesize);
		if (pagesize <= sizeof(tbflushbuf)) {
			retval = 1;
		}
		else {
			printk(KERN_EMERG "pagesize is too large. (max 0x%08x)\n", sizeof(tbflushbuf));
		}
	}

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return retval;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV
static int crashdump_findbml_blkdev(void)
{
	int retval;

	mm_segment_t fs;
	struct file *filp;

	int nErr;
	BML_DEVINFO_T stDevInfo;			// device information table
	BML_PARTTAB_T stPartTab;			// partition table structure
	int nPartNum;
	int i __attribute__((unused));

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	fs = get_fs();
	set_fs(KERNEL_DS);

	retval = 0;

	/* open */
	dprintk3(KERN_EMERG "***** filp_open *****\n");
	dprintk3(KERN_EMERG "0: crashdump_blkdev_name = %s\n", crashdump_blkdev_name);

	filp = filp_open(crashdump_blkdev_name, O_RDONLY, 0);

	dprintk3(KERN_EMERG "filp = %x\n", (unsigned int)filp);

	if ( IS_ERR(filp) ) {
		printk(KERN_EMERG "crashdump partition is not open\n");
		goto crashdump_findbml_blkdev_exit;
	}

	if ((filp->f_op == NULL) || (filp->f_op->unlocked_ioctl == NULL)) {
		printk(KERN_EMERG "crasudump partition doesn't support ioctl\n");
		goto crashdump_findbml_blkdev_exit2;
	}

	printk(KERN_EMERG "Crashdump to <%s>\n", crashdump_blkdev_name);

	/* FSR_GET_PART_INFO */
	dprintk3(KERN_EMERG "***** FSR_GET_PART_INFO *****\n");
	dprintk3(KERN_EMERG "0: filp = %x\n", (unsigned int)filp);

	nErr = filp->f_op->unlocked_ioctl(filp, FSR_GET_PART_INFO, (unsigned long)&stDevInfo);

	dprintk3(KERN_EMERG "nErr = %x\n", nErr);

	if (nErr != 0) {
		printk(KERN_EMERG "FSR_GET_PART_INFO error!\n");
		goto crashdump_findbml_blkdev_exit2;
	}

	dprintk3(KERN_EMERG "stDevInfo.phy_unit_size = %x\n", stDevInfo.phy_unit_size);
	dprintk3(KERN_EMERG "stDevInfo.num_units = %x\n", stDevInfo.num_units);
	dprintk3(KERN_EMERG "stDevInfo.page_msize = %x\n", stDevInfo.page_msize);
	dprintk3(KERN_EMERG "stDevInfo.page_ssize = %x\n", stDevInfo.page_ssize);
	dprintk3(KERN_EMERG "stDevInfo.dump_size = %x\n", stDevInfo.dump_size);

	/* FSR_GET_PART_TAB */
	dprintk3(KERN_EMERG "***** FSR_GET_PART_TAB *****\n");
	dprintk3(KERN_EMERG "0: filp = %x\n", (unsigned int)filp);

	nErr = filp->f_op->unlocked_ioctl(filp, FSR_GET_PART_TAB, (unsigned long)&stPartTab);

	dprintk3(KERN_EMERG "nErr = %x\n", nErr);

	if (nErr != 0) {
		printk(KERN_EMERG "FSR_GET_PART_TAB error!\n");
		goto crashdump_findbml_blkdev_exit2;
	}

	dprintk3(KERN_EMERG "stPartTab.num_parts = %x\n", stPartTab.num_parts);
	for (i = 0; i < stPartTab.num_parts; i++) {
		dprintk3(KERN_EMERG "stPartTab.part_size[%d] = %x\n", i, stPartTab.part_size[i]);
		dprintk3(KERN_EMERG "stPartTab.part_id[%d] = %x\n", i, stPartTab.part_id[i]);
		dprintk3(KERN_EMERG "stPartTab.part_attr[%d] = %x\n", i, stPartTab.part_attr[i]);
		dprintk3(KERN_EMERG "stPartTab.part_addr[%d] = %x\n", i, stPartTab.part_addr[i]);
	}

	nPartNum = simple_strtol(CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV_NUM, NULL, 10) - 1;
	if ((nPartNum < 0) || (nPartNum >= stPartTab.num_parts)) {
		printk(KERN_EMERG "crashdump partition was not found.\n");
		goto crashdump_findbml_blkdev_exit2;
	}

	crashdump_size = stPartTab.part_size[nPartNum] * stDevInfo.phy_unit_size;
	dprintk3(KERN_EMERG "crashdump_size = %llx\n", crashdump_size);

	pagesize = stDevInfo.page_msize;
	printk(KERN_EMERG "pagesize 0x%08x\n", pagesize);
	if (pagesize <= sizeof(tbflushbuf)) {
		retval = 1;
	}
	else {
		printk(KERN_EMERG "pagesize is too large. (max 0x%08x)\n", sizeof(tbflushbuf));
	}

crashdump_findbml_blkdev_exit2:
	/* close */
	dprintk3(KERN_EMERG "***** filp_close *****\n");
	dprintk3(KERN_EMERG "0: filp = %x\n", (unsigned int)filp);
	dprintk3(KERN_EMERG "1: current->files = %x\n", (unsigned int)current->files);

	filp_close(filp, current->files);

crashdump_findbml_blkdev_exit:
	set_fs(fs);

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return retval;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
#ifdef FINDBLKDEV_BYNAME
static char *crashdump_bdevt_str(dev_t devt, char *buf)
{
	if (MAJOR(devt) <= 0xff && MINOR(devt) <= 0xff) {
		char tbuf[BDEVT_SIZE];
		snprintf(tbuf, BDEVT_SIZE, "%02x%02x", MAJOR(devt), MINOR(devt));
		snprintf(buf, BDEVT_SIZE, "%-9s", tbuf);
	} else
		snprintf(buf, BDEVT_SIZE, "%03x:%05x", MAJOR(devt), MINOR(devt));

	return buf;
}

static int crashdump_findblkdev_byname(void)
{
	int myret;
	int retval;
	struct class_dev_iter iter;
	struct device *dev;

	myret = -1;

	class_dev_iter_init(&iter, &block_class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		struct gendisk *disk;
		struct disk_part_iter piter;
		struct hd_struct *part;
		char name_buf[BDEVNAME_SIZE];
		char devt_buf[BDEVT_SIZE];

		if ( strcmp(dev->type->name, "disk") != 0 ) {
			continue;
		}

		disk = dev_to_disk(dev);

		/*
		 * Don't show empty devices or things that have been
		 * surpressed
		 */
		if (get_capacity(disk) == 0 ||
		    (disk->flags & GENHD_FL_SUPPRESS_PARTITION_INFO))
			continue;

		printk(KERN_EMERG "disk name: (%s)\n", disk->disk_name);

		/*
		 * Note, unlike /proc/partitions, I am showing the
		 * numbers in hex - the same format as the root=
		 * option takes.
		 */
		disk_part_iter_init(&piter, disk, DISK_PITER_INCL_PART0);
		while (1) {
			bool is_part0;
			part = disk_part_iter_next(&piter);
			if (part == NULL) {
				break;
			}
			is_part0 = part == &disk->part0;

			printk("%s%s %10llu %s", is_part0 ? "" : "  ",
			       crashdump_bdevt_str(part_devt(part), devt_buf),
			       (unsigned long long)part->nr_sects >> 1,
			       disk_name(disk, part->partno, name_buf));
			if (is_part0) {
				if (disk->driverfs_dev != NULL &&
				    disk->driverfs_dev->driver != NULL)
					printk(" driver: %s\n",
					      disk->driverfs_dev->driver->name);
				else
					printk(" (driver?)\n");
			} else
				printk("\n");

			if ( strcmp(name_buf, CRASHDUMP_PARTITION_NAME) == 0 ) {
				printk(KERN_EMERG "partition no: %d  partition name: (%s)\n", part->partno, name_buf);
				printk(KERN_EMERG "start sector: %llu (0x%016llx)  total sectors: %llu (0x%016llx)\n",
					part->start_sect, part->start_sect, part->nr_sects, part->nr_sects);
				crashdump_part = bdget(part_devt(part));
				retval = blkdev_get(crashdump_part, FMODE_READ | FMODE_WRITE, NULL);
				if (retval) {
					crashdump_part = NULL;
					printk(KERN_EMERG "Failed to get block device: (%s)\n", name_buf);
					continue;
				}
				crashdump_size = part->nr_sects * SECTOR_SIZE;
				printk(KERN_EMERG "total bytes: %llu (0x%016llx)\n", crashdump_size, crashdump_size);
				myret = 0;
			}
		}
		disk_part_iter_exit(&piter);
	}
	class_dev_iter_exit(&iter);

	return myret;
}
#else
static int crashdump_findblkdev_static(const char *devname)
{
	int myret;
	int retval;
	struct block_device *bdev;
	fmode_t mode = FMODE_READ | FMODE_WRITE;
	int i;

	dprintk(KERN_EMERG "(%s:%s:%d) (%s)\n", __FILE__, __FUNCTION__, __LINE__, devname);
	myret = 0;

	bdev = lookup_bdev(devname);
	if(IS_ERR(bdev)) {
		printk(KERN_EMERG "Failed to lookup block device: (%s)\n", devname);
		myret = -1;
		goto crashdump_block_prinfo_exit;
	}

	retval = blkdev_get(bdev, mode, NULL);
	if(retval) {
		printk(KERN_EMERG "Failed to get block device: (%s)\n", devname);
		myret = -1;
		goto crashdump_block_prinfo_exit;
	}

	if (!bdev->bd_disk) {
		printk(KERN_EMERG "Failed to find disk information: (%s)\n", devname);
		myret = -1;
		goto crashdump_block_prinfo_exit1;
	}

	dprintk(KERN_EMERG "disk name: (%s)\n", bdev->bd_disk->disk_name);

	for (i=0; i<bdev->bd_disk->part_tbl->len; i++) {
		dprintk(KERN_EMERG "partition no: %d  partition name: (%s)\n", bdev->bd_disk->part_tbl->part[i]->partno, bdev->bd_disk->part_tbl->part[i]->info->volname);
		dprintk(KERN_EMERG "start sector: %llu (0x%016llx)  total sectors: %llu (0x%016llx)\n", bdev->bd_disk->part_tbl->part[i]->start_sect, bdev->bd_disk->part_tbl->part[i]->start_sect, bdev->bd_disk->part_tbl->part[i]->nr_sects, bdev->bd_disk->part_tbl->part[i]->nr_sects);
	}

	if (!bdev->bd_part) {
		printk(KERN_EMERG "Failed to find partition information: (%s)\n", devname);
		myret = -1;
		goto crashdump_block_prinfo_exit1;
	}

	dprintk(KERN_EMERG "partition no: %d  partition name: (%s)\n", bdev->bd_part->partno, bdev->bd_part->info->volname);
	dprintk(KERN_EMERG "start sector: %llu (0x%016llx)  total sectors: %llu (0x%016llx)\n", bdev->bd_part->start_sect, bdev->bd_part->start_sect, bdev->bd_part->nr_sects, bdev->bd_part->nr_sects);

	crashdump_part = bdev;
	crashdump_size = bdev->bd_part->nr_sects * SECTOR_SIZE;
	dprintk(KERN_EMERG "total bytes: %llu (0x%016llx)\n", crashdump_size, crashdump_size);

crashdump_block_prinfo_exit1:

crashdump_block_prinfo_exit:
	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return myret;
}
#endif

static int crashdump_findblkdev(void)
{
	int myret;
	int retval;

	myret = 0; // 0: not found  1: found
	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

#ifdef FINDBLKDEV_BYNAME
	retval = crashdump_findblkdev_byname();
#else
	retval = crashdump_findblkdev_static(CONFIG_CRASHDUMP_TO_FLASH_BLKDEV_PATH);
#endif

	if (retval < 0) {
		printk(KERN_EMERG "crashdump partition is not available\n");
		myret = 0;
		goto crashdump_findblkdev_exit;
	}

	myret = 1;

crashdump_findblkdev_exit:
	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return myret;
}
#endif

int crashdump_findpartition(void)
{
	int retval;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	retval = 0;

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
	retval = crashdump_findmtd();
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV
	retval = crashdump_findbml_blkdev();
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
	retval = crashdump_findblkdev();
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return retval;
}

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
static int crashdump_erasepartition_mtd(void)
{
	int retval;
	struct erase_info instr;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	instr.mtd = &crashdump_part->mtd;
	instr.addr = 0;
	instr.len = crashdump_size;
	instr.fail_addr = 0x00000000;
	instr.time = 0;
	instr.retries = 0;
	instr.dev = 0;
	instr.cell = 0;
	instr.callback = NULL;
	instr.priv = 0;
	instr.state = 0;
	instr.next = NULL;

	retval = crashdump_part->mtd.erase(&crashdump_part->mtd, &instr);
	if (retval < 0) {
		printk(KERN_EMERG "erase returned %d\n", retval);
		return -1;
	}

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV
static int crashdump_erasepartition_bml_blkdev(void)
{
	int retval;

	mm_segment_t fs;
	struct file *filp;
	int nErr;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	fs = get_fs();
	set_fs(KERNEL_DS);

	retval = -1;

	/* open */
	dprintk3(KERN_EMERG "***** filp_open *****\n");
	dprintk3(KERN_EMERG "0: crashdump_blkdev_name = %s\n", crashdump_blkdev_name);

	filp = filp_open(crashdump_blkdev_name, O_RDWR, 0);

	dprintk3(KERN_EMERG "filp = %x\n", (unsigned int)filp);

	if ( IS_ERR(filp) ) {
		printk(KERN_EMERG "crashdump partition is not open\n");
		goto crashdump_erasepartition_bml_blkdev_exit;
	}

	if ((filp->f_op == NULL) || (filp->f_op->unlocked_ioctl == NULL)) {
		printk(KERN_EMERG "crasudump partition doesn't support ioctl\n");
		goto crashdump_erasepartition_bml_blkdev_exit2;
	}

	/* BML_ERASE */
	dprintk3(KERN_EMERG "***** BML_ERASE *****\n");
	dprintk3(KERN_EMERG "0: filp = %x\n", (unsigned int)filp);

	nErr = filp->f_op->unlocked_ioctl(filp, BML_ERASE, (unsigned long)NULL);

	dprintk3(KERN_EMERG "nErr = %x\n", nErr);

	if (nErr != 0) {
		printk(KERN_EMERG "erase returned %d\n", nErr);
		goto crashdump_erasepartition_bml_blkdev_exit2;
	}

	/* OK */
	retval = 0;

crashdump_erasepartition_bml_blkdev_exit2:
	/* close */
	dprintk3(KERN_EMERG "***** filp_close *****\n");
	dprintk3(KERN_EMERG "0: filp = %x\n", (unsigned int)filp);
	dprintk3(KERN_EMERG "1: current->files = %x\n", (unsigned int)current->files);

	filp_close(filp, current->files);

crashdump_erasepartition_bml_blkdev_exit:
	set_fs(fs);

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return retval;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
static int crashdump_erasepartition_blkdev(void)
{
	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}
#endif

int crashdump_erasepartition(void)
{
	int retval;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	retval = -1;

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
	retval = crashdump_erasepartition_mtd();
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV
	retval = crashdump_erasepartition_bml_blkdev();
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
	retval = crashdump_erasepartition_blkdev();
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return retval;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_CONSOLE
extern void printch(int);
static int crashdump_uart_write(const u_char *buf, size_t len, size_t *retlen)
{
	*retlen = len;

	while (len-- > 0) {
		printch(*buf);
		buf++;
	}

	return 0;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
static int crashdump_writedo_mtd(char *abuf, int alen)
{
	int retval;
	loff_t off;
	size_t len;
	size_t retlen;
	u_char* buf;

	if ((crashdump_off + alen) > crashdump_size) {
		printk(KERN_EMERG "Not enough space in crashdump partition. required 0x%08x left 0x%08x bytes\n", (unsigned int)alen, (unsigned int)(crashdump_size - crashdump_off));
		return -1;
	}

	off = crashdump_off;
	len = (size_t)alen;
	retlen = 0;
	buf = (u_char*)abuf;

	retval = crashdump_part->mtd.panic_write(&crashdump_part->mtd, off, len, &retlen, buf);
	if (retval < 0) {
		printk(KERN_EMERG "panic_write returned %d\n", retval);
		return -1;
	}

	if (retlen == 0) {
		printk(KERN_EMERG "panic_write wrote 0 bytes\n");
		return -1;
	}

	if (retlen != len) {
		printk(KERN_EMERG "panic_write retlen 0x%08x != len 0x%08x\n", retlen, len);
	}

	dprintk2(KERN_EMERG "(%s:%s:%d) 0x%08x bytes written to 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, retlen, (unsigned int)crashdump_off);

	crashdump_off += retlen;

	return 0;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV
static int crashdump_writedo_bml_blkdev(char *abuf, int alen)
{
	int retval;

	mm_segment_t fs;
	struct file *filp;
	ssize_t nLen;

	retval = -1;

	fs = get_fs();
	set_fs(KERNEL_DS);

	if ((crashdump_off + alen) > crashdump_size) {
		printk(KERN_EMERG "Not enough space in crashdump partition. required 0x%08x left 0x%08x bytes\n", (unsigned int)alen, (unsigned int)(crashdump_size - crashdump_off));
		goto crashdump_writedo_bml_blkdev_exit;
	}

	/* open */
	dprintk3(KERN_EMERG "***** filp_open *****\n");
	dprintk3(KERN_EMERG "0: crashdump_blkdev_name = %s\n", crashdump_blkdev_name);

	filp = filp_open(crashdump_blkdev_name, O_WRONLY, 0);

	dprintk3(KERN_EMERG "filp = %x\n", (unsigned int)filp);

	if ( IS_ERR(filp) ) {
		printk(KERN_EMERG "crashdump partition is not open\n");
		goto crashdump_writedo_bml_blkdev_exit;
	}

	if ((filp->f_op == NULL) || (filp->f_op->write == NULL)) {
		printk(KERN_EMERG "crasudump partition doesn't support write\n");
		goto crashdump_writedo_bml_blkdev_exit2;
	}

	/* write */
	dprintk3(KERN_EMERG "***** write *****\n");
	dprintk3(KERN_EMERG "0: filp = %x\n", (unsigned int)filp);
	dprintk3(KERN_EMERG "1: abuf = %x\n", (unsigned int)abuf);
	dprintk3(KERN_EMERG "2: alen = %x\n", alen);
	dprintk3(KERN_EMERG "3: crashdump_off = %llx\n", crashdump_off);

	nLen = filp->f_op->write(filp, abuf, alen, &crashdump_off);

	dprintk3(KERN_EMERG "nLen = %x\n", nLen);
	dprintk3(KERN_EMERG "crashdump_off = %llx\n", crashdump_off);

	if (nLen <= 0) {
		printk(KERN_EMERG "crasudump partition is not write\n");
		goto crashdump_writedo_bml_blkdev_exit2;
	}

	/* OK */
	retval = 0;

crashdump_writedo_bml_blkdev_exit2:
	/* close */
	dprintk3(KERN_EMERG "***** filp_close *****\n");
	dprintk3(KERN_EMERG "0: filp = %x\n", (unsigned int)filp);
	dprintk3(KERN_EMERG "1: current->files = %x\n", (unsigned int)current->files);

	filp_close(filp, current->files);

crashdump_writedo_bml_blkdev_exit:
	set_fs(fs);

	return retval;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
static int crashdump_block_write(struct block_device *bdev, loff_t offset, const uint8_t *buf, size_t len, size_t *retlen)
{
	int myret;
	pgoff_t index;
	size_t start;
	size_t size;
	size_t len_in;
	struct page *page;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	myret = 0;
	*retlen = 0;

	index = offset >> PAGE_SHIFT;
	start = offset % PAGE_SIZE;
	len_in = len;

	do {
		if((start + len) > PAGE_SIZE) {
			size = PAGE_SIZE;
		}
		else {
			size = len;
		}

		page = read_mapping_page(bdev->bd_inode->i_mapping, index, NULL);
		if(page == NULL) {
			myret = -1;
			break;
		}
		if(IS_ERR(page)) {
			myret = -1;
			break;
		}

		lock_page(page);
		if(memcmp(page_address(page)+start, buf, size)) {
			memcpy(page_address(page)+start, buf, size);
			set_page_dirty(page);
		}
		unlock_page(page);
		page_cache_release(page);

		index++;
		start = 0;
		buf += size;
		len -= size;
	} while(len > 0);

	sync_blockdev(bdev);

	*retlen = len_in - len;
	if (*retlen) {
		myret = 0;
	}

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return myret;
}

static int crashdump_writedo_blkdev(char *abuf, int alen)
{
	int retval;
	loff_t off;
	size_t len;
	size_t retlen;
	u_char* buf;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	if ((crashdump_off + alen) > crashdump_size) {
		printk(KERN_EMERG "Not enough space in crashdump partition. required 0x%08x left 0x%08x bytes\n", (unsigned int)alen, (unsigned int)(crashdump_size - crashdump_off));
		return -1;
	}

	off = crashdump_off;
	len = (size_t)alen;
	retlen = 0;
	buf = (u_char*)abuf;

	retval = crashdump_block_write(crashdump_part, off, buf, len, &retlen);
	if (retval < 0) {
		printk(KERN_EMERG "crashdump_block_write returned %d\n", retval);
		return -1;
	}

	if (retlen == 0) {
		printk(KERN_EMERG "crashdump_block_write wrote 0 bytes\n");
		return -1;
	}

	if (retlen != len) {
		printk(KERN_EMERG "crashdump_block_write retlen 0x%08x != len 0x%08x\n", retlen, len);
	}

	dprintk2(KERN_EMERG "(%s:%s:%d) 0x%08x bytes written to 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, retlen, (unsigned int)crashdump_off);

	crashdump_off += retlen;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH
static int crashdump_writedo(char *abuf, int alen)
{
	int retval;

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	retval = -1;

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
	retval = crashdump_writedo_mtd(abuf, alen);
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BML_BLKDEV
	retval = crashdump_writedo_bml_blkdev(abuf, alen);
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
	retval = crashdump_writedo_blkdev(abuf, alen);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	return retval;
}
#endif

#ifdef CONFIG_CRASHDUMP_TO_CONSOLE
static int crashdump_writedo_console(char *abuf, int alen)
{
	int retval;
	size_t len;
	size_t retlen;
	u_char* buf;

	len = (size_t)alen;
	retlen = 0;
	buf = (u_char*)abuf;

	retval = crashdump_uart_write(buf, len, &retlen);
	if (retval < 0) {
		printk(KERN_EMERG "crashdump_uart_write returned %d\n", retval);
		return -1;
	}

	if (retlen == 0) {
		printk(KERN_EMERG "crashdump_uart_write wrote 0 bytes\n");
		return -1;
	}

	if (retlen != len) {
		printk(KERN_EMERG "crashdump_uart_write retlen 0x%08x != len 0x%08x\n", retlen, len);
	}

	dprintk2(KERN_EMERG "(%s:%s:%d) 0x%08x bytes written to console\n", __FILE__, __FUNCTION__, __LINE__, retlen);

	return 0;
}
#endif

void crashdump_write(char *abuf, int alen)
{
	int retval __attribute__((unused));
	int retval_console __attribute__((unused));
	int len;

	dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

	if ( (abuf == NULL) || (alen <= 0) ) {
		return;
	}

	if (alen < (pagesize - tbflushbuf_index)) {
		dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
		dprintk2(KERN_EMERG "(%s:%s:%d) tbflushbuf, tbflushbuf_index, abuf, alen: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, (unsigned int)tbflushbuf, (unsigned int)tbflushbuf_index, (unsigned int)abuf, (unsigned int)alen);

		memcpy(tbflushbuf + tbflushbuf_index, abuf, alen);
		tbflushbuf_index += alen;
		return;
	}
	else {
		dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
		len = pagesize - tbflushbuf_index;

		memcpy(tbflushbuf + tbflushbuf_index, abuf, len);
		tbflushbuf_index += len;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
		retval = crashdump_writedo(tbflushbuf, pagesize);
#endif

#ifdef CONFIG_CRASHDUMP_TO_CONSOLE
		retval_console = crashdump_writedo_console(tbflushbuf, pagesize);
#endif

		tbflushbuf_index = 0;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
		if (retval < 0) {
			return;
		}
#endif

#ifdef CONFIG_CRASHDUMP_TO_CONSOLE
		if (retval_console < 0) {
			return;
		}
#endif

		dprintk2(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
		crashdump_write(abuf + len, alen - len);
		return;
	}
}

void crashdump_write_bin(char *abuf, int alen)
{
	int i;

	if ( (abuf == NULL) || (alen <= 0) ) {
		return;
	}

	for (i=0; i<alen; i++) {
		crashdump_write_str("%02x", abuf[i]);
	}
}

void crashdump_write_str(const char *fmt, ...)
{
	va_list va;
	int retlen;

	va_start(va, fmt);
	retlen = vsnprintf(strbuf, sizeof(strbuf), fmt, va);
	va_end(va);

	crashdump_write(strbuf, retlen);
}

void crashdump_flush(void)
{
	int retval __attribute__((unused));

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "0x%08x bytes need to be flushed\n", tbflushbuf_index);
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	while (1) {
		dprintk2(KERN_EMERG "(%s:%s:%d) crashdump_off = 0x%08x limit = 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, (unsigned int)crashdump_off, (unsigned int)crashdump_size);

		if ((crashdump_off + pagesize) > crashdump_size) {
			break;
		}

		memset((void *)(tbflushbuf + tbflushbuf_index), '.', (size_t)(pagesize - tbflushbuf_index));

		retval = crashdump_writedo(tbflushbuf, pagesize);
		if (retval < 0) {
			break;
		}

		tbflushbuf_index = 0;
	}

#ifdef CONFIG_CRASHDUMP_TO_FLASH_MTD
	crashdump_part->mtd.sync(&crashdump_part->mtd);
#endif
#endif

#ifdef CONFIG_CRASHDUMP_TO_CONSOLE
	crashdump_writedo_console(tbflushbuf, tbflushbuf_index);
#endif

#ifdef CONFIG_CRASHDUMP_TO_FLASH_BLKDEV
	if (crashdump_part != NULL){
		blkdev_put(crashdump_part, FMODE_READ | FMODE_WRITE);
	}
#endif
}
