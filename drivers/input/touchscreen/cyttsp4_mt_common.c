/*
 * cyttsp4_mt_common.c
 * Cypress TrueTouch(TM) Standard Product V4 Multi-touch module.
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

#include "cyttsp4_mt_common.h"
#include "fj_touch_noracs.h"		/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG add */

/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add start */
#ifdef CONFIG_FTA
	#include <linux/fta.h>
#endif /* CONFIG_FTA */
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add end */

/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add start */
#define RECORDER_LOG_SIZE 128 /* FUJITSU:2013-04-22 recorder_count add */

#define _cyttsp4_mt_tch_info_event_counter(md, touch, add)\
{\
	if( ((int)(touch) == CY_EV_LIFTOFF) && (((struct cyttsp4_mt_data *)(md))->tch_info.lift_off < 0xFFFFFFF) ) {\
		((struct cyttsp4_mt_data *)(md))->tch_info.lift_off += (unsigned int)add;\
	}else if( ((int)(touch) == CY_EV_TOUCHDOWN) && (((struct cyttsp4_mt_data *)(md))->tch_info.touch_down < 0xFFFFFFF) ) {\
		((struct cyttsp4_mt_data *)(md))->tch_info.touch_down += (unsigned int)add;\
	}\
}

#define _cyttsp4_mt_tch_info_startup_counter(md)\
{\
	if (((struct cyttsp4_mt_data *)(md))->tch_info.startup < 0xFFFFFFF) {		\
		((struct cyttsp4_mt_data *)(md))->tch_info.startup++;					\
	}																			\
}

static inline void _cyttsp4_mt_tch_info_cnt_up(unsigned int *count, u8 *last_num, u8 num)
{
	if ((*last_num != num) && (count[ num ] < 0xFFFFFFFF)) {
		(count[ num ])++;
		*last_num = num;
	}
}

static inline void _cyttsp4_mt_tch_info_cnt_reset(unsigned int *count, u8 *last_num, u8 max_cnt)
{
	int cnt;

	for (cnt = 0 ; cnt < max_cnt; cnt++) {
		count[ cnt ] = 0;
	}
	*last_num = 0;
}

static inline void _cyttsp4_mt_tch_info_init(struct cyttsp4_mt_data *md)
{
	md->tch_info.lift_off = 0;
	md->tch_info.touch_down = 0;

	md->tch_info.last_rep_stat = 0;
	md->tch_info.last_hst_mode = 0;
	md->tch_info.last_tt_stat = 0;

	_cyttsp4_mt_tch_info_cnt_reset(md->tch_info.rep_stat_num, &md->tch_info.last_rep_stat, TSP_REP_STAT_COUNT);
	_cyttsp4_mt_tch_info_cnt_reset(md->tch_info.hst_mode_num, &md->tch_info.last_hst_mode, TSP_HST_MODE_COUNT);
	_cyttsp4_mt_tch_info_cnt_reset(md->tch_info.tt_stat_num,  &md->tch_info.last_tt_stat,  TSP_TT_STAT_COUNT);
}

static inline void _cyttsp4_mt_tch_info_xymode(struct cyttsp4_mt_data *md, u8 rep_stat, u8 hst_mode, u8 tt_stat)
{
	_cyttsp4_mt_tch_info_cnt_up(md->tch_info.rep_stat_num, &md->tch_info.last_rep_stat, TSP_REP_STAT_BIT(rep_stat));
	_cyttsp4_mt_tch_info_cnt_up(md->tch_info.hst_mode_num, &md->tch_info.last_hst_mode, TSP_HST_MODE_BIT(hst_mode));
	_cyttsp4_mt_tch_info_cnt_up(md->tch_info.tt_stat_num,  &md->tch_info.last_tt_stat,  TSP_TT_STAT_BIT(tt_stat));
}

