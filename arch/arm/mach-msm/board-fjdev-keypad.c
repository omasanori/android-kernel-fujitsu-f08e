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

#include <linux/platform_device.h>
#include <linux/gpio_event.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <asm/system.h>

/* key type */
enum{
	KEY_TYPE_FP_HOME= 0x0,
	KEY_TYPE_VOLUME_UP,
	KEY_TYPE_VOLUME_DOWN,
	KEY_TYPE_CAMERA,
	KEY_TYPE_XEMG_ALM
};

/* machine type */
#define MACHINE_TYPE_MASK       0xF0
#define MACHINE_TYPE_FJDEV001   0x50
#define MACHINE_TYPE_FJDEV002   0x40
#define MACHINE_TYPE_FJDEV003   0x70
#define MACHINE_TYPE_FJDEV004   0x60
#define MACHINE_TYPE_FJDEV005   0x10
/* trial type */
#define TRIAL_TYPE_MASK         0xF
#define TRIAL_TYPE_FJDEV001_1   0x0
#define TRIAL_TYPE_FJDEV004_1   0x0
#define TRIAL_TYPE_FJDEV005_1   0x0

static struct gpio_event_direct_entry keypad_nav_map[] = {
	{29, KEY_FP},
	{81, KEY_VOLUMEUP},
	{32, KEY_VOLUMEDOWN},
};

static struct gpio_event_direct_entry keypad_nav_map_hmw[] = {
	{29, KEY_HOME},
	{81, KEY_VOLUMEUP},
	{32, KEY_VOLUMEDOWN},
    {86, KEY_CAMERA},
    {72, KEY_XEMG_ALM},
};

static int keypad_gpio_event_nav_func(
	struct gpio_event_input_devs  *input_dev,
	struct gpio_event_info *info,
	void **data, int func);

static struct gpio_event_input_info keypad_nav_info = {
		.info.func   = keypad_gpio_event_nav_func,
		.flags       = GPIOKPF_LEVEL_TRIGGERED_IRQ |
						GPIOKPF_DRIVE_INACTIVE |
						GPIOEDF_PRINT_KEYS |
						GPIOEDF_PRINT_KEY_DEBOUNCE |
						GPIOEDF_PRINT_KEY_UNSTABLE,
		.type        = EV_KEY,
		.keymap      = keypad_nav_map,
		.debounce_time.tv64 = 20 * NSEC_PER_MSEC,
		.keymap_size = ARRAY_SIZE(keypad_nav_map)
};

static struct gpio_event_info *keypad_info[] = {
	&keypad_nav_info.info
};

static struct gpio_event_platform_data keypad_data = {
	.name       = "keypad",
	.info       = keypad_info,
	.info_count = ARRAY_SIZE(keypad_info)
};

struct platform_device keypad_device_fjdev = {
	.name   = GPIO_EVENT_DEV_NAME,
	.id     = -1,
	.dev    = {
		.platform_data  = &keypad_data,
	},
};


struct gpio_event_input_devs *keypad_dev;

static int enable_irq_wake_flag;

void gpio_event_keypad_suspend(void){
	int i, irq;

	for (i=KEY_TYPE_VOLUME_UP; i<ARRAY_SIZE(keypad_nav_map); i++) {
		irq = gpio_to_irq(keypad_nav_map[i].gpio);
		enable_irq_wake(irq);
	}

	enable_irq_wake_flag = true;
}

void gpio_event_keypad_resume(void){
	int i, irq;

	if(enable_irq_wake_flag == true){
		for (i=KEY_TYPE_VOLUME_UP; i<ARRAY_SIZE(keypad_nav_map); i++) {
			irq = gpio_to_irq(keypad_nav_map[i].gpio);
			disable_irq_wake(irq);
		}
		enable_irq_wake_flag = false;
	}
}


void gpio_event_keypad_suspend_hmw(void){
	enable_irq_wake(gpio_to_irq(keypad_nav_info.keymap[KEY_TYPE_VOLUME_UP].gpio));
	enable_irq_wake(gpio_to_irq(keypad_nav_info.keymap[KEY_TYPE_VOLUME_DOWN].gpio));

	enable_irq_wake_flag = true;
}


