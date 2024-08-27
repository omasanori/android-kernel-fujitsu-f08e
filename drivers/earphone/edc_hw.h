/*
  * Copyright(C) 2013 FUJITSU LIMITED
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU General Public License
  * as published by the Free Software Foundation; version 2
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

#ifndef _EDC_HW_H_
#define _EDC_HW_H_

/**********************************************************************/
/* macros															  */
/**********************************************************************/
struct pm8xxx_gpio_init {
	unsigned			gpio;
	struct pm_gpio		config;
};

struct pm8xxx_mpp_init {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8921_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8921_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}


#define PM8921_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable) \
{ \
	.gpio	= PM8921_GPIO_PM_TO_SYS(_gpio), \
	.config	= { \
		.direction	= _dir, \
		.output_buffer	= _buf, \
		.output_value	= _val, \
		.pull		= _pull, \
		.vin_sel	= _vin, \
		.out_strength	= _out_strength, \
		.function	= _func, \
		.inv_int_pol	= _inv, \
		.disable_pin	= _disable, \
	} \
}

#define PM8921_GPIO_DISABLE(_gpio) \
	PM8921_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM_GPIO_VIN_S4, \
			 0, 0, 0, 1)

/* for PM8921 control */
#define PM8921_MPP_BASE			(PM8921_GPIO_BASE + PM8921_NR_GPIOS)
#define PM8921_MPP_PM_TO_SYS(pm_mpp)	(pm_mpp - 1 + PM8921_MPP_BASE)
/* for earphone detect */

#define PHI35_INSERT_POLL_NUM	1 	/* 10times -> 5times -> 1times */
#define PHI35_INSERT_INTERVAL	5	/* 25msec -> 50msec -> 5msec */

#define	FIRST_SWITCH_PULL		30/*50*/
#define	HSCALL_PUSH_POLL_NUM	5
#define	HSCALL_PUSH_INTERVAL	20

/* add 2013/3/18 */
#define EDC_PIERCE_POSS_MAX		2

/* PM8921 GPIO configration */
#define GPIO_XHEADSET_DET		4		/* Earphone jack */
#define GPIO_XHSCALL_DET		5		/* Earphone switch */

#define GPIO_KO_JMIC_ONOFF		19		/* Jmic switch */
#define GPIO_ANSW_FET_ON		(BU1852_EXP2_GPIO_BASE+18) /* AN-SW switch */
#define GPIO_SEL_JEAR_LED		(BU1852_EXP2_GPIO_BASE+19) /* mic-pierce switch */

#define GPIO_KO_XLED_RST		(BU1852_EXP1_GPIO_BASE+15)

/** High */
#define EDC_GPIO_HIGH           1
/** Low */
#define EDC_GPIO_LOW            0

#define GPIO_HSDET_OWNER		(BU1852_EXP2_GPIO_BASE+16)	/* Earphone jack detect owner */

#define MPP_XHEADSET_VDD_DET	11		/* Earphone jack vol */
#define MPP_KO_PIERCE_DET		12		/* Earphone pierce jack vol */

#define PHI35_INSERT_CHATT_AUDIO	20	/* 60msec -> 20msec */
#define CHATT_CNT	30	/* 4 -> 30 */
#define EARPHONE_IN_VOL			400	/* 500mv->400mv */
#define EARPHONE_IN_VOL_PIERCE	100	/* 500mv->100mv */
#define EARPHONE_WAKELOCK_TO	(HZ * 60)

#define EARPHONE_ADC 1000

#define ADC_1000_LOW	0x82F5
#define ADC_1000_HIGH	0x87AE
#define ADC_1500_LOW	0x8AEB
#define ADC_1500_HIGH	0x8FF5
#define ADC_2200_LOW	0x935C
#define ADC_2200_HIGH	0x9870

#define EDC_WAKE_ON		1
#define EDC_WAKE_OFF	0

#define	EDC_APNV_PIERCE_JUDGE_I		47055
#define	EDC_APNV_PIERCE_JUDGE_II	47156

#define	EDC_APNV_PIERCE_JUDGE_I_SIZE		(12)

#define EDC_RETRY_COUNT_MAX	100

#define	PIERCE_ID_ALL_CHECK_BIT   ( PIERCE_ID_1000_OHM_CHECK_BIT | PIERCE_ID_1500_OHM_CHECK_BIT | PIERCE_ID_2200_OHM_CHECK_BIT )
						 			   
/* Initial PM8921 GPIO configurations */
static struct pm8xxx_gpio_init pm8921_gpios_disable = PM8921_GPIO_DISABLE(GPIO_XHSCALL_DET);

/* XHEADSET_DET INPUT Config	*/
static struct pm_gpio xheadset_det_in_en = {
 .direction      = PM_GPIO_DIR_IN,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

/* XHEADSET_DET OUT Config	*/
static struct pm_gpio xheadset_det_out_en = {
 .direction      = PM_GPIO_DIR_OUT,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};
/* XHSCALL_DET INPUT Config	*/
static struct pm_gpio xhscall_det_in_up = {
 .direction      = PM_GPIO_DIR_IN,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_UP_1P5,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

/* XHSCALL_DET OUT Config	*/
static struct pm_gpio xhscall_det_in_down = {
 .direction      = PM_GPIO_DIR_IN,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_DN,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

/* KO_JMIC_ONOFF INPUT Config	*/
static struct pm_gpio ko_jmic_onoff_out_L = {
 .direction      = PM_GPIO_DIR_OUT,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};

/* KO_JMIC_ONOFF OUT Config	*/
static struct pm_gpio ko_jmic_onoff_out_H = {
 .direction      = PM_GPIO_DIR_OUT,
 .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 1,
};

#if 0
static struct pm8xxx_mpp_config_data earphone_config = {
	.type		= PM8XXX_MPP_TYPE_A_INPUT,
	.level		= PM8XXX_MPP_AIN_AMUX_ABUS2,
	.control	= PM8XXX_MPP_AOUT_CTRL_MPP_LOW_EN,
};
static struct pm8xxx_mpp_config_data earphone_deconfig = {
	.type		= PM8XXX_MPP_TYPE_D_INPUT,
	.level		= PM8XXX_MPP_AIN_AMUX_ABUS2,
	.control	= PM8XXX_MPP_DIN_TO_INT,
};

static struct pm8xxx_mpp_config_data earphone_pierce_config = {
	.type		= PM8XXX_MPP_TYPE_A_INPUT,
	.level		= PM8XXX_MPP_AIN_AMUX_ABUS2,
	.control	= PM8XXX_MPP_AOUT_CTRL_MPP_LOW_EN,
};
static struct pm8xxx_mpp_config_data earphone_pierce_deconfig = {
	.type		= PM8XXX_MPP_TYPE_D_INPUT,
	.level		= PM8XXX_MPP_AIN_AMUX_ABUS2,
	.control	= PM8XXX_MPP_DIN_TO_INT,
};
#endif
#endif /* _EDC_HW_H_ */