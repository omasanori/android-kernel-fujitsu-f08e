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

   File:            authos.c

   Description: The prefix "AuthOs" represents the OS Platform Specific
                functions that need to be implemented for any given
                driver based on the AUTHTRANSPORT. A prototype implementation
                of these functions will be provided as a starting point
                for driver development. These functions have been
                separated from the AuthHal functions because they should
                be reusable for a given OS implementation, whereas the
                AuthHal functions are more dependent on the underlying
                hardware.

   Author:      Joe Tykowski

   ************************************************************************* */

/** include files **/
#include "hfiles.h"


/** local definitions **/

typedef struct _tsEventBlock {
    atomic_t event_count;
    wait_queue_head_t wait_queue;
} tsEventBlock;

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

Function AuthOsCreateEvent

Author:  Joe Tykowski

The AuthOsCreateEvent() function creates a event that can be used to block
simultaneous access to system resources.

Parameters

lpvContext (input)          Pointer to the driver context.

Return Value                An TR_RETURN value indicating no error or
                            specifying a particular error condition.  The value
                            TR_OK indicates that no error was encountered.
                            Negative values represent error conditions.

===============================================================================
--*/

int32
AuthOsCreateEvent (void *   lpvContext)
{
    lptsAUTHTRANSPORT    lptsAuthTransport = (lptsAUTHTRANSPORT)lpvContext;  /* pointer to Transport context */

    tsEventBlock *                lptsEvent          = NULL;          /* Pointer to event... */

    lptsAuthTransport->lpvEvent= (void*)vmalloc(sizeof(tsEventBlock));

    lptsEvent = (tsEventBlock*)lptsAuthTransport->lpvEvent;

    if (!lptsEvent)
    {
        DBGERROR((L("AuthOsCreateEvent: Error: Event Memory Allocation Failed.\n")));
        return TR_ERROR;
    }

    atomic_set(&lptsEvent->event_count, 0);
    init_waitqueue_head(&lptsEvent->wait_queue);

    return TR_OK;
}

/*
@lpvContext: context
@uiLockNumber: how many locks to create (only support 1 for now)
*/

int32
AuthOsCreateLock (void *   lpvContext, uint32 uiLockNumber)
{
    lptsAUTHTRANSPORT    lptsAuthTransport = (lptsAUTHTRANSPORT)lpvContext;  /* pointer to Transport context */
    lptsAuthTransport->pvLock = (void*)kmalloc(sizeof(spinlock_t), GFP_ATOMIC);
    spin_lock_init((spinlock_t*)lptsAuthTransport->pvLock);
    return TR_OK;
}

/*
@lpvContext: context
@uiLockNumber: delete all locks
*/
int32
AuthOsDeleteLock (void *   lpvContext)
{
    lptsAUTHTRANSPORT    lptsAuthTransport = (lptsAUTHTRANSPORT)lpvContext;  /* pointer to Transport context */
    spin_unlock((spinlock_t*)lptsAuthTransport->pvLock);
    kfree(lptsAuthTransport->pvLock);
    return TR_OK;
}

/*
@lpvContext: context
@uiLockIndex: which lock to lock (only one supported for now)
*/
int32
AuthOsLock (void *   lpvContext, uint32 uiLockIndex)
{
    lptsAUTHTRANSPORT    lptsAuthTransport = (lptsAUTHTRANSPORT)lpvContext;  /* pointer to Transport context */
    spin_lock((spinlock_t*)lptsAuthTransport->pvLock);
    return TR_OK;
}

/*
@lpvContext: context
@uiLockIndex: which lock to unlock (only one supported for now)
*/
int32
AuthOsUnlock (void *   lpvContext, uint32 uiLockIndex)
{
    lptsAUTHTRANSPORT    lptsAuthTransport = (lptsAUTHTRANSPORT)lpvContext;  /* pointer to Transport context */
    spin_unlock((spinlock_t*)lptsAuthTransport->pvLock);
    return TR_OK; 
}


/*++
===============================================================================

Function AuthOsWaitEvent

Author:  Joe Tykowski

The AuthOsWaitEvent () function returns when the AUTHTRANSPORT event is acquired or
the time specified by uiMilliseconds has elapsed. The return value will
indicate whether the event was obtained or not.

Parameters

lpvContext (input)          Pointer to the driver context.

uiMilliseconds (input)      Specifies the maximum time, in milliseconds, to
                            wait before the event becomes available. If
                            uiMilliseconds is set to ((uint32) -1), the maximum
                            time is infinite, and the function won't return
                            unless there is data available.

Return Value                An TR_RETURN value indicating no error or
                            specifying a particular error condition.  The value
                            TR_OK indicates that no error was encountered,
                            and the Mutex was acquired. TR_ERROR indicates
                            that the Mutex was not acquired after waiting for
                            uiMilliseconds. Negative values represent error
                            conditions.

===============================================================================
--*/

