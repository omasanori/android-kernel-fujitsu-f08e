/*
 * cyttsp4_regs.h
 * Cypress TrueTouch(TM) Standard Product V4 registers.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modified by: Cypress Semiconductor to add test modes and commands
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

#ifndef _CYTTSP4_REGS_H
#define _CYTTSP4_REGS_H

#define CY_FW_FILE_NAME			"cyttsp4_fw.bin"

#define CY_DEFAULT_ADAP_MAX_XFER	512
#define CY_ADAP_MIN_XFER		140

#define CY_MAX_PRBUF_SIZE		PIPE_BUF
#define CY_PR_TRUNCATED			" truncated..."

#define CY_DEFAULT_CORE_ID		"main_ttsp_core"
#define CY_MAX_NUM_CORE_DEVS		5

#define CY_TMA1036_MAX_TCH		0x0E
#define CY_TMA4XX_MAX_TCH		0x1E

#define IS_BOOTLOADER(hst_mode, reset_detect) \
		((hst_mode) & 0x01 || (reset_detect) != 0)
#define IS_BOOTLOADER_IDLE(hst_mode, reset_detect) \
		((hst_mode) & 0x01 && (reset_detect) & 0x01)

#define GET_HSTMODE(reg)		((reg & 0x70) >> 4)
#define GET_TOGGLE(reg)			((reg & 0x80) >> 7)

#define IS_LITTLEENDIAN(reg)		((reg & 0x01) == 1)
#define GET_PANELID(reg)		(reg & 0x07)

#define HI_BYTE(x)			(u8)(((x) >> 8) & 0xFF)
#define LO_BYTE(x)			(u8)((x) & 0xFF)

#define CY_REG_BASE			0x00
#define CY_NUM_REVCTRL			8
#define CY_NUM_DDATA			32
#define CY_NUM_MDATA			64

#define CY_REG_CAT_CMD			2
#define CY_CMD_COMPLETE_MASK		(1 << 6)
#define CY_CMD_MASK			0x3F

#define CY_TTCONFIG_VERSION_OFFSET	8
#define CY_TTCONFIG_VERSION_SIZE	2
#define CY_TTCONFIG_VERSION_ROW		0

#define CY_CONFIG_LENGTH_INFO_OFFSET	0
#define CY_CONFIG_LENGTH_INFO_SIZE	4
#define CY_CONFIG_LENGTH_OFFSET		0
#define CY_CONFIG_LENGTH_SIZE		2
#define CY_CONFIG_MAXLENGTH_OFFSET	2
#define CY_CONFIG_MAXLENGTH_SIZE	2

enum cyttsp4_ic_ebid {
	CY_TCH_PARM_EBID,
	CY_MDATA_EBID,
	CY_DDATA_EBID,
	CY_EBID_NUM,
};

/* touch record system information offset masks and shifts */
#define CY_BYTE_OFS_MASK		0x1F
#define CY_BOFS_MASK			0xE0
#define CY_BOFS_SHIFT			5

/* helpers */
#define GET_NUM_TOUCH_RECORDS(x)	((x) & 0x1F)
#define IS_LARGE_AREA(x)		((x) & 0x20)
#define IS_BAD_PKT(x)			((x) & 0x20)
#define IS_TTSP_VER_GE(p, maj, min) \
		((p)->si_ptrs.cydata == NULL ? \
		0 : \
		((p)->si_ptrs.cydata->ttsp_ver_major < (maj) ? \
			0 : \
			((p)->si_ptrs.cydata->ttsp_ver_minor < (min) ? \
				0 : \
				1)))

/* Timeout in ms. */
#define CY_COMMAND_COMPLETE_TIMEOUT	500
#define CY_CALIBRATE_COMPLETE_TIMEOUT	5000
#define CY_WATCHDOG_TIMEOUT		1000

/* drv_debug commands */
#define CY_DBG_SUSPEND			4
#define CY_DBG_RESUME			5
#define CY_DBG_SOFT_RESET		97
#define CY_DBG_RESET			98

