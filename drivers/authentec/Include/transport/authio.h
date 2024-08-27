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

   File:            AuthIO.h

   Description:     This file contains structures and definitions
                    for use with the AuthenTec Unified Driver Model.

   Author:          James Deng

   ************************************************************************* */
#ifndef _AUTH_IO_H_
#define _AUTH_IO_H_

#ifdef __cplusplus
extern "C"
 {
#endif  /* __cplusplus */

/** include files **/

/** local definitions **/

/* Generic I/O information: */
typedef struct tsAUTHIO_s
{
    /*
    ** Size of the tsAUTHIO structure in bytes
    */
    uint32              uiStructSize;

    void *              pvIoDevice;

    void *              pvLock;

    /*
    ** User defined, can point to additional user allocated structure for
    ** implementation specific details if required. User is responsible
    ** for allocation and de-allocation of memory.
    */
    void *              pvUser;

} tsAUTHIO;
typedef tsAUTHIO FAR *lptsAUTHIO;

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

BOOL AuthIoInit(void * lpvContext);

void AuthIoDeinit(lptsAUTHIO lptsAuthIO);

void AuthIoEnable(lptsAUTHIO lptsAuthIO);

void AuthIoDisable(lptsAUTHIO lptsAuthIO);

uint32 AuthIoWriteReadSensor(lptsAUTHIO lptsAuthIO, const uint8 * lpucWrite, uint32 uiBytes, uint8 * lpucRead, BOOL bExpectData);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTH_IO_H_ */
