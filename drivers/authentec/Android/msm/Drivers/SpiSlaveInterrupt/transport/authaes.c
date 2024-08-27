 /* /=======================================================================\
  * |                  AuthenTec Embedded (AE) Software                     |
  * |                                                                       |
  * |        THIS CODE IS PROVIDED AS PART OF AN AUTHENTEC CORPORATION      |
  * |                    EMBEDDED DEVELOPMENT KIT (EDK).                    |
  * |                                                                       |
  * |    Copyright (C) 2006-2011, AuthenTec, Inc. - All Rights Reserved.    |
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

   File:            authtraes.c

   Description:     This file contains basic driver entry points.

   Author:          James Deng

   ************************************************************************* */

/** include files **/
#include "hfiles.h"

/** local definitions **/
static struct cdev c_dev;
static dev_t dev;

/* name of the driver, for operating system registration purposes */
#define DRIVER_NAME "/dev/aeswipe"

#define MAX_READ_BUF 48*1024

static ssize_t module_entry_read(struct file* psFile, char* pcBuffer, size_t uCount, loff_t* puOffsetPosition);
static ssize_t module_entry_write(struct file* psFile, const char* pcBuffer, size_t uCount, loff_t* puOffsetPosition);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
static long  module_entry_ioctl(struct file* psFile, unsigned int uCommand, unsigned long uArgument);
#else
static int  module_entry_ioctl(struct inode* psInode, struct file* psFile, unsigned int uCommand, unsigned long uArgument);
#endif
static int  module_entry_open(struct inode* psInode, struct file* psFile);
static int  module_entry_close(struct inode* psInode, struct file* psFile);

/*
** FILE OPERATIONS FOR THIS DRIVER
**
** These are the driver 'entry points'
*/
static struct file_operations sSensorFileOperations =
{
   owner:   THIS_MODULE,
   read:    module_entry_read,
   write:   module_entry_write,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))  
	 unlocked_ioctl:	module_entry_ioctl, 	
#else
	 ioctl:   module_entry_ioctl,
#endif
   open:    module_entry_open,
   release: module_entry_close,
};
/* FUJITSU:2011-12-12 FingerPrintSensor mod start */
static int __devinit authfp_probe(struct platform_device *pdev);
/* FUJITSU:2011-12-12 FingerPrintSensor mod end */
static int __exit authfp_remove(struct platform_device *pdev);
static int authfp_suspend(struct platform_device *pdev, pm_message_t state);
static int authfp_resume(struct platform_device *pdev);

static struct platform_driver aeswipe_driver = {
	.probe = authfp_probe,
	.remove = __exit_p(authfp_remove),
    .suspend = authfp_suspend,
    .resume = authfp_resume,
    .driver	= {
        .name = "aeswipe",
        .owner = THIS_MODULE,
    },
};

static struct platform_device *aeswipe_device;
static struct class *s_DevClass;

static struct semaphore ioctl_semaphore;
static struct semaphore open_semaphore;

#if (TRANSPROT_INTERFACE_USED == TR_IFACE_SPI_S)
static struct spi_board_info fp_spi_board_info = 
{
	.modalias            = "fp_spi",
	.bus_num             = TRANSPORT_IO_PORT,
	.chip_select         = TRANSPORT_IO_SPI_CS,
	.max_speed_hz        = TRANSPORT_IO_DATA_RATE,
	.controller_data     = NULL,
	.irq                 = -1,
	.platform_data       = NULL,
};

struct spi_device *spi_fp = NULL;

static int __devinit fp_probe(struct spi_device *spi);
static int __devexit fp_remove(struct spi_device *spi);
static struct spi_driver fp_driver = 
{
	.driver = {
		.name       = "fp_spi",
		.bus        = &spi_bus_type,
		.owner      = THIS_MODULE,
	},

    .probe = fp_probe,
    .remove = __devexit_p(fp_remove),

};

static int __devinit fp_probe(struct spi_device *spi)
{
    int ret = 0;
    
    DBPRINT((L("fp_probe: is called\n")));

    //Setup the SPI slave 
    spi->mode = SPI_MODE_1;
    spi->bits_per_word = 8;
    ret =  spi_setup( spi );

    spi_fp = spi;

    return 0;
}

static int __devexit fp_remove(struct spi_device *spi)
{
    return 0;
}


/*++
 ===============================================================================
 Function spi_fp_attach

 The spi_fp_attach() function will be called by the driver to connect the spi master.

 Parameters

 Return Value        Return 0 on success and a negative error number if failure

 ===============================================================================
 --*/
static int spi_fp_attach(void)
{
	struct spi *master = NULL;
	int ret = 0;


	master = (struct spi *)spi_busnum_to_master(fp_spi_board_info.bus_num);
	if (NULL == master)
	{
		DBPRINT((L("spi.c - spi_fp_attach() error. NULL == master\n")));
		return -1;
	}
    
	spi_fp = (struct spi_device *)spi_new_device((struct spi_master *)master, &fp_spi_board_info);
	if (NULL == spi_fp)
	{
		DBPRINT((L("spi.c - spi_fp_attach() error. NULL == spi_fp\n")));
		return -1;
	}

	ret = spi_register_driver( &fp_driver);
	if (ret)
	{
		printk("fp_spi_connect -err:%d\n",ret);
		return ret;
	}
    
	return 0;	
}


/*++
 ===============================================================================
 Function fp_spi_deattach

 The fp_spi_deattach() function will be called by the driver to connect the spi master.

 Parameters

 Return Value        Return 0 on success and a negative error number if failure

 ===============================================================================
 --*/
static void fp_spi_deattach(void)
{
	spi_unregister_driver( &fp_driver);

	if (spi_fp)
	{
	    spi_unregister_device( spi_fp);
	}
}

#elif (TRANSPROT_INTERFACE_USED == TR_IFACE_I2C)

#define I2C_FP_NAME		"i2c_fp_driver"
struct i2c_client *i2c_fp_client;

static int fp_remove(struct i2c_adapter *adap)
{
    if (TRANSPORT_IO_PORT == i2c_adapter_id(adap))
    {
        i2c_unregister_device(i2c_fp_client);
    }    
    return 0;
}

static struct i2c_board_info i2c_fp_board_info = {
    I2C_BOARD_INFO(I2C_FP_NAME, 0x53),
};

static int fp_probe(struct i2c_adapter *adap)
{
    struct i2c_adapter *apater_used = NULL;

     //printk("fp_i2c_probe: adpter id %d, class 0x%08x, name=%s\n", adap->id, adap->class, adap->name);

    if (TRANSPORT_IO_PORT == i2c_adapter_id(adap))
    {
        i2c_fp_client = i2c_new_device(adap, &i2c_fp_board_info);
    } 

    return 0;
}

static struct i2c_driver fp_driver = {
	.attach_adapter = fp_probe,
	.detach_adapter  = fp_remove,
	.driver 	= {
		//.owner	= THIS_MODULE,
		.name = I2C_FP_NAME,
	},
};
static int i2c_fp_attach()
{
	int ret = 0;
	ret = i2c_add_driver(&fp_driver);
	return ret;
}

static void i2c_fp_deattach()
{
    i2c_del_driver(&fp_driver);
    return;
}

#endif

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/
lptsAUTHCONTEXT     g_lptsAuthContext = NULL;
static uint8* g_pucMemRead = NULL;

/** private functions **/