/* scan types */
#define CY_SCAN_TYPE_GLOVE		0x8
#define CY_SCAN_TYPE_STYLUS		0x10
#define CY_SCAN_TYPE_PROXIMITY		0x40
#define CY_SCAN_TYPE_APA_MC		0x80

enum cyttsp4_hst_mode_bits {
	CY_HST_TOGGLE      = (1 << 7),
	CY_HST_MODE_CHANGE = (1 << 3),
	CY_HST_DEVICE_MODE = (7 << 4),
	CY_HST_OPERATE     = (0 << 4),
	CY_HST_SYSINFO     = (1 << 4),
	CY_HST_CAT         = (2 << 4),
	CY_HST_LOWPOW      = (1 << 2),
	CY_HST_SLEEP       = (1 << 1),
	CY_HST_RESET       = (1 << 0),
};

enum cyttsp_cmd_bits {
	CY_CMD_COMPLETE    = (1 << 6),
};

enum cyttsp4_cmd_cat {
	CY_CMD_CAT_NULL,
	CY_CMD_CAT_RESERVED_1,
	CY_CMD_CAT_GET_CFG_ROW_SZ,
	CY_CMD_CAT_READ_CFG_BLK,
	CY_CMD_CAT_WRITE_CFG_BLK,
	CY_CMD_CAT_RESERVED_2,
	CY_CMD_CAT_LOAD_SELF_TEST_DATA,
	CY_CMD_CAT_RUN_SELF_TEST,
	CY_CMD_CAT_GET_SELF_TEST_RESULT,
	CY_CMD_CAT_CALIBRATE_IDACS,
	CY_CMD_CAT_INIT_BASELINES,
	CY_CMD_CAT_EXEC_PANEL_SCAN,
	CY_CMD_CAT_RETRIEVE_PANEL_SCAN,
	CY_CMD_CAT_START_SENSOR_DATA_MODE,
	CY_CMD_CAT_STOP_SENSOR_DATA_MODE,
	CY_CMD_CAT_INT_PIN_MODE,
	CY_CMD_CAT_RETRIEVE_DATA_STRUCTURE,
	CY_CMD_CAT_VERIFY_CFG_BLK_CRC,
	CY_CMD_CAT_RESERVED_N,
};

enum cyttsp4_cmd_op {
	CY_CMD_OP_NULL,
	CY_CMD_OP_RESERVED_1,
	CY_CMD_OP_GET_PARAM,
	CY_CMD_OP_SET_PARAM,
	CY_CMD_OP_RESERVED_2,
	CY_CMD_OP_GET_CRC,
	CY_CMD_OP_WAIT_FOR_EVENT,
};

enum cyttsp4_cmd_status {
	CY_CMD_STATUS_SUCCESS,
	CY_CMD_STATUS_FAILURE,
};

/* Operational Mode Command Sizes */
/* NULL Command */
#define CY_CMD_OP_NULL_CMD_SZ			1
#define CY_CMD_OP_NULL_RET_SZ			0
/* Get Parameter */
#define CY_CMD_OP_GET_PARAM_CMD_SZ		2
#define CY_CMD_OP_GET_PARAM_RET_SZ		6
/* Set Parameter */
#define CY_CMD_OP_SET_PARAM_CMD_SZ		7
#define CY_CMD_OP_SET_PARAM_RET_SZ		2
/* Get Config Block CRC */
#define CY_CMD_OP_GET_CFG_BLK_CRC_CMD_SZ	2
#define CY_CMD_OP_GET_CFG_BLK_CRC_RET_SZ	3
/* Wait For Event */
#define CY_CMD_OP_WAIT_FOR_EVENT_CMD_SZ		2

