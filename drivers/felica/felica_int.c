/*
 * linux/drivers/felica/felica_int.c
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
#include <linux/wakelock.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/poll.h>

#include "felica_ioctl.h"
#include "felica_hw.h"

#include <linux/syscalls.h>

#include <linux/interrupt.h>
#include <linux/kthread.h>

#define DEV_NAME "felica_int"
#define CLASS_NAME "felica_int"
#define LOCAL_BUFLEN 4

static struct cdev char_dev;
static dev_t dev;
static struct class	 *dev_class;
static int irq_detected;

static DECLARE_WAIT_QUEUE_HEAD(felica_int_wait);
static DEFINE_SPINLOCK(felica_spinlock);
static struct wake_lock felica_int_lock;

extern int makercmd_mode;

static int open_count = 0;

//// SW //////////////////////////////////////////////////////
static irqreturn_t handle_felica_int(int irq, void *devid)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":handle_felica_int() [IN]\n");
#endif
	wake_lock_timeout(&felica_int_lock, 2 * HZ);
	if (felica_int_hw_read()) {
		printk(KERN_INFO DEV_NAME ":irq() called HI.\n");
	} else {
		printk(KERN_INFO DEV_NAME ":irq() called LO.\n");
		irq_detected = 1;
		wake_up_interruptible(&felica_int_wait);
	}

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":handle_felica_int() OK\n");
#endif
	return IRQ_HANDLED;
}

static int felica_int_sw_init(void)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_init() [IN]\n");
#endif
	ret = felica_int_hw_init();
	if (unlikely(ret)) {
		return ret;
	}

	init_waitqueue_head(&felica_int_wait);
	wake_lock_init(&felica_int_lock, WAKE_LOCK_SUSPEND, "felica_int_lock");

	ret = felica_int_hw_init_irq(handle_felica_int);
	if (unlikely(ret)) {
		return ret;
	}

	return ret;
}

static int felica_int_sw_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_exit() [IN]\n");
#endif
	wake_lock_destroy(&felica_int_lock);
	return felica_int_hw_exit();
}

static int felica_int_sw_open(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_open() [IN]\n");
#endif
	irq_detected = 0;

	return felica_int_hw_open();
}

static int felica_int_sw_close(void)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_close() [IN]\n");
#endif
	ret = felica_int_hw_close();

	irq_detected = 0;

	return ret;
}

static int felica_int_sw_read(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_read() [IN]\n");
#endif
	return felica_int_hw_read();
}

static unsigned int felica_int_sw_poll(struct file *file, poll_table *wait)
{
	unsigned int mask;
	unsigned long flags;
	mask = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":sw_poll() [IN]\n");
#endif
	poll_wait(file, &felica_int_wait, wait);

	spin_lock_irqsave(&felica_spinlock, flags);
	if (irq_detected) {
		printk(KERN_INFO DEV_NAME ":poll() irq detected\n");
		mask |= POLLIN | POLLRDNORM;
		irq_detected = 0;
	}
	spin_unlock_irqrestore(&felica_spinlock, flags);

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":mask = %d\n", mask);
	printk(KERN_INFO DEV_NAME ":irq_detected = %d\n", irq_detected);
	printk(KERN_INFO DEV_NAME ":sw_poll() OK\n");
#endif
	return mask;
}

//// IF //////////////////////////////////////////////////////
static int felica_int_if_init(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_init() [IN]\n");
#endif
	return felica_int_sw_init();
}

static int felica_int_if_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_exit() [IN]\n");
#endif
	return felica_int_sw_exit();
}

static int felica_int_if_open(struct inode *inode, struct file *filp)
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

			if (unlikely((uid_chk != FCCTL_UID2) &&
				(uid_chk != FCCTL_UID5))) {
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
		ret = felica_int_sw_open();
		if (unlikely(ret)) {
			return ret;
		}


	open_count++;

#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":if_open() OK open_count = %d\n", open_count);
#endif
	return 0;
}

static int felica_int_if_close(struct inode *inode, struct file *filp)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_close() [IN]\n");
#endif
	if (likely(open_count == 1)) {
		ret = felica_int_sw_close();
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

static ssize_t felica_int_if_read(
	struct file *filp, char __user *data, size_t len, loff_t *pos)
{
	char localbuf[LOCAL_BUFLEN];
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_read() [IN]\n");
#endif
	if (unlikely((len < 1) || (data == NULL))) {
		printk(KERN_ERR DEV_NAME":read() parameter err1\n");
		return -EINVAL;
	}

	ret = felica_int_sw_read();
	if (ret > 0) {
		printk(KERN_INFO DEV_NAME":read() data HI, return 0\n");
		localbuf[0] = 0;
	} else if (ret == 0) {
		printk(KERN_INFO DEV_NAME":read() data LO, return 1\n");
		localbuf[0] = 1;
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

static unsigned int
felica_int_if_poll(struct file *file, poll_table *wait)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":if_poll() [IN]\n");
#endif
	return felica_int_sw_poll(file, wait);
}

//// Init //////////////////////////////////////////////////////
static struct file_operations driver_fops = {
	.open 		= felica_int_if_open,
	.read		= felica_int_if_read,
	.release 	= felica_int_if_close,
	.poll		= felica_int_if_poll,
};

static int __devinit felica_int_probe(struct platform_device *pdev)
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

	ret = felica_int_if_init();
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

static int __devexit felica_int_remove(struct platform_device *pdev)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":remove() [IN]\n");
#endif
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&char_dev);
	unregister_chrdev_region(dev, 1);

	ret = felica_int_if_exit();
#ifdef FELICA_DEBUG
	if (unlikely(ret)) {
		return ret;
	}
	printk(KERN_INFO DEV_NAME ":remove() OK\n");
#endif
	return 0;
}

static struct platform_device int_device = {
	.name           = "felica_int",
	.id             = -1,
};
static struct platform_driver int_driver = {
	.driver.name	= DEV_NAME,
	.driver.owner	= THIS_MODULE,
	.probe			= felica_int_probe,
	.remove			= __devexit_p(felica_int_remove),
};

static int felica_int_init(void)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":init() [IN]\n");
#endif
	ret = platform_device_register(&int_device);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME ":platform_device_register error[%d]\n", ret);
		return ret;
	}

	ret = platform_driver_register(&int_driver);
	if (unlikely(ret)) {
		platform_device_unregister(&int_device);
		printk(KERN_ERR DEV_NAME ":platform_driver_register error[%d]\n", ret);
		return ret;
	}
#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":init() OK\n");
#endif
	return 0;
}

static void felica_int_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME ":exit() [IN]\n");
#endif
	platform_driver_unregister(&int_driver);
	platform_device_unregister(&int_device);
#ifdef FELICA_DEBUG
	printk(KERN_INFO DEV_NAME ":exit() OK\n");
#endif
}

module_init(felica_int_init);
module_exit(felica_int_exit);

MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("Felica INT Port Control Driver");
MODULE_LICENSE("GPL");
