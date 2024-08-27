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

/* *************************************************************************

   File:            authisrInterrupt.c

   Description:	    This file contains interrupt service routine related
                    code for use with the AuthenTec Unified Driver Model.

                    The following functions are responsible for handling the
                    OS specific process of installing and removing interrupt
                    service routines.

                    This specific implementation of the ISR is targeted to
                    the SPI-Slave interrupt driven interface.

   Author:			James Deng

   ************************************************************************* */

/** include files **/
#include "hfiles.h"

/** local definitions **/

/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/

void *glpvContext = NULL;
static struct semaphore * g_lptsGpioRxEvent = NULL;        /* Pointer to event... */
volatile BOOL g_bTeminateGpioRxThread = FALSE;

/* FUJITSU:2012.09.10 FP_DRIVER Add Start */
static struct task_struct *g_thread = NULL;
/* FUJITSU:2012.09.10 FP_DRIVER Add End */

/*
int32 GetCurrentTime()
{
    struct timeval time;
    do_gettimeofday(&time);
    printk("driver: GetCurrentTime: time: %d : %d\n", time.tv_sec, time.tv_usec);
}
*/

/** private functions **/

/*++
===============================================================================

Function GpioISR

The GpioISR function is the device GPIO interrutp service routine.

Parameters
irq(input)           irq number to the GPIO interrupt line
dev_id(input)        pointer to the device driver context

Return Value         none

===============================================================================
--*/
static irqreturn_t GpioISR(int irq, void *dev_id)
{
    lptsAUTHCONTEXT     lptsAuthContext = NULL;    /* Pointer to device driver context */
    lptsAUTHTRANSPORT         lptsAuthTransport = NULL;        /* Pointer to the Transport context */


	//DBPRINT((L("GpioRxHandler: irq number is %d.\r\n"), irq));
	

    if (NULL == dev_id)
    {
        DBGERROR((L("GpioRxHandler: Error: No Context.\r\n")));
        return IRQ_HANDLED;
    }

    /* Point to the device driver context... */
    lptsAuthContext = PCONTEXT(dev_id);

    /* Pointer to the Transport context... */
    lptsAuthTransport = PTRANSPORT(lptsAuthContext);

    if( lptsAuthTransport->uiReadPolicy == RP_READ_ON_REQ 
            && lptsAuthTransport->uiOptions==0 )
        {
            return IRQ_HANDLED;
        }
        if( lptsAuthTransport->uiReadPolicy == RP_DO_NOT_READ 
            && lptsAuthTransport->uiOptions==0 )
        {
            return IRQ_HANDLED;
        }

        if (NULL != g_lptsGpioRxEvent)
        {
            //printk("GpioISR: up the signal\n\n\n");
            up(g_lptsGpioRxEvent);
        }
        
    return IRQ_HANDLED;
    
}


