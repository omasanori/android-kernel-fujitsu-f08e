







/*
 SiI8334 Linux Driver

 Copyright (C) 2011 Silicon Image Inc.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation version 2.

 This program is distributed .as is. WITHOUT ANY WARRANTY of any
 kind, whether express or implied; without even the implied warranty
 of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the
 GNU General Public License for more details.
*/
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/
/**
 * @file mhl_linuxdrv.h
 *
 * @brief Main header file of the MHL Tx driver.
 *
 * $Author: Dave Canfield
 * $Rev: $
 * $Date: Jan 20, 2011
 *
 *****************************************************************************/


#if !defined(MHL_LINUXDRV_H)
#define MHL_LINUXDRV_H
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include "sii_hal.h"


#if defined(CONFIG_ARCH_APQ8064)
#include <mach/board.h>
#endif /* deined(CONFIG_ARCH_APQ8064) */

#ifdef __cplusplus 
extern "C" { 
#endif  /* _defined (__cplusplus) */

/***** macro definitions *****************************************************/
#if defined(MAKE_9244_DRIVER)

#define MHL_PART_NAME "Sil-9244"
#define MHL_DRIVER_NAME "sii9244drv"
#define MHL_DRIVER_DESC "Sil-9244 MHL Tx Driver"
#define MHL_DEVICE_NAME "siI-9244"

#elif defined(MAKE_8334_DRIVER)

#define MHL_PART_NAME "SiI8334"	// WARNING! Must match I2C_BOARD_INFO() setting in board-omap3beagle.c
#define MHL_DRIVER_NAME "sii8334drv"
#define MHL_DRIVER_DESC "Sil-8334 MHL Tx Driver"
#define MHL_DEVICE_NAME "siI-833x"

#elif defined(MAKE_9232_DRIVER)

#define MHL_PART_NAME "Sil-9232"
#define MHL_DRIVER_NAME "sii9232drv"
#define MHL_DRIVER_DESC "Sil-9232 MHL Tx Driver"
#define MHL_DEVICE_NAME "siI-9232"

#else

#error "Need to add name and description strings for new drivers here!"

#endif



#define MHL_DRIVER_MINOR_MAX   1
#define EVENT_POLL_INTERVAL_30_MS	30

/***** public type definitions ***********************************************/

#define MHL_STATE_FLAG_CONNECTED	0x01	// MHL connection is estabished
#define MHL_STATE_FLAG_RCP_READY	0x02	// connection ready for RCP requests
#define MHL_STATE_FLAG_RCP_SENT		0x04	// last RCP event was a key send
#define MHL_STATE_FLAG_RCP_RECEIVED	0x08	// last RCP event was a key code receive
#define MHL_STATE_FLAG_RCP_ACK		0x10	// last RCP key code sent was ACK'd
#define MHL_STATE_FLAG_RCP_NAK		0x20	// last RCP key code sent was NAK'd


struct detect_callback {
	int (*func)(int, void*);
	void* arg;
};

struct notify_data {
	struct work_struct workq;
	int event;
};

enum {
	MHL_NOTIFYWQ_DETECTREQ = 0,
	MHL_NOTIFYWQ_DETECT,
	MHL_NOTIFYWQ_DISCONNECT,
	MHL_NOTIFYWQ_RGNDDETECTTIMER,
	MHL_NOTIFYWQ_CONNECTTIMER,
	MHL_NOTIFYWQ_ERROR,
	MHL_NOTIFYWQ
};

enum {
	MHL_MAINSTATE_INIT = 0,
	MHL_MAINSTATE_IDLE,
	MHL_MAINSTATE_DETECT,
	MHL_MAINSTATE_CONNECT,
	MHL_MAINSTATE_DISCONNECT
};

enum {
	MHL_SUBSTATE_INIT = 0,
	MHL_SUBSTATE_RGND_DETECT
};

enum {
	MHL_CONFIRM_USB_HOST = 0,
	MHL_CONFIRM_MHL,
	MHL_ERROR,
	MHL_DETECT_OUT,
	MHL_RGNDDETECT_TIMEOUT,
	MHL_CONNECT_TIMEOUT,
	MHL_RGND_DETECT,
	MHL_RGND_DETECT_OPEN,
	MHL_EVENT_COUNT
};

typedef struct {
	struct task_struct	*pTaskStruct;
	struct device		*pDevice;			// pointer to the driver's device object
	uint8_t				flags;				// various state flags
	uint8_t				keyCode;			// last RCP key code sent or received.
	uint8_t				errCode;			// last RCP NAK error code received
	uint8_t				devCapOffset;		// last Device Capability register
											// offset requested
	uint8_t				pendingEvent;		// event data wait for retrieval
	uint8_t				pendingEventData;	// by user mode application
	int						mainstate;
	rwlock_t				statelock;
	int						substate;
	bool					i2cerror;
	struct notify_data		notifyworkq[MHL_NOTIFYWQ];
	struct detect_callback	detectfunc;
	struct timer_list		rgnddetect_timer;
	struct timer_list		connect_timer;
	struct workqueue_struct	 *fjworkqueue;
	struct wake_lock		wakelock;
	bool					suspend;
#if defined(CONFIG_ARCH_APQ8064)
	struct fj_mhl_platform_data	*pd;
#endif /* deined(CONFIG_ARCH_APQ8064) */
} MHL_DRIVER_CONTEXT_T, *PMHL_DRIVER_CONTEXT_T;


/***** global variables ********************************************/

extern MHL_DRIVER_CONTEXT_T gDriverContext;


/***** public function prototypes ********************************************/
/**
 * \defgroup driver_public_api Driver Public API
 * @{
 */


/**
 * @brief Open the MHL transmitter device
 */
int32_t SiiMhlOpen(struct inode *pInode, struct file *pFile);


/**
 * @brief Close the MHL transmitter device
 */
int32_t SiiMhlRelease(struct inode *pInode, struct file *pFile);


/**
 * @brief IOCTL handler
 */
long SiiMhlIoctl(struct file *pFile, unsigned int ioctlCode,
					unsigned long ioctlParam);

/**
 * @}
 * end driver_public_api group
 */



#ifdef __cplusplus
}
#endif  /* _defined (__cplusplus) */

#endif /* _defined (MHL_LINUXDRV_H) */


