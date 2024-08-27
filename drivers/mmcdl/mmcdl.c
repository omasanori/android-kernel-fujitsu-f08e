/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011
/*----------------------------------------------------------------------------*/
// File Name:
//      mmcdl.c
//
// History:
//      2011.06.23  Created.
//
/*----------------------------------------------------------------------------*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pagemap.h>

#include <linux/fs.h>
#include <linux/err.h>
#include <linux/cdev.h>

#include <linux/mutex.h>

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#include <linux/mmc/card.h>     /* FUJITSU:2012-07-05 mc add */

#include <linux/proc_fs.h>

#if 0 /* LA16 kari del */
#include <mach/msm_smsm.h>      /* FUJITSU:2012-06-16 MC kari add */
#endif

#include "mmcdl.h"

#ifdef CONFIG_SECURITY_FJSEC
extern int fjsec_check_mmcdl_access_process(void);
#endif

#define DRIVER_NAME "mmcdl"
#define CLASS_NAME	"mmcdl"
#define DEVICE_NAME "mmcdl"

#define SECTOR_SHIFT	9
#define SECTOR_SIZE		512

//#define nprintk printk
#define nprintk(...)

static unsigned int mmcdl_devs  = 1; /* device count */
static unsigned int mmcdl_major = 0;
static unsigned int mmcdl_minor = 0;
static struct cdev mmcdl_cdev;
static struct class	 *mmcdl_class;

struct mmcdl_device {
	struct mutex lock;
	int partition;
	int initialized;
};

static struct mmcdl_device *mmcdl_device;

#if 0 /* LA16 kari del */
extern struct mmc_cid cid_save;             /* FUJITSU:2012-07-05 mc add */
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* FUJITSU:2012-01-20 add start */

#include <linux/bio.h>
#include <linux/blkdev.h>

static int mmdl_rw_wait_init = 0;
static wait_queue_head_t mmcdl_rw_wait;
static unsigned long mmcdl_rw_wait_flag = 0;

struct mmcdl_bio_info {
    int index;
    struct bio *bio;
    
    struct page *pages;
    int alloc_pages_order;
    int ret;
    unsigned long len;
    unsigned long offset;
}mmcdl_bio_info_t;

static void mmcdl_wait_event(unsigned long mask)
{
    nprintk(KERN_INFO "mmcdl_wait_event [in]\n");
    wait_event(mmcdl_rw_wait, (mask&mmcdl_rw_wait_flag)==mask);
    nprintk(KERN_INFO "mmcdl_wait_event [out]\n");
}

static void mmcdl_blockrw_end_io(struct bio *bio, int err)
{
    struct mmcdl_bio_info *bio_info = (struct mmcdl_bio_info *)bio->bi_private;;
    
    if (! test_bit(BIO_UPTODATE, &bio->bi_flags)) {
        printk(KERN_ERR "mmcdl_blockrw_end_io: I/O error\n");
        bio_info->ret = -1;
    }else{
        bio_info->ret = 0;
    }
    
    bio_put(bio);
    
    nprintk(KERN_INFO "mmcdl_blockrw_end_io[M]\n");
    set_bit(bio_info->index, &mmcdl_rw_wait_flag);
    wake_up(&mmcdl_rw_wait);
}

static void mmcdl_page_cahache_release(struct address_space *mapping, unsigned int aSector, size_t sectornum)
{
    struct page *page;
    pgoff_t index;
    size_t start;
    size_t size;
    uint64_t len;
    
    index = aSector >> (PAGE_SHIFT - SECTOR_SHIFT);
    start = (aSector % (PAGE_SIZE / SECTOR_SIZE)) * SECTOR_SIZE;
    len   = (uint64_t)(sectornum * SECTOR_SIZE);

    do {
        if((start + len) > PAGE_SIZE) {
            size = PAGE_SIZE - start;
        }
        else {
            size = len;
        }
        
        page = find_get_page(mapping, index);
        if (page) {
            page_cache_release(page);
        }
        
        index++;
        start = 0;
        len -= size;
    } while(len > 0);
    
}

