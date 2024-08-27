/*
 * linux/drivers/felica/felica_pon.c
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
/*------------------------------------------------------------------------
 * Copyright(C) 2012-2013 FUJITSU LIMITED
 *------------------------------------------------------------------------
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include "../../arch/arm/mach-msm/devices.h"

#include "felica_hw.h"
#include "felica_ioctl.h"

#include <linux/syscalls.h>

#define DEV_NAME "felica_pon"
#define CLASS_NAME "felica_pon"
#define LOCAL_BUFLEN 4

static struct cdev char_dev;
static dev_t dev;
static struct class	 *dev_class;

extern int makercmd_mode;

static int open_count = 0;

//// SW //////////////////////////////////////////////////////
static int felica_pon_sw_init(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_init() [IN]\n");
#endif
	return felica_pon_hw_init();
}

static int felica_pon_sw_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_exit() [IN]\n");
#endif
	return felica_pon_hw_exit();
}

static int felica_pon_sw_open(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_open() [IN]\n");
#endif
	return 0;
}

static int felica_pon_sw_close(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_close() [IN]\n");
#endif
	return felica_pon_hw_close();
}

static int felica_pon_sw_write(char val)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_write() [IN]\n");
#endif
	if (val == 0) {
		printk(KERN_INFO DEV_NAME":write() set 0\n");
		return felica_pon_hw_write(FELICA_POWER_OFF);
	} else if (val == 1) {
		printk(KERN_INFO DEV_NAME":write() set 1\n");
		return felica_pon_hw_write(FELICA_POWER_ON);
	} else {
		printk(KERN_ERR DEV_NAME":write() parameter err2\n");
		return -EINVAL;
	}
}

//// IF //////////////////////////////////////////////////////
static int felica_pon_if_init(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_init() [IN]\n");
#endif
	return felica_pon_sw_init();
}

static int felica_pon_if_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_exit() [IN]\n");
#endif
	return felica_pon_sw_exit();
}

static int felica_pon_if_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
#ifdef FELICA_UID_CHECK
	long uid_chk;
#endif

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_open() [IN]\n");
#endif

#ifdef FELICA_UID_CHECK
		if (likely(makercmd_mode != WKUP_MODE_MC)) {
			uid_chk = sys_getuid();

			if (unlikely(uid_chk != FCCTL_UID2)) {
				printk(KERN_ERR DEV_NAME ":open() NG UID = 0x%lx\n", uid_chk);
				return -EIO;
			}
		} else {
			uid_chk = sys_getuid();

			if (unlikely(uid_chk != FCCTL_UID6)) {
				printk(KERN_ERR DEV_NAME ":open() NG MC UID = 0x%lx\n", uid_chk);
				return -EIO;
			}
		}
#else
		printk(KERN_INFO DEV_NAME ":open() UID no check\n");
#endif
		ret = felica_pon_sw_open();
#ifdef FELICA_DEBUG
		if (unlikely(ret)) {
			return ret;
		}
#endif


	open_count++;

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":if_open() OK open_count = %d\n", open_count);
#endif
	return 0;
}

static int felica_pon_if_close(struct inode *inode, struct file *filp)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_close() [IN]\n");
#endif
	if (likely(open_count == 1)) {
		ret = felica_pon_sw_close();
#ifdef FELICA_DEBUG
		if (unlikely(ret)) {
			return ret;
		}
#endif
	} else if (unlikely(open_count <= 0)) {
		printk(KERN_INFO DEV_NAME ":invalid close\n");
		return 0;
	}

	open_count--;

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":if_close() OK open_count = %d\n", open_count);
#endif
	return 0;
}

static int felica_pon_if_write(
	struct file *filp, const char __user *data, size_t len, loff_t *pos)
{
	int ret = 0;
	char localbuf[LOCAL_BUFLEN];

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_write() [IN]\n");
#endif
	if (unlikely((len < 1) || (data == NULL))) {
		printk(KERN_ERR DEV_NAME":write() parameter err1\n");
		return -EINVAL;
	}

	if (unlikely(copy_from_user(localbuf, data, 1))) {
		printk(KERN_ERR DEV_NAME ":copy_from_user is fault.\n");
		return -EFAULT;
	}

	ret = felica_pon_sw_write(localbuf[0]);
	if (unlikely(ret)) {
		return ret;
	}
#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":if_write() OK\n");
#endif
	return 1;
}

//// Init //////////////////////////////////////////////////////
static struct file_operations driver_fops = {
	.open 		= felica_pon_if_open,
	.write		= felica_pon_if_write,
	.release 	= felica_pon_if_close,
};

static int __devinit felica_pon_probe(struct platform_device *pdev)
{
	struct device *class_dev = NULL;
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":probe() [IN]\n");
#endif
	ret = alloc_chrdev_region(&dev, 0, 1, DEV_NAME);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR DEV_NAME ":can't alloc major.\n");
		return ret;
	}

	cdev_init(&char_dev, &driver_fops);
	char_dev.owner = THIS_MODULE;
	ret = cdev_add(&char_dev, dev, 1);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR DEV_NAME ":Major no. cannot be assigned.\n");
		goto fail_cdev_add;
	}

	dev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (unlikely(IS_ERR(dev_class))) {
		printk(KERN_ERR DEV_NAME ":cannot be class_created.\n");
		ret = -EINVAL;
		goto fail_class_create;
	}

	class_dev = device_create(
					dev_class,
					NULL,
					dev,
					NULL,
					DEV_NAME);
	if (unlikely(IS_ERR(class_dev))) {
		printk(KERN_ERR DEV_NAME ":can't create device\n");
		ret = PTR_ERR(class_dev);
		goto fail_dev_create;
	}

	ret = felica_pon_if_init();
	if (unlikely(ret)) {
		goto fail_if_init;
	}

	open_count = 0;

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":probe() OK\n");
#endif
	return 0;

fail_if_init:
	device_destroy(dev_class, dev);
fail_dev_create:
	class_destroy(dev_class);
fail_class_create:
	cdev_del(&char_dev);
fail_cdev_add:
	unregister_chrdev_region(dev, 1);
	return ret;
}

static int __devexit felica_pon_remove(struct platform_device *pdev)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":remove() [IN]\n");
#endif
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&char_dev);
	unregister_chrdev_region(dev, 1);

	ret = felica_pon_if_exit();
#ifdef FELICA_DEBUG
	if (unlikely(ret)) {
		return ret;
	}
	printk(KERN_INFO DEV_NAME ":remove() OK\n");
#endif
	return 0;
}

static struct platform_device pon_device = {
	.name           = DEV_NAME,
	.id             = -1,
};
static struct platform_driver pon_driver = {
	.driver.name	= DEV_NAME,
	.driver.owner	= THIS_MODULE,
	.probe			= felica_pon_probe,
	.remove			= __devexit_p(felica_pon_remove),
};

static int felica_pon_init(void)
{
	int ret = 0;
	static struct regulator *vreg_s4 = NULL;
/* FUJITSU:2013-04-25 ADD For FeliCa Start */
	static struct regulator *vreg_l17 = NULL;