/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-S */
uint8 g_log_drv_hw = 0;            /* drv <=> HW log  : enable(== 1)/disable(!= 1) */
uint8 g_log_drv_hal = 0;           /* drv <=> HAL log : enable(== 1)/disable(!= 1) */
#define		FPDBGLOG(format, arg...) \
		if (g_log_drv_hw || g_log_drv_hal) printk("[FP_DRV] %s" format, KERN_DEBUG, ##arg)

#define		APNV_FINGERPRINT_LOGSW_DATA_I		0xA070
#define		APNV_FINGERPRINT_LOGSW_IOCTL_I		0xA071
#define		APNV_FINGERPRINT_LOGSW_DATA_MASK	0x1
#define		APNV_FINGERPRINT_LOGSW_IOCTL_MASK	0x1
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-E */

/*++
===============================================================================

Function RegisterNavInputDevice

This function is used to register an input device to linux system. While
navigation operation the driver reports the events/data via the input device.

Parameters
lptsAuthContext (input)         Pointer to the device driver context, can
                                be used to access the hardware and talk to
                                the sensor


Return Value                    TRUE indicates a success.
                                     FAILSE indicates a failure.
===============================================================================
--*/
BOOL
RegisterNavInputDevice(lptsAUTHCONTEXT     lptsAuthContext)
{
    int32 i = 0;
    lptsAUTHAES    lptsAuthaes = NULL;

    struct input_dev *psNavDevice = NULL;

    #define NUMKEYS 8
    uint16 sKeycodes[NUMKEYS] = {
                    KEY_LEFT,
                    KEY_RIGHT,
                    KEY_UP,
                    KEY_DOWN,
                    BTN_LEFT,
                    BTN_RIGHT,
                    KEY_ENTER,
                    BTN_MOUSE
                    };

    if (NULL == lptsAuthContext)
    {
        return FALSE;
    }

    lptsAuthaes = PAES(lptsAuthContext);
    if (NULL == lptsAuthaes)
    {
        return FALSE;
    }

    // add our key and mouse events to the Linux input system
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
    psNavDevice = kmalloc(sizeof(struct input_dev), GFP_KERNEL);
#else
    psNavDevice = input_allocate_device();
#endif

    // init_input_dev(psNavDevice);
    psNavDevice->name = DRIVER_NAME;            /*name of the device*/
    psNavDevice->phys = DRIVER_NAME;             /*physical path to the device in the system hierachy*/
    psNavDevice->evbit[0] = BIT(EV_KEY) | BIT(EV_REL); /*types of events supported*/
    psNavDevice->relbit[0] = BIT(REL_X) | BIT(REL_Y);    /*relative axes for the device*/
    psNavDevice->keycode = sKeycodes;                        /*scancodes to Keycodes for this device*/
    psNavDevice->keycodesize = sizeof(sKeycodes[0]);     /*size of elements in keycode table*/
    psNavDevice->keycodemax = sizeof(sKeycodes) / sizeof(sKeycodes[0]);   /*size of keycode table*/

    for (i = 0; i < (sizeof(sKeycodes) / sizeof(sKeycodes[0])); i++)
    {
        set_bit(sKeycodes[i], psNavDevice->keybit);      /*bitmap of keys/buttons this device has*/
    }

    psNavDevice->id.bustype = BUS_HOST;
    psNavDevice->id.vendor = 0x0001;
    psNavDevice->id.product = 0x3344;
    psNavDevice->id.version = 0xae70;

    /*register the device*/
    if (0 != input_register_device(psNavDevice))
    {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
        kfree(psNavDevice);
#else
        input_free_device(psNavDevice);
#endif
        psNavDevice= NULL;

        lptsAuthaes->pvUser = NULL;
        return FALSE;
    }

    lptsAuthaes->pvUser = (void*)psNavDevice;

    return TRUE;

}

/** public functions **/


/*++
===============================================================================

Function AES_Init

The AES_Init() function initializes the transport driver.

Parameters
lpvInit (input)         Pointer to information that is used to initialize
                        the device.

Return Value            Returns a pointer (lpvContext) to the actual driver
                        context cast to a uint32 value. The pointer returned
                        is used in subsequent calls to the other driver
                        entry points.

===============================================================================
--*/

uint32
AES_Init (void *    lpvInit)
{
    lptsAUTHCONTEXT     lptsAuthContext = NULL;     /* Pointer to the driver context */
    tsAUTHBUFFER        sAuthBuffer;                /* Driver context memory buffer  */
    lptsAUTHIO             lptsAuthIO = NULL;         /* Pointer to the I/O context */
    uint32 uiIdx;
    int32 iErr;
    


    DBPRINT((L("AES_Init: Called.\n")));
    uiIdx = 0;
    iErr = 0;

    /* Define the size of the structure needed for the driver context... */
    MEMSET(&sAuthBuffer, 0, sizeof(sAuthBuffer));
    sAuthBuffer.uiStructSize   = sizeof(tsAUTHBUFFER);
    sAuthBuffer.uiBufferLength = sizeof(tsAUTHCONTEXT);
    sAuthBuffer.uiFlags        = AB_NO_PHYSICAL;

    /* Create a device driver context and zero out the structure... */
    if ( AuthBufferAllocate( &sAuthBuffer ) )
    {
        /* This is the pointer to the driver context... */
        lptsAuthContext = (lptsAUTHCONTEXT) sAuthBuffer.pvVirtualBuffer;
        MEMSET(lptsAuthContext, 0, sizeof(tsAUTHCONTEXT));
    }
    else
    {
        /* Couldn't allocate memory, return an error... */
        DBGERROR((L("AES_Init: Error: AuthBufferAllocate (Global Context) Failed.\n")));
        return 0;
    }

    /* Fill out the basic memory structure information... */
    lptsAuthContext->sAuthTRANSPORT.uiStructSize              = sizeof(tsAUTHTRANSPORT);
    lptsAuthContext->sAuthAES.uiStructSize              = sizeof(tsAUTHAES);
    lptsAuthContext->sAuthBufferTX.uiStructSize         = sizeof(tsAUTHBUFFER);
    lptsAuthContext->sAuthBufferRX.uiStructSize         = sizeof(tsAUTHBUFFER);
    lptsAuthContext->sAuthISR.uiStructSize              = sizeof(tsAUTHISR);
    lptsAuthContext->sAuthDmaTX.uiStructSize            = sizeof(tsAUTHDMA);
    lptsAuthContext->sAuthDmaRX.uiStructSize            = sizeof(tsAUTHDMA);
    lptsAuthContext->sAuthGPIO.uiStructSize             = sizeof(tsAUTHGPIO);
    lptsAuthContext->sAuthIO.uiStructSize               = sizeof(tsAUTHIO);
    lptsAuthContext->sAuthPLATFORM.uiStructSize         = sizeof(tsAUTHPLATFORM);

    g_lptsAuthContext = lptsAuthContext;
    
    lptsAuthIO = PIO(lptsAuthContext);

#if (TRANSPROT_INTERFACE_USED == TR_IFACE_SPI_S)
    /*Save spi device to use IO read/write*/
    lptsAuthIO->pvIoDevice = (void*) spi_fp;
#elif (TRANSPROT_INTERFACE_USED == TR_IFACE_I2C)
    /*Save i2c device to use IO read/write*/
    lptsAuthIO->pvIoDevice = (void*) i2c_fp_client;
#endif

    /*register an input device to os for navigation*/
    if ( !RegisterNavInputDevice(g_lptsAuthContext) )
    {
        DBGERROR((L("AES_Init: Error: RegisterNavInputDevice() Failed.\n")));
        AES_Deinit ((void *)g_lptsAuthContext);
        return 0;
    }

    /* Try to initialize the remainder of the driver... */
    if (TR_OK == AuthTransportInit(PTRANSPORT(g_lptsAuthContext)))
    {
        /* Return a pointer to this device driver's context... */
        return (uint32)g_lptsAuthContext;
    }

    /* Device could not be initialized, undo everything... */
    AES_Deinit ((void *)g_lptsAuthContext);
    return 0;
}



/*++
===============================================================================

Function AES_Deinit

The AES_Deinit() function de-initializes the transport driver.

Parameters
lpvContext (input)      Pointer to the driver context.

Return Value            Returns a TRUE to indicate that the driver has
                        been de-initialized, FALSE otherwise.

===============================================================================
--*/

BOOL
AES_Deinit (void *  lpvContext)
{
    lptsAUTHCONTEXT     lptsAuthContext = NULL;     /* Pointer to the driver context */
    lptsAUTHAES             lptsAuthAes = NULL;
    uint32              uiIdx = 0;


    DBPRINT((L("AES_Deinit: Called.\n")));
    if (!lpvContext)
    {
        /* Invalid channel, do nothing... */
        DBGERROR((L("AES_Deinit: Error: No Context.\r\n")));
        return FALSE;
    }

    /* Point to the device driver context... */
    lptsAuthContext = PCONTEXT(lpvContext);
    uiIdx = 0;
    
    /* Undo the device driver... */
    AuthTransportDeinit (PTRANSPORT(lptsAuthContext));

    /*unregister navigation input device*/
    lptsAuthAes = PAES(lpvContext);
    if ( NULL != lptsAuthAes )
    {
        if (NULL != lptsAuthAes->pvUser)
        {
            input_unregister_device((struct input_dev *)lptsAuthAes->pvUser);

        #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
            kfree(lptsAuthAes->pvUser);
        #else
            input_free_device(lptsAuthAes->pvUser);
        #endif
            lptsAuthAes->pvUser = NULL;
        }
    }

    DBPRINT((L("AES_Deinit: AES_Deinit OK.\n")));
   

    /* Free up the remainder of the device driver context... */
    kfree (lptsAuthContext);
    g_lptsAuthContext = NULL;

    return TRUE;
}

/*++
===============================================================================

Function AES_Open

The AES_Open() function initializes access to the driver.

Parameters
lpvContext (input)      Pointer to the driver context returned by the
                        AES_Init() function.

uiAccess (input)        Specifies the access level requested by the caller,
                        such as shared read or read/write.

uiShareMode (input)     Specifies the sharing mode that the calling application
                        passed to the CreateFile() function.

Return Value            Returns lpvContext cast to a uint32. This serves as
                        a handle representing access to the device driver.
                        If access is not granted, this function returns a
                        zero (0).

===============================================================================
--*/

uint32
AES_Open (void *    lpvContext,
          uint32    uiAccess,
          uint32    uiShareMode)
{
    lptsAUTHCONTEXT     lptsAuthContext;    /* Pointer to the driver context */
    lptsAUTHTRANSPORT            lptsAuthTransport;         /* Pointer to the transport context */
    lptsAUTHPLATFORM    lptsAuthPlatform;   /* Pointer to the platform context*/
    lptsAUTHTRANSPORT lptsTransport;  /*Pointer to the transport context*/

    /* Get the current context... */
    lptsAuthContext = PCONTEXT(lpvContext);
    lptsAuthPlatform = PPLATFORM(lpvContext);
    lptsTransport = PTRANSPORT(lpvContext);

    /* Is this a valid context? */
    if ( lptsAuthContext )
    {
        lptsAuthTransport = PTRANSPORT(lptsAuthContext);

        /* Is the device currently open? */
        if ( !lptsAuthContext->sAuthAES.uiOpenCount )
        {
            /* Mark it open and return... */
            lptsAuthContext->sAuthAES.uiOpenCount = 1;
            lptsAuthPlatform->iPid = current->pid;
            lptsTransport->bOperationAborted = FALSE;
            
            
            DBPRINT((L("AES_Open: OK.\n")));
            return (uint32) lptsAuthContext;
        }
    }

    /* Device cannot be opened at this time... */
    DBGERROR((L("AES_Open: Error: Failed.\n")));
    return 0;
}


/*++
===============================================================================

Function AES_Close

The AES_Close() function closes access to  the finger print sensor driver. It
is called after an application calls the CloseHandle() function.

Note: closing the driver does not de-allocate memory, and it does not prevent
the sensor from being used as a navigation device.

Parameters
lpvContext (input)      Pointer to the driver context.

Return Value            Returns a TRUE to indicate that the driver has been
                        closed, FALSE otherwise.

===============================================================================
--*/

BOOL
AES_Close (void *   lpvContext)
{
    lptsAUTHCONTEXT     lptsAuthContext;    /* Pointer to the driver context */
    lptsAUTHTRANSPORT lptsTransport;  /*Pointer to the transport context*/

    /* Get the current context... */
    lptsAuthContext = PCONTEXT(lpvContext);
    lptsTransport = PTRANSPORT(lpvContext);

    /* Is this a valid context? */
    if ( lptsAuthContext )
    {
        /* Is the device currently open? */
        if ( lptsAuthContext->sAuthAES.uiOpenCount )
        {
            /* Mark it closed and return... */
            DBPRINT((L("AES_Close: OK.\n")));
            lptsAuthContext->sAuthAES.uiOpenCount = 0;
            lptsTransport->bOperationAborted = FALSE;
            
            return TRUE;
        }
    }

    /* Invalid request to close the device... */
    DBGERROR((L("AES_Close: Error: Failed.\n")));
    return FALSE;
}


/*++
===============================================================================

Function AES_Read

The AES_Read() is a pass-through to the AuthTransportRead() function which returns
a swipe buffer to the calling application when one is available. If a swipe
buffer is not available in a reasonable time frame, the function should return
without data and allow the application to decide if it wants to continue or
cancel the operation.

Parameters
lpvContext (input)      Pointer to the driver context.

lpvBuf (output)         Pointer to the location where the swipe buffer
                        should be copied.

uiBytes (input)         The size of the buffer, in bytes, pointed to by lpvBuf.

Return Value            The number of bytes read indicates success. A value
                        of -1 indicates failure.

===============================================================================
--*/

uint32
AES_Read (void *    lpvContext,
          void *    lpvBuf,
          uint32    uiBytes)
{
    uint32 uiReturnVal;

    uiReturnVal = AuthTransportRead((lptsAUTHTRANSPORT) lpvContext,
                        lpvBuf,
                        uiBytes);

    return( uiReturnVal );
}


/*++
===============================================================================

Function AES_Write

The AES_Write() is a pass-through to the AuthHalWrite() function which sends
raw write commands to the sensor.

Please Note: This is a special case for use in the x86 and arm GPL drivers.
             Also, you MUST put the sensor into Raw Mode before calling this
             function.  AND, you must send the IOCTL (AUTH_IOCTL_DMA_INT_DEPTH)
             down to the driver with the amount of expected data to be
             received from any commands sent to the sensor through this write.

Parameters
lpvContext (input)      Pointer to the driver context.

lpvParams (input)       Pointer to the bytes to be written to the sensor.

uiBytes (input)         Specifies the number of bytes pointed to by lpvParams.

Return Value            The number of bytes written indicates success. A value
                        of -1 indicates failure.

===============================================================================
--*/

uint32
AES_Write (void *   lpvContext,
           void *   lpucWrite,
           uint32   uiBytes )
{
    DBPRINT((L("AES_Write: Writing directly to HAL.\n" )));

    /* Try to send the raw data to the sensor... */
    return AuthTransportWrite (PTRANSPORT(lpvContext),
                                       (uint8 *)       lpucWrite,
                                       (uint32)        uiBytes) ;

}


/*++
===============================================================================

Function AES_PowerDown

This function indicates to the serial port driver that the platform is about
to go into suspend mode.

Parameters
pds         [in]    Pointer to the device state returned by the AES_Init
                    function.

Return Values
void

Remarks
This function is exported to users. As with all power-down handlers, this
function cannot call functions in other DLLs, memory allocators, debugging
output functions, or perform any action that could cause a page fault.
===============================================================================
--*/

void AES_PowerDown(void * lpvContext)
{
    uint32                  uiBytesTransferred;

    AuthTransportIOControl (lpvContext,
                      AUTH_IOCTL_OS_SUSPEND,
                      NULL,
                      0,
                      NULL,
                      0,
                      &uiBytesTransferred);
}


/*++
===============================================================================

Function AES_PowerUp

This function indicates to the serial port driver that the platform is
resuming from suspend mode.

Parameters
pds         [in]    Pointer to the device state returned by the AES_Init
                    function.

Return Values
void

Remarks
This function is exported to users. As with all power-up handlers, this
function cannot call functions in other DLLs, memory allocators, debugging
output functions, or perform any action that could cause a page fault.
===============================================================================
--*/

void AES_PowerUp(void * lpvContext)
{
    uint32                  uiBytesTransferred;

    AuthTransportIOControl (lpvContext,
                      AUTH_IOCTL_OS_RESUME,
                      NULL,
                      0,
                      NULL,
                      0,
                      &uiBytesTransferred);
}

extern int32 TransportSetupTransfer(IN void *lpvContext, 
                                                                IN uint32 uiPacketLen, 
                                                                IN uint32 uiNShot, 
                                                                IN BOOL bKeepOn,
                                                                IN uint32  uiReadPolicy,
                                                                IN uint32  uiOptionFlags);

#ifdef SENSOR_IO_TEST_850
int32 SensorIoTest_850()
{
        uint8 ucReset[7] = {0x80, 0x14, 0, 7, 0, 0, 0};
        uint8 ucGetSatus[7] = {0x80, 0x10, 0, 7, 0, 0};
        uint8 ucReadBuffer[40];
        int32 iRet = 0;


        printk("\n --------850 SensorIoTest begin--------------\n");

        TransportSetupTransfer(g_lptsAuthContext, 20, 1, FALSE, RP_DO_NOT_READ, 1);

        printk("TEST: Send reset command to the sensor\n");

        AuthTransportWrite(g_lptsAuthContext, ucReset, sizeof(ucReset));

        AuthOsSleep (30);

        printk("TEST: Reading data...\n");
        iRet = AuthTransportRead(g_lptsAuthContext, ucReadBuffer, 10);

        printk("TEST: Data received: %x %x %x %x ...\n", ucReadBuffer[0], ucReadBuffer[1], ucReadBuffer[2], ucReadBuffer[3]);
        if ((ucReadBuffer[0] != 0x80) || (ucReadBuffer[1] != 0x2f))
        {
            printk("TEST failed: invalid interrupt packet\n\n");
            return -1;
        }

        //printk("authfp_probe: call setup transfer\n");
        TransportSetupTransfer(g_lptsAuthContext, 16, 1, FALSE, RP_READ_ON_REQ, 0);

        printk("TEST: Send GetStatus command to the sensor\n");
        AuthTransportWrite(g_lptsAuthContext, ucGetSatus, sizeof(ucGetSatus));

        printk("TEST: Reading data...\n");
        iRet = AuthTransportRead(g_lptsAuthContext, ucReadBuffer, 16);

        printk("TEST: Data received: %x %x %x %x ...\n\n", ucReadBuffer[0], ucReadBuffer[1], ucReadBuffer[2], ucReadBuffer[3]);

        if ((ucReadBuffer[0] != 0x80) || (ucReadBuffer[1] != 0x20))
        {
            printk("TEST failed: invalid status packet\n\n");
            return -1;
        }

        printk("\n --------850 SensorIoTest succeeded--------------\n");

        return 0;
        
        
    }
#endif

#ifdef SENSOR_IO_TEST_1750
int32 SensorIoTest_1750()
{

    uint32 uiCount;
    uint8 ucReadBuffer[200];
    BOOL    bPass = TRUE;

    uint32 uiTestLoop=2;

    /*
    ** The following string will set the Rogers and later
    ** sensors to corresponding SSI mode. Once the proper
    ** SSI mode is set, Rogers will send the sensor ID to
    ** the driver with the following data sequence.
    ** Sensor Returns: (8 Bytes): 0x07, 0x05, 0x00, 0x50,
    **                            0x17, 0x20, 0x01, 0x00
    */
    /* D1-D0: set SSI MODE
    **     00b = SPI Slave
    **     01b = SPI Master with frame sync per transfer
    **     10b = SPI Master with frame sync per byte
    **     11b = McBSP master
    */
	 uint8 aucCmdCfg[] = {
		0x47, 0x02, 0x00,
        0x00, 0x00
	};

    /*
    ** The following string will set the Rogers
    ** sensors in ECHO mode.
    ** Once the sensor receives the command with header 0x70,
    ** it will echo the data back to the driver.
    ** Sensor Returns: (126 Bytes): 0x70, 0x7B, 0x00, 0x80,
    ** 0x7F, 0x81, 0x7E, 0x82, ...
    */
     uint8 aucCmdEcho[] = {
        0x70, 0x7B, 0x00,
        0x80, 0x7F, 0x81, 0x7E, 0x82, 0x7D, 0x83, 0x7C,
        0x84, 0x7B, 0x85, 0x7A, 0x86, 0x79, 0x87, 0x78,
        0x88, 0x77, 0x89, 0x76, 0x8A, 0x75, 0x8B, 0x74,
        0x8C, 0x73, 0x8D, 0x72, 0x8E, 0x71, 0x8F, 0x70,

        0x90, 0x6F, 0x91, 0x6E, 0x92, 0x6D, 0x93, 0x6C,
        0x94, 0x6B, 0x95, 0x6A, 0x96, 0x69, 0x97, 0x68,
        0x98, 0x67, 0x99, 0x66, 0x9A, 0x65, 0x9B, 0x64,
        0x9C, 0x63, 0x9D, 0x62, 0x9E, 0x61, 0x9F, 0x60,

        0xA0, 0x5F, 0xA1, 0x5E, 0xA2, 0x5D, 0xA3, 0x5C,
        0xA4, 0x5B, 0xA5, 0x5A, 0xA6, 0x59, 0xA7, 0x58,
        0xA8, 0x57, 0xA9, 0x56, 0xAA, 0x55, 0xAB, 0x54,
        0xAC, 0x53, 0xAD, 0x52, 0xAE, 0x51, 0xAF, 0x50,

        0xB0, 0x4F, 0xB1, 0x4E, 0xB2, 0x4D, 0xB3, 0x4C,
        0xB4, 0x4B, 0xB5, 0x4A, 0xB6, 0x49, 0xB7, 0x48,
        0xB8, 0x47, 0xB9, 0x46, 0xBA, 0x45, 0xBB, 0x44,
        0xBC, 0x43, 0xBD
    };


    printk("\n --------1750 SensorIoTest begin--------------\n");


    /*command to config SSI*/
    TransportSetupTransfer(g_lptsAuthContext, 8, 1, FALSE, RP_READ_ON_INT, 0);
    //printk("send SSI configuration command\n");
    AuthTransportWrite(g_lptsAuthContext, aucCmdCfg, sizeof(aucCmdCfg));
    AuthOsSleep(10);

    AuthTransportRead(g_lptsAuthContext, ucReadBuffer, 8);
    //printk("SSI command: Data received: %x %x %x %x ...\n", ucReadBuffer[0], ucReadBuffer[1], ucReadBuffer[2], ucReadBuffer[3]);

    while (uiTestLoop--)
    {
        /*echo command*/
        TransportSetupTransfer(g_lptsAuthContext, sizeof(aucCmdEcho), 1, FALSE, RP_READ_ON_INT, 0);
        //printk("send ECHO test command, aucCmdTest=0x%x\n", aucCmdEcho);
        AuthTransportWrite(g_lptsAuthContext, aucCmdEcho, sizeof(aucCmdEcho));
        AuthOsSleep(2);

        memset(ucReadBuffer, 0, sizeof(ucReadBuffer));
        AuthTransportRead(g_lptsAuthContext, ucReadBuffer, sizeof(aucCmdEcho));
        printk("Received ECHO test data ( and ECHO command) :\n");

        for (uiCount=0; uiCount<sizeof(aucCmdEcho); uiCount++)
        {
            if (aucCmdEcho[uiCount] != ucReadBuffer[uiCount])
            {
                printk("%02x(!=%02x) ", ucReadBuffer[uiCount], aucCmdEcho[uiCount]);
                bPass = FALSE;
            }
            else
            {
                printk("%02x(%02x) ", ucReadBuffer[uiCount], aucCmdEcho[uiCount]);
            }
            if ((uiCount+1)%8 == 0)
            {
                printk("\n");
            }
        }
    }

    if (bPass)
    {
        printk("\necho test sucessful\n");
    }
    else
    {
        printk("\necho test failed\n");
    }

    printk("\n --------1750  SensorIoTest end------------\n");

    return TR_OK;
}

#endif

/*++
===============================================================================

Function authfp_probe

The authfp_probe() function holds the driver-specific logic to
bind the driver to a given device. That includes verifying that
the device is present, that it's a version the driver can handle,
that driver data structures can be allocated and initialized,
and that any hardware can be initialized. Drivers often store a
pointer to their state with dev_set_drvdata(). When the driver
has successfully bound itself to that device, then probe() returns
zero and the driver model code will finish its part of binding
the driver to that device.

Parameters
dev                         Pointer to the platform device.

Return Value                Zero if success, an error code otherwise.

===============================================================================
--*/
/* FUJITSU:2011-12-12 FingerPrintSensor mod start */
static int __devinit authfp_probe(struct platform_device *pdev)
/* FUJITSU:2011-12-12 FingerPrintSensor mod end */
{
    DBPRINT((L("authfp_probe: Called.\n")));

#if 0
    /* register ourselves as a serial driver */
    if (register_chrdev(AUTH_DEVICE_NO_MAJOR, DRIVER_NAME, &sSensorFileOperations))
	{
        DBGERROR((L("authfp_probe: Error: register_chrdev() Failed For Major %d.\n"), AUTH_DEVICE_NO_MAJOR));
        return -1;
    }
#endif

    sema_init(&ioctl_semaphore, 1);
    sema_init(&open_semaphore, 1);

    /* temp */
    //init_MUTEX(&g_sSemaphore);

    if (0 == AES_Init(NULL))
    {
        /* there was an error in platform-specific initialization */
        DBGERROR((L("authfp_probe: Error: AES_Init() Failed.\n")));
        //unregister_chrdev(AUTH_DEVICE_NO_MAJOR, DRIVER_NAME);
        return -1;
    }

    g_pucMemRead = kmalloc(MAX_READ_BUF, GFP_KERNEL);

    if (NULL == g_pucMemRead)
    {
        DBGERROR((L("authfp_probe: Error: kmalloc() Failed.\n")));
        //unregister_chrdev(AUTH_DEVICE_NO_MAJOR, DRIVER_NAME);
        return -1;
    }
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-S */
	{
		int			retval = 0;
		uint8_t		nv_val;
		g_log_drv_hw = 0;
		g_log_drv_hal = 0;

		nv_val = 0;
		retval = get_nonvolatile(&nv_val, APNV_FINGERPRINT_LOGSW_DATA_I, 1);
		if (retval > 0){
			if (nv_val & APNV_FINGERPRINT_LOGSW_DATA_MASK) {
				g_log_drv_hw = 1;
			}
		}
		nv_val = 0;
		retval = get_nonvolatile(&nv_val, APNV_FINGERPRINT_LOGSW_IOCTL_I, 1);
		if (retval > 0){
			if (nv_val & APNV_FINGERPRINT_LOGSW_IOCTL_MASK) {
				g_log_drv_hal = 1;
			}
		}
		FPDBGLOG( "%s : drv_hw = %d, drv_hal = %d\n", __func__, g_log_drv_hw, g_log_drv_hal);
	}
/* FUJITSU:2012-10-04 FingerPrintSensor_Log add-E */

#ifdef SENSOR_IO_TEST_850
    SensorIoTest_850();
#endif

#ifdef SENSOR_IO_TEST_1750
    SensorIoTest_1750();
#endif

    return 0;

}


/*++
===============================================================================

Function authfp_remove

The authfp_remove() is called to unbind a driver from a device.
This may be called if a device is physically removed from the
system, if the driver module is being unloaded, during a reboot
sequence, or in other cases.

It is up to the driver to determine if the device is present or
not. It should free any resources allocated specifically for the
device.

Parameters
dev                         Pointer to the platform device.

Return Value                Zero if success, an error code otherwise.

===============================================================================
--*/

static int __exit authfp_remove(struct platform_device *pdev)
{
    DBPRINT((L("authfp_remove: Called.\n")));

    AES_Deinit(g_lptsAuthContext);
    g_lptsAuthContext = NULL;

    kfree(g_pucMemRead);
    g_pucMemRead = NULL;

    /* unregister our serial driver */
    //unregister_chrdev(AUTH_DEVICE_NO_MAJOR, DRIVER_NAME);

    return 0;
}


/*++
===============================================================================

Function authfp_suspend

The authfp_suspend() is called to put the device in a low
power state.

Parameters
dev                         Pointer to the platform device.
state                       Power management message.

Return Value                Zero if success, an error code otherwise.

===============================================================================
--*/

static int authfp_suspend(struct platform_device *pdev, pm_message_t state)
{
    uint32 uiBytesTransferred;
    
    DBPRINT((L("authfp_suspend: Called.\n")));


    if (TR_OK == AuthTransportIOControl(g_lptsAuthContext,
                                          AUTH_IOCTL_OS_SUSPEND,
                                          NULL,
                                          0,
                                          NULL,
                                          0,
                                          &uiBytesTransferred))
        {
                DBPRINT((L("ioctl: AUTH_IOCTL_OS_SUSPEND OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_OS_SUSPEND failed.\n")));
            }
            
    return 0;
}


/*++
===============================================================================

Function authfp_resume

The authfp_resume() is used to bring a device back from a low
power state.

Parameters
dev                         Pointer to the platform device.

Return Value                Zero if success, an error code otherwise.

===============================================================================
--*/

static int authfp_resume(struct platform_device *pdev)
{
    uint32 uiBytesTransferred;
    
    DBPRINT((L("authfp_suspend: Called.\n")));


    if (TR_OK == AuthTransportIOControl(g_lptsAuthContext,
                                          AUTH_IOCTL_OS_RESUME,
                                          NULL,
                                          0,
                                          NULL,
                                          0,
                                          &uiBytesTransferred))
        {
                DBPRINT((L("ioctl: AUTH_IOCTL_OS_RESUME OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_OS_RESUME failed.\n")));
            }
    return 0;
}




/*++
===============================================================================

Function fp_dev_init

The fp_dev_init() function will be called by the kernel to globally initialize the module.

Parameters

Return Value            Returns 0 on success and a negative error number if failure.

===============================================================================
--*/

static int fp_dev_init(void)
{
    int retval = 0;
    struct device *device = NULL;

    DBPRINT((L("init_module: Called.\n")));

#if (TRANSPROT_INTERFACE_USED == TR_IFACE_SPI_S)
    retval = spi_fp_attach();
    if (retval)
    {
        DBGERROR((L("init_module: Error: failed to connect spi master.\n")));
        return retval;
    }
#elif (TRANSPROT_INTERFACE_USED == TR_IFACE_I2C)
    retval = i2c_fp_attach();
    if (retval)
    {
        DBGERROR((L("init_module: Error: failed to connect i2c.\n")));
        return retval;
    }
#endif

    retval = alloc_chrdev_region(&dev, 0, 1, "aeswipe");
    if (retval < 0)
    {
        printk("fp_dev_init: error alloc_chrdev_region returned %d\n", retval);
        DBGERROR((L("fp_dev_init: error alloc_chrdev_region returned %d\n"), retval));
        return -1;
    }

    cdev_init(&c_dev, &sSensorFileOperations);
    c_dev.owner = THIS_MODULE;
    c_dev.ops = &sSensorFileOperations;

    retval = cdev_add(&c_dev, dev, 1);
    if (retval) 
    {
        DBGERROR((L("fp_dev_init: cdev_add returned %d\n"), retval));
        unregister_chrdev_region(dev,1);
        return -1;
    }

    retval = platform_driver_register(&aeswipe_driver);
    if (retval)
    {
        DBGERROR((L("init_module: Error: failed in platform_driver_register.\n")));
        unregister_chrdev_region(dev,1);
        cdev_del(&c_dev);
        return retval;
    }

    aeswipe_device = platform_device_alloc("aeswipe", -1);
    if (!aeswipe_device)
    {
        DBGERROR((L("init_module: Error: failed in platform_device_alloc.\n")));
        unregister_chrdev_region(dev,1);
        cdev_del(&c_dev);
        platform_driver_unregister(&aeswipe_driver);
        return -ENOMEM;
    }

    retval = platform_device_add(aeswipe_device);
	if (retval) 
    {
        DBGERROR((L("init_module: Error: failed in platform_device_add.\n")));
        unregister_chrdev_region(dev,1);
        cdev_del(&c_dev);
        platform_device_put(aeswipe_device);
        platform_driver_unregister(&aeswipe_driver);
        return retval;
    }

    /* Create device node */
    s_DevClass = class_create(THIS_MODULE, "aeswipe");
    if (IS_ERR(s_DevClass)) 
    {
        DBGERROR((L("init_module: Error: failed in class_create.\n")));
        unregister_chrdev_region(dev,1);
        cdev_del(&c_dev);
        platform_device_put(aeswipe_device);
        platform_driver_unregister(&aeswipe_driver);
        return PTR_ERR(s_DevClass);
    }

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)

    /* 
    ** Do not use device_create_drvdata() for a uniform API call, 
    ** since it will probably go away in future kernel versions.
    */
    //dev = device_create (s_DevClass, NULL, MKDEV(AUTH_DEVICE_NO_MAJOR, 200), "aeswipe");
    device = device_create (s_DevClass, NULL,dev, "aeswipe");


#else

    /*
    ** In >=2.6.27 this function adds a new paramenter (parameter 
    ** for passing data in this case can be NULL) in this function 
    ** and the module crash because use: 0 as pointer to char*, 
    ** all variables are moved 1 position.
    */
    //dev = device_create (s_DevClass, NULL, MKDEV(AUTH_DEVICE_NO_MAJOR, 200), NULL, "aeswipe");
    device = device_create (s_DevClass, NULL,dev, NULL, "aeswipe");


#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)) */

    if(IS_ERR(device)) 
    {
        DBGERROR((L("init_module: Error: failed in device_create.\n")));
        unregister_chrdev_region(dev,1);
        cdev_del(&c_dev);
        class_destroy(s_DevClass);
        platform_device_put(aeswipe_device);
        platform_driver_unregister(&aeswipe_driver);
        return PTR_ERR(device);
    }

    return retval;
}