static inline void _cyttsp4_mt_tch_info_suspend(struct cyttsp4_mt_data *md, const char *func_name)
{
	struct device *dev = &md->ttsp->dev;
  #ifdef CONFIG_FTA
	char *buf = NULL;
  #endif /* CONFIG_FTA */

	if(md->si != NULL) {
		_cyttsp4_hlog(dev,
			"[TPD_Es] %s: (%d,%d), su(%d), rs(%d,%d,%d,%d,%d,%d,%d,%d), hm(%d,%d,%d), lo(%d), td[%02X %02X], fw/cf(0x%02X%02X%02X%02X, 0x%02X%02X)\n",
			func_name, md->tch_info.lift_off, md->tch_info.touch_down, md->tch_info.startup,
			md->tch_info.rep_stat_num[ 0 ], md->tch_info.rep_stat_num[ 1 ], md->tch_info.rep_stat_num[ 2 ], md->tch_info.rep_stat_num[ 3 ],
			md->tch_info.rep_stat_num[ 4 ], md->tch_info.rep_stat_num[ 5 ], md->tch_info.rep_stat_num[ 6 ], md->tch_info.rep_stat_num[ 7 ],
			md->tch_info.hst_mode_num[ 0 ], md->tch_info.hst_mode_num[ 1 ], md->tch_info.hst_mode_num[ 2 ], md->tch_info.tt_stat_num[ 1 ],
			md->si->si_ptrs.test->post_codeh,  md->si->si_ptrs.test->post_codel,
			md->si->si_ptrs.cydata->revctrl[4], md->si->si_ptrs.cydata->revctrl[5], md->si->si_ptrs.cydata->revctrl[6], md->si->si_ptrs.cydata->revctrl[7],
			md->si->si_ptrs.cydata->cyito_verh, md->si->si_ptrs.cydata->cyito_verl);
		/* revctrl[4:7] : fw */
		/* cyito_verh/l : cf */

	} else {
		_cyttsp4_hlog(dev,
			"[TPD_Es] %s: (%d,%d), su(%d), rs(%d,%d,%d,%d,%d,%d,%d,%d), hm(%d,%d,%d), lo(%d)...\n",
			func_name, md->tch_info.lift_off, md->tch_info.touch_down, md->tch_info.startup,
			md->tch_info.rep_stat_num[ 0 ], md->tch_info.rep_stat_num[ 1 ], md->tch_info.rep_stat_num[ 2 ], md->tch_info.rep_stat_num[ 3 ],
			md->tch_info.rep_stat_num[ 4 ], md->tch_info.rep_stat_num[ 5 ], md->tch_info.rep_stat_num[ 6 ], md->tch_info.rep_stat_num[ 7 ],
			md->tch_info.hst_mode_num[ 0 ], md->tch_info.hst_mode_num[ 1 ], md->tch_info.hst_mode_num[ 2 ], md->tch_info.tt_stat_num[ 1 ])
	}

  #ifdef CONFIG_FTA
	buf = kzalloc(RECORDER_LOG_SIZE, GFP_KERNEL);
	if (buf != NULL) {
		if(md->si != NULL) {
			snprintf(buf, RECORDER_LOG_SIZE,
				"Touch: count[up=%d,down=%d] recover=%d, test_data[%02X %02X]\n",
				md->tch_info.lift_off, md->tch_info.touch_down, md->tch_info.startup,
				md->si->si_ptrs.test->post_codeh, md->si->si_ptrs.test->post_codel);
			ftadrv_send_str(buf);
		}
		kfree(buf);
	}
  #endif /* CONFIG_FTA */

}

static inline void _cyttsp4_mt_tch_info_resume(struct cyttsp4_mt_data *md, const char *func_name)
{
	struct device *dev = &md->ttsp->dev;

	_cyttsp4_hlog(dev, "[TPD_Lr] %s \n", func_name);

	_cyttsp4_mt_tch_info_init( md );
}
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add end */

static void cyttsp4_lift_all(struct cyttsp4_mt_data *md)
{
	if (!md->si)
		return;

	if (md->num_prv_rec != 0) {
		_cyttsp4_mt_tch_info_event_counter(md, CY_EV_LIFTOFF, md->num_prv_rec);		/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add */
		if (md->mt_function.report_slot_liftoff)
			md->mt_function.report_slot_liftoff(md,
				md->si->si_ofs.tch_abs[CY_TCH_T].max);
		input_sync(md->input);
		md->num_prv_rec = 0;
	}
}

static void cyttsp4_mt_process_touch(struct cyttsp4_mt_data *md,
	struct cyttsp4_touch *touch)
{
	struct device *dev = &md->ttsp->dev;
	int tmp;
	bool flipped;