static __inline int mmddl_do_bio(int rw, const char *devname, unsigned int aSector, uint8_t *buf, size_t sectornum)
{
    struct block_device *bdev;
    int ret;
    fmode_t mode=FMODE_READ|FMODE_WRITE;
    
    unsigned int bio_max_sectors;
    unsigned int bio_len;
    unsigned int bio_offset;
    unsigned long bio_wait_mask;
    int bio_nr = 0;
    int i;
    struct mmcdl_bio_info bio_info[32];
    struct request_queue *q;
    
    bdev = lookup_bdev(devname);
    if(IS_ERR(bdev)) {
        printk(KERN_ERR "mmddl_do_bio[%s] lookup_bdev fail.", (rw==READ)?"READ":"WRITE");
        return PTR_ERR(bdev);
    }
//    ret = blkdev_get(bdev, mode);
    ret = blkdev_get(bdev, mode, NULL);
    if(ret) {
        printk(KERN_ERR "mmddl_do_bio[%s] blkdev_get fail.", (rw==READ)?"READ":"WRITE");
        return ret;
    }
    
    q = bdev_get_queue(bdev);
    bio_max_sectors = queue_max_sectors(q);
    if (bio_max_sectors == 0) {
        ret = -EIO;
        goto END;
    }
    bio_nr = sectornum/bio_max_sectors;
    if ((sectornum%bio_max_sectors)!=0) {
        bio_nr++;
    }
    if (bio_nr > (sizeof(unsigned long) * 8)) {
        bio_nr = 0;
        ret = -EFBIG;
        goto END;
    }

    bio_offset = 0;
    bio_wait_mask = 0;
    ret = 0;
    memset(bio_info, 0, sizeof(bio_info));
    
    for( i = 0; i < bio_nr; i++)
    {
        if ((sectornum - bio_offset) > bio_max_sectors) {
            bio_len = bio_max_sectors << SECTOR_SHIFT;
        } else{
            bio_len = (sectornum - bio_offset) << SECTOR_SHIFT;
        }
        bio_info[i].len = bio_len;
        bio_info[i].offset = bio_offset;
        
        bio_info[i].alloc_pages_order = get_order(bio_len);
        
        if (unlikely(bio_info[i].alloc_pages_order >= MAX_ORDER)) {
            printk(KERN_ERR "alloc_pages_order over MAX_ORDER order=%d max=%d \n", bio_info[i].alloc_pages_order,MAX_ORDER);
            ret = -EFBIG;
            break;
        }
        
        bio_info[i].pages = alloc_pages(GFP_KERNEL, bio_info[i].alloc_pages_order);
        if (!bio_info[i].pages) {
            printk(KERN_ERR "mmddl_do_bio[%s] alloc_pages fail. order=%d, index=%d, offset=%d", (rw==READ)?"READ":"WRITE", bio_info[i].alloc_pages_order, i, bio_offset);
            ret = -ENOMEM;
            break;
        }
        
        if (rw == WRITE) {
            memcpy(page_address(bio_info[i].pages), buf + (bio_offset<<SECTOR_SHIFT), bio_len);
        }
        
        bio_info[i].index = i;
        bio_info[i].ret = -1;
        
        bio_info[i].bio = bio_alloc(GFP_NOFS, 1);
        if (!bio_info[i].bio) {
            printk(KERN_ERR "mmddl_do_bio[%s] bio_alloc fail. index=%d, ret=%d", (rw==READ)?"READ":"WRITE", bio_info[i].index, bio_info[i].ret);
            ret = -ENOMEM;
            break;
        }
        bio_info[i].bio->bi_bdev    = bdev;
        bio_info[i].bio->bi_sector  = aSector + bio_offset;
        bio_info[i].bio->bi_end_io  = mmcdl_blockrw_end_io;
        bio_info[i].bio->bi_private = &bio_info[i];
        
        clear_bit(i, &mmcdl_rw_wait_flag);

        if (bio_add_page(bio_info[i].bio, bio_info[i].pages, bio_len, 0) < bio_len) {
            printk(KERN_ERR "mmddl_do_bio[%s] bio_add_page fail.", (rw==READ)?"READ":"WRITE");
            bio_put(bio_info[i].bio);
            ret = -EIO;
            break;
        }
        if (!bio_info[i].bio->bi_size) {
            printk(KERN_ERR "mmddl_do_bio[%s] bio->bi_size error.", (rw==READ)?"READ":"WRITE");
            bio_put(bio_info[i].bio);
            ret = -EIO;
            break;
        }
        
        set_bit(i, &bio_wait_mask);
        
        submit_bio(rw, bio_info[i].bio);
        
        bio_offset += (bio_len >> SECTOR_SHIFT);
    }
    
    if (bio_wait_mask) {
        mmcdl_wait_event(bio_wait_mask);
        
        for ( i = 0; i < bio_nr; i++){
            if (((1<<i)&bio_wait_mask)!=0) {
                if (bio_info[i].ret != 0) {
                    printk(KERN_ERR "mmddl_do_bio[%s] submit_bio fail. index=%d", (rw==READ)?"READ":"WRITE", i);
                    ret = -EIO;
                    break;
                }
                if (rw == READ) {
                    memcpy(buf + (bio_info[i].offset<<SECTOR_SHIFT), page_address(bio_info[i].pages), bio_info[i].len);
                }
            }
        }
    }
    
END:
    if (rw == WRITE || ret != 0) { 
        mmcdl_page_cahache_release(bdev->bd_inode->i_mapping, aSector, sectornum);
    }
    
    for ( i = 0; i < bio_nr; i++) {
        if (bio_info[i].pages) {
            __free_pages(bio_info[i].pages, bio_info[i].alloc_pages_order);
        }
    }
    
    if (rw == WRITE) {
        sync_blockdev(bdev);
    }
    
    blkdev_put(bdev, mode);
    
    return ret;
}
/* FUJITSU:2012-01-20 add end */