/*++
===============================================================================

Function fp_dev_exit

The fp_dev_exit() function will be called by the kernel to globally terminate the module.

Parameters

Return Value

===============================================================================
--*/

static void fp_dev_exit(void)
{
    DBPRINT((L("fp_dev_exit: Called.\n")));

    /*delete device node under /dev */
    //device_destroy(s_DevClass, MKDEV(AUTH_DEVICE_NO_MAJOR, 200));
    
    unregister_chrdev_region(dev,1);
    cdev_del(&c_dev);
    device_destroy(s_DevClass, dev);

    /*delete class*/
    class_destroy(s_DevClass);

    platform_device_unregister(aeswipe_device);
    platform_driver_unregister(&aeswipe_driver);

#if (TRANSPROT_INTERFACE_USED == TR_IFACE_SPI_S)
    fp_spi_deattach();
#elif (TRANSPROT_INTERFACE_USED == TR_IFACE_I2C)
    i2c_fp_deattach();
#endif

}

/*++
===============================================================================

Function module_entry_open

The module_entry_open() function will be called when someone opens the device as a file.

Parameters
psInode                     Pointer to the inode associated to the file.
psFile                      Pointer to a struct representing the file associated
                            to the device.

Return Value                Zero if success, an error code otherwise.

===============================================================================
--*/

