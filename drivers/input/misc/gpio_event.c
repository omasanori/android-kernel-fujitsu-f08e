/* drivers/input/misc/gpio_event.c
 *
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012-2013
/*----------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/input.h>
#include <linux/gpio_event.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
/* FUJITSU:2013.4.1 Add Start */
#include <asm/system.h>
/* FUJITSU:2013.4.1 Add End */

struct gpio_event {
	struct gpio_event_input_devs *input_devs;
	const struct gpio_event_platform_data *info;
	void *state[0];
};

static int gpio_input_event(
	struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	int i;
	int devnr;
	int ret = 0;
	int tmp_ret;
	struct gpio_event_info **ii;
	struct gpio_event *ip = input_get_drvdata(dev);

	for (devnr = 0; devnr < ip->input_devs->count; devnr++)
		if (ip->input_devs->dev[devnr] == dev)
			break;
	if (devnr == ip->input_devs->count) {
		pr_err("gpio_input_event: unknown device %p\n", dev);
		return -EIO;
	}

	for (i = 0, ii = ip->info->info; i < ip->info->info_count; i++, ii++) {
		if ((*ii)->event) {
			tmp_ret = (*ii)->event(ip->input_devs, *ii,
						&ip->state[i],
						devnr, type, code, value);
			if (tmp_ret)
				ret = tmp_ret;
		}
	}
	return ret;
}

static int gpio_event_call_all_func(struct gpio_event *ip, int func)
{
	int i;
	int ret;
	struct gpio_event_info **ii;

	if (func == GPIO_EVENT_FUNC_INIT || func == GPIO_EVENT_FUNC_RESUME) {
		ii = ip->info->info;
		for (i = 0; i < ip->info->info_count; i++, ii++) {
			if ((*ii)->func == NULL) {
				ret = -ENODEV;
				pr_err("gpio_event_probe: Incomplete pdata, "
					"no function\n");
				goto err_no_func;
			}
			if (func == GPIO_EVENT_FUNC_RESUME && (*ii)->no_suspend)
				continue;
			ret = (*ii)->func(ip->input_devs, *ii, &ip->state[i],
					  func);
			if (ret) {
				pr_err("gpio_event_probe: function failed\n");
				goto err_func_failed;
			}
		}
		return 0;
	}

	ret = 0;
	i = ip->info->info_count;
	ii = ip->info->info + i;
	while (i > 0) {
		i--;
		ii--;
		if ((func & ~1) == GPIO_EVENT_FUNC_SUSPEND && (*ii)->no_suspend)
			continue;
		(*ii)->func(ip->input_devs, *ii, &ip->state[i], func & ~1);
err_func_failed:
err_no_func:
		;
	}
	return ret;
}

static void __maybe_unused gpio_event_suspend(struct gpio_event *ip)
{
	gpio_event_call_all_func(ip, GPIO_EVENT_FUNC_SUSPEND);
	if (ip->info->power)
		ip->info->power(ip->info, 0);
}

static void __maybe_unused gpio_event_resume(struct gpio_event *ip)
{
	if (ip->info->power)
		ip->info->power(ip->info, 1);
	gpio_event_call_all_func(ip, GPIO_EVENT_FUNC_RESUME);
}

/* FUJITSU:2012.11.16 FP_DRIVER Add Start */
#define XRESET_AUDDSP 28
extern void gpio_event_input_pm_suspend(void);
extern void gpio_event_keypad_suspend(void);
static int gpio_event_pm_suspend(struct platform_device *pdev, pm_message_t state)
{
	int gpio_sts;

	gpio_event_input_pm_suspend();
	gpio_sts = gpio_get_value(XRESET_AUDDSP);
	if(gpio_sts){
		gpio_event_keypad_suspend();
	}

	return 0;
}

extern void gpio_event_keypad_resume(void);
static int gpio_event_pm_resume(struct platform_device *pdev)
{
	gpio_event_keypad_resume();

	return 0;
}
/* FUJITSU:2012.11.16 FP_DRIVER Add End   */

/* FUJITSU:2013.4.1 Add Start */
extern void gpio_event_keypad_suspend_hmw(void);
static int gpio_event_pm_suspend_hmw(struct platform_device *pdev, pm_message_t state)
{
	int gpio_sts;

	gpio_event_input_pm_suspend();
	gpio_sts = gpio_get_value(XRESET_AUDDSP);
	if(gpio_sts){
		gpio_event_keypad_suspend_hmw();
	}

	return 0;
}


extern void gpio_event_keypad_resume_hmw(void);
static int gpio_event_pm_resume_hmw(struct platform_device *pdev)
{
	gpio_event_keypad_resume_hmw();

	return 0;
}
/* FUJITSU:2013.4.1 Add End */

