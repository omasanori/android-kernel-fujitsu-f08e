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

#define DEV_NAME SNFC_DEV_AVLPL
#define CLASS_NAME SNFC_DEV_AVLPL
#define LOCAL_BUFLEN 4

static struct cdev char_dev;
static dev_t dev;
static struct class	 *dev_class;

extern int makercmd_mode;
static int rd_cen = 0;
static int rd_rfs = 0;

static int irq_detected = 1;
static struct completion	wait_comp;
static struct wake_lock snfc_available_lock;

static int comp_ret = 0;
static irqreturn_t handle_snfc_available(int irq, void *devid) {
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif

	if(snfc_hw_rfs_read()) {
		MANAGELOG(KERN_INFO DEV_NAME ":irq() called HI.\n");
		if (!irq_detected) {
			irq_detected = 1;
			wake_lock_timeout(&snfc_available_lock, 2*HZ);
			complete(&wait_comp);
		}
	} else {
		MANAGELOG(KERN_INFO DEV_NAME ":irq() called LO.\n");
	}

#ifdef NFC_DEBUG
	printk(KERN_INFO DEV_NAME ":read() irq detected\n");
	printk(KERN_INFO DEV_NAME ":handle_snfc_available() OK\n");
#endif
	return IRQ_HANDLED;
}


//// SW //////////////////////////////////////////////////////
static int snfc_available_sw_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN]\n",__func__);
#endif
	ret = snfc_hw_available_init();
	if(unlikely(ret)) {
		return ret;
	}
	init_completion(&wait_comp);
	wake_lock_init(&snfc_available_lock, WAKE_LOCK_SUSPEND, "snfc_available_lock");
	ret = snfc_hw_rfs_req_irq(handle_snfc_available);
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static int snfc_available_sw_exit(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN]\n",__func__);
#endif
	snfc_hw_rfs_free_irq();
	ret = snfc_hw_available_exit();
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static inline int snfc_available_sw_open(void) {
	return 0;
}

static inline int snfc_available_sw_close(void) {
	if (comp_ret) {
#ifdef NFC_DEBUG
		printk(KERN_INFO "%s :irq disabling ret = %d\n",__func__, comp_ret);
#endif
		snfc_hw_rfs_irq_disable();
		irq_detected = 1;
		comp_ret = 0;
	}
	return 0;
}


static int snfc_available_sw_read(void) {

	if (!comp_ret) {
#ifdef NFC_DEBUG
		printk(KERN_INFO "%s :irq enabling ret = %d\n",__func__, comp_ret);
#endif
		snfc_hw_rfs_irq_enable();
	}

	do {
		rd_cen = snfc_hw_cen_read();
		if (rd_cen == 0) {
			printk(KERN_INFO DEV_NAME":read() err rd_cen=%d\n", rd_cen);
			snfc_hw_rfs_irq_disable();
			comp_ret = 0;
			return -EBUSY;
		}
		rd_rfs = snfc_hw_rfs_read();
		
		if ((rd_cen) && (rd_rfs)) {
			MANAGELOG(KERN_INFO DEV_NAME":read() AutoPolling available\n");
			comp_ret = 0;
		} else {
			MANAGELOG(KERN_INFO DEV_NAME ":read() AutoPolling not available\n");
			irq_detected = 0;
			comp_ret = wait_for_completion_interruptible(&wait_comp);
			if (comp_ret) {
#ifdef NFC_DEBUG
				printk(KERN_INFO "%s :wait_for_completion sig received ret = %d\n",__func__, comp_ret);
#endif
				return comp_ret;
			}
		}
	} while(!((rd_cen) && (rd_rfs)));

	snfc_hw_rfs_irq_disable();
	return 1;
}

//// IF //////////////////////////////////////////////////////

static int snfc_available_if_init(void) {
	int ret = 0;
	ret = snfc_available_sw_init();
	return ret;
}

static int snfc_available_if_exit(void) {
	int ret = 0;
	ret = snfc_available_sw_exit();
	return ret;
}

static int snfc_available_if_open(struct inode *inode, struct file *filp) {
	int ret = 0;
#ifdef NFC_UID_CHECK
	long uid_chk;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	if(likely(makercmd_mode != WKUP_MODE_MC)) {
		uid_chk = sys_getuid();

		if(unlikely(uid_chk != NFCCTL_UID7)) {
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

	ret = snfc_available_sw_open();
	if(unlikely(ret)) {
		return ret;
	}

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif

	return ret;
}

static int snfc_available_if_close(struct inode *inode, struct file *filp) {
	int ret = 0;
	ret = snfc_available_sw_close();
 	if(unlikely(ret)) {
		return ret;
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
	printk(KERN_INFO DEV_NAME ":close() OK\n");
#endif
	return ret;
}


static ssize_t snfc_available_if_read(struct file *filp, char __user *data, size_t len, loff_t *pos) {
	char localbuf[LOCAL_BUFLEN];
	int ret = 0;

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] data=0x%p, len=%d\n",__func__, data, len);
#endif
	if((len < 1) || (data == 0)){
		printk(KERN_ERR DEV_NAME":read() parameter\n");
		return -EINVAL;
	}

	ret = snfc_available_sw_read();
	if(ret > 0) {
		MANAGELOG(KERN_INFO DEV_NAME":read() data HI, return 1\n");
		localbuf[0] = 1;
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
	.open 		= snfc_available_if_open,
	.read		= snfc_available_if_read,
	.release 	= snfc_available_if_close,
};

static int __devinit snfc_available_probe(struct platform_device *pdev)
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

	ret = snfc_available_if_init();

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static int __devexit snfc_available_remove(struct platform_device *pdev)
{
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	cdev_del(&char_dev);
	unregister_chrdev_region(dev, 1);
	ret = snfc_available_if_exit();

#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static struct platform_device snfc_available_device = {
	.name           = "snfc_available_poll",
	.id             = -1,
};

static struct platform_driver available_driver = {
	.driver.name	= DEV_NAME,
	.driver.owner	= THIS_MODULE,
	.probe			= snfc_available_probe,
	.remove			= __devexit_p(snfc_available_remove),
};

static int __init snfc_available_init(void) {
	int ret = 0;
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	ret = platform_device_register(&snfc_available_device);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s : platform_device_register error(ret : %d)\n", __func__, ret);
		return ret;
	}
	ret = platform_driver_register(&available_driver);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s : platform_driver_register error(ret : %d)\n", __func__, ret);
		platform_device_unregister(&snfc_available_device);
	}
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[OUT] ret:[%d]\n",__func__, ret);
#endif
	return ret;
}

static void __exit snfc_available_exit(void) {
#ifdef NFC_DEBUG
	printk(KERN_INFO "%s :[IN] \n",__func__);
#endif
	platform_driver_unregister(&available_driver);
	platform_device_unregister(&snfc_available_device);
#ifdef NFC_DEBUG
	printk(KERN_INFO DEV_NAME ":exit() OK\n");
#endif
}

module_init(snfc_available_init);
module_exit(snfc_available_exit);

MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("NFC Available Poll Control Driver");
MODULE_LICENSE("GPL");
