/*
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
/*
 * notification LED driver for GPIOs
 *
 * Base on leds-gpio.c, ledtrig-timer.c
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/syscalls.h>

#include "../../arch/arm/include/asm/system.h"

#include "leds-notification_hw.h"
#include "leds-notification_log.h"

static struct pm_gpio led_config = {
	.direction		=	PM_GPIO_DIR_OUT,
	.output_value	=	0,
	.pull			=	PM_GPIO_PULL_NO,
	.out_strength	=	PM_GPIO_STRENGTH_MED,
	.vin_sel		=	PM_GPIO_VIN_S4,
	.function		=	PM_GPIO_FUNC_NORMAL,
	.output_buffer	=	PM_GPIO_OUT_BUF_CMOS,
};

int notification_led_hw_set(int red, int green, int blue)
{
	DBG_LOG_FUNCENTER("[red=0x%d, green=%d, blue=%d]", red, green, blue);

	gpio_set_value(PM8921_GPIO_PM_TO_SYS(LED_R), red);
	gpio_set_value(PM8921_GPIO_PM_TO_SYS(LED_G), green);
	gpio_set_value(PM8921_GPIO_PM_TO_SYS(LED_B), blue);

	DBG_LOG_FUNCLEAVE("");
	
	return 0;
}

int notification_led_hw_init(struct platform_device * pdev)
{
	int ret;
	
	DBG_LOG_FUNCENTER("[pdev=0x%p]", pdev);
	
	do {
		ret = gpio_request(PM8921_GPIO_PM_TO_SYS(LED_R), "led_r");
		if (unlikely(ret)) {
			printk(KERN_ERR "gpio_request[%d] failed.\n", LED_R );
			break;
		}
		ret = gpio_request(PM8921_GPIO_PM_TO_SYS(LED_G), "led_g");
		if (unlikely(ret)) {
			printk(KERN_ERR "gpio_request[%d] failed.\n", LED_G );
			break;
		}
		ret = gpio_request(PM8921_GPIO_PM_TO_SYS(LED_B), "led_b");
		if (unlikely(ret)) {
			printk(KERN_ERR "gpio_request[%d] failed.\n", LED_B );
			break;
		}

		ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(LED_G), &led_config);
		if (unlikely(ret)) {
			printk(KERN_ERR "pm8xxx_gpio_config[%d] failed.\n", LED_G );
			break;
		}
		ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(LED_B), &led_config);
		if (unlikely(ret)) {
			printk(KERN_ERR "pm8xxx_gpio_config[%d] failed.\n", LED_B );
			break;
		}
	} while(0);
	
	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	
	return ret;
}
int notification_led_hw_exit(struct platform_device * pdev)
{
	DBG_LOG_FUNCENTER("[pdev=0x%p]", pdev);
	gpio_free(PM8921_GPIO_PM_TO_SYS(LED_R));
	gpio_free(PM8921_GPIO_PM_TO_SYS(LED_G));
	gpio_free(PM8921_GPIO_PM_TO_SYS(LED_B));
	DBG_LOG_FUNCLEAVE("");

	return 0;
}
