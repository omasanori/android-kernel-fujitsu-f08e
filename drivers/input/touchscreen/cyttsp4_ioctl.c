/*
 * cyttsp4_ioctl.c
 * Cypress TrueTouch(TM) Standard Product V4 I/O Controller.
 * Configuration and Test command/status user interface.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
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
/*
 *----------------------------------------------------------------------------
 * COPYRIGHT(C) FUJITSU LIMITED 2013
 *----------------------------------------------------------------------------
 */

#include <linux/input/cyttsp4_bus.h>
#include <linux/input/cyttsp4_core.h>
#include <linux/input/cyttsp4_mt.h>

#include <linux/delay.h>
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL_EARLY_SUSPEND add start */
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL_EARLY_SUSPEND add end */
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include "fj_touch_if.h"
#include "cyttsp4_regs.h"
#include "cyttsp4_ioctl.h"
/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add start */
#include "fj_touch_noracs.h"
/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add end */

#include <linux/nonvolatile_common.h>	/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE add */

extern u8	cy_loglevel;
extern int makercmd_mode;				/* FUJITSU:2013-06-20 HMW_TOUCH_KOTEI_PERIODICAL_CALIB_TIM add */

/*==============================================
 * enum / define
 *==============================================*/
/* for debug */
#define LOG_I2C_ADRR			0x24		/* Fake I2C Slave address, for log param use only! */

#define	CY_PR_BUF_DUMP_LEN		16			/* number of data per line (16 or 32) */
#define	CY_PR_BUF_DUMP_LINE		256			/* line buffer size */

/* for ioctl state ctrl */
enum cyttsp4_ic_resource_ctrl {
	CY_IC_RES_NONE				= 0x00,
	CY_IC_RES_EXCLUSIVE			= 0x01,
	CY_IC_RES_MODECTRL			= 0x02,
};

/* for ioctl opereation */
enum cyttsp4_ic_calibrate_idacs_params {
	/* Capacitance sensing mode */
	CY_MUTUAL_CAPACITANCE_FINE    = 0x00,
	CY_MUTUAL_CAPACITANCE_BUTTONS = 0x01,
	CY_SELF_CAPACITANCE           = 0x02,
	CY_BALANCED                   = 0x03,
};

/* init baseline param */
enum cyttsp4_ic_initialize_baselines_scan_mode_params {
	CY_FINE_MUTUAL_SCAN   = 0x01,
	CY_BUTTON_MUTUAL_SCAN = 0x02,
	CY_SELF_SCAN          = 0x04, 
	CY_BALANCED_SCAN      = 0x08,
};

/* I2C Com max size (etc)*/
#define CY_QUALCOMM_I2C_LIMIT_HALF       128 /* QUALCOMM I2C CONTROLLER LIMIT is 256 byte */

/* Waiting for Touch IRQ */
#define CY_WAITIRQ_SLEEP_TOUCH_POS		16			/* 16 ms : pressure sens wait time */		/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add */

/* firm version data buffer size */
#define CY_FIRMVERSION_SIZE     80

/* 16 bit raw data read */
/* FUJITSU:2013-03-18 16bit_FW_IO add start */
#define CY_DIAG_RAW_DATA_SIZE_MAX   256
#define CY_DIAG_NUM_OF_ELEMENT      405
#define CY_DIAG_RPC_CMD_SIZE        5
#define CY_DIAG_ELEMENTSIZE_16BIT   2
#define CY_DIAG_ELEMENTSIZE_MASK    0x07
/* FUJITSU:2013-03-18 16bit_FW_IO add end */

/*==============================================
 * Struct definition
 *==============================================*/

struct cyttsp4_ioctl_data {
	struct cyttsp4_device *ttsp;
	struct cyttsp4_ioctl_platform_data *pdata;
	struct cyttsp4_sysinfo *si;
	struct cyttsp4_test_mode_params test;
	struct cdev cyttsp4_cdev;

	u8 pr_buf[CY_MAX_PRBUF_SIZE];			/* for debug log */
	u8 pr_dump[ CY_PR_BUF_DUMP_LINE ];		/* for debug */
	atomic_t						atm_mc_stat;
	int curr_mode;
	int curr_resource;						/* cyttsp4_ic_resource_ctrl */
	bool	chk_bootloadermode_ctrl;
/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG addstart */
	struct cyttsp4_nonvolatile_info		*nv_info;
/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add end */
/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add start */
	struct mutex							report_lock;
	struct cyttsp4_ioctl_touch_pos_info		touch_pos;
/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add end */

/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL_EARLY_SUSPEND add start */
	struct early_suspend					early_susp;
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL_EARLY_SUSPEND add end */
};

struct second_read_selftest_data
{
	unsigned int load_offset;				/* IN */
	unsigned int load_length;				/* IN */
	char *second_self_test_data;			/* OUT */
};

/*==============================================
 * work RAM
 *==============================================*/
static int cdev_major = 0;
static struct class* udev_class;

/*==============================================
 * Constant tables
 *==============================================*/
static u8 mutual_capacitance_fine_param[] = {
	CY_MUTUAL_CAPACITANCE_FINE, 0x00, 0x00
};
static u8 mutual_capacitance_buttons_param[] = {
  CY_MUTUAL_CAPACITANCE_BUTTONS, 0x00, 0x00
};

static u8 self_capacitance_param[] = {
	CY_SELF_CAPACITANCE, 0x00, 0x00
};

#define CY_BLMODE_EXIT_PARM_SZ		9
static u8 bootloadermode_exit[ CY_BLMODE_EXIT_PARM_SZ ] = {
	0x00, 0xFF, 0x01, 0x3B, 0x00, 0x00, 0x4F, 0x6D, 0x17
};

/*=================================================
 * Internal common macro func Prototype definition
 *=================================================*/

#define  _DIO_REF_DEV(dio)		&(((struct cyttsp4_ioctl_data *)(dio))->ttsp->dev)
#define  _DIO_REF_TTSP(dio)		(((struct cyttsp4_ioctl_data *)(dio))->ttsp)

/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG mod start */
#include <stdarg.h>
	#define		_cyttsp4_dbg(dio, lv, fmt,...) {\
					_cyttsp4_log(_DIO_REF_DEV(dio), lv, fmt, ## __VA_ARGS__);	\
				}

/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG mod end */

/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add start */
#define	_DIO_REF_SET_GET(IsSet)		(IsSet == true ? "set" : "get")
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add end */

/*=================================================
 * Internal common func Prototype definition
 *=================================================*/
static int _cyttsp4_ioctl_start(struct cyttsp4_ioctl_data *dio, int mode, int timeout);
static int _cyttsp4_ioctl_end  (struct cyttsp4_ioctl_data *dio, int mode);

static int _cyttsp4_ioctl_i2c_read (struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr);
static int _cyttsp4_ioctl_i2c_write(struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr);
static int _cyttsp4_ioctl_read_block_data (struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr);
static int _cyttsp4_ioctl_write_block_data(struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr);

static void _cyttsp4_ioctl_pr_buf(struct cyttsp4_ioctl_data *dio, u8 *dptr, int size, const char *data_name, int loglevel);
static int  _cyttsp4_ioctl_complete_wait( struct cyttsp4_ioctl_data *dio, int wait_ms);
static bool _cyttsp4_ioctl_IsCoreSleep(struct cyttsp4_ioctl_data *dio);
static bool _cyttsp4_ioctl_IsCoreProbeErr(struct cyttsp4_ioctl_data *dio);

#define		_cyttsp4_ioctl_runtime_debug( dio , reg, expect_val) \
	do{																	\
		u32		get_val = 0;											\
		if (cy_nv_info.data_io_debug == CY_TOUCH_DBG_ON) {				\
			if (cyttsp4_request_get_runtime_parm(_DIO_REF_TTSP(dio), reg, &get_val) == 0) {		\
				if (get_val != (u32)expect_val) {						\
					_cyttsp4_elog(_DIO_REF_DEV(dio), "(Debug) %s: not equal value (0x%02X : get = %d, expect = %d)\n", __func__, reg, get_val, expect_val);	\
				}														\
			} else {													\
				_cyttsp4_elog(_DIO_REF_DEV(dio), "(Debug) %s: get_runtime (0x%02X) error.\n", __func__, reg);	\
			}															\
		}																\
	}while(0)

/*=================================================
 * Program Section
 *=================================================*/

/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add start */
/*
 * External common func for Core.
 */
#if 0
__ext_common___________(){}
#endif

int cyttsp4_ioctl_runtime_PressThreshold_ctrl(void *ioctl, bool IsSet, enum cyttsp4_information_state *info, u8 *opt_val)
{
	struct cyttsp4_ioctl_data *dio = (struct cyttsp4_ioctl_data *)ioctl;
	u8	tmp_val = 0;
	int	retval = 0;
	int	use_opt_val = 1;
	u32		io_val = 0;
	u8		parm_id = 0;
	u8		parm_sz = 0;

	if (dio == NULL) {
		pr_info("[TPD_Ic] %s : Handle NULL", __func__);
		return -EINVAL;
	}

	if (opt_val == NULL) {
		opt_val		= &tmp_val;
		use_opt_val	= 0;
	}

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s (%s) [S]: info = %d, val = 0x%02X, use_opt = %d.\n", __func__, _DIO_REF_SET_GET(IsSet), *info, *opt_val, use_opt_val);

	/* *info : CY_INFOSTATE_TRUE = INT Mode, CY_INFOSTATE_FALSE = Normal Mode */
	if (IsSet == true) {
		io_val		= CY_PRESS_TH_VAL_NORMAL;		/* xxxx ms */
		parm_id		= CY_PRESS_THRESHOLD;
		parm_sz		= CY_PRESS_THRESHOLD_PARM_SZ;

		if (use_opt_val == 1) {
			io_val		= (u32)(*opt_val & 0x000000FF);
		} else {
			io_val		= (*info == CY_INFOSTATE_TRUE ? CY_PRESS_TH_VAL_MCMODE : CY_PRESS_TH_VAL_NORMAL);		/* 0xFF : INT Mode, != 0xFF : Normal Mode	*/
		}

		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: Set Press Threshold(%02X, %d : %d count).\n", __func__, parm_id, parm_sz, io_val);

		retval = cyttsp4_request_set_runtime_parm( _DIO_REF_TTSP(dio), parm_id, parm_sz, io_val);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Error.Set RT Config, val = %d\n", __func__, io_val);
			goto error;
		}
		_cyttsp4_ioctl_runtime_debug( _DIO_REF_TTSP(dio), parm_id, io_val);			/* for debug */

	} else {
		parm_id		= CY_PRESS_THRESHOLD;

		retval = cyttsp4_request_get_runtime_parm(_DIO_REF_TTSP(dio), parm_id, &io_val);

		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Error.Get RT Config.\n", __func__);
			goto error;
		}

		if (io_val == CY_PRESS_TH_VAL_MCMODE) {
			*info = CY_INFOSTATE_TRUE;
		} else {
			*info = CY_INFOSTATE_FALSE;
		}
		*opt_val = (io_val & 0x000000FF);
	}

error:
	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s (%s) [E]: ret = %d, info = %d, val = 0x%02X, use_opt = %d.\n", __func__, _DIO_REF_SET_GET(IsSet), retval, *info, *opt_val, use_opt_val);
	return retval;
}
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add end */

/*
 * Internal common func
 */
#if 0
__common___________(){}
#endif

static void _cyttsp4_ioctl_pr_buf(struct cyttsp4_ioctl_data *dio, u8 *dptr, int size, const char *data_name, int loglevel)
{
	int i, k;
	const char fmt[] = "%02X ";
	int max;
	u8 *pr_buf;

/* FUJITSU:2013-05-10 HMW_TTDA_MODIFY_LOG add start */
	if (cy_loglevel < loglevel) {
		return;
	}
/* FUJITSU:2013-05-10 HMW_TTDA_MODIFY_LOG add start */

	if (!size)
		return;
	max = (CY_MAX_PRBUF_SIZE - 1) - sizeof(CY_PR_TRUNCATED);

	pr_buf = &dio->pr_buf[0];

	pr_buf[0] = 0;
	for (i = k = 0; i < size && k < max; i++, k += 3)
		scnprintf(pr_buf + k, CY_MAX_PRBUF_SIZE, fmt, dptr[i]);

	pr_info( "[TPD_Ic] %s[0..%d]=%s%s\n", data_name, size - 1,
			pr_buf, size <= max ? "" : CY_PR_TRUNCATED);
}

static bool _cyttsp4_ioctl_chk_blmode_exit( struct cyttsp4_ioctl_data *dio , u8 command, size_t length, u8 *buf, bool use_subaddr)
{
	bool ret = false;

	if ((dio->chk_bootloadermode_ctrl != true) ||
		(use_subaddr == true) ||
		(length != CY_BLMODE_EXIT_PARM_SZ)) {
		return false;
	}

	if (memcmp(buf, &bootloadermode_exit[0], CY_BLMODE_EXIT_PARM_SZ) == 0) {
		ret = true;
		pr_info("[TPD_Ic] find BL mode exit!!\n");
	}

	return ret;
}

/*
 * wait complete
 */
static int _cyttsp4_ioctl_complete_wait( struct cyttsp4_ioctl_data *dio , int wait_ms)
{
	/* static int _cyttsp4_complete_wait( struct cyttsp4 *ts ) */
	int ret = 0;
	int count = 0;
	u8 read_cmd_reg = 0;
	int limit = 100;

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s: [S], ms = %d\n", __func__, wait_ms);

	if (wait_ms == 0) {
		/* false safe ... */
		wait_ms = CY_WAITCMD_SLEEP_TOUCH;
	}
	for (count = 0; count < limit; count++) {
		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: try = %d.\n", __func__, count);
		ret = _cyttsp4_ioctl_read_block_data(dio, CY_COMMAND_REG, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
		if (ret < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Command I2C error! \n", __func__);
			break;
		} else if (count > limit) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Command timeout! \n", __func__);
			ret = -EIO;
			break;
		}
		if ((read_cmd_reg & CY_CMD_COMPLETE) == CY_CMD_COMPLETE ) {
			/* donw wait complete! */
			break;
		}
		msleep( wait_ms );
	}

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s: [E], ret = %d, status = 0x%02X, c = %d\n", __func__, ret, read_cmd_reg , count);

	return ret;
}

/*
 * core state check
 */
static bool _cyttsp4_ioctl_IsCoreSleep(struct cyttsp4_ioctl_data *dio)
{
	int ret = 0;
	bool blRet = false;
	enum cyttsp4_information_state sts = CY_INFOSTATE_TRUE;

	ret = cyttsp4_request_get_information( _DIO_REF_TTSP(dio) , CY_INFOTYPE_IS_CORE_SLEEP, CY_INFOOPT_NONE, (void*)&sts);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on get info r=%d\n", __func__, ret);
		blRet = true;				/* false safe ... */
	} else {
		if (sts == CY_INFOSTATE_TRUE) {
			blRet = true;
		} else {
			blRet = false;
		}
	}

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s: ret = %d(%d)\n", __func__, ret , sts);
	return blRet;
}

/*
 * core probe error check
 */
static bool _cyttsp4_ioctl_IsCoreProbeErr(struct cyttsp4_ioctl_data *dio)
{
	int ret = 0;
	bool blRet = false;
	enum cyttsp4_information_state sts = CY_INFOSTATE_TRUE;

	ret = cyttsp4_request_get_information( _DIO_REF_TTSP(dio) , CY_INFOTYPE_IS_PROBE_ERR, CY_INFOOPT_NONE, (void*)&sts);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on get info r=%d\n", __func__, ret);
		blRet = true;				/* false safe ... */
	} else {
		if (sts == CY_INFOSTATE_TRUE) {
			blRet = true;
		} else {
			blRet = false;
		}
	}

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s: ret = %d(%d)\n", __func__, ret , sts);
	return blRet;
}

/*
 * Mode Ctrl
 */
static int _cyttsp4_ioctl_start(struct cyttsp4_ioctl_data *dio, int mode, int timeout)
{
	int ret = 0;

	pr_info("[TPD_Ic]ioctl_start [S] mode = %d.\n", mode);

	if ((dio->curr_resource & CY_IC_RES_EXCLUSIVE) == CY_IC_RES_NONE) {
		ret = cyttsp4_request_exclusive(dio->ttsp, timeout);
		if (ret < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Error on request exclusive r=%d\n", __func__, ret);
			return ret;
		}
	}
	dio->curr_resource |= CY_IC_RES_EXCLUSIVE;

	ret = cyttsp4_request_set_mode(dio->ttsp, mode);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on request set mode r=%d\n", __func__, ret);
		return ret;
	}

	dio->curr_resource |= CY_IC_RES_MODECTRL;

	dio->curr_mode	= mode;

	pr_info("[TPD_Ic]ioctl_start [E], ret = %d.\n", ret);

	return ret;
}

static int _cyttsp4_ioctl_start_exclusive(struct cyttsp4_ioctl_data *dio, int timeout)
{
	int ret = 0;

	pr_info("[TPD_Ic]ioctl_start_exclusive [S].\n");

	if ((dio->curr_resource & CY_IC_RES_EXCLUSIVE) == CY_IC_RES_NONE) {
		ret = cyttsp4_request_exclusive(dio->ttsp, timeout);
		if (ret < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Error on request exclusive r=%d\n", __func__, ret);
			return ret;
		}
	}
	dio->curr_resource |= CY_IC_RES_EXCLUSIVE;

	pr_info("[TPD_Ic]ioctl_start_exclusive [E], ret = %d.\n", ret);

	return ret;
}

static int _cyttsp4_ioctl_start_mode(struct cyttsp4_ioctl_data *dio, int mode)
{
	int ret = 0;

	pr_info("[TPD_Ic]ioctl_start_mode [S] mode = %d.\n", mode);

	ret = cyttsp4_request_set_mode(dio->ttsp, mode);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on request set mode r=%d\n", __func__, ret);
		return ret;
	}

	dio->curr_resource |= CY_IC_RES_MODECTRL;

	dio->curr_mode	= mode;

	pr_info("[TPD_Ic]ioctl_start_mode [E], ret = %d.\n", ret);

	return ret;
}

static int _cyttsp4_ioctl_end(struct cyttsp4_ioctl_data *dio, int mode)
{
	int ret = 0;

	pr_info("[TPD_Ic]ioctl_end [S] mode = %d.\n", mode);

	dio->curr_mode	= mode;

	ret = cyttsp4_request_set_mode(dio->ttsp, mode);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on request set mode r=%d\n", __func__, ret);
		return ret;
	}
	dio->curr_resource &= ~CY_IC_RES_MODECTRL;

	ret = cyttsp4_release_exclusive(dio->ttsp);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on request exclusive r=%d\n", __func__, ret);
		return ret;
	}

	dio->curr_resource &= ~CY_IC_RES_EXCLUSIVE;

	pr_info("[TPD_Ic]ioctl_end [E], ret = %d.\n", ret);

	return ret;
}

/*
 *	I2C Read/Write
 */
static int _cyttsp4_ioctl_i2c_read(struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr)
{
	/* i2c_addr is not use... */
	int ret = 0;

	if (use_subaddr != true) {
		command = CY_REG_BASE;			/* = 0x00 */
	}

	_cyttsp4_dbg(dio, CY_LOGLV2,"[TPD_Ic] %s len=%d, cmd=0x%02X \n", __func__, length, command);	/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add */

	ret = cyttsp4_request_direct_read(_DIO_REF_TTSP(dio), (u16)command, buf, length);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio), "%s: Error on adap_read r=%d\n", __func__, ret);
	}

	return ret;
}

static int _cyttsp4_ioctl_i2c_write(struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr)
{
	/* i2c_addr is not use... */
	int ret = 0;

	if (use_subaddr == true) {
		_cyttsp4_dbg(dio, CY_LOGLV2,"[TPD_Ic] %s [1] len=%d, cmd=0x%02X \n", __func__, length, command);	/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add */

		ret = cyttsp4_request_direct_write(_DIO_REF_TTSP(dio), (u16)command, buf, length);
	} else {
		/* buf[0] : address(command), buf[1...] : data */
		_cyttsp4_dbg(dio, CY_LOGLV2,"[TPD_Ic] %s [2] len=%d, cmd=0x%02X \n", __func__, length - 1, buf[0]);	/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add */

		ret = cyttsp4_request_direct_write(_DIO_REF_TTSP(dio), (u16)buf[0], &buf[1], length - 1);
	}

	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on adap_write r=%d\n", __func__, ret);
	}

	return ret;

}

