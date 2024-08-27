








/*
 SiI8334 Linux Driver

 Copyright (C) 2011 Silicon Image Inc.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation version 2.

 This program is distributed .as is. WITHOUT ANY WARRANTY of any
 kind, whether express or implied; without even the implied warranty
 of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the
 GNU General Public License for more details.
*/
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012 - 2013
/*----------------------------------------------------------------------------*/
/**
 * @file sii_hal_linux_gpio.c
 *
 * @brief Linux implementation of GPIO pin support needed by Silicon Image
 *        MHL devices.
 *
 * $Author: Dave Canfield
 * $Rev: $
 * $Date: Feb. 9, 2011
 *
 *****************************************************************************/

#define SII_HAL_LINUX_GPIO_C

/***** #include statements ***************************************************/
#include "sii_hal.h"
#include "sii_hal_priv.h"
#include "si_c99support.h"
#include "si_osdebug.h"

#include <linux/gpio.h>
#include <linux/delay.h>
#include "si_bitdefs.h"


/***** local macro definitions ***********************************************/


/***** local type definitions ************************************************/


/***** local variable declarations *******************************************/


/***** local function prototypes *********************************************/


/***** global variable declarations *******************************************/


// Simulate the DIP switches
bool	pinDbgMsgs	= false;	// simulated pinDbgSw2 0=Print
//RG modification to allow for D3
//bool	pinAllowD3	= true;	// false allows debugging
bool	pinAllowD3	= false;	// false allows debugging
bool	pinOverrideTiming = true;	// simulated pinDbgSw2
bool	pinDataLaneL	= true;		// simulated pinDbgSw3
bool	pinDataLaneH	= true;		// simulated pinDbgSw4
bool	pinDoHdcp	= true;		// simulated pinDbgSw5
bool    pinWakePulseEn = true;   // wake pulses enabled by default

//	 Simulate the GPIO pins
bool	 pinTxHwReset	= false;	// simulated reset pin %%%% TODO possible on Beagle?
bool	 pinM2uVbusCtrlM	= true;		// Active high, needs to be low in MHL connected state, high otherwise.
bool	 pinVbusEnM	= true;		// Active high input. If high sk is providing power, otherwise not.

// Simulate the LEDs
bool	pinMhlConn	= true;		// MHL Connected LED 
bool	pinUsbConn	= false;	// USB connected LED
bool	pinSourceVbusOn	= true;		// Active low LED. On when pinMhlVbusSense and pinVbusEnM are active
bool	pinSinkVbusOn	= true;		// Active low LED. On when pinMhlVbusSense is active and pinVbusEnM is not active


/***** local functions *******************************************************/
#if defined(CONFIG_ARCH_APQ8064) || defined(CONFIG_MACH_ARC)
static int HalGpioRequestMhlPowerPin(void);
static void HalGpioFreeMhlPowerPin(void);
static void HalGpioSetMhlPowerPin(bool value);
#endif /* deined(CONFIG_ARCH_APQ8064) || defined(CONFIG_MACH_ARC) */

#if defined(CONFIG_ARCH_APQ8064)
void hdmi_gpio_config(int on);
void active_i2c_gpio(int on);
#endif /* deined(CONFIG_ARCH_APQ8064) */

/***** public functions ******************************************************/


/*****************************************************************************/
/**
 * @brief Configure platform GPIOs needed by the MHL device.
 *
 *****************************************************************************/
