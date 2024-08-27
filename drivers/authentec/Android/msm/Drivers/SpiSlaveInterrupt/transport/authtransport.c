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

   File:            authtransport.c

   Description:     The goal of the authtransport.c file is to facilitate porting
                    AuthenTec transport driver to diverse platforms and
                    architectures in a timely manner. A fully functional
                    transport driver can be implemented by combining common code 
                    and operations with platform specific implementation modules.

   Author:          James Deng

   ************************************************************************* */

/** include files **/
#include "hfiles.h"

/** local definitions **/
/*How many locks are needed?*/
#define TRANSPORT_LOCK_NUMBER 1

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-S */
extern uint8 g_log_drv_hal;
#define		FPDBGLOG(format, arg...) \
		if (g_log_drv_hal) printk("[FP_DRV] %s" format, KERN_DEBUG, ##arg)
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-E */

/*++
===============================================================================

 *  Function: TransportSetupTransfer
 *
 *  Purpose:  This function specifies the amount and type of data to
 *            be expected from the sensor and how the kernel driver
 *            should behave after the next write or in case of inte-
 *            rrupt.
 *
 *            By default, the kernel driver will not read anything
 *            from the sensor before the next write is performed or
 *            an interrupt is received.
 *
 *            Buffering of the data is left to the driver implemen-
 *            tation (if the driver must also support the 1750,
 *            buffering is mandatory.  If only Saturn is supported,
 *            the driver may be built without buffering support).
 *
 *            The uiOptionFlags sepcify how the kernel driver should
 *            behave in case of interrupt.
 *
 *  Parameters:
 *
 *            psHandle (input) - Context of the transort layer.
 *
 *            uiPacketLen (input) - Size of the packets expected
 *                                  back from the sensor.  In case
 *                                  of read-data-on-interrupt, it
 *                                  specifies how much data can be
 *                                  read after each interrupt.
 *
 *            uiNShot (input) - Specifies how many packets should be
 *                              read and buffered by the driver (if
 *                              buffering is enabled).  This is also
 *                              an indication of how big each chunk
 *                              of data is requested in a single read
 *                              operation by the caller.
 *
 *            bKeepOn (input) - If set to TRUE, the kernel driver
 *                              must continue reading and buffering
 *                              data as (uiPacketLen * uiNShot)
 *                              chunks until the transfer is can-
 *                              celled.
 *
 *            uiReadPolicy (input) - Enumerated type that specifies
 *                                   if and when the kernel driver
 *                                   has to read data from the sen-
 *                                   sor.
 *
 *            uiOptionFlags (input) - Flags that control the cond-
 *                                    itions for reading and mapp-
 *                                    ing of interrupts.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/
int32 TransportSetupTransfer(IN void *lpvContext, 
                                                                IN uint32 uiPacketLen, 
                                                                IN uint32 uiNShot, 
                                                                IN BOOL bKeepOn,
                                                                IN uint32  uiReadPolicy,
                                                                IN uint32  uiOptionFlags)
{
    DBPRINT((L("TransportSetupTransfer: is called with uiPacketLen=%d, uiNShot=%d, bKeepOn=%d, uiReadPolicy=%d, uiOptionFlags=%d\n"), 
                    uiPacketLen, uiNShot, bKeepOn, uiReadPolicy, uiOptionFlags ));
    return AuthHalSetupTransfer(lpvContext, 
                                                uiPacketLen, 
                                                uiNShot, 
                                                bKeepOn, 
                                                uiReadPolicy, 
                                                uiOptionFlags);
}


/*++
===============================================================================

 *  Function: TransportInitialize
 *
 *  Purpose:  Loads the specified transport library (if multiple
 *            available) and creates the transport context.
 *
 *            If the transport library needs to talk to a kernel
 *            driver, the connection to the kernel driver should
 *            be opened here (open the device file).
 *
 *  Parameters:
 *
 *            szDeviceFileName (input) - A NULL-terminated string indi-
 *                                       cating the Kernel device to open.
 *                                       If this string is NULL, it will
 *                                       attempt to find the last avail-
 *                                       able device (by starting at 9
 *                                       and working back to 0).
 *
 *            psHandle (output) - Pointer to a transport module context.
 *                                tsTrHandle * is an opaque pointer. The
 *                                content of the handle structure is
 *                                known to the transport interface only
 *                                (platform dependent).  The structure
 *                                is allocated inside the transport layer.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/
int32 TransportInitialize (IN void *lpvContext, IN tsTrInitParams *pInitParams) 
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context            */

    DBPRINT((L("TransportInitialize: is called\n")));
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    #if 0
    if (!lptsAuthTransport->stTrInitParams.pucAbortSequence)
    {
        printk("TransportInitialize: pucAbortSequence is NULL\n\n");
        /* Initialize the swipe buffer size and flags... */
        MEMSET(&sAuthBuffer, 0, sizeof(sAuthBuffer));
        sAuthBuffer.uiStructSize     = sizeof(tsAUTHBUFFER);
        sAuthBuffer.uiBufferLength   = pInitParams->uiAbortSequenceLen;
        sAuthBuffer.uiFlags          = AB_NO_PHYSICAL;

        /* Try to allocate a swipe buffer... */
        if ( !AuthBufferRelease( &sAuthBuffer ) )
        {
            /* Allocation failure - don't go any further... */
            printk("TransportInitialize: TR_INVALID_PARAMETER \n\n");
            return TR_INVALID_PARAMETER;
        }
        lptsAuthTransport->stTrInitParams.pucAbortSequence = (uint8*)sAuthBuffer.pvVirtualBuffer;
    }
    #endif

    /*Must power on the sensor on initialization/open */
#ifdef TRANSPORT_ENABLE_SENSOR_POWEROFF
    /*Power off first and then power on*/
    AuthGpioPowerOn(PGPIO(lptsAuthTransport), FALSE);
    AuthOsSleep(10);
    AuthGpioPowerOn(PGPIO(lptsAuthTransport), TRUE);
    AuthOsSleep(10);
#endif

    lptsAuthTransport->bOperationAborted = FALSE;

    MEMCPY(&lptsAuthTransport->stTrInitParams, pInitParams, sizeof(tsTrInitParams));

    TransportSetupTransfer(lpvContext, 0, 0, FALSE, 0, 0);

    return TR_OK;
}


