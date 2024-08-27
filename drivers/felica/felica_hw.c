/*
 * linux/drivers/felica/felica_pon_hw.c
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
/*------------------------------------------------------------------------
 * Copyright(C) 2012-2013 FUJITSU LIMITED
 *------------------------------------------------------------------------
 */

#include "felica_ioctl.h"
#include "felica_hw.h"

#define DEV_NAME_PON	"felica_pon"
#define DEV_NAME_CEN	"felica_cen"
#define DEV_NAME_RFS	"felica_rfs"
#define DEV_NAME_INT	"felica_int"

/* PMGPIO_CEN_D Config */
struct pm_gpio felica_pm_gpio_cen_d = {
 .direction      = PM_GPIO_DIR_OUT,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_L17,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

/* PMGPIO_CEN_CLK Config */
struct pm_gpio felica_pm_gpio_cen_clk = {
 .direction      = PM_GPIO_DIR_OUT,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_L17,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

/** for pon *********************/
int felica_pon_hw_init(void)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_PON ":hw_init() [IN]\n");
#endif
	ret = gpio_request(FCCTL_ExGPIO_PON, DEV_NAME_PON);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_PON ":gpio_request failed[%d]\n", ret);
		return ret;
	}

	ret = gpio_direction_output(FCCTL_ExGPIO_PON, 0);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_PON
			":gpio_direction_output failed[%d]\n", ret);
		gpio_free(FCCTL_ExGPIO_PON);
		return ret;
	}

	gpio_set_value_cansleep(FCCTL_ExGPIO_PON, FELICA_HW_LOW);
	return 0;
}

int felica_pon_hw_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_PON ":hw_exit() [IN]\n");
#endif
	gpio_free(FCCTL_ExGPIO_PON);
	return 0;
}

int felica_pon_hw_close(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_PON ":hw_close() [IN]\n");
#endif
	gpio_set_value_cansleep(FCCTL_ExGPIO_PON, FELICA_HW_LOW);
	return 0;
}

int felica_pon_hw_write(int val)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_PON ":hw_write() [IN]\n");
#endif
	gpio_set_value_cansleep(FCCTL_ExGPIO_PON, val);
	return 0;
}


/** for cen *********************/
int felica_cen_hw_init(void)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_CEN ":hw_init() [IN]\n");
#endif
	ret = gpio_request(
			PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_D), "PMGPIO_CEN_D");
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_CEN
			":PM8921_CEN EXT_EN1 gpio_request failed[%d]\n", ret);
		return ret;
	}
	ret = gpio_request(
			PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK), "PMGPIO_CEN_CLK");
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_CEN
			":PM8921_CEN EXT_EN2 gpio_request failed[%d]\n", ret);
		goto gpio_init_err1;
	}

	ret = pm8xxx_gpio_config(
			PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_D),
			&felica_pm_gpio_cen_d);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR DEV_NAME_CEN ":PM8921_CEN EXT_EN1 config failed\n");
		ret = -EIO;
		goto gpio_init_err2;
	}
	ret = pm8xxx_gpio_config(
			PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK),
			&felica_pm_gpio_cen_clk);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR DEV_NAME_CEN ":PM8921_CEN EXT_EN2 config failed\n");
		ret = -EIO;
		goto gpio_init_err2;
	}

	ret = gpio_direction_output(PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_D), 0);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_CEN
			":PM8921_CEN EXT_EN1 gpio_direction_output failed[%d]\n", ret);
		goto gpio_init_err2;
	}
	ret = gpio_direction_output(PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK), 0);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_CEN
			":PM8921_CEN EXT_EN2 gpio_direction_output failed[%d]\n", ret);
		goto gpio_init_err2;
	}

#ifdef FELICA_IS_UNLOCKED
	/* FELICA FUNCTION is ALWAYS ENABLED */
	printk(KERN_INFO DEV_NAME_CEN ":FeliCa_DEBUG(FeliCa UNLOCK) START ===>\n");
	felica_cen_hw_write(FELICA_HW_HIGH);
	printk(KERN_INFO DEV_NAME_CEN ":FeliCa_DEBUG(FeliCa UNLOCK) END   <===\n");
#endif
	return 0;

gpio_init_err2:
	gpio_free(PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK));
gpio_init_err1:
	gpio_free(PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_D));
	return ret;
}

int felica_cen_hw_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_CEN ":hw_exit() [IN]\n");
#endif
	gpio_free(PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK));
	gpio_free(PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_D));

	return 0;
}

int felica_cen_hw_write(int val)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_CEN ":hw_write() [IN]\n");
#endif

	ret = gpio_get_value_cansleep(PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK));
	if (unlikely(ret > 0)) {
		gpio_set_value_cansleep(
			PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK), FELICA_HW_LOW);
		udelay(5);
	} else if(unlikely(ret < 0)) {
		printk(KERN_ERR DEV_NAME_CEN
			":PM8921_CEN EXT_EN2 read failed[%d]\n", ret);
		return ret;
	}

	gpio_set_value_cansleep(
		PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_D), val);
	udelay(5);

	gpio_set_value_cansleep(
		PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK), FELICA_HW_HIGH);
	udelay(5);

	gpio_set_value_cansleep(
		PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_CLK), FELICA_HW_LOW);

	/* FUJITSU:2013-04-11 add_start */
	ret = gpio_tlmm_config( GPIO_CFG(FCCTL_GPIO_RFS, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if ( ret ) {
		printk(KERN_ERR "%s: FEL_VRO config failed\n", __func__ );
	}

	ret = gpio_tlmm_config( GPIO_CFG(FCCTL_GPIO_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if ( ret ) {
		printk(KERN_ERR "%s: FEL_IBO_3 config failed\n", __func__ );
	}
	/* FUJITSU:2013-04-11 add_end */

	return 0;
}

int felica_cen_hw_read(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_CEN ":hw_read() [IN]\n");
#endif
	return gpio_get_value_cansleep(PM8921_GPIO_PM_TO_SYS(FCCTL_PMGPIO_CEN_D));
}

/** for rfs *********************/
int felica_rfs_hw_init(void)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_RFS ":hw_init() [IN]\n");
#endif
	ret = gpio_request(FCCTL_GPIO_RFS, DEV_NAME_RFS);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_RFS ":gpio_request failed[%d]\n", ret);
		return ret;
	}

	ret = gpio_direction_input(FCCTL_GPIO_RFS);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_RFS
			":gpio_direction_input failed[%d]\n", ret);
		gpio_free(FCCTL_GPIO_RFS);
		return ret;
	}

	/* FUJITSU:2013-04-11 mod_start */
	ret = gpio_tlmm_config(
			GPIO_CFG(FCCTL_GPIO_RFS, 0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	/* FUJITSU:2013-04-11 mod_end */

	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_RFS ":FeliCa FEL_VRO config failed\n");
		gpio_free(FCCTL_GPIO_RFS);
		return -EIO;
	}

	return ret;
}

