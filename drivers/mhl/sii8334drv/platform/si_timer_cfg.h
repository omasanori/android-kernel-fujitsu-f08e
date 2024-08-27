
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
//!file     si_timer_cfg.h
//!brief    Silicon Image timer definitions header.
//
// No part of this work may be reproduced, modified, distributed,
// transmitted, transcribed, or translated into any language or computer
// format, in any form or by any means without written permission of
// Silicon Image, Inc., 1140 East Arques Avenue, Sunnyvale, California 94085
//
// Copyright 2008-2010,2011 Silicon Image, Inc.  All rights reserved.
//***************************************************************************/
// put debug channel information here
//------------------------------------------------------------------------------
// Array of timer values
//------------------------------------------------------------------------------
// Timers - Target system uses these timers
#define ELAPSED_TIMER               0xFF
#define ELAPSED_TIMER1              0xFE

typedef enum TimerId
{
     TIMER_0 = 0		// DO NOT USE - reserved for TimerWait()
    ,TIMER_COUNT			// MUST BE LAST!!!!
} timerId_t;