/*++
===============================================================================

 *  Function: TransportUninitialize
 *
 *  Purpose:  This function closes the connection with the device
 *            and kernel driver (closes the device file) and
 *            releases the context.
 *
 *  Parameters:
 *
 *            lpvContext (input) - Context of the transport driver.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/

int32 TransportUnInitialize (IN void *lpvContext) 
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context            */

    DBPRINT((L("TransportUnInitialize: is called\n")));
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    #if 0
    if (lptsAuthTransport->stTrInitParams.pucAbortSequence)
    {
        /* Initialize the swipe buffer size and flags... */
        MEMSET(&sAuthBuffer, 0, sizeof(sAuthBuffer));
        sAuthBuffer.uiStructSize     = sizeof(tsAUTHBUFFER);
        sAuthBuffer.pvPhysicalBuffer = lptsAuthTransport->stTrInitParams.pucAbortSequence;
        sAuthBuffer.uiFlags          = AB_NO_PHYSICAL;

        AuthBufferRelease(&sAuthBuffer);
        lptsAuthTransport->stTrInitParams.pucAbortSequence = NULL;
    }
    #endif

    /*Must power off the sensor on uninitialization/close */
#ifdef TRANSPORT_ENABLE_SENSOR_POWEROFF
    AuthGpioPowerOn(PGPIO(lptsAuthTransport), FALSE);
#endif
    
    lptsAuthTransport->bOperationAborted = FALSE;

    TransportSetupTransfer(lpvContext, 0, 0, FALSE, 0, 0);

    return TR_OK;
    
}

/*++
===============================================================================

 *  Function: TransportReset
 *
 *  Purpose:  This function restores the connection to the device.
 *            Its effect at kernel driver level is equivalent to
 *            calling TransportUninitialize + TransportInitialize +
 *            TransportOpen.  However, the context is preserved
 *            and there is no need to resend the configuration
 *            parameters.  If an operation was aborted by the kernel
 *            driver because of an unexpected suspend event, this
 *            function will clear all errors (including sticky
 *            error codes).
 *
 *  Parameters:
 *
 *            lpvContext (input) - Context of the transport driver.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/

int32 TransportReset(IN void *lpvContext)
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */
    
    DBPRINT((L("TransportReset: is called\n")));

    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    lptsAuthTransport->bOperationAborted = FALSE;
    
    TransportSetupTransfer(lpvContext, 0, 0, FALSE, 0, 0);
    
    lptsAuthTransport->uiError = 0;
    lptsAuthTransport->bUserReadActive = FALSE;
    
    return TR_OK;
}

/*++
===============================================================================

 *  Function: TransportGetDriverInfo
 *
 *  Purpose:  This function returns a structure containing information
 *            about the driver.  Since there may be multiple versions
 *            of this structure over time, the caller must specify
 *            which version is desired.
 *
 *  Parameters:
 *
 *            lpvContext (input) - Context of the transport driver.
 *
 *            uiRequestedVersion (input) - Requested version of the
 *                                         driver info structure.
 *                                         If zero, the latest verison
 *                                         supported by the driver
 *                                         should be returned.
 *
 *            pDriverInfoBuffer (output) - Buffer to receive the
 *                                         desired version, allocated
 *                                         by the caller.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/

int32 TransportGetDriverInfo(IN void *lpvContext,
                                            IN uint32 uiRequestedVersion,
                                            IN uint32 *puiBufferSize,
                                            INOUT uint8 *pDriverInfoBuffer) 
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */

    DBPRINT((L("TransportGetDriverInfo: is called\n")));
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    if (*puiBufferSize < sizeof (lptsAuthTransport->stDriverInfo))
    {
        DBGERROR((L("TransportGetDriverInfo: buffer is too small: %d < %d\r\n"), (int)*puiBufferSize, sizeof (lptsAuthTransport->stDriverInfo)));
        *puiBufferSize = sizeof (lptsAuthTransport->stDriverInfo);
        return TR_INVALID_PARAMETER;
    }
    
    MEMCPY (pDriverInfoBuffer, (uint8*)&lptsAuthTransport->stDriverInfo, *puiBufferSize );
    *puiBufferSize = sizeof (lptsAuthTransport->stDriverInfo);

    return TR_OK;
}

/*++
===============================================================================

 *  Function: TrasnportSetDriverPower
 *
 *  Purpose:  This function suggests the recommended power state to
 *            the kernel mode driver.  The real power state used by
 *            the kernel driver is guaranteed to match what is spe-
 *            cified in this function, due to timing and supported
 *            features of the driver.
 *
 *  Parameters:
 *
 *            psHandle (input) - Context of the transport layer.
 *
 *            uiPowerMode (input) - Identifier of the power mode.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/
int32 TransportSetDriverPower(IN void *lpvContext, IN uint32 uiPowerMode)
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */
    uint32                  uiCurrentMode;
    uint32                  uiModeToSet;
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;
    
    uiCurrentMode = lptsAuthTransport->uiPowerMode;
    uiModeToSet = uiPowerMode;

    DBPRINT((L("TransportSetDriverPower: is called with power mode %d to set and current mode is %d\n"), uiPowerMode, uiCurrentMode));
    
    switch(uiModeToSet)
    {
        case POWER_MODE_OFF: 
        {
        #ifdef TRANSPORT_ENABLE_SENSOR_POWEROFF
            /*Power off the sensor*/
            AuthGpioPowerOn(PGPIO(lptsAuthTransport), FALSE);
        #else
            uiModeToSet = POWER_MODE_SLEEP;
        #endif
            break;
        }
        case POWER_MODE_SLEEP:
        {
            if (POWER_MODE_OFF == uiCurrentMode)
            {
            #ifdef TRANSPORT_ENABLE_SENSOR_POWEROFF
                /*Power on the sensor*/
                AuthGpioPowerOn(PGPIO(lptsAuthTransport), TRUE);
                AuthOsSleep(10);
            #endif
            }
            break;
        }
        case POWER_MODE_STANDBY:
        {
            break;
        }
        case POWER_MODE_ACTIVE:
        {
            if (POWER_MODE_OFF == uiCurrentMode)
            {
            #ifdef TRANSPORT_ENABLE_SENSOR_POWEROFF
                /*Power off the sensor*/
                AuthGpioPowerOn(PGPIO(lptsAuthTransport), TRUE);
                AuthOsSleep(10);
            #endif
            }
            break;
        }
    }

    lptsAuthTransport->uiPowerMode = uiModeToSet;

    return 0;
}

/*++
===============================================================================

 *  Function: TransportGetDriverPower
 *
 *  Purpose:  This function retrieves the effective power mode of
 *            the kernel driver.
 *
 *  Parameters:
 *
 *            psHandle (input) - Context of the transport layer.
 *
 *            puiPowerMode (output) - Identifier of the power
 *                                    mode.
 *
 *  Returns:  TR_OK or a TRXXXX error code.

===============================================================================
--*/