	if (md->pdata->flags & CY_MT_FLAG_FLIP) {
		tmp = touch->abs[CY_TCH_X];
		touch->abs[CY_TCH_X] = touch->abs[CY_TCH_Y];
		touch->abs[CY_TCH_Y] = tmp;
		flipped = true;
	} else
		flipped = false;

	if (md->pdata->flags & CY_MT_FLAG_INV_X) {
		if (flipped)
			touch->abs[CY_TCH_X] = md->si->si_ofs.max_y -
				touch->abs[CY_TCH_X];
		else
			touch->abs[CY_TCH_X] = md->si->si_ofs.max_x -
				touch->abs[CY_TCH_X];
	}
	if (md->pdata->flags & CY_MT_FLAG_INV_Y) {
		if (flipped)
			touch->abs[CY_TCH_Y] = md->si->si_ofs.max_x -
				touch->abs[CY_TCH_Y];
		else
			touch->abs[CY_TCH_Y] = md->si->si_ofs.max_y -
				touch->abs[CY_TCH_Y];
	}

	dev_vdbg(dev, "%s: flip=%s inv-x=%s inv-y=%s x=%04X(%d) y=%04X(%d)\n",
		__func__, flipped ? "true" : "false",
		md->pdata->flags & CY_MT_FLAG_INV_X ? "true" : "false",
		md->pdata->flags & CY_MT_FLAG_INV_Y ? "true" : "false",
		touch->abs[CY_TCH_X], touch->abs[CY_TCH_X],
		touch->abs[CY_TCH_Y], touch->abs[CY_TCH_Y]);
}

