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

/* *************************************************************************

   File:            AuthTransport.h

   Description:     This file contains function prototypes and definitions
                    for use with the AuthenTec Unified Transport Driver Model.

   Author:          James Deng

   ************************************************************************* */
#ifndef _AUTH_TRANSPROT_H_
#define _AUTH_TRANSPROT_H_

#ifdef __cplusplus
extern "C"
 {
#endif  /* __cplusplus */

/** include files **/

/** local definitions **/


#if (TRANSPORT_USE_OS == TRANSPORT_USE_OS_LINUX)

/* The following symbols represent the IOCTL codes used in this library. */
#define AUTH_IOCTL_BASE ('x')

#define AUTH_IOCTL_TRANSPORT_OPEN       _IOWR( AUTH_IOCTL_BASE,  0, tsTrOpenCloseIoctlBuffer )
#define AUTH_IOCTL_TRANSPORT_CLOSE      _IOWR( AUTH_IOCTL_BASE,  1, tsTrOpenCloseIoctlBuffer )
#define AUTH_IOCTL_TRANSPORT_RESET      _IOWR( AUTH_IOCTL_BASE,  2, tsTrResetIoctlBuffer )
#define AUTH_IOCTL_CHECK_PROTOCOL       _IOWR( AUTH_IOCTL_BASE,  3, tsTrCheckProtocolIoctlBuffer )
#define AUTH_IOCTL_GET_DRVINFO          _IOWR( AUTH_IOCTL_BASE,  4, tsTrDriverInfoIoctlBuffer )
#define AUTH_IOCTL_GET_DRIVER_POWER     _IOWR( AUTH_IOCTL_BASE,  5, tsTrDriverPowerIoctlBuffer )
#define AUTH_IOCTL_SET_DRIVER_POWER     _IOWR( AUTH_IOCTL_BASE,  6, tsTrDriverPowerIoctlBuffer )
#define AUTH_IOCTL_SETUP_TRANSFER       _IOWR( AUTH_IOCTL_BASE,  7, tsTrSetupTransferIoctlBuffer )
#define AUTH_IOCTL_CANCEL_TRANSFER      _IOWR( AUTH_IOCTL_BASE,  8, tsTrCancelIoctlBuffer )
#define AUTH_IOCTL_STOP_TRANSFER        _IOWR( AUTH_IOCTL_BASE,  9, tsTrStopIoctlBuffer )
#define AUTH_IOCTL_SET_TIMEOUT          _IOWR( AUTH_IOCTL_BASE, 10, tsTrTimeoutIoctlBuffer )
#define AUTH_IOCTL_TRANSPORT_EXT      _IOWR( AUTH_IOCTL_BASE, 11, tsTrIoctlBuffer )

#define AUTH_IOCTL_OS_SUSPEND       _IOWR( AUTH_IOCTL_BASE, 12, __u32 )
#define AUTH_IOCTL_OS_RESUME   _IOWR( AUTH_IOCTL_BASE, 13, __u32 )

#elif (TRANSPORT_USE_OS == TRANSPORT_USE_OS_WINMOBILE)
/*TODO: define IOCTL for Win Mobile*/
#define AUTH_IOCTL_TRANSPORT_OPEN       
#define AUTH_IOCTL_TRANSPORT_CLOSE    
#define AUTH_IOCTL_TRANSPORT_RESET     
#define AUTH_IOCTL_CHECK_PROTOCOL    
#define AUTH_IOCTL_GET_DRVINFO        
#define AUTH_IOCTL_GET_DRIVER_POWER    
#define AUTH_IOCTL_SET_DRIVER_POWER     
#define AUTH_IOCTL_SETUP_TRANSFER       
#define AUTH_IOCTL_CANCEL_TRANSFER     
#define AUTH_IOCTL_STOP_TRANSFER      
#define AUTH_IOCTL_SET_TIMEOUT       
#define AUTH_IOCTL_TRANSPORT_EXT      

#define AUTH_IOCTL_OS_SUSPEND    
#define AUTH_IOCTL_OS_RESUME   

#elif (TRANSPORT_USE_OS == TRANSPORT_USE_OS_SYMBIAN)
/*TODO: define IOCTL for Symbian*/
#define AUTH_IOCTL_TRANSPORT_OPEN       
#define AUTH_IOCTL_TRANSPORT_CLOSE    
#define AUTH_IOCTL_TRANSPORT_RESET     
#define AUTH_IOCTL_CHECK_PROTOCOL    
#define AUTH_IOCTL_GET_DRVINFO        
#define AUTH_IOCTL_GET_DRIVER_POWER    
#define AUTH_IOCTL_SET_DRIVER_POWER     
#define AUTH_IOCTL_SETUP_TRANSFER       
#define AUTH_IOCTL_CANCEL_TRANSFER     
#define AUTH_IOCTL_STOP_TRANSFER      
#define AUTH_IOCTL_SET_TIMEOUT       
#define AUTH_IOCTL_TRANSPORT_EXT      

#define AUTH_IOCTL_OS_SUSPEND    
#define AUTH_IOCTL_OS_RESUME   

#elif (TRANSPORT_USE_OS == TRANSPORT_USE_OS_WIN32)
/*TODO: define IOCTL for Win32*/
#define AUTH_IOCTL_TRANSPORT_OPEN       
#define AUTH_IOCTL_TRANSPORT_CLOSE    
#define AUTH_IOCTL_TRANSPORT_RESET     
#define AUTH_IOCTL_CHECK_PROTOCOL    
#define AUTH_IOCTL_GET_DRVINFO        
#define AUTH_IOCTL_GET_DRIVER_POWER    
#define AUTH_IOCTL_SET_DRIVER_POWER     
#define AUTH_IOCTL_SETUP_TRANSFER       
#define AUTH_IOCTL_CANCEL_TRANSFER     
#define AUTH_IOCTL_STOP_TRANSFER      
#define AUTH_IOCTL_SET_TIMEOUT       
#define AUTH_IOCTL_TRANSPORT_EXT      

#define AUTH_IOCTL_OS_SUSPEND    
#define AUTH_IOCTL_OS_RESUME   

#endif


/* Generic driver information, plus the size and location of the swipe buffer. */
typedef struct tsAUTHTRANSPORT_s
{
    /*
    ** Size of the tsAUTHUDM structure in bytes
    */
    uint32              uiStructSize;

    /*
    ** Number of bytes per packet expected in data buffers
    */
    uint32              uiBytes;
    /*
    ** Number of expected packets per data buffer
    */
    uint32              uiNShot;

    uint32              uiReadPolicy;

    uint32              uiOptions;

    BOOL                bPendingInterrupt;

    /*buffer queue that for application to read*/
    uint8  *            pucOutputBuffer[MAX_BUFFER_NUMBER_IN_QUEUE];

    /*number of bytes in the corrisponding output buffer*/
    uint32              uiOutputBytes[MAX_BUFFER_NUMBER_IN_QUEUE];

    tsTrInitParams  stTrInitParams;

    uint32              uiError;

    tsDriverInfo       stDriverInfo;

    uint32              uiPowerMode;

    BOOL            bUserReadActive;

    BOOL            bTransferCanceled;

    uint32             uiProtocolID;

    BOOL            bOperationAborted;

    /*
    ** Driver timeout setting: for the convenience of the driver.
    */
    uint32              uiMillisecondsWait;

    void *              pvLock;

    /*
    ** Pointer to be used for AuthOS Event related functions. As this is
    ** dependent on the implementation, it can be used as a single variable
    ** or to point to a structure containing more information. The AuthOS
    ** is responsible for allocating and deallocating memory if this member
    ** points to another structure.
    */
    void *              lpvEvent;


} tsAUTHTRANSPORT;
typedef tsAUTHTRANSPORT FAR *lptsAUTHTRANSPORT;


/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/


int32 AuthTransportInit(lptsAUTHTRANSPORT lptsAuthTransport);

int32 AuthTransportDeinit(lptsAUTHTRANSPORT lptsAuthTransport);

uint32 AuthTransportRead(lptsAUTHTRANSPORT lptsAuthTransport, void * lpvBuf, uint32 uiBytes);

int32 AuthTransportWrite(lptsAUTHTRANSPORT lptsAuthTransport, uint8 * lpucWrite, uint32 uiBytes);

int32 AuthTransportIOControl(void * lpvContext, uint32 uiIoctl, uint8 * lpucInBuf, uint32 uiInBufLength, uint8 * lpucOutBuf, uint32 uiOutBufLength, uint32 * lpuiBytesTransferred);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTH_TRANSPROT_H_ */