/* CaT Mode Command Sizes */
/* NULL Command */
#define CY_CMD_CAT_NULL_CMD_SZ			1
#define CY_CMD_CAT_NULL_RET_SZ			0
/* Get Config Row Size */
#define CY_CMD_CAT_GET_CFG_ROW_SIZE_CMD_SZ	1
#define CY_CMD_CAT_GET_CFG_ROW_SIZE_RET_SZ	2
/* Read Config Block */
#define CY_CMD_CAT_READ_CFG_BLK_CMD_SZ		6
#define CY_CMD_CAT_READ_CFG_BLK_RET_SZ		7 /* + Data */
#define CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ	5
/* Write Config Block */
#define CY_CMD_CAT_WRITE_CFG_BLK_CMD_SZ		8 /* + Data + Security Key */
#define CY_CMD_CAT_WRITE_CFG_BLK_RET_SZ		5
#define CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ	6
/* Load Self-Test Data */
#define CY_CMD_CAT_LOAD_SELFTEST_DATA_CMD_SZ	6
#define CY_CMD_CAT_LOAD_SELFTEST_DATA_RET_SZ	5 /* + Data */
/* Run Self-Test */
#define CY_CMD_CAT_RUN_SELFTEST_CMD_SZ		2
#define CY_CMD_CAT_RUN_SELFTEST_RET_SZ		3
/* Calibrate IDACs */
#define CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ	2
#define CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ	1
/* Get Self-Test Results */
#define CY_CMD_CAT_GET_SELFTEST_RES_CMD_SZ	6
#define CY_CMD_CAT_GET_SELFTEST_RES_RET_SZ	5 /* + Data */
/* Initialize Baselines */
#define CY_CMD_CAT_INIT_BASELINE_CMD_SZ		2
#define CY_CMD_CAT_INIT_BASELINE_RET_SZ		1
/* Execute Panel Scan */
#define CY_CMD_CAT_EXECUTE_PANEL_SCAN_CMD_SZ	1
#define CY_CMD_CAT_EXECUTE_PANEL_SCAN_RET_SZ	1
/* Retrieve Panel Scan */
#define CY_CMD_CAT_RETRIEVE_PANEL_SCAN_CMD_SZ	6
#define CY_CMD_CAT_RETRIEVE_PANEL_SCAN_RET_SZ	5 /* + Data */
/* Start Sensor Data Mode */
#define CY_CMD_CAT_START_SENSOR_MODE_CMD_SZ	1 /* + Data */
#define CY_CMD_CAT_START_SENSOR_MODE_RET_SZ	0 /* + Data */
/* Stop Sensor Data Mode */
#define CY_CMD_CAT_STOP_SENSOR_MODE_CMD_SZ	1
#define CY_CMD_CAT_STOP_SENSOR_MODE_RET_SZ	0
/* Interrupt Pin Override */
#define CY_CMD_CAT_INT_PIN_OVERRIDE_CMD_SZ	2
#define CY_CMD_CAT_INT_PIN_OVERRIDE_RET_SZ	1
/* Retrieve Data Structure */
#define CY_CMD_CAT_RETRIEVE_DATA_STRUCT_CMD_SZ	6
#define CY_CMD_CAT_RETRIEVE_DATA_STRUCT_RET_SZ	5 /* + Data */
/* Verify Config Block CRC */
#define CY_CMD_CAT_VERIFY_CFG_BLK_CRC_CMD_SZ	2
#define CY_CMD_CAT_VERIFY_CFG_BLK_CRC_RET_SZ	5

#define CY_RAM_ID_ACTIVE_DISTANCE		0x4A
#define CY_RAM_ID_SCAN_TYPE			0x4B
#define CY_RAM_ID_LOW_POWER_INTERVAL		0x4C
#define CY_RAM_ID_REFRESH_INTERVAL		0x4D
#define CY_RAM_ID_ACTIVE_MODE_TIMEOUT		0x4E
#define CY_RAM_ID_ACTIVE_LFT_INTERVAL		0x4F
#define CY_RAM_ID_ACTIVE_DISTANCE2		0x50
#define CY_RAM_ID_CHARGER_STATUS		0x51
#define CY_RAM_ID_IMO_TRIM_VALUE		0x52
#define CY_RAM_ID_FINGER_THRESHOLH		0x93
#define CY_RAM_ID_DETECT_AREA_MARGIN_X		0x58
#define CY_RAM_ID_DETECT_AREA_MARGIN_Y		0x59
#define CY_RAM_ID_GRIP_XEDGE_A			0x70
#define CY_RAM_ID_GRIP_XEDGE_B			0x71
#define CY_RAM_ID_GRIP_XEXC_A			0x72
#define CY_RAM_ID_GRIP_XEXC_B			0x73
#define CY_RAM_ID_GRIP_YEDGE_A			0x74
#define CY_RAM_ID_GRIP_YEDGE_B			0x75
#define CY_RAM_ID_GRIP_YEXC_A			0x76
#define CY_RAM_ID_GRIP_YEXC_B			0x77
#define CY_RAM_ID_GRIP_FIRST_EXC		0x78
#define CY_RAM_ID_GRIP_EXC_EDGE_ORIGIN		0x79
#define CY_RAM_ID_PROX_ACTIVE_DIST_Z_VALUE	0x9B