int32 TransportGetDriverPower (IN void *lpvContext, OUT uint32 *pPowerMode) 
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */

    DBPRINT((L("TransportGetDriverPower: is called\n")));

    if (NULL == lpvContext || NULL == pPowerMode)
    {
        return TR_INVALID_PARAMETER;
    }
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    if (NULL == lptsAuthTransport)
    {
        return TR_INVALID_PARAMETER;
    }

    if (NULL == pPowerMode)
    {
        return TR_INVALID_PARAMETER;
    }

    *pPowerMode = lptsAuthTransport->uiPowerMode;

    DBPRINT((L("TransportGetDriverPower: return power mode %d\n"), lptsAuthTransport->uiPowerMode));

    return TR_OK;

}


/*++
===============================================================================

 *  Function: TransportCancelTransfer
 *
 *  Purpose:  This function cancels an active data read.  If no data
 *            read is in progress, the funciton does not do anything.
 *
 *            Note: This function is the only one that may be called
 *                  in the midst of a read operation.  In such cases,
 *                  the read operation (if not immediate because the
 *                  data are already buffered) must be terminated.
 *
 *  Parameters:
 *
 *            psHandle (input) - Context of the transport layer.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/

int32 TransportCancelTransfer (IN void *lpvContext) 
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */

    DBPRINT((L("TransportCancelTransfer: is called\n")));

    if (NULL == lpvContext)
    {
        return TR_INVALID_PARAMETER;
    }
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    if (NULL == lptsAuthTransport)
    {
        return TR_INVALID_PARAMETER;
    }

    if (lptsAuthTransport->bUserReadActive)
    {
        lptsAuthTransport->bTransferCanceled = TRUE;
        if (lptsAuthTransport->uiReadPolicy == RP_READ_ON_INT )
        {   
            AuthOsSetEvent(lpvContext);
        }
    }

    TransportSetupTransfer(lpvContext, 0, 0, FALSE, 0, 0);

    return TR_OK;
        
}

#if 0
/*++
===============================================================================

 *  Function: TransportStopTransfer
 *
 *  Purpose:  This function tells the kernel driver to stop reading
 *            data and to discard all data that were buffered so far.
 *            This also applies to interrupt notifications converted
 *            into response packets for sensors that require this
 *            support.
 *
 *  Parameters:
 *
 *            psHandle (input) - Context of the transport layer.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/

int32 TransportStopTransfer (IN void *lpvContext) 
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */
    uint32      i;             

    if (NULL == lpvContext)
    {
        return TR_INVALID_PARAMETER;
    }
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    if (NULL == lptsAuthTransport)
    {
        return TR_INVALID_PARAMETER;
    }

    TransportSetupTransfer(lpvContext, 0, 0, FALSE, 0, 0);

    for (i = 0; i<1; i++)
    {
        MEMSET (lptsAuthTransport->pucOutputBuffer[i], 0, TRANSPORT_MAX_OUTPUT_BUFFER_SIZE);
    }

}

#endif

/*++
===============================================================================

 *  Function: TransportStopTransfer
 *
 *  Purpose:  This function tells the kernel driver to stop reading
 *            data and to discard all data that were buffered so far.
 *            This also applies to interrupt notifications converted
 *            into response packets for sensors that require this
 *            support.
 *
 *  Parameters:
 *
 *            psHandle (input) - Context of the transport layer.
 *
 *  Returns:  TR_OK or a TR_XXXX error code.

===============================================================================
--*/

int32 TransportCheckProtocol (IN void *lpvContext, uint32 *puiProtocolID)
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */

    DBPRINT((L("TransportCheckProtocol: is called with protocol ID %d \n"), *puiProtocolID));

    if (NULL == lpvContext)
    {
        return TR_INVALID_PARAMETER;
    }
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    if (NULL == lptsAuthTransport)
    {
        return TR_INVALID_PARAMETER;
    }

    if (lptsAuthTransport->uiProtocolID != *puiProtocolID)
    {
        DBGERROR((L("TransportCheckProtocol: protocol ID doesn't match %d != %d\r\n"), (int)lptsAuthTransport->uiProtocolID, (int)*puiProtocolID));
        *puiProtocolID = 0;
    }

    DBPRINT((L("TransportCheckProtocol: protocol ID matched \n")));
    
    return TR_OK;
    
}

#if 0
/*++
===============================================================================

 *  Function: TransportExt
 *
 *  Purpose:  This function is used to send additional IOCTLs to the
 *            kernel mode driver.
 *
 *  Parameters:
 *
 *            psHandle (input) - Context of the transport layer.
 *
 *            uiIoctl (input) - Identifier of the IOCTL.
 *
 *            uiInBufLen (input) - Length of the input buffer.
 *
 *            pucInBuf (input) - Input buffer.
 *
 *            uiOutBufLen (input) - Size of the output buffer.
 *
 *            pucOutBuffer (output) - The output buffer.
 *
 *            puiBytesReceived (output) - Bytes stored into the
 *                                        output buffer.
 *
 *  Returns:  TR_OK on success, or a TR_XXXX error code.

===============================================================================
--*/

int32 TransportExt(IN void *lpvContext,
	IN uint32  uiIoctl, 
	IN uint32 uiInBufLen, 
	IN uint8  *pInBuf, 
	IN uint32 uiOutBufLen, 
	OUT uint8 *pOutBuf, 
	OUT uint32 *pBytesReceived)
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */

    DBPRINT((L("TransportExt: is called\n")));

    if (NULL == lpvContext)
    {
        return TR_INVALID_PARAMETER;
    }
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    if (NULL == lptsAuthTransport)
    {
        return TR_INVALID_PARAMETER;
    }
    
}

#endif

/*++
===============================================================================

 *  Function: TransportSetTimeout
 *
 *  Purpose:  This function is used to send additional IOCTLs to the
 *            kernel mode driver.
 *
 *  Parameters:
 *
 *            lpvContext (input) - Context of the transport layer.
 *
 *            uiTimeout (input) - timeout in ms.
 *
 *  Returns:  TR_OK on success, or a TR_XXXX error code.

===============================================================================
--*/

int32 TransportSetTimeout(IN void *lpvContext, IN uint32 uiTimeout)
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context */

    DBPRINT((L("TransportExt: is called with timeout %d ms\n"), uiTimeout));

    if (NULL == lpvContext)
    {
        return TR_INVALID_PARAMETER;
    }
    
    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    if (NULL == lptsAuthTransport)
    {
        return TR_INVALID_PARAMETER;
    }
    
    /* Store the timeout value... */
    lptsAuthTransport->uiMillisecondsWait = uiTimeout;

    return TR_OK;

}

/** public functions **/