static int module_entry_open(struct inode* psInode, struct file* psFile)
{
    int32 iRet = 0;
    
    DBPRINT((L("open: Called.\n")));

    if (-EINTR == down_interruptible(&open_semaphore))
    {
        DBPRINT((L("ioctl: Semaphore error.\n")));
        return -EINTR;
    }

    iRet = AES_Open(g_lptsAuthContext, 0, 0);
    if (0 == iRet)
    {
        DBGERROR((L("open: Error: AES_Open() Failed.\n")));
        up(&open_semaphore);
        return -EINTR;
    }
    DBPRINT((L("open: after AES_Open(), 0x%lx\n"), (uint32)g_lptsAuthContext));

    /* Save the address of this device in the file */
    psFile->private_data = g_lptsAuthContext;

    up(&open_semaphore);

    return 0;
}

/*++
===============================================================================

Function module_entry_close

The module_entry_close() function will be called when someone closes the device as a file.

Parameters
psInode                     Pointer to the inode associated to the file.
psFile                      Pointer to a struct representing the file associated
                            to the device.

Return Value                Zero if success, an error code otherwise.

===============================================================================
--*/

static int module_entry_close(struct inode* psInode, struct file* psFile)
{
    BOOL bRet = 0;

    /* Lock the semaphore */
    /*
    if (-EINTR == down_interruptible(&g_sSemaphore))
    {
        DBPRINT((L("close: Semaphore error.\n")));
        return -EINTR;
    }
    */

    bRet = AES_Close(g_lptsAuthContext);

    //up(&g_sSemaphore);
    return (bRet == TRUE)? 0 : 1;
}