/*++
===============================================================================

Function GpioIST

The GpioIST() function is the bottom half of the ISR.

Parameters
data(input)           pointer to the device driver context

Return Value         none

===============================================================================
--*/
static int GpioIST(void *data)
{
    lptsAUTHCONTEXT     lptsAuthContext = NULL;    /* Pointer to device driver context */
    lptsAUTHTRANSPORT         lptsAuthTransport = NULL;        /* Pointer to the Transport context */
    lptsAUTHISR         lptsAuthISR = NULL;        /* Pointer to the ISR context */
    lptsAUTHIO          lptsAuthIO = NULL;         /* Pointer to the IO context */
    lptsAUTHBUFFER      lptsAuthBufferTX = NULL;   /* Pointer to the TX buffer */
    uint32              uiBytesExpected = 0;
    uint32              uiOptions = 0;
    uint32              uiPolicy = 0;
    int                 retval = 0;
    static              uint32              uiIndex = 0;

    if (NULL == data)
    {
        DBGERROR((L("GpioRxThread: Error: No Context.\r\n")));
        return -1;
    }

    /* Point to the device driver context... */
    lptsAuthContext = PCONTEXT(data);

    /* Pointer to the Transport context... */
    lptsAuthTransport = PTRANSPORT(lptsAuthContext);

    /* Pointer to the ISR context... */
    lptsAuthISR = PISR(lptsAuthContext);

    /* Pointer to the IO context */
    lptsAuthIO = PIO(lptsAuthContext);

    /* Pointer to the TX buffer */
    lptsAuthBufferTX = PBUFFERTX(lptsAuthContext);

#ifdef ENABLE_FPS_NSHOT
    /* Pointer to the RX buffer */
    lptsAuthBufferRX = PBUFFERRX(lptsAuthContext);
#endif

    /* Kernel threads are not freezable by default. However, a kernel
    ** thread may clear PF_NOFREEZE for itself by calling set_freezable()
    ** (the resetting of PF_NOFREEZE directly is strongly discouraged).
    ** From this point it is regarded as freezable and must call
    ** try_to_freeze() in a suitable place.
    */
/* FUJITSU:2012.09.10 FP_DRIVER Del Start */
    // set_freezable();
/* FUJITSU:2012.09.10 FP_DRIVER Del End */

    /*set the kernel thread priority*/
    set_user_nice(current, -19);

    /*set_current_state(TASK_UNINTERRUPTIBLE);*/
    //allow_signal(SIGTERM);

/* FUJITSU:2012.09.10 FP_DRIVER Mod Start */
    // while(1)
    while(!kthread_should_stop())
/* FUJITSU:2012.09.10 FP_DRIVER Mod End */
    {
        #if 0    	
        if (signal_pending (current)) /*Terminate thread if the signal is received*/
        {
        	DBGERROR((L("GpioRxThread: IRQ is free.\r\n")));
            free_irq((uint32)gpio_to_irq(SENSOR_GPIO_INT), glpvContext);
            break;
        }
        #endif
        
        if (NULL != g_lptsGpioRxEvent)
        {
            retval = down_interruptible(g_lptsGpioRxEvent);
        }

        //printk("GPIOIST: wakeup from the semaphor\n\n\n");

        if(g_bTeminateGpioRxThread) /*terminate thread*/
        {
            /* Reset the global value to inform AuthIsrUnintall() */
            g_bTeminateGpioRxThread = FALSE;
            break;
        }

        if (retval)
        {
            /* It is recommended to use the try_to_freeze() function
            ** (defined in include/linux/freezer.h), that checks the
            ** task's TIF_FREEZE flag and makes the task enter
            ** refrigerator() if the flag is set.
            */
			#if 0
        	try_to_freeze();
			#else
        	if (!try_to_freeze()) {  
               siginfo_t info;
               dequeue_signal_lock(current, &current->blocked, &info);
        	}
			#endif
            continue;
        }

        AuthOsLock(lptsAuthTransport, 0);
        uiBytesExpected = lptsAuthTransport->uiBytes;
        uiOptions = lptsAuthTransport->uiOptions;
        uiPolicy = lptsAuthTransport->uiReadPolicy;
        AuthOsUnlock(lptsAuthTransport, 0);

        //printk("\nGPIOIST: uiReadPolicy=%d, uiOptions=%d, uiIndex=%d, uiBytesExpected=%d, s_bReadOnUserRequest=%d\n",lptsAuthTransport->uiReadPolicy, uiOptions, uiIndex, uiBytesExpected, s_bReadOnUserRequest);

        lptsAuthTransport->uiOutputBytes[uiIndex] = lptsAuthTransport->uiBytes;

        switch(uiPolicy)
        {
            case RP_DO_NOT_READ:
            {
                if (uiOptions == TR_FLAG_RETURN_INTERRUPT_PACKET)
                {
                    AuthOsLock(lptsAuthTransport, 0);
                    if (!lptsAuthTransport->bPendingInterrupt )
                    {
                        lptsAuthTransport->bPendingInterrupt = TRUE;
                    }
                    AuthOsUnlock(lptsAuthTransport, 0);
                }
                else
                {
                    continue;
                }
                
                break;
            }

            case RP_READ_ON_INT:
            {
                if (uiOptions == TR_FLAG_RETURN_INTERRUPT_PACKET)
                {
                    continue;
                }
                else
                {
                    MEMSET((uint8*)lptsAuthBufferTX->pvVirtualBuffer, 0xff, uiBytesExpected);
                    AuthIoWriteReadSensor(lptsAuthIO,
                              (uint8 *)lptsAuthBufferTX->pvVirtualBuffer,
                              uiBytesExpected,
                              (uint8 *)lptsAuthTransport->pucOutputBuffer[uiIndex],
                              TRUE);
                    AuthOsSetEvent(lptsAuthContext);
                }
                break;
            }

            case RP_READ_ON_REQ:
            {
                if (uiOptions == TR_FLAG_RETURN_INTERRUPT_PACKET)
                {
                    AuthOsLock(lptsAuthTransport, 0);
                    if (!lptsAuthTransport->bPendingInterrupt )
                    {
                        lptsAuthTransport->bPendingInterrupt = TRUE;
                    }
                    AuthOsUnlock(lptsAuthTransport, 0);
                }
                else
                {
                   
                }
            
                break;
            }

            default:
                break;
        }
        
        uiIndex ++;
        uiIndex = uiIndex%MAX_BUFFER_NUMBER_IN_QUEUE;
    }

    return 0;
    
}