static void cyttsp4_get_mt_touches(struct cyttsp4_mt_data *md, int num_cur_rec)
{
	struct device *dev = &md->ttsp->dev;
	struct cyttsp4_sysinfo *si = md->si;
	struct cyttsp4_touch tch;
	int sig;
	int i, j, t = 0;
	int mt_sync_count = 0;
/* FUJITSU:2013-04-05 HMW_TTDA_PRESS add start */
	u8 *xy_mode;
	u8 press_h = 0;
	u8 press_l = 0;
/* FUJITSU:2013-04-05 HMW_TTDA_PRESS add end */
	DECLARE_BITMAP(ids, max(CY_TMA1036_MAX_TCH, CY_TMA4XX_MAX_TCH));

	bitmap_zero(ids, si->si_ofs.tch_abs[CY_TCH_T].max);

	for (i = 0; i < num_cur_rec; i++) {
		cyttsp4_get_touch_record(md->ttsp, i, tch.abs);

		/* Discard proximity event */
		if (tch.abs[CY_TCH_O] == CY_OBJ_PROXIMITY) {
			dev_dbg(dev, "%s: Discarding proximity event\n",
				__func__);
			continue;
		}

		if ((tch.abs[CY_TCH_T] < md->pdata->frmwrk->abs
			[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MIN_OST]) ||
			(tch.abs[CY_TCH_T] > md->pdata->frmwrk->abs
			[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MAX_OST])) {
			dev_err(dev, "%s: tch=%d -> bad trk_id=%d max_id=%d\n",
				__func__, i, tch.abs[CY_TCH_T],
				md->pdata->frmwrk->abs[(CY_ABS_ID_OST *
				CY_NUM_ABS_SET) + CY_MAX_OST]);
			if (md->mt_function.input_sync)
				md->mt_function.input_sync(md->input);
			mt_sync_count++;
			continue;
		}

		/* Process touch */
		cyttsp4_mt_process_touch(md, &tch);

		/* use 0 based track id's */
		sig = md->pdata->frmwrk->abs
			[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + 0];
		if (sig != CY_IGNORE_VALUE) {
			t = tch.abs[CY_TCH_T] - md->pdata->frmwrk->abs
				[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MIN_OST];
/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG add start */
			if (md->nv_info->saku2_trace_printk == CY_TOUCH_DBG_ON) {
				trace_printk("C|0|TOUCH_EVENT|%d\n", tch.abs[CY_TCH_E]);
			}
/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG add end */
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add start */
			_cyttsp4_mt_tch_info_event_counter(md, tch.abs[CY_TCH_E] , 1);
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add end */
			if (tch.abs[CY_TCH_E] == CY_EV_LIFTOFF) {
				dev_dbg(dev, "%s: t=%d e=%d lift-off\n",
					__func__, t, tch.abs[CY_TCH_E]);
				goto cyttsp4_get_mt_touches_pr_tch;
			}
			if (md->mt_function.input_report)
				md->mt_function.input_report(md->input, sig,
					t, tch.abs[CY_TCH_O]);
			__set_bit(t, ids);
		}

/* FUJITSU:2013-04-05 HMW_TTDA_PRESS add start */
		_cyttsp4_ilog(dev, "press = %d\n",tch.abs[CY_TCH_P])
		if (md->pdata->flags & CY_MT_FLAG_PRESS) {
			_cyttsp4_ilog_str(dev, "FLAG_PRESS is Enable\n");
			xy_mode = si->xy_mode + si->si_ofs.rep_ofs;
			press_h = xy_mode[3];
			press_l = xy_mode[4];
			tch.abs[CY_TCH_P] = (signed short)((press_h << 8) + press_l);
			_cyttsp4_ilog(dev, "touchpress = %d\n",tch.abs[CY_TCH_P]);
		}
/* FUJITSU:2013-04-05 HMW_TTDA_PRESS add end */

		/* all devices: position and pressure fields */
		for (j = 0; j <= CY_ABS_W_OST ; j++) {
			sig = md->pdata->frmwrk->abs[((CY_ABS_X_OST + j) *
				CY_NUM_ABS_SET) + 0];
/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG mod start */
			if (sig != CY_IGNORE_VALUE)
			{
				input_report_abs(md->input, sig,
					tch.abs[CY_TCH_X + j]);
				if (md->nv_info->saku2_trace_printk == CY_TOUCH_DBG_ON) {
					if (CY_TCH_X == j) {
						trace_printk("C|0|TOUCH_ABS_MT_POSITION_X|%d\n", tch.abs[CY_TCH_X]);
					} else if (CY_TCH_Y == j) {
						trace_printk("C|0|TOUCH_ABS_MT_POSITION_Y|%d\n", tch.abs[CY_TCH_Y]);
					}
				}
			}
/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG mod end */
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

			/* Get the extended touch fields */
			for (j = 0; j < CY_NUM_EXT_TCH_FIELDS; j++) {
				sig = md->pdata->frmwrk->abs
					[((CY_ABS_MAJ_OST + j) *
					CY_NUM_ABS_SET) + 0];
				if (sig != CY_IGNORE_VALUE)
					input_report_abs(md->input, sig,
						tch.abs[CY_TCH_MAJ + j]);
			}
		}
		if (md->mt_function.input_sync)
			md->mt_function.input_sync(md->input);
		mt_sync_count++;

cyttsp4_get_mt_touches_pr_tch:
/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG mod start */
		if (md->nv_info->saku2_log == CY_TOUCH_DBG_ON) {
			pr_info("[TPD_Sk2],%s%s%s%s,%04d,%04d,%04d\n",
				tch.abs[CY_TCH_E] == CY_EV_NO_EVENT ? "NoEvent" : "",
				tch.abs[CY_TCH_E] == CY_EV_TOUCHDOWN ? "Down" : "",
				tch.abs[CY_TCH_E] == CY_EV_MOVE ? "Move" : "",
				tch.abs[CY_TCH_E] == CY_EV_LIFTOFF ? "Up" : "",
				tch.abs[CY_TCH_X],
				tch.abs[CY_TCH_Y],
				tch.abs[CY_TCH_P]);
		} else if (IS_TTSP_VER_GE(si, 2, 3)){
			_cyttsp4_dlog(dev,
				"%s: t=%d x=%d y=%d z=%d M=%d m=%d o=%d e=%d\n",
				__func__, t,
				tch.abs[CY_TCH_X],
				tch.abs[CY_TCH_Y],
				tch.abs[CY_TCH_P],
				tch.abs[CY_TCH_MAJ],
				tch.abs[CY_TCH_MIN],
				tch.abs[CY_TCH_OR],
				tch.abs[CY_TCH_E]);
		} else {
			_cyttsp4_dlog(dev,
				"%s: t=%d x=%d y=%d z=%d e=%d\n", __func__,
				t,
				tch.abs[CY_TCH_X],
				tch.abs[CY_TCH_Y],
				tch.abs[CY_TCH_P],
				tch.abs[CY_TCH_E]);
		}
	}

	if (md->mt_function.final_sync)
		md->mt_function.final_sync(md->input,
			si->si_ofs.tch_abs[CY_TCH_T].max, mt_sync_count, ids);

	md->num_prv_rec = num_cur_rec;
	md->prv_tch_type = tch.abs[CY_TCH_O];

	return;
}

