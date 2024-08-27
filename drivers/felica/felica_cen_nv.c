/*
 * linux/drivers/felica/felica_cen_nv.c
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
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/system_info.h>

#include "felica_hw.h"
#include "felica_ioctl.h"

#include <linux/syscalls.h>

#define DEV_NAME "felica_cen_nv"
#define CLASS_NAME "felica_cen_nv"
#define LOCAL_BUFLEN 4

static struct cdev char_dev;
static dev_t  dev;
static struct class     *dev_class;

extern unsigned int system_rev;
extern int makercmd_mode;

static int open_count = 0;

//// SW //////////////////////////////////////////////////////
static int felica_cen_nv_sw_init(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_init() [IN]\n");
#endif
	return felica_cen_nv_hw_init();
}

static int felica_cen_nv_sw_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_exit() [IN]\n");
#endif
	return felica_cen_nv_hw_exit();
}

static int felica_cen_nv_sw_open(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_open() [IN]\n");
#endif
	return 0;
}

static int felica_cen_nv_sw_close(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_close() [IN]\n");
#endif
	return 0;
}

static int felica_cen_nv_sw_write(char val)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_write() [IN]\n");
#endif
	if (val == 0) {
		printk(KERN_INFO DEV_NAME":write() set 0\n");
		return felica_cen_nv_hw_write(FELICA_LOCK);
	} else if (val == 1) {
		printk(KERN_INFO DEV_NAME":write() set 1\n");
		return felica_cen_nv_hw_write(FELICA_UNLOCK);
	} else {
		printk(KERN_ERR DEV_NAME":write() parameter err2\n");
		return -EINVAL;
	}
}

static int felica_cen_nv_sw_read(uint8_t *val)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_read() [IN]\n");
#endif
	return felica_cen_nv_hw_read(val);
}

//// IF //////////////////////////////////////////////////////
static int felica_cen_nv_if_init(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_init() [IN]\n");
#endif
	return felica_cen_nv_sw_init();
}

static int felica_cen_nv_if_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_exit() [IN]\n");
#endif
	return felica_cen_nv_sw_exit();
}

static int felica_cen_nv_if_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_open() [IN]\n");
#endif
	if (likely(open_count == 0)) {
		ret = felica_cen_nv_sw_open();
#ifdef FELICA_DEBUG
		if (unlikely(ret)) {
			return ret;
		}
#endif
	}

	open_count++;

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":if_open() OK open_count = %d\n", open_count);
#endif
	return 0;
}

static int felica_cen_nv_if_close(struct inode *inode, struct file *filp)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_close() [IN]\n");
#endif
	if (likely(open_count == 1)) {
		ret = felica_cen_nv_sw_close();
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

static int felica_cen_nv_if_write(
	struct file *filp, const char __user *data, size_t len, loff_t *pos)
{
	char	localbuf[LOCAL_BUFLEN];
	int ret = 0;

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

	ret = felica_cen_nv_sw_write(localbuf[0]);
	if (unlikely(ret <= 0)) {
		printk(KERN_ERR DEV_NAME":write() failed[%d]\n", ret);
		return ret;
	}

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":if_write() OK\n");
#endif
	return 1;
}

static ssize_t felica_cen_nv_if_read(
	struct file *filp, char __user *data, size_t len, loff_t *pos)
{
	char	localbuf[LOCAL_BUFLEN];
	int		ret = 0;
	uint8_t	buf;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_read() [IN]\n");
#endif
	if (unlikely((len < 1) || (data == NULL))) {
		printk(KERN_ERR DEV_NAME":read() parameter err1\n");
		return -EINVAL;
	}

	ret = felica_cen_nv_sw_read(&buf);
	if (likely(ret > 0)) {
		if (buf > 0) {
			printk(KERN_INFO DEV_NAME":read() data 1\n");
			localbuf[0] = 1;
		} else if (buf == 0) {
			printk(KERN_INFO DEV_NAME":read() data 0\n");
			localbuf[0] = 0;
		}
	} else {
		printk(KERN_ERR DEV_NAME":read() failed[%d]\n", ret);
		return ret;
	}

	if (unlikely(copy_to_user(data, localbuf, 1))) {
		printk(KERN_ERR DEV_NAME ":copy_to_user is fault.\n");
		return -EFAULT;
	}

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":if_read() OK\n");
#endif
	return 1;
}

//// Init //////////////////////////////////////////////////////
static struct file_operations driver_fops = {
	.open 		= felica_cen_nv_if_open,
	.read		= felica_cen_nv_if_read,
	.write		= felica_cen_nv_if_write,
	.release 	= felica_cen_nv_if_close,
};

static int __devinit felica_cen_nv_probe(struct platform_device *pdev)
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

	ret = felica_cen_nv_if_init();
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

static int __devexit felica_cen_nv_remove(struct platform_device *pdev)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":remove() [IN]\n");
#endif
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&char_dev);
	unregister_chrdev_region(dev, 1);

	ret = felica_cen_nv_if_exit();
#ifdef FELICA_DEBUG
	if (unlikely(ret)) {
		return ret;
	}
	printk(KERN_INFO DEV_NAME ":remove() OK\n");
#endif
	return 0;
}

static struct platform_device cen_nv_device = {
	.name           = DEV_NAME,
	.id             = -1,
};
static struct platform_driver cen_nv_driver = {
	.driver.name	= DEV_NAME,
	.driver.owner	= THIS_MODULE,
	.probe			= felica_cen_nv_probe,
	.remove			= __devexit_p(felica_cen_nv_remove),
};

static int felica_cen_nv_init(void)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":init() [IN]\n");
#endif
	ret = platform_device_register(&cen_nv_device);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME ":platform_device_register error[%d]\n", ret);
		return ret;
	}

	ret = platform_driver_register(&cen_nv_driver);
	if (unlikely(ret)) {
		platform_device_unregister(&cen_nv_device);
		printk(KERN_ERR DEV_NAME ":platform_driver_register error[%d]\n", ret);
		return ret;
	}
#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":init() OK\n");
#endif
	return 0;
}

static void felica_cen_nv_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":exit() [IN]\n");
#endif
	platform_driver_unregister(&cen_nv_driver);
	platform_device_unregister(&cen_nv_device);
#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":exit() OK\n");
#endif
}

module_init(felica_cen_nv_init);
module_exit(felica_cen_nv_exit);

MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("Felica CEN NV Driver");
MODULE_LICENSE("GPL");