static int block_read(const char *devname, unsigned int aSector, uint8_t *buf, size_t sectornum)
{
#if 1 /* FUJITSU:2012-01-20 add */
    
    return mmddl_do_bio(READ, devname, aSector, buf, sectornum);
    
#else /* FUJITSU:2012-01-20 del start */
	struct block_device *bdev;
	pgoff_t index;
	size_t start;
	size_t size;
	uint64_t len;
	struct page *page;
	int ret;
	fmode_t mode=FMODE_READ;

	bdev = lookup_bdev(devname);
	if(IS_ERR(bdev)) {
		return PTR_ERR(bdev);
	}
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-s */
//	ret = blkdev_get(bdev, mode);
	ret = blkdev_get(bdev, mode, NULL);
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-e */

	if(ret) {
		return ret;
	}

	index = aSector >> (PAGE_SHIFT - SECTOR_SHIFT);
	start = (aSector % (PAGE_SIZE / SECTOR_SIZE)) * SECTOR_SIZE;
	len   = (uint64_t)(sectornum * SECTOR_SIZE);

	do {
		if((start + len) > PAGE_SIZE) {
			size = PAGE_SIZE - start;
		}
		else {
			size = len;
		}

		page = read_mapping_page(bdev->bd_inode->i_mapping, index, NULL);
		if(page == NULL) {
			ret = ENOMEM;
			break;
		}
		if(IS_ERR(page)) {
			ret = PTR_ERR(page);
			break;
		}

		memcpy(buf, page_address(page)+start, size);
		page_cache_release(page);

		index++;
		start = 0;
		buf += size;
		len -= size;
	} while(len > 0);

	blkdev_put(bdev, mode);

	return ret;
#endif /* FUJITSU:2012-01-20 del end */
}


