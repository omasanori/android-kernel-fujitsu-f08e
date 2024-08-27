/*
 * Copyright (C) 2010 Motorola Mobility, Inc.
 * Modified by Cypress Semiconductor 2011-2012
 *    - increase touch_settings.size from uint8_t to uint32_t
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
/*
 *----------------------------------------------------------------------------
 * COPYRIGHT(C) FUJITSU LIMITED 2013
 *----------------------------------------------------------------------------
 */

/* Defines generic platform structures for touch drivers */
#ifndef _LINUX_TOUCH_PLATFORM_H
#define _LINUX_TOUCH_PLATFORM_H

#include <linux/types.h>

struct touch_settings {
	const uint8_t   *data;
	uint32_t         size;
	uint8_t         tag;
} __attribute__ ((packed));

struct touch_firmware {
	const uint8_t   *img;
	uint32_t        size;
	const uint8_t   *ver;
	uint8_t         vsize;
} __attribute__ ((packed));

struct touch_framework {
	const int16_t   *abs; /* FUJITSU:2013-02-19 mod */
	uint8_t         size;
	uint8_t         enable_vkeys;
} __attribute__ ((packed));

/* FUJITSU:2012-07-30 mod start */
struct touch_platform_data {
	struct touch_settings   *sett[256];
	struct touch_firmware   *fw;
	struct touch_framework  *frmwrk;

	uint8_t         addr[2];
	uint16_t        flags;

	int         (*hw_reset)(void);
	int         (*hw_recov)(int);
	int         (*irq_stat)(void);
	void        (*hw_init)(void);
	u8          (*get_rev)(void);
	void        (*check_firmver)(u32*,u16*,u8);		/* 13-1stMultiPanel Change Config */
/* 13-1stUpdate Firmware start */
	int         (*get_model)(void);
	int         (*get_lcdid)(void);
/* 13-1stUpdate Firmware end */
} __attribute__ ((packed));

/* 13-1stUpdate Firmware start */
#define CY_MODEL_PTN1            0x01		/* T131HM     */
#define CY_MODEL_PTN4            0x04		/* T131LU     */
#define CY_MODEL_PTN5            0x05		/* T131CA     */
#define CY_MODEL_PTN6            0x06		/* T131TH     */
#define CY_LCDID_TYP1            1			/* S(SHARP)   */
#define CY_LCDID_TYP0            0			/* J(JDI)     */
#define CY_VENDOR_ID1            1			/* J(JDI)     */
#define CY_VENDOR_ID2            2			/* W(WINTEK)  */
#define CY_VENDOR_ID3            3			/* K(KYOCERA) */
/* 13-1stUpdate Firmware end */
/* 13-1stMultiPanel Change Config start */
enum cyttsp4_flags {
	CY_FLAG_NONE = 0x00,
	CY_FLAG_HOVER_AVAIL = 0x04,
/* 13-1stBootup FWVer. start */
	CY_FLAG_FLIP = 0x08,
	CY_FLAG_INV_X = 0x10,
	CY_FLAG_INV_Y = 0x20,
/* 13-1stBootup FWVer. end */
	CY_FLAG_NOTFY_PRESS = 0x40,					/* 13-2ndHMWBootup2 add */
};

#define CY_FIRMWARE_BASE_V1_FJ1_NPS				0x0005E767
#define CY_FIRMWARE_BASE_V1_2_TH				0x0006685E
#define CY_FIRMWARE_NP_V1_5						0x0006BF91
#define CY_FIRMWARE_NP_V1_5_TH_LU				0x0006DF0C
/* 13-1st HOVER start */
#define CY_FIRMWARE_HOVER_V2					0x00067b2a
#define CY_FIRMWARE_HOVER_V2_B15				0x00069958
#define CY_FIRMWARE_HOVER_V5					0x0006abd9
#define CY_FIRMWARE_HOVER_V7					0x0006b2b3
#define CY_FIRMWARE_HOVER_V9					0x0006ba89
#define CY_FIRMWARE_HOVER_VA					0x0006C16D
#define CY_FIRMWARE_HOVER_VC					0x0006CEC2
#define CY_FIRMWARE_HOVER_VF					0x0006DBBA
#define CY_FIRMWARE_HOVER_V11					0x0006E7C8
#define CY_FIRMWARE_HOVER_V15					0x06FEB7
#define CY_FIRMWARE_HOVER_V19					0x071A48
/* 13-1st HOVER end */
#define CY_FIRMWARE_HMW_PROTOTYPE				0x00056473
/* 13-1stMultiPanel Change Config end */
/* FUJITSU:2012-07-30 mod mod */

#endif /* _LINUX_TOUCH_PLATFORM_H */
