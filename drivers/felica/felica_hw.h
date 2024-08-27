/*
 * linux/drivers/felica/felica_hw.h
 */
/*------------------------------------------------------------------------
 * Copyright(C) 2012-2013 FUJITSU LIMITED
 *------------------------------------------------------------------------
 */
#ifndef __FELICA_HW__
#define __FELICA_HW__

#include <linux/gpio.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <../board-8064.h>
#include <linux/irq.h>
#include <linux/delay.h>

#define FCCTL_GPIO_UART_TX	06
#define FCCTL_GPIO_UART_RX	07

#define FCCTL_PMGPIO_CEN_D		10
#define FCCTL_PMGPIO_CEN_CLK	11

#define FCCTL_ExGPIO_PON	(BU1852_EXP1_GPIO_BASE + 8)
#define FCCTL_GPIO_INT		77
#define FCCTL_GPIO_RFS		45

#define FELICA_HW_LOW		0
#define FELICA_HW_HIGH		1

#define FELICA_POWER_OFF	FELICA_HW_LOW
#define FELICA_POWER_ON		FELICA_HW_HIGH

#define FELICA_LOCK			FELICA_HW_LOW
#define FELICA_UNLOCK		FELICA_HW_HIGH

/** pon ******/
extern int felica_pon_hw_init(void);
extern int felica_pon_hw_exit(void);
extern int felica_pon_hw_close(void);
extern int felica_pon_hw_write(int val);

/** cen ******/
extern int felica_cen_hw_init(void);
extern int felica_cen_hw_exit(void);
extern int felica_cen_hw_write(int val);
extern int felica_cen_hw_read(void);

/** rfs ******/
extern int felica_rfs_hw_init(void);
extern int felica_rfs_hw_exit(void);
extern int felica_rfs_hw_read(void);

/** int ******/
extern int felica_int_hw_init(void);
extern int felica_int_hw_init_irq(irq_handler_t handler);
extern int felica_int_hw_exit(void);
extern int felica_int_hw_open(void);
extern int felica_int_hw_close(void);
extern int felica_int_hw_read(void);

#endif /* __FELICA_HW__ */
