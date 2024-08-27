/*
 * COPYRIGHT(C) 2013 FUJITSU LIMITED
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef _FJTOUCH_IF_H_
#define _FJTOUCH_IF_H_

#include <linux/ioctl.h>

/* DeviceStatus */
enum devicepowerstatus
{
	DEEPSLEEP        = 0x00,   /* DeepSleep          */
	WAKEUP,                    /* WakeUp             */
	HARDRESET,                 /* HardReset          */
	SOFTRESET,                 /* SoftReset          */
	TOUCH_EVENT_ENABLE,        /* TouchEvent Enable  */
	TOUCH_EVENT_DISABLE,       /* TouchEvent Disable */
/* 13-1st HOVER start */
	FIRMWRITERESET,            /* FirmWriteReset     */
	HOVER_ENABLE,              /* Enable Hover       */
	HOVER_DISABLE,             /* Disable Hover      */
/* 13-1st HOVER end */
};

enum diagnostic_multi_scan_mode
{
	MULTI_INITIAL = 0,		/* Diagnostic Multi Scan Start */
	MULTI_UPDATE,			/* Diagnostic Scan(Multi) */
	MULTI_FINAL				/* Diagnostic multi Scan End */
};

struct fj_touch_i2c_data
{
	int  slaveaddress;
	int  offset;
	int  use_offset;
	int  length;
	char *i2c_data_buf;
};

struct fj_touch_do_selftest_data
{
	int  test_cmd;							/* IN */
	int  test_id;							/* IN */
	int  idac_flag;							/* IN */
	char *pin_mask;							/* IN */
	char *result_data_buf;					/* OUT */
	unsigned int load_offset;				/* IN */
	unsigned int load_length;				/* IN */
	char *self_test_data;					/* OUT */
};

struct fj_touch_debug_diagnostic_data
{
	int multi_scan;							/* IN */
	char *test_cmd;							/* IN */
	unsigned char status_code;				/* OUT */
	unsigned char data_ID_spec_type;		/* OUT */
	unsigned char num_ele1;					/* OUT */
	unsigned char num_ele2;					/* OUT */
	unsigned char data_format;				/* OUT */
	char *data;								/* OUT */
};

/* 13-1stUpdate Firmware start */
/* Model Code */
#define	TOUCH_MODEL_CODE_DMY		0		/* Dummy    */
#define	TOUCH_MODEL_CODE_CAND		1
#define	TOUCH_MODEL_CODE_THMS		2
#define	TOUCH_MODEL_CODE_LPND		3
/* 13-2ndHMWBootup2 start */
#define	TOUCH_MODEL_CODE_HMW		4
#define	TOUCH_MODEL_CODE_SPD		5		/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
#define	TOUCH_MODEL_CODE_ARG		6		/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
#define	TOUCH_MODEL_CODE_MAX		7		/* max      */
/* 13-2ndHMWBootup2 end */
/* Model Name */
#define	TOUCH_MODEL_NAME_DMY		"T131DMY"	/* Dummy    */
#define	TOUCH_MODEL_NAME_CAN		"T131CA"
#define	TOUCH_MODEL_NAME_THM		"T131TH"
#define	TOUCH_MODEL_NAME_LPN		"T131LU"
#define	TOUCH_MODEL_NAME_HMW		"T131HM"	/* 131HM    *//* 13-2ndHMWBootup2 */
#define	TOUCH_MODEL_NAME_SPD		"T131SP"	/* 131SP    *//* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
#define	TOUCH_MODEL_NAME_ARG		"T132AR"	/* 131AR    *//* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */

/* Maker ID */
#define	TOUCH_MAKER_ID_DMY			0		/* Dummy   */
#define	TOUCH_MAKER_ID_JDI			1		/* JDI     */
#define	TOUCH_MAKER_ID_WIN			2		/* Wintek  */
#define	TOUCH_MAKER_ID_KYO			3		/* Kyocera */
#define	TOUCH_MAKER_ID_MAX			4		/* max     */
/* Maker Name */
#define	TOUCH_MAKER_NAME_DMY		"D"		/* Dummy   */
#define	TOUCH_MAKER_NAME_JDI		"J"		/* JDI     */
#define	TOUCH_MAKER_NAME_WIN		"W"		/* Wintek  */
#define	TOUCH_MAKER_NAME_KYO		"K"		/* Kyocera */

/* Lcd ID */
#define	TOUCH_LCD_ID_DMY			0		/* Dummy */
#define	TOUCH_LCD_ID_JDI			1		/* JDI   */
#define	TOUCH_LCD_ID_SHP			2		/* Sharp */
#define	TOUCH_LCD_ID_MAX			3		/* max   */
/* Lcd Name */
#define	TOUCH_LCD_NAME_DMY			"D"		/* Dummy */
#define	TOUCH_LCD_NAME_JDI			"J"		/* JDI   */
#define	TOUCH_LCD_NAME_SHP			"S"		/* Sharp */

/* Extension & Connect */
#define	TOUCH_FIRM_EXT			".cyacd"	/* Extension        */
#define	TOUCH_FIRM_EXT_LEN			6		/* Extension Length */

/* Firmware Data */
struct fj_touch_firmware_data
{
	unsigned char model_code;				/* OUT */
	unsigned char makerID;					/* OUT */
	unsigned char lcdID;					/* OUT */
/* FUJITSU:2013-05-28 HMW_TOUCH_FWVER add start */
	unsigned int        revctrl_version;	/* OUT */	/* REVCTRL d(8:11) */
	unsigned short int  firmware_version;	/* OUT */	/* fw ver d(2:3) */
														/* bit[15:8] : TrueTouch Product Firmware Major Version */
														/* bit[7:0]  : TrueTouch Product Firmware Minor Version */
	unsigned short int  config_version;		/* OUT */	/* config ver d(29:30) */
														/* MSB of CYITO Project Configuration Version */
														/* LSB of CYITO Project Configuration Version */
	unsigned short int  project_id;			/* OUT */	/* ProjectID d(40) */
/* FUJITSU:2013-05-28 HMW_TOUCH_FWVER add end */
};

/* Firmware File Name */
struct fj_touch_cnv_firmname
{
	int		length;							/* Length */
	const char	*name;							/* Name   */
};

/* Model Name Table */
static const struct fj_touch_cnv_firmname _fj_touch_cnv_model_name[] =
{
	{
		7, TOUCH_MODEL_NAME_DMY
	},
	{
		6, TOUCH_MODEL_NAME_CAN
	},
	{
		6, TOUCH_MODEL_NAME_THM
	},
	{
		6, TOUCH_MODEL_NAME_LPN
	}
/* 13-2ndHMWBootup2 start */
	,{
		6, TOUCH_MODEL_NAME_HMW
	}
/* 13-2ndHMWBootup2 end */
/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
	,{
		6, TOUCH_MODEL_NAME_SPD
	}
	,{
		6, TOUCH_MODEL_NAME_ARG
	}
/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL end */
};

/* Maker Name Table */
static const struct fj_touch_cnv_firmname _fj_touch_cnv_maker_name[] =
{
	{
		1, TOUCH_MAKER_NAME_DMY
	},
	{
		1, TOUCH_MAKER_NAME_JDI
	},
	{
		1, TOUCH_MAKER_NAME_WIN
	},
	{
		1, TOUCH_MAKER_NAME_KYO
	}
};

/* LCD Name Table */
static const struct fj_touch_cnv_firmname _fj_touch_cnv_lcd_name[] =
{
	{
		1, TOUCH_LCD_NAME_DMY
	},
	{
		1, TOUCH_LCD_NAME_JDI
	},
	{
		1, TOUCH_LCD_NAME_SHP
	}
};