enum cyttsp4_scan_type {
	CY_ST_APA_MC,
	CY_ST_GLOVE,
	CY_ST_STYLUS,
	CY_ST_PROXIMITY,
};

enum cyttsp4_mode {
	CY_MODE_UNKNOWN      = 0,
	CY_MODE_BOOTLOADER   = (1 << 1),
	CY_MODE_OPERATIONAL  = (1 << 2),
	CY_MODE_SYSINFO      = (1 << 3),
	CY_MODE_CAT          = (1 << 4),
	CY_MODE_STARTUP      = (1 << 5),
	CY_MODE_LOADER       = (1 << 6),
	CY_MODE_CHANGE_MODE  = (1 << 7),
	CY_MODE_CHANGED      = (1 << 8),
	CY_MODE_CMD_COMPLETE = (1 << 9),
};

enum cyttsp4_int_state {
	CY_INT_NONE,
	CY_INT_IGNORE      = (1 << 0),
	CY_INT_MODE_CHANGE = (1 << 1),
	CY_INT_EXEC_CMD    = (1 << 2),
	CY_INT_AWAKE       = (1 << 3),
};

enum cyttsp4_ic_grpnum {
	CY_IC_GRPNUM_RESERVED,
	CY_IC_GRPNUM_CMD_REGS,
	CY_IC_GRPNUM_TCH_REP,
	CY_IC_GRPNUM_DATA_REC,
	CY_IC_GRPNUM_TEST_REC,
	CY_IC_GRPNUM_PCFG_REC,
	CY_IC_GRPNUM_TCH_PARM_VAL,
	CY_IC_GRPNUM_TCH_PARM_SIZE,
	CY_IC_GRPNUM_RESERVED1,
	CY_IC_GRPNUM_RESERVED2,
	CY_IC_GRPNUM_OPCFG_REC,
	CY_IC_GRPNUM_DDATA_REC,
	CY_IC_GRPNUM_MDATA_REC,
	CY_IC_GRPNUM_TEST_REGS,
	CY_IC_GRPNUM_BTN_KEYS,
	CY_IC_GRPNUM_TTHE_REGS,
	CY_IC_GRPNUM_NUM
};

#define CY_VKEYS_X			720
#define CY_VKEYS_Y			1280

enum cyttsp4_event_id {
	CY_EV_NO_EVENT,
	CY_EV_TOUCHDOWN,
	CY_EV_MOVE,		/* significant displacement (> act dist) */
	CY_EV_LIFTOFF,		/* record reports last position */
};

enum cyttsp4_object_id {
	CY_OBJ_STANDARD_FINGER = 0,
	CY_OBJ_PROXIMITY       = 1,
	CY_OBJ_STYLUS          = 2,
	CY_OBJ_GLOVE           = 4,
};

#define CY_POST_CODEL_WDG_RST           0x01
#define CY_POST_CODEL_CFG_DATA_CRC_FAIL 0x02
#define CY_POST_CODEL_PANEL_TEST_FAIL   0x04

/* test mode NULL command driver codes */
enum cyttsp4_null_test_cmd_code {
	CY_NULL_CMD_NULL,
	CY_NULL_CMD_MODE,
	CY_NULL_CMD_STATUS_SIZE,
	CY_NULL_CMD_HANDSHAKE,
	CY_NULL_CMD_LOW_POWER,
};

