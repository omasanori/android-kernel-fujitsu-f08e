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
#ifndef __BU26507GUL_INIT__
#define __BU26507GUL_INIT__

#include <linux/leds.h>

#define BU26507GUL_DRV_NAME_LED				"bu26507gul-led"
#define BU26507GUL_DRV_NAME_PIERCE_LED		"bu26507gul-pierce-led"
#define BU26507GUL_DRV_NAME_ILLUMINATION	"bu26507gul-illumination"
#define BU26507GUL_NUM_LEDS 30

/* BU26507GUL Register Slave Address */
#define BU26507GUL_REG_SFTRST		0x00
#define BU26507GUL_REG_OSCEN		0x01
#define BU26507GUL_REG_LED1LED6ON	0x11
#define BU26507GUL_REG_PWMSET		0x20
#define BU26507GUL_REG_CLKSEL		0x21
#define BU26507GUL_REG_SLP			0x2D
#define BU26507GUL_REG_SCLRST		0x2E
#define BU26507GUL_REG_SCLSPEED		0x2F
#define BU26507GUL_REG_START		0x30
#define BU26507GUL_REG_CLRAB		0x31
#define BU26507GUL_REG_RMCG_OAB_IAB	0x7F

#define BU26507GUL_REG_SW1LED1		0x01
#define BU26507GUL_REG_SW2LED1		0x02
#define BU26507GUL_REG_SW3LED1		0x03
#define BU26507GUL_REG_SW4LED1		0x04
#define BU26507GUL_REG_SW5LED1		0x05

#define BU26507GUL_REG_SW1LED2		0x06
#define BU26507GUL_REG_SW2LED2		0x07
#define BU26507GUL_REG_SW3LED2		0x08
#define BU26507GUL_REG_SW4LED2		0x09
#define BU26507GUL_REG_SW5LED2		0x0A

#define BU26507GUL_REG_SW1LED3		0x0B
#define BU26507GUL_REG_SW2LED3		0x0C
#define BU26507GUL_REG_SW3LED3		0x0D
#define BU26507GUL_REG_SW4LED3		0x0E
#define BU26507GUL_REG_SW5LED3		0x0F

#define BU26507GUL_REG_SW1LED4		0x10
#define BU26507GUL_REG_SW2LED4		0x11
#define BU26507GUL_REG_SW3LED4		0x12
#define BU26507GUL_REG_SW4LED4		0x13
#define BU26507GUL_REG_SW5LED4		0x14

#define BU26507GUL_REG_SW1LED5		0x15
#define BU26507GUL_REG_SW2LED5		0x16
#define BU26507GUL_REG_SW3LED5		0x17
#define BU26507GUL_REG_SW4LED5		0x18
#define BU26507GUL_REG_SW5LED5		0x19

#define BU26507GUL_REG_SW1LED6		0x1A
#define BU26507GUL_REG_SW2LED6		0x1B
#define BU26507GUL_REG_SW3LED6		0x1C
#define BU26507GUL_REG_SW4LED6		0x1D
#define BU26507GUL_REG_SW5LED6		0x1E

struct bu26507gul_led {
	struct led_classdev	cdev;
	struct work_struct	brightness_work;
	struct device		*master;
	enum led_brightness	new_brightness;
	int			id;
	int			flags;
};

struct bu26507gul_illumination {
	struct led_classdev	cdev;
	struct work_struct	brightness_work;
	struct device		*master;
	enum led_brightness	new_brightness;
	int			id;
	int			flags;
};

enum BU26507GUL_LEDS {
	BU26507GUL_SW1LED1 = 1,
	BU26507GUL_SW2LED1,
	BU26507GUL_SW3LED1,
	BU26507GUL_SW4LED1,
	BU26507GUL_SW5LED1,

	BU26507GUL_SW1LED2,
	BU26507GUL_SW2LED2,
	BU26507GUL_SW3LED2,
	BU26507GUL_SW4LED2,
	BU26507GUL_SW5LED2,

	BU26507GUL_SW1LED3,
	BU26507GUL_SW2LED3,
	BU26507GUL_SW3LED3,
	BU26507GUL_SW4LED3,
	BU26507GUL_SW5LED3,

	BU26507GUL_SW1LED4,
	BU26507GUL_SW2LED4,
	BU26507GUL_SW3LED4,
	BU26507GUL_SW4LED4,
	BU26507GUL_SW5LED4,

	BU26507GUL_SW1LED5,
	BU26507GUL_SW2LED5,
	BU26507GUL_SW3LED5,
	BU26507GUL_SW4LED5,
	BU26507GUL_SW5LED5,

	BU26507GUL_SW1LED6,
	BU26507GUL_SW2LED6,
	BU26507GUL_SW3LED6,
	BU26507GUL_SW4LED6,
	BU26507GUL_SW5LED6,
};

/*--------------------------------------------------------------------
Log output
--------------------------------------------------------------------*/
#define illumination_error_printk(format, arg...) \
  printk("%s" format, KERN_ERR, ##arg)

#ifdef CONFIG_LEDS_BU26507GUL_DEBUG

#define illumination_info2_printk(format, arg...) \
  printk("%s" format, KERN_INFO, ##arg)

#define illumination_info_printk(format, arg...)

#define illumination_debug_printk(format, arg...)
/* 
#define illumination_info_printk(format, arg...) \
  printk("%s" format, KERN_INFO, ##arg)

#define illumination_debug_printk(format, arg...) \
  printk("%s" format, KERN_DEBUG, ##arg)
*/

#else /* CONFIG_LEDS_BU26507GUL_DEBUG */
#define illumination_info2_printk(format, arg...)

#define illumination_info_printk(format, arg...)

#define illumination_debug_printk(format, arg...)
#endif 	/* CONFIG_LEDS_BU26507GUL_DEBUG */

#endif /* __BU26507GUL_INIT__ */
