








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
// COPYRIGHT(C) FUJITSU LIMITED 2012 - 2013
/*----------------------------------------------------------------------------*/
/**
 * @file mhl_linuxdrv_main.c
 *
 * @brief Main entry point of the Linux driver for Silicon Image MHL transmitters.
 *
 * $Author: Dave Canfield
 * $Rev: $
 * $Date: Jan. 20, 2011
 *
 *****************************************************************************/

#define MHL_LINUXDRV_MAIN_C

/***** #include statements ***************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
//RG addition
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <linux/platform_device.h>

#include "si_c99support.h"
#include "si_common.h"
#include "mhl_linuxdrv.h"
#include "osal/include/osal.h"
#include "si_mhl_tx_api.h"
#include "si_drvisrconfig.h"
#include "si_cra.h"
#include "si_osdebug.h"

#include <linux/workqueue.h>
#include <linux/mhl/fj_mhl.h>
#if defined(CONFIG_ARCH_APQ8064)
#include <asm/system.h>
#include <linux/mfd/fj_charger.h>
/* FUJITSU:2012-08-02 MHL start */
//#include <linux/io.h>
//#include <mach/msm_smsm.h>
/* FUJITSU:2012-08-02 MHL end */
#endif /* defined(CONFIG_ARCH_APQ8064) */
#include <linux/delay.h>
#include <linux/ovp.h>

extern int makercmd_mode;

extern void mhl_detect_device(void);
void enable_mhl_interrupt(void);
void disable_mhl_interrupt(void);
#define CONNECT_TIMEOUT		(60*HZ*30)
#define RGNDDETECT_TIMEOUT	(2*HZ)




/***** local macro definitions ***********************************************/


/***** local variable declarations *******************************************/
static int32_t devMajor = 0;    /**< default device major number */
static struct cdev siiMhlCdev;
static struct class *siiMhlClass;
static char *buildTime = "Build: " __DATE__"-" __TIME__ "\n";
static char *buildVersion = "1.00.60";

//static struct device *siiMhlClassDevice;

//RG addition for keyboard
static struct input_dev *dev_keyboard;		// define keyboard structure
inline int init_keyboard(void);				// Linux file system. These devices can be read but,
static char *mhl_event[MHL_EVENT_COUNT] = {
	"CONFIRM_USB_HOST",
	"CONFIRM_MHL",
	"ERROR",
	"DETECT_OUT",
	"RGNDDETECT_TIMEOUT",
	"CONNECT_TIMEOUT",
	"RGND_DETECT",
	"RGND_DETECT_OPEN",
};

/***** global variable declarations *******************************************/

MHL_DRIVER_CONTEXT_T gDriverContext;

enum mhl_log_level fj_mhl_loglevel;

//#if defined(DEBUG)
unsigned char DebugChannelMasks[SII_OSAL_DEBUG_NUM_CHANNELS/8+1]={0xFF,0xFF,0xFF,0xFF};
//ulong DebugChannelMask = 0xFFFFFFFF;
module_param_array(DebugChannelMasks, byte, NULL, S_IRUGO | S_IWUSR);

ushort DebugFormat = SII_OS_DEBUG_FORMAT_FILEINFO;
module_param(DebugFormat, ushort, S_IRUGO | S_IWUSR);
//#endif

//RG addition
// Local, keyboard specific helper function for initialize keybaord input_dev
inline int init_keyboard(void)
{
    int error;
//    int i;

	dev_keyboard = input_allocate_device();
	if (!dev_keyboard)
	{
		printk(KERN_ERR "CDBdev_keyboard: Not enough memory\n");
		error = -ENOMEM;
		goto err_free_irq;
	}

	set_bit(EV_KEY, dev_keyboard->evbit);
	set_bit(EV_REP, dev_keyboard->evbit);			//driver doesn't use this but, can in the future

	dev_keyboard->phys = "atakbd/input0";
	dev_keyboard->id.bustype = BUS_HOST;

//RG add in supported keys...

	set_bit(KEY_UP, dev_keyboard->keybit);
	set_bit(KEY_DOWN, dev_keyboard->keybit);
	set_bit(KEY_LEFT, dev_keyboard->keybit);
	set_bit(KEY_RIGHT, dev_keyboard->keybit);
	set_bit(KEY_ENTER, dev_keyboard->keybit);
	set_bit(KEY_BACK, dev_keyboard->keybit);
	set_bit(KEY_PLAYPAUSE, dev_keyboard->keybit);
	set_bit(KEY_STOP, dev_keyboard->keybit);
	set_bit(KEY_SELECT, dev_keyboard->keybit);
	set_bit(KEY_OK,dev_keyboard->keybit);
	set_bit(KEY_REPLY,dev_keyboard->keybit);
	set_bit(KEY_PLAYCD,dev_keyboard->keybit);
	set_bit(KEY_STOPCD,dev_keyboard->keybit);

	set_bit(BTN_LEFT,dev_keyboard->keybit);
	set_bit(BTN_SELECT,dev_keyboard->keybit);


/*	for(i = 0;i<550;i++)
		set_bit(i, dev_keyboard->keybit);
*/
	dev_keyboard->id.bustype = BUS_USB;
	dev_keyboard->id.vendor  = 0x1095;
	dev_keyboard->id.product = 0x9244;
	dev_keyboard->id.version = 0xA;				//use version to distinguish mouse from keyboard

	error = input_register_device(dev_keyboard);
	if (error) {
			printk(KERN_ERR "CDBdev_keyboard: Failed to register device\n");
			goto err_free_dev;
	}

	printk(KERN_INFO "CDBdev_keyboard: driver loaded\n");

	return 0;

 err_free_dev:
         input_free_device(dev_keyboard);
 err_free_irq:
         return error;
}

/***** local functions *******************************************************/

/**
 *  @brief Start the MHL transmitter device
 *
 *  This function is called during driver startup to initialize control of the
 *  MHL transmitter device by the driver.
 *
 *  @return     0 if successful, negative error code otherwise
 *
 *****************************************************************************/
int32_t StartMhlTxDevice(void)
{
	halReturn_t		halStatus;
	SiiOsStatus_t	osalStatus;
	fj_SiiDebugPrint("%s\n", __func__);

//    pr_info("Starting %s\n", MHL_PART_NAME);
	fj_SiiDebugPrint("Starting %s\n", MHL_PART_NAME);

    // Initialize the Common Register Access (CRA) layer.
    if(!SiiCraInitialize())
    {
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Initialization of CRA layer failed!\n");
    	return -EIO;
    }

    // Initialize the OS Abstraction Layer (OSAL) support.
    osalStatus = SiiOsInit(0);
    if (osalStatus != SII_OS_STATUS_SUCCESS)
    {
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"Initialization of OSAL failed, error code: %d\n",osalStatus);
    	return -EIO;
    }


    halStatus = HalInit();
    if (halStatus != HAL_RET_SUCCESS)
    {
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"Initialization of HAL failed, error code: %d\n",halStatus);
    	SiiOsTerm();
    	return -EIO;
    }

    halStatus = HalOpenI2cDevice(MHL_PART_NAME, MHL_DRIVER_NAME);
    if (halStatus != HAL_RET_SUCCESS)
    {
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"Opening of I2c device %s failed, error code: %d\n",
    			MHL_PART_NAME, halStatus);
    	HalTerm();
    	SiiOsTerm();
    	return -EIO;
    }

    halStatus = HalInstallIrqHandler(SiiMhlTxDeviceIsr);
    if (halStatus != HAL_RET_SUCCESS)
    {
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"Initialization of HAL interrupt support failed, error code: %d\n",
    			halStatus);
    	HalCloseI2cDevice();
    	HalTerm();
    	SiiOsTerm();
    	return -EIO;
    }

    /* Initialize the MHL Tx code a polling interval of 30ms. */
	halStatus = HalAcquireIsrLock();
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"AcquireIsrLock failed, error code: %d\n", halStatus);
	}
	SiiMhlTxInitialize(EVENT_POLL_INTERVAL_30_MS);
    HalReleaseIsrLock();

    return 0;
}



