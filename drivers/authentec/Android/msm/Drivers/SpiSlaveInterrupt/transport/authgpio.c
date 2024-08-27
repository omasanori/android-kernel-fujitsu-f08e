 /* /=======================================================================\
  * |                  AuthenTec Embedded (AE) Software                     |
  * |                                                                       |
  * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
  * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
  * |                                                                       |
  * |    Copyright (C) 2006-2011, AuthenTec, Inc. - All Rights Reserved.    |
  * |                                                                       |
  * |  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF    |
  * |  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO  |
  * |  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A       |
  * |  PARTICULAR PURPOSE.                                                  |
  * \=======================================================================/
  */

/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/

/* *************************************************************************

   File: 			AuthGPIO.c

   Description:		This file contains GPIO port specific code
   					      for use with the AuthenTec Unified Driver Model.

                    The AuthGpio functions are responsible for handling the
                    OS and processor specific function calls that control
                    general purpose I/O. GPIO's are typically used to handle
                    interrupts, sensor power, sensor reset, and low power
                    modes such as sleep. The tsAUTHGPIO structure within the
                    driver context is used to store all required state
                    information and variables for GPIO's.

   Author:			James Deng

   ************************************************************************* */

/** include files **/
/* FUJITSU:2011-12-12 FingerPrintSensor add start */
#include <linux/wakelock.h>
/* FUJITSU:2011-12-12 FingerPrintSensor add end */
#include "hfiles.h"

/** local definitions **/
/* FUJITSU:2011-12-12 FingerPrintSensor add start */
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + NR_GPIO_IRQS)
/* FUJITSU:2012-06-25 for mod start */
/* GPIO_20 -> GPIO_20 */
#define PM8921_FGR_REG18_ON_20 (20)
/* FUJITSU:2012-06-25 for  mod end */

/* FUJITSU:2011-12-12 FingerPrintSensor add end */
/** local definitions **/

/** default settings **/

/** external functions **/
/* FUJITSU:2012-7-12 nab add-s */
void spi_env_ctrl(int runtype/* 0:off, 1:on, 2:shutdown */, int csno);
#define SPI_CONTROL_OFF       0
#define SPI_CONTROL_ON        1
#define SPI_CONTROL_SHUTDOWN  2
/* FUJITSU:2012-7-12 nab add-e */

#define SPI_CLIENT_FPS (TRANSPORT_IO_SPI_CS)

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/
/* FUJITSU:2011-12-12 FingerPrintSensor add start */
struct wake_lock stWakeLock;
/* FUJITSU:2011-12-12 FingerPrintSensor add end */
/** private functions **/

/** public functions **/

/*++
===============================================================================

Function AuthGpioInit

The AuthGpioInit() function configures GPIO port pins for use with a specific
device driver implementation.

Parameters
lptsAuthGPIO (input/output) Pointer to the GPIO context.

lptsDrvInfo (input)         Pointer to the tsDRVINFO struct, contains
                            info about the swipe buffer, interface,
                            sensor, and compiled options.

Return Value                Returns a TRUE to indicate that the GPIO's have
                            been configured, FALSE otherwise.

===============================================================================
--*/