halReturn_t HalGpioInit(void)
{
	int status;

#if defined(CONFIG_ARCH_APQ8064) || defined(CONFIG_MACH_ARC)
	fj_SiiDebugPrint("%s\n", __func__);

	status = HalGpioRequestMhlPowerPin();
	if (status) {
		printk(KERN_ERR "init mhl power pin failed (%d)\n", status);
		return HAL_RET_FAILURE;
	}

	/* Configure GPIO used to perform a hard reset of the device. */
	status = gpio_request(W_RST_GPIO, "W_RST#");
	if (status) {
		printk(KERN_ERR "gpio_request failed on W_RST_GPIO (%d)\n", status);
		HalGpioFreeMhlPowerPin();
		return HAL_RET_FAILURE;
	}

#if defined(CONFIG_ARCH_APQ8064)
/* FUJITSU:2012-08-02 MHL start */
//	status = gpio_tlmm_config(GPIO_CFG(W_RST_GPIO, 0, GPIO_CFG_OUTPUT,
//		                      GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//	if (status) {
//		printk(KERN_ERR "gpio_tlmm_config failed on W_RST_GPIO (%d)\n", status);
//		HalGpioFreeMhlPowerPin();
//		gpio_free(W_RST_GPIO);
//		return HAL_RET_FAILURE;
//	}
/* FUJITSU:2012-08-02 MHL end */
#endif /* defined(CONFIG_ARCH_APQ8064) */
//	status = gpio_direction_output(W_RST_GPIO, 1);
	status = gpio_direction_output(W_RST_GPIO, 0);
	if (status) {
		printk(KERN_ERR "gpio_direction_output failed on W_RST_GPIO (%d)\n",
			                                                            status);
		gpio_set_value(W_RST_GPIO, 0);
		mdelay(1);
		HalGpioFreeMhlPowerPin();
		gpio_free(W_RST_GPIO);
		return HAL_RET_FAILURE;
	}

	/* Power on the MHL device. */
	gpio_set_value(W_RST_GPIO, 0);
	mdelay(1);
	HalGpioSetMhlPowerPin(HIGH);
	mdelay(1);
	gpio_set_value(W_RST_GPIO, 1);
	mdelay(10);

#if defined(CONFIG_ARCH_APQ8064)
	active_i2c_gpio(1);
#endif /* deined(CONFIG_ARCH_APQ8064) */

	/* Configure the GPIO used as an interrupt input from the device. */
	status = gpio_request(W_INT_GPIO, "W_INT");
	if (status) {
		printk(KERN_ERR "gpio_request failed on W_INT_GPIO (%d)\n", status);

#if defined(CONFIG_ARCH_APQ8064)
		active_i2c_gpio(0);
#endif /* deined(CONFIG_ARCH_APQ8064) */

		gpio_set_value(W_RST_GPIO, 0);
		mdelay(1);
		HalGpioFreeMhlPowerPin();
		gpio_free(W_RST_GPIO);
		return HAL_RET_FAILURE;
	}

#if defined(CONFIG_ARCH_APQ8064)
	status = gpio_tlmm_config(GPIO_CFG(W_INT_GPIO, 0, GPIO_CFG_INPUT,
		                      GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if (status) {
		printk(KERN_ERR "gpio_tlmm_config failed on W_INT_GPIO (%d)\n", status);

		active_i2c_gpio(0);

		gpio_set_value(W_RST_GPIO, 0);
		mdelay(1);
		HalGpioFreeMhlPowerPin();
		gpio_free(W_RST_GPIO);
		gpio_free(W_INT_GPIO);
		return HAL_RET_FAILURE;
	}
#endif /* defined(CONFIG_ARCH_APQ8064) */

	status = gpio_direction_input(W_INT_GPIO);
	if (status) {
		printk(KERN_ERR "gpio_direction_input failed on W_INT_GPIO (%d)\n",
			                                                            status);
#if defined(CONFIG_ARCH_APQ8064)
		active_i2c_gpio(0);
#endif /* deined(CONFIG_ARCH_APQ8064) */

		gpio_set_value(W_RST_GPIO, 0);
		mdelay(1);
		HalGpioFreeMhlPowerPin();
		gpio_free(W_RST_GPIO);
		gpio_free(W_INT_GPIO);
		return HAL_RET_FAILURE;
	}

#if defined(CONFIG_ARCH_APQ8064)
	hdmi_gpio_config(1);
#endif /* deined(CONFIG_ARCH_APQ8064) */
#else
	/* Configure GPIO used to perform a hard reset of the device. */
	status = gpio_request(W_RST_GPIO, "W_RST#");
	if (status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInit gpio_request for GPIO %d (H/W Reset) failed, status: %d\n",
    			W_RST_GPIO, status);
		return HAL_RET_FAILURE;
	}

	status = gpio_direction_output(W_RST_GPIO, 1);
	if (status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInit gpio_direction_output for GPIO %d (H/W Reset) failed, status: %d\n",
    			W_RST_GPIO, status);
		gpio_free(W_RST_GPIO);
		return HAL_RET_FAILURE;
	}

#ifndef MAKE_833X_DRIVER
	/* Configure GPIO used to control USB VBUS power. */
	status = gpio_request(M2U_VBUS_CTRL_M, "W_RST#");
	if (status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInit gpio_request for GPIO %d (VBUS) failed, status: %d\n",
    			M2U_VBUS_CTRL_M, status);
		return HAL_RET_FAILURE;
	}

	status = gpio_direction_output(M2U_VBUS_CTRL_M, 0);
	if (status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInit gpio_direction_output for GPIO %d (VBUS) failed, status: %d\n",
    			M2U_VBUS_CTRL_M, status);
		gpio_free(W_RST_GPIO);
		gpio_free(M2U_VBUS_CTRL_M);
		return HAL_RET_FAILURE;
	}