/**
 *  @brief Stop the MHL transmitter device
 *
 *  This function shuts down control of the transmitter device so that
 *  the driver can exit
 *
 *  @return     0 if successful, negative error code otherwise
 *
 *****************************************************************************/
int32_t StopMhlTxDevice(void)
{
	halReturn_t		halStatus;

	halReturn_t		retStatus;
	fj_SiiDebugPrint("%s\n", __func__);

	pr_info("Stopping %s\n", MHL_PART_NAME);

	HalRemoveIrqHandler();

	halStatus = HalCloseI2cDevice();
	retStatus = halStatus;
    if (halStatus != HAL_RET_SUCCESS)
    {
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"Closing of I2c device failed, error code: %d\n",halStatus);
/*    	return -EIO;*/
    }

	halStatus = HalTerm();
    if (halStatus != HAL_RET_SUCCESS)
    {
    	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
    			"Termination of HAL failed, error code: %d\n",halStatus);
		if (retStatus == HAL_RET_SUCCESS)
			retStatus = halStatus;
/*    	return -EIO;*/
    }

	SiiOsTerm();
	return retStatus;
/*	return 0;*/
}




/***** public functions ******************************************************/

/**
 * @brief Handle read request to the connection_state attribute file.
 */
ssize_t ShowConnectionState(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	if(gDriverContext.flags & MHL_STATE_FLAG_CONNECTED) {
		return scnprintf(buf, PAGE_SIZE, "connected %s_ready",
				gDriverContext.flags & MHL_STATE_FLAG_RCP_READY? "rcp" : "not_rcp");
	} else {
		return scnprintf(buf, PAGE_SIZE, "not connected");
	}
}


/**
 * @brief Handle read request to the rcp_keycode attribute file.
 */
ssize_t ShowRcp(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	int		status = 0;

	if(HalAcquireIsrLock() != HAL_RET_SUCCESS)
	{
		return -ERESTARTSYS;
	}

	if(gDriverContext.flags &
		(MHL_STATE_FLAG_RCP_SENT | MHL_STATE_FLAG_RCP_RECEIVED))
	{
		status = scnprintf(buf, PAGE_SIZE, "0x%02x %s",
				gDriverContext.keyCode,
				gDriverContext.flags & MHL_STATE_FLAG_RCP_SENT? "sent" : "received");
	}

	HalReleaseIsrLock();
	return status;
}



/**
 * @brief Handle write request to the rcp_keycode attribute file.
 */
ssize_t SendRcp(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long	keyCode;
	int				status = -EINVAL;

	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "SendRcp received string: ""%s""\n", buf);

	if(HalAcquireIsrLock() != HAL_RET_SUCCESS)
	{
		return -ERESTARTSYS;
	}

	while(gDriverContext.flags & MHL_STATE_FLAG_RCP_READY) {

		if(strict_strtoul(buf, 0, &keyCode)) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Unable to convert keycode string\n");
			break;
		}

		if(keyCode >= 0xFE) {
//			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
//					"keycode (0x%x) is too large to be valid\n", keyCode);
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"keycode is too large to be valid\n");
			break;
		}

		gDriverContext.flags &= ~(MHL_STATE_FLAG_RCP_RECEIVED |
								  MHL_STATE_FLAG_RCP_ACK |
								  MHL_STATE_FLAG_RCP_NAK);
		gDriverContext.flags |= MHL_STATE_FLAG_RCP_SENT;
		gDriverContext.keyCode = (uint8_t)keyCode;
		SiiMhlTxRcpSend((uint8_t)keyCode);
		status = count;
		break;
	}

	HalReleaseIsrLock();

	return status;
}


/**
 * @brief Handle write request to the rcp_ack attribute file.
 *
 * This file is used to send either an ACK or NAK for a received
 * Remote Control Protocol (RCP) key code.
 *
 * The format of the string in buf must be:
 * 	"keycode=<keyvalue> errorcode=<errorvalue>
 * 	where:	<keyvalue>		is replaced with value of the RCP to be ACK'd or NAK'd
 * 			<errorvalue>	0 if the RCP key code is to be ACK'd
 * 							non-zero error code if the RCP key code is to be NAK'd
 */
ssize_t SendRcpAck(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long	keyCode = 0x100;	// initialize with invalid values
	unsigned long	errCode = 0x100;
	char			*pStr;
	int				status = -EINVAL;

	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "SendRcpAck received string: ""%s""\n", buf);

	// Parse the input string and extract the RCP key code and error code
	do {
		pStr = strstr(buf, "keycode=");
		if(pStr != NULL) {
			if(strict_strtoul(pStr + 8, 0, &keyCode)) {
				SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Unable to convert keycode string\n");
				break;
			}
		} else {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Invalid string format, can't "\
							"find ""keycode"" value\n");
			break;
		}

		pStr = strstr(buf, "errorcode=");
		if(pStr != NULL) {
			if(strict_strtoul(pStr + 10, 0, &errCode)) {
				SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Unable to convert errorcode string\n");
				break;
			}
		} else {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Invalid string format, can't "\
							"find ""errorcode"" value\n");
			break;
		}
	} while(false);

	if((keyCode > 0xFF) || (errCode > 0xFF)) {
/*		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Invalid key code or error code "\ */
/*						"specified, key code: 0x%02x  error code: 0x%02x\n",     */
/*						keyCode, errCode);                                       */
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Invalid key code or error code");
		return status;
	}

	if(HalAcquireIsrLock() != HAL_RET_SUCCESS)
	{
		return -ERESTARTSYS;
	}

	while(gDriverContext.flags & MHL_STATE_FLAG_RCP_READY) {

		if((keyCode != gDriverContext.keyCode)
			|| !(gDriverContext.flags & MHL_STATE_FLAG_RCP_RECEIVED)) {

			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Attempting to ACK a key code that was not received!\n");
			break;
		}

		if(errCode == 0) {
			SiiMhlTxRcpkSend((uint8_t)keyCode);
		} else {
			SiiMhlTxRcpeSend((uint8_t)errCode);
		}

		status = count;
		break;
	}

	HalReleaseIsrLock();

	return status;
}



