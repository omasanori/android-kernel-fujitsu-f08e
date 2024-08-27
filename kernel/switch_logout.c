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

#include <linux/switch_logout.h>

#ifdef SWITCH_LOGOUT

#include <linux/kernel.h>
#include <linux/nonvolatile_common.h>

#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/mfd/pm8xxx/mpp.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
extern void clock_dump_info(void);
extern void rmp_vreg_dump_info(void);
extern void bu1852_dbg_dump(void);


#define APNV_KERNEL_DEBUGLOG_SWICHFLAG_I   41054 // APP_NV_ITEM

static unsigned int swich_bitmap = 0;
static int is_gettable  = 0;


unsigned int check_and_get_table_from_NV(void)
{
	uint32_t out_buf;
	
	if (!is_gettable) {
		// Get Switch table Value.
		if (get_nonvolatile((void*)&out_buf, APNV_KERNEL_DEBUGLOG_SWICHFLAG_I, sizeof(out_buf)) > 0) {
			is_gettable = 1;
			swich_bitmap = out_buf;
		}
	}

	return swich_bitmap;
}

// Switch log is Output.
void switch_printk(unsigned int logtype, const char *fmt, ...)
{
	va_list args;

	// Check & Get Switch table.
	check_and_get_table_from_NV();

	// Check Get Switch table..
	if (is_gettable) {
		if (swich_bitmap & logtype) {
			va_start(args, fmt);
			vprintk(fmt, args);
			va_end(args);
		}
	}
}

// GPIO log is Output.
void switch_gpio_dump(unsigned int detail)
{
	// Check & Get Switch table.
	check_and_get_table_from_NV();

	if (swich_bitmap & GpioLogType) {
		//: show msm gpio.
		msm_gpio_dbg_out();
		//: show pmic gpio.
		pm8xxx_gpio_dbg_out(detail);
		//: show mpp gpio.
		pm8xxx_mpp_dbg_out(detail);
		//: show gpio-exp.
		bu1852_dbg_dump();
	}
}

// GPIO log is Output.
void switch_gpio_dump_op(void)
{
	check_and_get_table_from_NV();

	if (swich_bitmap & GpioLogType) {
		//: show pmic gpio.
		pm8xxx_gpio_dbg_out(1);
		//: show mpp gpio.
		pm8xxx_mpp_dbg_out(1);
		//: show gpio-exp.
		bu1852_dbg_dump();
	}
}

void switch_powercollapse_dump(void)
{
	check_and_get_table_from_NV();


	if (swich_bitmap & ClockLogType) {
		clock_dump_info();
	}
	if (swich_bitmap & RegulatorLogType) {
		rmp_vreg_dump_info();
	}
	if (swich_bitmap & GpioLogType) {
		//: show msm gpio.
		msm_gpio_dbg_out();
	}
}

#endif
