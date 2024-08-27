 /* /=======================================================================\
  * |                  AuthenTec Embedded (AE) Software                     |
  * |                                                                       |
  * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
  * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
  * |                                                                       |
  * |   Copyright (C) 2006-2007, AuthenTec, Inc. - All Rights Reserved.     |
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

   File:            AuthBuffer.h

   Description:     This file contains structures and definitions
                    for use with the AuthenTec Unified Driver Model.

   Author:          Joe Tykowski

   ************************************************************************* */
#ifndef _AUTH_BUFFER_H_
#define _AUTH_BUFFER_H_

#ifdef __cplusplus
extern "C"
 {
#endif  /* __cplusplus */

/** include files **/

/** local definitions **/

/*
** Bit definitions used to refine the memory allocation methods
** to a specific task. If no physical address is required,
** the memory can be allocated off of the heap, and only the
** virtual address is stored in the structure.
*/
#define AB_NO_PHYSICAL              0x0001
#define AB_DMA_PHYSICAL             0x0002
#define AB_ISR_PHYSICAL             0x0004
#define AB_REG_VIRTUAL              0x0008


/* Generic memory buffer information: */
typedef struct tsAUTHBUFFER_s
{
    /*
    ** Size of the tsAUTHBUFFER structure in bytes
    */
    uint32              uiStructSize;

    /*
    ** Address, if required, of the physical location of the
    ** buffer in memory (for buffer sharing or DMA). This
    ** value can be NULL if physical addressing of the buffer
    ** is not required.
    */
    void *              pvPhysicalBuffer;

    /*
    ** Address of the virtual location of the buffer in
    ** memory. This value is used by the AUDM for ALL buffer
    ** accesses and must not be NULL.
    */
    void *              pvVirtualBuffer;

    /*
    ** The length of the memory region allocated for the
    ** buffer, in bytes.
    */
    uint32              uiBufferLength;

    /* Flags indicating the use for this buffer:
    **      AB_NO_PHYSICAL
    **      AB_DMA_PHYSICAL
    **      AB_ISR_PHYSICAL
    */
    uint32              uiFlags;


    /*
    ** User defined, can point to additional user allocated structure for
    ** implementation specific details if required. User is responsible
    ** for allocation and de-allocation of memory.
    */
    void *              pvUser;

} tsAUTHBUFFER;
typedef tsAUTHBUFFER FAR *lptsAUTHBUFFER;

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

BOOL AuthBufferAllocate(lptsAUTHBUFFER lptsAuthBuffer);

BOOL AuthBufferRelease(lptsAUTHBUFFER lptsAuthBuffer);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTH_BUFFER_H_ */