static int _cyttsp4_ioctl_read_block_data(struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr)
{
	/* _cyttsp4_read_block_data */
	int ret = 0;

	if ((buf == NULL) || (length == 0)) {
		dev_err( _DIO_REF_DEV(dio), "%s: pointer or length error buf=%p length=%d\n", __func__, buf, length);
		ret = -EINVAL;
	} else {
		ret = _cyttsp4_ioctl_i2c_read(dio, command, length, buf, i2c_addr, use_subaddr);
		if (ret < 0) {
			dev_err( _DIO_REF_DEV(dio), "%s: bus read block data fail (ret=%d)\n", __func__, ret);
		}
	}

	return ret;
}

static int _cyttsp4_ioctl_write_block_data(struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr)
{
	/* _cyttsp4_write_block_data */
	int ret = 0;

	if ((buf == NULL) || (length == 0)) {
		dev_err( _DIO_REF_DEV(dio), "%s: pointer or length error buf=%p length=%d\n", __func__, buf, length);
		ret = -EINVAL;
	} else {
		ret = _cyttsp4_ioctl_i2c_write(dio, command, length, buf, i2c_addr, use_subaddr);
		if (ret < 0) {
			dev_err( _DIO_REF_DEV(dio), "%s: bus write block data fail (ret=%d)\n", __func__, ret);
		}
	}

	return ret;
}

static int _cyttsp4_ioctl_write(struct cyttsp4_ioctl_data *dio, u8 command, size_t length, u8 *buf, int i2c_addr, bool use_subaddr)
{
	/* call from IOCTL_I2C_WRITE use only */
	int ret = 0;

/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_WRITE_REG_BASE add start */
	if ((use_subaddr != true) && (length == 1) && (buf[0] == 0x00)) {
		/* W 24 00 */
		/* Touch-IC status reading after FIRMWRITERESET from MC. */
		/* "W 24 00" don't write to Touch-IC, because "length == 0" is -EINVAL(cyttsp4_i2c). */
		pr_info("[TPD_Ic] %s, W 24 00.\n", __func__);
		return 0;
	}
/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_WRITE_REG_BASE add end */

	ret = _cyttsp4_ioctl_write_block_data(dio, command, length, buf, i2c_addr, use_subaddr);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error w blk r=%d\n", __func__, ret);
		goto ioctl_write_exit;
	}

	if (dio->chk_bootloadermode_ctrl == true) {
		if (_cyttsp4_ioctl_chk_blmode_exit(dio, command, length, buf, use_subaddr) == true) {
			/* BL Mode exit cmd is found, it is necessary to wait OPE-Mode. */
			/* Because mkcmd is waiting for Sysinfo-Mode. */
			dio->chk_bootloadermode_ctrl = false;

			/* Prayer 3 sec */
			msleep( 3000 );

			_cyttsp4_dbg(dio, CY_LOGLV2,"[TPD_Ic] %s [1]\n", __func__);

			dio->curr_resource = CY_IC_RES_NONE;
			/* Change to Sysinfo-Mode. */
			ret = _cyttsp4_ioctl_start(dio, CY_MODE_SYSINFO, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
			if (ret < 0) {
				dev_err( _DIO_REF_DEV(dio) , "%s: Error on request exclusive r=%d\n", __func__, ret);
				goto ioctl_write_exit;
			}
		}
	}

ioctl_write_exit:

	return ret;
}


/*
 * sub function
 */
#if 0
__sub___________(){}
#endif

static int _cyttsp4_ioctl_get_updatefirminfo(struct cyttsp4_ioctl_data *dio, struct fj_touch_firmware_data *fwinfo)
{
	/* IOCTL_GET_UPDATEFIRMINFO */
	int ret = 0;
	struct cyttsp4_touch_hardware_info hwinfo;
	struct cyttsp4_cydata *cydata = NULL;			/* FUJITSU:2013-06-13 HMW_TOUCH_INVALID_FW mod */
	unsigned short int			fw_ver = 0;

/* FUJITSU:2013-06-13 HMW_TOUCH_INVALID_FW add start */
	if (!dio->si) {
		ret = -ENODEV;
		dev_err( _DIO_REF_DEV(dio) , "%s: sysinfo pointer error r=%d\n", __func__, ret);
		goto update_firminfo_exit;
	}
	if (!dio->si->si_ptrs.cydata ) {
		ret = -ENODEV;
		dev_err( _DIO_REF_DEV(dio) , "%s: cydata pointer error r=%d\n", __func__, ret);
		goto update_firminfo_exit;
	}
	cydata = dio->si->si_ptrs.cydata;
/* FUJITSU:2013-06-13 HMW_TOUCH_INVALID_FW add end */

	ret = cyttsp4_request_hardware_info(_DIO_REF_TTSP(dio), &hwinfo);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on hardware_info r=%d\n", __func__, ret);
		goto update_firminfo_exit;
	}

	switch ( hwinfo.model_type ) {
	case CY_MODEL_PTN1:
		fwinfo->model_code = TOUCH_MODEL_CODE_HMW;
		break;
/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add start */
	case CY_MODEL_PTN2:
		fwinfo->model_code = TOUCH_MODEL_CODE_SPD;
		break;
	case CY_MODEL_PTN3:			/* Pending...*/
		fwinfo->model_code = TOUCH_MODEL_CODE_ARG;
		break;
/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add end */
	case CY_MODEL_PTN4:
		fwinfo->model_code = TOUCH_MODEL_CODE_LPND;
		break;
	case CY_MODEL_PTN5:
		fwinfo->model_code = TOUCH_MODEL_CODE_CAND;
		break;
	case CY_MODEL_PTN6:
		fwinfo->model_code = TOUCH_MODEL_CODE_THMS;
		break;
	default:
		fwinfo->model_code = TOUCH_MODEL_CODE_DMY;
		break;
	}

	switch ( hwinfo.panel_maker_code ) {
	case CY_VENDOR_ID1:
		fwinfo->makerID = TOUCH_MAKER_ID_JDI;
		break;
	case CY_VENDOR_ID2:
		fwinfo->makerID = TOUCH_MAKER_ID_WIN;
		break;
	case CY_VENDOR_ID3:
		fwinfo->makerID = TOUCH_MAKER_ID_KYO;
		break;
	default:
		fwinfo->makerID = TOUCH_MAKER_ID_DMY;
		break;
	}

	switch ( hwinfo.lcd_maker_id ) {
	case CY_LCDID_TYP0:
		fwinfo->lcdID = TOUCH_LCD_ID_JDI;
		break;
	case CY_LCDID_TYP1:
		fwinfo->lcdID = TOUCH_LCD_ID_SHP;
		break;
	default:
		fwinfo->lcdID = TOUCH_LCD_ID_DMY;
		break;
	}

/* FUJITSU:2013-05-28 HMW_TOUCH_FWVER add start */
	fw_ver = (((cydata->fw_ver_major << 8) & 0xFF00) | cydata->fw_ver_minor);
	fwinfo->revctrl_version		= hwinfo.firmware_version;
	fwinfo->firmware_version	= fw_ver;
	fwinfo->config_version		= hwinfo.config_version;
	fwinfo->project_id			= hwinfo.project_id;
/* FUJITSU:2013-05-28 HMW_TOUCH_FWVER add end */

	_cyttsp4_dbg(dio, CY_LOGLV0,"[TPD_Ic] %s, hi m = %d, tp=%d, lc = %d\n", __func__, hwinfo.model_type, hwinfo.panel_maker_code, hwinfo.lcd_maker_id);
	_cyttsp4_dbg(dio, CY_LOGLV0,"[TPD_Ic] hi mk = %d, trial=%d, fv=0x%X rc=0x%X, cv=0x%X, pj=0x%X\n", hwinfo.lcd_maker_code, hwinfo.trial_type, fw_ver, hwinfo.firmware_version, hwinfo.config_version, hwinfo.project_id);
	_cyttsp4_dbg(dio, CY_LOGLV0,"[TPD_Ic] fi m = %d, tp=%d, lc = %d\n", fwinfo->model_code, fwinfo->makerID, fwinfo->lcdID);

update_firminfo_exit:

	return ret;
}


static int _cyttsp4_ioctl_set_powermode(struct cyttsp4_ioctl_data *dio, enum devicepowerstatus mode)
{
	int retval = 0;

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S], mode = %d\n", __func__, mode);

	switch ( mode ) {
	case DEEPSLEEP:
		pr_info("[TPD_Ic] Enter DEEPSLEEP\n");
		atomic_set( &dio->atm_mc_stat , 0x00);
		retval = cyttsp4_request_set_powermode( _DIO_REF_TTSP(dio) , CY_POWMODE_SUSPEND );
		atomic_set( &dio->atm_mc_stat , 0x01);
		break;

	case WAKEUP:
		pr_info("[TPD_Ic] Enter WAKEUP\n");
		atomic_set( &dio->atm_mc_stat , 0x00);
		retval = cyttsp4_request_set_powermode( _DIO_REF_TTSP(dio) , CY_POWMODE_RESUME );
		atomic_set( &dio->atm_mc_stat , 0x01);
		break;

	case HARDRESET:
		pr_info("[TPD_Ic] Enter HARDRESET\n");

		retval = cyttsp4_request_set_powermode( _DIO_REF_TTSP(dio) , CY_POWMODE_HARDRESET );

		break;

	case SOFTRESET:
		pr_info("[TPD_Ic] Enter SOFTRESET\n");

		retval = cyttsp4_request_set_powermode( _DIO_REF_TTSP(dio) , CY_POWMODE_SOFTRESET );

		break;

	case FIRMWRITERESET:
		pr_info("[TPD_Ic] Enter FIRMWRITERESET\n");


		retval = _cyttsp4_ioctl_start_exclusive( dio , CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Error on request exclusive r=%d\n", __func__, retval);
			goto exit_firmrest;
		}

		{/* based from cyttsp4_upgrade_firmware() */

			cyttsp4_request_stop_wd( _DIO_REF_TTSP(dio) );

			retval = cyttsp4_request_reset( _DIO_REF_TTSP(dio) );
			if (retval < 0) {
				dev_err(_DIO_REF_DEV(dio), "%s: Fail reset device r=%d\n", __func__, retval);
				goto exit_firmrest;
			}
		}

		dio->chk_bootloadermode_ctrl	= true;

exit_firmrest:

		break;
	default:
		retval = -EIO;
		break;
	}

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, retval);
	return retval;
}

static int _cyttsp4_calibrate(struct cyttsp4_ioctl_data *dio)
{
	u8 cmd_dat = 0;
	u8 config_cmd = 0;
	u8 initialize_baselines_param = 0;
	int retval = 0;

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S]\n", __func__);

	retval = _cyttsp4_ioctl_start(dio, CY_MODE_CAT, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT );
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on ioctl start r=%d\n", __func__, retval);
		goto cyttsp4_calibrate_exit;
	}

	pr_info("[TPD_Ic] %s: Calibrate IDACs : Mutual Capacitance Fine \n", __func__);

	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, sizeof(mutual_capacitance_fine_param), &mutual_capacitance_fine_param[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Mutual Capacitance Fine (ret=%d)\n", __func__, retval);
		goto cyttsp4_calibrate_exit;
	}

	config_cmd = CY_CMD_CAT_CALIBRATE_IDACS;

	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, sizeof(config_cmd), &config_cmd, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Calibrate IDACs (ret=%d)\n", __func__, retval);
		goto cyttsp4_calibrate_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_calibrate_exit;
	}

	/* Return data */
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &cmd_dat, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) ,"%s: Calibrate IDACs : I2C error \n", __func__);
		goto cyttsp4_calibrate_exit;
	}

	if (cmd_dat != 0) {
		retval = -EINVAL;
		dev_err( _DIO_REF_DEV(dio) ,"%s: Calibrate IDACs : Mutual Capacitance Fine : Failure 0x%02X \n", __func__, cmd_dat);
		goto cyttsp4_calibrate_exit;
	}
	
	pr_info("[TPD_Ic] %s: Calibrate IDACs : Mutual Capacitance Fine : Success ! \n", __func__);
	pr_info("[TPD_Ic] %s: Calibrate IDACs : Self Capacitance \n", __func__);

	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, sizeof(self_capacitance_param), &self_capacitance_param[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Self Capacitance(ret=%d)\n", __func__, retval);
		goto cyttsp4_calibrate_exit;
	}

	config_cmd = CY_CMD_CAT_CALIBRATE_IDACS;

	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, sizeof(config_cmd), &config_cmd, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Calibrate IDACs (ret=%d)\n", __func__, retval);
		goto cyttsp4_calibrate_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_calibrate_exit;
	}
	
	/* Return data */
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &cmd_dat, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) ,"%s: Calibrate IDACs : I2C error \n", __func__);
		goto cyttsp4_calibrate_exit;
	}

	if (cmd_dat != 0) {
		retval = -EINVAL;
		dev_err( _DIO_REF_DEV(dio) ,"%s: Calibrate IDACs : Self Capacitance : Failure 0x%02X \n", __func__, cmd_dat);
		goto cyttsp4_calibrate_exit;
	}
	
	pr_info("[TPD_Ic] %s: Calibrate IDACs : Self Capacitance : Success ! \n", __func__);
	pr_info("[TPD_Ic] %s: Initialize Baselines\n", __func__);
	pr_info("[TPD_Ic] %s: Initialize : Self scan, Fine mutual scan\n", __func__);

	initialize_baselines_param = CY_FINE_MUTUAL_SCAN | CY_SELF_SCAN;

	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, sizeof(initialize_baselines_param), &initialize_baselines_param, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Writing Self scan, Fine mutual scan (ret=%d)\n", __func__, retval);
		goto cyttsp4_calibrate_exit;
	}

	config_cmd = CY_CMD_CAT_INIT_BASELINES;
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, sizeof(config_cmd), &config_cmd, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Initialize Baselines (ret=%d)\n", __func__, retval);
		goto cyttsp4_calibrate_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_calibrate_exit;
	}

	/* Return data */
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &cmd_dat, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) ,"%s: initialize baselines : I2C error \n", __func__);
		goto cyttsp4_calibrate_exit;
	}

	if (cmd_dat != 0) {
		retval = -EINVAL;
		dev_err( _DIO_REF_DEV(dio) ,"%s: initialize baselines : Failure 0x%02X \n", __func__, cmd_dat);
		goto cyttsp4_calibrate_exit;
	}

	pr_info("[TPD_Ic] %s: initialize baselines : Success ! \n", __func__);

cyttsp4_calibrate_exit:
	if (retval != 0) {
		_cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	} else {
		retval = _cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	}

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, retval);
	return retval;
}

static int cyttsp4_get_crc(struct cyttsp4_ioctl_data *dio, enum cyttsp4_ic_ebid ebid, u8 *crc_h, u8 *crc_l)
{
	int retval = 0;
	u8 write_cmd_reg = 0;
	u8 cmd_dat[3];

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S]\n", __func__);

	memset(cmd_dat, 0, sizeof(cmd_dat));

	/* Command data set */
	write_cmd_reg = ebid;
	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 1, &write_cmd_reg, LOG_I2C_ADRR, true);  /* EBID */
	if (retval < 0) {
		goto cyttsp4_get_crc_exit;
	}

	write_cmd_reg = CY_CMD_OP_GET_CRC;		/* 5 */
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Fail Set CRC command r=%d\n", __func__, retval);
		goto cyttsp4_get_crc_exit;
	}
	
	/* Wait to rise of complete bit */
	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_get_crc_exit;
	}
	
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 3, cmd_dat, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Fail Get CRC status r=%d\n", __func__, retval);
		goto cyttsp4_get_crc_exit;
	}

	if (cmd_dat[0] == 0x01) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Get CRC status Failed", __func__);
		retval = -EINVAL;
		goto cyttsp4_get_crc_exit;
	}
	*crc_h = cmd_dat[1];
	*crc_l = cmd_dat[2];

	_cyttsp4_dbg(dio, CY_LOGLV1, "%s: crc_data h=0x%02X l=0x%02X", __func__, *crc_h, *crc_l );

cyttsp4_get_crc_exit:
	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, retval);
	return retval;
}

static int cyttsp4_opens_calibrate( struct cyttsp4_ioctl_data *dio )
{
	int retval = 0;
	u8 read_cmd_reg = 0;
	u8 write_cmd_reg = 0;

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S]\n", __func__);

	/* calibrate iDac: mutual_capacitance */
	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, sizeof(mutual_capacitance_fine_param), &mutual_capacitance_fine_param[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Mutual Capacitance Fine (ret=%d)\n", __func__, retval);
		goto cyttsp4_opens_calibrate_exit;
	}

	write_cmd_reg = CY_CMD_CAT_CALIBRATE_IDACS;
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Calibrate IDACs (ret=%d)\n", __func__, retval);
		goto cyttsp4_opens_calibrate_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_opens_calibrate_exit;
	}

	/* Return data */
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) ,"%s: Calibrate IDACs : I2C error \n", __func__);
		goto cyttsp4_opens_calibrate_exit;
	}

	if (read_cmd_reg != 0) {
		retval = -EINVAL;
		dev_err( _DIO_REF_DEV(dio) ,"%s: Calibrate IDACs : Mutual Capacitance Fine : Failure 0x%02X \n", __func__, read_cmd_reg);
		goto cyttsp4_opens_calibrate_exit;
	}

	_cyttsp4_dbg(dio, CY_LOGLV2,"%s: Calibrate IDACs : Mutual Capacitance Fine : Success ! \n", __func__);

	/* calibrate iDac: Buttons */
	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, sizeof(mutual_capacitance_buttons_param), &mutual_capacitance_buttons_param[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Mutual Capacitance Buttons (ret=%d)\n", __func__, retval);
		goto cyttsp4_opens_calibrate_exit;
	}

	write_cmd_reg = CY_CMD_CAT_CALIBRATE_IDACS;
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Calibrate IDACs (ret=%d)\n", __func__, retval);
		goto cyttsp4_opens_calibrate_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_opens_calibrate_exit;
	}

	/* Return data */
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) ,"%s: Calibrate IDACs : I2C error \n", __func__);
		goto cyttsp4_opens_calibrate_exit;
	}
	if (read_cmd_reg != 0) {
		retval = -EINVAL;
		dev_err( _DIO_REF_DEV(dio) ,"%s: Calibrate IDACs : Mutual Capacitance Buttons : Failure 0x%02X \n", __func__, read_cmd_reg);
		goto cyttsp4_opens_calibrate_exit;
	}

	_cyttsp4_dbg(dio, CY_LOGLV2,"%s: Calibrate IDACs : Mutual Capacitance Buttons : Success ! \n", __func__);

cyttsp4_opens_calibrate_exit:

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, retval);

	return retval;
}


