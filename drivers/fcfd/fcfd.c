/*
  fcfd Driver

  COPYRIGHT(C) FUJITSU LIMITED 2011

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");

#define DRIVER_NAME "fcfd"

static int fcfd_dev_count = 1;
static int fcfd_major = 0;
static int fcfd_minor = 0;
module_param(fcfd_major, int, 0);
static struct class *fcfd_class = NULL;
static struct cdev fcfd_cdev;

static int fcfd_open (struct inode *inode, struct file *filp)
{
	return 0;
}

static int fcfd_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations fcfd_ops = {
	.owner   = THIS_MODULE,
	.open    = fcfd_open,
	.release = fcfd_release,
};

static int fcfd_init(void)
{
	dev_t dev = MKDEV(fcfd_major, 0);
	int alloc_ret = 0;
	int cdev_err = 0;
    //struct class_device *class_dev = NULL;
    struct device *class_dev = NULL;

	if (fcfd_major) {
		alloc_ret = register_chrdev_region(dev, fcfd_dev_count, DRIVER_NAME);
		if (alloc_ret < 0) {
			printk(KERN_ERR "fcfd: unable to get major %d\n", fcfd_major);
			goto error;
		}
		if (fcfd_major == 0)
			fcfd_major = alloc_ret;
	}
	else {
		alloc_ret = alloc_chrdev_region(&dev, fcfd_minor, fcfd_dev_count, DRIVER_NAME);
		if (alloc_ret) {
			printk(KERN_ERR "fcfd: unable to get major \n");
			goto error;
		}
		fcfd_major = MAJOR(dev);
	}

	cdev_init(&fcfd_cdev, &fcfd_ops);

	fcfd_cdev.owner = THIS_MODULE;
	fcfd_cdev.ops = &fcfd_ops;
	cdev_err = cdev_add (&fcfd_cdev, MKDEV(fcfd_major, fcfd_minor), fcfd_dev_count);

	if (cdev_err) {
		goto error;
	}

	fcfd_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(fcfd_class)) {
		goto error;
	}
	class_dev = device_create(fcfd_class, NULL, MKDEV(fcfd_major, fcfd_minor), NULL, DRIVER_NAME);

	if (IS_ERR(class_dev))
		printk(KERN_ERR "fcfd: can't create device\n");

	return 0;
  
error:
	if (cdev_err == 0)
		cdev_del(&fcfd_cdev);
	
	if (alloc_ret == 0)
		unregister_chrdev_region(MKDEV(fcfd_major, 0), fcfd_dev_count);

	return -1;
}

static void fcfd_exit(void)
{
	device_destroy(fcfd_class, MKDEV(fcfd_major, 0));
	class_destroy(fcfd_class);

	cdev_del(&fcfd_cdev);
	unregister_chrdev_region(MKDEV(fcfd_major, 0), fcfd_dev_count);
}

module_init(fcfd_init);
module_exit(fcfd_exit);
