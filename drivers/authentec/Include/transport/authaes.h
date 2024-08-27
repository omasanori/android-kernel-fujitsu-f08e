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

   File:            authtraes.h

   Description:     This file contains structures and definitions
                    for use with the AuthenTec Unified Driver Model.

   Author:          James Deng

   ************************************************************************* */
#ifndef _AUTH_AES_H_
#define _AUTH_AES_H_

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

/** include files **/

/** local definitions **/

/* Generic device driver entry point information: */
typedef struct tsAUTHAES_s
{
    /*
    ** Size of the tsAUTHAES structure in bytes
    */
    uint32              uiStructSize;

    /*
    ** Number of open driver handles. The device driver should
    ** be written to allow only 1 device to be opened by an
    ** application for RW access at a time. If additional open
    ** requests are made, they should fail.
    */
    uint32              uiOpenCount;

    /*
    ** User defined, can point to additional user allocated structure for
    ** implementation specific details if required. User is responsible
    ** for allocation and de-allocation of memory.
    */
    void *              pvUser;

} tsAUTHAES;
typedef tsAUTHAES FAR *lptsAUTHAES;

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/
uint32
AES_Init (void *    lpvInit);

BOOL
AES_Deinit (void *  lpvContext);

uint32
AES_Open (void *    lpvContext,
          uint32    uiAccess,
          uint32    uiShareMode);

BOOL
AES_Close (void *   lpvContext);

uint32
AES_Read (void *    lpvContext,
          void *    lpvBuf,
          uint32    uiBytes);

uint32
AES_Write (void *   lpvContext,
           void *   lpvParams,
           uint32   uiBytes );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTH_AES_H_ */