static int cyttsp4_opens_retrieve( struct cyttsp4_ioctl_data *dio, struct fj_touch_do_selftest_data *data, unsigned char *self_test_data, struct second_read_selftest_data *second_data, unsigned char *second_self_test_data )
{
	int retval = 0;
	u8 read_cmd_reg = 0;
	u8 write_cmd_reg = 0;
	unsigned char sensor_palam[5];	/*  = { 0, 0, 0, CY_SENSORS_INTERSECTION, 0 }; */

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S]\n", __func__);

	memset(sensor_palam, 0, sizeof(sensor_palam));
	
	/* retrieve_data structure parameter set */
	sensor_palam[0] = ( ( data->load_offset >> 8 ) & 0xFF );
	sensor_palam[1] = (   data->load_offset & 0xFF );
	sensor_palam[2] = ( ( data->load_length >> 8 ) & 0xFF );
	sensor_palam[3] = (   data->load_length & 0xFF );
	sensor_palam[4] = 0;

	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 5, sensor_palam, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Retrieve_data structure of Sensor (ret=%d)\n", __func__, retval);
		goto cyttsp4_opens_retrieve_exit;
	}

	write_cmd_reg = CY_CMD_CAT_RETRIEVE_DATA_STRUCTURE;		/* 0x10 */
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Retrieve_data structure of Sensor (ret=%d)\n", __func__, retval);
		goto cyttsp4_opens_retrieve_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_opens_retrieve_exit;
	}
	
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [1] (ret=%d)\n", __func__, retval);
		goto cyttsp4_opens_retrieve_exit;
	}
	if (read_cmd_reg != 0) {
		retval = -EINVAL;
		dev_err( _DIO_REF_DEV(dio) ,"%s: Retrieve_data structure : Sensor : Failure 0x%02X \n", __func__, read_cmd_reg);
		goto cyttsp4_opens_retrieve_exit;
	}
	
	if(data->load_length > CY_QUALCOMM_I2C_LIMIT_HALF) {
		
		retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, CY_QUALCOMM_I2C_LIMIT_HALF, self_test_data, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [2] (ret=%d)\n", __func__, retval);
			goto cyttsp4_opens_retrieve_exit;
		}

		retval = _cyttsp4_ioctl_read_block_data(dio, second_data->load_offset, second_data->load_length, second_self_test_data, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [3] (ret=%d)\n", __func__, retval);
			goto cyttsp4_opens_retrieve_exit;
		}
		/* Concatenate second read data to first read data */
		memcpy(&self_test_data[CY_QUALCOMM_I2C_LIMIT_HALF], second_self_test_data, second_data->load_length);

	} else {
		retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, data->load_length, self_test_data, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [4] (ret=%d)\n", __func__, retval);
			goto cyttsp4_opens_retrieve_exit;
		}
	}
	
	_cyttsp4_dbg(dio, CY_LOGLV2,"%s: Retrieve_data structure : Sensor : Success! \n", __func__);

cyttsp4_opens_retrieve_exit:

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, retval);

	return retval;
}
	
static int cyttsp4_auto_mask( struct cyttsp4_ioctl_data *dio, char *pin_mask )
{
	int retval = 0;
	u8 read_cmd_reg = 0;
	unsigned char tmd_mask[9] =    { 0x3C, 0xC0, 0xFF, 0x03, 0xFC, 0x3F, 0xC0, 0x07, 0x00 };		/* fixed pin mask is error... */
	unsigned char wintek_mask[9] = { 0x3E, 0xC0, 0xFF, 0x03, 0xFC, 0x3F, 0xC0, 0x03, 0x00 };		/* fixed pin mask is error... */

	struct cyttsp4_sysinfo_data sysinfo_offset;
	size_t ddata_ofs = 0;

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S]\n", __func__);

	memset( &sysinfo_offset, 0, sizeof(sysinfo_offset) );

	retval = _cyttsp4_ioctl_start_mode(dio , CY_MODE_SYSINFO);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on request set mode r=%d\n", __func__, retval);
		goto cyttsp4_auto_mask_exit;
	}

	retval = _cyttsp4_ioctl_read_block_data(dio, CY_REG_BASE, sizeof(struct cyttsp4_sysinfo_data), (u8 *)&sysinfo_offset, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: fail read sysinfo data r=%d\n", __func__, retval);
		goto cyttsp4_auto_mask_exit;
	}

	ddata_ofs = (sysinfo_offset.ddata_ofsh * 256) + sysinfo_offset.ddata_ofsl;

	retval = _cyttsp4_ioctl_read_block_data(dio, ddata_ofs, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: fail read ddata r=%d\n", __func__, retval);
		goto cyttsp4_auto_mask_exit;
	}
	_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Panel = 0x%02X\n", __func__, read_cmd_reg);

	if (read_cmd_reg == 0x01) {
		memcpy( pin_mask, tmd_mask, sizeof(tmd_mask) );
		_cyttsp4_dbg(dio, CY_LOGLV0, "%s: ERROR : Auto pin mask set : TMD panel\n", __func__);
		retval = -1;
	} else if (read_cmd_reg == 0x02) {
		memcpy( pin_mask, wintek_mask, sizeof(wintek_mask) );
		_cyttsp4_dbg(dio, CY_LOGLV0, "%s: ERROR : Auto pin mask set : wintek panel\n", __func__);
		retval = -1;
	}

cyttsp4_auto_mask_exit:

	retval = _cyttsp4_ioctl_start_mode(dio , CY_MODE_CAT);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on request set mode r=%d\n", __func__, retval);
	}

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, retval);

	return retval;
}


static int cyttsp4_selftest_idac(struct cyttsp4_ioctl_data *dio, struct fj_touch_do_selftest_data *data, unsigned char *self_test_data, struct second_read_selftest_data *second_data, unsigned char *second_self_test_data)
{
	int retval = 0;
	u8 read_cmd_reg = 0;
	u8 write_cmd_reg = 0;
	unsigned char sensor_palam[5];
	
	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S]\n", __func__);

	memset(sensor_palam, 0, sizeof(sensor_palam));

	/* retrieve_data structure parameter set */
	sensor_palam[0] = ( ( data->load_offset >> 8 ) & 0xFF );
	sensor_palam[1] = (   data->load_offset & 0xFF );
	sensor_palam[2] = ( ( data->load_length >> 8 ) & 0xFF );
	sensor_palam[3] = (   data->load_length & 0xFF );
	/* IDAC 0:Mutual 1:Self 2:alternate  start */
	sensor_palam[4] = data->test_cmd;
	if (sensor_palam[4] == 2) {
		sensor_palam[4] = 4;	/* alternate: 2 -> 4 cmd change */
	}
	_cyttsp4_dbg(dio, CY_LOGLV2,"%s: IDAC_Flag : 0x%02x, sensor_palam[4] : %d \n", __func__, data->idac_flag, sensor_palam[4]);
	/* IDAC 0:Mutual 1:Self 2:alternate  end */

	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 5, sensor_palam, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Retrieve_data structure of Sensor (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_idac_exit;
	}

	write_cmd_reg = CY_CMD_CAT_RETRIEVE_DATA_STRUCTURE;		/* 0x10 */
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Retrieve_data structure of Sensor (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_idac_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_selftest_idac_exit;
	}
	
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [1] (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_idac_exit;
	}
	if (read_cmd_reg != 0) {
		retval = -EINVAL;
		dev_err( _DIO_REF_DEV(dio) ,"%s: Retrieve_data structure : Sensor : Failure 0x%02X \n", __func__, read_cmd_reg);
		goto cyttsp4_selftest_idac_exit;
	}
	
	if(data->load_length > CY_QUALCOMM_I2C_LIMIT_HALF) {
		
		retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, CY_QUALCOMM_I2C_LIMIT_HALF, self_test_data, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [2] (ret=%d)\n", __func__, retval);
			goto cyttsp4_selftest_idac_exit;
		}

		retval = _cyttsp4_ioctl_read_block_data(dio, second_data->load_offset, second_data->load_length, second_self_test_data, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [3] (ret=%d)\n", __func__, retval);
			goto cyttsp4_selftest_idac_exit;
		}

		/* Concatenate second read data to first read data */
		memcpy(&self_test_data[CY_QUALCOMM_I2C_LIMIT_HALF], second_self_test_data, second_data->load_length);

	} else {
		retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, data->load_length, self_test_data, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [4](ret=%d)\n", __func__, retval);
			goto cyttsp4_selftest_idac_exit;
		}
	}
	
	_cyttsp4_dbg(dio, CY_LOGLV2,"%s: Retrieve_data structure : Sensor : Success! \n", __func__);
	
cyttsp4_selftest_idac_exit:
	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, retval);
	return retval;
}


static int cyttsp4_selftest( struct cyttsp4_ioctl_data *dio, struct fj_touch_do_selftest_data *data,
	unsigned char *pin_mask, unsigned char *result_data_buf, unsigned char *self_test_data)
{
	int retval = 0;
	u8 read_cmd_reg = 0;
	u8 write_cmd_reg = 0;
	unsigned char test_palam[6];
	int count = 0;
	int mask_offset = 0;
	struct second_read_selftest_data sr_selftest_data;
	unsigned char *second_self_test_data = NULL; 	/* second read selftest access	*/

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S]\n", __func__);
	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] idac = %d, cmd = %d, id = %d, ofst = %d, len = %d", data->idac_flag, data->test_cmd, data->test_id, data->load_offset, data->load_length);
	
	memset(test_palam, 0, sizeof(test_palam));
	memset(&sr_selftest_data, 0, sizeof(sr_selftest_data));

	retval = _cyttsp4_ioctl_start(dio, CY_MODE_CAT, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT );
	if (retval < 0) {
		dev_err(_DIO_REF_DEV(dio), "%s: Error on ioctl start r=%d\n", __func__, retval);
		goto cyttsp4_selftest_exit;
	}

	if(data->load_length > CY_QUALCOMM_I2C_LIMIT_HALF) {
		sr_selftest_data.load_length = data->load_length - CY_QUALCOMM_I2C_LIMIT_HALF;
		second_self_test_data = kzalloc(sr_selftest_data.load_length, GFP_KERNEL);
		if (second_self_test_data == NULL) {
			retval = -EINVAL;
			dev_err( _DIO_REF_DEV(dio) , "second_self_test_data kzalloc fail %d \n", retval);
			goto cyttsp4_selftest_exit;
		}
		memset(second_self_test_data, 0xFF, sr_selftest_data.load_length);
	}

	sr_selftest_data.load_offset = 0x08 + CY_QUALCOMM_I2C_LIMIT_HALF;

	if ((data->idac_flag & 0xFF) == 0x01) { /* IDAC */
		retval = cyttsp4_selftest_idac(dio, data, self_test_data, &sr_selftest_data, second_self_test_data);
		if (retval < 0) {
			result_data_buf[0] = 1;
		} else {
			result_data_buf[0] = 0;
			result_data_buf[1] = 0;
			result_data_buf[2] = 1;
		}
		goto cyttsp4_selftest_exit;
	}

	/* Command data set */
	test_palam[0] = ((data->load_offset >> 8) & 0xFF);
	test_palam[1] = (data->load_offset & 0xFF);
	test_palam[2] = ((data->load_length >> 8) & 0xFF);
	test_palam[3] = (data->load_length & 0xFF);
	test_palam[4] = (u8)(data->test_cmd & 0xFF);

	if (test_palam[4] != 0x04) {
		/* pin mask all zero */
		for (count = 0; count < 9; count++) {
			if (pin_mask[count] != 0) {
				break;
			}
		}

		if (count == 9) {
			retval = cyttsp4_auto_mask(dio, pin_mask);
			if (retval < 0) {
				goto cyttsp4_selftest_exit;
			}
		}

		if ((data->test_cmd & 0xFF) == 0x03 || (data->test_cmd & 0xFF) == 0x01) { /* Opens Test or BIST */
			retval = cyttsp4_opens_calibrate(dio);
			if (retval < 0) {
				goto cyttsp4_selftest_exit;
			}

			retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 5, test_palam, LOG_I2C_ADRR, true);
			pr_info("%s: parameter 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
					__func__, test_palam[0], test_palam[1], test_palam[2], test_palam[3], test_palam[4]);
			mask_offset = 0x08;
		} else {
			test_palam[5] = (u8)(data->test_id & 0xFF);
			retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 6, test_palam, LOG_I2C_ADRR, true);
			pr_info("%s: parameter 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", __func__,
					test_palam[0], test_palam[1], test_palam[2], test_palam[3], test_palam[4], test_palam[5]);
			mask_offset = 0x09;
		}

		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Load Self-test params (ret=%d)\n", __func__, retval);
			goto cyttsp4_selftest_exit;
		}

		pr_info("%s: pin mask 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
				__func__, pin_mask[0], pin_mask[1], pin_mask[2], pin_mask[3], pin_mask[4],
				pin_mask[5], pin_mask[6], pin_mask[7], pin_mask[8]);

		retval = _cyttsp4_ioctl_write_block_data(dio, mask_offset, 9, (u8*)pin_mask, LOG_I2C_ADRR, true);

		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write pin mask (ret=%d)\n", __func__, retval);
			goto cyttsp4_selftest_exit;
		}

		/* Load Self-test data */
		write_cmd_reg = CY_CMD_CAT_LOAD_SELF_TEST_DATA;
		retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Load Self-test command (ret=%d)\n", __func__, retval);
			goto cyttsp4_selftest_exit;
		}

		retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
		if(retval < 0){
			goto cyttsp4_selftest_exit;
		}

		/* status_data get */
		retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed read Load Self-test returns (ret=%d)\n", __func__, retval);
			goto cyttsp4_selftest_exit;
		}

		switch (read_cmd_reg) {
		case 0x01:
			dev_err( _DIO_REF_DEV(dio) , "%s: Load Self-test Failure\n", __func__);
			retval = -EINVAL;
			goto cyttsp4_selftest_exit;
		case 0xFF:
			dev_err( _DIO_REF_DEV(dio) , "%s: Command unsupported\n", __func__);
			retval = -EINVAL;
			goto cyttsp4_selftest_exit;
		default :
			pr_info("%s: Load Self-test Success (status=0x%02X)\n", __func__, read_cmd_reg);
			break;
		}
	}

	/* Command data set */
	write_cmd_reg = test_palam[4];
	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 1, &write_cmd_reg, LOG_I2C_ADRR, true);	 /* Self-Test ID */
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Run Self-test params (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_exit;
	}

	/* Run Self-test */
	write_cmd_reg = CY_CMD_CAT_RUN_SELF_TEST;			/* 0x07 */
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Run Self-test Command (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_selftest_exit;
	}

	/* status data get */
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 3, result_data_buf, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed read Run Self-test returns (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_exit;
	}

	switch (result_data_buf[0]) {
	case 0x01:
		dev_err( _DIO_REF_DEV(dio) , "%s: Run Self-test Failure\n", __func__);
		retval = -EINVAL;
		goto cyttsp4_selftest_exit;
	case 0xFF:
		dev_err( _DIO_REF_DEV(dio) , "%s: Command unsupported\n", __func__);
		retval = -EINVAL;
		goto cyttsp4_selftest_exit;
	default :
		_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Run Self-test Success (status=0x%02X)\n", __func__, result_data_buf[0]);
		break;
	}

	/* Command data set */
	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 5, test_palam, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Get Self-test Results params (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_exit;
	}

	/* Get Self-test Results */
	write_cmd_reg = CY_CMD_CAT_GET_SELF_TEST_RESULT;		/* 0x08 */
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Get Self-test Results command (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_exit;
	}

	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
	if (retval < 0) {
		goto cyttsp4_selftest_exit;
	}

	/* self_test_data get */
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Get Self-test Results returns (ret=%d)\n", __func__, retval);
		goto cyttsp4_selftest_exit;
	}
	switch (read_cmd_reg) {
	case 0x01:
		dev_err( _DIO_REF_DEV(dio) , "%s: Get Self-test Results Failure\n", __func__);
		retval = -EINVAL;
		goto cyttsp4_selftest_exit;
	case 0xFF:
		dev_err( _DIO_REF_DEV(dio) , "%s: Command unsupported\n", __func__);
		retval = -EINVAL;
		goto cyttsp4_selftest_exit;
	default :
		_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Get Self-test Results Success (status=0x%02X)\n", __func__, read_cmd_reg);
		break;
	}

	if ((data->test_cmd & 0xFF) == 0x03 || (data->test_cmd & 0xFF) == 0x01) { /* Opens Test or BIST */
		retval = cyttsp4_opens_retrieve(dio, data, self_test_data, &sr_selftest_data, second_self_test_data);
		if (retval < 0) {
			cyttsp4_opens_calibrate(dio);
		} else {
			retval = cyttsp4_opens_calibrate(dio);
			if (retval == 0) {
				result_data_buf[1] = 0;
				result_data_buf[2] = 1;
			}
		}
	} else {
		if(data->load_length > CY_QUALCOMM_I2C_LIMIT_HALF) {
			retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, CY_QUALCOMM_I2C_LIMIT_HALF, self_test_data, LOG_I2C_ADRR, true);
			if (retval < 0) {
				dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [1] (ret=%d)\n", __func__, retval);
				goto cyttsp4_selftest_exit;
			}

			retval = _cyttsp4_ioctl_read_block_data(dio, sr_selftest_data.load_offset, sr_selftest_data.load_length, second_self_test_data, LOG_I2C_ADRR, true);
			if (retval < 0) {
				dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [2] (ret=%d)\n", __func__, retval);
				goto cyttsp4_selftest_exit;
			}
			/* Concatenate second read data to first read data */
			memcpy(&self_test_data[CY_QUALCOMM_I2C_LIMIT_HALF], second_self_test_data, sr_selftest_data.load_length);
		} else {
			retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, data->load_length, self_test_data, LOG_I2C_ADRR, true);
			if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve_data structure of Sensor [3] (ret=%d)\n", __func__, retval);
			goto cyttsp4_selftest_exit;
			}
		}
	}

cyttsp4_selftest_exit:
	if(second_self_test_data != NULL) {
		kfree(second_self_test_data);
	}

	if (retval != 0) {
		_cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	} else {
		retval = _cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	}

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, retval);
	return retval;
}

/*
 * raw check
 */
static int cyttsp4_raw_check( struct cyttsp4_ioctl_data *dio, struct fj_touch_debug_diagnostic_data *data, unsigned char *test_cmd)
{
	int retval = 0;
	u8 read_cmd_reg = 0;
	u8 write_cmd_reg = 0;
	unsigned char raw_return[5];
	unsigned char *return_data = NULL;
	unsigned char *second_return_data = NULL;
	unsigned int number_of_element = 0;
	int multi_scan = data->multi_scan;
/* FUJITSU:2013-03-18 16bit_FW_IO add start */
	unsigned char rpc_cmd[CY_DIAG_RPC_CMD_SIZE];
	int size_wk, read_size, buf_point;
	int cnt, max;
	u16 offset_adr;
	u8  element_size;
/* FUJITSU:2013-03-18 16bit_FW_IO add end */

	pr_info("[TPD_Ic] cyttsp4_raw_check. scan = %d\n", multi_scan);

	memset(raw_return, 0, sizeof(raw_return));

	switch ( multi_scan ) {
	case MULTI_INITIAL:
		/* change to CONFIG MODE */
		/* Enter to Config And Test mode... */
		retval = _cyttsp4_ioctl_start(dio, CY_MODE_CAT, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT );	
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Error on ioctl start r=%d\n", __func__, retval);
			return retval;
		}

		break;
	case MULTI_FINAL:
		/* change OPERATING MODE */
		/* Exit from Config And Test mode... */
		retval = _cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Error on ioctl end r=%d\n", __func__, retval);
			return retval;
		}
		break;

	case MULTI_UPDATE:

		if ((dio->curr_mode != CY_MODE_CAT) || (_cyttsp4_ioctl_IsCoreSleep(dio) == true)) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Cannot I2C RawCheck MULTI_UPDATE\n", __func__);
			retval = -EFAULT;
			goto cyttsp4_raw_check_exit;
		}

		/* Execute Panel Scan */
		write_cmd_reg = CY_CMD_CAT_EXEC_PANEL_SCAN;
		retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Execute Panel Scan Command (ret=%d)\n", __func__, retval);
			goto cyttsp4_raw_check_exit;
		}

		/* wait complete */
		retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
		if (retval < 0) {
			goto cyttsp4_raw_check_exit;
		}

		/* Check Result Data */
		retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &read_cmd_reg, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Execute Panel Scan Returns (ret=%d)\n", __func__, retval);
			goto cyttsp4_raw_check_exit;
		}
		if (read_cmd_reg != 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Execute Panel Scan Failure (status=0x%02X)\n", __func__, read_cmd_reg);
			retval = -EINVAL;
			goto cyttsp4_raw_check_exit;
		}

		_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Execute Panel Scan Success\n", __func__);

		/* Command data set */
		retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 5, test_cmd, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Retrieve Panel Scan params (ret=%d)\n", __func__, retval);
			goto cyttsp4_raw_check_exit;
		}

		/* Retrieve Panel Scan */
		write_cmd_reg = CY_CMD_CAT_RETRIEVE_PANEL_SCAN;
		retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Retrieve Panel Scan Command (ret=%d)\n", __func__, retval);
			goto cyttsp4_raw_check_exit;
		}
		/* wait complete */
		retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
		if (retval < 0) {
			goto cyttsp4_raw_check_exit;
		}

		/* Get Result Data */
		retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, sizeof(raw_return), raw_return, LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve Panel Scan returns (ret=%d)\n", __func__, retval);
			goto cyttsp4_raw_check_exit;
		}
		data->status_code = raw_return[0];
		data->data_ID_spec_type = raw_return[1];
		data->num_ele1 = raw_return[2];
		data->num_ele2 = raw_return[3];
		data->data_format = raw_return[4];

		if (raw_return[0] & 0x01) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Retrieve Panel Scan Failure (status=0x%02X)\n", __func__, raw_return[0]);
			retval = -EINVAL;
			goto cyttsp4_raw_check_exit;
		}
