/*
 * cyttsp4_core.h
 * Cypress TrueTouch(TM) Standard Product V4 Core driver module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modifed by: Cypress Semiconductor to add touch settings
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_CORE_H
#define _LINUX_CYTTSP4_CORE_H

#include <linux/stringify.h>

#define CYTTSP4_CORE_NAME "cyttsp4_core"

#define CY_DRIVER_NAME TTDA
#define CY_DRIVER_MAJOR 02
#define CY_DRIVER_MINOR 03

#define CY_DRIVER_REVCTRL 467394

#define CY_DRIVER_VERSION		    \
__stringify(CY_DRIVER_NAME)		    \
"." __stringify(CY_DRIVER_MAJOR)	    \
"." __stringify(CY_DRIVER_MINOR)	    \
"." __stringify(CY_DRIVER_REVCTRL)

#define CY_DRIVER_DATE "20130429"	/* YYYYMMDD */

/* x-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_X_MASK 0x7F

/* y-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_Y_MASK 0x7F

/* x-axis, 0:origin is on left side of panel, 1: right */
#define CY_PCFG_ORIGIN_X_MASK 0x80

/* y-axis, 0:origin is on top side of panel, 1: bottom */
#define CY_PCFG_ORIGIN_Y_MASK 0x80

#define CY_TOUCH_SETTINGS_MAX 32
#define CY_TOUCH_SETTINGS_PARAM_REGS 6

enum cyttsp4_core_platform_flags {
	CY_CORE_FLAG_NONE = 0x00,
	CY_CORE_FLAG_WAKE_ON_GESTURE = 0x01,
};

enum cyttsp4_core_platform_easy_wakeup_gesture {
	CY_CORE_EWG_NONE = 0x00,
	CY_CORE_EWG_TAP_TAP = 0x01,
	CY_CORE_EWG_TWO_FINGER_SLIDE = 0x02,
	CY_CORE_EWG_RESERVED = 0x03,
	CY_CORE_EWG_WAKE_ON_INT_FROM_HOST = 0xFF,
};

enum cyttsp4_loader_platform_flags {
	CY_LOADER_FLAG_NONE = 0x00,
	CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE = 0x01,
	/* Use CONFIG_VER field in TT_CFG to decide TT_CFG update */
	CY_LOADER_FLAG_CHECK_TTCONFIG_VERSION = 0x02,
};

struct touch_settings {
	const uint8_t *data;
	uint32_t size;
	uint8_t tag;
} __packed;

struct cyttsp4_touch_firmware {
	const uint8_t *img;
	uint32_t size;
	const uint8_t *ver;
	uint8_t vsize;
} __packed;

struct cyttsp4_touch_config {
	struct touch_settings *param_regs;
	struct touch_settings *param_size;
	const uint8_t *fw_ver;
	uint8_t fw_vsize;
};

struct cyttsp4_loader_platform_data {
	struct cyttsp4_touch_firmware *fw;
	struct cyttsp4_touch_config *ttconfig;
	u32 flags;
} __packed;

typedef int (*cyttsp4_platform_read) (struct device *dev, u16 addr,
	void *buf, int size);

/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add start */
struct cyttsp4_touch_firmware_info {
	u32 firmware_version;
	u16 config_crc;
	u8  panel_maker_code;
	u8  lcd_maker_code;
	u8  project_id;					/* HMW:0x0B */	/* FUJITSU:2013-05-28 HMW_TOUCH_FWVER add */
};

struct cyttsp4_touch_platform_info {
	u8  model_type;
	u8  trial_type;
	u8  lcd_maker_id;
};

#define CY_INIT_FLAG_SET_CONFIG_PARAMS 0x01
#define CY_INIT_FLAG_SET_PDATA_FLAGS   0x02
/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add end */
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add start */
enum cyttsp4_core_support_type {
	 CY_CORE_SUPPORT_PRESS		= (int)0
	,CY_CORE_SUPPORT_HOVER
};
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add end */

struct cyttsp4_core_platform_data {
	int irq_gpio;
	int rst_gpio;
	int level_irq_udelay;
	int max_xfer_len;
	int (*xres)(struct cyttsp4_core_platform_data *pdata,
		struct device *dev);
	int (*init)(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev);
	int (*power)(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq);
	int (*detect)(struct cyttsp4_core_platform_data *pdata,
		struct device *dev, cyttsp4_platform_read read);
	int (*irq_stat)(struct cyttsp4_core_platform_data *pdata,
		struct device *dev);
/* FUJITSU:2013-04-05 HMW_TTDA_PRESS add start */
	int (*mt_setting)(struct cyttsp4_core_platform_data *pdata,
		u8 setting, unsigned short flags, struct device *dev);
/* FUJITSU:2013-04-05 HMW_TTDA_PRESS add end */
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add start */
	int (*board_is_support)(struct cyttsp4_core_platform_data *pdata, int check_type, struct device *dev);
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add end */
/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add start */
	int (*pdata_init)(struct cyttsp4_core_platform_data *pdata,
		struct cyttsp4_touch_firmware_info *fi, int flags);
/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add end */
/* FUJITSU:2013-04-08 HMW_TTDA_CONFIG_UPDATE add start */
	int (*get_platform_info)(struct cyttsp4_core_platform_data *pdata, struct cyttsp4_touch_platform_info *pi);
/* FUJITSU:2013-04-08 HMW_TTDA_CONFIG_UPDATE add end */
	struct touch_settings *sett[CY_TOUCH_SETTINGS_MAX];
	struct cyttsp4_loader_platform_data *loader_pdata;
	u32 flags;
	u8 easy_wakeup_gesture;
};

#ifdef VERBOSE_DEBUG
extern void cyttsp4_pr_buf(struct device *dev, u8 *pr_buf, u8 *dptr, int size,
			   const char *data_name);
#else
#define cyttsp4_pr_buf(a, b, c, d, e) do { } while (0)
#endif

/* FUJITSU:2013-03-25 HMW_TTDA_MODIFY add start */
#define CY_MODEL_PTN1            0x01		/* T131HM     */
#define CY_MODEL_PTN2            0x02		/* T131SP     */	/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
#define CY_MODEL_PTN3            0x03		/* T132AR     */	/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
#define CY_MODEL_PTN4            0x04		/* T131LU     */
#define CY_MODEL_PTN5            0x05		/* T131CA     */
#define CY_MODEL_PTN6            0x06		/* T131TH     */
#define CY_LCDID_TYP1            1			/* S(SHARP)   */
#define CY_LCDID_TYP0            0			/* J(JDI)     */
#define CY_VENDOR_ID1            1			/* J(JDI)     */
#define CY_VENDOR_ID2            2			/* W(WINTEK)  */
#define CY_VENDOR_ID3            3			/* K(KYOCERA) */

#define CY_FIRMWARE_BASE_V1_FJ1_NPS   0x0005E767
#define CY_FIRMWARE_HOVER_V2_B15      0x00069958
#define CY_FIRMWARE_PRESS_V0_PRT      0x00056473
/* FUJITSU:2013-03-25 HMW_TTDA_MODIFY add end */

#endif /* _LINUX_CYTTSP4_CORE_H */
