/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add start */
/*
 * COPYRIGHT(C) 2011-2013 FUJITSU LIMITED
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
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/
#ifndef __FJTOUCH_NORACS_H__
#define __FJTOUCH_NORACS_H__
#define CY_FJTOUCH_BIT0                     0x01
#define CY_FJTOUCH_BIT1                     0x02
#define CY_FJTOUCH_BIT2                     0x04
#define CY_FJTOUCH_BIT3                     0x08
#define CY_FJTOUCH_BIT4                     0x10
#define CY_FJTOUCH_BIT5                     0x20
#define CY_FJTOUCH_BIT6                     0x40
#define CY_FJTOUCH_BIT7                     0x80

#define		CY_TOUCH_DBG_ON					(u8)1
#define		CY_TOUCH_DBG_OFF				(u8)0

/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add start */
#define		CY_TOUCH_PRS_BTN_DEF_ALT_TX_PERIOD_OFST		0	/* adr 0xB837, ofset 0 */
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add end */

/* default values for Nor Access err */


/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add start */
#define	CY_FJ_BUF_DUMP_LEN		16			/* number of data per line (16 or 32) */
#define	CY_FJ_BUF_DUMP_LINE		256			/* line buffer size */

static inline void _cyttsp4_buf_dump(u8 *dptr, int size, const char *data_name, int loglevel)
{
	int ii, nLen = 0;
	u8	tmp_buf[CY_FJ_BUF_DUMP_LINE];

	if (cy_loglevel < loglevel) {
		return;
	}

	pr_info("[TPD] dump [%s] size = %d", data_name, size);
	memset (&tmp_buf[0], 0, sizeof(u8) * CY_FJ_BUF_DUMP_LINE);

	for (ii = 0; ii < size; ii++, dptr++) {
		if ((ii % CY_FJ_BUF_DUMP_LEN) == 0) {
			nLen = snprintf( &tmp_buf[ 0 ], CY_FJ_BUF_DUMP_LINE, "[TPD] %08X | ", ii);
		}
		nLen += snprintf( &tmp_buf[ nLen ], (CY_FJ_BUF_DUMP_LINE - nLen), "%02X ", *dptr);
		if ((ii % CY_FJ_BUF_DUMP_LEN) == (CY_FJ_BUF_DUMP_LEN - 1)) {
			pr_info("%s", &tmp_buf[ 0 ] );
			nLen = 0;
			memset (&tmp_buf[0], 0, sizeof(unsigned char) * CY_FJ_BUF_DUMP_LINE);
		}
	}

	if ((ii % CY_FJ_BUF_DUMP_LEN) != 0) {
		pr_info("%s", &tmp_buf[ 0 ] );
	}
}
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add start */

/* ========================================
 * use only cyttsp4_core.c
 * ========================================
 */

#ifdef FJ_TOUCH_CORE
#include <linux/nonvolatile_common.h>
static inline int _cyttsp4_dump_all_nor_info(struct cyttsp4_nonvolatile_info *nv_info)
{
	int result = 0;

	pr_info("[TPD]%s :[IN]\n", __func__ );
	/* 0xA064 */
	pr_info("[TPD] touch_disable                        = %d\n", nv_info->touch_disable);
	pr_info("[TPD] power_on_calibration                 = %d\n", nv_info->power_on_calibration);

	pr_info("[TPD] cy_loglevel                          = %d\n", cy_loglevel);
	pr_info("[TPD] saku2_trace_printk                   = %d\n", nv_info->saku2_trace_printk);
	pr_info("[TPD] saku2_log                            = %d\n", nv_info->saku2_log);
	pr_info("[TPD] saku2_led                            = %d\n", nv_info->saku2_led);

	pr_info("[TPD] recovery_led                         = %d\n", nv_info->recovery_led);

	pr_info("[TPD] set_cyttsp_debug_tools               = %d\n", nv_info->cyttsp_debug_tools);
	pr_info("[TPD] not_write_config_parm                = %d\n", nv_info->not_write_config_parm);

	pr_info("[TPD] not_use_charger_armor                = %d\n", nv_info->not_use_charger_armor);
	pr_info("[TPD] data_io_debug                        = %d\n", nv_info->data_io_debug);						/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add */
	pr_info("[TPD] i2c_log                              = %d\n", nv_info->i2c_log);								/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_LOG add */
	pr_info("[TPD] press_normalize                      = %d\n", nv_info->press_normalize);						/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add */
	pr_info("[TPD] press_resume_force_calib             = %d\n", nv_info->press_resume_force_calib);			/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add */

	/* 0xA065 */

	/* 0xA066 */

	pr_info("[TPD]%s :[OUT]\n", __func__ );
	
	return result;
}