BOOL
AuthGpioInit (lptsAUTHGPIO lptsAuthGPIO)
{
    int iRet = 0;
    int rc = 0;

/* FUJITSU:2011-12-12 FingerPrintSensor add start */
    debug_printk("%s: AuthGpioInit\n", __func__);

/* FUJITSU:2012-06-25 for mod start */
/* GPIO[36] -> GPIO[86] */
    /* spi_cs1 gpio mode */
//  rc = gpio_tlmm_config(GPIO_CFG(36, 0, GPIO_CFG_OUTPUT
    rc = gpio_tlmm_config(GPIO_CFG(86, 0, GPIO_CFG_OUTPUT
                , GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
    if (rc != 0) {
//      DBGERROR((L("%s: GPIO_36 config = %d\n"), __func__, rc));
        DBGERROR((L("%s: GPIO_86 config = %d\n"), __func__, rc));
    }

    /* spi_cs1 = LOW */
//  gpio_set_value(36, 0);
    gpio_set_value(86, 0);

    spi_env_ctrl(SPI_CONTROL_OFF, SPI_CLIENT_FPS);
    gpio_set_value(86, 0);

#ifdef _DEBUG_PROC_
//  rc = gpio_get_value(36);
    rc = gpio_get_value(86);
//  debug_printk("%s: GPIO_36 = %d\n", __func__, rc);
    debug_printk("%s: GPIO_86 = %d\n", __func__, rc);
#endif /* _DEBUG_PROC_ */
/* FUJITSU:2012-06-25 for mod end */

    wake_lock_init(&stWakeLock, WAKE_LOCK_SUSPEND, "fpsWakeLock");
/* FUJITSU:2011-12-12 FingerPrintSensor add end */

#ifdef SENSOR_GPIO_INT
    iRet = gpio_request(SENSOR_GPIO_INT, "fp_int");
    iRet = gpio_direction_input(SENSOR_GPIO_INT);
    //iRet = gpio_get_value(SENSOR_GPIO_INT);
#endif

#ifdef SENSOR_GPIO_I2C_ADDRESS
    iRet = gpio_request(SENSOR_GPIO_I2C_ADDRESS, "fp_i2c_addr");
    iRet = gpio_direction_output(SENSOR_GPIO_I2C_ADDRESS, 0);
#endif

#ifdef SENSOR_GPIO_I2C_DR
    iRet = gpio_request(SENSOR_GPIO_I2C_DR, "fp_i2c_dr");
    iRet = gpio_direction_output(SENSOR_GPIO_I2C_DR, 1);
#endif

    return TRUE;
}


/*++
===============================================================================

Function AuthGpioDeinit

The AuthGpioDeinit() function frees up GPIO resources for use with a specific
device driver implementation.

Parameters
lptsAuthGPIO (input)	    Pointer to the GPIO context.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void
AuthGpioDeinit (lptsAUTHGPIO	lptsAuthGPIO)
{
/* FUJITSU:2011-12-12 FingerPrintSensor add start */
    debug_printk("%s: AuthGpioDeinit\n", __func__);
/* FUJITSU:2011-12-12 FingerPrintSensor add end */

#ifdef SENSOR_GPIO_INT
    gpio_free(SENSOR_GPIO_INT);
#endif

#ifdef SENSOR_GPIO_I2C_ADDRESS
    gpio_free(SENSOR_GPIO_I2C_ADDRESS);
#endif

#ifdef SENSOR_GPIO_I2C_DR
    gpio_free(SENSOR_GPIO_I2C_DR);
#endif

    return;

}


/*++
===============================================================================

Function AuthGpioReset

The AuthGpioReset() function asserts to the sensor's reset pin, holding it
in a reset state.

Parameters
lptsAuthGPIO (input)	    Pointer to the GPIO context.

Return Value                TRUE if the RESET GPIO is implemented,
                            FALSE otherwise.

===============================================================================
--*/

void
AuthGpioReset (lptsAUTHGPIO	lptsAuthGPIO)
{
    return;
}



/*++
===============================================================================

Function AuthGpioSuspend

The AuthGpioSuspend () stops the IO clocks and setup wakeup interrupt.

Parameters
lpvContext (input)	        Pointer to the driver context.

Return Value                Returns a TRUE to indicate that the GPIO's
                            works fine, FALSE otherwise.

===============================================================================
--*/
BOOL
AuthGpioSuspend (lptsAUTHGPIO lptsAuthGPIO)
{
    return TRUE;
}


/*++
===============================================================================

Function AuthGpioPowerOn

The AuthGpioPowerOn () powers on/off the sensor.

Parameters
lptsAuthGPIO (input)	        Pointer to the GPIO context.

Return Value                Returns a TRUE to indicate success, FALSE otherwise.

===============================================================================
--*/
BOOL
AuthGpioPowerOn (lptsAUTHGPIO lptsAuthGPIO, BOOL bON)
{
/* FUJITSU:2011-12-12 FingerPrintSensor add start */
    int rc = 0;
/* FUJITSU:2011-12-12 FingerPrintSensor add end */

    DBPRINT((L("AuthGpioPowerOn: called with bON %d\n"), bON));

/* FUJITSU:2011-12-12 FingerPrintSensor add start */
    if (bON == TRUE) {

		printk("AuthGpioPower ON\n");		/* FUJITSU:2012-10-04 FingerPrintSensor_Log mod */

        wake_lock(&stWakeLock);

        /* pm8921_GPIO 20 read */
        rc = gpio_get_value_cansleep(
                            PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20));

        debug_printk("%s: PM8921_FGR_REG18_ON GPIO_%d = %d\n"
                ,__func__,PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20), rc);

        if (rc == 0) {
/* FUJITSU:2012-09-25 SPI_EXCLUSIVE_ACCESS_CONTROL add start */
			gpio_set_value(86, 1);
/* FUJITSU:2012-09-25 SPI_EXCLUSIVE_ACCESS_CONTROL add end */

            /* pm8921_GPIO 20 = "H". finger print sensor power on */
            gpio_set_value_cansleep(
                            PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20), 1);

            debug_printk("%s: PM8921_FGR_REG18_ON GPIO_%d -> High\n"
                    ,__func__,PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20));

#ifdef _DEBUG_PROC_
            /* pm8921_GPIO 20 read */
            rc = gpio_get_value_cansleep(
                            PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20));

            debug_printk("%s: PM8921_FGR_REG18_ON GPIO_%d = %d\n"
            , __func__, PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20), rc);
