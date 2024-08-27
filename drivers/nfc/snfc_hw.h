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
#ifndef __NFC_HW__
#define __NFC_HW__

#include <linux/gpio.h>
#include <asm/system.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <../board-8064.h>

#define NFCCTL_GPIO_UART_TX	6
#define NFCCTL_GPIO_UART_RX	7

#define NFCCTL_GPIO_PON		BU1852_EXP1_GPIO_BASE + 8
#define NFCCTL_GPIO_RFS		45
#define NFCCTL_GPIO_HSEL	BU1852_EXP1_GPIO_BASE + 9
#define NFCCTL_GPIO_INTU	38
#define NFCCTL_GPIO_HVDD	PM8921_MPP_PM_TO_SYS(2)
#define NFCCTL_GPIO_INT		77
#define NFCCTL_GPIO_CEN_DATA PM8921_GPIO_PM_TO_SYS(10)
#define NFCCTL_GPIO_CEN_CLK  PM8921_GPIO_PM_TO_SYS(11)



#define NFCCTL_GPIO_MDM2AP	49

#define SNFC_HW_LOW  0
#define SNFC_HW_HIGH 1

enum snfc_hw_id {
	SNFC_HW_CEN = 0,
	SNFC_HW_PON,
	SNFC_HW_RFS,
	SNFC_HW_INTU,
	SNFC_HW_HSEL,
	SNFC_HW_POLL,
	SNFC_HW_POW,

	SNFC_HW_NUM
};


/** cen ******/
extern int snfc_hw_cen_init(void);
extern int snfc_hw_cen_exit(void);
extern int snfc_hw_cen_write(int val);
extern int snfc_hw_cen_read(void);

/** pon ******/
extern int snfc_hw_pon_init(void);
extern int snfc_hw_pon_exit(void);
static inline int snfc_hw_pon_write(int val) {
	gpio_set_value_cansleep(NFCCTL_GPIO_PON, val);
	return 0;
}
static inline int snfc_hw_pon_read(void) { return gpio_get_value_cansleep(NFCCTL_GPIO_PON); }

/** rfs ******/
#define snfc_hw_rfs_req_irq( handler ) snfc_request_irq(NFCCTL_GPIO_RFS, handler, IRQF_TRIGGER_RISING, "snfc_rfs_irq")
#define snfc_hw_rfs_irq_enable() snfc_hw_irq_enable(gpio_to_irq(NFCCTL_GPIO_RFS))
#define snfc_hw_rfs_irq_disable() snfc_hw_irq_disable(gpio_to_irq(NFCCTL_GPIO_RFS))

extern int snfc_hw_rfs_init(void);
extern int snfc_hw_rfs_exit(void);
static inline int snfc_hw_rfs_read(void) { return gpio_get_value(NFCCTL_GPIO_RFS); }
static inline int snfc_hw_rfs_free_irq(void) {
	free_irq(gpio_to_irq(NFCCTL_GPIO_RFS), 0);
	return 0;
}

/** hsel ******/
extern int snfc_hw_hsel_init(void);
extern int snfc_hw_hsel_exit(void);
static inline int snfc_hw_hsel_write(int val) { 
	gpio_set_value_cansleep(NFCCTL_GPIO_HSEL, val);
	return 0;
}
static inline int snfc_hw_hsel_read(void) { return gpio_get_value_cansleep(NFCCTL_GPIO_HSEL);}

/** intu ******/
#define snfc_hw_intu_req_irq( handler) snfc_request_irq(NFCCTL_GPIO_INTU, handler, IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, "snfc_intu_irq")
#define snfc_hw_intu_irq_enable() snfc_hw_irq_enable(gpio_to_irq(NFCCTL_GPIO_INTU))
#define snfc_hw_intu_irq_disable() snfc_hw_irq_disable(gpio_to_irq(NFCCTL_GPIO_INTU))

extern int snfc_hw_intu_init(void);
extern int snfc_hw_intu_exit(void);
static inline int snfc_hw_intu_read(void) { return gpio_get_value(NFCCTL_GPIO_INTU); }
static inline int snfc_hw_intu_free_irq(void) {
	free_irq(gpio_to_irq(NFCCTL_GPIO_INTU), 0);
	return 0;
}

/** available polling ******/
extern int snfc_hw_available_init(void);
extern int snfc_hw_available_exit(void);
extern int snfc_hw_power_observe_init(void);
extern int snfc_hw_power_observe_exit(void);

/** others ******/
extern int snfc_hw_hvdd_read(void);
extern int snfc_hw_hvdd_write(int val);
static inline int snfc_hw_mstatus_read(void) { return gpio_get_value(NFCCTL_GPIO_MDM2AP); }

/** common ***/
extern int snfc_request_irq(int port, irq_handler_t handler, unsigned long flags, const char *name);
extern int snfc_hw_irq_enable(int irq);
extern int snfc_hw_irq_disable(int irq);
extern int snfc_hw_gpio_config(void);

#endif /* __NFC_HWL__ */

