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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <linux/platform_device.h>

#include "snfc_ioctl.h"
#include "linux/nfc/snfc_dev_ioctl.h"


#define DEV_NAME SNFC_DEV_ABTR
#define CLASS_NAME SNFC_DEV_ABTR
#define NFCABTR_UNLOCK -1

static struct cdev char_dev;
static dev_t dev;
static struct class	 *dev_class;

static struct mutex snfc_mlock;

static long lock_status;
static long nfc_mem;

uint8_t g_managelog_flg = 0;

//// SW //////////////////////////////////////////////////////
static int snfc_arbiter_sw_init(void) {
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN]\n",__func__);
#endif
	mutex_init(&snfc_mlock);
	lock_status = NFCABTR_UNLOCK;
	nfc_mem = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 0;
}

static int snfc_arbiter_sw_exit(void) {
	return 0;
}

static inline int snfc_arbiter_sw_open(void) {
	return 0;
}

static inline int snfc_arbiter_sw_close(void) {
	return 0;
}

static int snfc_arbiter_sw_lock(unsigned long param)
{
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] param=%ld\n",__func__, param);
#endif
	mutex_lock(&snfc_mlock);

	switch(param) {
	case 0:
		if(lock_status == sys_getuid()) {
			lock_status = NFCABTR_UNLOCK;
		} else {
			ret = -EIO;
		}
		break;
	case 1:
		if(NFCABTR_UNLOCK == lock_status) {
			lock_status = sys_getuid();
		} else {
			ret = -EBUSY;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&snfc_mlock);
	return ret;
}

static int snfc_arbiter_sw_setshmem(unsigned long param)
{
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] param=%ld\n",__func__, param);
#endif
	mutex_lock(&snfc_mlock);
	nfc_mem = (long)param;
	mutex_unlock(&snfc_mlock);
	return 0;
}

static long snfc_arbiter_sw_getshmem(void)
{
	long ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN]\n",__func__);
#endif
	mutex_lock(&snfc_mlock);
	ret = nfc_mem;
	mutex_unlock(&snfc_mlock);
	return ret;
}

static int snfc_sw_log_output_switch(const char *buf)
{
	long int num = 0;

	if (strict_strtol(buf, 0, &num)) {
		printk(KERN_ERR DEV_NAME ":%s :parameter.\n", __func__);
		return -EINVAL;
	}
	
	g_managelog_flg = (uint8_t)num;

	return 0;
}

//// IF //////////////////////////////////////////////////////
static int snfc_arbiter_if_init(void) {
	int ret = 0;
	ret = snfc_arbiter_sw_init();
	return ret;
}

static int snfc_arbiter_if_exit(void) {
	int ret = 0;
	ret = snfc_arbiter_sw_exit();
	return ret;
}

static int snfc_arbiter_if_open(struct inode *inode, struct file *filp) {
	int ret = 0;
#ifdef NFC_UID_CHECK
	long uid_chk;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	uid_chk = sys_getuid();

	if(NFCCTL_UID2 != uid_chk && NFCCTL_UID7 != uid_chk && NFCCTL_UID1 != uid_chk) {
		printk(KERN_ERR DEV_NAME ":open() NG UID = 0x%lx\n",uid_chk);
		return -EIO;
	}
#else
	MANAGELOG(KERN_INFO DEV_NAME ":open() UID no check\n");
#endif

	ret = snfc_arbiter_sw_open();
	if(unlikely(ret)) {
		return ret;
	}

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif

	return ret;
}

static int snfc_arbiter_if_close(struct inode *inode, struct file *filp) {
	int ret = 0;
	ret = snfc_arbiter_sw_close();
	if(unlikely(ret)) {
		return ret;
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
	printk(KERN_INFO DEV_NAME ":close() OK\n");
#endif
	return ret;
}

static long snfc_arbiter_if_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] cmd=%x, arg=%ld\n",__func__, cmd, arg);
#endif
	switch(cmd){
	case NFCABTR_IOCTL_LOCK:
		ret = snfc_arbiter_sw_lock(arg);
		break;
	case NFCABTR_IOCTL_MEM_SET:
		ret = snfc_arbiter_sw_setshmem(arg);
		break;
	case NFCABTR_IOCTL_MEM_GET:
		ret = snfc_arbiter_sw_getshmem();
		break;
	default:
		ret =-EFAULT;
		break;
		}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret=%ld\n",__func__, ret);
#endif
	return ret;
}

static ssize_t snfc_if_log_output_switch(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = 0;
	
	ret = snfc_sw_log_output_switch(buf);
	if (likely(!ret)) {
		ret = count;
	}
	return ret;
}

//// Init //////////////////////////////////////////////////////

static DEVICE_ATTR(logswitch, (S_IWUSR | S_IWGRP), NULL,
                   snfc_if_log_output_switch);

static struct file_operations driver_fops = {
	.open 			= snfc_arbiter_if_open,
	.unlocked_ioctl	= snfc_arbiter_if_ioctl,
	.release		= snfc_arbiter_if_close,
};

static int __devinit snfc_arbiter_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *class_dev = NULL;

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif

	ret = alloc_chrdev_region(&dev, 0, 1, DEV_NAME);
	if(ret<0){
		printk(KERN_ERR  DEV_NAME ":can't alloc major.\n" );
		return ret;
	}

	cdev_init(&char_dev, &driver_fops);
	char_dev.owner = THIS_MODULE;
	ret = cdev_add(&char_dev, dev, 1);
	if (ret < 0) {
		printk(KERN_ERR  DEV_NAME ":Major no. cannot be assigned.\n" );
		return ret;
	}

	dev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(dev_class)) {
		printk(KERN_ERR  DEV_NAME ":cannot be class_created.\n" );
		return -EINVAL;
	}

	class_dev = device_create(
					dev_class,
					NULL,
					dev,
					NULL,
					DEV_NAME);

	if (device_create_file(&pdev->dev, &dev_attr_logswitch)) {
		pr_err("%s: device_create_file() failed\n", __func__);
	}

	ret = snfc_arbiter_if_init();

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static int __devexit snfc_arbiter_remove(struct platform_device *pdev)
{
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	cdev_del(&char_dev);
	unregister_chrdev_region(dev, 1);
	ret = snfc_arbiter_if_exit();

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static struct platform_device snfc_arbiter_device = {
	.name           = "snfc_arbiter",
	.id             = -1,
};

static struct platform_driver arbiter_driver = {
	.driver.name	= DEV_NAME,
	.driver.owner	= THIS_MODULE,
	.probe			= snfc_arbiter_probe,
	.remove			= __devexit_p(snfc_arbiter_remove),
};


static int __init snfc_arbiter_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = platform_device_register(&snfc_arbiter_device);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s : platform_device_register error(ret : %d)\n", __func__, ret);
		return ret;
	}
	ret = platform_driver_register(&arbiter_driver);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s : platform_driver_register error(ret : %d)\n", __func__, ret);
		platform_device_unregister(&snfc_arbiter_device);
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static void __exit snfc_arbiter_exit(void) {
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	platform_driver_unregister(&arbiter_driver);
	platform_device_unregister(&snfc_arbiter_device);
#ifdef NFC_DEBUG
	printk(KERN_INFO DEV_NAME ":exit() OK\n");
#endif
}

module_init(snfc_arbiter_init);
module_exit(snfc_arbiter_exit);

MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("NFC Arbiter");
MODULE_LICENSE("GPL");