/**
 * @brief Handle read request to the rcp_ack attribute file.
 *
 * Reads from this file return a string detailing the last RCP
 * ACK or NAK received by the driver.
 *
 * The format of the string returned in buf is:
 * 	"keycode=<keyvalue> errorcode=<errorvalue>
 * 	where:	<keyvalue>		is replaced with value of the RCP key code for which
 * 							an ACK or NAK has been received.
 * 			<errorvalue>	0 if the last RCP key code was ACK'd or
 * 							non-zero error code if the RCP key code was NAK'd
 */
ssize_t ShowRcpAck(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int				status = -EINVAL;

	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "ShowRcpAck called\n");

	if(HalAcquireIsrLock() != HAL_RET_SUCCESS)
	{
		return -ERESTARTSYS;
	}

	if(gDriverContext.flags & (MHL_STATE_FLAG_RCP_ACK | MHL_STATE_FLAG_RCP_NAK)) {

		status = scnprintf(buf, PAGE_SIZE, "keycode=0x%02x errorcode=0x%02x",
				gDriverContext.keyCode, gDriverContext.errCode);
	}

	HalReleaseIsrLock();

	return status;
}



/**
 * @brief Handle write request to the devcap attribute file.
 *
 * Writes to the devcap file are done to set the offset of a particular
 * Device Capabilities register to be returned by a subsequent read
 * from this file.
 *
 * All we need to do is validate the specified offset and if valid
 * save it for later use.
 */
ssize_t SelectDevCap(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned long	devCapOffset;
	int				status = -EINVAL;

	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "SelectDevCap received string: ""%s""\n", buf);

	do {

		if(strict_strtoul(buf, 0, &devCapOffset)) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
							"Unable to convert register offset string\n");
			break;
		}

		if(devCapOffset >= 0x0F) {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"dev cap offset (0x%x) is too large to be valid\n",
					devCapOffset);
			break;
		}

		gDriverContext.devCapOffset = (uint8_t)devCapOffset;

		status = count;

	} while(false);

	return status;
}



/**
 * @brief Handle read request to the devcap attribute file.
 *
 * Reads from this file return the hexadecimal string value of the last
 * Device Capability register offset written to this file.
 *
 * The return value is the number characters written to buf, or EAGAIN
 * if the driver is busy and cannot service the read request immediately.
 * If EAGAIN is returned the caller should wait a little and retry the
 * read.
 *
 * The format of the string returned in buf is:
 * 	"offset:<offset>=<regvalue>
 * 	where:	<offset>	is the last Device Capability register offset
 * 						written to this file
 * 			<regvalue>	the currentl value of the Device Capability register
 * 						specified in offset
 */
ssize_t ReadDevCap(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	uint8_t		regValue;
	int			status = -EINVAL;

	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "ReadDevCap called\n");

	if(HalAcquireIsrLock() != HAL_RET_SUCCESS)
	{
		return -ERESTARTSYS;
	}

	do {
		if(gDriverContext.flags & MHL_STATE_FLAG_CONNECTED) {

			status = SiiTxGetPeerDevCapEntry(gDriverContext.devCapOffset,
											 &regValue);
			if(status != 0) {
				// Driver is busy and cannot provide the requested DEVCAP
				// register value right now so inform caller they need to
				// try again later.
				status = -EAGAIN;
				break;
			}
			status = scnprintf(buf, PAGE_SIZE, "offset:0x%02x=0x%02x",
								gDriverContext.devCapOffset, regValue);
		}
	} while(false);

	HalReleaseIsrLock();

	return status;
}



ssize_t SelectLogLevel(struct device *dev, struct device_attribute *attr,
												const char *buf, size_t count)
{
	int status = -EINVAL;
	unsigned long	level;

	fj_SiiDebugPrint("%s\n", __func__);

	if (strict_strtoul(buf, 0, &level)) {
		return -EINVAL;
	}

	if (level < MHL_LOG_MAX) {
		fj_mhl_loglevel = level;
	} else {
		fj_SiiDebugPrint("Invalid log Level:%lu\n", level);
	}

	fj_SiiDebugPrint("Log Level:%d\n", (int)fj_mhl_loglevel);

	status = count;
	return status;
}

ssize_t ReadLogLevel(struct device *dev, struct device_attribute *attr,
																	  char *buf)
{
	int status = -EINVAL;

	fj_SiiDebugPrint("%s,%d\n", __func__, (int)fj_mhl_loglevel);
	status = scnprintf(buf, PAGE_SIZE, "%d\n", (int)fj_mhl_loglevel);
	return status;
}


#define MAX_EVENT_STRING_LEN 40
/*****************************************************************************/
/**
 * @brief Handler for MHL hot plug detect status change notifications
 *  from the MhlTx layer.
 *
 *****************************************************************************/
void  AppNotifyMhlDownStreamHPDStatusChange(bool_t connected)
{
	char	event_string[MAX_EVENT_STRING_LEN];
	char	*envp[] = {event_string, NULL};


	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
			"AppNotifyMhlDownStreamHPDStatusChange called, "\
			"HPD status is: %s\n", connected? "CONNECTED" : "NOT CONNECTED");

	snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=%s",
			connected? "HPD" : "NO_HPD");
	kobject_uevent_env(&gDriverContext.pDevice->kobj,
						KOBJ_CHANGE, envp);
}



/*****************************************************************************/
/**
 * @brief Handler for most of the event notifications from the MhlTx layer.
 *
 *****************************************************************************/