/* FUJITSU:2013-04-25 ADD For FeliCa End */

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":init() [IN]\n");
#endif
	ret = platform_device_register(&pon_device);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME ":platform_device_register error[%d]\n", ret);
		return ret;
	}

	ret = platform_driver_register(&pon_driver);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME ":platform_driver_register error[%d]\n", ret);
		goto fail_device_unreg;
	}

	vreg_s4 = regulator_get(&apq8064_device_uart_gsbi3.dev, "felica_s4");
	if (unlikely(!vreg_s4 || IS_ERR(vreg_s4))) {
		ret = PTR_ERR(vreg_s4);
		vreg_s4 = NULL;
		printk(KERN_ERR DEV_NAME ":could not get felica s4, ret=%d\n",ret);
		ret = -EIO;
		goto fail_driver_unreg;
	}

	ret = regulator_set_voltage(vreg_s4, 1800000, 1800000);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME ":set_voltage felica UART failed, ret=%d\n", ret);
		ret = -EIO;
		goto fail_driver_unreg;
	}

	ret = regulator_set_optimum_mode(vreg_s4, 100000);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR DEV_NAME ":set_optimum_mode felica UART failed, ret=%d\n", ret);
		ret = -EIO;
		goto fail_driver_unreg;
	}

	ret = regulator_enable(vreg_s4);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME ":enable felica UART failed, ret=%d\n", ret);
		ret = -EIO;
		goto fail_driver_unreg;
	}
/* FUJITSU:2013-04-25 ADD For FeliCa Start */
	vreg_l17 = regulator_get( &apq8064_device_uart_gsbi3.dev, "felica_l17");
	if( !vreg_l17 || IS_ERR(vreg_l17) ) {
		ret = PTR_ERR(vreg_l17);
		vreg_l17 = NULL;
		printk(KERN_ERR DEV_NAME ":could not get  felica l17 ret = %d\n", ret);
		goto fail_driver_unreg;
	}
	
	ret = regulator_set_voltage( vreg_l17, 2850000, 2850000 );
		if( ret ) {
		printk(KERN_ERR DEV_NAME ":felica failed to set voltage. ret=%d\n", ret);
		goto fail_driver_unreg;
	}

	ret = regulator_set_optimum_mode( vreg_l17, 300000 );
	if( ret < 0 ) {
		printk(KERN_ERR DEV_NAME ":felica failed to set voltage. ret=%d\n", ret);
		goto fail_driver_unreg;
	}

	ret = regulator_enable( vreg_l17 );
	if( ret < 0 ) {
		printk(KERN_ERR DEV_NAME ":felica failed to set voltage. ret=%d\n", ret);
		goto fail_driver_unreg;
	}
/* FUJITSU:2013-04-25 ADD For FeliCa End */
#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":init() OK\n");
#endif
	return 0;

fail_driver_unreg:
	platform_driver_unregister(&pon_driver);
fail_device_unreg:
	platform_device_unregister(&pon_device);
	return ret;
}

static void felica_pon_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":exit() [IN]\n");
#endif
	platform_driver_unregister(&pon_driver);
	platform_device_unregister(&pon_device);
#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":exit() OK\n");
#endif
}

module_init(felica_pon_init);
module_exit(felica_pon_exit);

MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("Felica PON Port Control Driver");
MODULE_LICENSE("GPL");