enum cyttsp4_test_mode {
	CY_TEST_MODE_NORMAL_OP,		/* Send touch data to OS; normal op */
	CY_TEST_MODE_CAT,		/* Configuration and Test */
	CY_TEST_MODE_SYSINFO,		/* System information mode */
	CY_TEST_MODE_CLOSED_UNIT,	/* Send scan data to sysfs */
};

struct cyttsp4_test_mode_params {
	int cur_mode;
	int cur_cmd;
	size_t cur_status_size;
};

/* GEN4/SOLO Operational interface definitions */
/* TTSP System Information interface definitions */
struct cyttsp4_cydata {
	u8 ttpidh;
	u8 ttpidl;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u8 revctrl[CY_NUM_REVCTRL];
	u8 blver_major;
	u8 blver_minor;
	u8 jtag_si_id3;
	u8 jtag_si_id2;
	u8 jtag_si_id1;
	u8 jtag_si_id0;
	u8 mfgid_sz;
	u8 cyito_idh;
	u8 cyito_idl;
	u8 cyito_verh;
	u8 cyito_verl;
	u8 ttsp_ver_major;
	u8 ttsp_ver_minor;
	u8 device_info;
	u8 mfg_id[];
} __packed;

struct cyttsp4_test {
	u8 post_codeh;
	u8 post_codel;
} __packed;

struct cyttsp4_pcfg {
	u8 electrodes_x;
	u8 electrodes_y;
	u8 len_xh;
	u8 len_xl;
	u8 len_yh;
	u8 len_yl;
	u8 res_xh;
	u8 res_xl;
	u8 res_yh;
	u8 res_yl;
	u8 max_zh;
	u8 max_zl;
	u8 panel_info0;
} __packed;

enum cyttsp4_tch_abs {	/* for ordering within the extracted touch data array */
	CY_TCH_X,	/* X */
	CY_TCH_Y,	/* Y */
	CY_TCH_P,	/* P (Z) */
	CY_TCH_T,	/* TOUCH ID */
	CY_TCH_E,	/* EVENT ID */
	CY_TCH_O,	/* OBJECT ID */
	CY_TCH_W,	/* SIZE */
	CY_TCH_MAJ,	/* TOUCH_MAJOR */
	CY_TCH_MIN,	/* TOUCH_MINOR */
	CY_TCH_OR,	/* ORIENTATION */
	CY_TCH_NUM_ABS
};

static const char * const cyttsp4_tch_abs_string[] = {
	[CY_TCH_X]	= "X",
	[CY_TCH_Y]	= "Y",
	[CY_TCH_P]	= "P",
	[CY_TCH_T]	= "T",
	[CY_TCH_E]	= "E",
	[CY_TCH_O]	= "O",
	[CY_TCH_W]	= "W",
	[CY_TCH_MAJ]	= "MAJ",
	[CY_TCH_MIN]	= "MIN",
	[CY_TCH_OR]	= "OR",
	[CY_TCH_NUM_ABS] = "INVALID"
};

#define CY_NUM_TCH_FIELDS		7
#define CY_NUM_EXT_TCH_FIELDS		3

struct cyttsp4_tch_rec_params {
	u8 loc;
	u8 size;
} __packed;

struct cyttsp4_opcfg {
	u8 cmd_ofs;
	u8 rep_ofs;
	u8 rep_szh;
	u8 rep_szl;
	u8 num_btns;
	u8 tt_stat_ofs;
	u8 obj_cfg0;
	u8 max_tchs;
	u8 tch_rec_size;
	struct cyttsp4_tch_rec_params tch_rec_old[CY_NUM_TCH_FIELDS];
	u8 btn_rec_size;/* btn record size (in bytes) */
	u8 btn_diff_ofs;/* btn data loc ,diff counts, (Op-Mode byte ofs) */
	u8 btn_diff_size;/* btn size of diff counts (in bits) */
	struct cyttsp4_tch_rec_params tch_rec_new[CY_NUM_EXT_TCH_FIELDS];
	u8 noise_data_ofs;
	u8 noise_data_sz;
} __packed;