MhlTxNotifyEventsStatus_e  AppNotifyMhlEvent(uint8_t eventCode, uint8_t eventParam)
{
	char						event_string[MAX_EVENT_STRING_LEN];
	char						*envp[] = {event_string, NULL};
	MhlTxNotifyEventsStatus_e	rtnStatus = MHL_TX_EVENT_STATUS_PASSTHROUGH;

	if (eventCode != MHL_TX_EVENT_RCP_RECEIVED) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"AppNotifyEvent called, eventCode: 0x%02x eventParam: 0x%02x\n",
				eventCode, eventParam);
	} else {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"AppNotifyEvent called, RCP_RECEIVED\n");
	}

	// Save the info on the most recent event.  This is done to support the
	// SII_MHL_GET_MHL_TX_EVENT IOCTL.  If at some point in the future the
	// driver's IOCTL interface is abandoned in favor of using sysfs attributes
	// this can be removed.
	gDriverContext.pendingEvent = eventCode;
	gDriverContext.pendingEventData = eventParam;

	switch(eventCode) {

		case MHL_TX_EVENT_CONNECTION:
			gDriverContext.flags |= MHL_STATE_FLAG_CONNECTED;
			strncpy(event_string, "MHLEVENT=connected", MAX_EVENT_STRING_LEN);
			kobject_uevent_env(&gDriverContext.pDevice->kobj,
								KOBJ_CHANGE, envp);
			break;

		case MHL_TX_EVENT_RCP_READY:
			gDriverContext.flags |= MHL_STATE_FLAG_RCP_READY;
			strncpy(event_string, "MHLEVENT=rcp_ready", MAX_EVENT_STRING_LEN);
			kobject_uevent_env(&gDriverContext.pDevice->kobj,
								KOBJ_CHANGE, envp);
			break;

		case MHL_TX_EVENT_DISCONNECTION:
			gDriverContext.flags = 0;
			gDriverContext.keyCode = 0;
			gDriverContext.errCode = 0;
			strncpy(event_string, "MHLEVENT=disconnected", MAX_EVENT_STRING_LEN);
			kobject_uevent_env(&gDriverContext.pDevice->kobj,
								KOBJ_CHANGE, envp);
			break;

		case MHL_TX_EVENT_RCP_RECEIVED:
			gDriverContext.flags &= ~MHL_STATE_FLAG_RCP_SENT;
			gDriverContext.flags |= MHL_STATE_FLAG_RCP_RECEIVED;
			gDriverContext.keyCode = eventParam;
			snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=received_RCP key code=0x%02x", eventParam);
			kobject_uevent_env(&gDriverContext.pDevice->kobj,
								KOBJ_CHANGE, envp);
#if 0
			//RG addition to send key to keyboard interface
			TX_DEBUG_PRINT(("SENDING KEY TO INPUT DEVICE: 0x%02x\n",eventParam));
			switch(eventParam)
			{
			case 0x01: //UP
			    input_report_key(dev_keyboard, KEY_UP, 1);
			    input_sync(dev_keyboard);	    			// generate event
			    input_report_key(dev_keyboard, KEY_UP, 0);
			    input_sync(dev_keyboard);	    			// generate event
				break;
			case 0x02: //DOWN
			    input_report_key(dev_keyboard, KEY_DOWN, 1);
			    input_sync(dev_keyboard);	    			// generate event
			    input_report_key(dev_keyboard, KEY_DOWN, 0);
			    input_sync(dev_keyboard);	    			// generate event
				break;
			case 0x03: //LEFT
				input_report_key(dev_keyboard, KEY_LEFT, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_LEFT, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;
			case 0x04: //RIGHT
				input_report_key(dev_keyboard, KEY_RIGHT, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_RIGHT, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;
			case 0x00: //SELECT
				//input_report_key(dev_keyboard, KEY_ENTER, 1); //RG replaced by KEY_REPLAY
				input_report_key(dev_keyboard, KEY_REPLY, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_REPLY, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;
			case 0x0D: //RETURN
				input_report_key(dev_keyboard, KEY_BACK, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_BACK, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;
			case 0x44: //PLAY
				//input_report_key(dev_keyboard, KEY_PLAYPAUSE, 1);
				input_report_key(dev_keyboard, KEY_PLAYCD, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_PLAYCD, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;
			case 0x45: //STOP
				//input_report_key(dev_keyboard, KEY_STOP, 1);
				input_report_key(dev_keyboard, KEY_STOPCD, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_STOPCD, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;
			case 0x20: //No 1 key -- RG mapping to Select for test
				input_report_key(dev_keyboard, KEY_SELECT, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_SELECT, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;

			case 0x21: //No 2 key -- RG mapping to OK for test
				input_report_key(dev_keyboard, KEY_OK, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_OK, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;

			case 0x22: //No 3 key -- RG mapping to KEY_REPLY for test
				input_report_key(dev_keyboard, KEY_REPLY, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, KEY_REPLY, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;

			case 0x23: //No 4 key -- RG mapping to BTN SELECT for test
				input_report_key(dev_keyboard, BTN_SELECT, 1);
				input_sync(dev_keyboard);	    			// generate event
				input_report_key(dev_keyboard, BTN_SELECT, 0);
				input_sync(dev_keyboard);	    			// generate event
				break;
			}

			//RG TODO if processing RCP events here need to send RCPK/E from here
#endif
			break;

		case MHL_TX_EVENT_RCPK_RECEIVED:
			if((gDriverContext.flags & MHL_STATE_FLAG_RCP_SENT)
				&& (gDriverContext.keyCode == eventParam)) {

				gDriverContext.flags |= MHL_STATE_FLAG_RCP_ACK;

				SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
						"Generating RCPK received event, keycode: 0x%02x\n",
						eventParam);
				snprintf(event_string, MAX_EVENT_STRING_LEN,
						"MHLEVENT=received_RCPK key code=0x%02x", eventParam);
				kobject_uevent_env(&gDriverContext.pDevice->kobj,
									KOBJ_CHANGE, envp);
			} else {
				SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
						"Ignoring unexpected RCPK received event, keycode: 0x%02x\n",
						eventParam);
			}
			break;

		case MHL_TX_EVENT_RCPE_RECEIVED:
			if(gDriverContext.flags & MHL_STATE_FLAG_RCP_SENT) {

				gDriverContext.errCode = eventParam;
				gDriverContext.flags |= MHL_STATE_FLAG_RCP_NAK;

				SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
						"Generating RCPE received event, error code: 0x%02x\n",
						eventParam);
				snprintf(event_string, MAX_EVENT_STRING_LEN,
						"MHLEVENT=received_RCPE error code=0x%02x", eventParam);
				kobject_uevent_env(&gDriverContext.pDevice->kobj,
									KOBJ_CHANGE, envp);
			} else {
				SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
						"Ignoring unexpected RCPE received event, error code: 0x%02x\n",
						eventParam);
			}
			break;

		case MHL_TX_EVENT_DCAP_CHG:
			snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=DEVCAP change");
			kobject_uevent_env(&gDriverContext.pDevice->kobj,
								KOBJ_CHANGE, envp);

			break;

		case MHL_TX_EVENT_DSCR_CHG:	// Scratch Pad registers have changed
			snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=SCRATCHPAD change");
			kobject_uevent_env(&gDriverContext.pDevice->kobj,
								KOBJ_CHANGE, envp);
			break;

		case MHL_TX_EVENT_POW_BIT_CHG:	// Peer's power capability has changed
			if(eventParam) {
				// Since downstream device is supplying VBUS power we should
				// turn off our VBUS power here.  If the platform application
				// can control VBUS power it should turn off it's VBUS power
				// now and return status of MHL_TX_EVENT_STATUS_HANDLED.  If
				// platform cannot control VBUS power it should return
				// MHL_TX_EVENT_STATUS_PASSTHROUGH to allow the MHL layer to
				// try to turn it off.

				// In this sample driver all that is done is to report an
				// event describing the requested state of VBUS power and
				// return MHL_TX_EVENT_STATUS_PASSTHROUGH to allow lower driver
				// layers to control VBUS power if possible.
				snprintf(event_string, MAX_EVENT_STRING_LEN,
						"MHLEVENT=MHL VBUS power OFF");

			} else {
				snprintf(event_string, MAX_EVENT_STRING_LEN,
						"MHLEVENT=MHL VBUS power ON");
			}

			kobject_uevent_env(&gDriverContext.pDevice->kobj,
								KOBJ_CHANGE, envp);
			break;

		case MHL_TX_EVENT_RGND_MHL:
			// RGND measurement has determine that the peer is an MHL device.
			// If platform application can determine that the attached device
			// is not supplying VBUS power it should turn on it's VBUS power
			// here and return MHL_TX_EVENT_STATUS_HANDLED to indicate to
			// indicate to the caller that it handled the notification.

			// In this sample driver all that is done is to report the event
			// and return MHL_TX_EVENT_STATUS_PASSTHROUGH to allow lower driver
			// layers to control VBUS power if possible.
			snprintf(event_string, MAX_EVENT_STRING_LEN,
					"MHLEVENT=MHL device detected");
			kobject_uevent_env(&gDriverContext.pDevice->kobj,
								KOBJ_CHANGE, envp);
			rtnStatus = MHL_TX_EVENT_STATUS_HANDLED;
			break;

		default:
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"AppNotifyEvent called with unrecognized event code!\n");
	}
	return rtnStatus;
}



/*****************************************************************************/
/**
 * @brief Handler for MHL transmitter reset requests.
 *
 * This function is called by the MHL layer to request that the MHL transmitter
 * chip be reset.  Since the MHL layer is platform agnostic and therefore doesn't
 * know how to control the transmitter's reset pin each platform application is
 * required to implement this function to perform the requested reset operation.
 *
 * @param[in]	hwResetPeriod	Time in ms. that the reset pin is to be asserted.
 * @param[in]	hwResetDelay	Time in ms. to wait after reset pin is released.
 *
 *****************************************************************************/
void AppResetMhlTx(uint16_t hwResetPeriod,uint16_t hwResetDelay)
{

	// Reset the TX chip,
	HalGpioSetTxResetPin(LOW);
	HalTimerWait(hwResetPeriod);
	HalGpioSetTxResetPin(HIGH);

	HalTimerWait(hwResetDelay);
}


#if defined(CONFIG_MACH_TAT)
#define SHARED_RESOURCE_BASE	0xFA010000
#define CTL_REG					(SHARED_RESOURCE_BASE + 0x2A80)
#define STATE_REG				(SHARED_RESOURCE_BASE + 0x2FD0)

void enable_shared_resource(void)
{
	void* busy;
	u32 reg;
	int count;

	fj_SiiDebugPrint("%s\n", __func__);
	busy = smem_alloc_vendor0(SMEM_OEM_V0_008);
	if (!busy) {
		printk(KERN_ERR "%s:busy address NULL\n", __func__);
		return;
	}

	writel_relaxed(0x1, busy);
	mb();

	mdelay(50);

	reg = readl_relaxed(CTL_REG);
	reg |= BIT(4);
	writel_relaxed(reg, CTL_REG);
	mb();

	for (count = 200; count > 0; count--) {
		if ((readl_relaxed(STATE_REG) & BIT(15)) == 0x0)
			break;
		udelay(1);
	} 
	if (count == 0)
		printk(KERN_ERR "%s:shared resouce disable\n", __func__);
}

void disable_shared_resource(void)
{
	void* busy;
	void* condition;
	u32 reg;
	unsigned long state;
	unsigned long flags;

	fj_SiiDebugPrint("%s\n", __func__);
	busy = smem_alloc_vendor0(SMEM_OEM_V0_008);
	if (!busy) {
		printk(KERN_ERR "%s:busy address NULL\n", __func__);
		return;
	}

	condition = smem_alloc_vendor0(SMEM_OEM_V0_009);
	if (!condition) {
		printk(KERN_ERR "%s:condition address MULL\n", __func__);
		return;
	}

	local_irq_save(flags);
	state = readl_relaxed(condition);
	if (state == 0x0) {
		reg = readl_relaxed(CTL_REG);
		reg &= ~(BIT(4));
		writel_relaxed(reg, CTL_REG);
		mb();
	}
	local_irq_restore(flags);

	writel_relaxed(0x0, busy);
	mb();
}
#else
void enable_shared_resource(void)
{
}

void disable_shared_resource(void)
{
}
#endif /* defined(CONFIG_MACH_TAT) */

void detectreq_workqueue(struct work_struct *work)
{
	int32_t ret;
	unsigned long flags;

	fj_SiiDebugPrint("%s\n", __func__);

	read_lock_irqsave(&gDriverContext.statelock, flags);
	if (MHL_MAINSTATE_DETECT != gDriverContext.mainstate) {
		printk(KERN_ERR "detect device error(wq). state:%d\n",
		                                              gDriverContext.mainstate);
		read_unlock_irqrestore(&gDriverContext.statelock, flags);
		return;
	}
	read_unlock_irqrestore(&gDriverContext.statelock, flags);

	enable_shared_resource();
	gDriverContext.rgnddetect_timer.expires = jiffies + RGNDDETECT_TIMEOUT;
	add_timer(&gDriverContext.rgnddetect_timer);

	ret = StartMhlTxDevice();
	if (ret)
		printk(KERN_ERR "StartMhlTxDevice Err:%d\n", ret);
}


void detectnotify_workqueue(struct work_struct *work)
{
	struct detect_callback *fn;
	struct notify_data *data;
	unsigned long flags;

	fj_SiiDebugPrint("%s\n", __func__);

	read_lock_irqsave(&gDriverContext.statelock, flags);
	if (MHL_MAINSTATE_DETECT != gDriverContext.mainstate) {
		printk(KERN_ERR "detect notify error(wq). state:%d\n",
		                                              gDriverContext.mainstate);
		read_unlock_irqrestore(&gDriverContext.statelock, flags);
		return;
	}
	read_unlock_irqrestore(&gDriverContext.statelock, flags);

	del_timer_sync(&gDriverContext.rgnddetect_timer);
	del_timer_sync(&gDriverContext.connect_timer);

	fn = &gDriverContext.detectfunc;
	if (NULL == fn) {
		printk(KERN_ERR "Callback function is not registered.\n");
		return;
	}

	data = container_of(work, struct notify_data, workq);
	if (CONFIRM_MHL == data->event) {
		fj_SiiDebugPrint("CONFIRM_MHL callbak(%d):IN\n", data->event);
		if (fn->func(data->event, fn->arg))
			printk(KERN_ERR "Detect Callback Err\n");
		fj_SiiDebugPrint("callbak:OUT\n");
#if defined(CONFIG_ARCH_APQ8064)
		fj_SiiDebugPrint("Call fj_chg_mhl_vbus_draw:FJ_CHG_MHL_CURRENT\n");
//		fj_chg_mhl_vbus_draw(FJ_CHG_MHL_CURRENT);
		fj_chg_mhl_vbus_draw(FJ_CHG_ENABLE);
#endif /* defined(CONFIG_ARCH_APQ8064) */
		write_lock_irqsave(&gDriverContext.statelock, flags);
		gDriverContext.mainstate = MHL_MAINSTATE_CONNECT;
		write_unlock_irqrestore(&gDriverContext.statelock, flags);
	} else {
		disable_mhl_interrupt();
		StopMhlTxDevice();
		disable_shared_resource();
		wake_unlock(&gDriverContext.wakelock);
		fj_SiiDebugPrint("%s callbak(%d):IN\n", 
			   (data->event == CONFIRM_USB_HOST) ? "CONFIRM_USB_HOST" :
			   (data->event == DETECT_OUT_MHL) ? "DETECT_OUT_MHL" : "incorrect",
 			   data->event);
		if (fn->func(data->event, fn->arg))
			printk(KERN_ERR "Detect Callback Err\n");
		fj_SiiDebugPrint("callbak:OUT\n");
		write_lock_irqsave(&gDriverContext.statelock, flags);
		gDriverContext.mainstate = MHL_MAINSTATE_IDLE;
		gDriverContext.substate = MHL_SUBSTATE_INIT;
		gDriverContext.i2cerror = false;
		write_unlock_irqrestore(&gDriverContext.statelock, flags);
	}
}

void disconnectnotify_workqueue(struct work_struct *work)
{
	struct detect_callback *fn;
	struct notify_data *data;
	unsigned long flags;

	fj_SiiDebugPrint("%s\n", __func__);

	write_lock_irqsave(&gDriverContext.statelock, flags);
	if (MHL_MAINSTATE_CONNECT != gDriverContext.mainstate) {
		printk(KERN_ERR "disconnect notify error(wq). state:%d\n",
		                                              gDriverContext.mainstate);
		write_unlock_irqrestore(&gDriverContext.statelock, flags);
		return;
	}
	gDriverContext.mainstate = MHL_MAINSTATE_DISCONNECT;
	write_unlock_irqrestore(&gDriverContext.statelock, flags);

	del_timer_sync(&gDriverContext.rgnddetect_timer);
	del_timer_sync(&gDriverContext.connect_timer);

	disable_mhl_interrupt();
	mdelay(50);
	StopMhlTxDevice();
	disable_shared_resource();

#if defined(CONFIG_ARCH_APQ8064)
	fj_SiiDebugPrint("Call fj_chg_mhl_vbus_draw:FJ_CHG_OFF_CURRENT\n");
//	fj_chg_mhl_vbus_draw(FJ_CHG_OFF_CURRENT);
	fj_chg_mhl_vbus_draw(FJ_CHG_DISABLE);
#endif /* defined(CONFIG_ARCH_APQ8064) */

	wake_unlock(&gDriverContext.wakelock);

	write_lock_irqsave(&gDriverContext.statelock, flags);
	gDriverContext.mainstate = MHL_MAINSTATE_IDLE;
	gDriverContext.substate = MHL_SUBSTATE_INIT;
	gDriverContext.i2cerror = false;
	write_unlock_irqrestore(&gDriverContext.statelock, flags);

	data = container_of(work, struct notify_data, workq);
	fn = &gDriverContext.detectfunc;
	fj_SiiDebugPrint("%s callbak:IN(%d)\n",
			   (data->event == DETECT_OUT_MHL) ? "DETECT_OUT_MHL" : "incorrect",
			   data->event);
	if (fn->func(data->event, fn->arg))
		printk(KERN_ERR "Disconnect Callback Err:%s\n", __func__);
	fj_SiiDebugPrint("callbak:OUT\n");
}

void errornotify_workqueue(struct work_struct *work)
{
	struct detect_callback *fn;
	struct notify_data *data;
	unsigned long flags;
#if defined(CONFIG_ARCH_APQ8064)
	int laststate;
#endif /* defined(CONFIG_ARCH_APQ8064) */

	fj_SiiDebugPrint("%s\n", __func__);

	write_lock_irqsave(&gDriverContext.statelock, flags);
#if defined(CONFIG_ARCH_APQ8064)
	laststate = gDriverContext.mainstate;
#endif /* defined(CONFIG_ARCH_APQ8064) */
	if (!((MHL_MAINSTATE_DETECT == gDriverContext.mainstate) ||
		                 (MHL_MAINSTATE_CONNECT == gDriverContext.mainstate))) {
		printk(KERN_ERR "error notify:state not MHL_MAINSTATE_DETECT\n");
		write_unlock_irqrestore(&gDriverContext.statelock, flags);
		return;
	}
	gDriverContext.mainstate = MHL_MAINSTATE_DISCONNECT;
	write_unlock_irqrestore(&gDriverContext.statelock, flags);

	del_timer_sync(&gDriverContext.rgnddetect_timer);
	del_timer_sync(&gDriverContext.connect_timer);

	disable_mhl_interrupt();
	StopMhlTxDevice();
	disable_shared_resource();

#if defined(CONFIG_ARCH_APQ8064)
	if (MHL_MAINSTATE_CONNECT == laststate) {
		fj_SiiDebugPrint("Call fj_chg_mhl_vbus_draw:FJ_CHG_OFF_CURRENT\n");
//		fj_chg_mhl_vbus_draw(FJ_CHG_OFF_CURRENT);
		fj_chg_mhl_vbus_draw(FJ_CHG_DISABLE);
	}
#endif /* defined(CONFIG_ARCH_APQ8064) */

	wake_unlock(&gDriverContext.wakelock);

	write_lock_irqsave(&gDriverContext.statelock, flags);
	gDriverContext.mainstate = MHL_MAINSTATE_IDLE;
	gDriverContext.substate = MHL_SUBSTATE_INIT;
	gDriverContext.i2cerror = false;
	write_unlock_irqrestore(&gDriverContext.statelock, flags);

	data = container_of(work, struct notify_data, workq);
	fn = &gDriverContext.detectfunc;
	fj_SiiDebugPrint("%s callbak(%d):IN\n",
							(data->event == DETECT_OUT_MHL) ? "DETECT_OUT_MHL" :
							(data->event == ERROR_MHL) ? "ERROR_MHL" : "incorrect",
							data->event);
	if (fn->func(data->event, fn->arg))
		printk(KERN_ERR "Detect error Callback Err:%s\n", __func__);
	fj_SiiDebugPrint("callbak:OUT\n");
}

void detect_notify(int event)
{
	if (event < MHL_EVENT_COUNT) {
		fj_SiiDebugPrint("%s Event:%s(%d)\n", __func__, mhl_event[event], event);
	} else {
		fj_SiiDebugPrint("%s Event:(%d) is not supported\n", __func__, event);
	}

	switch (event) {
	case MHL_CONFIRM_USB_HOST:
		gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECT].event = CONFIRM_USB_HOST;
		queue_work(gDriverContext.fjworkqueue,
			            &gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECT].workq);
		break;
	case MHL_CONFIRM_MHL:
		gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECT].event = CONFIRM_MHL;
		queue_work(gDriverContext.fjworkqueue,
			            &gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECT].workq);
		break;
	case MHL_RGND_DETECT_OPEN:
		gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECT].event = DETECT_OUT_MHL;
		queue_work(gDriverContext.fjworkqueue,
			            &gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECT].workq);
		break;
	case MHL_DETECT_OUT:
		gDriverContext.notifyworkq[MHL_NOTIFYWQ_DISCONNECT].event = DETECT_OUT_MHL;
		queue_work(gDriverContext.fjworkqueue,
			        &gDriverContext.notifyworkq[MHL_NOTIFYWQ_DISCONNECT].workq);
		break;
	case MHL_RGNDDETECT_TIMEOUT:
		gDriverContext.notifyworkq[MHL_NOTIFYWQ_RGNDDETECTTIMER].event = DETECT_OUT_MHL;
		queue_work(gDriverContext.fjworkqueue,
			   &gDriverContext.notifyworkq[MHL_NOTIFYWQ_RGNDDETECTTIMER].workq);
		break;
	case MHL_CONNECT_TIMEOUT:
		gDriverContext.notifyworkq[MHL_NOTIFYWQ_CONNECTTIMER].event = ERROR_MHL;
		queue_work(gDriverContext.fjworkqueue,
			      &gDriverContext.notifyworkq[MHL_NOTIFYWQ_CONNECTTIMER].workq);
		break;
	case MHL_ERROR:
		disable_mhl_interrupt();
		gDriverContext.i2cerror = true;
		gDriverContext.notifyworkq[MHL_NOTIFYWQ_ERROR].event = DETECT_OUT_MHL;
		queue_work(gDriverContext.fjworkqueue,
			             &gDriverContext.notifyworkq[MHL_NOTIFYWQ_ERROR].workq);
		break;
	case MHL_RGND_DETECT:
		gDriverContext.substate = MHL_SUBSTATE_RGND_DETECT;
		del_timer_sync(&gDriverContext.rgnddetect_timer);
		break;
	}
}