/* FUJITSU:2013-03-18 16bit_FW_IO add start */
		element_size = data->data_format & CY_DIAG_ELEMENTSIZE_MASK;

		if (element_size == CY_DIAG_ELEMENTSIZE_16BIT) {
			pr_info("[TPD_Ic] %s: Data Format (element_size=%d)\n", __func__, element_size);

			number_of_element = CY_DIAG_NUM_OF_ELEMENT * element_size;
			_cyttsp4_dbg(dio, CY_LOGLV2, "%s: number_of_element = %d(Data Element Size=%d)\n",
				           __func__, number_of_element, element_size);

			/* User Spase limit length */
			size_wk = ((test_cmd[2] * 256) + test_cmd[3]) * element_size;
			if (number_of_element > size_wk){
				number_of_element = size_wk;
				_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Size_Limit(%d)\n", __func__, number_of_element);
			}

			/* Get Kernel space memory, Don't use User Spase to the I2C */
			return_data = kzalloc(number_of_element + 1, GFP_KERNEL);
			if (return_data == NULL) {
				retval = -EINVAL;
				dev_err( _DIO_REF_DEV(dio) , "return_data kzalloc fail %d \n", retval);
				goto cyttsp4_raw_check_exit;
			}
			memset( return_data, 0xFF, number_of_element + 1 );

			/* Get Retrieved Data */
			rpc_cmd[2] = (CY_DIAG_NUM_OF_ELEMENT & 0xFF00) >> 8;
			rpc_cmd[3] = (CY_DIAG_NUM_OF_ELEMENT & 0x00FF);
			rpc_cmd[4] = test_cmd[4];
			buf_point  = 0;
			offset_adr = 0;
			read_size  = CY_DIAG_RAW_DATA_SIZE_MAX;
			max        = number_of_element / CY_DIAG_RAW_DATA_SIZE_MAX;
			if ((number_of_element % CY_DIAG_RAW_DATA_SIZE_MAX) != 0){
				max++;
			}

			pr_info("[TPD_Ic] %s: max=%d, Cmd=0x%02X.\n", __func__, max, rpc_cmd[4]);

			for (cnt = 0; cnt < max; cnt++){
				_cyttsp4_dbg(dio, CY_LOGLV2, "%s: read_point = %d, offset_adr = %d\n",
								__func__, buf_point, offset_adr);
				/* Retrieve Panel Scan */
				rpc_cmd[0] = (offset_adr & 0xFF00) >> 8;
				rpc_cmd[1] = (offset_adr & 0x00FF);
				retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, CY_DIAG_RPC_CMD_SIZE, (u8 *)&rpc_cmd, LOG_I2C_ADRR, true);
				if (retval < 0) {
					dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Retrieve Panel Scan Command (ret=%d)\n", __func__, retval);
					goto multi_update_exit;
				}

				msleep(20);
				retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &write_cmd_reg, LOG_I2C_ADRR, true);
				if (retval < 0) {
					dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Retrieve Panel Scan Command (ret=%d)\n", __func__, retval);
					goto multi_update_exit;
				}
				/* wait complete */
				retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_TOUCH);
				if (retval < 0) {
					goto multi_update_exit;
				}

				if (cnt == (max - 1)){
					read_size = (number_of_element % CY_DIAG_RAW_DATA_SIZE_MAX) + 1;
				}
				retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, read_size, &return_data[buf_point], LOG_I2C_ADRR, true);
				if (retval < 0) {
					dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve Panel Scan returns (ret=%d)\n", __func__, retval);
					goto multi_update_exit;
				}

				offset_adr += (read_size / element_size);
				buf_point  += read_size;
			}
		} else {
/* FUJITSU:2013-03-18 16bit_FW_IO add end */
			number_of_element = (( data->num_ele1 * 256 ) + data->num_ele2 );

			if (( 3 <= data->data_ID_spec_type ) && ( data->data_ID_spec_type <= 5 )) {
				number_of_element = number_of_element * 2;
				data->num_ele1 = ( ( number_of_element >> 8 ) & 0xFF );
				data->num_ele2 = (   number_of_element        & 0xFF );
				_cyttsp4_dbg(dio, CY_LOGLV2, "%s: number_of_element * 2 = %d\n", __func__, number_of_element);
			}
			
			/* User Spase limit length */
			if( number_of_element > (( test_cmd[2] * 256 ) + test_cmd[3]) ){
				number_of_element = (( test_cmd[2] * 256 ) + test_cmd[3]);
			}
			
			/* Get Kernel space memory, Don't use User Spase to the I2C */
			return_data = kzalloc(number_of_element, GFP_KERNEL);
			if (return_data == NULL) {
				retval = -EINVAL;
				dev_err( _DIO_REF_DEV(dio) , "return_data kzalloc fail %d \n", retval);
				goto cyttsp4_raw_check_exit;
			}
			memset( return_data, 0xFF, number_of_element );
			
			/* Get Retrieved Data */
			if (number_of_element > CY_QUALCOMM_I2C_LIMIT_HALF) {
				
				second_return_data = kzalloc((number_of_element - CY_QUALCOMM_I2C_LIMIT_HALF), GFP_KERNEL);
				if (second_return_data == NULL) {
					retval = -EINVAL;
					dev_err( _DIO_REF_DEV(dio) , "return_data kzalloc fail %d \n", retval);
					goto multi_update_exit;		/* FUJITSU:2012-11-20 mod */
				}
				
				memset( second_return_data, 0xFF, (number_of_element - CY_QUALCOMM_I2C_LIMIT_HALF) );
				retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, CY_QUALCOMM_I2C_LIMIT_HALF, return_data, LOG_I2C_ADRR, true);
				if (retval < 0) {
					dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve Panel Scan returns (ret=%d)\n", __func__, retval);
					goto multi_update_exit;
				}
				retval = _cyttsp4_ioctl_read_block_data(dio, (0x08 + CY_QUALCOMM_I2C_LIMIT_HALF), (number_of_element - CY_QUALCOMM_I2C_LIMIT_HALF), second_return_data, LOG_I2C_ADRR, true);
				if (retval < 0) {
					dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve Panel Scan returns (ret=%d)\n", __func__, retval);
					goto multi_update_exit;
				}
				/* Concatenate second read data to first read data */
				memcpy(&return_data[CY_QUALCOMM_I2C_LIMIT_HALF], second_return_data, (number_of_element - CY_QUALCOMM_I2C_LIMIT_HALF));
			} else {
				retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, number_of_element, return_data, LOG_I2C_ADRR, true);
				if (retval < 0) {
					dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve Panel Scan returns (ret=%d)\n", __func__, retval);
					goto multi_update_exit;
				}
			}
/* FUJITSU:2013-03-18 16bit_FW_IO add start*/
		}
/* FUJITSU:2013-03-18 16bit_FW_IO add end */

		retval = copy_to_user( data->data, return_data, number_of_element );
		if (retval != 0) {
			retval = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_to_user err %d \n", retval);
		}
		
multi_update_exit:
		if (return_data != NULL) {
			kfree( return_data );
		}
		if (second_return_data != NULL) {
			kfree( second_return_data );
		}

cyttsp4_raw_check_exit:

		break;

	default:
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Multi scan mode \n", __func__);
		retval = -EINVAL;
		break;
	}

	return retval;
}

static int cyttsp4_get_firmversion(struct cyttsp4_ioctl_data *dio, u8 *verdata)
{
	size_t cydata_ofs = 0;
	size_t cydata_size = 0;
	size_t ddata_ofs = 0;
	size_t ddata_size = 0;
	size_t pcfg_ofs = 0;
	size_t pcfg_size = 0;
	int ret = 0;
	struct cyttsp4_sysinfo_data sysinfo_offset;

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [S]\n", __func__);

	memset( &sysinfo_offset, 0, sizeof(sysinfo_offset) );

	ret = _cyttsp4_ioctl_start(dio, CY_MODE_SYSINFO, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT );
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Error on ioctl start r=%d\n", __func__, ret);
		return ret;
	}

	ret = _cyttsp4_ioctl_read_block_data(dio, CY_REG_BASE, sizeof(struct cyttsp4_sysinfo_data), (u8 *)&sysinfo_offset, LOG_I2C_ADRR, true);
	if (ret < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Fail read cmd regs r=%d\n", __func__, ret);
		goto cyttsp4_get_firmversion_exit;
	}

	cydata_ofs = (sysinfo_offset.cydata_ofsh * 256) + sysinfo_offset.cydata_ofsl;
	cydata_size = ((sysinfo_offset.test_ofsh * 256) + sysinfo_offset.test_ofsl ) - cydata_ofs;

	pcfg_ofs = (sysinfo_offset.pcfg_ofsh * 256) + sysinfo_offset.pcfg_ofsl;
	pcfg_size = ((sysinfo_offset.opcfg_ofsh * 256) + sysinfo_offset.opcfg_ofsl ) - pcfg_ofs;

	ddata_ofs = (sysinfo_offset.ddata_ofsh * 256) + sysinfo_offset.ddata_ofsl;
	ddata_size = ((sysinfo_offset.mdata_ofsh * 256) + sysinfo_offset.mdata_ofsl ) - ddata_ofs;

	_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: cydata_ofs = %d cydata_size = %d pcfg_ofs = %d pcfg_size = %d ddata_ofs = %d ddata_size = %d\n",
				__func__, cydata_ofs, cydata_size, pcfg_ofs, pcfg_size, ddata_ofs, ddata_size);

	ret = _cyttsp4_ioctl_read_block_data(dio, cydata_ofs, cydata_size, (u8 *)&verdata[0], LOG_I2C_ADRR, true);
	if (ret < 0){
		dev_err( _DIO_REF_DEV(dio) , "%s: Fail read cmd regs r=%d\n", __func__, ret);
		goto cyttsp4_get_firmversion_exit;
	}

	
	ret = _cyttsp4_ioctl_read_block_data(dio, pcfg_ofs + pcfg_size - 1, 1, (u8 *)&verdata[37], LOG_I2C_ADRR, true);
	if (ret < 0){
		dev_err( _DIO_REF_DEV(dio) , "%s: Fail read cmd regs r=%d\n", __func__, ret);
		goto cyttsp4_get_firmversion_exit;
	}

	ret = _cyttsp4_ioctl_read_block_data(dio, ddata_ofs, ddata_size, &verdata[38], LOG_I2C_ADRR, true);
	if (ret < 0){
		dev_err( _DIO_REF_DEV(dio) , "%s: Fail read cmd regs r=%d\n", __func__, ret);
		goto cyttsp4_get_firmversion_exit;
	}


cyttsp4_get_firmversion_exit:

	if (ret != 0) {
		_cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	} else {
		ret = _cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
		if (ret < 0) {
			goto cyttsp4_get_firm_crc_exit;
		}
		ret = cyttsp4_get_crc(dio, CY_TCH_PARM_EBID, &verdata[34], &verdata[35]);
	}
	
cyttsp4_get_firm_crc_exit:

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s [E] = %d\n", __func__, ret);

	return ret;
}


#define IC_PRS_CMD_DATA_SIZE		16						/* cmd buffer size (work) */

static int _cyttsp4_ioctl_get_pressure_info( struct cyttsp4_ioctl_data *dio, struct fj_touch_pressure_info *pressure_info)
{
	const int	PRESS_SENS_OF_SIZE			= 8;		/* 2(16 bit) x 4 param	*/
	const int	NUM_OF_PRESS_SENS			= FJTOUCH_PRESSURE_NUM_OF_ELEMENTS;		/* 2 sens */
	const int	READ_ELEMENT_SIZE			= (PRESS_SENS_OF_SIZE * NUM_OF_PRESS_SENS);

	int	ii, ofst;
	int	retval = 0;
	unsigned char cmd_data[ IC_PRS_CMD_DATA_SIZE ];
	unsigned char raw_return[ READ_ELEMENT_SIZE + 1 ];

	/* [Section] Start of module. */
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Start. multi_scan = %d\n", __func__, pressure_info->multi_scan);

	/* [Section] Main Routine. */
	switch ( pressure_info->multi_scan ) {
	default:
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Multi scan mode \n", __func__);
		retval = -EINVAL;
		break;	/* default: */
	case MULTI_INITIAL:
		/* ; Change Mode to Test and Config */
		retval = _cyttsp4_ioctl_start(dio, CY_MODE_CAT, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT );
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed to switch to config mode\n", __func__);
		}
		break;	/* case MULTI_INITIAL: */
	case MULTI_FINAL:
		retval = _cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed to switch to config mode\n", __func__);
		}
		break;	/* case MULTI_FINAL: */
	case MULTI_UPDATE:

		if ((dio->curr_mode != CY_MODE_CAT) || (_cyttsp4_ioctl_IsCoreSleep(dio) == true)) {
			dev_err( _DIO_REF_DEV(dio) , "%s: get_pressure_info MULTI_UPDATE\n", __func__);
			retval = -EFAULT;
			goto cyttsp4_ioctl_get_pressure_info;
		}
		
		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: Execute Panel Scan(Set).\n", __func__);
		/* Execute Panel Scan :			w 24 02 0B p ; Execute Scan */
		memset(cmd_data, 0x00, sizeof(cmd_data));
		cmd_data[0]		= CY_CMD_CAT_EXEC_PANEL_SCAN;		/* 0x0B */
		retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &cmd_data[0], LOG_I2C_ADRR, true);

		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Execute Panel Scan Command (ret=%d)\n", __func__, retval);
			goto cyttsp4_ioctl_get_pressure_info;
		}

		/* wait complete */
		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] (Execute Panel Scan : wait comp) w 24 02; r 24 xx \n");
		retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_PRESS);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed to complete wait\n", __func__);
			goto cyttsp4_ioctl_get_pressure_info;
		}

		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: Check Result Data.\n", __func__);
		/* Check Result Data :			r 24 03 xx */
		memset(raw_return, 0x00, sizeof(raw_return));

		retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &raw_return[0], LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Execute Panel Scan Returns (ret=%d)\n", __func__, retval);
			goto cyttsp4_ioctl_get_pressure_info;
		}
		if (raw_return[0] != 0x00) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Execute Panel Scan Failure (status=0x%02X)\n", __func__, raw_return[0]);
			retval = -EINVAL;
			goto cyttsp4_ioctl_get_pressure_info;
		}

		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Execute Panel Scan Success\n", __func__);

		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: Command data set(Set Button Raw,Base,Diff,On/Off).\n", __func__);
		/* Command data set :			w 24 03 00 00 00 08 09 p ; Set Button Raw,Base,Diff,On/Off */
		memset(cmd_data, 0x00, sizeof(cmd_data));
		cmd_data[0]		= 0x00;							/* read Offset [15:8] */
		cmd_data[1]		= 0x00;							/* read Offset [7:0] */
		cmd_data[2]		= 0x00;							/* Number of Elements [15:8]	: 0x00 */
		cmd_data[3]		= PRESS_SENS_OF_SIZE;			/* Number of Elements [7:0]		: 0x08 */
		cmd_data[4]		= 0x09;							/* FILTERD PRESS Z DATA			: 0x09 */

		retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 5, &cmd_data[0], LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Command data set params (ret=%d)\n", __func__, retval);
			goto cyttsp4_ioctl_get_pressure_info;
		}

		_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Retrieve Panel Scan\n", __func__);
		/* Retrieve Panel Scan :			w 24 02 0C */
		memset(cmd_data, 0x00, sizeof(cmd_data));
		cmd_data[0]		= CY_CMD_CAT_RETRIEVE_PANEL_SCAN;		/*	0x0C */

		retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &cmd_data[0], LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Write Retrieve Panel Scan Command (ret=%d)\n", __func__, retval);
			goto cyttsp4_ioctl_get_pressure_info;
		}

		/* wait complete */
		retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_PRESS);
		if (retval < 0) {
			goto cyttsp4_ioctl_get_pressure_info;
		}

		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: Get Result Data.\n", __func__);
		/* Get Result Data */
		memset(raw_return, 0, sizeof(raw_return));

		retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 5, &raw_return[0], LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Read Retrieve Panel Scan returns (ret=%d)\n", __func__, retval);
			goto cyttsp4_ioctl_get_pressure_info;
		}
		if (raw_return[0] != 0x00) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Retrieve Panel Scan Failure (status=0x%02X)\n", __func__, raw_return[0]);
			retval = -EINVAL;
			goto cyttsp4_ioctl_get_pressure_info;
		}

		_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Get Retrieved Pressure Data\n", __func__);
		/* Get Retrieved Pressure Data */
		memset(raw_return, 0, sizeof(raw_return));

		retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, READ_ELEMENT_SIZE, &raw_return[0], LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Failed Get Retrieved Data (ret=%d)\n", __func__, retval);
			goto cyttsp4_ioctl_get_pressure_info;
		}

		/*  LeftFilteredRawLSB, LeftFilteredRawMSB, LeftBaseLSB, LeftBaseMSB, LeftDiffLSB, LeftDiffMSB, LeftSwLSB, LeftSwMSB, RightFilteredRawLSB, RightFilteredRawMSB, RightBaseLSB, RightBaseMSB, */
		for (ii = 0; ii < NUM_OF_PRESS_SENS; ii++) {
			ofst = ii * PRESS_SENS_OF_SIZE;
			pressure_info->elem[ ii ].raw_val		=	(raw_return[ ofst + 0 ]  | ((raw_return[ ofst + 1 ]  << 8) & 0xFF00));
			pressure_info->elem[ ii ].baseline_val	=	(raw_return[ ofst + 2 ]  | ((raw_return[ ofst + 3 ]  << 8) & 0xFF00));
			pressure_info->elem[ ii ].diff_val		=	(raw_return[ ofst + 4 ]  | ((raw_return[ ofst + 5 ]  << 8) & 0xFF00));
			pressure_info->elem[ ii ].status		=	(raw_return[ ofst + 6 ]  | ((raw_return[ ofst + 7 ]  << 8) & 0xFF00));
			_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: elem[%d] raw=%d, baseline=%d, diff=%d, status=%d.\n",
		    				__func__, ii, (signed short int)pressure_info->elem[ ii ].raw_val, (signed short int)pressure_info->elem[ ii ].baseline_val, (signed short int)pressure_info->elem[ ii ].diff_val, (signed short int)pressure_info->elem[ ii ].status);
		}

		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Get Retrieved Pressure Data Success\n", __func__);

		break;	/* case MULTI_UPDATE: */
	}

cyttsp4_ioctl_get_pressure_info:
	/* [Section] End of module. */

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: End = %d, multi_scan = %d.\n", __func__, retval, pressure_info->multi_scan);
	return retval;
}

static int _cyttsp4_ioctl_do_calibration_pressure_PWCs( struct cyttsp4_ioctl_data *dio, struct fj_touch_do_calibration_pressure *do_calibration_pressure)
{
	const int	RAW_RETSIZE					= 16;
	int	retval = 0;
	unsigned char cmd_data[ IC_PRS_CMD_DATA_SIZE ];
	unsigned char raw_return[ RAW_RETSIZE + 1 ];

	/* [Section] Start of module. */
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Start.\n", __func__);

/* FUJITSU:2013-06-20 HMW_TOUCH_KOTEI_PERIODICAL_CALIB_TIM add start */
	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] mkcmd = %d.\n", makercmd_mode);
	if (makercmd_mode) {
		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Set P_Calib_Tim (0ms).\n", __func__);
		/* W 24 03 5E 02 00 00 */
		retval = cyttsp4_request_set_runtime_parm( _DIO_REF_TTSP(dio), CY_PERIODICAL_CALIB_TIM, CY_PERIODICAL_CALIB_TIM_PARM_SZ, 0);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Error.Set RT Config, ret = %d\n", __func__, retval);
			goto exit;
		}
		_cyttsp4_ioctl_runtime_debug( _DIO_REF_TTSP(dio), CY_PERIODICAL_CALIB_TIM, 0);			/* for debug */
	}