struct cyttsp4_sysinfo_data {
	u8 hst_mode;
	u8 reserved;
	u8 map_szh;
	u8 map_szl;
	u8 cydata_ofsh;
	u8 cydata_ofsl;
	u8 test_ofsh;
	u8 test_ofsl;
	u8 pcfg_ofsh;
	u8 pcfg_ofsl;
	u8 opcfg_ofsh;
	u8 opcfg_ofsl;
	u8 ddata_ofsh;
	u8 ddata_ofsl;
	u8 mdata_ofsh;
	u8 mdata_ofsl;
} __packed;

struct cyttsp4_sysinfo_ptr {
	struct cyttsp4_cydata *cydata;
	struct cyttsp4_test *test;
	struct cyttsp4_pcfg *pcfg;
	struct cyttsp4_opcfg *opcfg;
	struct cyttsp4_ddata *ddata;
	struct cyttsp4_mdata *mdata;
} __packed;

struct cyttsp4_touch {
	int abs[CY_TCH_NUM_ABS];
};

struct cyttsp4_tch_abs_params {
	size_t ofs;	/* abs byte offset */
	size_t size;	/* size in bits */
	size_t max;	/* max value */
	size_t bofs;	/* bit offset */
};

#define CY_NORMAL_ORIGIN		0	/* upper, left corner */
#define CY_INVERT_ORIGIN		1	/* lower, right corner */

struct cyttsp4_sysinfo_ofs {
	size_t chip_type;
	size_t cmd_ofs;
	size_t rep_ofs;
	size_t rep_sz;
	size_t num_btns;
	size_t num_btn_regs;	/* ceil(num_btns/4) */
	size_t tt_stat_ofs;
	size_t tch_rec_size;
	size_t obj_cfg0;
	size_t max_tchs;
	size_t mode_size;
	size_t data_size;
	size_t rep_hdr_size;
	size_t map_sz;
	size_t max_x;
	size_t x_origin;	/* left or right corner */
	size_t max_y;
	size_t y_origin;	/* upper or lower corner */
	size_t max_p;
	size_t cydata_ofs;
	size_t test_ofs;
	size_t pcfg_ofs;
	size_t opcfg_ofs;
	size_t ddata_ofs;
	size_t mdata_ofs;
	size_t cydata_size;
	size_t test_size;
	size_t pcfg_size;
	size_t opcfg_size;
	size_t ddata_size;
	size_t mdata_size;
	size_t btn_keys_size;
	struct cyttsp4_tch_abs_params tch_abs[CY_TCH_NUM_ABS];
	size_t btn_rec_size; /* btn record size (in bytes) */
	size_t btn_diff_ofs;/* btn data loc ,diff counts, (Op-Mode byte ofs) */
	size_t btn_diff_size;/* btn size of diff counts (in bits) */
	size_t noise_data_ofs;
	size_t noise_data_sz;
};

/* button to keycode support */
#define CY_NUM_BTN_PER_REG		4
#define CY_BITS_PER_BTN			2

enum cyttsp4_btn_state {
	CY_BTN_RELEASED = 0,
	CY_BTN_PRESSED = 1,
	CY_BTN_NUM_STATE
};

struct cyttsp4_btn {
	bool enabled;
	int state;	/* CY_BTN_PRESSED, CY_BTN_RELEASED */
	int key_code;
};

struct cyttsp4_ttconfig {
	u16 version;
	u16 length;
	u16 max_length;
	u16 crc;
};

struct cyttsp4_sysinfo {
	bool ready;
	struct cyttsp4_sysinfo_data si_data;
	struct cyttsp4_sysinfo_ptr si_ptrs;
	struct cyttsp4_sysinfo_ofs si_ofs;
	struct cyttsp4_ttconfig ttconfig;
	struct cyttsp4_btn *btn;	/* button states */
	u8 *btn_rec_data;		/* button diff count data */
	u8 *xy_mode;			/* operational mode and status regs */
	u8 *xy_data;			/* operational touch regs */
};

/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add start */
enum cyttsp4_ic_operating_mode_params {
	/* Enable /Disable for FW */
	CY_ENABLE                         = 0x01,
	CY_DISABLE                        = 0x00,

