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

   File:            AuthOS.h

   Description:     This file contains function prototypes and definitions
                    for use with the AuthenTec Unified Driver Model. These
                    functions form the Operating System Abstraction Layer
                    of the Transport.

   Author:          Joe Tykowski

   ************************************************************************* */

#ifndef _AUTHOS_H_
#define _AUTHOS_H_

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

int32 AuthOsCreateEvent(void * lpvContext);

int32 AuthOsWaitEvent(void * lpvContext, uint32 uiMilliseconds);

void AuthOsSetEvent(void * lpvContext);

void AuthOsCloseEvent(void * lpvContext);

int32 AuthOsCreateLock (void *   lpvContext, uint32 uiLockNumber);

int32 AuthOsDeleteLock (void *   lpvContext);

int32 AuthOsLock (void *   lpvContext, uint32 uiLockIndex);

int32 AuthOsUnlock (void *   lpvContext, uint32 uiLockIndex);

void AuthOsSleep(uint32 uiMilliseconds);

int32 AuthOsOutputKeyEvent(void * lpvContext, tsTrKeyEvent *pstKeyEvent);

int32 AuthOsOutputMouseEvent(void * lpvContext, tsTrMouseEvent *pstMouseEvent);



#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _AUTHOS_H_ */