/* FUJITSU:2013-06-20 HMW_TOUCH_KOTEI_PERIODICAL_CALIB_TIM add end */

	/* ; Change Mode to Test and Config */
	retval = _cyttsp4_ioctl_start(dio, CY_MODE_CAT, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT );
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed to switch to config mode\n", __func__);
		goto cyttsp4_ioctl_do_calibration_pressure_PWCs;
	}

	/* [Section] Main Routine. */

	_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Set ParameterID = Calib PWCs\n", __func__);
	/* (W) Calibrate PWCs(Mutual cap Button) :		w 24 03 01 00 00  : Mutual cap Button(= Press Self)*/
	memset(cmd_data, 0x00, sizeof(cmd_data));
	cmd_data[0]		= CY_MUTUAL_CAPACITANCE_BUTTONS;	/* Mutual cap Button(= Press Self) : 0x01 */
	cmd_data[1]		= 0x00;
	cmd_data[2]		= 0x00;

	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 3, &cmd_data[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Calib PWCs(Press self) (ret=%d)\n", __func__, retval);
		goto cyttsp4_ioctl_do_calibration_pressure_PWCs;
	}

	_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Do SetParameter ID (= Calib PWCs)\n", __func__);
	/* (W) Do Calibrate PWCs			: w 24 02 09  : do Calib */
	memset(cmd_data, 0x00, sizeof(cmd_data));
	cmd_data[0]		= CY_CMD_CAT_CALIBRATE_IDACS;		/*	0x09 */

	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &cmd_data[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Do SetParameter ID(= Calib PWCs) (ret=%d)\n", __func__, retval);
		goto cyttsp4_ioctl_do_calibration_pressure_PWCs;
	}

	/* wait complete 				: w 24 02; r 24 C9 */
	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_PRESS);
	if (retval < 0) {
		goto cyttsp4_ioctl_do_calibration_pressure_PWCs;
	}

	_cyttsp4_dbg(dio, CY_LOGLV2, "%s: Check Result Data\n", __func__);
	/* (R) Return data				: w 24 03; r 24 xx		*/
	memset(raw_return, 0x00, sizeof(raw_return));

	retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 1, &raw_return[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Do Calib returns (ret=%d)\n", __func__, retval);
		goto cyttsp4_ioctl_do_calibration_pressure_PWCs;
	}

	if (raw_return[0] != 0x00) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Do Calib Failure (status=0x%02X)\n", __func__, raw_return[0]);
		retval = -EINVAL;
		goto cyttsp4_ioctl_do_calibration_pressure_PWCs;
	}

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Do calib press Success\n", __func__);

	/* do not Base Line Initialize!! */


cyttsp4_ioctl_do_calibration_pressure_PWCs:

	/* [Section] End of module. */
	/* change OPERATING MODE */
	if (retval != 0) {
		_cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	} else {
		retval = _cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	}

exit: /* FUJITSU:2013-06-20 HMW_TOUCH_KOTEI_PERIODICAL_CALIB_TIM add */

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: End = %d.\n", __func__, retval);
	return retval;
}

static int _cyttsp4_ioctl_do_calibration_pressure_GIDAC( struct cyttsp4_ioctl_data *dio, struct fj_touch_do_calibration_pressure *do_calibration_pressure)
{
	int	retval = 0;

	/* [Section] Start of module. */
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Start.\n", __func__);

	/* ; Change Mode to Test and Config */
	retval = _cyttsp4_ioctl_start(dio, CY_MODE_OPERATIONAL, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT );
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed to switch to config mode\n", __func__);
		goto cyttsp4_ioctl_do_calibration_pressure_GIDAC;
	}

	/* [Section] Main Routine. */
	retval = cyttsp4_request_do_force_calib( _DIO_REF_TTSP(dio), CY_INFOOPT_NONE );

cyttsp4_ioctl_do_calibration_pressure_GIDAC:

	/* [Section] End of module. */
	/* change OPERATING MODE */
	if (retval != 0) {
		_cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	} else {
		retval = _cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	}
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: End = %d.\n", __func__, retval);
	return retval;
}

static int _cyttsp4_ioctl_touchpress_ctrl(struct cyttsp4_ioctl_data *dio, bool IsSet, struct fj_touch_touchpress_ctrl *touch_press)
{
	u8 cmd_dat[3];
	int ret = 0;
	enum cyttsp4_information_state info;

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s (%s) [S]\n", __func__, _DIO_REF_SET_GET(IsSet));

	memset(cmd_dat, 0, sizeof(cmd_dat));

	if (IsSet == true) {
		if (touch_press->touchpress_setting == FJTOUCH_TOUCHPRESS_ENABLE) {
			info		= CY_INFOSTATE_TRUE;
		} else {
			info		= CY_INFOSTATE_FALSE;
		}

		ret = cyttsp4_request_set_pressure_ctrl(_DIO_REF_TTSP(dio), true/* IsSet*/, &info);
		if (ret < 0) {
			goto touchpress_ctrl_exit;
		}
		_cyttsp4_dbg(dio, CY_LOGLV2,"[TPD_Ic] %s (set) info = %d\n", __func__, info);

	} else {

		ret = cyttsp4_request_set_pressure_ctrl(_DIO_REF_TTSP(dio), false/* IsSet*/, &info);
		if (ret < 0) {
			goto touchpress_ctrl_exit;
		}

		_cyttsp4_dbg(dio, CY_LOGLV2,"[TPD_Ic] %s (get) info = %d\n", __func__, info);

		if (info == CY_INFOSTATE_TRUE) {
			touch_press->touchpress_setting = FJTOUCH_TOUCHPRESS_ENABLE;
		} else {
			touch_press->touchpress_setting = FJTOUCH_TOUCHPRESS_DISABLE;
		}
	}

touchpress_ctrl_exit:

	_cyttsp4_dbg(dio, CY_LOGLV1,"[TPD_Ic] %s (%s) [E]  = %d\n", __func__, _DIO_REF_SET_GET(IsSet), ret);

	return ret;
}


static int _cyttsp4_ioctl_get_ret_calib_pressure(struct cyttsp4_ioctl_data *dio, struct fj_touch_ret_calibration_pressure *ret_calib_press)
{
	const int	RAW_RETSIZE					= 16;

	int	retval = 0;
	unsigned char cmd_data[ IC_PRS_CMD_DATA_SIZE ];
	unsigned char raw_return[ RAW_RETSIZE + 1 ];

	/* [Section] Start of module. */
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Start.\n", __func__);

	/* ; Change Mode to Test and Config */
	retval = _cyttsp4_ioctl_start(dio, CY_MODE_CAT, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT );
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed to switch to config mode\n", __func__);
		goto cyttsp4_ioctl_get_ret_calib_pressure;
	}

	/* [Section] Main Routine. */
	/* set parameter to buttom(Press)		W 24 03 00 00 00 03 03 p */
	memset(cmd_data, 0x00, sizeof(cmd_data));
	cmd_data[0]		= 0x00;		/* Read Offset[15:8]: Byte offset into Data Structure (see Table 8-35 for more details) */
	cmd_data[1]		= 0x00;		/* Read Offset[7:0] */
	cmd_data[2]		= 0x00;		/* Read Length[15:8]: Number of bytes to return starting from Read Offset */
	cmd_data[3]		= 0x03;		/* Read Length[7:0]: */
	cmd_data[4]		= 0x03;		/* Data ID: Type of scan mode data to retrieve (see Table 8-35 for more details) */

	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 5, &cmd_data[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: set parameter to buttom(Press) (ret=%d)\n", __func__, retval);
		goto cyttsp4_ioctl_get_ret_calib_pressure;
	}

	/* Retrieve Data Structure 				W 24 02 10 */
	memset(cmd_data, 0x00, sizeof(cmd_data));
	cmd_data[0]		= CY_CMD_CAT_RETRIEVE_DATA_STRUCTURE;		/* 0x10 */
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &cmd_data[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed Retrieve_data structure of Sensor (ret=%d)\n", __func__, retval);
		goto cyttsp4_ioctl_get_ret_calib_pressure;
	}

	/* Retrieve Data Structure Command Wait comp ... bit6 = 1 => R 24 D0 */
	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_PRESS);
	if (retval < 0) {
		goto cyttsp4_ioctl_get_ret_calib_pressure;
	}

	/* PWCS, GIDAC read 					R 24 00 (same as R 24 08 & 3 Bytes read...)*/
	/* data [HST_MODE , NoCare , NoCare , NoCare , NoCare , NoCare , NoCare , NoCare ,GIDAC(1byte),Left Sens PWC(1byte),Right Sens PWC(1byte)] */
	memset(raw_return, 0x00, sizeof(raw_return));
	retval = _cyttsp4_ioctl_read_block_data(dio, 0x08, 3, &raw_return[0], LOG_I2C_ADRR, true);

	if ((ret_calib_press->get_calib_type & FJTOUCH_CALIB_PRESS_PTN1) != 0) {
		ret_calib_press->ptn1_val[ 0 ] = raw_return[ 1 ];			/* Left PWCs */
		ret_calib_press->ptn1_val[ 1 ] = raw_return[ 2 ];			/* Right PWCs */
		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: ptn1_val[0] = %d, ptn1_val[1] = %d.\n", __func__, ret_calib_press->ptn1_val[ 0 ], ret_calib_press->ptn1_val[ 1 ]);
	}
	if ((ret_calib_press->get_calib_type & FJTOUCH_CALIB_PRESS_PTN2) != 0) {
		ret_calib_press->ptn2_val = raw_return[ 0 ];				/* GIDAC */
		_cyttsp4_dbg(dio, CY_LOGLV2, "[TPD_Ic] %s: ptn2_val = %d.\n", __func__, ret_calib_press->ptn2_val);
	}

cyttsp4_ioctl_get_ret_calib_pressure:

	/* [Section] End of module. */
	/* change OPERATING MODE */
	if (retval != 0) {
		_cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	} else {
		retval = _cyttsp4_ioctl_end(dio, CY_MODE_OPERATIONAL);
	}

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: End = %d.\n", __func__, retval);
	return retval;
}

/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add start */
#if 0
__sub_____get_touch_position____(){}
#endif

/*
 * Collect position
 *
 * calls from attention_irq context
 */
static void _cyttsp4_ioctl_collect_pos(struct cyttsp4_ioctl_data *dio, struct cyttsp4_touch *tch)
{
	struct cyttsp4_ioctl_touch_pos_info		*tch_pos = &dio->touch_pos;
	struct fj_touch_position				*pos;
	int										curr_cnt;

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s [S].\n", __func__);

	curr_cnt	= atomic_read( &tch_pos->atm_curr_cnt );

	if (curr_cnt >= tch_pos->pos_info.num_of_position) {
		/* event full... */
		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s, Event Full.., curr_cnt = %d, req_cnt = %d\n", __func__, curr_cnt, tch_pos->pos_info.num_of_position);
		return;
	}

	pos = &tch_pos->pos_info.pos [ curr_cnt ];

	pos->pos_x		= tch->abs[CY_TCH_X];
	pos->pos_y		= tch->abs[CY_TCH_Y];
	pos->press_val	= tch->abs[CY_TCH_P];

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s, cnt = %03d/%03d  (%4d, %4d, %4d), press_info_sts = %d.\n",
					__func__, curr_cnt, tch_pos->pos_info.num_of_position, pos->pos_x, pos->pos_y, pos->press_val, tch_pos->press_info_sts);

	atomic_inc( &tch_pos->atm_curr_cnt );
	curr_cnt = atomic_read( &tch_pos->atm_curr_cnt );
	if (curr_cnt >= tch_pos->pos_info.num_of_position) {

		tch_pos->pos_info.result = FJTOUCH_GET_TOUCHPOS_EOK;
		atomic_set( &tch_pos->atm_status , CY_IC_TOUCHPOS_STS_SUCCESS);

		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s change sts = %d [E].\n", __func__, CY_IC_TOUCHPOS_STS_SUCCESS);
	}

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s [E].\n", __func__);
}

/*
 * get mt touch
 *
 * calls from attention_irq context
 */
static void _cyttsp4_ioctl_get_mt_touches(struct cyttsp4_ioctl_data *dio, int num_cur_tch)
{
	/* based from cyttsp4_get_mt_touches() */
	struct cyttsp4_sysinfo *si = dio->si;
	struct cyttsp4_touch tch;
	int i;
	int mt_sync_count = 0;
	u8 *xy_mode;
	u8 press_h = 0;
	u8 press_l = 0;
	struct cyttsp4_ioctl_touch_pos_info		*tch_pos = &dio->touch_pos;

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: num_cur_tch=%d\n", __func__, num_cur_tch);

	memset(&tch, 0, sizeof(struct cyttsp4_touch));

	for (i = 0; i < num_cur_tch; i++) {
		cyttsp4_get_touch_record(_DIO_REF_TTSP(dio), i, tch.abs);

		if (tch.abs[CY_TCH_E] == CY_EV_LIFTOFF) {
			_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: e=%d lift-off\n", __func__, tch.abs[CY_TCH_E]);
			goto cyttsp4_get_mt_touches_pr_tch;
		}

		if (tch_pos->press_info_sts == CY_INFOSTATE_TRUE) {
			/* Replace Pressure value */
			xy_mode = si->xy_mode + si->si_ofs.rep_ofs;
			press_h = xy_mode[3];
			press_l = xy_mode[4];
			tch.abs[CY_TCH_P] = (signed short)((press_h << 8) + press_l);
		}

		if (IS_TTSP_VER_GE(si, 2, 3)) {
			/*
			 * TMA400 size and orientation fields:
			 * if pressure is non-zero and major touch
			 * signal is zero, then set major and minor touch
			 * signals to minimum non-zero value
			 */
			if (tch.abs[CY_TCH_P] > 0 && tch.abs[CY_TCH_MAJ] == 0)
				tch.abs[CY_TCH_MAJ] = tch.abs[CY_TCH_MIN] = 1;
		}
		mt_sync_count++;

		_cyttsp4_ioctl_collect_pos(dio, &tch);

cyttsp4_get_mt_touches_pr_tch:

		_cyttsp4_dbg(dio, CY_LOGLV0,
			"[TPD_Ic],%s%s%s%s,%04d,%04d,%04d\n",
			tch.abs[CY_TCH_E] == CY_EV_NO_EVENT ? "NoEvent" : "",
			tch.abs[CY_TCH_E] == CY_EV_TOUCHDOWN ? "Down" : "",
			tch.abs[CY_TCH_E] == CY_EV_MOVE ? "Move" : "",
			tch.abs[CY_TCH_E] == CY_EV_LIFTOFF ? "Up" : "",
			tch.abs[CY_TCH_X],
			tch.abs[CY_TCH_Y],
			tch.abs[CY_TCH_P]);
	}

	return;
}

/*
 * xy worker
 *
 * calls from attention_irq context
 */
static int _cyttsp4_ioctl_xy_worker(struct cyttsp4_ioctl_data *dio)
{
	/* based from cyttsp4_xy_worker() */

	struct device *dev = &dio->ttsp->dev;
	struct cyttsp4_sysinfo *si = dio->si;
	u8 num_cur_tch;
	u8 hst_mode;
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;
	int rc = 0;
	struct cyttsp4_ioctl_touch_pos_info		*tch_pos = &dio->touch_pos;
	int										status;

	/*
	 * Get event data from cyttsp4 device.
	 * The event data includes all data
	 * for all active touches.
	 * Event data also includes button data
	 */
	/*
	 * Use 2 reads:
	 * 1st read to get mode + button bytes + touch count (core)
	 * 2nd read (optional) to get touch 1 - touch n data
	 */

	status		= atomic_read( &tch_pos->atm_status );
	if (status != CY_IC_TOUCHPOS_STS_WORKING) {
		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s, Not working get touch pos = %d\n", __func__, status);
		goto cyttsp4_xy_worker_exit;
	}

	hst_mode = si->xy_mode[CY_REG_BASE];
	rep_len = si->xy_mode[si->si_ofs.rep_ofs];
	rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
	tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: %s%02X %s%d %s%02X %s%02X\n", __func__,
		"hst_mode=", hst_mode, "rep_len=", rep_len,
		"rep_stat=", rep_stat, "tt_stat=", tt_stat);


	num_cur_tch = GET_NUM_TOUCH_RECORDS(tt_stat);
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: num_cur_tch=%d\n", __func__, num_cur_tch);

	if (rep_len == 0 && num_cur_tch > 0) {
		dev_err(dev, "%s: report length error rep_len=%d num_tch=%d\n",
			__func__, rep_len, num_cur_tch);
		goto cyttsp4_xy_worker_exit;
	}

	/* read touches */
	if (num_cur_tch > 0) {
		rc = cyttsp4_read(dio->ttsp, CY_MODE_OPERATIONAL, si->si_ofs.tt_stat_ofs + 1, si->xy_data, num_cur_tch * si->si_ofs.tch_rec_size);
		if (rc < 0) {
			dev_err(dev, "%s: read fail on touch regs r=%d\n", 	__func__, rc);
			goto cyttsp4_xy_worker_exit;
		}
	}

	/* print xy mode */
	_cyttsp4_ioctl_pr_buf(dio, si->xy_mode, si->si_ofs.mode_size, "ioctl_xy_mode", CY_LOGLV2);
	/* print xy data */
	_cyttsp4_ioctl_pr_buf(dio, si->xy_data, num_cur_tch * si->si_ofs.tch_rec_size, "ioctl_xy_data", CY_LOGLV2);

	/* check any error conditions */
	if (IS_BAD_PKT(rep_stat)) {
		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Invalid buffer detected\n", __func__);
		rc = 0;
		goto cyttsp4_xy_worker_exit;
	}

	if (IS_LARGE_AREA(tt_stat)) {
		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Large area detected(Error)\n", __func__);
		tch_pos->pos_info.result = FJTOUCH_GET_TOUCHPOS_ENOSIGLETOUCH;
		atomic_set( &tch_pos->atm_status , CY_IC_TOUCHPOS_STS_ERROREND);
		rc = 0;
		goto cyttsp4_xy_worker_exit;
	}

	/* 1 touch only ... */
	if (num_cur_tch > 1) {
		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: Find Multi touch(Error)\n", __func__);
		tch_pos->pos_info.result = FJTOUCH_GET_TOUCHPOS_ENOSIGLETOUCH;
		atomic_set( &tch_pos->atm_status , CY_IC_TOUCHPOS_STS_ERROREND);
		rc = 0;
		goto cyttsp4_xy_worker_exit;
	}

	if (num_cur_tch > si->si_ofs.max_tchs) {
		if (num_cur_tch > max(CY_TMA1036_MAX_TCH, CY_TMA4XX_MAX_TCH)) {
			/* terminate all active tracks */
			dev_err(dev, "%s: Num touch err detected (n=%d)\n", __func__, num_cur_tch);
			num_cur_tch = 0;
		} else {
			dev_err(dev, "%s: %s (n=%d c=%d)\n", __func__, "too many tch; set to max tch", num_cur_tch, si->si_ofs.max_tchs);
			num_cur_tch = si->si_ofs.max_tchs;
		}
	}

	/* extract xy_data for all currently reported touches */
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: extract data num_cur_tch=%d\n", __func__, num_cur_tch);
	if (num_cur_tch) {
		_cyttsp4_ioctl_get_mt_touches(dio, num_cur_tch);
	}

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s: done\n", __func__);
	rc = 0;

cyttsp4_xy_worker_exit:

	return rc;
}

/*
 * IRQ handler
 */