/*++
===============================================================================

Function module_entry_read

The module_entry_read() function will be called when the user reads data from the device file.

Parameters
psFile                      Pointer to the struct that represents the device file.
pcBuffer                    Pointer to the user-space buffer where the read
                            data will be copied.
uCount                      Data length in bytes.
puOffsetPosition            Pointer to a loff_t that holds the offset
                            from where the data will be read.

Return Value                If successful the number of bytes read, a negative error
                            code otherwise.

===============================================================================
--*/

static ssize_t module_entry_read(struct file * psFile, char * pcBuffer,
                                size_t uCount, loff_t * puOffsetPosition)
{

    int32 iBytesRead = 0;

    if (0 == uCount)
    {
        return 0;
    }

	/* Lock the semaphore */
    /*
    if (-EINTR == down_interruptible(&g_sSemaphore))
    {
        DBPRINT((L("read: Semaphore error.\n")));
        return -EINTR;
    }*/

    MEMSET(g_pucMemRead, 0, MAX_READ_BUF);

    iBytesRead = AES_Read(g_lptsAuthContext, g_pucMemRead, uCount);
    
    if (TR_OPERATION_ABORTED == iBytesRead)
    {
        return iBytesRead;
    }

    if (iBytesRead > MAX_READ_BUF)
    {
        iBytesRead = MAX_READ_BUF;
        //DBGERROR((L("read: exceeds the maximum (%d, %d).\r\n"), iBytesRead, s_uiMaxReadBufferLength));
    }

    if (iBytesRead > uCount)
    {
        iBytesRead = uCount;
        DBGERROR((L("read: driver gives more data than app expected (%d, %d).\r\n"), (int)iBytesRead, uCount));
    }


    //up(&g_sSemaphore);

    if (copy_to_user(pcBuffer, g_pucMemRead, (uint32)iBytesRead))
    {
        DBGERROR((L("read: Error: copy_to_user() Failed.\n")));
        return -EFAULT;
    }
    else
    {
       return iBytesRead;
    }
}