/** public functions **/

/*++
===============================================================================

Function AuthIsrInstall

The AuthIsrInstall() function performs all the steps required to install an
interrupt service routine on a system. If a shared memory is required, the
information about the shared memory buffer is written into
lptsAuthISR->sAuthBufferISR.

The memory location for DMA, etc, is contained in lptsAuthBuffer.

This function is passed the entire context to the driver. This is due to the
possibility of the ISR relying on a number of other contexts, such as DMA,
GPIO, I/O, memory buffers, etc.

Parameters
lpvContext (input)	        Pointer to the entire driver context.

Return Value                Returns a TRUE to indicate that the interrupt has
                            been installed, FALSE otherwise.

===============================================================================
--*/

BOOL
AuthIsrInstall (void    * lpvContext)
{
    uint32      uiGpioIrq              = 0;

    /* Cast the lpvContext pointer into the actual context pointer */
    lptsAUTHCONTEXT lptsAuthContext    = PCONTEXT(lpvContext);


    /*
    ** Validate the size of the structure(s) / context(s) that
    ** may be required for the ISR: Tailor this to your needs...
    */
    if ((lptsAuthContext->sAuthISR.uiStructSize      != sizeof(tsAUTHISR))    ||
        (lptsAuthContext->sAuthBufferTX.uiStructSize != sizeof(tsAUTHBUFFER)) ||
        (lptsAuthContext->sAuthBufferRX.uiStructSize != sizeof(tsAUTHBUFFER)) ||
        (lptsAuthContext->sAuthDmaTX.uiStructSize    != sizeof(tsAUTHDMA))    ||
        (lptsAuthContext->sAuthDmaRX.uiStructSize    != sizeof(tsAUTHDMA))    ||
        (lptsAuthContext->sAuthGPIO.uiStructSize     != sizeof(tsAUTHGPIO))   ||
        (lptsAuthContext->sAuthIO.uiStructSize       != sizeof(tsAUTHIO)))
    {
        return FALSE;
    }


    uiGpioIrq = (uint32)gpio_to_irq(SENSOR_GPIO_INT);
    DBPRINT((L("AuthIsrInstall: GPIO# is %d, IRQ# is %ld.\r\n"),
            SENSOR_GPIO_INT, uiGpioIrq));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
		irq_set_irq_type(uiGpioIrq, IRQF_TRIGGER_RISING);
#else
    set_irq_type(uiGpioIrq, IRQF_TRIGGER_RISING);
#endif
    
	if (request_irq(uiGpioIrq, GpioISR,
        IRQF_DISABLED, "GPIO interrupt", lpvContext))
    {
		DBGERROR((L("AuthIsrInstall: Error: request_irq() Failed (GPIO Interrupt %ld).\n"),
                uiGpioIrq));
		return FALSE;
	}

#if defined(SENSOR_GPIO_INT) && defined(UDM_WAKEUP_INTERRUPT)
    /* This is a workaround for setting GPIO_WAKEUPENABLE regisger.
    ** The Linux patch will not care about any settings from outside.
    ** Its suspend routine will clear the whole register, then set
    ** its own congigurations. And because the devices that have been
    ** registered last, are suspended first. The way that calls
    ** AuthGpioInit() in our suspend routine would not work too.
    **
    ** Instead of modifing the Linux patch, we follow its rules to
    ** call the set_irq_wake() routine to enable power management
    ** wakeup.
    */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)  
		irq_set_irq_wake(uiGpioIrq, 1);
#else  
    set_irq_wake(uiGpioIrq, 1);
#endif    
    lptsAuthISR->bTouchToWake = TRUE;