int felica_rfs_hw_exit(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_RFS ":hw_exit() [IN]\n");
#endif
	gpio_free(FCCTL_GPIO_RFS);
	return 0;
}

int felica_rfs_hw_read(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_RFS ":hw_read() [IN]\n");
#endif
	return gpio_get_value(FCCTL_GPIO_RFS);
}

/** for int *********************/
int felica_int_hw_init(void)
{
	int ret = 0;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_INT ":hw_init() [IN]\n");
#endif
	ret = gpio_request(FCCTL_GPIO_INT, DEV_NAME_INT);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_INT ":gpio_request failed[%d]\n", ret);
		return ret;
	}

	ret = gpio_direction_input(FCCTL_GPIO_INT);
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_INT
			":gpio_direction_input failed[%d]\n", ret);
		gpio_free(FCCTL_GPIO_INT);
		return ret;
	}

	/* FUJITSU:2013-04-11 mod_start */
	ret = gpio_tlmm_config(
			GPIO_CFG(FCCTL_GPIO_INT, 0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	/* FUJITSU:2013-04-11 mod_end */
	if (unlikely(ret)) {
		printk(KERN_ERR DEV_NAME_INT ":Felica FEL_IBO_3 config failed\n");
		gpio_free(FCCTL_GPIO_INT);
		return -EIO;
	}
	/* FUJITSU:2013-04-11 add_start */
	ret = gpio_request(FCCTL_GPIO_UART_TX, "Felica_uart_tx");
	if (unlikely(!ret)) {
		gpio_free(FCCTL_GPIO_UART_TX);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", FCCTL_GPIO_UART_TX );
		return ret;
	}

	ret = gpio_request(FCCTL_GPIO_UART_RX, "Felica_uart_rx");
	if (unlikely(!ret)) {
		gpio_free(FCCTL_GPIO_UART_RX);
	} else if(unlikely(ret != -EBUSY)) {
		printk(KERN_ERR "gpio_request[%d] failed.\n", FCCTL_GPIO_UART_RX );
		return ret;
	}
	ret = gpio_tlmm_config( GPIO_CFG(FCCTL_GPIO_UART_TX, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	if ( ret ) {
		printk(KERN_ERR "%s: Felica UART TXD config failed\n", __func__ );
	}

	/* FUJITSU:2013-05-31 mod_start */
	ret = gpio_tlmm_config( GPIO_CFG(FCCTL_GPIO_UART_RX, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	/* FUJITSU:2013-05-31 mod_end */
	if ( ret ) {
		printk(KERN_ERR "%s: Felica UART RXD config failed\n", __func__ );
	}
	/* FUJITSU:2013-04-11 add_end */

	return 0;
}

int felica_int_hw_init_irq(irq_handler_t handler)
{
	unsigned int irq;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_INT ":hw_init_irq() [IN]\n");
#endif
	irq = gpio_to_irq(FCCTL_GPIO_INT);
	if (unlikely(request_irq(irq, handler,
					IRQF_TRIGGER_FALLING,
					"felica_irq", 0))) {
		printk(KERN_ERR DEV_NAME_INT ":request_irq err.\n");
		gpio_free(FCCTL_GPIO_INT);
		return -EINVAL;
	}

	disable_irq(irq);

	return 0;
}

int felica_int_hw_exit(void)
{
	unsigned int irq;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_INT ":hw_exit() [IN]\n");
#endif
	irq = gpio_to_irq(FCCTL_GPIO_INT);
	free_irq(irq, 0);
	gpio_free(FCCTL_GPIO_INT);
	return 0;
}

int felica_int_hw_open(void)
{
	unsigned int irq;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_INT ":hw_open() [IN]\n");
#endif
	irq = gpio_to_irq(FCCTL_GPIO_INT);
	enable_irq(irq);
	if (unlikely(enable_irq_wake(irq))) {
		printk(KERN_ERR DEV_NAME_INT ":enable_irq_wake err.\n");
		return -EINVAL;
	}

	return 0;
}

int felica_int_hw_close(void)
{
	unsigned int irq;

#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_INT ":hw_close() [IN]\n");
#endif
	irq = gpio_to_irq(FCCTL_GPIO_INT);
	disable_irq_wake(irq);
	disable_irq(irq);

	return 0;
}

int felica_int_hw_read(void)
{
#ifdef FELICA_DEBUG_SUPPORT
	printk(KERN_INFO DEV_NAME_INT ":hw_read() [IN]\n");
#endif
	return gpio_get_value(FCCTL_GPIO_INT);
}
