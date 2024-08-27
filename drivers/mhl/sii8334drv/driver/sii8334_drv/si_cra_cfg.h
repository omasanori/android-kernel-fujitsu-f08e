








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
//!brief    SiI 8334 CRA configuration data.
//
// Copyright 2008-2010, Silicon Image, Inc.  All rights reserved.
//***************************************************************************/

#ifndef __SI_CRA_CFG_H__
#define __SI_CRA_CFG_H__

typedef enum _deviceAddrTypes_t
{
    // The following four should remain together because they are used
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
} deviceAddrTypes_t;

// Actual I2C page addresses for the various devices

enum
{
    DEV_PAGE_TPI_0      = (0x72),
    DEV_PAGE_TX_L0_0    = (0x72),
    DEV_PAGE_TPI_1      = (0x76),
    DEV_PAGE_TX_L0_1    = (0x76),
    DEV_PAGE_TX_L1_0    = (0x7A),
    DEV_PAGE_TX_L1_1    = (0x7E),
    DEV_PAGE_TX_2_0     = (0x92),
    DEV_PAGE_TX_2_1     = (0x96),
	DEV_PAGE_TX_3_0		= (0x9A),
	DEV_PAGE_TX_3_1		= (0x9E),

    DEV_PAGE_CBUS		= (0xC8),

    DEV_PAGE_DDC_EDID   = (0xA0),
    DEV_PAGE_DDC_SEGM   = (0x60),
};

enum        // Index into pageConfig_t array (shifted left by 8)
{
    TX_PAGE_TPI         = 0x0000,   // TPI
    TX_PAGE_L0          = 0x0100,   // TX Legacy page 0
    TX_PAGE_L1          = 0x0200,   // TX Legacy page 1
    TX_PAGE_2           = 0x0300,   // TX page 2
    TX_PAGE_3           = 0x0400,   // TX page 3
    TX_PAGE_CBUS        = 0x0500,   // CBUS
    TX_PAGE_DDC_EDID    = 0x0600,   // TX DDC EDID
    TX_PAGE_DDC_SEGM    = 0x0700,   // TX DDC EDID Segment address
};

#define SII_CRA_MAX_DEVICE_INSTANCES    1   // Maximum size of instance dimension of address descriptor array
#define SII_CRA_DEVICE_PAGE_COUNT       8  // Number of entries in pageConfig_t array

// Index to this array is the virtual page number in the MSB of the REG_xxx address values
// Indexed with siiRegPageIndex_t value shifted right 8 bits
// DEV_PAGE values must correspond to the order specified in the siiRegPageIndex_t array

typedef struct pageConfig
{
    deviceAddrTypes_t   busType;    // I2C, Parallel
#ifdef __KERNEL__    //(
    uintptr_t           address;    // I2C DEV ID, parallel mem offset
#else //)(
	prefuint_t			address;
#endif //)
} pageConfig_t;

#endif  // __SI_CRA_CFG_H__