//static int block_write(const char *devname, unsigned int aSector, const uint8_t *buf, size_t sectornum)
static int block_write(const char *devname, unsigned int aSector, uint8_t *buf, size_t sectornum)
{
#if 1 /* FUJITSU:2012-01-20 add */
    
    return mmddl_do_bio(WRITE, devname, aSector, buf, sectornum);
    
#else /* FUJITSU:2012-01-20 del start */
	struct block_device *bdev;
	pgoff_t index;
	size_t start;
	size_t size;
	uint64_t len;
	struct page *page;
	int ret;
	fmode_t mode=FMODE_READ|FMODE_WRITE;

	bdev = lookup_bdev(devname);
	if(IS_ERR(bdev)) {
		return PTR_ERR(bdev);
	}
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-s */
//	ret = blkdev_get(bdev, mode);
	ret = blkdev_get(bdev, mode, NULL);
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-e */
	if(ret) {
		return ret;
	}

	index = aSector >> (PAGE_SHIFT - SECTOR_SHIFT);
	start = (aSector % (PAGE_SIZE / SECTOR_SIZE)) * SECTOR_SIZE;
	len   = (uint64_t)(sectornum * SECTOR_SIZE);

	do {
		if((start + len) > PAGE_SIZE) {
			size = PAGE_SIZE - start;
		}
		else {
			size = len;
		}

		page = read_mapping_page(bdev->bd_inode->i_mapping, index, NULL);
		if(page == NULL) {
			ret = ENOMEM;
			break;
		}
		if(IS_ERR(page)) {
			ret = PTR_ERR(page);
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
	blkdev_put(bdev, mode);

	return ret;
#endif /* FUJITSU:2012-01-20 del end */
}

int ReadMMCDL(	unsigned int aSector,
				uint8_t* aBuff,
				loff_t   aSize )
{
	int ret = 0;

	nprintk(KERN_INFO "[IN ] ReadMMCDL() sector=%d  size=%lld\n", aSector, aSize);

//	nprintk(KERN_INFO "  sector=%d  size=%lld\n", aSector, aSize);

	ret = block_read(MMCDL_BLOCK_DEV,
	                 aSector,
	                 aBuff,
	                 aSize );

	return ret;
}

int WriteMMCDL( unsigned int aSector,
				uint8_t* aBuff,
				loff_t   aSize )
{
	int ret = 0;

	nprintk(KERN_INFO "[IN ] WriteMMCDL() sector=%d  size=%lld\n", aSector, aSize);

//	nprintk(KERN_INFO "  sector=%d  size=%lld\n", aSector, aSize);

	ret = block_write(MMCDL_BLOCK_DEV,
	                  aSector,
	                  aBuff,
	                  aSize );

	return ret;
}


static int Init_Setup(void)
{
	return 0;
}

static int Initialize(void)
{
	int ret = 0;

	ret = Init_Setup();

	return ret;
}
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-s */
//static int mmcdl_ioctl(struct inode *inode, struct file *filp, unsigned cmd, unsigned long arg)
//{

static long mmcdl_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-e */
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-s */
//	int ret = 0;
	long ret = 0;
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-e */
	
	mmcdl_data_t data;
	char *workBuff = NULL;

/* FUJITSU:2011-12-13 MSM8960 SDDL ADD-S */
	fmode_t mode=FMODE_READ|FMODE_WRITE;
	unsigned long	all_sector_size;
	struct block_device *bdev;
/* FUJITSU:2011-12-13 MSM8960 SDDL ADD-E */
#if 0 /* LA16 kari del */
/* TESTMODE20120116 ADD-S */
    uint8_t emmc_info[7];
    uint8_t *manfid;
/* TESTMODE20120116 ADD-E */
/* FUJITSU:2012-06-16 MC kari add */
    uint8_t *smem_p;
    uint8_t raminfo = 0xFF;
/* FUJITSU:2012-06-16 MC kari add */
#endif
	
/* FUJITSU:2011-12-13 MSM8960 SDDL ADD-S */
	if( cmd != IOCTL_MMCDLGETSIZE){                                       /* TESTMODE20120116- */
#if 0 /* LA16 kari del */
//	if(( cmd != IOCTL_MMCDLGETSIZE)&&( cmd != IOCTL_MMCDLGETINFO)){       /* TESTMODE20120116+ */
//	if(( cmd != IOCTL_MMCDLGETSIZE)&&( cmd != IOCTL_MMCDLGETINFO)&&( cmd != IOCTL_RAMGETINFO)){       /* kari */
/* FUJITSU:2011-12-13 MSM8960 SDDL ADD-E */
#endif

		if (copy_from_user(&data, (mmcdl_data_t *)arg, sizeof(mmcdl_data_t)) != 0) {
			nprintk(KERN_INFO "ERROR(arg) copy_from_user() \n");
			return -EFAULT;
		}
	
		workBuff = kzalloc((data.iSize * SECTOR_SIZE), GFP_KERNEL);
		if (workBuff == NULL) {
			nprintk(KERN_INFO "ERROR(ioctl) kzalloc()\n");
			return -EFAULT;
		}
	
//		if (copy_from_user(workBuff, data.iBuff, (data.iSize * SECTOR_SIZE)) != 0) {
//			nprintk(KERN_INFO "ERROR(data.iBuff) copy_from_user() \n");
//			ret =  -EFAULT;
//			goto fail;
//		}
/* FUJITSU:2011-12-13 MSM8960 SDDL ADD-S */
	}
/* FUJITSU:2011-12-13 MSM8960 SDDL ADD-E */
	

	switch (cmd) {
	case IOCTL_MMCDLWRITE:
		if (copy_from_user(workBuff, data.iBuff, (data.iSize * SECTOR_SIZE)) != 0) {
			nprintk(KERN_INFO "ERROR(data.iBuff) copy_from_user() \n");
			ret =  -EFAULT;
			break;
		}
		ret = WriteMMCDL(data.iSector, workBuff, data.iSize);
		break;

	case IOCTL_MMCDLREAD:
		ret = ReadMMCDL(data.iSector, workBuff, data.iSize);
		if (ret == 0) {
			if (copy_to_user(data.iBuff, workBuff, (data.iSize * SECTOR_SIZE)) != 0){
				nprintk(KERN_INFO "ERROR(data.iBuff) copy_to_user() \n");
				ret = -EFAULT;
			} else {
				if (copy_to_user((mmcdl_data_t *)arg, &data, sizeof(mmcdl_data_t)) != 0){
					nprintk(KERN_INFO "ERROR(arg) copy_to_user() \n");
					ret = -EFAULT;
				}
			}
		} else {
			ret = -EFAULT;
		}
		break;
		
/* FUJITSU:2011-12-13 MSM8960 SDDL ADD-S */
	case IOCTL_MMCDLGETSIZE:

		bdev = lookup_bdev(MMCDL_BLOCK_DEV);
		if(IS_ERR(bdev)) {
			return PTR_ERR(bdev);
		}

		ret = blkdev_get(bdev, mode, NULL);
		if(ret) {
			return ret;
		}

		all_sector_size = (unsigned long)(bdev->bd_inode->i_size/512);
		printk(KERN_INFO "all_sector_size = %ld \n", all_sector_size);

		if (copy_to_user( (unsigned long *) arg, &all_sector_size, sizeof(all_sector_size)) != 0){
			nprintk(KERN_INFO "ERROR(arg) copy_to_user() \n");
			ret = -EFAULT;
		}
		
		break;
/* FUJITSU:2011-12-13 MSM8960 SDDL ADD-E */

#if 0 /* LA16 kari del */
/* TESTMODE20120116 ADD-S */
	case IOCTL_MMCDLGETINFO:
        manfid = (uint8_t *)&cid_save.manfid;
	    emmc_info[0] = *(manfid+0);
	    memcpy(&emmc_info[1],&cid_save.prod_name[0],6);

		if (copy_to_user( (unsigned long *) arg, &emmc_info[0], 7) != 0){
			nprintk(KERN_INFO "ERROR(arg) copy_to_user() \n");
			ret = -EFAULT;
		}
		
		break;
/* TESTMODE20120116 ADD-E */
/* FUJITSU:2012-06-16 MC kari add */
	case IOCTL_RAMGETINFO:
		smem_p = (uint8_t*)smem_alloc_vendor0(SMEM_OEM_V0_003);
		if (smem_p == NULL) {
			ret = -EFAULT;
			break;
		}
		raminfo = *smem_p;

		if (copy_to_user((void *)arg, &raminfo, 1) != 0){
			nprintk(KERN_INFO "ERROR(arg) copy_to_user() \n");
			ret = -EFAULT;
		}

		break;
/* FUJITSU:2012-06-16 MC kari add */
#endif

	default:
		ret = -ENOTTY;
		break;
	}

//fail:
	if (workBuff != NULL) {
		kfree(workBuff);
	}

	return ret;
}

static int mmcdl_open(struct inode *inode, struct file *filp)
{
	int ret=0;

#ifdef CONFIG_SECURITY_FJSEC
	if(fjsec_check_mmcdl_access_process()) {
		return -EPERM;
	}
#endif

#ifdef INIT_READ_MMC
	if(!mmcdl_device->initialized) {
		ret = Initialize();
		if(ret == 0) {
			mmcdl_device->initialized = 1;
		}
	}
#endif
	mmcdl_device->partition = 0;

	return ret;
}

static int mmcdl_close(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations mmcdl_fops = {
	.owner   = THIS_MODULE,
	.open    = mmcdl_open,
	.release = mmcdl_close,
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-s */
//	.ioctl   = mmcdl_ioctl,
    .unlocked_ioctl   = mmcdl_ioctl,
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-e */

};

/*----------------------------------------------------------------------*/
/* procfs entry function                                                */
/*----------------------------------------------------------------------*/
/*!
 @brief mmcdl_write

 /proc/driver/mmcdl write process

 @param [in]  filp   pointer of the file strucure
 @param [in]  ptr    pointer of the write data
 @param [in]  len    length of the write data
 @param [in]  data   user data

 @retval -ENOSPC:    no left memory space
 @retval -EFAULT:    system error
 @retval -EINVAL:    parameter error
 @retval      >0:    success

 @note this function is called when data write to /proc/driver/mmcdl.
*/
static int mmcdl_write(struct file *filp, const char *ptr, unsigned long len, void *data)
{
    char buf[255];

    if(len >= sizeof(buf)) {
        printk(KERN_INFO "mmcdl_write, -ENOSPC");
        return -ENOSPC;
    }
    if(copy_from_user(buf,ptr,len)) {
        printk(KERN_INFO "mmcdl_write, -EFAULT");
        return -EFAULT;
    }

    buf[len] = 0;
    printk(KERN_INFO "%s",buf);

    return len;
} /* mmcdl_write */


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static int mmcdl_drv_probe(struct platform_device *pdev)
{
	dev_t dev = MKDEV(0, 0);	// 
	int ret;
	struct device *class_dev = NULL;

	nprintk(KERN_INFO "*** mmcdl probe ***\n");

	mmcdl_device = kzalloc(sizeof(struct mmcdl_device), GFP_KERNEL);
	if (mmcdl_device == NULL) {
		goto error;
	}

    // 
    ret = alloc_chrdev_region(&dev, 0, mmcdl_devs, DRIVER_NAME);
	if (ret) {
		goto error;
	}

	mmcdl_major = MAJOR(dev);

	// 
    cdev_init(&mmcdl_cdev, &mmcdl_fops);

	mmcdl_cdev.owner = THIS_MODULE;
	mmcdl_cdev.ops   = &mmcdl_fops;

	ret = cdev_add(&mmcdl_cdev, MKDEV(mmcdl_major, mmcdl_minor), 1);
	if (ret) {
		goto error;
	}

	// register class
	mmcdl_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mmcdl_class)) {
		goto error;
	}

	// register class device
	class_dev = device_create(
					mmcdl_class,
					NULL,
					MKDEV(mmcdl_major, mmcdl_minor),
					NULL,
					"%s",
					DEVICE_NAME);

	mutex_init(&mmcdl_device->lock);

/* FUJITSU:2012-01-20 add start */
    if(mmdl_rw_wait_init==0)
    {
        init_waitqueue_head(&mmcdl_rw_wait);
        mmdl_rw_wait_init = 1;
    }
/* FUJITSU:2012-01-20 add end */

#ifdef INIT_READ_MMC
	mmcdl_device->initialized = 0;
#else
	ret = Initialize();
	if (ret) {
		goto error;
	}
#endif
	nprintk(KERN_INFO "(%s:%d)\n", __FUNCTION__, __LINE__);
	return 0;

error:
	nprintk(KERN_INFO "(%s:%d)\n", __FUNCTION__, __LINE__);
	return -1;
}

