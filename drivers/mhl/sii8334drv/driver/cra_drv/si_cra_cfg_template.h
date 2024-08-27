
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
//!file     si_cra_cfg.h
//!brief    Silicon Image Starter Kit CRA configuration data.
//
// No part of this work may be reproduced, modified, distributed,
// transmitted, transcribed, or translated into any language or computer
// format, in any form or by any means without written permission of
// Silicon Image, Inc., 1140 East Arques Avenue, Sunnyvale, California 94085
//
// Copyright 2008-2011, Silicon Image, Inc.  All rights reserved.
//***************************************************************************/

#ifndef __SI_CRA_CFG_H__
#define __SI_CRA_CFG_H__

typedef enum _deviceBusTypes_t
{
    // The following four must remain together because they are used
    // as bus indices for the CraWriteBlockI2c and CraReadBlockI2c functions
    DEV_I2C_0,          // Main I2C bus
    DEV_I2C_1,          // Separate I2C bus
    DEV_I2C_2,          // Separate I2C bus
    DEV_I2C_3,          // Separate I2C bus

    // The following four must remain together because they are used
    // as bus indices for the CraWriteBlockI2c and CraReadBlockI2c functions
    // with the formula offsetBusTypeX - DEV_I2C_OFFSET == DEV_I2C_x
    DEV_I2C_OFFSET,     // Main I2C bus with register offset
    DEV_I2C_1_OFFSET,   // Separate I2C bus
    DEV_I2C_2_OFFSET,   // Separate I2C bus
    DEV_I2C_3_OFFSET,   // Separate I2C bus

    DEV_DDC_0,          // DDC bus for TX 0
    DEV_DDC_1,          // DDC bus for TX 1

    DEV_PARALLEL,       // Parallel interface
} deviceBusTypes_t;

// Actual I2C page addresses for the various devices

enum
{
    DEV_PAGE_PP_0       = (0xB0),
    DEV_PAGE_PP_2       = (0x66),
    DEV_PAGE_IPV        = (0xFA),
    DEV_PAGE_PP_4       = (0x68),
    DEV_PAGE_PP_5       = (0x50),
    DEV_PAGE_PP_6       = (0x52),
    DEV_PAGE_PP_7       = (0x54),
    DEV_PAGE_PP_8       = (0xC0),
    DEV_PAGE_PP_9       = (0xE0),
    DEV_PAGE_PP_A       = (0x64),
    DEV_PAGE_PP_B       = (0x90),
    DEV_PAGE_PP_C       = (0xE6),
    DEV_PAGE_CBUS_1     = (0xE6),
    DEV_PAGE_HEAC       = (0xD0),
    DEV_PAGE_CPI_TX     = (0xC8),
    DEV_PAGE_CPI_RX     = (0xC0),
    DEV_PAGE_TPI_0      = (0x72),
    DEV_PAGE_TX_L0_0    = (0x72),
    DEV_PAGE_TX_L1_0    = (0x74),
    DEV_PAGE_TX_2_0     = (0x76),
    DEV_PAGE_TPI_1      = (0x7A),
    DEV_PAGE_TX_L0_1    = (0x7A),
    DEV_PAGE_TX_L1_1    = (0x7C),
    DEV_PAGE_TX_2_1     = (0x7E),
    DEV_PAGE_OSD        = (0xF0),
    DEV_PAGE_AUDIO      = (0xF8),

    DEV_PAGE_DDC_EDID   = (0xA0),
    DEV_PAGE_DDC_SEGM   = (0x60),
};

enum        // Index into pageConfig_t array (shifted left by 8)
{
    PP_PAGE             = 0x0000,
    PP_PAGE_1           = 0x0100,   // Unused
    PP_PAGE_2           = 0x0200,
    PP_PAGE_IPV           = 0x0300,
    PP_PAGE_4           = 0x0400,
    PP_PAGE_5           = 0x0500,
    PP_PAGE_PAUTH1      = 0x0500,   // same as PP_PAGE_5
    PP_PAGE_6           = 0x0600,
    PP_PAGE_PAUTH2      = 0x0600,   // same as PP_PAGE_6
    PP_PAGE_7           = 0x0700,
    PP_PAGE_PAUTH3      = 0x0700,   // same as PP_PAGE_7
    CPI_PAGE            = 0x0800,   // CPI
    PP_PAGE_9           = 0x0900,   // hardware page 9
    PP_PAGE_EDID        = 0x0900,   // Same as PP_PAGE_9
    PP_PAGE_GPIO        = 0x0900,   // Same as PP_PAGE_9
    PP_PAGE_A           = 0x0A00,
    PP_PAGE_B           = 0x0B00,
    CBUS_PAGE           = 0x0C00,   // CBUS
    PP_PAGE_HEAC        = 0x0D00,
    PP_PAGE_OSD         = 0x0E00,
    PP_PAGE_AUDIO       = 0x0F00,
    TX_PAGE_TPI         = 0x1000,   // TPI
    TX_PAGE_L0          = 0x1100,   // TX Legacy page 0
    TX_PAGE_L1          = 0x1200,   // TX Legacy page 1
    TX_PAGE_2           = 0x1300,   // TX page 2
    TX_PAGE_DDC_EDID    = 0x1400,   // TX DDC EDID
    TX_PAGE_DDC_SEGM    = 0x1500,   // TX DDC EDID Segment address
};

#define SII_CRA_MAX_DEVICE_INSTANCES    2   // Maximum size of instance dimension of address descriptor array
#define SII_CRA_DEVICE_PAGE_COUNT       22  // Number of entries in pageConfig_t array

// Index to this array is the virtual page number in the MSB of the REG_xxx address values
// Indexed with siiRegPageIndex_t value shifted right 8 bits
// DEV_PAGE values must correspond to the order specified in the siiRegPageIndex_t array

typedef struct pageConfig
{
    deviceAddrTypes_t   busType;    // I2C, Parallel
    int_t               address;    // I2C DEV ID, parallel mem offset
} pageConfig_t;

#endif  // __SI_CRA_CFG_H__