#endif

    /* This is a workaround for AuthIsrUninstall(lptsAUTHISR lptsAuthISR)
    ** And it is also required when bottom half is work queue.
    */
    glpvContext = lpvContext;

    g_lptsGpioRxEvent = (struct semaphore *)kmalloc(sizeof(struct semaphore), GFP_ATOMIC);
    if (!g_lptsGpioRxEvent)
    {
        DBGERROR((L("AuthIsrInstall: Error: Event Memory Allocation Failed.\r\n")));
        return FALSE;
    }
    
	sema_init(g_lptsGpioRxEvent, 0);  /* init event */

	g_bTeminateGpioRxThread = FALSE; /*will be set TRUE when unistalling isr to terminate the thread*/

/* FUJITSU:2012.09.10 FP_DRIVER Mod Start */
    // kernel_thread(GpioIST, lpvContext, 0);
    g_thread = kthread_run(GpioIST, lpvContext, "authisr");
    if (IS_ERR(g_thread))
    {
        g_thread = NULL;
        return FALSE;
    }
/* FUJITSU:2012.09.10 FP_DRIVER Mod End */

    return TRUE;
}


/*++
===============================================================================

Function AuthIsrUninstall

The AuthIsrUninstall() function performs all the steps required to remove an
interrupt service routine from a system. If a shared memory was allocated,
the shared memory buffer is freed.

Parameters
lptsAuthISR (input)	        Pointer to the ISR context.

Return Value                Returns a TRUE to indicate that the interrupt has
                            been uninstalled, FALSE otherwise.

===============================================================================
--*/

BOOL
AuthIsrUninstall (lptsAUTHISR 		lptsAuthISR)
{
    uint32      uiGpioIrq              = 0;

    /* Wake up and terminate the thread */
	g_bTeminateGpioRxThread = TRUE;
	if (NULL != g_lptsGpioRxEvent)
    {
        up(g_lptsGpioRxEvent);

        /* Wait until the GpioRxThread quit. */
        while (g_bTeminateGpioRxThread)
        {
            /*give up CPU to let the data process thread
        		to set the quit flag*/
            AuthOsSleep(10);
        }

/* FUJITSU:2012.09.10 FP_DRIVER Add Start */
        kthread_stop(g_thread);
        g_thread = NULL;
/* FUJITSU:2012.09.10 FP_DRIVER Add End */

        /* Deallocate the event structures */
	    kfree(g_lptsGpioRxEvent);
	    g_lptsGpioRxEvent = NULL;
    }

    uiGpioIrq = (uint32)gpio_to_irq(SENSOR_GPIO_INT);

#if defined(SENSOR_GPIO_INT) && defined(UDM_WAKEUP_INTERRUPT)
    /* Enable/disable power management wakeup mode, which is
    ** disabled by default.  Enables and disables must match,
    ** just as they match for non-wakeup mode support.
    */
    if (TRUE == lptsAuthISR->bTouchToWake)
    {
        set_irq_wake(uiGpioIrq, 0);
        lptsAuthISR->bTouchToWake = FALSE;
    }
#endif

    free_irq(uiGpioIrq, glpvContext);

    glpvContext = NULL;
    
    return TRUE;
    
}

void AuthIsrReadOnRequest(void    * lpvContext, uint8* pucReadBuffer, uint32 uiLength)
{
    lptsAUTHTRANSPORT         lptsAuthTransport = NULL;        /* Pointer to the Transport context */
    lptsAUTHIO          lptsAuthIO = NULL;         /* Pointer to the IO context */
    lptsAUTHBUFFER      lptsAuthBufferTX = NULL;   /* Pointer to the TX buffer */
    
    /* Pointer to the Transport context... */
    lptsAuthTransport = PTRANSPORT(lpvContext);

    DBPRINT((L("AuthIsrReadOnRequest: is called to read %d bytes\r\n"), uiLength));

    if (NULL == lptsAuthTransport)
    {
        DBGERROR((L("AuthIsrSetupReceive: Error: Invalid context\r\n")));
        return;
    }

    /* Pointer to the IO context */
    lptsAuthIO = PIO(lpvContext);

    /* Pointer to the TX buffer */
    lptsAuthBufferTX = PBUFFERTX(lpvContext);

    MEMSET((uint8*)lptsAuthBufferTX->pvVirtualBuffer, 0xff, uiLength);
    MEMSET(pucReadBuffer, 0, uiLength);
    AuthIoWriteReadSensor(lptsAuthIO,
                              (uint8 *)lptsAuthBufferTX->pvVirtualBuffer,
                              uiLength,
                              pucReadBuffer,
                              TRUE
                              );

}