void gpio_event_keypad_resume_hmw(void)
{
	if(enable_irq_wake_flag == true){
		disable_irq_wake(gpio_to_irq(keypad_nav_info.keymap[KEY_TYPE_VOLUME_UP].gpio));
		disable_irq_wake(gpio_to_irq(keypad_nav_info.keymap[KEY_TYPE_VOLUME_DOWN].gpio));

		enable_irq_wake_flag = false;
	}
}

extern int gpio_event_input_func(struct gpio_event_input_devs *input_devs,
						struct gpio_event_info *info, void **data, int func);

static int keypad_gpio_event_nav_func(
	struct gpio_event_input_devs  *input_dev,
	struct gpio_event_info *info,
	void **data, int func)
{
    int err;
	
	err = gpio_event_input_func(input_dev, info, data, func);
	
	if (func == GPIO_EVENT_FUNC_INIT && !err) {
		keypad_dev = input_dev;
	} else if (func == GPIO_EVENT_FUNC_UNINIT) {
		keypad_dev = NULL;
	}

	return err;
}

struct gpio_event_input_devs *msm_keypad_get_input_dev(void)
{
	return keypad_dev;
}


static int __init fjdev_init_keypad(void)
{
	enable_irq_wake_flag = false;

	switch(system_rev & MACHINE_TYPE_MASK)
	{
		case MACHINE_TYPE_FJDEV001:
			if ((system_rev & TRIAL_TYPE_MASK) == TRIAL_TYPE_FJDEV001_1) {
				keypad_nav_map[KEY_TYPE_VOLUME_DOWN].gpio = 1;
			}
			break;

		case MACHINE_TYPE_FJDEV002:
			break;

		case MACHINE_TYPE_FJDEV003:
			keypad_nav_map[KEY_TYPE_VOLUME_DOWN].gpio = 1;
			break;

		case MACHINE_TYPE_FJDEV004:
			if ((system_rev & TRIAL_TYPE_MASK) == TRIAL_TYPE_FJDEV004_1) {
				keypad_nav_map[KEY_TYPE_VOLUME_DOWN].gpio = 1;
			}
			break;

		case MACHINE_TYPE_FJDEV005:
			keypad_nav_info.keymap = keypad_nav_map_hmw;
			keypad_nav_info.keymap_size = ARRAY_SIZE(keypad_nav_map_hmw);

			gpio_tlmm_config(GPIO_CFG(keypad_nav_info.keymap[KEY_TYPE_CAMERA].gpio,
								0,
								GPIO_CFG_INPUT,
								GPIO_CFG_PULL_UP,
								GPIO_CFG_2MA),
								GPIO_CFG_ENABLE); /* KEY_CAMERA */

			gpio_tlmm_config(GPIO_CFG(keypad_nav_info.keymap[KEY_TYPE_XEMG_ALM].gpio,
								0,
								GPIO_CFG_INPUT,
								GPIO_CFG_PULL_UP,
								GPIO_CFG_2MA),
								GPIO_CFG_ENABLE); /* KEY_XEMG_ALM */
			break;

		default:
			pr_err("%s: incorrect machine type[0x%x]", __func__, system_rev & MACHINE_TYPE_MASK);	
			break;
	}

	gpio_tlmm_config(GPIO_CFG(keypad_nav_info.keymap[KEY_TYPE_VOLUME_UP].gpio,
						0,
						GPIO_CFG_INPUT,
						GPIO_CFG_PULL_UP,
						GPIO_CFG_2MA),
						GPIO_CFG_ENABLE); /* KEY_VOLUMEUP   */
	gpio_tlmm_config(GPIO_CFG(keypad_nav_info.keymap[KEY_TYPE_VOLUME_DOWN].gpio,
						0,
						GPIO_CFG_INPUT,
						GPIO_CFG_PULL_UP,
						GPIO_CFG_2MA),
						GPIO_CFG_ENABLE);  /* KEY_VOLUMEDOWN */
	gpio_tlmm_config(GPIO_CFG(keypad_nav_info.keymap[KEY_TYPE_FP_HOME].gpio,
						0,
						GPIO_CFG_INPUT,
						GPIO_CFG_PULL_UP,
						GPIO_CFG_2MA),
						GPIO_CFG_ENABLE); /* KEY_FP or KEY_HOME(FJDEV005)*/

	return platform_device_register(&keypad_device_fjdev); 
}

device_initcall(fjdev_init_keypad);