static int cyttsp4_ioctl_attention_irq(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_ioctl_data *dio = dev_get_drvdata(dev);
	int rc = 0;

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s\n", __func__);
	mutex_lock( &dio->report_lock );

	rc = _cyttsp4_ioctl_xy_worker(dio);

	mutex_unlock( &dio->report_lock );
	if (rc < 0) {
		dev_err(dev, "%s: xy_worker error r=%d\n", __func__, rc);
	}

	return rc;
}

/*
 * wait touch position
 *
 * calls from user context
 */
static int _cyttsp4_ioctl_wait_touch_position(struct cyttsp4_ioctl_data *dio)
{
	struct cyttsp4_ioctl_touch_pos_info	*tch_pos = &dio->touch_pos;
	int									status;
	int									curr_cnt;
	int									ii;
	int	retval = 0;

	msleep( CY_WAITIRQ_SLEEP_TOUCH_POS * 3 );							/* wait some IRQ... */

	curr_cnt	= atomic_read( &tch_pos->atm_curr_cnt );
	if (curr_cnt == 0) {
		atomic_set( &tch_pos->atm_status , CY_IC_TOUCHPOS_STS_ERROREND);
		tch_pos->pos_info.result			= FJTOUCH_GET_TOUCHPOS_ENOINT;

		return 0;		/* success, result = error */
	}

	/* main loop, waiting for IRQ... */
	for (ii = 0; ii < tch_pos->pos_info.num_of_position; ii++) {
		status		= atomic_read( &tch_pos->atm_status );
		if (status != CY_IC_TOUCHPOS_STS_WORKING) {
			/* finish ? */
			break;
		}
		msleep( CY_WAITIRQ_SLEEP_TOUCH_POS );
	}

	if (status == CY_IC_TOUCHPOS_STS_WORKING) {
		/* main loop is finish but status is working... Error! */
		atomic_set( &tch_pos->atm_status , CY_IC_TOUCHPOS_STS_ERROREND);
		tch_pos->pos_info.result			= FJTOUCH_GET_TOUCHPOS_ENOINT;
	}

	curr_cnt	= atomic_read( &tch_pos->atm_curr_cnt );
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s, (Finish) ret = %d, cnt = %3d/%3d, (sts = %d).\n", __func__, retval, curr_cnt, tch_pos->pos_info.num_of_position , status);

	return retval;
}

/*
 * get touch position
 *
 * calls from user context
 */
static int _cyttsp4_ioctl_get_touch_position(struct cyttsp4_ioctl_data *dio)
{
	struct cyttsp4_ioctl_touch_pos_info		*tch_pos = &dio->touch_pos;
	int									curr_cnt;
	int									status;
	enum cyttsp4_information_state		press_th_sts	= CY_INFOSTATE_FALSE;	/* Press Threshold state */
	enum cyttsp4_information_state		press_info_sts	= CY_INFOSTATE_TRUE;
	u8									press_th_val	= CY_PRESS_TH_VAL_NORMAL;
	bool								subscribe_atn		= false;			/* Is subscribe attention IRQ */
	bool								change_press_th		= false;			/* Is change Press Threshold */
	int	retval = 0, retval2 = 0;

	status		= atomic_read( &tch_pos->atm_status );

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [S], num_of_pos = %3d.\n", __func__, tch_pos->pos_info.num_of_position);

	if (status != CY_IC_TOUCHPOS_STS_IDLE) {
		retval = - EBUSY;
		_cyttsp4_elog(_DIO_REF_DEV(dio), "%s, busy(sts = %d).\n", __func__, status);
		goto exit;
	}

	if (tch_pos->pos_info.num_of_position == 0) {
		/* success */
		tch_pos->pos_info.result		= FJTOUCH_GET_TOUCHPOS_EOK;
		goto exit;
	}

	if ((tch_pos->pos_info.num_of_position < 0) || (tch_pos->pos_info.num_of_position > FJTOUCH_NUM_OF_POSITION)) {
		retval = -EINVAL;
		_cyttsp4_elog(_DIO_REF_DEV(dio), "%s, invalid parm(pos = %d).\n", __func__, tch_pos->pos_info.num_of_position);
		goto exit;
	}

	press_th_sts	= CY_INFOSTATE_FALSE;
	retval = cyttsp4_ioctl_runtime_PressThreshold_ctrl((void *)dio, false /* get */, &press_th_sts, &press_th_val);
	if (retval < 0) {
		_cyttsp4_elog(_DIO_REF_DEV(dio), "%s, get INT Mode = %d.\n", __func__, retval);
		goto exit;
	}

	retval = cyttsp4_request_set_pressure_ctrl(_DIO_REF_TTSP(dio), false/* IsSet*/, &press_info_sts);
	if (retval < 0) {
		_cyttsp4_elog(_DIO_REF_DEV(dio), "%s, get pressure ctrl = %d.\n", __func__, retval);
		goto exit;
	}

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s, press_th_val = 0x%02X, press_info_sts = %d.\n", __func__, press_th_val, press_info_sts);

	/* init get pos */
	tch_pos->press_th_val			= press_th_val;
	tch_pos->pos_info.result		= FJTOUCH_GET_TOUCHPOS_ENOINT;
	tch_pos->press_info_sts			= press_info_sts;
	atomic_set( &tch_pos->atm_curr_cnt , 0);

	/* subscribe irq event */
	retval = cyttsp4_subscribe_attention( _DIO_REF_TTSP(dio), CY_ATTEN_IRQ, cyttsp4_ioctl_attention_irq, CY_MODE_OPERATIONAL);
	if (retval < 0) {
		_cyttsp4_elog(_DIO_REF_DEV(dio), "%s, subscribe error = %d.\n", __func__, retval);
		goto exit;
	}

	subscribe_atn = true;

	press_th_sts	= CY_INFOSTATE_TRUE;
	press_th_val	= CY_PRESS_TH_VAL_MCMODE;			/* 0xFF */
	retval = cyttsp4_ioctl_runtime_PressThreshold_ctrl((void *)dio, true /* set */, &press_th_sts, &press_th_val);
	if (retval < 0) {
		_cyttsp4_elog(_DIO_REF_DEV(dio), "%s, get INT Mode = %d.\n", __func__, retval);
		goto exit;
	}

	change_press_th = true;

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s, IOCTL set atm_status bef.\n", __func__);

	/* start measure touch position */
	atomic_set( &tch_pos->atm_status , CY_IC_TOUCHPOS_STS_WORKING);

	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s, IOCTL set atm_status aft.\n", __func__);

	retval = _cyttsp4_ioctl_wait_touch_position( dio );
	if (retval < 0) {
		_cyttsp4_elog(_DIO_REF_DEV(dio), "%s, wait touch pos = %d.\n", __func__, retval);
		goto exit;
	}

	/* Success */
	retval = 0;

exit:

	if (change_press_th == true) {
		press_th_sts	= CY_INFOSTATE_FALSE;
		press_th_val	= tch_pos->press_th_val;			/* restore value */
		retval2 = cyttsp4_ioctl_runtime_PressThreshold_ctrl((void *)dio, true /* set */, &press_th_sts, &press_th_val);
		if (retval2 < 0) {
			_cyttsp4_elog(_DIO_REF_DEV(dio), "%s, restore int mode error = %d.\n", __func__, retval2);
		}
		change_press_th = false;
	}

	if (subscribe_atn == true) {
		cyttsp4_unsubscribe_attention(_DIO_REF_TTSP(dio), CY_ATTEN_IRQ, cyttsp4_ioctl_attention_irq, CY_MODE_OPERATIONAL);

		subscribe_atn = false;
	}

	curr_cnt	= atomic_read( &tch_pos->atm_curr_cnt );
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [E], ret = %d, result = %d, num = %03d/%03d.\n", __func__, retval, tch_pos->pos_info.result, curr_cnt, tch_pos->pos_info.num_of_position);

	/* final */
	atomic_set( &tch_pos->atm_status , CY_IC_TOUCHPOS_STS_IDLE);
	atomic_set( &tch_pos->atm_curr_cnt , 0);

	return retval;
}

/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE add start */
#if 0
__sub_____set_press_normalization____(){}
#endif

/*
 * set normalization for IOCTL
 *
 */
static int _cyttsp4_ioctl_set_press_normalize(struct cyttsp4_ioctl_data *dio, struct fj_touch_press_normalization *prs_normalize)
{
	int	retval = 0;
	enum cyttsp4_information_state	info = 0;

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [S], normalize = %d.\n", __func__, prs_normalize->normalization);

	/* IOCTL_GET_PRESSUREINFO (INITAL) call, it is already changed into "CAT." */
	if ((dio->curr_mode != CY_MODE_CAT) || (_cyttsp4_ioctl_IsCoreSleep(dio) == true)) {
		dev_err( _DIO_REF_DEV(dio) , "%s: not enter CAT Mode(%d) or sleep  \n", __func__, dio->curr_mode);
		retval = -EFAULT;
		goto cyttsp4_ioctl_set_press_normalize;
	}
	if (prs_normalize->normalization == FJTOUCH_PRESS_NORMALIZE_ENABLE) {
		info = CY_INFOSTATE_TRUE;
	} else {
		info = CY_INFOSTATE_FALSE;
	}

	retval = cyttsp4_request_set_normalize(_DIO_REF_TTSP(dio), true/* IsSet*/, &info);

	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Failed set_press_normalize_mode (ret=%d)\n", __func__, retval);
		goto cyttsp4_ioctl_set_press_normalize;
	}

cyttsp4_ioctl_set_press_normalize:

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [E] = %d.\n", __func__, retval);

	return retval;
}

/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE add end */


static int cyttsp4_ioctl_attention_startup(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_ioctl_data *dio = dev_get_drvdata(dev);
	int rc = 0;

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [S].\n", __func__);

	/* Re-registration of ioctl handle for HW/SW-RESET. */
	cyttsp4_request_set_ioctl_handle( ttsp, (void *)dio );

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [E].\n", __func__);

	return rc;
}

/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add end */

/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add start */
#if 0
__sub_____get_press_noise_eval____(){}
#endif

#define IC_COMP_WAIT_NOISE_EVAL_1ST_SLEEP		150			/* 150 ms */
#define CY_WAITCMD_PRS_NOISE_EVAL				5			/* 5 ms */
#define		fmMask(type, num)		(type)(((type)(-1)) & num)

static const u8 cTbl_CY_BTN_ALT_TX_PERIOD[ FJ_TOUCH_PRESS_NOISE_EVAL_NUM ] = {
	CY_BTN_ALT_TX_PERIOD1,
	CY_BTN_ALT_TX_PERIOD2,
	CY_BTN_ALT_TX_PERIOD3,
	CY_BTN_ALT_TX_PERIOD4,
};

static const u8 cTbl_BTN_ALT_TX_POSITIVE_SUM[ FJ_TOUCH_PRESS_NOISE_EVAL_NUM ] = {
	CY_BTN_ALT_TX1_POSITIVE_SUM,
	CY_BTN_ALT_TX2_POSITIVE_SUM,
	CY_BTN_ALT_TX3_POSITIVE_SUM,
	CY_BTN_ALT_TX4_POSITIVE_SUM,
};

static const u8 cTbl_BTN_ALT_TX_NEGATIVE_SUM[ FJ_TOUCH_PRESS_NOISE_EVAL_NUM ] = {
	CY_BTN_ALT_TX1_NEGATIVE_SUM,
	CY_BTN_ALT_TX2_NEGATIVE_SUM,
	CY_BTN_ALT_TX3_NEGATIVE_SUM,
	CY_BTN_ALT_TX4_NEGATIVE_SUM,
};

static int _cyttsp4_ioctl_set_BTN_ALT_TX_PERIOD(struct cyttsp4_ioctl_data *dio, struct fj_touch_press_noise_eval *press_noise_eval)
{
	int rc = 0, retval = 0;
	int ii;
	u32	set_val;

	for (ii = 0; ii < FJ_TOUCH_PRESS_NOISE_EVAL_NUM; ii++ ) {
		_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s : CY_BTN_ALT_TX_PERIOD%d(0x%02X).\n",
									__func__, (ii + 1), (u8)cTbl_CY_BTN_ALT_TX_PERIOD[ ii ]);

		set_val = (fmMask(u8, press_noise_eval->eval_freq[ ii ]) & 0x000000FF);
		retval = cyttsp4_request_set_runtime_parm( _DIO_REF_TTSP(dio), (u8)cTbl_CY_BTN_ALT_TX_PERIOD[ ii ], CY_BTN_ALT_TX_PERIODX_PARM_SZ, set_val);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s : CY_BTN_ALT_TX_PERIOD%d(0x%02X) ret = %d, freq = 0x%02X\n",
										__func__, (ii + 1), (u8)cTbl_CY_BTN_ALT_TX_PERIOD[ ii ], retval, fmMask(u8, set_val));
			rc = -1;
			break;
		}
	}

	return rc;
}

/*
 * wait complete
 */

static int _cyttsp4_ioctl_complete_wait_Noise_Eval( struct cyttsp4_ioctl_data *dio , int wait_ms)
{
	/* Bit6(0x40) == comp. But BTN_START_NOISE_EVAL comp is 24 67 01 xx. */
	/* wait 67 01 00 ...  (W 24 03 67;W 24 02 02; W 24 03 / r 24 67 01 xx) */

	unsigned char cmd_data[ IC_PRS_CMD_DATA_SIZE ];
	int retval = -1, rc = -1;
	int count = 0;
	int limit = 100;

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [S].\n", __func__);

	msleep( IC_COMP_WAIT_NOISE_EVAL_1ST_SLEEP );	/* 150 ms sleep */

	if (wait_ms == 0) {
		/* false safe ... */
		wait_ms = CY_WAITCMD_PRS_NOISE_EVAL;
	}

	for (count = 0; count < limit; count++) {

		/* Set Parameter = BTN_START_NOISE_EVAL 	W 24 03 67	*/
		_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s : Set Parameter = BTN_START_NOISE_EVAL (W 24 03 67).\n", __func__);
		memset(cmd_data, 0x00, sizeof(cmd_data));
		cmd_data[0]		= CY_BTN_START_NOISE_EVAL;		/* 0x67 */
		retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 1, &cmd_data[0], LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Set ParameterID = BTN_START_NOISE_EVAL (ret=%d)\n", __func__, retval);
			rc = -1;
			break;
		}

		/*  Get Param cmd Exec 						W 24 02 02 */
		_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s : Get Param cmd Exec (W 24 02 02).\n", __func__);
		memset(cmd_data, 0x00, sizeof(cmd_data));
		cmd_data[0]		= 0x02;		/* 0x02 */
		retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &cmd_data[0], LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Set ParameterID = BTN_START_NOISE_EVAL (ret=%d)\n", __func__, retval);
			rc = -1;
			break;
		}

		/* Check Result Data R 24 03; len 3*/
		_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s : Check Result Data (R 24 03; len 3).\n", __func__);
		memset(cmd_data, 0xEE, sizeof(cmd_data));
		retval = _cyttsp4_ioctl_read_block_data(dio, 0x03, 3, &cmd_data[0], LOG_I2C_ADRR, true);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "%s: Check Result Data (ret=%d)\n", __func__, retval);
			rc = -1;
			break;
		}

		/* 67 01 00 = success, 67 01 01 = doing... */
		if (cmd_data[2] == 0) {
			/* SUCCESS */
			rc = 0;
			break;
		}
		msleep(wait_ms);
	}

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [E] rc = %d, cont=%d.\n", __func__, rc, count);
	return rc;
}

static int _cyttsp4_ioctl_do_BTN_START_NOISE_EVAL(struct cyttsp4_ioctl_data *dio)
{
	int rc = -1, retval = 0;
	unsigned char cmd_data[ IC_PRS_CMD_DATA_SIZE ];

	/*  Set Parameter = BTN_START_NOISE_EVAL exec			W 24 03 67 01 01 */
	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s : Set Parameter = BTN_START_NOISE_EVAL exec (W 24 03 67 01 01).\n", __func__);
	memset(cmd_data, 0x00, sizeof(cmd_data));
	cmd_data[0]		= CY_BTN_START_NOISE_EVAL;		/* 0x67 */
	cmd_data[1]		= 0x01;							/* Length */
	cmd_data[2]		= 0x01;
	retval = _cyttsp4_ioctl_write_block_data(dio, 0x03, 3, &cmd_data[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Set ParameterID = BTN_START_NOISE_EVAL (ret=%d)\n", __func__, retval);
		goto exit;
	}

	/* Do SetParameter ID = BTN_START_NOISE_EVAL exec		W 24 02 03	*/
	_cyttsp4_dbg(dio, CY_LOGLV1, "[TPD_Ic] %s : Do SetParameter ID = BTN_START_NOISE_EVAL exec (W 24 02 03).\n", __func__);
	memset(cmd_data, 0x00, sizeof(cmd_data));
	cmd_data[0]		= 0x03;							/* 0x03 */
	retval = _cyttsp4_ioctl_write_block_data(dio, CY_COMMAND_REG, 1, &cmd_data[0], LOG_I2C_ADRR, true);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Do SetParameter ID (= BTN_START_NOISE_EVAL) (ret=%d)\n", __func__, retval);
		goto exit;
	}

	/* (wait for communications interrupt)			Accept BTN_START_NOISE_EVAL => int wait(bit6) */
	retval = _cyttsp4_ioctl_complete_wait(dio, CY_WAITCMD_SLEEP_PRESS);
	if (retval < 0) {
		goto exit;
	}

	/* wait 67 01 00 */
	retval = _cyttsp4_ioctl_complete_wait_Noise_Eval(dio, CY_WAITCMD_PRS_NOISE_EVAL);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "%s: Do SetParameter ID (= BTN_START_NOISE_EVAL) (ret=%d)\n", __func__, retval);
		goto exit;
	}

	rc = 0;
exit:

	return rc;
}

/* BTN_ALT_TX1_POSITIVE_SUM */
static int _cyttsp4_ioctl_get_BTN_ALT_TX_POSINEGA_SUM(struct cyttsp4_ioctl_data *dio , struct fj_touch_press_noise_eval *press_noise_eval)
{
	int ii;
	int rc = 0, retval = 0;
	u32		io_val = 0;

	for (ii = 0; ii < FJ_TOUCH_PRESS_NOISE_EVAL_NUM; ii++) {
		/* POSITIVE N */
		retval = cyttsp4_request_get_runtime_parm(_DIO_REF_TTSP(dio), (u8)cTbl_BTN_ALT_TX_POSITIVE_SUM[ ii ], &io_val);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: Get POSITIVE_SUM(%d), ret = %d\n, reg = 0x%02X", __func__, (ii + 1), retval, (u8)cTbl_BTN_ALT_TX_POSITIVE_SUM[ ii ]);
			rc = -1;
			break;
		}
		press_noise_eval->positive_sum[ ii ] = (io_val & 0xFFFF);

		/* NEGATIVE N */
		retval = cyttsp4_request_get_runtime_parm(_DIO_REF_TTSP(dio), (u8)cTbl_BTN_ALT_TX_NEGATIVE_SUM[ ii ], &io_val);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: Get NEGATIVE_SUM(%d), ret = %d\n, reg = 0x%02X", __func__, (ii + 1), retval, (u8)cTbl_BTN_ALT_TX_NEGATIVE_SUM[ ii ]);
			rc = -1;
			break;
		}
		press_noise_eval->negative_sum[ ii ] = (io_val & 0xFFFF);

		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] POSI_%d(0x%02X) = 0x%04X, NEGA_%d(0x%02X) = 0x%04X.\n",
					(ii + 1), (u8)cTbl_BTN_ALT_TX_POSITIVE_SUM[ ii ], press_noise_eval->positive_sum[ ii ],
					(ii + 1), (u8)cTbl_BTN_ALT_TX_NEGATIVE_SUM[ ii ], press_noise_eval->negative_sum[ ii ]);
	}

	return rc;
}

static int _cyttsp4_ioctl_get_BTN_DEFAULT_ALT_TX_PERIOD(struct cyttsp4_ioctl_data *dio , signed char *def_tx_period)
{
	int rc = -1, retval = 0;
	u32		io_val = 0;

	*def_tx_period = 0;

	retval = cyttsp4_request_get_runtime_parm(_DIO_REF_TTSP(dio), CY_BTN_DEFAULT_ALT_TX_PERIOD, &io_val);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: Get CY_BTN_DEFAULT_ALT_TX_PERIOD ret = %d\n", __func__, retval);
		goto exit;
	}

	*def_tx_period = 0xFF & io_val;

	rc = 0;

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s BTN_DEFAULT_ALT_TX_PERIOD = 0x%02X.\n",  __func__, fmMask(u8, *def_tx_period));

