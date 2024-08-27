 /* /=======================================================================\
  * |                  AuthenTec Embedded (AE) Software                     |
  * |                                                                       |
  * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
  * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
  * |                                                                       |
  * |    Copyright (C) 2000-2009, AuthenTec, Inc. - All Rights Reserved.    |
  * |                                                                       |
  * |  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF    |
  * |  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO  |
  * |  THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND/OR FITNESS FOR A       |
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

   File: acpstype.h-- Basic numeric type definitions for AE.  These 
                      definitions are platform and compiler dependant.  

   ************************************************************************* */

#ifndef __ACPSTYPE_H__
#define __ACPSTYPE_H__

#ifndef _Gentypes_H
struct s_I64                    /* This type is currently not used except */
{                               /* .. in function prototypes.  This is */
	unsigned long u32[2];       /* .. defined to be an array of 2 32-bit */
};                              /* .. values so that we can use strict ANSI.*/

typedef unsigned char  uint8;	/* Unsigned  8-bit integer. */
typedef unsigned short uint16;	/* Unsigned 16-bit integer. */
typedef unsigned long  uint32;	/* Unsigned 32-bit integer. */
//typedef struct   s_I64 uint64;	/* Unsigned 64-bit integer. */      
typedef long long unsigned int uint64;

typedef signed   char  int8;	/* Signed    8-bit integer. */
typedef signed   short int16;	/* Signed   16-bit integer. */
typedef signed   long  int32;	/* Signed   32-bit integer. */
//typedef struct   s_I64 int64;	/* Signed   64-bit integer. */      
typedef long long signed int int64;

#define MAX_INT32 0x7fffffff

/*
 * The following 4 types are "index" types, and are used for loop indices and
 * indexing arrays.  These are normally defined to be native int's, as
 * these are typically the most efficient on a system.  A "indx" and "uindx"
 * is assumed to be no more than 16 bits when it is used. A "mini index" 
 * (mindx and umindx) is assumed to be no more than 8 bits when it is used.
 * This will normally only really be an 8-bit type on a very small 8-bit 
 * processor, in which 16-bit operations are costly.
 */

typedef signed   int   indx;	/* native integer type, at least 16 bits.*/
typedef signed   int   mindx;	/* native integer type, at least 8 bits. */
typedef unsigned int   uindx;   /* (unsigned version) */
typedef unsigned int   umindx;	/* (unsigned version) */

typedef int            BOOL;    /* Boolean value TRUE/FALSE. */
typedef uint8          bool8;	/* Boolean value TRUE/FALSE.*/
typedef uint32	       bool32;	/* Boolean value TRUE/FALSE.*/

typedef char           char8;   /* This will typically be signed, but we let 
                                   it be whatever the compiler default is to 
                                   avoid compiler warnings when using string 
                                   constants. Care must be taken to use this 
                                   only for characters and not for general
                                   byte variables. */
#endif /*_Gentypes_H */

/*
 *   The following define (SIZEOFCHAR) is very important.  It must be set to 
 *   the size in bits of a "char" (int8 and uint8).  The define is tested in 
 *   a few places in the AuthenTec Matcher to perform optimizations that work 
 *   on processors that support 8-Bit operations.  For processors such as DSPs 
 *   that may not support an 8-Bit char and use 16 bits or 32 bits instead to 
 *   represent items of type "char", the symbol below must be set to the size 
 *   used for char.
 */

#define SIZEOFCHAR 8                  /* Size of a "char" on this system. */


/*
 *   The following is used to support FAR pointers on x86 machines.
 *   Define FAR to nothing for machines that do not require FAR pointers.
 *   Defime MEMCPY and MEMSET to their normal lower case names for
 *   systems that don't require far pointers.
 */

#undef  FAR
#define FAR

#define MEMCPY  memcpy
#define MEMSET  memset
#define MEMMOVE memmove

/*
 *   The following allow the implementor to implement custom memory
 *   support.  For systems that support normal heaps simply define
 *   these symbols to their normal lower case names.  Of course, if
 *   the implementor choses to replace the normal malloc and free,
 *   the replacements must have the same arguments and return
 *   as the normal malloc and free.
 */

#define MALLOC  AEPSMalloc
#define FREE    AEPSFree


/*
 *   Null-out the ATASSERT macro.
 */

#define ATASSERT(a) {}

/*
 *	Define AT_RESULT_CODE, since we do not have the AT tree.
 */
#ifndef _Gentypes_H
typedef int16 AT_RESULT_CODE; /* AT function return value */
#endif /* _Gentypes_H */

/*
 * Template I/O Comcode routines need to know whether we have a big-endian
 * or little-endian machine.
 */

#define AT_IS_INTEL_FORMAT

/*
 * Time-critical function indicator.  Not used in this platform.
 */

#define TIME_CRITICAL

#endif /* __ACPSTYPE_H__ */

