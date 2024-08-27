







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
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/
/**
 * @file sii_hal_linux_isr.c
 *
 * @brief Linux implementation of interrupt support used by Silicon Image
 *        MHL devices.
 *
 * $Author: Dave Canfield
 * $Rev: $
 * $Date: Jan. 31, 2011
 *
 *****************************************************************************/

#define SII_HAL_LINUX_ISR_C

/***** #include statements ***************************************************/
#include "sii_hal.h"
#include "sii_hal_priv.h"
#include "si_c99support.h"
#include "si_osdebug.h"

//#include <linux/lnw_gpio.h>

/***** local macro definitions ***********************************************/

/***** local type definitions ************************************************/

/***** local variable declarations *******************************************/

/***** local function prototypes *********************************************/

/***** global variable declarations *******************************************/


/***** local functions *******************************************************/
bool iserroroccurred(void);

/*****************************************************************************/
/*
 *  @brief Interrupt handler for MHL transmitter interrupts.
 *
 *  @param[in]		irq		The number of the asserted IRQ line that caused
 *  						this handler to be called.
 *  @param[in]		data	Data pointer passed when the interrupt was enabled,
 *  						which in this case is a pointer to the
 *  						MhlDeviceContext of the I2c device.
 *
 *  @return     Always returns IRQ_HANDLED.
 *
 *****************************************************************************/
static irqreturn_t HalThreadedIrqHandler(int irq, void *data)
{
	pMhlDeviceContext	pMhlDevContext = (pMhlDeviceContext)data;


	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "HalThreadedIrqHandler called\n");
	if (iserroroccurred()) {
		return IRQ_HANDLED;
	}
	if (HalAcquireIsrLock() == HAL_RET_SUCCESS) {
		(pMhlDevContext->irqHandler)();
		HalReleaseIsrLock();
	}

	return IRQ_HANDLED;
}



/***** public functions ******************************************************/


/*****************************************************************************/
/**
 * @brief Install IRQ handler.
 *
 *****************************************************************************/
halReturn_t HalInstallIrqHandler(fwIrqHandler_t irqHandler)
{
	int				retStatus;
	halReturn_t 	halRet;

	if(irqHandler == NULL)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInstallIrqHandler: irqHandler cannot be NULL!\n");
		return HAL_RET_PARAMETER_ERROR;
	}

	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

	if(gMhlDevice.pI2cClient->irq == 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInstallIrqHandler: No IRQ assigned to I2C device!\n");
		return HAL_RET_FAILURE;
	}

	gMhlDevice.irqHandler = irqHandler;
	//RG addition
	gMhlDevice.pI2cClient->irq = gpio_to_irq(W_INT_GPIO);

//	lnw_gpio_set_alt(W_INT_GPIO, LNW_GPIO);
//	udelay(100);

	retStatus = request_threaded_irq(gMhlDevice.pI2cClient->irq,
						// gpio_to_irq(W_INT_GPIO),
						NULL,
						 HalThreadedIrqHandler,
									 IRQF_TRIGGER_LOW | IRQF_ONESHOT,
//						 IRQ_TYPE_EDGE_FALLING | IRQF_ONESHOT,
						 gMhlI2cIdTable[0].name,
						 &gMhlDevice);
	if(retStatus != 0)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalInstallIrqHandler: request_threaded_irq failed, status: %d\n",
    			retStatus);
		gMhlDevice.irqHandler = NULL;
		return HAL_RET_FAILURE;
	}

	return HAL_RET_SUCCESS;
}



/*****************************************************************************/
/**
 * @brief Remove IRQ handler.
 *
 *****************************************************************************/
halReturn_t HalRemoveIrqHandler(void)
{
	halReturn_t 	halRet;


	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS)
	{
		return halRet;
	}

	if(gMhlDevice.irqHandler == NULL)
	{
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"HalRemoveIrqHandler: no irqHandler installed!\n");
		return HAL_RET_FAILURE;
	}

	free_irq(gMhlDevice.pI2cClient->irq, &gMhlDevice);

	gMhlDevice.irqHandler = NULL;

	return HAL_RET_SUCCESS;
}

void enable_mhl_interrupt(void)
{
	if (gMhlDevice.pI2cClient != NULL)
		enable_irq(gMhlDevice.pI2cClient->irq);
}

void disable_mhl_interrupt(void)
{
	if (gMhlDevice.pI2cClient != NULL)
		disable_irq_nosync(gMhlDevice.pI2cClient->irq);
}