static int gpio_event_probe(struct platform_device *pdev)
{
	int err;
	struct gpio_event *ip;
	struct gpio_event_platform_data *event_info;
	int dev_count = 1;
	int i;
	int registered = 0;

	event_info = pdev->dev.platform_data;
	if (event_info == NULL) {
		pr_err("gpio_event_probe: No pdata\n");
		return -ENODEV;
	}
	if ((!event_info->name && !event_info->names[0]) ||
	    !event_info->info || !event_info->info_count) {
		pr_err("gpio_event_probe: Incomplete pdata\n");
		return -ENODEV;
	}
	if (!event_info->name)
		while (event_info->names[dev_count])
			dev_count++;
	ip = kzalloc(sizeof(*ip) +
		     sizeof(ip->state[0]) * event_info->info_count +
		     sizeof(*ip->input_devs) +
		     sizeof(ip->input_devs->dev[0]) * dev_count, GFP_KERNEL);
	if (ip == NULL) {
		err = -ENOMEM;
		pr_err("gpio_event_probe: Failed to allocate private data\n");
		goto err_kp_alloc_failed;
	}
	ip->input_devs = (void*)&ip->state[event_info->info_count];
	platform_set_drvdata(pdev, ip);

	for (i = 0; i < dev_count; i++) {
		struct input_dev *input_dev = input_allocate_device();
		if (input_dev == NULL) {
			err = -ENOMEM;
			pr_err("gpio_event_probe: "
				"Failed to allocate input device\n");
			goto err_input_dev_alloc_failed;
		}
		input_set_drvdata(input_dev, ip);
		input_dev->name = event_info->name ?
					event_info->name : event_info->names[i];
		input_dev->event = gpio_input_event;
		ip->input_devs->dev[i] = input_dev;
	}
	ip->input_devs->count = dev_count;
	ip->info = event_info;
	if (event_info->power)
		ip->info->power(ip->info, 1);

	err = gpio_event_call_all_func(ip, GPIO_EVENT_FUNC_INIT);
	if (err)
		goto err_call_all_func_failed;

	for (i = 0; i < dev_count; i++) {
		err = input_register_device(ip->input_devs->dev[i]);
		if (err) {
			pr_err("gpio_event_probe: Unable to register %s "
				"input device\n", ip->input_devs->dev[i]->name);
			goto err_input_register_device_failed;
		}
		registered++;
	}

	return 0;

err_input_register_device_failed:
	gpio_event_call_all_func(ip, GPIO_EVENT_FUNC_UNINIT);
err_call_all_func_failed:
	if (event_info->power)
		ip->info->power(ip->info, 0);
	for (i = 0; i < registered; i++)
		input_unregister_device(ip->input_devs->dev[i]);
	for (i = dev_count - 1; i >= registered; i--) {
		input_free_device(ip->input_devs->dev[i]);
err_input_dev_alloc_failed:
		;
	}
	kfree(ip);
err_kp_alloc_failed:
	return err;
}

static int gpio_event_remove(struct platform_device *pdev)
{
	struct gpio_event *ip = platform_get_drvdata(pdev);
	int i;

	gpio_event_call_all_func(ip, GPIO_EVENT_FUNC_UNINIT);
	if (ip->info->power)
		ip->info->power(ip->info, 0);
	for (i = 0; i < ip->input_devs->count; i++)
		input_unregister_device(ip->input_devs->dev[i]);
	kfree(ip);
	return 0;
}

static struct platform_driver gpio_event_driver = {
	.probe		= gpio_event_probe,
	.remove		= gpio_event_remove,
/* FUJITSU:2012.11.16 FP_DRIVER Add Start */
	.suspend	= gpio_event_pm_suspend,
	.resume		= gpio_event_pm_resume,
/* FUJITSU:2012.11.16 FP_DRIVER Add End   */
	.driver		= {
		.name	= GPIO_EVENT_DEV_NAME,
	},
};

/* FUJITSU:2013.4.1 Add Start */
/* machine type */
#define MACHINE_TYPE_MASK       0xF0
#define MACHINE_TYPE_FJDEV005   0x10
/* FUJITSU:2013.4.1 Add End */

static int __devinit gpio_event_init(void)
{
/* FUJITSU:2013.4.1 Add Start */
	if((system_rev & MACHINE_TYPE_MASK) == MACHINE_TYPE_FJDEV005){
		gpio_event_driver.suspend = gpio_event_pm_suspend_hmw;
		gpio_event_driver.resume = gpio_event_pm_resume_hmw;
	}
/* FUJITSU:2013.4.1 Add End */

	return platform_driver_register(&gpio_event_driver);
}

static void __exit gpio_event_exit(void)
{
	platform_driver_unregister(&gpio_event_driver);
}

module_init(gpio_event_init);
module_exit(gpio_event_exit);

MODULE_DESCRIPTION("GPIO Event Driver");
MODULE_LICENSE("GPL");

