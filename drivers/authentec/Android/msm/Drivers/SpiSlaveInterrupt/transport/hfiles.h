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
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/
/* *************************************************************************

   File:            hfiles.h

   Description:     This file contains the list of header files used by
                    the transport.

   Author:          James Deng

   ************************************************************************* */


#ifndef _AUTH_HFILES_H_
#define _AUTH_HFILES_H_


/** include files **/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/freezer.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/cdev.h>

/* FUJITSU:2011-12-12 FingerPrintSensor add start */
#include <linux/semaphore.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <mach/gpio.h>
/* FUJITSU:2011-12-12 FingerPrintSensor add end */

/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-S */
#include <linux/nonvolatile_common.h>
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-E */


/******************************************************************************
**                      These files are for sensor support                   **
******************************************************************************/

#include "acpstype.h"           /* basic data types                          */

#include "TransportCommon.h"
#include "PlatCfg.h"                    /* Driver configuration info*/

/*
** The following structure MUST be defined BEFORE
** including the "authcontext.h" header to flesh
** out the unified device driver context.
*/

/* Platform specific / driver specific information: */
typedef struct _tsAUTHPLATFORM
{
    /*
    ** Size of the tsAUTHPLATFORM structure in bytes
    */
    uint32       uiStructSize;

    /*
    ** The rest of the information in this structure is
    ** user defined...
    */

   /*
    ** Handle to file for mapping
    */
  int32          hFileBuffer;

    /*
    ** pid of the user space process which opens the driver
    */
    int32         iPid;

} tsAUTHPLATFORM, FAR *lptsAUTHPLATFORM;


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


/******************************************************************************
**                  Required header files for using the Transport                  **
******************************************************************************/

/* common */
#include "authtransport.h"            /* Transport structures, prototypes and defs*/

/* CPU specific */
#include "authdma.h"            /* generic DMA structures, prototypes and defs          */
#include "authgpio.h"           /* generic GPIO structures, prototypes and defs         */
#include "authio.h"             /* generic IO structures, prototypes and defs           */

/* OS specific */
#include "authaes.h"            /* generic entry point structures, prototypes and defs  */
#include "authos.h"             /* generic OS structures, prototypes and defs           */
#include "authbuffer.h"         /* generic buffer structures, prototypes and defs       */
#include "authisr.h"            /* generic ISR structures, prototypes and defs          */

/* Transport */
#include "authhal.h"            /* HAL structures, prototypes and defs                  */

#include "authcontext.h"        /* Device driver context                                */



/** local definitions **/

//#undef offsetof
//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/*
#ifndef FAR
#define FAR
#endif
*/


#ifndef MIN
#define MIN(x,y)  (((x) < (y)) ? (x) : (y))
#endif


#if 0
#ifndef MEMSET
#define MEMSET  memset     /* MEMSET(dest, val, count) */
#endif

#ifndef MEMCPY
#define MEMCPY  memcpy     /* MEMCPY(dest, source, count) */
#endif

#ifndef MEMMOVE
#define MEMMOVE memmove    /* MEMMOVE(dest, source, count) */
#endif
#endif

/* FUJITSU:2011-12-12 FingerPrintSensor add start */
//#define _DEBUG_PROC_
#define _ERROR_TRACING_ENABLED_
#define debug_printk(format, arg...) \
    if (0) printk("%s" format, KERN_DEBUG, ##arg)
/* FUJITSU:2011-12-12 FingerPrintSensor add end */

/** default settings **/

/** common typedefs **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

/** private functions **/

/** public functions **/

#endif /* _AUTH_HFILES_H_ */

