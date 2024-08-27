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

#include "snfc_hw.h"
#include "snfc_ioctl.h"

#define DEV_NAME SNFC_DEV_HSEL
#define CLASS_NAME SNFC_DEV_HSEL
#define LOCAL_BUFLEN 4

static struct cdev char_dev;
static dev_t dev;
static struct class	 *dev_class;

extern int makercmd_mode;

//// SW //////////////////////////////////////////////////////
static int snfc_hsel_sw_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN]\n",__func__);
#endif
	ret = snfc_hw_hsel_init();
#ifdef NFC_DEBUG
	printk(KERN_INFO "Debug Read [HSEL Register = %d]\n", snfc_hw_hsel_read());
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static int snfc_hsel_sw_exit(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN]\n",__func__);
#endif
	ret = snfc_hw_hsel_exit();
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static inline int snfc_hsel_sw_open(void) {
	return 0;
}

static inline int snfc_hsel_sw_close(void) {
	return 0;
}

static int snfc_hsel_sw_write(char val) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] val=%d\n",__func__, (unsigned int)val);
#endif
	if(val == 0) {
		MANAGELOG(KERN_INFO DEV_NAME":write() set 0\n");
		snfc_hw_hsel_write(SNFC_HW_LOW);
	} else if(val == 1) {
		MANAGELOG(KERN_INFO DEV_NAME":write() set 1\n");
		snfc_hw_hsel_write(SNFC_HW_HIGH);
	} else {
		printk(KERN_ERR DEV_NAME":write() failed.[%d]\n", ret);
		return -EINVAL;
	}

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static inline int snfc_hsel_sw_read(void) {
	return snfc_hw_hsel_read();
}

//// IF //////////////////////////////////////////////////////
static int snfc_hsel_if_init(void) {
	int ret = 0;
	ret = snfc_hsel_sw_init();
	return ret;
}

static int snfc_hsel_if_exit(void) {
	int ret = 0;
	ret = snfc_hsel_sw_exit();
	return ret;
}

static int snfc_hsel_if_open(struct inode *inode, struct file *filp) {
	int ret = 0;
#ifdef NFC_UID_CHECK
	long uid_chk;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	if(likely(makercmd_mode != WKUP_MODE_MC)) {
		uid_chk = sys_getuid();

		if(unlikely((uid_chk != NFCCTL_UID2) && (uid_chk != NFCCTL_UID7))) {
			printk(KERN_ERR DEV_NAME ":open() NG UID = 0x%lx\n",uid_chk);
			return -EIO;
		}
	} else {
		uid_chk = sys_getuid();

		if(unlikely(uid_chk != NFCCTL_UID6)) {
			printk(KERN_ERR DEV_NAME ":open() NG UID = 0x%lx\n",uid_chk);
			return -EIO;
		}
	}
#else
	MANAGELOG(KERN_INFO DEV_NAME ":open() UID no check\n");
#endif

	ret = snfc_hsel_sw_open();
	if(unlikely(ret)) {
		return ret;
	}

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif

	return ret;
}

static int snfc_hsel_if_close(struct inode *inode, struct file *filp) {
	int ret = 0;
	ret = snfc_hsel_sw_close();
 	if(unlikely(ret)) {
		return ret;
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
	printk(KERN_INFO DEV_NAME ":close() OK\n");
#endif
	return ret;
}

static int snfc_hsel_if_write(struct file *filp, const char __user *data, size_t len, loff_t *pos) {
	int  ret = 0;
	char localbuf[LOCAL_BUFLEN];

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] data=0x%p, len=%d\n",__func__, data, len);
#endif
	if((len < 1) || (data == 0)){
		printk(KERN_ERR DEV_NAME":write() parameter err\n");
		return -EINVAL;
	}

	if(copy_from_user(localbuf, data, 1)){
		printk(KERN_ERR DEV_NAME":write() copy failed\n");
		return -EINVAL;
	}

	ret = snfc_hsel_sw_write(localbuf[0]);
	if(likely(!ret)) {
		ret = 1;
	}
	
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static ssize_t snfc_hsel_if_read(struct file *filp, char __user *data, size_t len, loff_t *pos) {
	char localbuf[LOCAL_BUFLEN];
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] data=0x%p, len=%d\n",__func__, data, len);
#endif
	if((len < 1) || (data == 0)){
		printk(KERN_ERR DEV_NAME":read() parameter\n");
		return -EINVAL;
	}

	ret = snfc_hsel_sw_read();
	if(ret > 0) {
		MANAGELOG(KERN_INFO DEV_NAME":read() data HI, return 1\n");
		localbuf[0] = 1;
	} else if(!ret) {
		MANAGELOG(KERN_INFO DEV_NAME":read() data LO, return 0\n");
		localbuf[0] = 0;
	} else {
		printk(KERN_ERR DEV_NAME":read() failed:[%d]\n", ret);
		return ret;
	}

	if(copy_to_user(data,localbuf,1)){
		printk(KERN_ERR DEV_NAME":read() copy faild\n");
		return -EINVAL;
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT]\n",__func__);
#endif
	return 1;
}


//// Init //////////////////////////////////////////////////////
static struct file_operations driver_fops = {
	.open 		= snfc_hsel_if_open,
	.write		= snfc_hsel_if_write,
	.read		= snfc_hsel_if_read,
	.release 	= snfc_hsel_if_close,
};

static int __devinit snfc_hsel_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *class_dev = NULL;

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif

	ret = alloc_chrdev_region(&dev, 0, 1, DEV_NAME);
	if(unlikely(ret)){
		printk(KERN_ERR  DEV_NAME ":can't alloc major.\n" );
		return ret;
	}
	
	cdev_init(&char_dev, &driver_fops);
	char_dev.owner = THIS_MODULE;
	ret = cdev_add(&char_dev, dev, 1);
	if(unlikely(ret)){
		printk(KERN_ERR  DEV_NAME ":Major no. cannot be assigned.\n" );
		return ret;
	}

	dev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (unlikely(IS_ERR(dev_class))) {
		printk(KERN_ERR  DEV_NAME ":cannot be class_created.\n" );
		return -EINVAL;
	}

	class_dev = device_create(
					dev_class,
					NULL,
					dev,
					NULL,
					DEV_NAME);

	ret = snfc_hsel_if_init();

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}


static int __devexit snfc_hsel_remove(struct platform_device *pdev)
{
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	cdev_del(&char_dev);
	unregister_chrdev_region(dev, 1);
	ret = snfc_hsel_if_exit();

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static struct platform_device snfc_hsel_device = {
	.name           = "snfc_hsel",
	.id             = -1,
};

static struct platform_driver hsel_driver = {
	.driver.name	= DEV_NAME,
	.driver.owner	= THIS_MODULE,
	.probe			= snfc_hsel_probe,
	.remove			= __devexit_p(snfc_hsel_remove),
};


static int __init snfc_hsel_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = platform_device_register(&snfc_hsel_device);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s : platform_device_register error(ret : %d)\n", __func__, ret);
		return ret;
	}
	ret = platform_driver_register(&hsel_driver);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s : platform_driver_register error(ret : %d)\n", __func__, ret);
		platform_device_unregister(&snfc_hsel_device);
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static void __exit snfc_hsel_exit(void) {
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	platform_driver_unregister(&hsel_driver);
	platform_device_unregister(&snfc_hsel_device);
#ifdef NFC_DEBUG
	printk(KERN_INFO DEV_NAME ":exit() OK\n");
#endif
}

module_init(snfc_hsel_init);
module_exit(snfc_hsel_exit);

MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("NFC HSEL Port Control Driver");
MODULE_LICENSE("GPL");