static void error_timeout(unsigned long arg)
{
	fj_SiiDebugPrint("%s\n", __func__);
	detect_notify(arg);
}

bool iserroroccurred(void)
{
	return gDriverContext.i2cerror;
}


#if defined(CONFIG_ARCH_APQ8064)
void hdmi_gpio_config(int on)
{
	fj_SiiDebugPrint("%s:%d\n", __func__, on);
	if (gDriverContext.pd) {
		gDriverContext.pd->mhl_hdmi_gpio(on);
	}
}

void active_i2c_gpio(int on)
{
	fj_SiiDebugPrint("%s:%d\n", __func__, on);
	if (gDriverContext.pd) {
		gDriverContext.pd->active_i2c_gpio(on);
	}
}


uint8_t get_model_type(void)
{
	uint8_t model = (uint8_t)((system_rev & 0x70) >> 4);
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "%s:model type 0x%02x\n", __func__, model);
	return model;
}
#endif /* deined(CONFIG_ARCH_APQ8064) */

int fj_mhl_detect_device(int (*callback)(int event, void *arg), void *arg)
{
	unsigned long flags;

	fj_SiiDebugPrint("%s\n", __func__);

	write_lock_irqsave(&gDriverContext.statelock, flags);
	if ((MHL_MAINSTATE_IDLE == gDriverContext.mainstate) && (NULL != callback)) {
		if (gDriverContext.suspend) {
			printk(KERN_ERR "MHL driver is suspended.\n");
			write_unlock_irqrestore(&gDriverContext.statelock, flags);
			return ERROR_MHL;
		}
		gDriverContext.mainstate = MHL_MAINSTATE_DETECT;
	} else if (MHL_MAINSTATE_CONNECT == gDriverContext.mainstate) {
		printk(KERN_ERR "detect request(connected). state:%d\n",
			                                          gDriverContext.mainstate);
		detect_notify(MHL_CONFIRM_MHL);
		write_unlock_irqrestore(&gDriverContext.statelock, flags);
		return 0;
	} else {
		printk(KERN_ERR "detect device error. state:%d\n", gDriverContext.mainstate);
		write_unlock_irqrestore(&gDriverContext.statelock, flags);
		return ERROR_MHL;
	}
	write_unlock_irqrestore(&gDriverContext.statelock, flags);

	wake_lock(&gDriverContext.wakelock);

	gDriverContext.detectfunc.func = callback;
	gDriverContext.detectfunc.arg  = arg;

	gDriverContext.connect_timer.expires = jiffies + CONNECT_TIMEOUT;
	add_timer(&gDriverContext.connect_timer);

	queue_work(gDriverContext.fjworkqueue,
		             &gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECTREQ].workq);

	return 0;
}
EXPORT_SYMBOL(fj_mhl_detect_device);


