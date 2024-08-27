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

/* *************************************************************************

   File:            AuthHalSlaveInterrupt.c

   Description:     The following functions define the API interface between
                    the AUTHTRANSPORT and the HAL. 
                    
                    This specific implementation of the HAL is targeted to
                    the SPI-Slave interrupt driven interface.

   Author:          James Deng

   ************************************************************************* */

/** include files **/
#include "hfiles.h"

/** local definitions **/
#define MAX_WRITE_BUFFER_SIZE 48*1024

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

/*++
===============================================================================

Function AuthHalSetupTransfer

The AuthHalSetupTransfer() function conveys information about the expected
exchange of data between the sensor and the microprocessor. It is provided as
a convenience for platforms that support DMA and ISR based transfers. It is
also used to indicate when transfers are finished (so DMA hardware can be
reset).

NOTE: Proper operation of the AuthUdmProcessData() function requires the
AuthHalSetupTransfer() function to place uiPacketLen and uiNShot values back
into the lptsAUTHTRANSPORT context.

Parameters

lpvContext (input)          Pointer to the driver context.

uiPacketLen (input)         The number of bytes for the DMA or ISR to transfer.
                            If uiPacketLen is set to zero, the current transfer
                            will be stopped. The value in uiPacketLen
                            represents the number of bytes in a single packet,
                            but there may be more than one packet per transfer.

uiNShot (input)             The number of packets per data buffer. A value of 2
                            or more indicates that there will be multiple
                            buffers that need to be processed per transfer.
                            Multiple packets per buffer increases system
                            throughput by reducing interrupt overhead. A value
                            of zero will be interpreted to mean there is one
                            packet per buffer.

uiUseChain (input)          When non-zero, the data transfer should be set up
                            as a ping-pong / chained  transfers with 2 buffers.
                            Each buffer will have (uiPacketLen * uiNShot) bytes
                            of DMA data. Every time one of the ping-pong
                            buffers fills up, the data should be processed. If
                            this function is called with uiUseChain set to
                            non-zero, there will be no subsequent calls to this
                            function until the transfer is stopped.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

int32
AuthHalSetupTransfer(IN void *lpvContext, 
                                            IN uint32 uiPacketLen, 
                                            IN uint32 uiNShot, 
                                            IN BOOL bKeepOn,
                                            IN uint32  uiReadPolicy,
                                            IN uint32  uiOptionFlags)
{
    lptsAUTHTRANSPORT         lptsAuthTransport;        /* Pointer to Transport Context            */

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

    AuthOsLock(lptsAuthTransport, 0);
    lptsAuthTransport->uiBytes = uiPacketLen;
    lptsAuthTransport->uiNShot = uiNShot;
    lptsAuthTransport->uiReadPolicy = uiReadPolicy;
    lptsAuthTransport->uiOptions = uiOptionFlags;
    AuthOsUnlock(lptsAuthTransport, 0);

    return TR_OK;

}


/*++
===============================================================================

Function AuthHalWriteSensor

The AuthHalWriteSensor() function is responsible for writing uiBytes worth of
data starting at lpucWrite. If lpucRead is non-null, the corresponding byte
read for each byte written should be stored and returned to the caller.

Parameters

lpvContext (input)          Pointer to the driver context.

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

Return Value                The number of bytes written indicates success.
                            A value of -1 indicates failure.

===============================================================================
--*/

uint32
AuthHalWriteSensor (void *          lpvContext,
                    const uint8 *   lpucWrite,
                    uint32          uiBytes,
                    uint8 *         lpucRead)
{
    uint32          uiRetval = 0;              /* Returned status */
    lptsAUTHTRANSPORT     lptsAuthTransport = NULL;        /* Pointer to the Transport context */
    lptsAUTHIO      lptsAuthIO = NULL;         /* Pointer to the IO context */


    /* Point to the Transport context... */
    lptsAuthTransport   = PTRANSPORT(lpvContext);

    /* Pointer to the IO context... */
    lptsAuthIO = PIO(lpvContext);

    uiRetval = AuthIoWriteReadSensor(PIO(lpvContext),
                                     lpucWrite,
                                     uiBytes,
                                     lpucRead,
                                     FALSE);


    return uiRetval;
}


/*++
===============================================================================

Function AuthHalInit

The AuthHalInit() function is used to perform all platform specific
initializations, such as, but not limited to, transport initialization, memory
allocation, and hardware initialization. All information that is required for
driver operation should be contained within the driver's context, or as part
of the published AUTHTRANSPORT context.

Parameters

lpvContext (input)          Pointer to the driver context.

Return Value                An TR_RETURN value indicating no error or
                            specifying a particular error condition.  The value
                            TR_OK indicates that no error was encountered.
                            Negative values represent error conditions.

===============================================================================
--*/

