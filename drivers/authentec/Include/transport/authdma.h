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

   File:            AuthDMA.h

   Description:     This file contains structures and definitions
                    for use with the AuthenTec Unified Driver Model.

   Author:          Joe Tykowski

   ************************************************************************* */
#ifndef _AUTH_DMA_H_
#define _AUTH_DMA_H_

#ifdef __cplusplus
extern "C"
 {
#endif  /* __cplusplus */

/** include files **/

/** local definitions **/

/* Generic interrupt service routine information: */
typedef struct tsAUTHDMA_s
{
    /*
    ** Size of the tsAUTHDMA structure in bytes
    */
    uint32              uiStructSize;


    /*
    ** Number of bytes per packet expected via a DMA transfer.
    */
    uint32              uiBytes;

    /*
    ** Indicates status of DMA channel:
    */
    uint32              uiFlags;

    /*
    ** User defined, can point to additional user allocated structure for
    ** implementation specific details if required. User is responsible
    ** for allocation and de-allocation of memory.
    */
    void *              pvUser;

} tsAUTHDMA;
typedef tsAUTHDMA FAR *lptsAUTHDMA;

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

BOOL AuthDmaAllocate(void * lpvContext);

BOOL AuthDmaRelease(void * lpvContext);

uint32 AuthDmaSetupTx(void * lpvContext, uint32 uiBytes);

void AuthDmaSetupRx(void * lpvContext, uint32 uiPacketLen, uint32 uiNShot, uint32 uiUseChain);

BOOL AuthDmaStart(lptsAUTHDMA lptsAuthDma);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTH_DMA_H_ */