exit:

	return rc;
}

static int _cyttsp4_ioctl_set_BTN_DEFAULT_ALT_TX_PERIOD_command(struct cyttsp4_ioctl_data *dio , struct fj_touch_press_noise_eval *press_noise_eval)
{
	int rc = -1, retval = 0, ii;
	u32		io_val = 0;

	io_val = press_noise_eval->eval_freq[0];

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s BTN_DEFAULT_ALT_TX_PERIOD Set Command! (0x%02X).\n",__func__, fmMask(u8, io_val));
	/* [0] == [1] == [2] == [3] : BTN_DEFAULT_ALT_TX_PERIOD Change Command */

	retval = cyttsp4_request_set_runtime_parm( _DIO_REF_TTSP(dio), CY_BTN_DEFAULT_ALT_TX_PERIOD, CY_BTN_DEFAULT_ALT_TX_PERIOD_PARM_SZ, io_val);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: Set CY_BTN_DEFAULT_ALT_TX_PERIOD ret = %d\n", __func__, retval);
		goto exit;
	}

	retval = _cyttsp4_ioctl_get_BTN_DEFAULT_ALT_TX_PERIOD(dio, &press_noise_eval->def_tx_period);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: get_BTN_ALT_TX_PERIOD ret = %d\n", __func__, retval);
		goto exit;
	}

	for (ii = 0; ii < FJ_TOUCH_PRESS_NOISE_EVAL_NUM; ii++) {
		press_noise_eval->positive_sum[ ii ] = 0;
		press_noise_eval->negative_sum[ ii ] = 0;
	}

	rc = 0;

exit:

	return rc;
}
static int _cyttsp4_ioctl_get_press_noise_eval( struct cyttsp4_ioctl_data *dio , struct fj_touch_press_noise_eval *press_noise_eval)
{
	int rc = -1, retval = 0;

	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [S], freq[0]=0x%02X, freq[1]=0x%02X, freq[2]=0x%02X, freq[3]=0x%02X.\n",
								__func__, fmMask(u8, press_noise_eval->eval_freq[0]), fmMask(u8, press_noise_eval->eval_freq[1]), fmMask(u8, press_noise_eval->eval_freq[2]), fmMask(u8, press_noise_eval->eval_freq[3]));

	if ((press_noise_eval->eval_freq[0] == press_noise_eval->eval_freq[1]) &&
		(press_noise_eval->eval_freq[1] == press_noise_eval->eval_freq[2]) &&
		(press_noise_eval->eval_freq[2] == press_noise_eval->eval_freq[3])) {
		/* [0] == [1] == [2] == [3] : BTN_DEFAULT_ALT_TX_PERIOD Change Command */
		retval = _cyttsp4_ioctl_set_BTN_DEFAULT_ALT_TX_PERIOD_command(dio, press_noise_eval);
		if (retval < 0) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: set_BTN_DEFAULT_ALT_TX_PERIOD_command ret = %d\n", __func__, retval);
			goto exit;
		}
		/* Success */
		rc = 0;
		goto exit;
	}

	retval = _cyttsp4_ioctl_get_BTN_DEFAULT_ALT_TX_PERIOD(dio, &press_noise_eval->def_tx_period);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: get_BTN_ALT_TX_PERIOD ret = %d\n", __func__, retval);
		goto exit;
	}


	retval = _cyttsp4_ioctl_set_BTN_ALT_TX_PERIOD(dio, press_noise_eval);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: set_BTN_ALT_TX_PERIOD ret = %d\n", __func__, retval);
		goto exit;
	}

	retval = _cyttsp4_ioctl_do_BTN_START_NOISE_EVAL( dio );
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: do_BTN_START_NOISE_EVAL ret = %d\n", __func__, retval);
		goto exit;
	}

	retval = _cyttsp4_ioctl_get_BTN_ALT_TX_POSINEGA_SUM(dio, press_noise_eval);
	if (retval < 0) {
		dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic] %s: get_BTN_ALT_TX_POSINEGA_SUM ret = %d\n", __func__, retval);
		goto exit;
	}

	rc = 0;

exit:
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s [E] = %d.\n", __func__, rc);
	return rc;
}

/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add end */

/*
 * main function
 */
#if 0
__main___________(){}
#endif

static long cyttsp4_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret   = 0;
	enum devicepowerstatus	pow_mode;
	u8 verdata[CY_FIRMVERSION_SIZE];
	struct cyttsp4_ioctl_data *dio = file->private_data;
	struct fj_touch_i2c_data i2c_data;
	struct fj_touch_do_selftest_data do_selftest_data;
	struct fj_touch_debug_diagnostic_data debug_raw_data;
	u8 *rdata = NULL; /* read	*/
	u8 *wdata = NULL; /* write	*/
	unsigned char pin_mask[9]; /* selftest	*/
	int anti_warning = 0;
	bool use_subaddr = false;
	unsigned char result_data_buf[3]; /* selftest	*/
	unsigned char *self_test_data = NULL; /* selftest	*/
	unsigned char test_cmd[5]; /* raw	*/
	unsigned char *init_data = NULL; /* raw	*/
	unsigned int number_of_element = 0; /* raw	*/
	enum fj_touch_get_device_mode get_mode = 0;
	struct fj_touch_firmware_data firmware_data; /* 13-1stUpdate Firmware */
	struct fj_touch_pressure_info				pressure_info;
	struct fj_touch_do_calibration_pressure		do_calibration_pressure;
	struct fj_touch_touchpress_ctrl				touch_press;
	struct fj_touch_ret_calibration_pressure	ret_calibration_pressure;
/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add start */
	struct fj_touch_press_normalization			press_normalize;
/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add end */
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add start */
	struct fj_touch_press_noise_eval			press_noise_eval;
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add end */
	pr_info("[TPD_Ic] cyttsp4_ioctl [S] cmd=0x%08X.\n", cmd);

	if (atomic_read( &dio->atm_mc_stat) == 0x02) {
		dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Error.\n", __func__);
		goto cyttsp4_ioctl_exit;
	}

	switch (cmd) {
	case IOCTL_SET_POWERMODE:
		pr_info("[TPD_Ic] Enter Set PowerMode\n");
		ret = copy_from_user( &pow_mode, (int __user *) arg, sizeof(int));
		if (ret != 0) {
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			break;
		}
		pr_info("[TPD_Ic] SetPowerMode=%d\n", pow_mode);
		ret = _cyttsp4_ioctl_set_powermode( dio , pow_mode );
		break; /* case IOCTL_SET_POWERMODE : */

	case IOCTL_GET_FIRMVERSION:
		pr_info("[TPD_Ic] Enter FIRMVERSION\n");
		memset(&verdata[0], 0xFF, sizeof(verdata));

		if (_cyttsp4_ioctl_IsCoreSleep(dio) == true) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Cannot Get Firmversion\n", __func__);
			ret = -EFAULT;
			goto get_firmversion_exit;
		}

		ret= cyttsp4_get_firmversion(dio, verdata);
		if (ret != 0) {
			goto get_firmversion_exit;
		}

		/* firm data dump */
		_cyttsp4_buf_dump(verdata, sizeof(verdata), "FIRMVERSION", CY_LOGLV1);	/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE mod */

get_firmversion_exit:
		if (ret != 0) {
			anti_warning = copy_to_user( (int __user *) arg, &verdata[0], sizeof(verdata) );
		} else {
			ret = copy_to_user( (int __user *) arg, &verdata[0], sizeof(verdata) );
			if (ret != 0) {
				ret = -EIO;
				dev_err( _DIO_REF_DEV(dio) , "copy_to_user err %d \n", ret);
			}
		}
		break;	/* case IOCTL_GET_FIRMVERSION : */

	case IOCTL_DO_CALIBRATION:
		pr_info("[TPD_Ic] Enter CALIBRATION\n");

		if (_cyttsp4_ioctl_IsCoreSleep(dio) == true) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Cannot Calibration\n", __func__);
			ret = -EFAULT;
			goto calibrate_exit;
		}
		ret = _cyttsp4_calibrate(dio);

calibrate_exit:
		break;	/* case IOCTL_DO_CALIBRATION : */

	case IOCTL_I2C_READ:
		pr_info("[TPD_Ic] Enter I2C_READ\n");
		memset(&i2c_data, 0, sizeof(i2c_data));

		ret = copy_from_user( &i2c_data, (int __user *) arg, sizeof(i2c_data));
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			break;
		}
/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add start */
		if (i2c_data.length <= 0) {			/* int */
			ret = -EINVAL;
			dev_err( _DIO_REF_DEV(dio) , "Invalid parm (len = %d).\n", i2c_data.length);
			break;
		}
/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add end */
		rdata = kzalloc(i2c_data.length, GFP_KERNEL);
		if (rdata == NULL) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "rdata kzalloc fail %d \n", ret);
			break;
		}
		memset( rdata, 0xFF, i2c_data.length );

		if (_cyttsp4_ioctl_IsCoreSleep(dio) == true) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Cannot I2C Read\n", __func__);
			ret = -EFAULT;
			goto read_exit;
		}
		if (i2c_data.use_offset != 0) {
			use_subaddr = true;
		}
		ret = _cyttsp4_ioctl_read_block_data(dio, (u8)i2c_data.offset, (size_t)i2c_data.length, rdata, i2c_data.slaveaddress, use_subaddr);
		
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "i2c_read err %d \n", ret);
		}
read_exit:
		if (ret != 0) {
			anti_warning = copy_to_user( i2c_data.i2c_data_buf, rdata, i2c_data.length );
		} else {
			ret = copy_to_user( i2c_data.i2c_data_buf, rdata, i2c_data.length );
			if (ret != 0) {
				ret = -EIO;
				dev_err( _DIO_REF_DEV(dio) , "copy_to_user err %d \n", ret);
				anti_warning = copy_to_user( (int __user *) arg, &i2c_data, sizeof(i2c_data) );
			}
		}
		if (rdata != NULL) {
			kfree( rdata );
		}
		break;	/* case IOCTL_I2C_READ : */

	case IOCTL_I2C_WRITE:
		pr_info("[TPD_Ic] Enter I2C_WRITE\n");
		memset(&i2c_data, 0, sizeof(i2c_data));

		if (_cyttsp4_ioctl_IsCoreSleep(dio) == true) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Cannot I2C Write\n", __func__);
			ret = -EFAULT;
			break;
		}

		ret = copy_from_user( &i2c_data, (int __user *) arg, sizeof(i2c_data));
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			break;
		}
/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add start */
		if (i2c_data.length <= 0) {			/* int */
			ret = -EINVAL;
			dev_err( _DIO_REF_DEV(dio) , "Invalid parm (len = %d).\n", i2c_data.length);
			break;
		}
/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add end */

		wdata = kzalloc(i2c_data.length, GFP_KERNEL);
		if (wdata == NULL) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "wdata kzalloc fail %d \n", ret);
			break;
		}
		memset( wdata, 0xFF, i2c_data.length );
		
		/* "data.i2c_data_buf" is user space pointer */
		ret = copy_from_user( wdata, i2c_data.i2c_data_buf, i2c_data.length );
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			goto write_exit;
		}

		if (i2c_data.use_offset != 0) {
			use_subaddr = true;
		}

		ret = _cyttsp4_ioctl_write(dio, (u8) i2c_data.offset, (size_t)i2c_data.length, wdata, i2c_data.slaveaddress, use_subaddr);

		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "i2c_write err %d \n", ret);
		}

write_exit:
		if (wdata != NULL) {
			kfree( wdata );
		}
		
		break;	/* case IOCTL_I2C_WRITE : */

	case IOCTL_DO_SELFTEST:
		pr_info("[TPD_Ic] Enter SELFTEST\n");
		memset(&do_selftest_data, 0, sizeof(do_selftest_data));
		memset(pin_mask, 0, sizeof(pin_mask));
		memset(result_data_buf, 0xFF, sizeof(result_data_buf));

		ret = copy_from_user( &do_selftest_data, (int __user *) arg, sizeof(do_selftest_data));
		if (ret != 0) {
			ret = -EINVAL;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			break;
		}
/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add start */
		if (do_selftest_data.load_length == 0) {	/* unsigned int */
			ret = -EINVAL;
			dev_err( _DIO_REF_DEV(dio) , "Invalid parm (len = %d).\n", do_selftest_data.load_length);
			break;
		}
/* FUJITSU:2013-05-28 HMW_TOUCH_ERR_CHECK add end */

		/* Don't use User Spase to the I2C */
		ret = copy_from_user( pin_mask, do_selftest_data.pin_mask, sizeof(pin_mask) );
		if (ret != 0) {
			ret = -EINVAL;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			break;
		}
		
		self_test_data = kzalloc(do_selftest_data.load_length, GFP_KERNEL);
		if (self_test_data == NULL) {
			ret = -EINVAL;
			dev_err( _DIO_REF_DEV(dio) , "self_test_data kzalloc fail %d \n", ret);
			break;
		}
		memset(self_test_data, 0xFF, do_selftest_data.load_length);
		
		if (_cyttsp4_ioctl_IsCoreSleep(dio) == true) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Cannot I2C Self Test\n", __func__);
			ret = -EFAULT;
			goto selftest_exit;
		}

		ret = cyttsp4_selftest(dio, &do_selftest_data, pin_mask, result_data_buf, self_test_data );
		
selftest_exit:
		/* Don't use User Spase to the I2C */
		if (ret != 0) {
			anti_warning = copy_to_user( do_selftest_data.result_data_buf, result_data_buf, sizeof(result_data_buf) );
			anti_warning = copy_to_user( do_selftest_data.self_test_data, self_test_data, do_selftest_data.load_length );
		} else {
			ret = copy_to_user( do_selftest_data.result_data_buf, result_data_buf, sizeof(result_data_buf) );
			if (ret != 0) {
				ret = -EINVAL;
				dev_err( _DIO_REF_DEV(dio) , "copy_to_user err *result_data_buf* %d \n", ret);
				anti_warning = copy_to_user( do_selftest_data.self_test_data, self_test_data, do_selftest_data.load_length );
			} else {
				ret = copy_to_user( do_selftest_data.self_test_data, self_test_data, do_selftest_data.load_length );
				if (ret != 0) {
					ret = -EINVAL;
					dev_err( _DIO_REF_DEV(dio) , "copy_to_user err *self_test_data* %d \n", ret);
				}
			}
		}

		if (self_test_data != NULL) {
			kfree(self_test_data);
		}
		break;	/* case IOCTL_DO_SELFTEST : */

	case IOCTL_DEBUG_DIAGNOSTIC:
		pr_info("[TPD_Ic] Enter RawCheck\n");

		memset(&debug_raw_data, 0, sizeof(debug_raw_data));
		memset(test_cmd, 0, sizeof(test_cmd));

		ret = copy_from_user(&debug_raw_data, (int __user *) arg, sizeof(debug_raw_data));
		if (ret != 0) {
			ret = -EINVAL;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			break;
		}

		if (debug_raw_data.multi_scan == MULTI_UPDATE) {
			/* Don't use User Spase to the I2C */
			ret = copy_from_user( test_cmd, debug_raw_data.test_cmd, sizeof(test_cmd));
			if (ret != 0) {
				ret = -EINVAL;
				dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
				break;
			}
			/* Get Kernel space memory, Don't use User Spase to the I2C */
			number_of_element = (( test_cmd[2] * 256 ) + test_cmd[3] );
			init_data = kzalloc(number_of_element, GFP_KERNEL);
			if (init_data == NULL) {
				ret = -EINVAL;
				dev_err( _DIO_REF_DEV(dio) , "init_data kzalloc fail %d \n", ret);
				break;
			}
			memset( init_data, 0, number_of_element );
			
			ret = copy_to_user( debug_raw_data.data, init_data, number_of_element );
			if (ret != 0) {
				ret = -EINVAL;
				dev_err( _DIO_REF_DEV(dio) , "copy_to_user err %d \n", ret);
				if (init_data != NULL) {
					kfree( init_data );
				}
				goto raw_exit;
			}
			if (init_data != NULL) {
				kfree(init_data);
			}
		}

		if (_cyttsp4_ioctl_IsCoreSleep(dio) == true) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Cannot I2C RawCheck\n", __func__);
			ret = -EFAULT;
			goto raw_exit;
		}

		ret = cyttsp4_raw_check(dio, &debug_raw_data, test_cmd);
		
raw_exit:
		if (ret != 0) { /* If get error code, status_code value is return */
			anti_warning = copy_to_user( (int __user *) arg, &debug_raw_data, sizeof(debug_raw_data) );
		} else {
			ret = copy_to_user( (int __user *) arg, &debug_raw_data, sizeof(debug_raw_data) );
			if (ret != 0) {
				ret = -EINVAL;
				dev_err( _DIO_REF_DEV(dio) , "copy_to_user err %d \n", ret);
			}
		}

		break; /* case IOCTL_DEBUG_DIAGNOSTIC : */

	case IOCTL_GET_UPDATEFIRMINFO:
		pr_info("[TPD_Ic] Enter UPDATEFIRMINFO\n");
		memset(&firmware_data, 0, sizeof(firmware_data)); /* 13-1stUpdate Firmware */

		ret = _cyttsp4_ioctl_get_updatefirminfo(dio, &firmware_data);
		if (ret != 0) {
			goto get_updatefirminfo;
		}

		ret = copy_to_user( (int __user *) arg, &firmware_data, sizeof(firmware_data) );
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_to_user err %d \n", ret);
			goto get_updatefirminfo;
		}

get_updatefirminfo:

		break; /* case IOCTL_GET_UPDATEFIRMINFO : */

	case IOCTL_SET_TOUCHMODE:
		pr_info("[TPD_Ic] nop ...\n");			/* 132HMW_Pending... Not Support. */
		pr_info("[TPD_Ic] Enter SetTouchMode\n");

		break; /* case IOCTL_SET_TOUCHMODE : */

	case IOCTL_GET_TOUCHMODE:
		pr_info("[TPD_Ic] nop ...\n");			/* 132HMW_Pending... Not Support. */
		pr_info("[TPD_Ic] Enter GetTouchMode\n");

		get_mode = NORMAL_FINGER_MODE;
		ret = copy_to_user( (int __user *) arg, &get_mode, sizeof(get_mode) );
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_to_user err %d \n", ret);
		}
		break; /* case IOCTL_GET_TOUCHMODE : */

	case IOCTL_GET_PRESSUREINFO:
		pr_info("[TPD_Ic] Enter GetPressureInfo\n");
		memset(&pressure_info, 0, sizeof(pressure_info));

		if (_cyttsp4_ioctl_IsCoreSleep(dio) == true) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Cannot GetPressureInfo\n", __func__);
			ret = -EFAULT;						/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE add */
			goto get_pressureinfo_exit;
		}
		
		ret = copy_from_user( &pressure_info, (int __user *) arg, sizeof(pressure_info));
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			goto get_pressureinfo_exit;
		}

		ret = _cyttsp4_ioctl_get_pressure_info(dio, &pressure_info);