static int mmcdl_drv_remove(struct platform_device *pdev)
{
	// 
	kfree(mmcdl_device);

	nprintk(KERN_INFO "(%s:%d)\n", __FUNCTION__, __LINE__);
	return 0;
}

static struct platform_device mmcdl_devices = {
	.name = DEVICE_NAME,
	.id   = -1,
	.dev = {
		.dma_mask          = NULL,
		.coherent_dma_mask = 0xffffffff,
	},
};

static struct platform_device *devices[] __initdata = {
	&mmcdl_devices,
};

static struct platform_driver mmcdl_driver = {
	.probe    = mmcdl_drv_probe,
	.remove   = mmcdl_drv_remove,
	.driver   = {
		.name = DRIVER_NAME,
	},
};

/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-s */
//static int __devinit mmcdl_init(void)
//{
static int __init mmcdl_init(void)
{
/* FUJITSU:2011-11-15 MSM8960 SDDL  mod-e */

#define MMCDL_PROC "driver/mmcdl"

    struct proc_dir_entry *entry;

    entry = create_proc_entry( MMCDL_PROC, 0660, NULL );
    if ( entry == NULL) {
        printk( KERN_ERR "create_mmcdl_proc_entry failed\n" );
        return -EBUSY;
    }
    entry->write_proc = mmcdl_write;
    
    platform_add_devices(devices, ARRAY_SIZE(devices));

	return platform_driver_register(&mmcdl_driver);
}

static void __exit mmcdl_exit(void)
{
	platform_driver_unregister(&mmcdl_driver);
}

module_init(mmcdl_init);
module_exit(mmcdl_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("mmcdl device");
MODULE_LICENSE("GPL");