/* read xy_data for all current touches */
static int cyttsp4_xy_worker(struct cyttsp4_mt_data *md)
{
	struct device *dev = &md->ttsp->dev;
	struct cyttsp4_sysinfo *si = md->si;
	u8 num_cur_rec;
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;
	int rc = 0;

	/*
	 * Get event data from cyttsp4 device.
	 * The event data includes all data
	 * for all active touches.
	 * Event data also includes button data
	 */
	rep_len = si->xy_mode[si->si_ofs.rep_ofs];
	rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
	tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];

/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add start */
	_cyttsp4_mt_tch_info_xymode( md, rep_stat, si->xy_mode[CY_REG_BASE], tt_stat );
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add end */

	num_cur_rec = GET_NUM_TOUCH_RECORDS(tt_stat);

	if (rep_len == 0 && num_cur_rec > 0) {
		dev_err(dev, "%s: report length error rep_len=%d num_tch=%d\n",
			__func__, rep_len, num_cur_rec);
		goto cyttsp4_xy_worker_exit;
	}

	/* check any error conditions */
	if (IS_BAD_PKT(rep_stat)) {
		dev_dbg(dev, "%s: Invalid buffer detected\n", __func__);
		rc = 0;
		goto cyttsp4_xy_worker_exit;
	}

/* FUJITSU:2013-05-24 HMW_TTDA_LARGE_SUPPRESS mod start */
	if (md->detect_large == true) {
		if (num_cur_rec == 0) {
			dev_dbg(dev, "%s: Large area clear\n", __func__);
			md->detect_large = false;
		} else {
			num_cur_rec = 0;
		}
	} else if (IS_LARGE_AREA(tt_stat)) {
		if (md->pdata->flags & CY_MT_FLAG_NO_TOUCH_ON_LO){
			/* terminate all active tracks */
			if (md->detect_large != true) {
				_cyttsp4_hlog(dev, "%s: Large area detecte\n", __func__);
			}
			md->detect_large = true;
			num_cur_rec = 0;
		}
	}
/* FUJITSU:2013-05-24 HMW_TTDA_LARGE_SUPPRESS mod end */

	if (num_cur_rec > si->si_ofs.max_tchs) {
		dev_err(dev, "%s: %s (n=%d c=%d)\n", __func__,
			"too many tch; set to max tch",
			num_cur_rec, si->si_ofs.max_tchs);
		num_cur_rec = si->si_ofs.max_tchs;
	}

	/* extract xy_data for all currently reported touches */
	dev_vdbg(dev, "%s: extract data num_cur_rec=%d\n", __func__,
		num_cur_rec);
	if (num_cur_rec)
		cyttsp4_get_mt_touches(md, num_cur_rec);
	else
		cyttsp4_lift_all(md);

	dev_vdbg(dev, "%s: done\n", __func__);
	rc = 0;

cyttsp4_xy_worker_exit:
	return rc;
}

static void cyttsp4_mt_send_dummy_event(struct cyttsp4_mt_data *md)
{
	unsigned long ids = 0;

	/* for easy wakeup */
	if (md->mt_function.input_report)
		md->mt_function.input_report(md->input, ABS_MT_TRACKING_ID,
			0, CY_OBJ_STANDARD_FINGER);
	if (md->mt_function.input_sync)
		md->mt_function.input_sync(md->input);
	if (md->mt_function.final_sync)
		md->mt_function.final_sync(md->input, 0, 1, &ids);
	if (md->mt_function.report_slot_liftoff)
		md->mt_function.report_slot_liftoff(md, 1);
	if (md->mt_function.final_sync)
		md->mt_function.final_sync(md->input, 1, 1, &ids);
}

static int cyttsp4_mt_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);
	int rc = 0;
	struct timeval tv;			/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG add */

	dev_vdbg(dev, "%s\n", __func__);