get_pressureinfo_exit:

		if (ret != 0) {
			anti_warning = copy_to_user( (int __user *) arg, &pressure_info, sizeof(pressure_info) );
		} else {
			ret = copy_to_user( (int __user *) arg, &pressure_info, sizeof(pressure_info) );
			if (ret != 0) {
				ret = -EIO;
				dev_err( _DIO_REF_DEV(dio) , "copy_to_user err %d \n", ret);
			}
		}
		break; /* case IOCTL_GET_PRESSUREINFO : */

	case IOCTL_DO_CALIBRATION_PRESSURE:
		pr_info("[TPD_Ic] Enter DoCalibrationPressure\n");
		memset(&do_calibration_pressure, 0, sizeof(do_calibration_pressure));

		if (_cyttsp4_ioctl_IsCoreSleep(dio) == true) {
			dev_err( _DIO_REF_DEV(dio) , "[TPD_Ic]%s: Cannot DoCalibrationPressure\n", __func__);
			ret = -EFAULT;
			goto do_calibration_pressure_exit;
		}
		
		ret = copy_from_user( &do_calibration_pressure, (int __user *) arg, sizeof(do_calibration_pressure));
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			goto do_calibration_pressure_exit;
		}
		pr_info("[TPD_Ic] do_calib_type = 0x%08X.\n", do_calibration_pressure.do_calib_type);

		if ((do_calibration_pressure.do_calib_type & FJTOUCH_CALIB_PRESS_PTN1) != 0) {
			ret = _cyttsp4_ioctl_do_calibration_pressure_PWCs(dio, &do_calibration_pressure);
			if (ret != 0) {
				dev_err( _DIO_REF_DEV(dio) , "do_calib prs_PWCs err %d \n", ret);
				goto do_calibration_pressure_exit;
			}
		}
		if ((do_calibration_pressure.do_calib_type & FJTOUCH_CALIB_PRESS_PTN2) != 0) {
			ret = _cyttsp4_ioctl_do_calibration_pressure_GIDAC(dio, &do_calibration_pressure);
			if (ret != 0) {
				dev_err( _DIO_REF_DEV(dio) , "do_calib prs_GIDAC err %d \n", ret);
				goto do_calibration_pressure_exit;
			}
		}

do_calibration_pressure_exit:

		break; /* case IOCTL_DO_CALIBRATION_PRESSURE: */

	case IOCTL_SET_TOUCHPRESS:
		pr_info("[TPD_Ic] Enter Set Touch Press\n");
		memset(&touch_press, 0, sizeof(touch_press));

		ret = copy_from_user( &touch_press, (int __user *) arg, sizeof(touch_press));
		if (ret != 0) {
			ret = -EFAULT;
			dev_err(_DIO_REF_DEV(dio), "copy_from_user err %d \n", ret);
			goto set_touchpress_exit;
		}

		ret = _cyttsp4_ioctl_touchpress_ctrl(dio, true, &touch_press);
		if (ret != 0) {
			dev_err( _DIO_REF_DEV(dio) , "set_touchpress_ctrl = %d \n", ret);
			goto set_touchpress_exit;
		}

set_touchpress_exit:

		break; /* case IOCTL_SET_TOUCHPRESS : */

	case IOCTL_GET_TOUCHPRESS:
		pr_info("[TPD_Ic] Enter Get Touch Press\n");
		memset(&touch_press, 0, sizeof(touch_press));

		ret = _cyttsp4_ioctl_touchpress_ctrl(dio, false, &touch_press);
		if (ret != 0) {
			dev_err( _DIO_REF_DEV(dio) , "get_touchpress_ctrl = %d \n", ret);
			goto get_touchpress_exit;
		}

		ret = copy_to_user( (int __user *) arg, &touch_press, sizeof(touch_press) );
		if (ret != 0) {
			ret = -EIO;
			dev_err(_DIO_REF_DEV(dio), "copy_to_user err %d \n", ret);
		}

get_touchpress_exit:

		break; /* case IOCTL_GET_TOUCHPRESS : */

	case IOCTL_GET_RET_CALIB_PRESSURE:
		pr_info("[TPD_Ic] Enter Get Ret Calib Pressure\n");
		memset(&ret_calibration_pressure, 0, sizeof(ret_calibration_pressure));

		ret = copy_from_user( &ret_calibration_pressure, (int __user *) arg, sizeof(ret_calibration_pressure));
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			goto get_ret_calib_pressure_exit;
		}

		ret = _cyttsp4_ioctl_get_ret_calib_pressure(dio, &ret_calibration_pressure);
		if (ret != 0) {
			dev_err( _DIO_REF_DEV(dio) , "get ret calib = %d \n", ret);
			goto get_ret_calib_pressure_exit;
		}

		ret = copy_to_user( (int __user *) arg, &ret_calibration_pressure, sizeof(ret_calibration_pressure) );
		if (ret != 0) {
			ret = -EIO;
			dev_err(_DIO_REF_DEV(dio), "copy_to_user err %d \n", ret);
		}

get_ret_calib_pressure_exit:

		break;	/* case IOCTL_GET_RET_CALIB_PRESSURE : */

/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add start */
	case IOCTL_GET_TOUCH_POSITION:
		pr_info("[TPD_Ic] Enter Get Touch Position\n");
		memset( &dio->touch_pos.pos_info, 0, sizeof(dio->touch_pos.pos_info));

		ret = copy_from_user( &dio->touch_pos.pos_info, (int __user *) arg, sizeof(dio->touch_pos.pos_info));
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			goto get_touch_position_exit;
		}

		ret = _cyttsp4_ioctl_get_touch_position( dio );
		if (ret != 0) {
			dev_err( _DIO_REF_DEV(dio) , "get touch pos = %d \n", ret);
			goto get_touch_position_exit;
		}

		ret = copy_to_user( (int __user *) arg, &dio->touch_pos.pos_info, sizeof(dio->touch_pos.pos_info) );
		if (ret != 0) {
			ret = -EIO;
			dev_err(_DIO_REF_DEV(dio), "copy_to_user err %d \n", ret);
		}

get_touch_position_exit:

		break;	/* case IOCTL_GET_TOUCH_POSITION: */

	case IOCTL_SET_PRESS_NORMALIZATION:
		pr_info("[TPD_Ic] Enter Set Press Normalize\n");
		memset(&press_normalize, 0, sizeof(press_normalize));

		ret = copy_from_user( &press_normalize, (int __user *) arg, sizeof(press_normalize));
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			goto set_press_nomalization_exit;
		}

		ret = _cyttsp4_ioctl_set_press_normalize( dio , &press_normalize );

		if (ret != 0) {
			dev_err( _DIO_REF_DEV(dio) , "set press normalize = %d \n", ret);
			goto set_press_nomalization_exit;
		}


set_press_nomalization_exit:

		break; /* case IOCTL_SET_PRESS_NORMALIZATION:*/
/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add end */
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add start */
	case IOCTL_GET_PRESS_NOISE_EVAL:
		pr_info("[TPD_Ic] Enter Get Press Noise Eval\n");
		memset(&press_noise_eval, 0, sizeof(press_noise_eval));

		ret = copy_from_user( &press_noise_eval, (int __user *) arg, sizeof(press_noise_eval));
		if (ret != 0) {
			ret = -EIO;
			dev_err( _DIO_REF_DEV(dio) , "copy_from_user err %d \n", ret);
			goto get_press_noise_eval_exit;
		}

		ret = _cyttsp4_ioctl_get_press_noise_eval( dio , &press_noise_eval );

		if (ret != 0) {
			dev_err( _DIO_REF_DEV(dio) , "get press noise eval = %d \n", ret);
			goto get_press_noise_eval_exit;
		}

		ret = copy_to_user( (int __user *) arg, &press_noise_eval, sizeof(press_noise_eval) );
		if (ret != 0) {
			ret = -EIO;
			dev_err(_DIO_REF_DEV(dio), "copy_to_user err %d \n", ret);
			goto get_press_noise_eval_exit;
		}

get_press_noise_eval_exit:
		break; /* case IOCTL_GET_PRESS_NOISE_EVAL: */
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add end */

	default:
		dev_err( _DIO_REF_DEV(dio) , "unkwon cmd ! \n");
		ret = -ENOTTY;
		break;
	}

cyttsp4_ioctl_exit:

	pr_info("[TPD_Ic] return code = %d, cmd=0x%08X.\n", ret, cmd);
	return (long)ret;

}

/*
 * IOCTL main
 */

/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL_EARLY_SUSPEND add start */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp4_ioctl_early_suspend(struct early_suspend *h)
{
	struct cyttsp4_ioctl_data *dio = container_of( h, struct cyttsp4_ioctl_data, early_susp );
	int rc;
	u8	press_info[8 + 1];
	int	sup_prs, sup_hov;

	memset(&press_info[0], 0, sizeof(press_info));

	rc = cyttsp4_request_get_information( _DIO_REF_TTSP(dio) , CY_INFOTYPE_PRESS_CTRL_INFO, CY_INFOOPT_NONE, (void*)&press_info[0]);
	if (rc < 0) {
		_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] press info get error.\n");
	}

	sup_prs = cyttsp4_request_is_support( _DIO_REF_TTSP(dio), CY_CORE_SUPPORT_PRESS);
	sup_hov = cyttsp4_request_is_support( _DIO_REF_TTSP(dio), CY_CORE_SUPPORT_HOVER);
	_cyttsp4_dbg(dio, CY_LOGLV0, "[TPD_Ic] %s (%d,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X) sup(%d,%d).\n",
				__func__, _cyttsp4_ioctl_IsCoreProbeErr( dio ),
				press_info[0], press_info[1], press_info[2], press_info[3], press_info[4], press_info[5], press_info[6], press_info[7],
				sup_prs, sup_hov);
				/* [0] : press_setting_val, [1] : press_setting_req, [2] : press_normalize_nv_read, [3] : press_normalize_sts, [4] : press_force_calib_sts */
}

static void cyttsp4_ioctl_late_resume(struct early_suspend *h)
{

}

static void _cyttsp4_ioctl_setup_early_suspend(struct cyttsp4_ioctl_data *dio)
{
	dio->early_susp.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	dio->early_susp.suspend	= cyttsp4_ioctl_early_suspend;
	dio->early_susp.resume	= cyttsp4_ioctl_late_resume;

	register_early_suspend( &dio->early_susp );
}
#endif /* CONFIG_HAS_EARLYSUSPEND */
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL_EARLY_SUSPEND add end */

#ifdef CONFIG_PM_SLEEP
static int cyttsp4_ioctl_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static int cyttsp4_ioctl_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	return 0;
}
#endif

static const struct dev_pm_ops cyttsp4_ioctl_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cyttsp4_ioctl_suspend, cyttsp4_ioctl_resume)
};

#if 0
static int cyttsp4_setup_sysfs(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_device_access_data *dad = dev_get_drvdata(dev);
	int rc = 0;

	rc = device_create_file(dev, &dev_attr_ic_grpnum);
	if (rc) {
		dev_err(dev, "%s: Error, could not create ic_grpnum\n",
				__func__);
		goto exit;
	}

	rc = device_create_file(dev, &dev_attr_ic_grpoffset);
	if (rc) {
		dev_err(dev, "%s: Error, could not create ic_grpoffset\n",
				__func__);
		goto unregister_grpnum;
	}

	rc = device_create_file(dev, &dev_attr_ic_grpdata);
	if (rc) {
		dev_err(dev, "%s: Error, could not create ic_grpdata\n",
				__func__);
		goto unregister_grpoffset;
	}

	rc = device_create_file(dev, &dev_attr_get_panel_data);
	if (rc) {
		dev_err(dev, "%s: Error, could not create get_panel_data\n",
				__func__);
		goto unregister_grpdata;
	}

	dad->sysfs_nodes_created = true;
	return rc;

unregister_grpdata:
	device_remove_file(dev, &dev_attr_get_panel_data);
unregister_grpoffset:
	device_remove_file(dev, &dev_attr_ic_grpoffset);
unregister_grpnum:
	device_remove_file(dev, &dev_attr_ic_grpnum);
exit:
	return rc;
}
#endif

static int cyttsp4_setup_sysfs_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_ioctl_data *dio = dev_get_drvdata(dev);
	int rc = 0;

	dev_vdbg(dev, "%s\n", __func__);

	dio->si = cyttsp4_request_sysinfo(ttsp);
	if (!dio->si)
		return -1;

//	rc = cyttsp4_setup_sysfs(ttsp);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
		cyttsp4_setup_sysfs_attention, 0);

	return rc;

}

static int cyttsp4_ioctl_open( struct inode *inode, struct file *file )
{
	int retval = 0;
	struct cyttsp4_ioctl_data *dio = container_of( inode->i_cdev,struct cyttsp4_ioctl_data, cyttsp4_cdev );
	struct device *dev = &dio->ttsp->dev;

	pr_info("[TPD_Ic] %s.\n", __func__);

/* prob_err start */
	if (_cyttsp4_ioctl_IsCoreProbeErr(dio) == true) {
		dev_err( dev, "%s : prob err access inhibit !\n", __func__ );
		return -EFAULT;
	}
/* prob_err end */

	if (atomic_read( &dio->atm_mc_stat ) != 0x00) {
		dev_err( dev, "%s : double open !\n", __func__ );
		atomic_set( &dio->atm_mc_stat , 0x02);
		retval = -EBUSY;
	} else {
		atomic_set( &dio->atm_mc_stat , 0x01);
		dio->curr_mode	= CY_MODE_OPERATIONAL;
		dio->chk_bootloadermode_ctrl	= false;
	}

    file->private_data = dio;

	return retval;
}

static int cyttsp4_ioctl_close(struct inode *inode, struct file *file)
{
	struct cyttsp4_ioctl_data *dio = container_of(inode->i_cdev,struct cyttsp4_ioctl_data, cyttsp4_cdev);

	pr_info("[TPD_Ic] %s, res = 0x%02X.\n", __func__, dio->curr_resource);

	if ((dio->curr_resource & CY_IC_RES_MODECTRL) != CY_IC_RES_NONE) {
		cyttsp4_request_set_mode(dio->ttsp, CY_MODE_OPERATIONAL);
	}

	if ((dio->curr_resource & CY_IC_RES_EXCLUSIVE) != CY_IC_RES_NONE) {
		cyttsp4_release_exclusive(dio->ttsp);
	}

	dio->curr_mode	= CY_MODE_OPERATIONAL;
	dio->curr_resource	= CY_IC_RES_NONE;
	atomic_set( &dio->atm_mc_stat , 0x00);

	return 0;
}

static struct file_operations cyttsp4_fops = {
    .owner          = THIS_MODULE,
	.release        = cyttsp4_ioctl_close,
    .open           = cyttsp4_ioctl_open,
    .unlocked_ioctl = cyttsp4_ioctl,
};


static int cyttsp4_ioctl_probe(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_ioctl_data *dio;
	struct cyttsp4_ioctl_platform_data *pdata = dev_get_platdata(dev);
	dev_t dev_ioctl;
	int devno;
	int rc = 0;
	struct cyttsp4_ioctl_touch_pos_info		*tch_pos = NULL;		/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add */

	_cyttsp4_hlog(dev, "%s [S].\n", __func__);
	dev_dbg(dev, "%s: debug on\n", __func__);
	dev_vdbg(dev, "%s: verbose debug on\n", __func__);

	dio = kzalloc(sizeof(*dio), GFP_KERNEL);
	if (dio == NULL) {
		dev_err(dev, "%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto cyttsp4_ioctl_probe_data_failed;
	}

	dio->ttsp = ttsp;
	dio->pdata = pdata;
	dev_set_drvdata(dev, dio);

	pm_runtime_enable(dev);

	/* get sysinfo */
	dio->si = cyttsp4_request_sysinfo(ttsp);
	if (dio->si) {
//		rc = cyttsp4_setup_sysfs(ttsp);
		if (rc)
			goto cyttsp4_ioctl_setup_sysfs_failed;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
				__func__, dio->si);
		cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
			cyttsp4_setup_sysfs_attention, 0);
	}

	dev_ioctl = MKDEV(cdev_major, 0);
	udev_class = class_create(THIS_MODULE, "fj_touch");

	rc = alloc_chrdev_region(&dev_ioctl, 0, 1, "fj_touch");
	cdev_major = MAJOR(dev_ioctl);
	if (cdev_major == 0) {
		cdev_major = rc;
	}

	devno = MKDEV( cdev_major, 0 ); 
	cdev_init( &(dio->cyttsp4_cdev), &cyttsp4_fops );
	dio->cyttsp4_cdev.owner = THIS_MODULE;
	dio->cyttsp4_cdev.ops = &cyttsp4_fops;
	rc = cdev_add ( &(dio->cyttsp4_cdev), devno, 1 );

	device_create(udev_class, NULL, devno, NULL, "fj_touch");

	atomic_set( &dio->atm_mc_stat , 0x00);
	dio->curr_mode		= CY_MODE_OPERATIONAL;
	dio->curr_resource	= CY_IC_RES_NONE;
	dio->chk_bootloadermode_ctrl	= false;
/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add start */
	tch_pos = &dio->touch_pos;
	mutex_init( &dio->report_lock );
	atomic_set( &tch_pos->atm_status , CY_IC_TOUCHPOS_STS_IDLE);			/* init */
	atomic_set( &tch_pos->atm_curr_cnt , 0);								/* init */

	/* Registration of Startup Handler for HW/SW-RESET. */
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP, cyttsp4_ioctl_attention_startup, 0);
/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add end */

/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG addstart */
	dio->nv_info = cyttsp4_request_ref_nonvolatile_info( ttsp );
/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add end */
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add start */
	cyttsp4_request_set_ioctl_handle( ttsp, (void *)dio );
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add end */

/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL_EARLY_SUSPEND add start */
#ifdef CONFIG_HAS_EARLYSUSPEND
	_cyttsp4_ioctl_setup_early_suspend( dio );
#endif /* CONFIG_HAS_EARLYSUSPEND */
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL_EARLY_SUSPEND add end */

	_cyttsp4_hlog(dev, "%s [E] ok.\n", __func__);
	return 0;

 cyttsp4_ioctl_setup_sysfs_failed:
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
	dev_set_drvdata(dev, NULL);
	kfree(dio);
 cyttsp4_ioctl_probe_data_failed:
	dev_err(dev, "%s failed.\n", __func__);
	return rc;
}

static int cyttsp4_ioctl_release(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_ioctl_data *dio = dev_get_drvdata(dev);

	_cyttsp4_hlog(dev, "%s\n", __func__);

	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);

/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add start */
	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP, cyttsp4_ioctl_attention_startup, 0);
/* FUJITSU:2013-05-10 HMW_TTDA_TOUCH_POS add end */
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add start */
	cyttsp4_request_set_ioctl_handle( ttsp, (void *)NULL );
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add end */

	dev_set_drvdata(dev, NULL);
	kfree(dio);
	return 0;
}

static struct cyttsp4_driver cyttsp4_ioctl_driver = {
	.probe = cyttsp4_ioctl_probe,
	.remove = cyttsp4_ioctl_release,
	.driver = {
		.name = CYTTSP4_IOCTL_NAME,
		.bus = &cyttsp4_bus_type,
		.owner = THIS_MODULE,
		.pm = &cyttsp4_ioctl_pm_ops,
	},
};

static struct cyttsp4_ioctl_platform_data 
               _cyttsp4_ioctl_platform_data = {
	.ioctl_dev_name = CYTTSP4_IOCTL_NAME,
};
static const char cyttsp4_ioctl_name[] = CYTTSP4_IOCTL_NAME;
static struct cyttsp4_device_info cyttsp4_ioctl_infos;

static int __init cyttsp4_ioctl_init(void)
{
	int rc = 0;

	cyttsp4_ioctl_infos.name = cyttsp4_ioctl_name;
	cyttsp4_ioctl_infos.core_id = CY_DEFAULT_CORE_ID;
	cyttsp4_ioctl_infos.platform_data = &_cyttsp4_ioctl_platform_data;
	pr_info("%s: Registering ioctl device for core_id: %s\n", __func__, cyttsp4_ioctl_infos.core_id);

	rc = cyttsp4_register_device(&cyttsp4_ioctl_infos);
	if (rc < 0) {
		pr_err("%s: Error, failed registering device\n", __func__);
		return rc;
	}

	rc = cyttsp4_register_driver(&cyttsp4_ioctl_driver);
	if (rc) {
		pr_err("%s: Error, failed registering driver\n", __func__);
		cyttsp4_unregister_device(cyttsp4_ioctl_infos.name, cyttsp4_ioctl_infos.core_id);
	}

	pr_info("%s: Cypress TTSP IOCTL (Built %s) rc=%d\n", __func__, CY_DRIVER_DATE, rc);
	return rc;

}
module_init(cyttsp4_ioctl_init);

static void __exit cyttsp4_ioctl_exit(void)
{
	cyttsp4_unregister_driver(&cyttsp4_ioctl_driver);

	pr_info("%s: module exit\n", __func__);
}
module_exit(cyttsp4_ioctl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product I/O Controller Driver");
MODULE_AUTHOR("Cypress Semiconductor");