/*++
===============================================================================

Function AuthTransportInit

The AuthTransportInit() function verifies the AUTHTRANSPORT context and acquires any
resources that are needed for its proper operation.

Parameters

lptsAuthTransport (input)         Pointer to the AUTHTRANSPORT Context.

Return Value                An int32 value indicating no error or
                            specifying a particular error condition.  The value
                            TR_OK indicates that no error was encountered.
                            Negative values represent error conditions.

===============================================================================
--*/

int32
AuthTransportInit (lptsAUTHTRANSPORT    lptsAuthTransport)
{
    tsAUTHBUFFER        sAuthBuffer;
    uint32          i =0;

    DBPRINT((L("AuthTransportInit: is called\n")));

    /* Are we pointing to a valid memory block? */
    if (NULL == lptsAuthTransport)
    {
        /* NULL Pointer - don't go any further... */
        return TR_INVALID_PARAMETER;
    }

    /* Verify the current size of the Transport context... */
    if  (lptsAuthTransport->uiStructSize  != sizeof(tsAUTHTRANSPORT))

    {
        DBGERROR((L("AuthTransportInit: invalid size of transport context\r\n")));
        
        /* Structure size does not match, report incompatible formats... */
        return TR_INVALID_PARAMETER;
    }

    //lptsAuthTransport->uiDataLength = 0;

#if 1
   for(i=0; i<MAX_BUFFER_NUMBER_IN_QUEUE; i++)
   {
        /* Initialize the swipe buffer size and flags... */
        MEMSET(&sAuthBuffer, 0, sizeof(sAuthBuffer));
        sAuthBuffer.uiStructSize     = sizeof(tsAUTHBUFFER);
        sAuthBuffer.uiBufferLength   = ONE_BUFFER_SIZE_IN_QUEUE;
        sAuthBuffer.uiFlags          = AB_NO_PHYSICAL;

        /* Try to allocate a swipe buffer... */
        if ( !AuthBufferAllocate( &sAuthBuffer ) )
        {
            DBGERROR((L("AuthTransportInit: failed to allocate memory %d bytes\r\n"), (int)sAuthBuffer.uiBufferLength));
            /* Allocation failure - don't go any further... */
            return TR_INVALID_PARAMETER;
        }
        lptsAuthTransport->pucOutputBuffer[i] = (uint8*)sAuthBuffer.pvVirtualBuffer;
    }
#endif

    lptsAuthTransport->uiProtocolID = TRANSPROT_SUPPORTED_PROTOCOL;

    /*Initialze tranport type info*/
    lptsAuthTransport->stDriverInfo.uiDriverVersion = 0;
    lptsAuthTransport->stDriverInfo.uiInterfaceSpeed = TRANSPORT_IO_DATA_RATE;
    lptsAuthTransport->stDriverInfo.uiStructSize = sizeof (tsDriverInfo);
#ifdef TRANSPORT_ENABLE_SENSOR_POWEROFF
    lptsAuthTransport->stDriverInfo.uiSupportsSensorPowerOff = 1;
#else
    lptsAuthTransport->stDriverInfo.uiSupportsSensorPowerOff = 0;
#endif
    lptsAuthTransport->stDriverInfo.uiVersion = 1;
    lptsAuthTransport->stDriverInfo.uiInterfaceType = TRANSPROT_INTERFACE_USED;

    lptsAuthTransport->uiMillisecondsWait = 300;

    lptsAuthTransport->bOperationAborted = FALSE;
    lptsAuthTransport->bTransferCanceled = FALSE;


    /* Try to initialize the HAL context... */
    if ( TR_OK != AuthHalInit((void *) lptsAuthTransport) )
    {
        /* Could not create call back timer, report an error... */
        DBGERROR((L("AuthTransportInit: Error: AuthHalInit Failed.\n")));
        return TR_INVALID_PARAMETER;
    }

    if ( TR_OK != AuthOsCreateEvent((void *) lptsAuthTransport) )
    {
        /* Could not create Event, report an error... */
        DBGERROR((L("AuthTransportInit: Error: AuthOsCreateEvent Failed.\n")));
        return TR_INVALID_PARAMETER;
    }

    if ( TR_OK != AuthOsCreateLock((void *) lptsAuthTransport, TRANSPORT_LOCK_NUMBER) )
    {
        /* Could not create Event, report an error... */
        DBGERROR((L("AuthTransportInit: Error: AuthOsCreateLock Failed.\n")));
        return TR_INVALID_PARAMETER;
    }

    

    DBPRINT((L("AuthTransportInit: OK\n")));

    /* Everything appears OK... */
    return TR_OK;
}


/*++
===============================================================================

Function AuthUdmDeinit

The AuthUdmDeinit() function frees up any resources that were required, such
as a timer and an Event.

Parameters

lptsAuthTransport (input)         Pointer to the AUTHTRANSPORT Context.

Return Value                An int32 value indicating no error or
                            specifying a particular error condition.  The value
                            TR_OK indicates that no error was encountered.
                            Negative values represent error conditions.

===============================================================================
--*/

int32
AuthTransportDeinit (lptsAUTHTRANSPORT      lptsAuthTransport)
{
    tsAUTHBUFFER    sAuthBuffer;           /* Swipe buffer memory addresses    */
    uint32                i = 0;

    DBPRINT((L("AuthTransportDeinit: is called\n")));

    /* Are we pointing to a valid memory block? */
    if (NULL == lptsAuthTransport)
    {
        /* NULL Pointer - don't go any further... */
        return TR_INVALID_PARAMETER;
    }

    /* Release and destroy the Event... */
    AuthOsSetEvent((void *) lptsAuthTransport);
    AuthOsCloseEvent((void *) lptsAuthTransport);

    /* Destroy the lock... */
    AuthOsDeleteLock((void *) lptsAuthTransport);

    /* Free up the HAL resources... */
    AuthHalDeinit((void *) lptsAuthTransport );

    for (i=0; i<MAX_BUFFER_NUMBER_IN_QUEUE; i++)
    {
        /* Do we still have a swipe buffer allocated? */
        if ( lptsAuthTransport->pucOutputBuffer[i] )
        {
            /* Initialize the swipe buffer size and flags... */
            MEMSET(&sAuthBuffer, 0, sizeof(sAuthBuffer));
            sAuthBuffer.uiStructSize     = sizeof(tsAUTHBUFFER);
            sAuthBuffer.pvVirtualBuffer  = lptsAuthTransport->pucOutputBuffer[i];
            sAuthBuffer.uiFlags          = AB_NO_PHYSICAL;

            /* Release the swipe buffer... */
            AuthBufferRelease( &sAuthBuffer );

            /* Use these settings for the swipe buffer... */
            lptsAuthTransport->pucOutputBuffer[i]  = NULL;
        }
    }
    
    DBPRINT((L("AuthTransportDeinit: OK\n")));
    
    /* Everything appears OK... */
    return TR_OK;
}