/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG add start */
	if (md->nv_info->saku2_trace_printk == CY_TOUCH_DBG_ON) {
		 trace_printk("B|0|cyttsp4_irq\n");
	}
	if (md->nv_info->saku2_log == CY_TOUCH_DBG_ON) {
		do_gettimeofday(&tv);
		pr_info("[TPD_Sk2],%lu%06lu,%s,IN", tv.tv_sec, tv.tv_usec,__func__ );
	}
/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG add end */

	mutex_lock(&md->report_lock);
	if (!md->is_suspended) {
		/* core handles handshake */
		rc = cyttsp4_xy_worker(md);
	} else {
		dev_vdbg(dev, "%s: Ignoring report while suspended\n",
			__func__);
	}
	mutex_unlock(&md->report_lock);
	if (rc < 0)
		dev_err(dev, "%s: xy_worker error r=%d\n", __func__, rc);

/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG add start */
	if (md->nv_info->saku2_log == CY_TOUCH_DBG_ON) {
		do_gettimeofday(&tv);
		pr_info("[TPD_Sk2],%lu%06lu,%s,OUT", tv.tv_sec, tv.tv_usec,__func__ );
	}
	if (md->nv_info->saku2_trace_printk == CY_TOUCH_DBG_ON) {
		 trace_printk("E\n");
	}
/* FUJITSU:2013-05-01 HMW_TTDA_SAKU2_LOG add end */
	return rc;
}

static int cyttsp4_mt_wake_attention(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_mt_data *md = dev_get_drvdata(&ttsp->dev);

	mutex_lock(&md->report_lock);
	cyttsp4_mt_send_dummy_event(md);
	mutex_unlock(&md->report_lock);
	return 0;
}

static int cyttsp4_startup_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);
	int rc = 0;

	dev_vdbg(dev, "%s\n", __func__);

	mutex_lock(&md->report_lock);
	cyttsp4_lift_all(md);
	mutex_unlock(&md->report_lock);
	_cyttsp4_mt_tch_info_startup_counter(md);		/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add */
	return rc;
}

static int cyttsp4_mt_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp4_device *ttsp =
		container_of(dev, struct cyttsp4_device, dev);

	dev_dbg(dev, "%s\n", __func__);

	pm_runtime_get(dev);

	dev_vdbg(dev, "%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_IRQ,
		cyttsp4_mt_attention, CY_MODE_OPERATIONAL);

	/* set up startup call back */
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
		cyttsp4_startup_attention, 0);

	/* set up wakeup call back */
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_WAKE,
		cyttsp4_mt_wake_attention, 0);

	return 0;
}

static void cyttsp4_mt_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp4_device *ttsp =
		container_of(dev, struct cyttsp4_device, dev);

	dev_dbg(dev, "%s\n", __func__);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
		cyttsp4_mt_attention, CY_MODE_OPERATIONAL);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
		cyttsp4_startup_attention, 0);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_WAKE,
		cyttsp4_mt_wake_attention, 0);

	pm_runtime_put(dev);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp4_mt_early_suspend(struct early_suspend *h)
{
	struct cyttsp4_mt_data *md =
		container_of(h, struct cyttsp4_mt_data, es);
	struct device *dev = &md->ttsp->dev;

	dev_dbg(dev, "%s\n", __func__);

#ifndef CONFIG_PM_RUNTIME
	mutex_lock(&md->report_lock);
	md->is_suspended = true;
	cyttsp4_lift_all(md);
/* FUJITSU:2013-05-24 HMW_TTDA_LARGE_SUPPRESS add start */
	md->detect_large = false;
/* FUJITSU:2013-05-24 HMW_TTDA_LARGE_SUPPRESS add end */
	mutex_unlock(&md->report_lock);
#endif

/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add start */
	_cyttsp4_mt_tch_info_suspend( md, __func__ );
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add end */

	pm_runtime_put(dev);
}

static void cyttsp4_mt_late_resume(struct early_suspend *h)
{
	struct cyttsp4_mt_data *md =
		container_of(h, struct cyttsp4_mt_data, es);
	struct device *dev = &md->ttsp->dev;

	dev_dbg(dev, "%s\n", __func__);

#ifndef CONFIG_PM_RUNTIME
	mutex_lock(&md->report_lock);
	md->is_suspended = false;
	mutex_unlock(&md->report_lock);
#endif

/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add start */
	_cyttsp4_mt_tch_info_resume( md, __func__ );
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add end */

	pm_runtime_get(dev);
}

