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

   File:            AuthISR.h

   Description:     This file contains structures and definitions
                    for use with the AuthenTec Unified Driver Model.

   Author:          Joe Tykowski

   ************************************************************************* */
#ifndef _AUTH_ISR_H_
#define _AUTH_ISR_H_

#ifdef __cplusplus
extern "C"
 {
#endif  /* __cplusplus */

/** include files **/

/** local definitions **/

/* Generic interrupt service routine information: */
typedef struct tsAUTHISR_s
{
    /*
    ** Size of the tsAUTHISR structure in bytes
    */
    uint32              uiStructSize;

    /*
    ** This flag indicates taht the sensor supports touch-to-wake
    ** when TRUE.
    */
    uindx               bTouchToWake;

    /*
    ** User defined, can point to additional user allocated structure for
    ** implementation specific details if required. User is responsible
    ** for allocation and de-allocation of memory.
    */
    void *              pvUser;

} tsAUTHISR;
typedef tsAUTHISR FAR *lptsAUTHISR;

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

BOOL AuthIsrInstall(void * lpvContext);

BOOL AuthIsrUninstall(lptsAUTHISR lptsAuthISR);

void AuthIsrSetupReceive(void    * lpvContext, uint32 uiLength);

void AuthIsrReadOnRequest(void    * lpvContext, uint8* pucReadBuffer, uint32 uiLength);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTH_ISR_H_ */
