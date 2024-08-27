 /* /=======================================================================\
  * |                  AuthenTec Embedded (AE) Software                     |
  * |                                                                       |
  * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
  * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
  * |                                                                       |
  * |    Copyright (C) 2006-2009, AuthenTec, Inc. - All Rights Reserved.    |
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

   File:            authtrace.c

   Description:     This file contains funcions that can be used to do 
                    logging and tracing. These functions capture a trace 
                    file in memory by using the underlying file systme.
                    The trace file can be copied to a PC and be used to 
                    examine what is happening.

   Author:          James Deng

   ************************************************************************* */

/** include files **/
#include "hfiles.h"

/** local definitions **/
#if (UDM_USE_OS_LINUX != UDM_USE_OS)
    #error "This authtrace file is only for linux OS"
#endif

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

Function AuthDbgPrint

Author:  James Deng

The AuthDbgPrint() function prints out the ASCII string passed in.

Parameters

this function takes variable length parameters.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void AuthDbgPrint(const int8 * format, ...)
{
#if defined(_TRACING_ENABLED_) || defined(_ERROR_TRACING_ENABLED_)
    va_list args;
    int r;
    const char* fmt = (const char*)format;

    va_start(args, fmt);
    r = vprintk(fmt, args);
    va_end(args);
    
    return;

#else
    return;
#endif
}



/*++
===============================================================================

Function AuthDbgPrintW

Author:  James Deng

The AuthDbgPrintW() function prints out the unicode string passed in. 

Parameters

this function takes variable length parameters.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void AuthDbgPrintW(const uint16 * format, ...)
{
#if defined(_TRACING_ENABLED_) || defined(_ERROR_TRACING_ENABLED_)
    /*Note: no unicode string is supported in the linux driver, so all string is defined 
    to ASCII string. So the actual parameters would be in ASCII format
    and we can call AuthDbgPrint to output it*/
    return AuthDbgPrint((const int8 *)format);
#else
    return;
#endif
}