static void cyttsp4_setup_early_suspend(struct cyttsp4_mt_data *md)
{
	md->es.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	md->es.suspend = cyttsp4_mt_early_suspend;
	md->es.resume = cyttsp4_mt_late_resume;

	register_early_suspend(&md->es);
}
#endif

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM_RUNTIME)
static int cyttsp4_mt_suspend(struct device *dev)
{
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	mutex_lock(&md->report_lock);
	md->is_suspended = true;
	cyttsp4_lift_all(md);
/* FUJITSU:2013-05-24 HMW_TTDA_LARGE_SUPPRESS add start */
	md->detect_large = false;
/* FUJITSU:2013-05-24 HMW_TTDA_LARGE_SUPPRESS add end */
	mutex_unlock(&md->report_lock);

	return 0;
}

static int cyttsp4_mt_rt_resume(struct device *dev)
{
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	mutex_lock(&md->report_lock);
	md->is_suspended = false;
	mutex_unlock(&md->report_lock);

	return 0;
}

static int cyttsp4_mt_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops cyttsp4_mt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cyttsp4_mt_suspend, cyttsp4_mt_resume)
	SET_RUNTIME_PM_OPS(cyttsp4_mt_suspend, cyttsp4_mt_rt_resume, NULL)
};

static int cyttsp4_setup_input_device(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);
	int signal = CY_IGNORE_VALUE;
	int max_x, max_y, max_p, min, max;
	int max_x_tmp, max_y_tmp;
	int i;
	int rc;

	dev_vdbg(dev, "%s: Initialize event signals\n", __func__);
	__set_bit(EV_ABS, md->input->evbit);
	__set_bit(EV_REL, md->input->evbit);
	__set_bit(EV_KEY, md->input->evbit);
#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, md->input->propbit);
#endif

	/* If virtualkeys enabled, don't use all screen */
	if (md->pdata->flags & CY_MT_FLAG_VKEYS) {
		max_x_tmp = CY_VKEYS_X;
		max_y_tmp = CY_VKEYS_Y;
	} else {
		max_x_tmp = md->si->si_ofs.max_x;
		max_y_tmp = md->si->si_ofs.max_y;
	}

	/* get maximum values from the sysinfo data */
	if (md->pdata->flags & CY_MT_FLAG_FLIP) {
		max_x = max_y_tmp - 1;
		max_y = max_x_tmp - 1;
	} else {
		max_x = max_x_tmp - 1;
		max_y = max_y_tmp - 1;
	}
	max_p = md->si->si_ofs.max_p;

	/* set event signal capabilities */
	for (i = 0; i < (md->pdata->frmwrk->size / CY_NUM_ABS_SET); i++) {
		signal = md->pdata->frmwrk->abs
			[(i * CY_NUM_ABS_SET) + CY_SIGNAL_OST];
		if (signal != CY_IGNORE_VALUE) {
			__set_bit(signal, md->input->absbit);
			min = md->pdata->frmwrk->abs
				[(i * CY_NUM_ABS_SET) + CY_MIN_OST];
			max = md->pdata->frmwrk->abs
				[(i * CY_NUM_ABS_SET) + CY_MAX_OST];
			if (i == CY_ABS_ID_OST) {
				/* shift track ids down to start at 0 */
				max = max - min;
				min = min - min;
			} else if (i == CY_ABS_X_OST)
				max = max_x;
			else if (i == CY_ABS_Y_OST)
				max = max_y;
			else if (i == CY_ABS_P_OST)
				max = max_p;
			input_set_abs_params(md->input, signal, min, max,
				md->pdata->frmwrk->abs
				[(i * CY_NUM_ABS_SET) + CY_FUZZ_OST],
				md->pdata->frmwrk->abs
				[(i * CY_NUM_ABS_SET) + CY_FLAT_OST]);
			dev_dbg(dev, "%s: register signal=%02X min=%d max=%d\n",
				__func__, signal, min, max);
			if (i == CY_ABS_ID_OST && !IS_TTSP_VER_GE(md->si, 2, 3))
				break;
		}
	}

	rc = md->mt_function.input_register_device(md->input,
			md->si->si_ofs.tch_abs[CY_TCH_T].max);
	if (rc < 0)
		dev_err(dev, "%s: Error, failed register input device r=%d\n",
			__func__, rc);
	else
		md->input_device_registered = true;

	return rc;
}

