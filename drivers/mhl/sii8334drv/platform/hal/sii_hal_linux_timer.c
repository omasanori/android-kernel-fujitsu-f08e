








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
 * @file sii_hal_linux_timer.c
 *
 * @brief Linux implementation of timer support used by Silicon Image
 *        MHL devices.
 *
 * $Author: Dave Canfield
 * $Rev: $
 * $Date: Feb. 4, 2011
 *
 *****************************************************************************/

#define SII_HAL_LINUX_TIMER_C

/***** #include statements ***************************************************/
#include "sii_hal.h"
#include "sii_hal_priv.h"
#include "si_timer_cfg.h"
/***** local macro definitions ***********************************************/

/** Convert a value specified in milliseconds to nanoseconds */
#define MSEC_TO_NSEC(x)	(x * 1000000UL)


/***** local type definitions ************************************************/


/***** local variable declarations *******************************************/

static struct hrtimer	hiResTimer;

static ktime_t		timerPeriod;
static DEFINE_SPINLOCK(timerLock);
static bool timerRunning;



/*****************************************************************************
 * Array of timer values
 *****************************************************************************/
static uint16_t g_timerCounters[ TIMER_COUNT ];

static uint16_t g_timerElapsed;
static uint16_t g_elapsedTick;
static uint16_t g_timerElapsedGranularity;

static uint16_t g_timerElapsed1;
static uint16_t g_elapsedTick1;
static uint16_t g_timerElapsedGranularity1;


/***** local function prototypes *********************************************/


/***** global variable declarations *******************************************/


/***** local functions *******************************************************/

/*****************************************************************************/
/*
 *  @brief Timer callback handler.
 *
 *  @param[in]	timer	Pointer to the timer structure responsible for this
 *  					function being called.
 *
 *  @return     Returns HRTIMER_RESTART if there are still counters in use.
 *  			Otherwise HRTIMER_NORESTART is returned to stop the timer.
 *
 *****************************************************************************/
static enum hrtimer_restart TimerTickHandler(struct hrtimer *timer)
{
	uint16_t				countersDone = 0;
    uint8_t					i;
    enum hrtimer_restart	restartFlag;
    unsigned long 			flags;


    spin_lock_irqsave(&timerLock, flags);

    //decrement all active timers in array
    for ( i = 0; i < TIMER_COUNT; i++ )
    {
        if ( g_timerCounters[ i ] > 0 )
        {
            g_timerCounters[ i ]--;
        }
        countersDone |= g_timerCounters[ i ];
    }

    g_elapsedTick++;
    if ( g_elapsedTick == g_timerElapsedGranularity )
    {
        g_timerElapsed++;
        g_elapsedTick = 0;
    }
    g_elapsedTick1++;
    if ( g_elapsedTick1 == g_timerElapsedGranularity1 )
    {
        g_timerElapsed1++;
        g_elapsedTick1 = 0;
    }

    // Don't restart the timer if no one is requesting timer service.
    if(countersDone == 0 &&
    	g_timerElapsedGranularity == 0 &&
    	g_timerElapsedGranularity1 == 0)
    {
    	timerRunning = false;
    	restartFlag = HRTIMER_NORESTART;
    }
    else
    {
    	timerPeriod = ktime_set(0, MSEC_TO_NSEC(1));
    	hrtimer_forward(&hiResTimer, hiResTimer.base->get_time(), timerPeriod);
    	restartFlag = HRTIMER_RESTART;
    }

    spin_unlock_irqrestore(&timerLock, flags);

    return restartFlag;
}




/***** public functions ******************************************************/


/*****************************************************************************/
/**
 * @brief Initialize timer support.
 *
 *****************************************************************************/
void HalTimerInit(void)
{
    uint8_t		i;

    spin_lock_init(&timerLock);

    //initializer timer counters in array
    for ( i = 0; i < TIMER_COUNT; i++ )
    {
        g_timerCounters[ i ] = 0;
    }
    g_timerElapsed  = 0;
    g_timerElapsed1 = 0;
    g_elapsedTick   = 0;
    g_elapsedTick1  = 0;
    g_timerElapsedGranularity   = 0;
    g_timerElapsedGranularity1  = 0;

    // Initialize and start a periodic 1ms. timer.
    hrtimer_init(&hiResTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hiResTimer.function = TimerTickHandler;

    timerRunning = false;
}



/*****************************************************************************/
/**
 * @brief Terminate HAL timer support.
 *
 *****************************************************************************/
void HalTimerTerm(void)
{
	hrtimer_cancel(&hiResTimer);
	timerRunning = false;
}



/*****************************************************************************/
/**
 * @brief Set a timer counter.
 *
 *****************************************************************************/
void HalTimerSet( uint8_t index, uint16_t m_sec )
{
    unsigned long flags;

    // prevent any races with the timer tick handler
    spin_lock_irqsave(&timerLock, flags);

    switch ( index )
    {
    case ELAPSED_TIMER:
        g_timerElapsedGranularity = m_sec;
        g_timerElapsed = 0;
        g_elapsedTick = 0;
        break;

    case ELAPSED_TIMER1:
        g_timerElapsedGranularity1 = m_sec;
        g_timerElapsed1 = 0;
        g_elapsedTick1 = 0;
        break;

	default:
		if (index < TIMER_COUNT)
		{
	        g_timerCounters[ index ] = m_sec;
		}
        break;

    }

    spin_unlock_irqrestore(&timerLock, flags);

    if(!timerRunning)
    {
    	timerPeriod = ktime_set(0, MSEC_TO_NSEC(1));
    	hrtimer_start(&hiResTimer, timerPeriod, HRTIMER_MODE_REL);
    	timerRunning = true;
    }
}



/*****************************************************************************/
/**
 * @brief Wait for the specified number of milliseconds to elapse.
 *
 *****************************************************************************/
void HalTimerWait(uint16_t m_sec)
{
	unsigned long	time_usec = m_sec * 1000;
//    printk("SCHEULDING USLEEP_RANGE [%x]\n",(int)time_usec);

	usleep_range(time_usec, time_usec);
}



/*****************************************************************************/
/**
 * @brief Check if the specified timer has expired.
 *
 *****************************************************************************/
uint8_t HalTimerExpired (uint8_t timerIndex)
{
	uint16_t		remainingTime;
    unsigned long flags;


	if(timerIndex >= TIMER_COUNT)
	{
		return 0;
	}

    // prevent any races with the timer tick handler
    spin_lock_irqsave(&timerLock, flags);

    remainingTime = g_timerCounters[timerIndex];

    spin_unlock_irqrestore(&timerLock, flags);

    return(remainingTime == 0);
}



/*****************************************************************************/
/**
 * @brief Read the current value of one of the elapsed timer counters.
 *
 *****************************************************************************/
uint16_t HalTimerElapsed(uint8_t elapsedTimerIndex)
{
    uint16_t		elapsedTime = 0;

    unsigned long flags;

    // prevent any races with the timer tick handler
    spin_lock_irqsave(&timerLock, flags);

    if (elapsedTimerIndex == ELAPSED_TIMER)
    {
        elapsedTime = g_timerElapsed;
    }
    else if (elapsedTimerIndex == ELAPSED_TIMER1)
    {
        elapsedTime = g_timerElapsed1;
    }

    spin_unlock_irqrestore(&timerLock, flags);

    return elapsedTime;
}