/*++
===============================================================================

Function AuthUdmRead

The AuthUdmRead() function returns a swipe buffer to the calling application
when one is available. If a swipe buffer is not available in a reasonable time
frame, the function should return without data and allow the application to
decide if it wants to continue or cancel the operation.

Parameters

lptsAuthTransport (input)         Pointer to the transport Context.

lpvBuf (output)             Pointer to the location where data
                            should be copied.

uiBytes (input)             The size of the buffer, in bytes, pointed to
                            by lpvBuf.

Return Value                The number of bytes read indicates success.
                            A value of -1 indicates failure.

===============================================================================
--*/

uint32
AuthTransportRead (lptsAUTHTRANSPORT    lptsAuthTransport,
             void *         lpvBuf,
             uint32         uiBytes)
{
    uint32      uiDataLength;       /* Amount of data in swipe buffer */
    static      uint32     uiIndex = 0; 
    uint32    uiRet = 0;
    uint32      uiReadPolicy = 0;
    uint32      uiOptions = 0;

    #define FAKED_PACKET_LENGTH 10
    uint8 ucSensorInterruptPacket[FAKED_PACKET_LENGTH] = { 0x80, 0x2f, 0x5a, 0x0a, 0x00, 0x00, 0x00, 0x16, 0x01, 0x01 };


    DBPRINT((L("AuthTransportRead: is called requesting %d bytes\n"), uiBytes));

    /* Are we pointing to a valid memory block? */
    if ((NULL == lptsAuthTransport) || (NULL == lpvBuf))
    {
        /* NULL Pointer(s) - don't go any further... */
        return ((uint32) -1);
    }

    if (lptsAuthTransport->bOperationAborted)
    {
        DBGERROR((L("AuthTransportRead: Operation was aborted. Return TR_OPERATION_ABORTED\n")));
        return TR_OPERATION_ABORTED;
    }

    if (lptsAuthTransport->uiPowerMode == POWER_MODE_OFF)
    {
/* FUJITSU:2012-09-10 UPDATE Start */
#if 0
/* FUJITSU:2011-12-12 FingerPrintSensor mod start */
        AuthOsSleep(lptsAuthTransport->uiMillisecondsWait / 8);
/* FUJITSU:2011-12-12 FingerPrintSensor mod end */
#else
        int i;
        int iCount;

        if (lptsAuthTransport->uiMillisecondsWait < 100)
        {
            AuthOsSleep(lptsAuthTransport->uiMillisecondsWait);
        }
        else
        {
            /*remainder is ignored as we don't have to be precise 
            and also there is overload caused by breaking sleep into multiple ones
            */
            iCount = lptsAuthTransport->uiMillisecondsWait /100;
            for (i=0; i<iCount; i++)
            {
                AuthOsSleep(100);
                if (lptsAuthTransport->uiPowerMode != POWER_MODE_OFF)
                {
                    break;
                }
            }
        }
#endif
/* FUJITSU:2012-09-10 UPDATE End */

        return 0;
        
    }
    
    if (lptsAuthTransport->bUserReadActive)
    {
        DBGERROR((L("AuthTransportRead: previous read request is still ongoing\n")));
        return ((uint32) -1);
    }

    AuthOsLock(lptsAuthTransport, 0);
    uiReadPolicy = lptsAuthTransport->uiReadPolicy;
    uiDataLength = lptsAuthTransport->uiBytes;
    uiOptions = lptsAuthTransport->uiOptions;
    AuthOsUnlock(lptsAuthTransport, 0);

    lptsAuthTransport->bUserReadActive = TRUE;
    
    DBPRINT((L("AuthTransportRead: read policy %d, read option =%d\n"), lptsAuthTransport->uiReadPolicy, lptsAuthTransport->uiOptions));

    if ( uiReadPolicy == RP_DO_NOT_READ)
    {
        if ( uiOptions == TR_FLAG_RETURN_INTERRUPT_PACKET )
        {
            AuthOsLock(lptsAuthTransport, 0);
            if (lptsAuthTransport->bPendingInterrupt)
            {
                DBPRINT((L("AuthTransportRead: there is pending interrupt packet\n")));
                MEMCPY((uint8*)lpvBuf, ucSensorInterruptPacket, FAKED_PACKET_LENGTH);
                lptsAuthTransport->bPendingInterrupt = FALSE;
                lptsAuthTransport->bUserReadActive = FALSE;
                AuthOsUnlock(lptsAuthTransport, 0);
                return FAKED_PACKET_LENGTH;
            }
            AuthOsUnlock(lptsAuthTransport, 0);
        }
    }

    else if (uiReadPolicy == RP_READ_ON_REQ)
    {
        if ( uiOptions == TR_FLAG_RETURN_INTERRUPT_PACKET )
        {
            //interrupt packet first
            AuthOsLock(lptsAuthTransport, 0);
            if (lptsAuthTransport->bPendingInterrupt)
            {
                DBPRINT((L("AuthTransportRead: there is pending interrupt packet\n")));
                MEMCPY((uint8*)lpvBuf, ucSensorInterruptPacket, FAKED_PACKET_LENGTH);
                lptsAuthTransport->bPendingInterrupt = FALSE;
                lptsAuthTransport->bUserReadActive = FALSE;
                AuthOsUnlock(lptsAuthTransport, 0);
                return FAKED_PACKET_LENGTH;
                //goto readBufferedData;
            }
            AuthOsUnlock(lptsAuthTransport, 0);
            
            AuthIsrReadOnRequest(lptsAuthTransport, lpvBuf, uiDataLength);
            lptsAuthTransport->bUserReadActive = FALSE;
            DBPRINT((L("AuthTransportRead: %d bytes of data read on request returned\n"), uiDataLength));
            return uiDataLength;
            
            
        }
        else
        {
            AuthIsrReadOnRequest(lptsAuthTransport, lpvBuf, uiDataLength);
            lptsAuthTransport->bUserReadActive = FALSE;
            DBPRINT((L("AuthTransportRead: %d bytes of data read on request returned 2\n"), uiDataLength));
            return uiDataLength;
        }
    }

    else
    {
        /* Wait until there is data, or the timeout elapses... */
        if (TR_OK == AuthOsWaitEvent((void *) lptsAuthTransport,
                                      lptsAuthTransport->uiMillisecondsWait))
        {
            lptsAuthTransport->bUserReadActive = FALSE;
            if (lptsAuthTransport->bTransferCanceled )
            {
                lptsAuthTransport->bTransferCanceled = FALSE;
                /*The event could be set by TransportCancel at the right moment we get data ready event,
                so nee to clear it. If this case doesn't happen, 1ms timeout*/
                AuthOsWaitEvent((void *) lptsAuthTransport, 1);
                /*Data read canceled, return 0 bytes.*/
                return 0;
            }
        
            MEMCPY((uint8*)lpvBuf, lptsAuthTransport->pucOutputBuffer[uiIndex], lptsAuthTransport->uiOutputBytes[uiIndex]);

            uiRet = lptsAuthTransport->uiOutputBytes[uiIndex];

            DBPRINT((L("AuthTransportRead: data available, return %d bytes\n"), uiRet));

            uiIndex ++ ; 
            uiIndex = uiIndex %MAX_BUFFER_NUMBER_IN_QUEUE;
            return uiRet;
        }
        else
        {
            DBPRINT((L("AuthTransportRead: data is not available, return 0 bytes\n")));
            lptsAuthTransport->bUserReadActive = FALSE;
            return 0;/* No data was available right now*/
        }
    }

    lptsAuthTransport->bUserReadActive = FALSE;
    
    return 0;

}


