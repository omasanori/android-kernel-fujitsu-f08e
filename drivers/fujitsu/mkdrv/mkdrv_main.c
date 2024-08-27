/*
 * mkdrv_main.c
 *
 * Copyright(C) 2012 - 2013 FUJITSU LIMITED
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

//==============================================================================
// include file
//==============================================================================
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
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
#include <asm/uaccess.h>

#include "mkdrv_common.h"
#include "mkdrv_regulator.h"
#include "mkdrv_regs.h"
#include "mkdrv_sys.h"

//==============================================================================
// define
//==============================================================================
#define DRIVER_NAME "mkdrv"
#define CLASS_NAME	"mkdrv"
#define DEVICE_NAME "mkdrv"

//==============================================================================
// const data
//==============================================================================
static unsigned int mkdrv_devs  = 1;
static unsigned int mkdrv_major = 0;
static unsigned int mkdrv_minor = 0;
static struct cdev mkdrv_cdev;
static struct class	 *mkdrv_class;
static DEFINE_MUTEX(mkdrv_mutex);

//==============================================================================
// global valuable
//==============================================================================
#ifdef CONFIG_SECURITY_FJSEC
extern int fjsec_check_mkdrv_access_process(void);
#endif
//==============================================================================
// static functions prototype
//==============================================================================

//==============================================================================
// functions
//==============================================================================
/**
 * mkdrv_open
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
static int mkdrv_open(struct inode *inode, struct file *filp)
{
#ifdef CONFIG_SECURITY_FJSEC
	if(fjsec_check_mkdrv_access_process()) {
		return -EPERM;
	}
#endif
	return 0;
}
/**
 * mkdrv_close
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
static int mkdrv_close(struct inode *inode, struct file *filp)
{
	return 0;
}
/**
 * mkdrv_ioctl
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int mkdrv_ioctl(struct inode *inode, struct file *filp, unsigned cmd, unsigned long arg)
{
	int ret = 0;
#else  /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36) */
static long mkdrv_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
	long ret = 0;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36) */
	mkdrv_func_data_t data;

	mutex_lock(&mkdrv_mutex);

	switch (cmd) {
	case IOCTL_MKDRV_FUNCTION_EXE:
		if (copy_from_user(&data, (mkdrv_func_data_t *)arg, sizeof(mkdrv_func_data_t)) != 0) {
			printk(KERN_ERR "ERROR(arg) copy_from_user() \n");
			ret = -EFAULT;
			break;
		}
		switch(data.iFuncno){
		case 1:
			ret = mkdrv_regulator_switch(&data);
			break;
		case 2:
			ret = mkdrv_regulator_trim(&data);
			break;
		case 3:
			ret = mkdrv_reg_read(&data);
			break;
		case 4:
			ret = mkdrv_reg_write(&data);
			break;
			
		case 5:
			ret = mkdrv_raminfo_get(&data);
			break;
		default:
			printk(KERN_ERR "Unknown Function Number received.\n");
			ret = -1;
			break;
		}
		if(ret == 0){
			if (copy_to_user((mkdrv_func_data_t *)arg, &data, sizeof(mkdrv_func_data_t)) != 0){
				printk(KERN_ERR "ERROR(arg) copy_to_user() \n");
				ret = -EFAULT;
				break;
			}
		}
		break;
	default:
		printk(KERN_ERR "Unknown command received.\n");
		ret = -1;
		break;
	}
	mutex_unlock(&mkdrv_mutex);

	return ret;
}
struct file_operations mkdrv_fops = {
	.owner   = THIS_MODULE,
	.open    = mkdrv_open,
	.release = mkdrv_close,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	.ioctl		= mkdrv_ioctl,
#else  /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36) */
	.unlocked_ioctl   = mkdrv_ioctl,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36) */
};
/**
 * mkdrv_drv_probe
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
static int mkdrv_drv_probe(struct platform_device *pdev)
{
	dev_t dev = MKDEV(0, 0);
	int ret;
	struct device *class_dev = NULL;

    ret = alloc_chrdev_region(&dev, 0, mkdrv_devs, DRIVER_NAME);
	if (ret) {
		goto error;
	}

	mkdrv_major = MAJOR(dev);
    cdev_init(&mkdrv_cdev, &mkdrv_fops);

	mkdrv_cdev.owner = THIS_MODULE;
	mkdrv_cdev.ops   = &mkdrv_fops;

	ret = cdev_add(&mkdrv_cdev, MKDEV(mkdrv_major, mkdrv_minor), 1);
	if (ret) {
		goto error;
	}

	// register class
	mkdrv_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(mkdrv_class)) {
		goto error;
	}

	// register class device
	class_dev = device_create(
					mkdrv_class,
					NULL,
					MKDEV(mkdrv_major, mkdrv_minor),
					NULL,
					"%s",
					DEVICE_NAME);

	// device init
	mkdrv_regulator_init();
	printk(KERN_INFO "%s sucess\n", __func__);
	return 0;

error:
	printk(KERN_ERR "%s error\n", __func__);
	return -1;
}
/**
 * mkdrv_drv_remove
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
static int mkdrv_drv_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "(%s:%d)\n", __FUNCTION__, __LINE__);
	return 0;
}

static struct platform_device mkdrv_devices = {
	.name = DEVICE_NAME,
	.id   = -1,
	.dev = {
		.dma_mask          = NULL,
		.coherent_dma_mask = 0xffffffff,
	},
};

static struct platform_device *devices[] __initdata = {
	&mkdrv_devices,
};

static struct platform_driver mkdrv_driver = {
	.probe    = mkdrv_drv_probe,
	.remove   = mkdrv_drv_remove,
	.driver   = {
		.name = DRIVER_NAME,
	},
};
/**
 * mkdrv_init
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
static int __init mkdrv_init(void)
{
	int ret = 0;

	//makermode only
	platform_add_devices(devices, ARRAY_SIZE(devices));
	ret = platform_driver_register(&mkdrv_driver);
	return ret;
}
/**
 * mkdrv_exit
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
static void __exit mkdrv_exit(void)
{
	platform_driver_unregister(&mkdrv_driver);
}
module_init(mkdrv_init);
module_exit(mkdrv_exit);

MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("mkdrv device");
MODULE_LICENSE("GPL");
