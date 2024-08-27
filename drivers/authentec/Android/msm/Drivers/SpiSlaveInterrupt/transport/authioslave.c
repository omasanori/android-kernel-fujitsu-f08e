 /* /=======================================================================\
  * |                  AuthenTec Embedded (AE) Software                     |
  * |                                                                       |
  * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
  * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
  * |                                                                       |
  * |   Copyright (C) 2006-2011, AuthenTec, Inc. - All Rights Reserved.     |
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

   File:            authioSlave.c

   Description:     This file contains I/O interface specific code
                    for use with the AuthenTec Unified Driver Model.

                    This specific implementation of the I/O is targeted to
                    the SPI-Slave interface.

   Author:          James Deng

   ************************************************************************* */

/** include files **/
#include "hfiles.h"


/** local definitions **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-S */
extern uint8 g_log_drv_hw;
#define		FPDBGLOG(format, arg...) \
		if (g_log_drv_hw) printk("[FP_DRV] %s" format, KERN_DEBUG, ##arg)
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-E */

/* FUJITSU:2013-02-07 Spi_Bus4_SoftCS_Mutex add-s */
extern void fjdev_spi_bus4_softcs_mutex(int enable /* 0:unlock, 1:lock */);

void AuthFj_SpiCsCtrlExclusion(int lock, struct spi_device *spi)
{
	FPDBGLOG("AuthFj_SpiCsCtrlExclusion [S]: lock = %d\n", lock);

	if (1 == lock) {
		fjdev_spi_bus4_softcs_mutex( 1 );
        spi_setup( spi );
		gpio_set_value(86, 0);				/* CS:Low(Active) */
	} else {
		gpio_set_value(86, 1);				/* CS:Hi(Inactive) */
		fjdev_spi_bus4_softcs_mutex( 0 );
	}

	FPDBGLOG("AuthFj_SpiCsCtrlExclusion [E]: lock = %d\n", lock);
}

/* FUJITSU:2013-02-07 Spi_Bus4_SoftCS_Mutex add-e */

/*++
===============================================================================

Function AuthIoInit

The AuthIoInit() function performs any required port initialization required
for communicating with the sensor.

Parameters
lpvContext (input)          Pointer to the entire driver context.

Return Value                Returns a TRUE to indicate that the IO's have
                            been configured, FALSE otherwise.

===============================================================================
--*/

BOOL
AuthIoInit (void * lpvContext)
{
    lptsAUTHIO                  lptsAuthIO = NULL;         /* Pointer to the I/O context */

    /* Validate the structure / context... */
    if (NULL == lpvContext)
    {
        return FALSE;
    }

    lptsAuthIO = PIO(lpvContext);

    if (lptsAuthIO->pvLock == NULL )
    {
        //lptsAuthIO->pvLock = (void *)kmalloc(sizeof(struct mutex), GFP_ATOMIC);
        //mutex_init(lptsAuthIO->pvLock);
    }
    
    return TRUE;
}


/*++
===============================================================================

Function AuthIoDeinit

The AuthIoDeinit() function releases port resources that were required for
communicating with the sensor.

Parameters
lptsAuthIO (input)          Pointer to the IO context.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void AuthIoDeinit (lptsAUTHIO   lptsAuthIO)
{
    //mutex_destroy(lptsAuthIO->pvLock); 
    //kfree(lptsAuthIO->pvLock);
}

/*++
===============================================================================

Function AuthIoEnable

The AuthIoEnable() function enables the I/O port. This may be required to
take a peripheral out of low power mode, or to clear an error condition.

Parameters
lptsAuthIO (input)          Pointer to the IO context.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void
AuthIoEnable (lptsAUTHIO    lptsAuthIO)
{

}



/*++
===============================================================================

Function AuthIoDisable

The AuthIoDisable() function disables the I/O port. This may be required to
allow a peripheral to be put in a low power mode, or to clear an error
condition.

Parameters
lptsAuthIO (input)          Pointer to the IO context.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void
AuthIoDisable (lptsAUTHIO   lptsAuthIO)
{

}



/*++
===============================================================================

Function AuthIoWriteReadSensor

The AuthIoWriteReadSensor() function is responsible for writing uiBytes worth of
data starting at lpucWrite. If lpucRead is non-null, the corresponding byte
read for each byte written should be stored and returned to the caller. This
function is called from AuthHalWriteSensor().

Parameters
lptsAuthIO (input)          Pointer to the IO context.

lpucWrite (input)           Pointer to the bytes to be written to the sensor.

uiBytes (input)             Specifies the number of bytes to be written to the
                            sensor pointed to by lpucWrite (and lpucRead if
                            non-null).

lpucRead (input)            If non-null, a pointer to a buffer that will store
                            data read back from the sensor. The buffer length
                            is only uiBytes long. This is provided for
                            interfaces such as SPI Slave where data must be
                            read out of the sensor for every byte that is
                            transmitted to it.

bExpectData (input)         If TRUE, indicates that the sensor will start or
                            continue to send data after this command is issued.
                            FALSE indicates the sensor will not respond to this
                            command with data.

Return Value                The number of bytes written indicates success. A
                            value of -1 indicates failure. In the case of a
                            SPI Slave DMA transfer, it indicates that the
                            transfer is started and the command should be
                            sent via the ISR DMA buffer.

===============================================================================
--*/


uint32
AuthIoWriteReadSensor (lptsAUTHIO       lptsAuthIO,
                   const uint8 *    lpucWrite,
                   uint32           uiBytes,
                   uint8 *          lpucRead,
                   BOOL            bExpectData
                    )
{

    struct spi_device *spi_fp;
    uint32 uiCount;
    uint32 uiLoop = 0;
    uint32 uiBytesToWrite = 0;
    
    DBPRINT((L("AuthIoWriteReadSensor: called with %d bytes to write, bExpectData=%d, MAX_WRITE per request is %d\n"), uiBytes, bExpectData, MAX_WRITE));

    if (NULL == lptsAuthIO)
    {
        DBGERROR((L("AuthIoWriteReadSensor: invalid IO\n")));
        return -1;
    }

    spi_fp = (struct spi_device *)lptsAuthIO->pvIoDevice;

    //mutex_lock(lptsAuthIO->pvLock); 

    uiCount = uiBytes / MAX_SPI_TRANSFER_PER_REQUEST;

    if (uiCount == 0)
    {
        uiCount = 1;
    }
    else
    {
        if (uiBytes%MAX_SPI_TRANSFER_PER_REQUEST != 0)
        {
            uiCount += 1;
        }
    }
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-S */
	if(lpucRead==NULL) {
		FPDBGLOG( "%s : spi_write %ld bytes of sensor command %02x %02x %02x %02x ...\n", __func__, uiBytes, lpucWrite[0], lpucWrite[1], lpucWrite[2], lpucWrite[3] );
	}
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-E */

    for(uiLoop= 0; uiLoop < uiCount; uiLoop++)
    {
        if (uiCount == 1)
        {
            uiBytesToWrite = uiBytes;
        }
        else
        {
            if ((uiBytes%MAX_SPI_TRANSFER_PER_REQUEST != 0) && (uiLoop == (uiCount-1)) ) 
            {
                uiBytesToWrite = uiBytes%MAX_SPI_TRANSFER_PER_REQUEST;
            }
            else
            {
                uiBytesToWrite = MAX_SPI_TRANSFER_PER_REQUEST;
            }
        }
            
        AuthFj_SpiCsCtrlExclusion( 1 , spi_fp );			/* FUJITSU:2013-02-07 Spi_Bus4_SoftCS_Mutex add */

        if(lpucRead==NULL) 
        {
            DBPRINT((L("AuthIoWriteReadSensor: spi_write %d bytes\n"), uiBytesToWrite));
            spi_write(spi_fp, (lpucWrite+uiLoop*MAX_SPI_TRANSFER_PER_REQUEST), uiBytesToWrite);
        }
        else
        {
            struct spi_message message;
            struct spi_device *spi = spi_fp;
            struct spi_transfer transfer = {
            .tx_buf = (const void *)(lpucWrite+uiLoop*MAX_SPI_TRANSFER_PER_REQUEST),
            .rx_buf = lpucRead + uiLoop*MAX_SPI_TRANSFER_PER_REQUEST,
            .len = uiBytesToWrite,
            .cs_change = 0,
            .delay_usecs = 0,
            };

            DBPRINT((L("AuthIoWriteReadSensor: spi_sync write and read %d bytes\n"), uiBytesToWrite));

            spi_message_init(&message);
            spi_message_add_tail(&transfer, &message);
            if (spi_sync(spi, &message) != 0 || message.status != 0)
            {
                DBGERROR((L("AuthIoWriteReadSensor: error: spi_sync failed.\n")));
		        AuthFj_SpiCsCtrlExclusion( 0 , spi_fp );			/* FUJITSU:2013-02-07 Spi_Bus4_SoftCS_Mutex add */
                //mutex_unlock(lptsAuthIO->pvLock); 
                return -1;
            }

			/* FUJITSU:2013-01-17 FingerPrintSensor_SpiCheck add-S */
			if ((lpucRead != NULL) && (uiLoop == 0) && (lpucRead[0]==0x00) && (lpucRead[1]==0x00) && (lpucRead[2]==0x00) && (lpucRead[3]==0x00)) {
				printk("[FP_DRV] Top 4-0.\n");
			}
			/* FUJITSU:2013-01-17 FingerPrintSensor_SpiCheck add-E */

            }
	        AuthFj_SpiCsCtrlExclusion( 0 , spi_fp );			/* FUJITSU:2013-02-07 Spi_Bus4_SoftCS_Mutex add */
        }
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-S */
        if(lpucRead!=NULL) {
            FPDBGLOG( "%s : got %ld bytes of sensor data %02x %02x %02x %02x ...\n", __func__, uiBytes, lpucRead[0], lpucRead[1], lpucRead[2], lpucRead[3] );
        }
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-E */

        //mutex_unlock(lptsAuthIO->pvLock);
        return uiBytes;
        
}