/*++
===============================================================================

Function AuthUdmWrite

The AuthUdmWrite() function writes command to the sensor.


Parameters

lptsAuthTransport (input)         Pointer to the transport Context.

lpucWrite   (input)         Pointer to the bytes to be written to the sensor.

uiBytes     (input)         Specifies the number of bytes to be written to the
                            sensor pointed to by lpucWrite.

Return Value                An int32 value indicating no error or
                            specifying a particular error condition.  The value
                            TR_OK indicates that no error was encountered.
                            Negative values represent error conditions.

===============================================================================
--*/


int32
AuthTransportWrite (lptsAUTHTRANSPORT       lptsAuthTransport,
              uint8 *           lpucWrite,
              uint32            uiBytes)
{
    DBPRINT((L("AuthTransportWrite: is called writing %d bytes\n"), uiBytes));
    
    /* Are we pointing to a valid memory block? */
    if ((NULL == lptsAuthTransport) || (NULL == lpucWrite))
    {
        /* NULL Pointer - don't go any further... */
        return TR_INVALID_PARAMETER;
    }

    if (lptsAuthTransport->bOperationAborted)
    {
        DBPRINT((L("AuthTransportWrite: operation was aborted, return %d\n"), TR_OPERATION_ABORTED));
        return TR_OPERATION_ABORTED;
    }
  
    /* Write the data... */
    AuthHalWriteSensor ((void *)    lptsAuthTransport,
                        (uint8 *)   lpucWrite,
                        (uint32)    uiBytes,
                        (uint8 *)   NULL);

    /* Everything appears OK... */
    return uiBytes;
}



/*++
===============================================================================

Function AuthUdmIOControl

The AuthUdmIOControl() function performs processing on common IOCTL commands.

Parameters

lptsAuthTransport (input)         Pointer to the AUTHTRANSPORT Context.

uiIoctl     (input)         Specifies an I/O control code to be performed.

lpucInBuf   (input)         Pointer to data input to the device.

uiInBufLength (input)       Specifies the number of bytes being passed in via
                            the lpucInBuf pointer.

lpucOutBuf  (output)        Pointer to data output from the driver.

uiOutBufLength (input)      Specifies the maximum number of bytes that can be
                            stored in lpucOutBuf.

lpuiBytesTransferred (output)   Pointer to the actual number of bytes stored
                                in lpucOutBuf.

Return Value                An int32 value indicating no error or
                            specifying a particular error condition.  The value
                            TR_OK indicates that no error was encountered.
                            Negative values represent error conditions.
                            
Note:  Calling code must guarantee that lpuiBytesTransferred, lpucInBuf and lpucOutBuf valid and 
          the minimum size of lpucInBuf and lpucOutBuf is the size of tsTrIoctlHeader for 
          AuthTransportIOControl to set the return code in the ioctl buffer.
          
===============================================================================
--*/

int32
AuthTransportIOControl (void *        lpvContext,
                  uint32        uiIoctl,
                  uint8 *       lpucInBuf,
                  uint32        uiInBufLength,
                  uint8 *       lpucOutBuf,
                  uint32        uiOutBufLength,
                  uint32 *      lpuiBytesTransferred)
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context            */
    int32               iRet = 0;

/* FUJITSU:2012-10-04 FingerPrintSensor_Log mod-S */
	FPDBGLOG("%s : is called with %s\n", __func__,
		(uiIoctl == AUTH_IOCTL_TRANSPORT_OPEN  ? "AUTH_IOCTL_TRANSPORT_OPEN" :
		(uiIoctl == AUTH_IOCTL_TRANSPORT_CLOSE ? "AUTH_IOCTL_TRANSPORT_CLOSE" :
		(uiIoctl == AUTH_IOCTL_TRANSPORT_RESET ? "AUTH_IOCTL_TRANSPORT_RESET" :
		(uiIoctl == AUTH_IOCTL_CHECK_PROTOCOL  ? "AUTH_IOCTL_CHECK_PROTOCOL" :
		(uiIoctl == AUTH_IOCTL_GET_DRVINFO     ? "AUTH_IOCTL_GET_DRVINFO" :
		(uiIoctl == AUTH_IOCTL_GET_DRIVER_POWER ? "AUTH_IOCTL_GET_DRIVER_POWER" :
		(uiIoctl == AUTH_IOCTL_SET_DRIVER_POWER ? "AUTH_IOCTL_SET_DRIVER_POWER" :
		(uiIoctl == AUTH_IOCTL_SETUP_TRANSFER  ? "AUTH_IOCTL_SETUP_TRANSFER" :
		(uiIoctl == AUTH_IOCTL_CANCEL_TRANSFER ? "AUTH_IOCTL_CANCEL_TRANSFER" :
		(uiIoctl == AUTH_IOCTL_STOP_TRANSFER   ? "AUTH_IOCTL_STOP_TRANSFER" :
		(uiIoctl == AUTH_IOCTL_SET_TIMEOUT     ? "AUTH_IOCTL_SET_TIMEOUT" :
		(uiIoctl == AUTH_IOCTL_TRANSPORT_EXT   ? "AUTH_IOCTL_TRANSPORT_EXT" :
		(uiIoctl == AUTH_IOCTL_OS_SUSPEND      ? "AUTH_IOCTL_OS_SUSPEND" :
		(uiIoctl == AUTH_IOCTL_OS_RESUME       ? "AUTH_IOCTL_OS_RESUME" : "Unknown!!")))))))))))))) );