int32
AuthOsWaitEvent (void *     lpvContext,
           uint32       uiMilliseconds)
{
    lptsAUTHTRANSPORT    lptsAuthTransport = (lptsAUTHTRANSPORT)lpvContext;  /* pointer to Transport context */
    tsEventBlock*                lptsEvent          = NULL;          /* Pointer to event... */

    lptsEvent = (tsEventBlock*)lptsAuthTransport->lpvEvent;

    DBPRINT((L("AuthOsWaitEvent: called with %ms\n"), uiMilliseconds));

    if (wait_event_interruptible_timeout(lptsEvent->wait_queue, 
                                                      atomic_add_unless( &lptsEvent->event_count, -1, 0), 
                                                      msecs_to_jiffies(uiMilliseconds) )> 0)
    {
        return TR_OK;
    }
    else
    {
        return TR_ERROR;
    }
}


/*++
===============================================================================

Function AuthOsSetEvent

Author:  Joe Tykowski

The AuthOsSetEvent () functionThe AuthOsSetEvent() function sets the event
and unblocks any thread that might be waiting on it..

Parameters

lpvContext (input)          Pointer to the driver context.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void
AuthOsSetEvent (void *  lpvContext)
{
    lptsAUTHTRANSPORT    lptsAuthTransport = (lptsAUTHTRANSPORT)lpvContext;  /* pointer to Transport context */
    tsEventBlock*    lptsEvent          = NULL;          /* Pointer to event... */

    lptsEvent = (tsEventBlock*)lptsAuthTransport->lpvEvent;

    if (NULL != lptsEvent)
    {
        atomic_inc(&lptsEvent->event_count);
        wake_up_interruptible( &lptsEvent->wait_queue);
    }
}


/*++
===============================================================================

Function AuthOsCloseEvent

Author:  Joe Tykowski

The AuthOsCloseEvent() function releases any resources associated with
creating the event for the AUTHTRANSPORT.

Parameters

lpvContext (input)          Pointer to the driver context.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void
AuthOsCloseEvent (void *    lpvContext)
{
    lptsAUTHTRANSPORT    lptsAuthTransport = (lptsAUTHTRANSPORT)lpvContext;  /* pointer to Transport context */

    if (lptsAuthTransport->lpvEvent)
    {
        vfree(lptsAuthTransport->lpvEvent);
        lptsAuthTransport->lpvEvent = NULL;
    }

}


/*++
===============================================================================

Function AuthOsSleep

Author:  Joe Tykowski

The AuthOsSleep() function is used to delay at least uiMilliseconds before
returning control to an application. This is only used to implement the
correct timing when an application is power cycling the sensor. It is not
used by the other Transport components because they could be called from within
an interrupt service routine.

Parameters
uiMilliseconds (input)      Specifies the maximum time, in milliseconds, to
                            wait before returning. A wait time of zero
                            returns immediately.

Return Value                Void, this function does not return any value.

===============================================================================
--*/

void
AuthOsSleep (uint32     uiMilliseconds)
{
    /* See if we need to delay... */
    if ( uiMilliseconds )
    {
        /* DBPRINT((L("Sleep %d ms. \n"), uiMilliseconds)); */
        msleep(uiMilliseconds);
    }
}


int32 AuthOsOutputMouseEvent(void * lpvContext, tsTrMouseEvent *pstMouseEvent)
{
    lptsAUTHTRANSPORT     lptsAuthTransport;        /* Pointer to the Transport context */

    lptsAUTHAES     lptsAuthAes;

    struct input_dev *ptsNavDevice;

    /* Point to the Transport context... */
    lptsAuthTransport = PTRANSPORT(lpvContext);

    lptsAuthAes = PAES(lpvContext);

    ptsNavDevice = (struct input_dev *)lptsAuthAes->pvUser;
    
    input_report_rel(ptsNavDevice, REL_X, pstMouseEvent->iDx);
    input_report_rel(ptsNavDevice, REL_Y, pstMouseEvent->iDy);

    input_sync(ptsNavDevice);

    return TR_OK;
    
}

int32 AuthOsOutputKeyEvent(void * lpvContext, tsTrKeyEvent *pstKeyEvent)
{
    lptsAUTHTRANSPORT     lptsAuthTransport;        /* Pointer to the Transport context */

    lptsAUTHAES     lptsAuthAes;

    struct input_dev *ptsNavDevice;

    //printk("driver: AuthOsOutputKeyEvent is called\n ");

    /* Point to the Transport context... */
    lptsAuthTransport = PTRANSPORT(lpvContext);

    lptsAuthAes = PAES(lpvContext);

    ptsNavDevice = (struct input_dev *)lptsAuthAes->pvUser;

    //printk("driver: AuthOsOutputKeyEvent: pstKeyEvent->uiKeyCode=%d, pstKeyEvent->uiKeyEvent=%d\n ", pstKeyEvent->uiKeyCode, pstKeyEvent->uiKeyEvent);
    
    input_report_key(ptsNavDevice, pstKeyEvent->uiKeyCode, pstKeyEvent->uiKeyEvent);

    input_sync(ptsNavDevice);

    return TR_OK;
    
}