static int cyttsp4_setup_input_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);
	int rc = 0;

	dev_vdbg(dev, "%s\n", __func__);

	md->si = cyttsp4_request_sysinfo(ttsp);
	if (!md->si)
		return -EINVAL;

	rc = cyttsp4_setup_input_device(ttsp);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
		cyttsp4_setup_input_attention, 0);

	return rc;
}

static int cyttsp4_mt_release(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

#ifdef CONFIG_HAS_EARLYSUSPEND
	/*
	 * This check is to prevent pm_runtime usage_count drop below zero
	 * because of removing the module while in suspended state
	 */
	if (md->is_suspended)
		pm_runtime_get_noresume(dev);

	unregister_early_suspend(&md->es);
#endif

	if (md->input_device_registered) {
		input_unregister_device(md->input);
	} else {
		input_free_device(md->input);
		cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
			cyttsp4_setup_input_attention, 0);
	}

	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);

	dev_set_drvdata(dev, NULL);
	kfree(md);
	return 0;
}

static int cyttsp4_mt_probe(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md;
	struct cyttsp4_mt_platform_data *pdata = dev_get_platdata(dev);
	int rc = 0;

	dev_info(dev, "%s\n", __func__);
	dev_dbg(dev, "%s: debug on\n", __func__);
	dev_vdbg(dev, "%s: verbose debug on\n", __func__);

	if (pdata == NULL) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (md == NULL) {
		dev_err(dev, "%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

	cyttsp4_init_function_ptrs(md);

	mutex_init(&md->report_lock);
	md->prv_tch_type = CY_OBJ_STANDARD_FINGER;
	md->ttsp = ttsp;
	md->pdata = pdata;
	dev_set_drvdata(dev, md);
	/* Create the input device and register it. */
	dev_vdbg(dev, "%s: Create the input device and register it\n",
		__func__);
	md->input = input_allocate_device();
	if (md->input == NULL) {
		dev_err(dev, "%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	md->input->name = ttsp->name;
	scnprintf(md->phys, sizeof(md->phys)-1, "%s", dev_name(dev));
	md->input->phys = md->phys;
	md->input->dev.parent = &md->ttsp->dev;
	md->input->open = cyttsp4_mt_open;
	md->input->close = cyttsp4_mt_close;
	input_set_drvdata(md->input, md);

	pm_runtime_enable(dev);

	/* get sysinfo */
	md->si = cyttsp4_request_sysinfo(ttsp);
	/* FUJITSU:2013-05-01 HMW_TTDA_MODIFY_LOG add start */
	md->nv_info		= cyttsp4_request_ref_nonvolatile_info(ttsp);
	/* FUJITSU:2013-05-01 HMW_TTDA_MODIFY_LOG add end */
/* FUJITSU:2013-05-24 HMW_TTDA_LARGE_SUPPRESS add start */
	md->detect_large = false;
/* FUJITSU:2013-05-24 HMW_TTDA_LARGE_SUPPRESS add end */
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add start */
	_cyttsp4_mt_tch_info_init( md );
/* FUJITSU:2013-05-28 HMW_TOUCH_INFORMATION add end */

	if (md->si) {
		rc = cyttsp4_setup_input_device(ttsp);
		if (rc)
			goto error_init_input;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, md->si);
		cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
			cyttsp4_setup_input_attention, 0);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	cyttsp4_setup_early_suspend(md);
#endif

	dev_dbg(dev, "%s: OK\n", __func__);
	return 0;

error_init_input:
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
	input_free_device(md->input);
error_alloc_failed:
	dev_set_drvdata(dev, NULL);
	kfree(md);
error_alloc_data_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);
	return rc;
}

struct cyttsp4_driver cyttsp4_mt_driver = {
	.probe = cyttsp4_mt_probe,
	.remove = cyttsp4_mt_release,
	.driver = {
		.name = CYTTSP4_MT_NAME,
		.bus = &cyttsp4_bus_type,
		.pm = &cyttsp4_mt_pm_ops,
	},
};