static int fj_mhl_driver_probe(struct platform_device *pdev)
{
	fj_SiiDebugPrint("%s\n", __func__);
#if defined(CONFIG_ARCH_APQ8064)
	gDriverContext.pd = pdev->dev.platform_data;
#endif /* deined(CONFIG_ARCH_APQ8064) */
	return 0;
};

static int fj_mhl_driver_remove(struct platform_device *pdev)
{
	fj_SiiDebugPrint("%s\n", __func__);
#if defined(CONFIG_ARCH_APQ8064)
	gDriverContext.pd = NULL;
#endif /* deined(CONFIG_ARCH_APQ8064) */
	return 0;
}

static int fj_mhl_suspend(struct device* dev)
{
	unsigned long flags;
	int ret = 0;

	fj_SiiDebugPrint("%s\n", __func__);

	write_lock_irqsave(&gDriverContext.statelock, flags);
	gDriverContext.suspend = true;
	if (MHL_MAINSTATE_IDLE == gDriverContext.mainstate) {
		ret = 0;
	} else {
//		fj_SiiDebugPrint("MHL is connecting. The transition to the suspend is canceled.%s\n");
		fj_SiiDebugPrint("MHL is connecting. The transition to the suspend is canceled.\n");
		ret = -EBUSY;
	}
	write_unlock_irqrestore(&gDriverContext.statelock, flags);

	return ret;
}