int32
AuthHalInit (void *     lpvContext)
{
    lptsAUTHTRANSPORT     lptsAuthTransport;        /* Pointer to the Transport context       */
    lptsAUTHCONTEXT lptsAuthContext;    /* Pointer to device driver context */


    /* Are we pointing to a valid memory block? */
    if (NULL == lpvContext)
    {
        /* NULL Pointer - don't go any further... */
        return TR_INVALID_PARAMETER;
    }

    /* Point to the Transport context... */
    lptsAuthTransport = PTRANSPORT(lpvContext);

    /* Point to the device driver context... */
    lptsAuthContext = PCONTEXT(lpvContext);

    /**************************************************************************
    ** Allocate interface memory buffer(s):
    **
    **
    ** 4K is shown here for an approximation (rounded up to a power of 2)
    **
    ** For a typical SPI-Slave interrupt mode driver, you would want enough memory for:
    **      1 receive buffer (capture: 4 slices, HP nav: 10 slices), + pad
    **		1 transmit buffer (Dummy write to read back data), + pad
    **
    **************************************************************************/

    /*
    ** If a TX buffer is required, allocate it...
    */
    
    lptsAuthContext->sAuthBufferTX.uiBufferLength	= MAX_WRITE_BUFFER_SIZE;
    lptsAuthContext->sAuthBufferTX.uiFlags			= AB_NO_PHYSICAL;
    if ( !AuthBufferAllocate( PBUFFERTX(lpvContext) ) )
    {
        /* Allocation failure - don't go any further... */
        DBGERROR((L("AuthHalInit: AuthBufferAllocate(TX buffer) failed.\r\n")));
        return TR_INVALID_PARAMETER;
    }
   

    /*
    ** If GPIO's are required (and they almost certainly are),
    ** then initialize them and find out which ones are supported...
    */
    if ( !AuthGpioInit(PGPIO(lpvContext)))
    {
        /* Allocation failure - don't go any further... */
        DBGERROR((L("AuthHalInit: AuthGpioInit failed.\r\n")));
        return TR_INVALID_PARAMETER;
    }

    /*
    ** Perform any I/O channel initialization...
    */
    if ( !AuthIoInit( lpvContext ) )
    {
        /* Allocation failure - don't go any further... */
        DBGERROR((L("AuthHalInit: AuthIoInit failed.\r\n")));
        return TR_INVALID_PARAMETER;
    }

    /* Install an ISR if required... */
    if ( !AuthIsrInstall( lpvContext ) )
    {
        /* Allocation failure - don't go any further... */
        DBGERROR((L("AuthHalInit: AuthIsrInstall failed.\r\n")));
        return TR_INVALID_PARAMETER;
    }
    
    /* Everything succeeded, all systems initialized... */
    return ( TR_OK );
}


/*++
===============================================================================

Function AuthHalDeinit

The AuthHalDeinit() function should deallocate
memory, and free up any hardware resources that were needed by the driver.

Parameters

lpvContext (input)          Pointer to the driver context.

Return Value                An TR_RETURN value indicating no error or
                            specifying a particular error condition.  The value
                            TR_OK indicates that no error was encountered.
                            Negative values represent error conditions.

===============================================================================
--*/

int32
AuthHalDeinit (void *   lpvContext)
{
    lptsAUTHTRANSPORT     lptsAuthTransport;        /* Pointer to the Transport context       */

    /* Are we pointing to a valid memory block? */
    if (NULL == lpvContext)
    {
        /* NULL Pointer - don't go any further... */
        return TR_INVALID_PARAMETER;
    }

    /* Point to the Transport context... */
    lptsAuthTransport = PTRANSPORT(lpvContext);

    /**************************************************************************
    ** Release the platform interfaces...
    **************************************************************************/
    /*
    ** Release the ISR...
    */
    AuthIsrUninstall( PISR(lpvContext) );

    /*
    ** Release the I/O channel...
    */
    AuthIoDeinit( PIO(lpvContext) );

    /**************************************************************************
    ** De-allocate interface memory:
    **************************************************************************/
    AuthBufferRelease( PBUFFERTX(lpvContext)  );

	/*
    ** Release GPIO resources...
    */
    AuthGpioDeinit( PGPIO(lpvContext) );

    return TR_OK;
}