/* Firmware File Name Convert Macro */
static inline void fj_touch_cnv_firmfilename(struct fj_touch_firmware_data *st_data, int *length, char *name)
{
	*length = 0;
	/* Mode Name Convert */
	if ( name ) {
		strncpy( &name[ *length ], _fj_touch_cnv_model_name[ st_data->model_code ].name, _fj_touch_cnv_model_name[ st_data->model_code ].length );
	}
	*length += _fj_touch_cnv_model_name[ st_data->model_code ].length;
	/* Maker Name Convert */
	if ( name ) {
		strncpy( &name[ *length ], _fj_touch_cnv_maker_name[ st_data->makerID ].name, _fj_touch_cnv_maker_name[ st_data->makerID ].length );
	}
	*length += _fj_touch_cnv_maker_name[ st_data->makerID ].length;
	/* LCD Name Convert */
	if ( name ) {
		strncpy( &name[ *length ], _fj_touch_cnv_lcd_name[ st_data->lcdID ].name, _fj_touch_cnv_lcd_name[ st_data->lcdID ].length );
	}
	*length += _fj_touch_cnv_lcd_name[ st_data->lcdID ].length;
	/* Extension Set */
	if ( name ) {
		strncpy( &name[ *length ], TOUCH_FIRM_EXT, TOUCH_FIRM_EXT_LEN );
	}
	*length += TOUCH_FIRM_EXT_LEN;
}
/* 13-1stUpdate Firmware end */

/* 13-1st HOVER start */
/* device set mode */
enum fj_touch_set_device_mode
{
	ENABLE_HOVERMODE = (int)0,
	DISABLE_HOVERMODE,
};

/* device mode */
enum fj_touch_get_device_mode
{
	NORMAL_FINGER_MODE = (int)0,
	HOVER_MODE,
};
/* 13-1st HOVER end */

/* 13-2ndHMWBootup2 start */
/* Pressure Sensor */
#define	FJTOUCH_PRESSURE_NUM_OF_ELEMENTS		2
struct fj_touch_pressure_element
{
	int			raw_val;						/* OUT */	/* raw data (16 bit) */
	int			baseline_val;					/* OUT */	/* base line data (16 bit) */
	int			diff_val;						/* OUT */	/* diff data (16 bit) */
	int			status;							/* OUT */	/* status (16 bit) */
};

struct fj_touch_pressure_info
{
	/* Cautions! : Should 0-Initialize this structure(fj_touch_pressure_info). */

	int			multi_scan;						/* IN */	/* diagnostic_multi_scan_mode */

	struct fj_touch_pressure_element	elem[ FJTOUCH_PRESSURE_NUM_OF_ELEMENTS ];
};

enum {
	FJTOUCH_CALIB_PRESS_PTN1				= 0x00000001,	/* Bit0 : PWC of Press Sensor */
	FJTOUCH_CALIB_PRESS_PTN2				= 0x00000002,	/* Bit1 : gidac of Press Sensor */
};

struct fj_touch_do_calibration_pressure
{
	/* Cautions! : Should 0-Initialize this structure(fj_touch_do_calibration_pressure). */
	int			do_calib_type;					/* IN */	/* calibration type , FJTOUCH_CALIB_PRESS_xxxx */
};
/* 13-2ndHMWBootup2 end */

/* 13-2ndHMWBootup3 start */
/* Pressure Sensor Calibration result */
enum {
	/* not use this enum. */	/* 13-2ndHMWBootup4 add comment */
	FJTOUCH_CALIB_RET_OK				= 0x00,		/* OK */
	FJTOUCH_CALIB_RET_NG				= 0x01,		/* NG */
};

struct fj_touch_ret_calibration_pressure {
	/* Cautions! : Should 0-Initialize this structure(fj_touch_ret_calibration_pressure). */
	int			get_calib_type;					/* IN */	/* calibration type , FJTOUCH_CALIB_PRESS_xxxx */

/* 13-2ndHMWBootup4 mod start */
	unsigned char 		ptn1_val[ FJTOUCH_PRESSURE_NUM_OF_ELEMENTS ];	/* OUT */	/* FJTOUCH_CALIB_PRESS_PTN1's value */
																					/* 0 : Left, 1 : Right */
	unsigned char 		ptn2_val;										/* OUT */	/* FJTOUCH_CALIB_PRESS_PTN2's value */