static inline int _cyttsp4_set_all_nor_info(struct cyttsp4_nonvolatile_info *nv_info)
{
	/* 0xA064 */
	nv_info->touch_disable								= (nv_info->debug_i[0] & 0x01) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;
	nv_info->power_on_calibration						= (nv_info->debug_i[1] & 0x01) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;

	cy_loglevel											=  nv_info->debug_i[2] & 0x03;
	nv_info->saku2_trace_printk							= (nv_info->debug_i[2] & 0x04) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;
	nv_info->saku2_log									= (nv_info->debug_i[2] & 0x08) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;
	nv_info->saku2_led									= (nv_info->debug_i[2] & 0x80) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;

	nv_info->recovery_led								= (nv_info->debug_i[2] & 0x40) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;

	nv_info->cyttsp_debug_tools							= (nv_info->debug_i[3] & 0x01) ? CY_TOUCH_DBG_OFF : CY_TOUCH_DBG_ON; /* 1 : disable */
	nv_info->not_write_config_parm						= (nv_info->debug_i[4] & 0x01) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;

	nv_info->not_use_charger_armor						= (nv_info->debug_i[6] & 0x01) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;
	nv_info->data_io_debug								= (nv_info->debug_i[6] & 0x02) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;	/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add */
	nv_info->i2c_log									= (nv_info->debug_i[6] & 0x04) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;	/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_LOG add */
	nv_info->press_normalize							= (nv_info->debug_i[7] & 0x01) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;	/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add */
	nv_info->press_resume_force_calib					= (nv_info->debug_i[7] & 0x02) ? CY_TOUCH_DBG_ON : CY_TOUCH_DBG_OFF;	/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add */

	/* 0xA065 */

	/* 0xA066 */

	return 0;
}

static inline int _cyttsp4_get_all_nor_info(struct cyttsp4_nonvolatile_info *nv_info)
{
	int retval = 0;
	int result = 0;

	pr_info("[TPD]%s :[IN]\n", __func__ );

	memset(&nv_info->debug_i[0],			0,	CY_TOUCH_DEBUG_FLAGS);
	memset(&nv_info->set_config_param[0],	0,	CY_TOUCH_SET_PARM_FLAGS);
	memset(&nv_info->config_param_value[0],	0,	CY_TOUCH_CONFIG_PARM_VALUE);

	retval = get_nonvolatile(&nv_info->debug_i[0], 0xA064, CY_TOUCH_DEBUG_FLAGS);
	if (retval < 0){
		pr_info("[TPD]%s:set default value (0xA064)\n", __func__);
		nv_info->touch_disable					= CY_TOUCH_DBG_OFF;
		nv_info->power_on_calibration			= CY_TOUCH_DBG_OFF;

		cy_loglevel								= 0;
		nv_info->saku2_trace_printk				= CY_TOUCH_DBG_OFF;
		nv_info->saku2_log						= CY_TOUCH_DBG_OFF;
		nv_info->saku2_led						= CY_TOUCH_DBG_OFF;

		nv_info->recovery_led					= CY_TOUCH_DBG_OFF;

		nv_info->cyttsp_debug_tools				= CY_TOUCH_DBG_OFF;
		nv_info->not_write_config_parm			= CY_TOUCH_DBG_OFF;

		nv_info->not_use_charger_armor			= CY_TOUCH_DBG_OFF;
		nv_info->data_io_debug					= CY_TOUCH_DBG_OFF;	/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add */
		nv_info->i2c_log						= CY_TOUCH_DBG_OFF;	/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_LOG add */
		nv_info->press_normalize				= CY_TOUCH_DBG_OFF;	/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add */
		nv_info->press_resume_force_calib		= CY_TOUCH_DBG_OFF;	/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add */

		result = retval;
	}
	retval = get_nonvolatile(&nv_info->set_config_param[0], 0xA065, CY_TOUCH_SET_PARM_FLAGS);
	if (retval < 0){
		pr_info("[TPD]%s:set default value (0xA065)\n", __func__);
		result = retval;
    }
	retval = get_nonvolatile(&nv_info->config_param_value[0], 0xA066, CY_TOUCH_CONFIG_PARM_VALUE);
	if (retval < 0){
		pr_info("[TPD]%s:set default value (0xA066)\n", __func__);
		result = retval;
	}

/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add start */
	memset(&nv_info->press_noise_param[0],	0,	CY_TOUCH_PRESS_NOISE_PARM);
	retval = get_nonvolatile(&nv_info->press_noise_param[0], 0xB837, CY_TOUCH_PRESS_NOISE_PARM);
	if (retval < 0){
		pr_info("[TPD]%s:set default value (0xB837)\n", __func__);
		result = retval;
	}
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add end */

	pr_info("[TPD]%s :[OUT]\n", __func__ );

	if (result == 0) {
		_cyttsp4_set_all_nor_info( nv_info );
	}

	_cyttsp4_dump_all_nor_info(nv_info);

/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add start */
	if (nv_info->data_io_debug == CY_TOUCH_DBG_ON) {
		_cyttsp4_buf_dump(&nv_info->debug_i[0],				CY_TOUCH_DEBUG_FLAGS,		"0xA064", CY_LOGLV0);
		_cyttsp4_buf_dump(&nv_info->set_config_param[0],	CY_TOUCH_SET_PARM_FLAGS,	"0xA065", CY_LOGLV0);
		_cyttsp4_buf_dump(&nv_info->config_param_value[0],	CY_TOUCH_CONFIG_PARM_VALUE,	"0xA066", CY_LOGLV0);
		_cyttsp4_buf_dump(&nv_info->press_noise_param[0],	CY_TOUCH_PRESS_NOISE_PARM,	"0xB837", CY_LOGLV0);		/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add */
	}
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add end */

	return result;
}
#endif /* FJ_TOUCH_CORE */

#endif // __FJIDTOUCH_NORACS_H__
/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add end */