/*++
===============================================================================

Function module_entry_write

The module_entry_write() function will be called when the user writes data to the device file.

Parameters
psFile                      Pointer to the struct that represents the device file.
pcBuffer                    Pointer to the user-space buffer where the read
                            data will be copied.
uCount                      Data length in bytes.
puOffsetPosition            Pointer to a loff_t that holds the offset
                            from where the data will be read.

Return Value                If successful the number of bytes written, a negative error
                            code otherwise.

===============================================================================
--*/

static ssize_t module_entry_write(struct file* psFile, const char* pcBuffer,
                                 size_t uCount, loff_t* puOffsetPosition)
{
    uint8* pcTmpBuffer = NULL;
    uint32 uiRet = 0;

    //printk("module_entry_write: is called\n");

    /* Allocate temporary buffer to hold what is requested to be written */
    pcTmpBuffer = kmalloc(uCount, GFP_KERNEL);
    if (NULL == pcTmpBuffer)
    {
        DBGERROR((L("write: Error: Temporary Buffer Allocation Failed.\n")));
        return -ENOMEM;
    }

    if (copy_from_user(pcTmpBuffer, pcBuffer, uCount))
    {
        DBGERROR((L("write: Error: copy_from_user() Failed.\n")));
        kfree(pcTmpBuffer);
        return -ENOMEM;
    }

    /* Lock the semaphore */
    /*
    if (-EINTR == down_interruptible(&g_sSemaphore))
    {
        DBPRINT((L("write: Semaphore error.\n")));
        return -EINTR;
    }
    */

    //printk("module_entry_write: calling AES_Write with command: %x %x %x, uCount=%d\n", uCount, *pcTmpBuffer, *(pcTmpBuffer+1), *(pcTmpBuffer+2));

    uiRet=  AES_Write(g_lptsAuthContext, pcTmpBuffer, uCount);

    //printk("module_entry_write:  AES_Write return %d\n", uiRet);


    if (NULL != pcTmpBuffer)
    {
        kfree(pcTmpBuffer);
        pcTmpBuffer = NULL;
    }

    //up(&g_sSemaphore);

    return uiRet;

}