	/* not use this member. */
	int			calib_ret;						/* OUT */	/* FJTOUCH_CALIB_RET_xx	*/
/* 13-2ndHMWBootup4 mod end */
};

/* Pressure Sensor Ctrl */
enum {
	FJTOUCH_TOUCHPRESS_ENABLE				= 0x00,		/* Pre-Touch Enable */
	FJTOUCH_TOUCHPRESS_DISABLE			= 0x01,		/* Pre-Touch Disable */
};

struct fj_touch_touchpress_ctrl {
	/* Cautions! : Should 0-Initialize this structure(fj_touch_touchpress_ctrl). */
	int		touchpress_setting;						/* FJTOUCH_TOUCHPRESS_xxxxx */
													/* IN  : IOCTL_SET_TOUCHPRESS */
													/* OUT : IOCTL_GET_TOUCHPRESS (for debug) */
};
/* 13-2ndHMWBootup3 end */

/* 13-2ndHMW_press_get_pos start */
/*
 * Touch Position
 */
#define FJTOUCH_NUM_OF_POSITION					255

enum {
	FJTOUCH_GET_TOUCHPOS_EOK = 0,							/* OK : Success 		*/
	FJTOUCH_GET_TOUCHPOS_ENOINT,							/* NG : No Interrupt	*/
	FJTOUCH_GET_TOUCHPOS_ENOSIGLETOUCH,						/* NG : No Sigle touch (Multi touch or Large Object)	*/
};

struct fj_touch_position {
	unsigned short int		pos_x;						/* OUT */
	unsigned short int		pos_y;						/* OUT */
	signed short int		press_val;					/* OUT */
};

struct fj_touch_position_info {
	/* Cautions! : Should 0-Initialize this structure(fj_touch_position_info). */
	int		num_of_position;					/* IN */	/* 0 - FJTOUCH_NUM_OF_POSITION(255)	*/
															/* number or touch IC INT(IRQ).		*/
	int		result;								/* OUT */	/* FJTOUCH_GET_TOUCHPOS_xxx			*/
	struct fj_touch_position	pos [ FJTOUCH_NUM_OF_POSITION ];	/* OUT */
};

/*
 * Press Normalization
 */
enum {
	FJTOUCH_PRESS_NORMALIZE_ENABLE = 0,				/* Enable press normalization 		*/
	FJTOUCH_PRESS_NORMALIZE_DISABLE,				/* Disable press normalization 		*/
};
	
struct fj_touch_press_normalization
{
	/* Cautions! : Should 0-Initialize this structure(fj_touch_press_normalization). */
	int			normalization;						/* IN */ /* FJTOUCH_PRESS_NORMALIZE_xxxx */
};
/* 13-2ndHMW_press_get_pos end */

/* 13-2ndHMW_press_noise_eval start */
/* fj_touch_press_noise_eval */
#define FJ_TOUCH_PRESS_NOISE_EVAL_NUM	4

struct fj_touch_press_noise_eval
{
	/* Cautions! : Should 0-Initialize this structure(fj_touch_press_noise_eval). */

	signed char				eval_freq[ FJ_TOUCH_PRESS_NOISE_EVAL_NUM ];		/* IN */
	
	signed char				def_tx_period;									/* OUT */
																			/* BTN_DEFAULT_ALT_TX_PERIOD from touch-IC */
	unsigned short int		positive_sum[ FJ_TOUCH_PRESS_NOISE_EVAL_NUM ];	/* OUT */
	unsigned short int		negative_sum[ FJ_TOUCH_PRESS_NOISE_EVAL_NUM ];	/* OUT */
};
/* 13-2ndHMW_press_noise_eval end */

