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

   File:            AuthGPIO.h

   Description:     This file contains structures and definitions
                    for use with the AuthenTec Unified Driver Model.

   Author:          James Deng

   ************************************************************************* */
#ifndef _AUTH_GPIO_H_
#define _AUTH_GPIO_H_

#ifdef __cplusplus
extern "C"
 {
#endif  /* __cplusplus */

/** include files **/

/** local definitions **/

/* General Purpose I/O information: */
typedef struct tsAUTHGPIO_s
{
    /*
    ** Size of the tsAUTHGPIO structure in bytes
    */
    uint32              uiStructSize;

    /*
    ** User defined, can point to additional user allocated structure for
    ** implementation specific details if required. User is responsible
    ** for allocation and de-allocation of memory.
    */
    void *              pvUser;

} tsAUTHGPIO;
typedef tsAUTHGPIO FAR *lptsAUTHGPIO;

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

BOOL AuthGpioInit(lptsAUTHGPIO lptsAuthGPIO);

void AuthGpioDeinit(lptsAUTHGPIO lptsAuthGPIO);

void AuthGpioReset(lptsAUTHGPIO lptsAuthGPIO);

int32 AuthGpioIsInterruptAsserted(lptsAUTHGPIO	lptsAuthGPIO);


uint32 AuthGpioWriteSensor (lptsAUTHGPIO   lptsAuthGPIO,
                            const uint8 *  lpucWrite,
                            uint32         uiBytes);

BOOL
AuthGpioSuspend (lptsAUTHGPIO lptsAuthGPIO);

BOOL
AuthGpioPowerOn (lptsAUTHGPIO lptsAuthGPIO, BOOL bON);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTH_GPIO_H_ */