#endif	// MAKE_833X_DRIVER

	/*
	 * Configure the GPIO used as an interrupt input from the device
	 * NOTE: GPIO support should probably be initialized BEFORE enabling
	 * interrupt support
	 */
	status = gpio_request(W_INT_GPIO, "W_INT");
	if(status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInitGpio gpio_request for GPIO %d (interrupt)failed, status: %d\n",
    			W_INT_GPIO, status);
		gpio_free(W_RST_GPIO);
		return HAL_RET_FAILURE;
	}

	status = gpio_direction_input(W_INT_GPIO);
	if(status < 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInitGpio gpio_direction_input for GPIO %d (interrupt)failed, status: %d",
    			W_INT_GPIO, status);
		gpio_free(W_INT_GPIO);
		gpio_free(W_RST_GPIO);
#ifndef MAKE_833X_DRIVER
		gpio_free(M2U_VBUS_CTRL_M);
#endif
		return HAL_RET_FAILURE;
	}
#endif /* deined(CONFIG_ARCH_APQ8064) || defined(CONFIG_MACH_ARC) */
	return HAL_RET_SUCCESS;
}



/*****************************************************************************/
/**
 * @brief Release GPIO pins needed by the MHL device.
 *
 *****************************************************************************/
halReturn_t HalGpioTerm(void)
{
	halReturn_t 	halRet;

	fj_SiiDebugPrint("%s\n", __func__);

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

#if defined(CONFIG_ARCH_APQ8064)
	hdmi_gpio_config(0);
	active_i2c_gpio(0);
// FJFEAT_COMMENT_cut_start
/* FUJITSU:2013-03-06 MHL End */
#endif /* deined(CONFIG_ARCH_APQ8064) */

	gpio_set_value(W_RST_GPIO, 0);
	mdelay(1);

	/* Power off the MHL device. */
	HalGpioSetMhlPowerPin(LOW);
	mdelay(1);

	gpio_free(W_INT_GPIO);
	gpio_free(W_RST_GPIO);

#if 0
#ifndef MAKE_833X_DRIVER
	gpio_free(M2U_VBUS_CTRL_M);
#endif
#endif
	HalGpioFreeMhlPowerPin();

	return HAL_RET_SUCCESS;
}



/*****************************************************************************/
/**
 * @brief Platform specific function to control the reset pin of the MHL
 * 		  transmitter device.
 *
 *****************************************************************************/
halReturn_t HalGpioSetTxResetPin(bool value)
{
	halReturn_t 	halRet;

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

	gpio_set_value(W_RST_GPIO, value);
	return HAL_RET_SUCCESS;
}



/*****************************************************************************/
/**
 * @brief Platform specific function to control power on the USB port.
 *
 *****************************************************************************/
halReturn_t HalGpioSetUsbVbusPowerPin(bool value)
{
	halReturn_t 	halRet;

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

#if 0
	gpio_set_value(M2U_VBUS_CTRL_M, value);
#endif

	return HAL_RET_SUCCESS;
}