#endif /* _DEBUG_PROC_ */

/* FUJITSU:2012-06-25 for mod start */
#if 0
            /* 3m wait */
            AuthOsSleep(3);
#else
            /* 3m -> 10m wait */ // FROM TATOOINE
            AuthOsSleep(10);
#endif
/* FUJITSU:2012-06-25 for mod end */

        }
#ifdef _DEBUG_PROC_
        else{
            debug_printk("%s: already power on\n", __func__);
        }
#endif /* _DEBUG_PROC_ */

        /* msm8960 GPIO 23 set no pull */
        rc = gpio_tlmm_config(GPIO_CFG(SENSOR_GPIO_INT, 0, GPIO_CFG_INPUT
                        , GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if (rc != 0) {
            DBGERROR((L("%s: MSM8960_SENSOR_GPIO_INT config failed\n")
                                                                , __func__));
        }

/* FUJITSU:2012-06-25 for mod start */
/* GPIO[36] -> GPIO[86] */
        /* spi_cs1 chipselect mode */
//      rc = gpio_tlmm_config(GPIO_CFG(36, 4, GPIO_CFG_OUTPUT
/* FUJITSU:2012-09-25 SPI_EXCLUSIVE_ACCESS_CONTROL mod start */
//      rc = gpio_tlmm_config(GPIO_CFG(86, 4, GPIO_CFG_OUTPUT
        rc = gpio_tlmm_config(GPIO_CFG(86, 0, GPIO_CFG_OUTPUT
/* FUJITSU:2012-09-25 SPI_EXCLUSIVE_ACCESS_CONTROL mod end */
                       , GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if (rc != 0) {
//          DBGERROR((L("%s: GPIO_36 config = %d\n"), __func__, rc));
            DBGERROR((L("%s: GPIO_86 config = %d\n"), __func__, rc));
        }

        spi_env_ctrl(SPI_CONTROL_ON, SPI_CLIENT_FPS);

/* FUJITSU:2012-09-25 SPI_EXCLUSIVE_ACCESS_CONTROL del start */
//    	gpio_set_value(86, 0);
/* FUJITSU:2012-09-25 SPI_EXCLUSIVE_ACCESS_CONTROL del end */

    }else{
		printk("AuthGpioPower OFF\n");		/* FUJITSU:2012-10-04 FingerPrintSensor_Log mod */

        /* spi_cs1 gpio mode */
//      rc = gpio_tlmm_config(GPIO_CFG(36, 0, GPIO_CFG_OUTPUT
        rc = gpio_tlmm_config(GPIO_CFG(86, 0, GPIO_CFG_OUTPUT
                    , GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if (rc != 0) {
//          DBGERROR((L("%s: GPIO_36 config = %d\n"), __func__, rc));
            DBGERROR((L("%s: GPIO_86 config = %d\n"), __func__, rc));
        }

        spi_env_ctrl(SPI_CONTROL_OFF, SPI_CLIENT_FPS);
    	gpio_set_value(86, 1);
/* FUJITSU:2012-06-25 for mod end */

        /* msm8960 GPIO 23 set pull down */
        rc = gpio_tlmm_config(GPIO_CFG(SENSOR_GPIO_INT, 0, GPIO_CFG_INPUT
                    , GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if (rc != 0) {
            DBGERROR((L("%s: MSM8960_SENSOR_GPIO_INT config failed\n")
                                                                , __func__));
        }

        /* pm8921_GPIO 20 read */
        rc = gpio_get_value_cansleep(
                            PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20));
        debug_printk("%s: PM8921_FGR_REG18_ON GPIO_%d = %d\n"
            , __func__,PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20),rc);

        if (rc == 1) {

            /* 1m wait*/
            AuthOsSleep(1);

            /* pm8921_GPIO 20 = "L". finger print sensor power off */
            gpio_set_value_cansleep(
                            PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20), 0);
            debug_printk("%s: PM8921_FGR_REG18_ON GPIO_%d -> Low\n"
                , __func__, PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20));

#ifdef _DEBUG_PROC_
            /* pm8921_GPIO 20 read */
            rc = gpio_get_value_cansleep(
                            PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20));
            debug_printk("%s: PM8921_FGR_REG18_ON GPIO_%d : %d\n"
            , __func__,PM8921_GPIO_PM_TO_SYS(PM8921_FGR_REG18_ON_20),rc);
#endif /* _DEBUG_PROC_ */

        }
#ifdef _DEBUG_PROC_
        else{
            debug_printk("%s: already power off\n", __func__);
        }
#endif /* _DEBUG_PROC_ */

        wake_unlock(&stWakeLock);
    }
/* FUJITSU:2011-12-12 FingerPrintSensor add end */
    return TRUE;
}