/*++
===============================================================================

Function module_entry_ioctl

The module_entry_ioctl() function is the driver entry funtion that receives ioctl from the user space.

Parameters
psInode                     Pointer to the inode associated with the file.
psFile                      Pointer to a struct representing the file associated
                            with the device.
uCommand                    Integer value of the configuration command sent
                            to the device.
uArgument                   Configuration argument for uCommand.

Return Value                Zero if success, an error code otherwise.

===============================================================================
--*/

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
static long module_entry_ioctl(struct file* psFile,
                             unsigned int uCommand, unsigned long uArgument)
#else
static int module_entry_ioctl(struct inode* psInode, struct file* psFile,
                             unsigned int uCommand, unsigned long uArgument)
#endif
{
    int                             iRetVal = -ENOTTY;
    lptsAUTHTRANSPORT                     lptsAuthTransport = NULL;
    uint32                          uiBytesTransferred = 0;
    int32       iIoctlRetCode = -1;


    /* Lock the semaphore */
    if ( uCommand != AUTH_IOCTL_CANCEL_TRANSFER)
    {
        if (-EINTR == down_interruptible(&ioctl_semaphore))
        {
            DBPRINT((L("ioctl: Semaphore error.\n")));
            return -EINTR;
        }
    }
    

    //printk("module_entry_ioctl:  is called with uCommand=%d\n", uCommand);

    lptsAuthTransport = &g_lptsAuthContext->sAuthTRANSPORT;

    /* Process the IO controls */
    switch (uCommand)
    {

        case AUTH_IOCTL_SETUP_TRANSFER:
        {
            tsTrSetupTransferIoctlBuffer stTransferInfo;

            iRetVal = copy_from_user(&stTransferInfo,
                                     (tsTrSetupTransferIoctlBuffer *)uArgument,
                                     sizeof(tsTrSetupTransferIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_SETUP_TRANSFER Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stTransferInfo,
                                          sizeof(tsTrSetupTransferIoctlBuffer),
                                          (uint8*)&stTransferInfo,
                                          sizeof(tsTrSetupTransferIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_SETUP_TRANSFER OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_SETUP_TRANSFER failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrSetupTransferIoctlBuffer *)uArgument,
                                       &stTransferInfo,
                                       sizeof(tsTrSetupTransferIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stTransferInfo.sHeader.retCode;

            break;
        }

        case AUTH_IOCTL_TRANSPORT_OPEN:
        {
            tsTrOpenCloseIoctlBuffer   stOpenIoctlBuffer;
            
            iRetVal = copy_from_user(&stOpenIoctlBuffer,
                                     (tsTrOpenCloseIoctlBuffer *)uArgument,
                                     sizeof(tsTrOpenCloseIoctlBuffer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_TRANSPORT_OPEN Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stOpenIoctlBuffer,
                                          sizeof(tsTrOpenCloseIoctlBuffer),
                                          (uint8*)&stOpenIoctlBuffer,
                                          sizeof(tsTrOpenCloseIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_TRANSPORT_OPEN OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_TRANSPORT_OPEN failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrOpenCloseIoctlBuffer *)uArgument,
                                       &stOpenIoctlBuffer,
                                       sizeof(tsTrOpenCloseIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stOpenIoctlBuffer.sHeader.retCode;
            
            break;
        }

        case AUTH_IOCTL_TRANSPORT_RESET:
        {
            tsTrResetIoctlBuffer stResetIoctlBuffer;

            iRetVal = copy_from_user(&stResetIoctlBuffer,
                                     (tsTrResetIoctlBuffer *)uArgument,
                                     sizeof(tsTrResetIoctlBuffer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_TRANSPORT_RESET Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }
            
            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stResetIoctlBuffer,
                                          sizeof(tsTrResetIoctlBuffer),
                                          (uint8*)&stResetIoctlBuffer,
                                          sizeof(tsTrResetIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_TRANSPORT_RESET OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_TRANSPORT_RESET failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrResetIoctlBuffer *)uArgument,
                                       &stResetIoctlBuffer,
                                       sizeof(tsTrResetIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stResetIoctlBuffer.sHeader.retCode;
            
            break;
        }

        case AUTH_IOCTL_CHECK_PROTOCOL:
        {
            tsTrCheckProtocolIoctlBuffer stCheckProtocolIoctlBuffer;
            
            iRetVal = copy_from_user(&stCheckProtocolIoctlBuffer,
                                     (tsTrCheckProtocolIoctlBuffer *)uArgument,
                                     sizeof(tsTrCheckProtocolIoctlBuffer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_CHECK_PROTOCOL Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stCheckProtocolIoctlBuffer,
                                          sizeof(tsTrCheckProtocolIoctlBuffer),
                                          (uint8*)&stCheckProtocolIoctlBuffer,
                                          sizeof(tsTrCheckProtocolIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_CHECK_PROTOCOL OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_CHECK_PROTOCOL failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrCheckProtocolIoctlBuffer *)uArgument,
                                       &stCheckProtocolIoctlBuffer,
                                       sizeof(tsTrCheckProtocolIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stCheckProtocolIoctlBuffer.sHeader.retCode;

            break;
        }
        
        case AUTH_IOCTL_GET_DRVINFO:
        {
            tsTrDriverInfoIoctlBuffer stDriverInfo;
            uint8 *pucDriverInfoBuffer;
            uint32 uiBufferSize = 0;
            tsDriverInfo *pDriverInfo; 

            iRetVal = copy_from_user(&stDriverInfo,
                                     (tsTrDriverInfoIoctlBuffer *)uArgument,
                                     sizeof(tsTrDriverInfoIoctlBuffer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_GET_DRVINFO Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }
            
            uiBufferSize = offsetof(tsTrDriverInfoIoctlBuffer, aucBuffer) + stDriverInfo.uiBufferSize;
            
            pucDriverInfoBuffer = kmalloc(uiBufferSize, GFP_KERNEL);

            iRetVal = copy_from_user(pucDriverInfoBuffer,
                                     (tsTrDriverInfoIoctlBuffer *)uArgument,
                                     uiBufferSize);
            
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_GET_DRVINFO Error: copy_from_user() Failed %d.\n"), iRetVal));
                if (pucDriverInfoBuffer != NULL)
                {
                    kfree(pucDriverInfoBuffer);
                }
                break;
            }
            
            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)pucDriverInfoBuffer,
                                          uiBufferSize,
                                          (uint8*)pucDriverInfoBuffer,
                                          uiBufferSize,
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_GET_DRVINFO OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_GET_DRVINFO failed.\n")));
            }
            
            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrDriverInfoIoctlBuffer *)uArgument,
                                       pucDriverInfoBuffer,
                                       uiBufferSize);

            pDriverInfo = (tsDriverInfo *)(((tsTrDriverInfoIoctlBuffer*)pucDriverInfoBuffer)->aucBuffer);
            
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = ((tsTrIoctlHeader*)pucDriverInfoBuffer)->retCode;

            if (pucDriverInfoBuffer != NULL)
            {
                kfree(pucDriverInfoBuffer);
            }
            
            break;
        }

        case AUTH_IOCTL_SET_DRIVER_POWER:
        {
            tsTrDriverPowerIoctlBuffer stDriverPower;

            iRetVal = copy_from_user(&stDriverPower,
                                     (tsTrDriverPowerIoctlBuffer *)uArgument,
                                     sizeof(stDriverPower)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_SET_DRIVER_POWER Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }
            
            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stDriverPower,
                                          sizeof(tsTrDriverPowerIoctlBuffer),
                                          (uint8*)&stDriverPower,
                                          sizeof(tsTrDriverPowerIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_SET_DRIVER_POWER OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_SET_DRIVER_POWER failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrCheckProtocolIoctlBuffer *)uArgument,
                                       (uint8*)&stDriverPower,
                                       sizeof(tsTrDriverPowerIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stDriverPower.sHeader.retCode;
            
            break;
        }

        case AUTH_IOCTL_GET_DRIVER_POWER:
        {
            tsTrDriverPowerIoctlBuffer stDriverPower;

            iRetVal = copy_from_user(&stDriverPower,
                                     (tsTrDriverPowerIoctlBuffer *)uArgument,
                                     sizeof(stDriverPower)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_GET_DRIVER_POWER Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stDriverPower,
                                          sizeof(tsTrDriverPowerIoctlBuffer),
                                          (uint8*)&stDriverPower,
                                          sizeof(tsTrDriverPowerIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_GET_DRIVER_POWER OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_GET_DRIVER_POWER failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrCheckProtocolIoctlBuffer *)uArgument,
                                       (uint8*)&stDriverPower,
                                       sizeof(tsTrDriverPowerIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stDriverPower.sHeader.retCode;
            
            break;
        }

        case AUTH_IOCTL_CANCEL_TRANSFER:
        {
            tsTrCancelIoctlBuffer stCancelTransfer;

            iRetVal = copy_from_user(&stCancelTransfer,
                                     (tsTrCancelIoctlBuffer *)uArgument,
                                     sizeof(stCancelTransfer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_CANCEL_TRANSFER Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stCancelTransfer,
                                          sizeof(tsTrCancelIoctlBuffer),
                                          (uint8*)&stCancelTransfer,
                                          sizeof(tsTrCancelIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_CANCEL_TRANSFER OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_CANCEL_TRANSFER failed.\n")));
            }
            
            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrCancelIoctlBuffer *)uArgument,
                                       (uint8*)&stCancelTransfer,
                                       sizeof(tsTrCancelIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stCancelTransfer.sHeader.retCode;
            
            break;
        }

#if 0
        case AUTH_IOCTL_STOP_TRANSFER:
        {
            tsTrStopIoctlBuffer stStopTransfer;

            iRetVal = copy_from_user(&stStopTransfer,
                                     (tsTrStopIoctlBuffer *)uArgument,
                                     sizeof(tsTrStopIoctlBuffer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_STOP_TRANSFER Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stStopTransfer,
                                          sizeof(tsTrStopIoctlBuffer),
                                          (uint8*)&stStopTransfer,
                                          sizeof(tsTrStopIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl: AUTH_IOCTL_STOP_TRANSFER OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_STOP_TRANSFER failed.\n")));
            }
            
            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrCancelIoctlBuffer *)uArgument,
                                       (uint8*)&stStopTransfer,
                                       sizeof(tsTrStopIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stStopTransfer.sHeader.retCode;
            
            break;
        }
#endif

        case AUTH_IOCTL_TRANSPORT_EXT:
        {
            tsTrIoctlBuffer stIoctlBuffer;
            uint8 *pucIoctlBuffer;
            uint32 uiIoctlBufferSize;

            iRetVal = copy_from_user(&stIoctlBuffer,
                                     (tsTrIoctlBuffer *)uArgument,
                                     sizeof(tsTrIoctlBuffer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_TRANSPORT_EXT Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            uiIoctlBufferSize = offsetof(tsTrIoctlBuffer, aucBuffer) + stIoctlBuffer.uiDataLen;
            pucIoctlBuffer = kmalloc(uiIoctlBufferSize, GFP_KERNEL);

            iRetVal = copy_from_user(pucIoctlBuffer,
                                     (tsTrIoctlBuffer *)uArgument,
                                     uiIoctlBufferSize); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_TRANSPORT_EXT Error: copy_from_user() Failed %d.\n"), iRetVal));
                if (pucIoctlBuffer != NULL)
                {
                    kfree(pucIoctlBuffer);
                }
                break;
            }

            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)pucIoctlBuffer,
                                          uiIoctlBufferSize,
                                          (uint8*)pucIoctlBuffer,
                                          uiIoctlBufferSize,
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl:AUTH_IOCTL_TRANSPORT_EXT OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_TRANSPORT_EXT failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrCancelIoctlBuffer *)uArgument,
                                       (uint8*)pucIoctlBuffer,
                                       uiIoctlBufferSize);
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = ((tsTrIoctlHeader*)pucIoctlBuffer)->retCode;

            if (pucIoctlBuffer != NULL)
            {
                kfree(pucIoctlBuffer);
            }
            
            break;
        }

        case AUTH_IOCTL_TRANSPORT_CLOSE: 
        {
            tsTrOpenCloseIoctlBuffer   stCloseIoctlBuffer;
            //printk("\driver: module_entry_ioctl: AUTH_IOCTL_SET_TIMEOUT\n\n");
            
            iRetVal = copy_from_user(&stCloseIoctlBuffer,
                                     (tsTrOpenCloseIoctlBuffer *)uArgument,
                                     sizeof(tsTrOpenCloseIoctlBuffer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_SET_TIMEOUT Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stCloseIoctlBuffer,
                                          sizeof(tsTrOpenCloseIoctlBuffer),
                                          (uint8*)&stCloseIoctlBuffer,
                                          sizeof(tsTrOpenCloseIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl:AUTH_IOCTL_TRANSPORT_CLOSE OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_TRANSPORT_CLOSE failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrOpenCloseIoctlBuffer *)uArgument,
                                       (tsTrOpenCloseIoctlBuffer*)&stCloseIoctlBuffer,
                                       sizeof(tsTrOpenCloseIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }
            
            iIoctlRetCode = stCloseIoctlBuffer.sHeader.retCode;
            
            break;
        }

        case AUTH_IOCTL_SET_TIMEOUT:
        {
            tsTrTimeoutIoctlBuffer stTimoutBuffer;
            //printk("\driver: module_entry_ioctl: AUTH_IOCTL_SET_TIMEOUT\n\n");
            
            iRetVal = copy_from_user(&stTimoutBuffer,
                                     (tsTrTimeoutIoctlBuffer *)uArgument,
                                     sizeof(tsTrTimeoutIoctlBuffer)); 
            if (iRetVal)
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_SET_TIMEOUT Error: copy_from_user() Failed %d.\n"), iRetVal));
                break;
            }

            //printk("\driver: module_entry_ioctl: AUTH_IOCTL_SET_TIMEOUT calling AuthTransportIOControl\n\n");
            if (TR_OK == AuthTransportIOControl(lptsAuthTransport,
                                          uCommand,
                                          (uint8*)&stTimoutBuffer,
                                          sizeof(tsTrTimeoutIoctlBuffer),
                                          (uint8*)&stTimoutBuffer,
                                          sizeof(tsTrTimeoutIoctlBuffer),
                                          &uiBytesTransferred))
            {
                DBPRINT((L("ioctl:AUTH_IOCTL_SET_TIMEOUT OK\n")));
            }
            else
            {
                DBGERROR((L("ioctl: AUTH_IOCTL_SET_TIMEOUT failed.\n")));
            }

            /* copy the data to the user */
            iRetVal = copy_to_user((tsTrTimeoutIoctlBuffer *)uArgument,
                                       (tsTrTimeoutIoctlBuffer*)&stTimoutBuffer,
                                       sizeof(tsTrTimeoutIoctlBuffer));
            if (iRetVal)
            {
                DBGERROR((L("ioctl: Error: copy_to_user() Failed.\n")));
            }

            iIoctlRetCode = stTimoutBuffer.sHeader.retCode;

            break;
            
        }
        
        default:
        {
            DBPRINT((L("ioctl: Unknown IOCTL request: %x.\n"), uCommand));
            iIoctlRetCode = -1;
            break;
        }
    }

    if ( uCommand != AUTH_IOCTL_CANCEL_TRANSFER)
    {
        up(&ioctl_semaphore);
    }

    return iIoctlRetCode;

}

MODULE_ALIAS("auth transport");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AuthenTec, Inc.");
MODULE_DESCRIPTION("AuthenTec Transport Driver");

module_init(fp_dev_init);
module_exit(fp_dev_exit);