/* FUJITSU:2012-10-04 FingerPrintSensor_Log mod-E */

    /* Are we pointing to a valid memory block? */
    if (NULL == lpvContext)
    {
        /* NULL Pointer - don't go any further... */
        DBGERROR((L("Transport IoCtl: Error: No Context.\r\n")));
        return TR_INVALID_PARAMETER;
    }

    /* Point to the Transport context */
    lptsAuthTransport = (lptsAUTHTRANSPORT) lpvContext;

    if (NULL == lptsAuthTransport)
    {
        return TR_INVALID_PARAMETER;
    }

    if (lptsAuthTransport->bOperationAborted 
        && (uiIoctl != AUTH_IOCTL_TRANSPORT_RESET)
        && (uiIoctl != AUTH_IOCTL_TRANSPORT_CLOSE)
        && (uiIoctl != AUTH_IOCTL_TRANSPORT_OPEN)
        &&(uiIoctl != AUTH_IOCTL_OS_SUSPEND)
        &&(uiIoctl != AUTH_IOCTL_OS_RESUME))
    {
        DBGERROR((L("AuthTransportIOControl: TR_OPERATION_ABORTED. Ioctl : %d\n"), (int)uiIoctl));
        if (lpucOutBuf != NULL)
        {
            tsTrIoctlHeader *pIoctlHeader =  (tsTrIoctlHeader *)lpucOutBuf;
            pIoctlHeader->retCode = TR_OPERATION_ABORTED;
        }
        
        return TR_OPERATION_ABORTED;
    }

    /* See if we can process this request... */
    switch ( uiIoctl )
    {
        case AUTH_IOCTL_SETUP_TRANSFER:
        {
            tsTrSetupTransferIoctlBuffer   *pstSetupTransfer = (tsTrSetupTransferIoctlBuffer*)lpucInBuf;
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_SETUP_TRANSFER\n")));
            iRet = TransportSetupTransfer(lpvContext, 
                                                        pstSetupTransfer->uiPacketLen,
                                                        pstSetupTransfer->uiNshots,
                                                        pstSetupTransfer->bKeepOn,
                                                        pstSetupTransfer->uiReadPolicy,
                                                        pstSetupTransfer->uiOptionFlags);

            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrSetupTransferIoctlBuffer)));
            pstSetupTransfer = (tsTrSetupTransferIoctlBuffer*)lpucOutBuf;
            pstSetupTransfer->sHeader.retCode = iRet;

            if (lpuiBytesTransferred != NULL)
            {
                *lpuiBytesTransferred = sizeof (tsTrSetupTransferIoctlBuffer);
            }
            break;
        }


        case AUTH_IOCTL_TRANSPORT_OPEN:
        {
            tsTrOpenCloseIoctlBuffer   *pstOpenIoctlBuffer = (tsTrOpenCloseIoctlBuffer*)lpucInBuf;
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_TRANSPORT_OPEN\n")));
            iRet = TransportInitialize(lpvContext, &pstOpenIoctlBuffer->sParams);

            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrOpenCloseIoctlBuffer)));
            pstOpenIoctlBuffer = (tsTrOpenCloseIoctlBuffer*)lpucOutBuf;
            pstOpenIoctlBuffer->sHeader.retCode = iRet;
            if (lpuiBytesTransferred != NULL)
            {
                *lpuiBytesTransferred = sizeof (tsTrOpenCloseIoctlBuffer);
            }
            
            break;
        }

        case AUTH_IOCTL_TRANSPORT_CLOSE:
        {
            tsTrOpenCloseIoctlBuffer   *pstCloseIoctlBuffer = (tsTrOpenCloseIoctlBuffer*)lpucInBuf;
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_TRANSPORT_CLOSE\n")));
            iRet = TransportUnInitialize(lpvContext);

            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrOpenCloseIoctlBuffer)));
            pstCloseIoctlBuffer = (tsTrOpenCloseIoctlBuffer*)lpucOutBuf;
            pstCloseIoctlBuffer->sHeader.retCode = iRet;
            *lpuiBytesTransferred = sizeof(tsTrOpenCloseIoctlBuffer);
            
            break;
        }

        case AUTH_IOCTL_TRANSPORT_RESET:
        {
            tsTrResetIoctlBuffer *pstResetIoctlBuffer = (tsTrResetIoctlBuffer*)lpucInBuf;
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_TRANSPORT_RESET\n")));
            iRet = TransportReset(lpvContext);
            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrResetIoctlBuffer)));
            pstResetIoctlBuffer = (tsTrResetIoctlBuffer*)lpucOutBuf;
            pstResetIoctlBuffer->sHeader.retCode = iRet;
            *lpuiBytesTransferred = sizeof(tsTrResetIoctlBuffer);
            
            break;
        }

        case AUTH_IOCTL_CHECK_PROTOCOL:
        {
            tsTrCheckProtocolIoctlBuffer *pstCheckProtocolIoctlBuffer = (tsTrCheckProtocolIoctlBuffer*)lpucInBuf;
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_CHECK_PROTOCOL with protocol ID %d\n"), pstCheckProtocolIoctlBuffer->uiProtocolID));
            iRet = TransportCheckProtocol(lpvContext, &pstCheckProtocolIoctlBuffer->uiProtocolID);

            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrCheckProtocolIoctlBuffer)));
            pstCheckProtocolIoctlBuffer = (tsTrCheckProtocolIoctlBuffer*)lpucOutBuf;
            pstCheckProtocolIoctlBuffer->sHeader.retCode = iRet;
            *lpuiBytesTransferred = sizeof(tsTrCheckProtocolIoctlBuffer);
            break;
        }

        case AUTH_IOCTL_GET_DRVINFO:
        {
            uint32 uiBufferSize;
            tsTrDriverInfoIoctlBuffer *pstDriverInfo = (tsTrDriverInfoIoctlBuffer*)lpucInBuf;

            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_GET_DRVINFO\n")));

            uiBufferSize = pstDriverInfo->uiBufferSize;
            
            iRet = TransportGetDriverInfo(lpvContext, 
                                                                pstDriverInfo->uiRequestedVersion, 
                                                                &uiBufferSize, 
                                                                pstDriverInfo->aucBuffer);
            
            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, pstDriverInfo->uiBufferSize));
            pstDriverInfo = (tsTrDriverInfoIoctlBuffer*)lpucOutBuf;
            pstDriverInfo->uiBufferSize = uiBufferSize;
            pstDriverInfo->sHeader.retCode = iRet;
            
            break;
        }

        case AUTH_IOCTL_SET_DRIVER_POWER:
        {
            tsTrDriverPowerIoctlBuffer *pstDriverPower = (tsTrDriverPowerIoctlBuffer*)lpucInBuf;
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_SET_DRIVER_POWER with power mode %d\n"), pstDriverPower->uiPowerMode));
            iRet = TransportSetDriverPower(lpvContext, pstDriverPower->uiPowerMode);
            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrDriverPowerIoctlBuffer)));
            pstDriverPower = (tsTrDriverPowerIoctlBuffer*) lpucOutBuf;
            pstDriverPower->sHeader.retCode = iRet;
            
            break;
        }

        case AUTH_IOCTL_GET_DRIVER_POWER:
        {
            tsTrDriverPowerIoctlBuffer *pstDriverPower = (tsTrDriverPowerIoctlBuffer*)lpucInBuf;

            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_GET_DRIVER_POWER\n")));
            
            iRet = TransportGetDriverPower(lpvContext, &pstDriverPower->uiPowerMode);
            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrDriverPowerIoctlBuffer)));
            pstDriverPower = (tsTrDriverPowerIoctlBuffer*) lpucOutBuf;
            pstDriverPower->sHeader.retCode = iRet;
            break;
        }

        case AUTH_IOCTL_CANCEL_TRANSFER:
        {
            tsTrCancelIoctlBuffer *pstCancelTransfer = (tsTrCancelIoctlBuffer*)lpucInBuf;
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_CANCEL_TRANSFER\n")));
            iRet = TransportCancelTransfer(lpvContext);
            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrCancelIoctlBuffer)));
            pstCancelTransfer = (tsTrCancelIoctlBuffer*)lpucOutBuf;
            pstCancelTransfer->sHeader.retCode = iRet;
            
            break;
        }