static int fj_mhl_resume(struct device* dev)
{
	unsigned long flags;

	fj_SiiDebugPrint("%s\n", __func__);

	write_lock_irqsave(&gDriverContext.statelock, flags);
	gDriverContext.suspend = false;
	write_unlock_irqrestore(&gDriverContext.statelock, flags);

	return 0;
}

static const struct dev_pm_ops fj_mhl_dev_pm_ops = {
	.suspend = fj_mhl_suspend,
	.resume = fj_mhl_resume,
};

static struct platform_driver fj_mhl_driver = {
	.probe = fj_mhl_driver_probe,
	.remove = fj_mhl_driver_remove,
	.driver = {
		.name = "fj_mhl",
		.owner = THIS_MODULE,
		.pm = &fj_mhl_dev_pm_ops,
	},
};


/**
 *  File operations supported by the MHL driver
 */
static const struct file_operations siiMhlFops = {
    .owner			= THIS_MODULE,
    .open			= SiiMhlOpen,
    .release		= SiiMhlRelease,
    .unlocked_ioctl	= SiiMhlIoctl
};


/*
 * Sysfs attribute files supported by this driver.
 */
struct device_attribute driver_attribs[] = {
		__ATTR(connection_state, 0444, ShowConnectionState, NULL),
		__ATTR(rcp_keycode, 0666, ShowRcp, SendRcp),
		__ATTR(rcp_ack, 0666, ShowRcpAck, SendRcpAck),
		__ATTR(devcap, 0666, ReadDevCap, SelectDevCap),
		__ATTR(loglevel, 0666, ReadLogLevel, SelectLogLevel),
		__ATTR_NULL
};