	/* Charger Status */
	CY_CHARGER_STATUS                 = 0x51,		/* Charger Armor */
	CY_CHARGER_STATUS_PARM_SZ         = 4,

	/* Press Threshold */
	CY_PRESS_THRESHOLD                = 0x60,		/* Press Threshold */
	CY_PRESS_THRESHOLD_PARM_SZ        = 1,
		CY_PRESS_TH_VAL_MCMODE            = 0xFF,		/* for test mode */
		CY_PRESS_TH_VAL_NORMAL            = 0x03,		/* for normal mode */

	/* Press Sensor Enable */
	CY_PRESS_SENSOR_ENABLE            = 0x61,		/* [Press Sensor Enable]: 0x61 */
	CY_PRESS_SENSOR_ENABLE_PARM_SZ    = 1,

	/* (Press Sensor) Active calib time */
	CY_ACTIVE_CALIB_TIM               = 0x5D,		/* Active Calibration Time (size = 2) */
	CY_ACTIVE_CALIB_TIM_PARM_SZ       = 2,

	/* (Press Sensor) Periodical calib time */
	CY_PERIODICAL_CALIB_TIM           = 0x5E,		/* Periodical Calibration Time (size = 2) */
	CY_PERIODICAL_CALIB_TIM_PARM_SZ   = 2,

	CY_FORCE_CALIB_GIDAC              = 0x5F,		/* ForceCalibration configurable parameter */
	CY_FORCE_CALIB_GIDAC_PARM_SZ      = 1,			/* ForceCalibration configurable parameter size */

/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add start */
	CY_BTN_DEFAULT_ALT_TX_PERIOD		= 0x62,		/* Press Freq (size = 1) */
	CY_BTN_DEFAULT_ALT_TX_PERIOD_PARM_SZ= 1,

	CY_BTN_ALT_TX_PERIOD1				= 0x63,		/* Press Freq relative 1 (size = 1) */
	CY_BTN_ALT_TX_PERIOD2				= 0x64,		/* Press Freq relative 2 (size = 1) */
	CY_BTN_ALT_TX_PERIOD3				= 0x65,		/* Press Freq relative 3 (size = 1) */
	CY_BTN_ALT_TX_PERIOD4				= 0x66,		/* Press Freq relative 4 (size = 1) */
	CY_BTN_ALT_TX_PERIODX_PARM_SZ		= 1,

	CY_BTN_START_NOISE_EVAL				= 0x67,		/* Press Noise evaluation (size = 1) */

	CY_BTN_ALT_TX1_POSITIVE_SUM			= 0x68,		/* Press ALT TX1 positive sum (size = 2) */
	CY_BTN_ALT_TX1_NEGATIVE_SUM			= 0x69,		/* Press ALT TX1 negative sum (size = 2) */

	CY_BTN_ALT_TX2_POSITIVE_SUM			= 0x6A,		/* Press ALT TX2 positive sum (size = 2) */
	CY_BTN_ALT_TX2_NEGATIVE_SUM			= 0x6B,		/* Press ALT TX2 negative sum (size = 2) */

	CY_BTN_ALT_TX3_POSITIVE_SUM			= 0x6C,		/* Press ALT TX3 positive sum (size = 2) */
	CY_BTN_ALT_TX3_NEGATIVE_SUM			= 0x6D,		/* Press ALT TX3 negative sum (size = 2) */

	CY_BTN_ALT_TX4_POSITIVE_SUM			= 0x6E,		/* Press ALT TX4 positive sum (size = 2) */
	CY_BTN_ALT_TX4_NEGATIVE_SUM			= 0x6F,		/* Press ALT TX4 negative sum (size = 2) */
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add end */

/*
	CY_ACTIVE_DISTANCE                = 0x4A,
	CY_SCAN_TYPE          	          = 0x4B,
	CY_LOW_POWER_INTERVAL             = 0x4C,
	CY_REFRESH_INTERVAL               = 0x4D,
	CY_ACTIVE_MODETIMEOUT             = 0x4E,
	CY_ACTIVE_LOOK_FOR_TOUCH_INTERVAL = 0x4F,
	CY_ACTIVE_DISTANCE_2              = 0x50,
	CY_CHARGER_STATUS                 = 0x51,
	CY_SIZE_AND_ORIENTATION_ENABLE    = 0x80,
*/

};

