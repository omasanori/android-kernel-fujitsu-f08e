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

   File:            AuthHal.h

   Description:     This file contains function prototypes and definitions
                    for use with the AuthenTec Unified Driver Model. These
                    functions form the Hardware Abstraction Layer of the
                    Transport.

   Author:          Joe Tykowski

   ************************************************************************* */

#ifndef _AUTHHAL_H_
#define _AUTHHAL_H_

#ifdef __cplusplus
extern "C"
 {
#endif  /* __cplusplus */

/** include files **/

/** local definitions **/

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

int32 AuthHalSetupTransfer(IN void *lpvContext, 
                                            IN uint32 uiPacketLen, 
                                            IN uint32 uiNShot, 
                                            IN BOOL bKeepOn,
                                            IN uint32  uiReadPolicy,
                                            IN uint32  uiOptionFlags);

uint32 AuthHalWriteSensor(void * lpvContext, const uint8 * lpucWrite, uint32 uiBytes, uint8 *lpucRead);

int32 AuthHalInit(void * lpvContext);

int32 AuthHalDeinit(void * lpvContext);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTHHAL_H_ */