static int __init SiiMhlInit(void)
{
//    int32_t	ret, error;
    int32_t	ret;
    dev_t	devno;
#if defined(CONFIG_MACH_TAT)
	void* busy;
#endif /* defined(CONFIG_ARCH_APQ8064) */

	if (makercmd_mode) {
		fj_mhl_loglevel = MHL_LOG_FJ | MHL_LOG_SII;
	} else {
		fj_mhl_loglevel = MHL_LOG_NONE;
#if defined(FJ_DEBUG)
		fj_mhl_loglevel |= MHL_LOG_FJ;
#endif
#if defined(DEBUG)
		fj_mhl_loglevel |= MHL_LOG_SII;
#endif
	}

	fj_SiiDebugPrint("%s\n", __func__);
	gDriverContext.mainstate = MHL_MAINSTATE_INIT;

	pr_info("%s driver starting!\n", MHL_DRIVER_NAME);
	pr_info("Version: %s\n", buildVersion);
	pr_info("%s", buildTime);

    /* register chrdev */
    pr_info("register_chrdev %s\n", MHL_DRIVER_NAME);

    /* If a major device number has already been selected use it,
     * otherwise dynamically allocate one.
     */
    if (devMajor) {
        devno = MKDEV(devMajor, 0);
        ret = register_chrdev_region(devno, MHL_DRIVER_MINOR_MAX,
                MHL_DRIVER_NAME);
    } else {
        ret = alloc_chrdev_region(&devno,
                        0, MHL_DRIVER_MINOR_MAX,
                        MHL_DRIVER_NAME);
        devMajor = MAJOR(devno);
    }
    if (ret) {
    	pr_info("register_chrdev %d, %s failed, error code: %d\n",
    					devMajor, MHL_DRIVER_NAME, ret);
        return ret;
    }
/*    return 0; */

    cdev_init(&siiMhlCdev, &siiMhlFops);
    siiMhlCdev.owner = THIS_MODULE;
    ret = cdev_add(&siiMhlCdev, devno, MHL_DRIVER_MINOR_MAX);
    if (ret) {
    	pr_info("cdev_add %s failed %d\n", MHL_DRIVER_NAME, ret);
        goto free_chrdev;
    }

    siiMhlClass = class_create(THIS_MODULE, "mhl");
    if (IS_ERR(siiMhlClass)) {
    	pr_info("class_create failed %d\n", ret);
        ret = PTR_ERR(siiMhlClass);
        goto free_cdev;
    }

    siiMhlClass->dev_attrs = driver_attribs;

    gDriverContext.pDevice  = device_create(siiMhlClass, NULL,
    									 MKDEV(devMajor, 0),  NULL,
    									 "%s", MHL_DEVICE_NAME);
    if (IS_ERR(gDriverContext.pDevice)) {
    	pr_info("class_device_create failed %s %d\n", MHL_DEVICE_NAME, ret);
        ret = PTR_ERR(gDriverContext.pDevice);
        goto free_class;
    }

//	error = init_keyboard();
//	if (error) pr_info("ERROR INSTALLING KEYBOARD");
//	if (error) {
//		pr_info("ERROR INSTALLING KEYBOARD\n");
//		ret = error;
//		goto free_device;
//	}

#if defined(CONFIG_MACH_TAT)
	busy = smem_alloc_vendor0(SMEM_OEM_V0_008);
	if (!busy) {
		printk(KERN_ERR "%s:busy address NULL\n", __func__);
		goto free_device;
	}
	writel_relaxed(0x0, busy);
	mb();
#endif /* defined(CONFIG_MACH_TAT) */

	rwlock_init(&gDriverContext.statelock);
	init_timer(&gDriverContext.rgnddetect_timer);
	init_timer(&gDriverContext.connect_timer);

	gDriverContext.rgnddetect_timer.function = error_timeout;
	gDriverContext.rgnddetect_timer.data = (unsigned long)MHL_RGNDDETECT_TIMEOUT;
	gDriverContext.rgnddetect_timer.expires = jiffies + RGNDDETECT_TIMEOUT;

	gDriverContext.connect_timer.function = error_timeout;
	gDriverContext.connect_timer.data = (unsigned long)MHL_CONNECT_TIMEOUT;
	gDriverContext.connect_timer.expires = jiffies + CONNECT_TIMEOUT;

	gDriverContext.fjworkqueue = create_singlethread_workqueue("fj_mhl_work");

	INIT_WORK(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECTREQ].workq, detectreq_workqueue);
	INIT_WORK(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECT].workq, detectnotify_workqueue);
	INIT_WORK(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_DISCONNECT].workq, disconnectnotify_workqueue);
	INIT_WORK(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_RGNDDETECTTIMER].workq, errornotify_workqueue);
	INIT_WORK(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_CONNECTTIMER].workq, errornotify_workqueue);
	INIT_WORK(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_ERROR].workq, errornotify_workqueue);
	wake_lock_init(&gDriverContext.wakelock, WAKE_LOCK_SUSPEND, "fj_mhl_wakelock");
	gDriverContext.mainstate = MHL_MAINSTATE_IDLE;
	gDriverContext.substate  = MHL_SUBSTATE_INIT;
	gDriverContext.i2cerror = false;
#if 0
    ret = StartMhlTxDevice();
    if(ret == 0) {
    	return 0;

    } else {
    	// Transmitter startup failed so fail the driver load.
    	device_destroy(siiMhlClass, MKDEV(devMajor, 0));
    }
#endif

	gDriverContext.suspend = false;
	platform_driver_register(&fj_mhl_driver);
	ovp_device_initialized(INITIALIZE_MHL);
    return 0;


#if defined(CONFIG_MACH_TAT)
free_device:
#endif /* defined(CONFIG_MACH_TAT) */

	device_destroy(siiMhlClass, MKDEV(devMajor, 0));
free_class:
	class_destroy(siiMhlClass);

free_cdev:
	cdev_del(&siiMhlCdev);

free_chrdev:
	unregister_chrdev_region(MKDEV(devMajor, 0), MHL_DRIVER_MINOR_MAX);

	return ret;
}



static void __exit SiiMhlExit(void)
{
	fj_SiiDebugPrint("%s\n", __func__);

	pr_info("%s driver exiting!\n", MHL_DRIVER_NAME);

	platform_driver_unregister(&fj_mhl_driver);
	wake_lock_destroy(&gDriverContext.wakelock);

	del_timer_sync(&gDriverContext.rgnddetect_timer);
	del_timer_sync(&gDriverContext.connect_timer);

	cancel_work_sync(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECTREQ].workq);
	cancel_work_sync(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_DETECT].workq);
	cancel_work_sync(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_DISCONNECT].workq);
	cancel_work_sync(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_RGNDDETECTTIMER].workq);
	cancel_work_sync(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_CONNECTTIMER].workq);
	cancel_work_sync(&gDriverContext.notifyworkq[MHL_NOTIFYWQ_ERROR].workq);

	destroy_workqueue(gDriverContext.fjworkqueue);
	gDriverContext.fjworkqueue = NULL;
	StopMhlTxDevice();
	disable_shared_resource();

	device_destroy(siiMhlClass, MKDEV(devMajor, 0));
    class_destroy(siiMhlClass);
    unregister_chrdev_region(MKDEV(devMajor, 0), MHL_DRIVER_MINOR_MAX);
//    input_unregister_device(dev_keyboard);

}

module_init(SiiMhlInit);
module_exit(SiiMhlExit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Silicon Image <http://www.siliconimage.com>");
MODULE_DESCRIPTION(MHL_DRIVER_DESC);