#define FJTOUCH_IOCTL_MAGIC 'c'

#define IOCTL_SET_POWERMODE         _IOR(FJTOUCH_IOCTL_MAGIC, 1, int)
#define IOCTL_GET_FIRMVERSION       _IOW(FJTOUCH_IOCTL_MAGIC, 2,  char * )
#define IOCTL_DO_CALIBRATION        _IO(FJTOUCH_IOCTL_MAGIC, 3)
#define IOCTL_I2C_READ              _IOR(FJTOUCH_IOCTL_MAGIC, 4, struct fj_touch_i2c_data)
#define IOCTL_I2C_WRITE             _IOW(FJTOUCH_IOCTL_MAGIC, 5, struct fj_touch_i2c_data)
#define IOCTL_DO_SELFTEST           _IOR(FJTOUCH_IOCTL_MAGIC, 6, struct fj_touch_do_selftest_data)
#define IOCTL_DEBUG_DIAGNOSTIC      _IOR(FJTOUCH_IOCTL_MAGIC, 7, struct fj_touch_debug_diagnostic_data)
#define IOCTL_GET_UPDATEFIRMINFO    _IOW(FJTOUCH_IOCTL_MAGIC, 8, struct fj_touch_firmware_data)	/* 13-1stUpdate Firmware */
/* 13-1st HOVER start */
#define IOCTL_SET_TOUCHMODE         _IOR(FJTOUCH_IOCTL_MAGIC, 9, int)
#define IOCTL_GET_TOUCHMODE         _IOW(FJTOUCH_IOCTL_MAGIC, 10, int)
/* 13-1st HOVER end */

/* 13-2ndHMWBootup2 start */
#define IOCTL_GET_PRESSUREINFO				_IOW(FJTOUCH_IOCTL_MAGIC, 11, struct fj_touch_pressure_info)
#define IOCTL_DO_CALIBRATION_PRESSURE		_IOW(FJTOUCH_IOCTL_MAGIC, 12, struct fj_touch_do_calibration_pressure)
/* 13-2ndHMWBootup2 end */

/* 13-2ndHMWBootup3 start */
#define IOCTL_GET_RET_CALIB_PRESSURE		_IOW(FJTOUCH_IOCTL_MAGIC, 13, struct fj_touch_ret_calibration_pressure)
#define IOCTL_SET_TOUCHPRESS				_IOW(FJTOUCH_IOCTL_MAGIC, 14, struct fj_touch_touchpress_ctrl)
#define IOCTL_GET_TOUCHPRESS				_IOW(FJTOUCH_IOCTL_MAGIC, 15, struct fj_touch_touchpress_ctrl)
/* 13-2ndHMWBootup3 end */

/* 13-2ndHMW_press_get_pos start */
#define IOCTL_GET_TOUCH_POSITION			_IOW(FJTOUCH_IOCTL_MAGIC, 16, struct fj_touch_position_info)
#define IOCTL_SET_PRESS_NORMALIZATION		_IOW(FJTOUCH_IOCTL_MAGIC, 17, struct fj_touch_press_normalization)
/* 13-2ndHMW_press_get_pos end */

/* 13-2ndHMW_press_noise_eval start */
#define IOCTL_GET_PRESS_NOISE_EVAL			_IOW(FJTOUCH_IOCTL_MAGIC, 18, struct fj_touch_press_noise_eval)
/* 13-2ndHMW_press_noise_eval end */

#define FJTOUCH_IOCTL_INFO 'c'

// ***Sample***
//
// int touch_fd;
// unsigned long Data; /* Data  0x00000001:TMD  0x00000002:Wintek */
// touch_fd = open("/dev/fj_touch_info", O_RDWR);
// ioctl(touch_fd, IOCTL_GET_PANELVENDOR, &Data);
// close(touch_fd);
//
#define IOCTL_GET_PANELVENDOR        _IOR(FJTOUCH_IOCTL_INFO, 1, unsigned long)

#endif // _FJTOUCH_IF_H_