/*****************************************************************************/
/**
 * @brief Platform specific function to control Vbus power on the MHL port.
 *
 *****************************************************************************/
halReturn_t HalGpioSetVbusPowerPin(bool powerOn)
{
	halReturn_t 	halRet;

	halRet = HalInitCheck();
	if(halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
			"HalGpioSetVbusPowerPin called but this function is not implemented yet!\n");

	return HAL_RET_SUCCESS;
}

//RG addition to read INT GPIO status
/*****************************************************************************/
/**
 * @brief Platform specific function to control the reset pin of the MHL
 * 		  transmitter device.
 *
 *****************************************************************************/
int HalGpioGetTxIntPin()
{
	int	value;

	value = gpio_get_value(W_INT_GPIO);
	return value;
}

#if defined(CONFIG_ARCH_APQ8064) || defined(CONFIG_MACH_ARC)
static int HalGpioRequestMhlPowerPin(void)
{
	int status = 0;

	fj_SiiDebugPrint("%s\n", __func__);
	/* Configure GPIO used to perform a power on of the MHL device. */
#if defined(CONFIG_ARCH_APQ8064)
	status = gpio_request(GPIO_MHL_REG_ON, "gpio_mhl_reg_on");
	if (status) {
		printk(KERN_ERR "gpio_request failed on GPIO_MHL_REG_ON (%d)\n",
			                                                            status);
		return status;
	}
/* FUJITSU:2012-08-02 MHL start */
//	status = gpio_tlmm_config(GPIO_CFG(GPIO_MHL_REG_ON, 0, GPIO_CFG_OUTPUT,
//		                      GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//	if (status) {
//		printk(KERN_ERR "gpio_tlmm_config failed on GPIO_MHL_REG_ON (%d)\n",
//			                                                            status);
//		gpio_free(GPIO_MHL_REG_ON);
//		return status;
//	}
/* FUJITSU:2012-08-02 MHL end */
#elif defined(CONFIG_MACH_ARC)
	status = gpio_request(W_MHL_PON12, "gpio_mhl_reg_on");
	if (status) {
		printk(KERN_ERR "gpio_request failed on W_MHL_PON12 (%d)\n", status);
		return status;
	}

	status = gpio_direction_output(W_MHL_PON12, 0);
	if (status) {
		printk(KERN_ERR "gpio_direction_output failed on W_MHL_PON12 (%d)\n",
			                                                            status);
		gpio_free(W_MHL_PON12);
		return status;
	}

	status = gpio_request(W_MHL_PON18, "gpio_mhl_reg_on");
	if (status) {
		printk(KERN_ERR "gpio_request failed on W_MHL_PON18 (%d)\n", status);
		gpio_free(W_MHL_PON12);
		return status;
	}

	status = gpio_direction_output(W_MHL_PON18, 0);
	if (status) {
		printk(KERN_ERR "gpio_direction_output failed on W_MHL_PON18 (%d)\n",
			                                                            status);
		gpio_free(W_MHL_PON12);
		gpio_free(W_MHL_PON18);
		return status;
	}
#endif

	HalGpioSetMhlPowerPin(LOW);
	return status;
}

static void HalGpioFreeMhlPowerPin(void)
{
	fj_SiiDebugPrint("%s\n", __func__);

	HalGpioSetMhlPowerPin(LOW);
	mdelay(1);

#if defined(CONFIG_ARCH_APQ8064)
	gpio_free(GPIO_MHL_REG_ON);
#elif defined(CONFIG_MACH_ARC)
	gpio_free(W_MHL_PON12);
	gpio_free(W_MHL_PON18);
#endif
}

static void HalGpioSetMhlPowerPin(bool value)
{
	fj_SiiDebugPrint("%s, value=%d\n", __func__, (int)value);
#if defined(CONFIG_ARCH_APQ8064)
	gpio_set_value(GPIO_MHL_REG_ON, value);
#elif defined(CONFIG_MACH_ARC)
	gpio_set_value(W_MHL_PON12, value);
	gpio_set_value(W_MHL_PON18, value);
#endif
}
#endif /* CONFIG_ARCH_APQ8064) || defined(CONFIG_MACH_ARC) */