/* I2C R/W Command reg */
#define		CY_COMMAND_REG				0x02		/* command register addrres */
	/* Waiting for command completion, for _cyttsp4_ioctl_complete_wait, _cyttsp4_core_complete_wait */
#define		CY_WAITCMD_SLEEP_PRESS		12			/* 12 ms : pressure sens wait time */
#define		CY_WAITCMD_SLEEP_TOUCH		30			/* 30 ms : touch sens wait time */
#define		IC_PRS_CMD_DATA_SIZE		16			/* cmd buffer size (work) */
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add end */

/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add start */
enum {
	 CY_LOGLV0			= (int)0				/* every time logging...			*/
	 											/* Very important, Same as error.	*/
	 											/* Hi Level log.					*/
	,CY_LOGLV1			= (int)1				/* Important, Same as warning.		*/
	,CY_LOGLV2			= (int)2				/* Not Important, Same as tarce		*/
	,CY_LOGLV3			= (int)3				/* Information log					*/
};


/* for FJ Cust */
extern u8	cy_loglevel;
/*
 * log macro
 */
#define		_cyttsp4_log(dev, lv, fmt, arg...)	{ if (cy_loglevel >= lv) { if (dev == NULL) {/* nop */;} pr_info(fmt, ## arg); } }
#define		_cyttsp4_log_str(dev, lv, fmt)		{ if (cy_loglevel >= lv) { if (dev == NULL) {/* nop */;} pr_info(fmt); } }			/* for string log only */

/* =========================================
 * for cypress original source code
 * ========================================= */
/*
 * Error log (dev_err => _cyttsp4_elog)
 */
#define		_cyttsp4_elog(dev, fmt, arg...) { _cyttsp4_log(dev, CY_LOGLV0, "[TPD] E:"fmt, arg); }
#define		_cyttsp4_elog_str(dev, fmt)		{ _cyttsp4_log_str(dev, CY_LOGLV0, "[TPD] E:"fmt); }

/*
 * Hi level log = Very important but not error log.
 */
#define		_cyttsp4_hlog(dev, fmt, arg...) { _cyttsp4_log(dev, CY_LOGLV0, "[TPD] H:"fmt, arg); }
#define		_cyttsp4_hlog_str(dev, fmt)		{ _cyttsp4_log_str(dev, CY_LOGLV0, "[TPD] H:"fmt); }
			/* cyttsp4_si_put_log_data(), dev_dbg => _cyttsp4_hlog */

/*
 * Debug log (dev_dbg => _cyttsp4_dlog)
 */
#define		_cyttsp4_dlog(dev, fmt, arg...) { _cyttsp4_log(dev, CY_LOGLV1, "[TPD] D:"fmt, arg); }
#define		_cyttsp4_dlog_str(dev, fmt)		{ _cyttsp4_log_str(dev, CY_LOGLV1, "[TPD] D:"fmt); }

/*
 * VDebug log (dev_vdbg => _cyttsp4_vlog)
 */
#define		_cyttsp4_vlog(dev, fmt, arg...) { _cyttsp4_log(dev, CY_LOGLV3, "[TPD] V:"fmt, arg); }
#define		_cyttsp4_vlog_str(dev, fmt)		{ _cyttsp4_log_str(dev, CY_LOGLV3, "[TPD] V:"fmt); }

/*
 * Information log (dev_info => _cyttsp4_ilog)
 */
#define		_cyttsp4_ilog(dev, fmt, arg...) { _cyttsp4_log(dev, CY_LOGLV3, "[TPD] I:"fmt, arg); }
#define		_cyttsp4_ilog_str(dev, fmt)		{ _cyttsp4_log_str(dev, CY_LOGLV3, "[TPD] I:"fmt); }

/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add end */

#endif /* _CYTTSP4_REGS_H */