#if 0                                                
        case AUTH_IOCTL_STOP_TRANSFER:
        {
            tsTrStopIoctlBuffer *pstStopTransfer = (tsTrStopIoctlBuffer*)lpucInBuf;
            iRet = TransportStopTransfer(lpvContext);
            
            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrStopIoctlBuffer)));

            pstStopTransfer = (tsTrStopIoctlBuffer*)lpucOutBuf;
            pstStopTransfer->sHeader.retCode = iRet;
            
            break;
        }
#endif

        case AUTH_IOCTL_TRANSPORT_EXT:
        {
            tsTrIoctlBuffer *pIoctlBuffer = (tsTrIoctlBuffer*)lpucInBuf;
            uint32 uiOperationID = pIoctlBuffer->sHeader.uiOperationId;
            
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_TRANSPORT_EXT with uiOperationID %d \n"), uiOperationID));

            switch(uiOperationID)
            {
                case TROPID_SEND_MOUSE_EVENT:
                {
                    tsTrMouseEvent *pstMouseEvent = (tsTrMouseEvent*)pIoctlBuffer->aucBuffer;
                    iRet = AuthOsOutputMouseEvent(lpvContext, pstMouseEvent);
                    break;
                }
                case TROPID_SEND_KEY_EVENT:
                {
                    tsTrKeyEvent *pstKeyEvent = (tsTrKeyEvent*)pIoctlBuffer->aucBuffer;
                    iRet = AuthOsOutputKeyEvent(lpvContext, pstKeyEvent);
                    break;
                }
                default:
                    DBGERROR((L("AUTH_IOCTL_TRANSPORT_EXT: Error: invalid op id\r\n")));
                    iRet = TR_INVALID_PARAMETER;
                    break;
            }

            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrIoctlBuffer)));
            pIoctlBuffer = (tsTrIoctlBuffer*)lpucOutBuf;
            pIoctlBuffer->sHeader.retCode = iRet;
            
/* FUJITSU:2012-09-10 UPDATE Start */
            break;
/* FUJITSU:2012-09-10 UPDATE End */
        }

        case AUTH_IOCTL_SET_TIMEOUT:
        {
            tsTrTimeoutIoctlBuffer *pstTimoutBuffer = (tsTrTimeoutIoctlBuffer*) lpucInBuf;

            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_SET_TIMEOUT with timeout %d \n"), pstTimoutBuffer->uiTimeout));
            
            iRet = TransportSetTimeout(lpvContext, pstTimoutBuffer->uiTimeout);

            MEMCPY(lpucOutBuf, lpucInBuf, MIN(uiOutBufLength, sizeof(tsTrTimeoutIoctlBuffer)));

            pstTimoutBuffer = (tsTrTimeoutIoctlBuffer*)lpucOutBuf;
            pstTimoutBuffer->sHeader.retCode = iRet;
            
            break;
        }

        case AUTH_IOCTL_OS_RESUME:
        {
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_OS_RESUME \n")));
            break;
        }

        case AUTH_IOCTL_OS_SUSPEND:
        {
            uint32 uiCount = 0;
            
            DBPRINT((L("AuthTransportIOControl: AUTH_IOCTL_OS_SUSPEND with power mode %d\n"), lptsAuthTransport->uiPowerMode));

            switch(lptsAuthTransport->uiPowerMode)
            {
                case POWER_MODE_OFF:
                    break;
                    
                case POWER_MODE_SLEEP:
                    break;

                case POWER_MODE_STANDBY:
                    /*TODO: add TTI and TTW support if needed*/
                    break;
                case POWER_MODE_ACTIVE:
                    DBGERROR((L("AuthTransportIOControl: warning: suspend while operation is going\r\n")));
                #ifndef TRANSPORT_ENABLE_SENSOR_POWEROFF
                    lptsAuthTransport->bOperationAborted = TRUE;
                    lptsAuthTransport->bUserReadActive = FALSE;
                    if (lptsAuthTransport->stTrInitParams.uiAbortSequenceLen > 0)
                    {
                        AuthTransportWrite(lptsAuthTransport, 
                                  lptsAuthTransport->stTrInitParams.pucAbortSequence,
                                  lptsAuthTransport->stTrInitParams.uiAbortSequenceLen);
                    }
                    lptsAuthTransport->uiPowerMode = POWER_MODE_SLEEP;
                #endif
                    break;
                
            }

        /*Must power off the sensor on suspend if power on/off is supported */
        #ifdef TRANSPORT_ENABLE_SENSOR_POWEROFF
            AuthGpioPowerOn(PGPIO(lptsAuthTransport), FALSE);
            lptsAuthTransport->bUserReadActive = FALSE;
            if (lptsAuthTransport->uiPowerMode != POWER_MODE_OFF)
            {
                lptsAuthTransport->uiPowerMode = POWER_MODE_OFF;
                lptsAuthTransport->bOperationAborted = TRUE;
            }
        #endif
            /* Disable IO before entering suspend state... */
            AuthIoDisable( PIO( lptsAuthTransport ) );

            /*clear all data on suspend*/
            for (uiCount=0; uiCount<MAX_BUFFER_NUMBER_IN_QUEUE; uiCount++)
            {
                MEMSET(lptsAuthTransport->pucOutputBuffer[uiCount], 0, MAX_BUFFER_NUMBER_IN_QUEUE);
                lptsAuthTransport->uiOutputBytes[uiCount] = 0;
            }
            
            break;
        }

        default:
            break;
    }
    
    /* If we get to this point, the request was not handled... */
    return TR_OK;
}



