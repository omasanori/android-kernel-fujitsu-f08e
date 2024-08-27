/*
 * Copyright(C) 2013 FUJITSU LIMITED
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include "leds-bu26507gul-init.h"
#include "leds-bu26507gul-if.h"

/* prot type */
static int __devinit bu26507gul_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit bu26507gul_remove(struct i2c_client *client);
static int bu26507gul_suspend(struct device *dev);
static int bu26507gul_resume(struct device *dev);
static int __init bu26507gul_init(void);
static void __exit bu26507gul_exit(void);
static void bu26507gul_shutdown(struct i2c_client *client);


static const struct i2c_device_id bu26507gul_id[] = {
	{"leds-bu26507gul", 0},
	{ }
};

static SIMPLE_DEV_PM_OPS(bu26507gul_pm, bu26507gul_suspend, bu26507gul_resume);
#define BU26507GUL_PM (&bu26507gul_pm)

static struct i2c_driver bu26507gul_driver = {
	.driver	= {
		.name	= "leds-bu26507gul",
		.pm	= BU26507GUL_PM,
		.owner	=THIS_MODULE,
	},
	.probe	= bu26507gul_probe,
	.remove	= __devexit_p(bu26507gul_remove),
	.shutdown   = bu26507gul_shutdown,
	.id_table	= bu26507gul_id,
};

static int __init bu26507gul_init(void)
{
	int	iret = 0;
	
	illumination_info_printk("[%s] \n",__func__);

	iret = bu26507gul_if_init();
	if (unlikely(iret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_if_init ret=%d \n",__func__,iret);
		return (iret);
	}
	
	iret = i2c_add_driver(&bu26507gul_driver);
	if (unlikely(iret != 0)) {
		illumination_debug_printk("[%s]  i2c_add_driver ret=%d \n",__func__,iret);
		return (iret);
	}

	return iret;
}

static int __devinit bu26507gul_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int iret;
	illumination_info_printk("[%s] \n",__func__);
	iret = bu26507gul_if_probe(client,id);
	if (unlikely(iret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_if_probe ret=%d \n",__func__,iret);
	}
	return iret;
}

static int bu26507gul_suspend(struct device *dev)
{
	int iret;
	illumination_info_printk("[%s] \n",__func__);
	iret = bu26507gul_if_suspend(dev);
	if (unlikely(iret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_if_suspend ret=%d \n",__func__,iret);
	}
	return iret;
}

static int bu26507gul_resume(struct device *dev)
{
	int iret;
	illumination_info_printk("[%s] \n",__func__);
	iret = bu26507gul_if_resume(dev);
	if (unlikely(iret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_if_resume ret=%d \n",__func__,iret);
	}
	return iret;
}

static int __devexit bu26507gul_remove(struct i2c_client *client)
{
	int iret;
	illumination_info_printk("[%s] \n",__func__);
	iret = bu26507gul_if_remove(client);
	if (unlikely(iret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_if_remove ret=%d \n",__func__,iret);
	}
	return iret;
}

static void bu26507gul_shutdown(struct i2c_client *client)
{
	int iret;
	illumination_info_printk("[%s] \n",__func__);
	iret = bu26507gul_if_shutdown(client);
	if (unlikely(iret != 0)) {
		illumination_debug_printk("[%s] bu26507gul_if_shutdown ret=%d \n",__func__,iret);
	}
}

static void __exit bu26507gul_exit(void)
{
	int iret;
	illumination_info_printk("[%s] \n",__func__);

	i2c_del_driver(&bu26507gul_driver);
	
	iret = bu26507gul_if_exit();
	if (unlikely(iret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_if_exit ret=%d \n",__func__,iret);
	}
}

module_init(bu26507gul_init);
module_exit(bu26507gul_exit);

MODULE_DESCRIPTION("BU26507GUL LED/GPO driver");
MODULE_DEVICE_TABLE(i2c, bu26507gul);
