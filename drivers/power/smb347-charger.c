/*
 * Summit Microelectronics SMB347 Battery Charger Driver
 *
 * Copyright (C) 2011, Intel Corporation
 *
 * Authors: Bruce E. Robertson <bruce.e.robertson@intel.com>
 *          Mika Westerberg <mika.westerberg@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012-2013
/*----------------------------------------------------------------------------*/

/* FUJITSU:2012-11-20 FJ_CHARGER start */
#define FJ_CHARGER
/* FUJITSU:2012-11-20 FJ_CHARGER end */

#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/power/smb347-charger.h>
#include <linux/seq_file.h>

/* FUJITSU:2012-11-20 FJ_CHARGER start */
#ifdef FJ_CHARGER
#include <asm/system.h>
#include <linux/uaccess.h>
#include <mach/board_fj.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/mfd/fj_charger.h>
#include <linux/mfd/fj_charger_local.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/mfd/pm8xxx/ccadc.h>
#include <linux/power/smb347-charger_hw.h>
#include <linux/power_supply.h>
#include <linux/nonvolatile_common.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/ovp.h>
#ifndef FJ_CHARGER_LOCAL
#include <linux/fta.h>
#endif /* FJ_CHARGER_LOCAL */
#endif /* FJ_CHARGER */

#ifdef FJ_CHARGER
//#define FJ_CHARGER_DBG
#undef FJ_CHARGER_DBG

#ifdef FJ_CHARGER_DBG
static int fj_smb_charger_debug = 1;
#define FJ_CHARGER_DBGLOG(x, y...) \
			if (fj_smb_charger_debug){ \
				printk(KERN_WARNING "[smb347_chg] " x, ## y); \
			}
#else
#define FJ_CHARGER_DBGLOG(x, y...)
#endif /* FJ_CHARGER_DBG */

#define FJ_CHARGER_INFOLOG(x, y...)			printk(KERN_INFO "[smb347_chg] " x, ## y)
#define FJ_CHARGER_WARNLOG(x, y...)			printk(KERN_WARNING "[smb347_chg] " x, ## y)
#define FJ_CHARGER_ERRLOG(x, y...)			printk(KERN_ERR "[smb347_chg] " x, ## y)

#ifndef FJ_CHARGER_LOCAL
#define FJ_CHARGER_REC_LOG(x, y...)			{												\
												char recbuf[128];							\
												snprintf(recbuf, sizeof(recbuf), x, ## y);	\
												ftadrv_send_str(recbuf);					\
											}
#else
#define FJ_CHARGER_REC_LOG(x, y...)
#endif /* FJ_CHARGER_LOCAL */

#define TEMP_FETON2_GPIO                    17              /* GPIO Number 17 */
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)      (pm_gpio - 1 + NR_GPIO_IRQS)

/* check times */
#define CHARGE_CHECK_PERIOD_MS				( 10 * 1000)			/*  10sec */
#define FULL_CHARGE_CHECK1_PERIOD_MS		(  3 * 1000 * 60)		/*   3min */
#define FULL_CHARGE_CHECK2_PERIOD_MS		(117 * 1000 * 60)		/* 117min */
#define SAFETY_CHARGE_CHECK_PERIOD_MS		( 13 * 1000 * 60 * 60)	/*  13h   */
#define AICL_RESTART_CHECK_PERIOD_MS		( 10 * 1000 * 60)		/*  10min */

/* battery not present temp(0ver -30) */
#define BATTERY_NOT_PRESENT_TEMP			(-30)

#define APNV_CHG_ADJ_VBAT_I					(41040)
#define APNV_FUNC_LIMITS_I					(41053)

#define VBAT_ADJ_RATIO						(40)
#define VBAT_TBL_MAX						(32)

#define CHARGE_MODE_INVALID					(-1)
#define CHARGE_MODE_NOT_CHARGE				(0)
#define CHARGE_MODE_MANUAL					(1)
#define CHARGE_MODE_NUM						(2)

/* SMB347 related GPIOs */
#define SMB347_INT_GPIO						(84)


#define CHARGE_MODE_IS_FJ()					fj_smb_get_fj_mode()

#define CHG_INIT_EVENT						BIT(0)
#define CHG_INIT_ERR_EVENT					BIT(1)

#define CHG_SUSPENDING						BIT(0)

/* charge err status bit */
#define FJ_CHG_PARAM_NO_ERROR			 	0x00000000
#define FJ_CHG_PARAM_BATT_TEMP_HOT		 	0x00000001
#define FJ_CHG_PARAM_BATT_TEMP_COLD		 	0x00000002
#define FJ_CHG_PARAM_LOW_BATT_VOLTAGE	 	0x00000004
#define FJ_CHG_PARAM_BATT_VOLTAGE_OV	 	0x00000008
#define FJ_CHG_PARAM_BATT_VOLTAGE_UV	 	0x00000010
#define FJ_CHG_PARAM_ADP_VOLTAGE_OV		 	0x00000020
#define FJ_CHG_PARAM_ADP_VOLTAGE_UV		 	0x00000040
#define FJ_CHG_PARAM_MOBILE_TEMP_HOT	 	0x00000080
#define FJ_CHG_PARAM_MOBILE_TEMP_COLD	 	0x00000100
#define FJ_CHG_PARAM_RECEIVER_TEMP_HOT	 	0x00000200
#define FJ_CHG_PARAM_RECEIVER_TEMP_COLD	 	0x00000400
#define FJ_CHG_PARAM_OVP_STATE_ERROR	 	0x00000800
#define FJ_CHG_PARAM_BATT_TEMP_WARM		 	0x00001000
#define FJ_CHG_PARAM_BATT_TEMP_COOL		 	0x00002000
#define FJ_CHG_PARAM_CHG_DISABLE		 	0x00004000
#define FJ_CHG_PARAM_FGIC_NOT_INITIALIZE 	0x00008000
#define FJ_CHG_PARAM_UNSPEC_FAILURE		 	0x00010000
#define FJ_CHG_PARAM_LIMITED_CURR			0x00020000
#define FJ_CHG_PARAM_CHG_IC_ERROR			0x00040000
#define FJ_CHG_PARAM_AICL_ERROR				0x00080000

#define FJ_CHG_PARAM_ERR_MASK			   (FJ_CHG_PARAM_BATT_TEMP_HOT		| \
											FJ_CHG_PARAM_BATT_TEMP_COLD		| \
											FJ_CHG_PARAM_LOW_BATT_VOLTAGE	| \
											FJ_CHG_PARAM_BATT_VOLTAGE_OV	| \
											FJ_CHG_PARAM_ADP_VOLTAGE_OV		| \
											FJ_CHG_PARAM_ADP_VOLTAGE_UV		| \
											FJ_CHG_PARAM_MOBILE_TEMP_HOT	| \
											FJ_CHG_PARAM_MOBILE_TEMP_COLD	| \
											FJ_CHG_PARAM_RECEIVER_TEMP_HOT	| \
											FJ_CHG_PARAM_RECEIVER_TEMP_COLD	| \
											FJ_CHG_PARAM_OVP_STATE_ERROR	| \
											FJ_CHG_PARAM_FGIC_NOT_INITIALIZE| \
											FJ_CHG_PARAM_AICL_ERROR)

#define FJ_CHG_PARAM_PAUSE_MASK			   (FJ_CHG_PARAM_CHG_DISABLE | \
											FJ_CHG_PARAM_CHG_IC_ERROR)

#define FJ_CHG_PARAM_LIMIT_MASK				(FJ_CHG_PARAM_BATT_TEMP_WARM	| \
											FJ_CHG_PARAM_BATT_TEMP_COOL		| \
											FJ_CHG_PARAM_BATT_TEMP_HOT		| \
											FJ_CHG_PARAM_BATT_TEMP_COLD)

#define FJ_CHG_PARAM_BATT_ERR_MASK		   (FJ_CHG_PARAM_BATT_TEMP_HOT		| \
											FJ_CHG_PARAM_BATT_TEMP_COLD)


#define FJ_CHG_VALUE_BATT_TEMP_HOT			(50)
#define FJ_CHG_VALUE_BATT_TEMP_WARM			(45)
#define FJ_CHG_VALUE_BATT_TEMP_COOL			(10)
#define FJ_CHG_VALUE_BATT_TEMP_COLD			(0)

#define FJ_CHG_VALUE_ADP_VOLTAGE_OV			0x75C2		/* 6.409V */
#define FJ_CHG_VALUE_ADP_VOLTAGE_UV			0x6D94		/* 4.000V */
#define FJ_CHG_VALUE_ADP_AICL_CHK_V			0x6EC5		/* 4.350V */

#define FJ_CHG_VALUE_LOW_BATT_VOLTAGE		0xA780		/* 3.35V  */
#define FJ_CHG_VALUE_BATT_VOLTAGE_OV		0xDE80		/* 4.45V  */
#define FJ_CHG_VALUE_UNSPEC_BATT_VOLTAGE	0xA000		/* 3.20V  */

#define FJ_CHG_VALUE_FULL_SOC_LV1			(99)		/* 99%  */
#define FJ_CHG_VALUE_FULL_SOC_LV2			(99)		/* 99%  */
#define FJ_CHG_VALUE_RECHARGE_LV1			(99)		/* 99%  */
#define FJ_CHG_VALUE_RECHARGE_LV2			(98)		/* 98%  */


#define FJ_CHG_FULL_CHECK_FACT_T1			0x00000001	/* start full check(soc threshold1) */
#define FJ_CHG_FULL_CHECK_FACT_T2			0x00000002	/* start full check(soc threshold2) */

#define FJ_CHG_LIMIT_CURRENT_900			(900)		/* charge current (mA) */
#define FJ_CHG_LIMIT_CURRENT_500			(500)		/* charge current (mA) */
#define FJ_CHG_LIMIT_CURRENT_300			(300)		/* charge current (mA) */

#define FJ_CHG_CURRENT_NOT_LIMITED			(0)			/* charge current not limited   */
#define FJ_CHG_CURRENT_LIMITED_900			(1)			/* charge current limited 900mA */
#define FJ_CHG_CURRENT_LIMITED_500			(2)			/* charge current limited 500mA */
#define FJ_CHG_CURRENT_LIMITED_300			(3)			/* charge current limited 300mA */

#define FJ_CHG_NOT_CHANGE_CL				(0)			/* not change charge current limit */
#define FJ_CHG_CHANGE_CL					(1)			/* change charge current limit */

#define FJ_CHG_IC_ERR_DETECT_CNT			(3)

#define FJ_CHG_BATT_CURR_CHECK				0x280

static const int fj_vbat_tbl[VBAT_TBL_MAX] =
{
	  0,		/* +0  */ /* NV Setting	 = 0x00 */
	  1,		/* +1  */ /*			 = 0x01 */
	  2,		/* +2  */ /*			 = 0x02 */
	  3,		/* +3  */ /* 			 = 0x03 */
	  4,		/* +4  */ /* 			 = 0x04 */
	  5,		/* +5  */ /* 			 = 0x05 */
	  6,		/* +6  */ /* 			 = 0x06 */
	  7,		/* +7  */ /* 			 = 0x07 */
	  8,		/* +8  */ /* 			 = 0x08 */
	  9,		/* +9  */ /* 			 = 0x09 */
	 10,		/* +10 */ /* 			 = 0x0A */
	 11,		/* +11 */ /* 			 = 0x0B */
	 12,		/* +12 */ /* 			 = 0x0C */
	 13,		/* +13 */ /* 			 = 0x0D */
	 14,		/* +14 */ /* 			 = 0x0E */
	 15,		/* +15 */ /* 			 = 0x0F */
	-16,		/* -16 */ /* 			 = 0x10 */
	-15,		/* -15 */ /*			 = 0x11 */
	-14,		/* -14 */ /* 			 = 0x12 */
	-13,		/* -13 */ /* 			 = 0x13 */
	-12,		/* -12 */ /* 			 = 0x14 */
	-11,		/* -11 */ /* 			 = 0x15 */
	-10,		/* -10 */ /* 			 = 0x16 */
	 -9,		/* -9  */ /* 			 = 0x17 */
	 -8,		/* -8  */ /* 			 = 0x18 */
	 -7,		/* -7  */ /* 			 = 0x19 */
	 -6,		/* -6  */ /* 			 = 0x1A */
	 -5,		/* -5  */ /* 			 = 0x1B */
	 -4,		/* -4  */ /* 			 = 0x1C */
	 -3,		/* -3  */ /* 			 = 0x1D */
	 -2,		/* -2  */ /* 			 = 0x1E */
	 -1			/* -1  */ /* 			 = 0x1F */
};

struct fj_smb_charger_param
{
	int supply_volt;
	int batt_volt;
	int batt_temp;
	int mobile_temp;
	int receiver_temp;
	int chg_ic_temp;
	int board_temp;
	int sensor_temp;
	int soc;
	int soc_raw;
	int chg_ic_err;
	int aicl;
};

struct fj_smb_chg_queue_t {
	struct list_head list;
	struct work_struct *work;
};

typedef enum {
	FJ_CHG_STATE_NOT_CHARGING = 0,			/* not charging				*/
	FJ_CHG_STATE_CHARGING,					/* charging/re-charging		*/
	FJ_CHG_STATE_FULL_CHECKING,				/* full charge check		*/
	FJ_CHG_STATE_FULL,						/* full charging			*/
	FJ_CHG_STATE_ERROR,						/* charge error occurred	*/
	FJ_CHG_STATE_PAUSE,						/* charging paused			*/
	FJ_CHG_STATE_TIME_OUT,					/* safe timer expire		*/
	FJ_CHG_STATE_NUM
} fj_smb_charger_state_type;

typedef enum {
	FJ_CHG_EVENT_MONITOR_PARAM = 0,			/* period monitor			*/
	FJ_CHG_EVENT_CHARGE_START,				/* charge start				*/
	FJ_CHG_EVENT_CHARGE_STOP,				/* charge end				*/
	FJ_CHG_EVENT_FULL_CHECK,				/* additional charge		*/
	FJ_CHG_EVENT_FULL_CHECK_END,			/* full charge				*/
	FJ_CHG_EVENT_SAFE_TIMEOUT,				/* safe timer expire		*/
	FJ_CHG_EVENT_NUM
} fj_smb_charger_event_type;

typedef enum {
	FJ_CHG_ERR_NON = 0,						/* error not occurred		*/
	FJ_CHG_ERR_SUPPLY_VOLTAGE_HIGH_ERR,		/* over supply voltage		*/
	FJ_CHG_ERR_SUPPLY_VOLTAGE_LOW_ERR,		/* under supply voltage		*/
	FJ_CHG_ERR_BATT_TEMP_HIGH_ERR,			/* batt temp hi 			*/
	FJ_CHG_ERR_BATT_TEMP_LOW_ERR,			/* batt temp low			*/
	FJ_CHG_ERR_MOBILE_TEMP_HIGH_ERR,		/* mobile temp hi			*/
	FJ_CHG_ERR_MOBILE_TEMP_LOW_ERR,			/* mobile temp low			*/
	FJ_CHG_ERR_OVER_BATT_VOLTAGE,			/* over batt voltage		*/
	FJ_CHG_ERR_EXPIER_TIME_OUT,				/* safe timer expire		*/
	FJ_CHG_ERR_NUM
} fj_smb_charger_err_type;

typedef enum {
	FJ_CHG_FULL_CHECK_NON = 0,
	FJ_CHG_FULL_CHECK1,
	FJ_CHG_FULL_CHECK2,
	FJ_CHG_FULL_CHECK_NUM
} fj_smb_charger_full_check_type;

struct fj_smb_charger_full_chk_param_table
{
	int age_upper;
	int age_lower;
	int fullsoc_threshold;
	int vmax_adj;
};

static struct fj_smb_charger_full_chk_param_table fj_smb_chg_full_chk_param_table[] =
{
	/* upper,	lower,	full_thr,	vmax_adj */
	{  100,		91,			99,		   0	 },
	{   90,		88,			97,		   1	 },
	{   87,		84,			95,		   2	 },
	{   83,		81,			93,		   3	 },
	{   80,		78,			91,		   4	 },
	{   77,		76,			89,		   5	 },
	{   75,		74,			87,		   6	 },
	{   73,		 0,			85,		   7	 }
};

#define FJ_SMB_CHG_FULL_CHK_PARAM_MAX		(sizeof(fj_smb_chg_full_chk_param_table)/sizeof(struct fj_smb_charger_full_chk_param_table))


#endif /* FJ_CHARGER */
/* FUJITSU:2012-11-20 FJ_CHARGER end */

/*
 * Configuration registers. These are mirrored to volatile RAM and can be
 * written once %CMD_A_ALLOW_WRITE is set in %CMD_A register. They will be
 * reloaded from non-volatile registers after POR.
 */
#define CFG_CHARGE_CURRENT			0x00
#define CFG_CHARGE_CURRENT_FCC_MASK		0xe0
#define CFG_CHARGE_CURRENT_FCC_SHIFT		5
#define CFG_CHARGE_CURRENT_PCC_MASK		0x18
#define CFG_CHARGE_CURRENT_PCC_SHIFT		3
#define CFG_CHARGE_CURRENT_TC_MASK		0x07
#define CFG_CURRENT_LIMIT			0x01
#define CFG_CURRENT_LIMIT_DC_MASK		0xf0
#define CFG_CURRENT_LIMIT_DC_SHIFT		4
#define CFG_CURRENT_LIMIT_USB_MASK		0x0f
#define CFG_FLOAT_VOLTAGE			0x03
#define CFG_FLOAT_VOLTAGE_THRESHOLD_MASK	0xc0
#define CFG_FLOAT_VOLTAGE_THRESHOLD_SHIFT	6
#define CFG_STAT				0x05
#define CFG_STAT_DISABLED			BIT(5)
#define CFG_STAT_ACTIVE_HIGH			BIT(7)
#define CFG_PIN					0x06
#define CFG_PIN_EN_CTRL_MASK			0x60
#define CFG_PIN_EN_CTRL_ACTIVE_HIGH		0x40
#define CFG_PIN_EN_CTRL_ACTIVE_LOW		0x60
#define CFG_PIN_EN_APSD_IRQ			BIT(1)
#define CFG_PIN_EN_CHARGER_ERROR		BIT(2)
#define CFG_THERM				0x07
#define CFG_THERM_SOFT_HOT_COMPENSATION_MASK	0x03
#define CFG_THERM_SOFT_HOT_COMPENSATION_SHIFT	0
#define CFG_THERM_SOFT_COLD_COMPENSATION_MASK	0x0c
#define CFG_THERM_SOFT_COLD_COMPENSATION_SHIFT	2
#define CFG_THERM_MONITOR_DISABLED		BIT(4)
#define CFG_SYSOK				0x08
#define CFG_SYSOK_SUSPEND_HARD_LIMIT_DISABLED	BIT(2)
#define CFG_OTHER				0x09
#define CFG_OTHER_RID_MASK			0xc0
#define CFG_OTHER_RID_ENABLED_AUTO_OTG		0xc0
#define CFG_OTG					0x0a
#define CFG_OTG_TEMP_THRESHOLD_MASK		0x30
#define CFG_OTG_TEMP_THRESHOLD_SHIFT		4
#define CFG_OTG_CC_COMPENSATION_MASK		0xc0
#define CFG_OTG_CC_COMPENSATION_SHIFT		6
#define CFG_TEMP_LIMIT				0x0b
#define CFG_TEMP_LIMIT_SOFT_HOT_MASK		0x03
#define CFG_TEMP_LIMIT_SOFT_HOT_SHIFT		0
#define CFG_TEMP_LIMIT_SOFT_COLD_MASK		0x0c
#define CFG_TEMP_LIMIT_SOFT_COLD_SHIFT		2
#define CFG_TEMP_LIMIT_HARD_HOT_MASK		0x30
#define CFG_TEMP_LIMIT_HARD_HOT_SHIFT		4
#define CFG_TEMP_LIMIT_HARD_COLD_MASK		0xc0
#define CFG_TEMP_LIMIT_HARD_COLD_SHIFT		6
#define CFG_FAULT_IRQ				0x0c
#define CFG_FAULT_IRQ_DCIN_UV			BIT(2)
#define CFG_STATUS_IRQ				0x0d
#define CFG_STATUS_IRQ_TERMINATION_OR_TAPER	BIT(4)
#define CFG_ADDRESS				0x0e

/* Command registers */
#define CMD_A					0x30
#define CMD_A_CHG_ENABLED			BIT(1)
#define CMD_A_SUSPEND_ENABLED			BIT(2)
#define CMD_A_ALLOW_WRITE			BIT(7)
#define CMD_B					0x31
#define CMD_C					0x33

/* Interrupt Status registers */
#define IRQSTAT_A				0x35
#define IRQSTAT_C				0x37
#define IRQSTAT_C_TERMINATION_STAT		BIT(0)
#define IRQSTAT_C_TERMINATION_IRQ		BIT(1)
#define IRQSTAT_C_TAPER_IRQ			BIT(3)
#define IRQSTAT_E				0x39
#define IRQSTAT_E_USBIN_UV_STAT			BIT(0)
#define IRQSTAT_E_USBIN_UV_IRQ			BIT(1)
#define IRQSTAT_E_DCIN_UV_STAT			BIT(4)
#define IRQSTAT_E_DCIN_UV_IRQ			BIT(5)
#define IRQSTAT_F				0x3a

/* Status registers */
#define STAT_A					0x3b
#define STAT_A_FLOAT_VOLTAGE_MASK		0x3f
#define STAT_B					0x3c
#define STAT_C					0x3d
#define STAT_C_CHG_ENABLED			BIT(0)
#define STAT_C_CHG_MASK				0x06
#define STAT_C_CHG_SHIFT			1
#define STAT_C_CHARGER_ERROR			BIT(6)
#define STAT_E					0x3f

/* FUJITSU:2012-11-20 FJ_CHARGER start */
#ifndef FJ_CHARGER
/**
 * struct smb347_charger - smb347 charger instance
 * @lock: protects concurrent access to online variables
 * @client: pointer to i2c client
 * @mains: power_supply instance for AC/DC power
 * @usb: power_supply instance for USB power
 * @battery: power_supply instance for battery
 * @mains_online: is AC/DC input connected
 * @usb_online: is USB input connected
 * @charging_enabled: is charging enabled
 * @dentry: for debugfs
 * @pdata: pointer to platform data
 */
struct smb347_charger {
	struct mutex		lock;
	struct i2c_client	*client;
	struct power_supply	mains;
	struct power_supply	usb;
	struct power_supply	battery;
	bool			mains_online;
	bool			usb_online;
	bool			charging_enabled;
	struct dentry		*dentry;
	const struct smb347_charger_platform_data *pdata;
};
#endif /* !FJ_CHARGER */
/* FUJITSU:2012-11-20 FJ_CHARGER end */

/* FUJITSU:2012-11-20 FJ_CHARGER start */
#ifdef FJ_CHARGER
static int fj_smb_chg_stay_full = 0;
static struct dentry *fj_smb_chg_debug_root;    /* for debugfs */
static unsigned int fj_smb_chg_old_limit = 0;
struct smb347_charger *the_smb = NULL;
static unsigned long fj_smb_charger_src = 0;
static unsigned int fj_smb_charger_src_current[CHG_SOURCE_NUM] = {0};
static unsigned int fj_smb_chg_err_state = FJ_CHG_ERROR_NONE;
static unsigned int fj_smb_chg_enable = FJ_CHG_ENABLE;
static unsigned long fj_smb_chg_init_event = 0;
static unsigned int fj_smb_chg_full_check_factor = 0;
static unsigned char fj_smb_chg_change_curr_limit = 0;	/* change charge current limit */
static int fj_smb_chg_curr_limit = 0;					/* Enable/Disable charge current limit */
static int fj_smb_chg_ic_error = 0;
static int fj_smb_chg_ic_error_cnt[FJ_SMB_CHG_IC_ERROR_MAX] = {0};
static int fj_smb_chg_aicl_restart = 0;
static int fj_smb_chg_aicl_error_recovery_check_flag = 0;
static unsigned long fj_smb_chg_aicl_expire = 0;
static fj_smb_charger_full_check_type fj_smb_chg_full_check_state = FJ_CHG_FULL_CHECK_NON;

int fj_smb_chg_435V_flag = 1;

static DEFINE_SPINLOCK(fj_smb_chg_info_lock);
static DEFINE_SPINLOCK(fj_smb_chg_list_lock);
static LIST_HEAD(fj_smb_chg_list_head);

extern int charging_mode;
#endif /* FJ_CHARGER */
/* FUJITSU:2012-11-20 FJ_CHARGER end */

/* Fast charge current in uA */
static const unsigned int fcc_tbl[] = {
	700000,
	900000,
	1200000,
	1500000,
	1800000,
	2000000,
	2200000,
	2500000,
};

/* Pre-charge current in uA */
static const unsigned int pcc_tbl[] = {
	100000,
	150000,
	200000,
	250000,
};

/* Termination current in uA */
static const unsigned int tc_tbl[] = {
	37500,
	50000,
	100000,
	150000,
	200000,
	250000,
	500000,
	600000,
};

/* Input current limit in uA */
static const unsigned int icl_tbl[] = {
	300000,
	500000,
	700000,
	900000,
	1200000,
	1500000,
	1800000,
	2000000,
	2200000,
	2500000,
};

/* Charge current compensation in uA */
static const unsigned int ccc_tbl[] = {
	250000,
	700000,
	900000,
	1200000,
};

/* Convert register value to current using lookup table */
static int hw_to_current(const unsigned int *tbl, size_t size, unsigned int val)
{
	if (val >= size)
		return -EINVAL;
	return tbl[val];
}

/* Convert current to register value using lookup table */
static int current_to_hw(const unsigned int *tbl, size_t size, unsigned int val)
{
	size_t i;

	for (i = 0; i < size; i++)
		if (val < tbl[i])
			break;
	return i > 0 ? i - 1 : -EINVAL;
}

static int smb347_read(struct smb347_charger *smb, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(smb->client, reg);
	if (ret < 0)
		dev_warn(&smb->client->dev, "failed to read reg 0x%x: %d\n",
			 reg, ret);
	return ret;
}

static int smb347_write(struct smb347_charger *smb, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(smb->client, reg, val);
	if (ret < 0)
		dev_warn(&smb->client->dev, "failed to write reg 0x%x: %d\n",
			 reg, ret);
	return ret;
}

/**
 * smb347_update_status - updates the charging status
 * @smb: pointer to smb347 charger instance
 *
 * Function checks status of the charging and updates internal state
 * accordingly. Returns %0 if there is no change in status, %1 if the
 * status has changed and negative errno in case of failure.
 */
static int smb347_update_status(struct smb347_charger *smb)
{
	bool usb = false;
	bool dc = false;
	int ret;

	ret = smb347_read(smb, IRQSTAT_E);
	if (ret < 0)
		return ret;

	/*
	 * Dc and usb are set depending on whether they are enabled in
	 * platform data _and_ whether corresponding undervoltage is set.
	 */
	if (smb->pdata->use_mains)
		dc = !(ret & IRQSTAT_E_DCIN_UV_STAT);
	if (smb->pdata->use_usb)
		usb = !(ret & IRQSTAT_E_USBIN_UV_STAT);

	mutex_lock(&smb->lock);
	ret = smb->mains_online != dc || smb->usb_online != usb;
	smb->mains_online = dc;
	smb->usb_online = usb;
	mutex_unlock(&smb->lock);

	return ret;
}

/*
 * smb347_is_online - returns whether input power source is connected
 * @smb: pointer to smb347 charger instance
 *
 * Returns %true if input power source is connected. Note that this is
 * dependent on what platform has configured for usable power sources. For
 * example if USB is disabled, this will return %false even if the USB
 * cable is connected.
 */
static bool smb347_is_online(struct smb347_charger *smb)
{
	bool ret;

	mutex_lock(&smb->lock);
	ret = smb->usb_online || smb->mains_online;
	mutex_unlock(&smb->lock);

	return ret;
}

/**
 * smb347_charging_status - returns status of charging
 * @smb: pointer to smb347 charger instance
 *
 * Function returns charging status. %0 means no charging is in progress,
 * %1 means pre-charging, %2 fast-charging and %3 taper-charging.
 */
static int smb347_charging_status(struct smb347_charger *smb)
{
	int ret;

	if (!smb347_is_online(smb))
		return 0;

	ret = smb347_read(smb, STAT_C);
	if (ret < 0)
		return 0;

	return (ret & STAT_C_CHG_MASK) >> STAT_C_CHG_SHIFT;
}

static int smb347_charging_set(struct smb347_charger *smb, bool enable)
{
	int ret = 0;

	if (smb->pdata->enable_control != SMB347_CHG_ENABLE_SW) {
		dev_dbg(&smb->client->dev,
			"charging enable/disable in SW disabled\n");
		return 0;
	}

	mutex_lock(&smb->lock);
	if (smb->charging_enabled != enable) {
		ret = smb347_read(smb, CMD_A);
		if (ret < 0)
			goto out;

		smb->charging_enabled = enable;

		if (enable)
			ret |= CMD_A_CHG_ENABLED;
		else
			ret &= ~CMD_A_CHG_ENABLED;

		ret = smb347_write(smb, CMD_A, ret);
	}
out:
	mutex_unlock(&smb->lock);
	return ret;
}

static inline int smb347_charging_enable(struct smb347_charger *smb)
{
	return smb347_charging_set(smb, true);
}

static inline int smb347_charging_disable(struct smb347_charger *smb)
{
	return smb347_charging_set(smb, false);
}

static int smb347_update_online(struct smb347_charger *smb)
{
	int ret;

	/*
	 * Depending on whether valid power source is connected or not, we
	 * disable or enable the charging. We do it manually because it
	 * depends on how the platform has configured the valid inputs.
	 */
	if (smb347_is_online(smb)) {
		ret = smb347_charging_enable(smb);
		if (ret < 0)
			dev_err(&smb->client->dev,
				"failed to enable charging\n");
	} else {
		ret = smb347_charging_disable(smb);
		if (ret < 0)
			dev_err(&smb->client->dev,
				"failed to disable charging\n");
	}

	return ret;
}

static int smb347_set_charge_current(struct smb347_charger *smb)
{
	int ret, val;

	ret = smb347_read(smb, CFG_CHARGE_CURRENT);
	if (ret < 0)
		return ret;

	if (smb->pdata->max_charge_current) {
		val = current_to_hw(fcc_tbl, ARRAY_SIZE(fcc_tbl),
				    smb->pdata->max_charge_current);
		if (val < 0)
			return val;

		ret &= ~CFG_CHARGE_CURRENT_FCC_MASK;
		ret |= val << CFG_CHARGE_CURRENT_FCC_SHIFT;
	}

	if (smb->pdata->pre_charge_current) {
		val = current_to_hw(pcc_tbl, ARRAY_SIZE(pcc_tbl),
				    smb->pdata->pre_charge_current);
		if (val < 0)
			return val;

		ret &= ~CFG_CHARGE_CURRENT_PCC_MASK;
		ret |= val << CFG_CHARGE_CURRENT_PCC_SHIFT;
	}

	if (smb->pdata->termination_current) {
		val = current_to_hw(tc_tbl, ARRAY_SIZE(tc_tbl),
				    smb->pdata->termination_current);
		if (val < 0)
			return val;

		ret &= ~CFG_CHARGE_CURRENT_TC_MASK;
		ret |= val;
	}

	return smb347_write(smb, CFG_CHARGE_CURRENT, ret);
}

static int smb347_set_current_limits(struct smb347_charger *smb)
{
	int ret, val;

	ret = smb347_read(smb, CFG_CURRENT_LIMIT);
	if (ret < 0)
		return ret;

	if (smb->pdata->mains_current_limit) {
		val = current_to_hw(icl_tbl, ARRAY_SIZE(icl_tbl),
				    smb->pdata->mains_current_limit);
		if (val < 0)
			return val;

		ret &= ~CFG_CURRENT_LIMIT_DC_MASK;
		ret |= val << CFG_CURRENT_LIMIT_DC_SHIFT;
	}

	if (smb->pdata->usb_hc_current_limit) {
		val = current_to_hw(icl_tbl, ARRAY_SIZE(icl_tbl),
				    smb->pdata->usb_hc_current_limit);
		if (val < 0)
			return val;

		ret &= ~CFG_CURRENT_LIMIT_USB_MASK;
		ret |= val;
	}

	return smb347_write(smb, CFG_CURRENT_LIMIT, ret);
}

static int smb347_set_voltage_limits(struct smb347_charger *smb)
{
	int ret, val;

	ret = smb347_read(smb, CFG_FLOAT_VOLTAGE);
	if (ret < 0)
		return ret;

	if (smb->pdata->pre_to_fast_voltage) {
		val = smb->pdata->pre_to_fast_voltage;

		/* uV */
		val = clamp_val(val, 2400000, 3000000) - 2400000;
		val /= 200000;

		ret &= ~CFG_FLOAT_VOLTAGE_THRESHOLD_MASK;
		ret |= val << CFG_FLOAT_VOLTAGE_THRESHOLD_SHIFT;
	}

	if (smb->pdata->max_charge_voltage) {
		val = smb->pdata->max_charge_voltage;

		/* uV */
		val = clamp_val(val, 3500000, 4500000) - 3500000;
		val /= 20000;

		ret |= val;
	}

	return smb347_write(smb, CFG_FLOAT_VOLTAGE, ret);
}

static int smb347_set_temp_limits(struct smb347_charger *smb)
{
	bool enable_therm_monitor = false;
	int ret, val;

	if (smb->pdata->chip_temp_threshold) {
		val = smb->pdata->chip_temp_threshold;

		/* degree C */
		val = clamp_val(val, 100, 130) - 100;
		val /= 10;

		ret = smb347_read(smb, CFG_OTG);
		if (ret < 0)
			return ret;

		ret &= ~CFG_OTG_TEMP_THRESHOLD_MASK;
		ret |= val << CFG_OTG_TEMP_THRESHOLD_SHIFT;

		ret = smb347_write(smb, CFG_OTG, ret);
		if (ret < 0)
			return ret;
	}

	ret = smb347_read(smb, CFG_TEMP_LIMIT);
	if (ret < 0)
		return ret;

	if (smb->pdata->soft_cold_temp_limit != SMB347_TEMP_USE_DEFAULT) {
		val = smb->pdata->soft_cold_temp_limit;

		val = clamp_val(val, 0, 15);
		val /= 5;
		/* this goes from higher to lower so invert the value */
		val = ~val & 0x3;

		ret &= ~CFG_TEMP_LIMIT_SOFT_COLD_MASK;
		ret |= val << CFG_TEMP_LIMIT_SOFT_COLD_SHIFT;

		enable_therm_monitor = true;
	}

	if (smb->pdata->soft_hot_temp_limit != SMB347_TEMP_USE_DEFAULT) {
		val = smb->pdata->soft_hot_temp_limit;

		val = clamp_val(val, 40, 55) - 40;
		val /= 5;

		ret &= ~CFG_TEMP_LIMIT_SOFT_HOT_MASK;
		ret |= val << CFG_TEMP_LIMIT_SOFT_HOT_SHIFT;

		enable_therm_monitor = true;
	}

	if (smb->pdata->hard_cold_temp_limit != SMB347_TEMP_USE_DEFAULT) {
		val = smb->pdata->hard_cold_temp_limit;

		val = clamp_val(val, -5, 10) + 5;
		val /= 5;
		/* this goes from higher to lower so invert the value */
		val = ~val & 0x3;

		ret &= ~CFG_TEMP_LIMIT_HARD_COLD_MASK;
		ret |= val << CFG_TEMP_LIMIT_HARD_COLD_SHIFT;

		enable_therm_monitor = true;
	}

	if (smb->pdata->hard_hot_temp_limit != SMB347_TEMP_USE_DEFAULT) {
		val = smb->pdata->hard_hot_temp_limit;

		val = clamp_val(val, 50, 65) - 50;
		val /= 5;

		ret &= ~CFG_TEMP_LIMIT_HARD_HOT_MASK;
		ret |= val << CFG_TEMP_LIMIT_HARD_HOT_SHIFT;

		enable_therm_monitor = true;
	}

	ret = smb347_write(smb, CFG_TEMP_LIMIT, ret);
	if (ret < 0)
		return ret;

	/*
	 * If any of the temperature limits are set, we also enable the
	 * thermistor monitoring.
	 *
	 * When soft limits are hit, the device will start to compensate
	 * current and/or voltage depending on the configuration.
	 *
	 * When hard limit is hit, the device will suspend charging
	 * depending on the configuration.
	 */
	if (enable_therm_monitor) {
		ret = smb347_read(smb, CFG_THERM);
		if (ret < 0)
			return ret;

		ret &= ~CFG_THERM_MONITOR_DISABLED;

		ret = smb347_write(smb, CFG_THERM, ret);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->suspend_on_hard_temp_limit) {
		ret = smb347_read(smb, CFG_SYSOK);
		if (ret < 0)
			return ret;

		ret &= ~CFG_SYSOK_SUSPEND_HARD_LIMIT_DISABLED;

		ret = smb347_write(smb, CFG_SYSOK, ret);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->soft_temp_limit_compensation !=
	    SMB347_SOFT_TEMP_COMPENSATE_DEFAULT) {
		val = smb->pdata->soft_temp_limit_compensation & 0x3;

		ret = smb347_read(smb, CFG_THERM);
		if (ret < 0)
			return ret;

		ret &= ~CFG_THERM_SOFT_HOT_COMPENSATION_MASK;
		ret |= val << CFG_THERM_SOFT_HOT_COMPENSATION_SHIFT;

		ret &= ~CFG_THERM_SOFT_COLD_COMPENSATION_MASK;
		ret |= val << CFG_THERM_SOFT_COLD_COMPENSATION_SHIFT;

		ret = smb347_write(smb, CFG_THERM, ret);
		if (ret < 0)
			return ret;
	}

	if (smb->pdata->charge_current_compensation) {
		val = current_to_hw(ccc_tbl, ARRAY_SIZE(ccc_tbl),
				    smb->pdata->charge_current_compensation);
		if (val < 0)
			return val;

		ret = smb347_read(smb, CFG_OTG);
		if (ret < 0)
			return ret;

		ret &= ~CFG_OTG_CC_COMPENSATION_MASK;
		ret |= (val & 0x3) << CFG_OTG_CC_COMPENSATION_SHIFT;

		ret = smb347_write(smb, CFG_OTG, ret);
		if (ret < 0)
			return ret;
	}

	return ret;
}

/*
 * smb347_set_writable - enables/disables writing to non-volatile registers
 * @smb: pointer to smb347 charger instance
 *
 * You can enable/disable writing to the non-volatile configuration
 * registers by calling this function.
 *
 * Returns %0 on success and negative errno in case of failure.
 */
static int smb347_set_writable(struct smb347_charger *smb, bool writable)
{
	int ret;

	ret = smb347_read(smb, CMD_A);
	if (ret < 0)
		return ret;

	if (writable)
		ret |= CMD_A_ALLOW_WRITE;
	else
		ret &= ~CMD_A_ALLOW_WRITE;

	return smb347_write(smb, CMD_A, ret);
}

static int smb347_hw_init(struct smb347_charger *smb)
{
	int ret;

	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		return ret;

	/*
	 * Program the platform specific configuration values to the device
	 * first.
	 */
	ret = smb347_set_charge_current(smb);
	if (ret < 0)
		goto fail;

	ret = smb347_set_current_limits(smb);
	if (ret < 0)
		goto fail;

	ret = smb347_set_voltage_limits(smb);
	if (ret < 0)
		goto fail;

	ret = smb347_set_temp_limits(smb);
	if (ret < 0)
		goto fail;

	/* If USB charging is disabled we put the USB in suspend mode */
	if (!smb->pdata->use_usb) {
		ret = smb347_read(smb, CMD_A);
		if (ret < 0)
			goto fail;

		ret |= CMD_A_SUSPEND_ENABLED;

		ret = smb347_write(smb, CMD_A, ret);
		if (ret < 0)
			goto fail;
	}

	ret = smb347_read(smb, CFG_OTHER);
	if (ret < 0)
		goto fail;

	/*
	 * If configured by platform data, we enable hardware Auto-OTG
	 * support for driving VBUS. Otherwise we disable it.
	 */
	ret &= ~CFG_OTHER_RID_MASK;
	if (smb->pdata->use_usb_otg)
		ret |= CFG_OTHER_RID_ENABLED_AUTO_OTG;

	ret = smb347_write(smb, CFG_OTHER, ret);
	if (ret < 0)
		goto fail;

	ret = smb347_read(smb, CFG_PIN);
	if (ret < 0)
		goto fail;

	/*
	 * Make the charging functionality controllable by a write to the
	 * command register unless pin control is specified in the platform
	 * data.
	 */
	ret &= ~CFG_PIN_EN_CTRL_MASK;

	switch (smb->pdata->enable_control) {
	case SMB347_CHG_ENABLE_SW:
		/* Do nothing, 0 means i2c control */
		break;
	case SMB347_CHG_ENABLE_PIN_ACTIVE_LOW:
		ret |= CFG_PIN_EN_CTRL_ACTIVE_LOW;
		break;
	case SMB347_CHG_ENABLE_PIN_ACTIVE_HIGH:
		ret |= CFG_PIN_EN_CTRL_ACTIVE_HIGH;
		break;
	}

	/* Disable Automatic Power Source Detection (APSD) interrupt. */
	ret &= ~CFG_PIN_EN_APSD_IRQ;

	ret = smb347_write(smb, CFG_PIN, ret);
	if (ret < 0)
		goto fail;

	ret = smb347_update_status(smb);
	if (ret < 0)
		goto fail;

	ret = smb347_update_online(smb);

fail:
	smb347_set_writable(smb, false);
	return ret;
}

static irqreturn_t smb347_interrupt(int irq, void *data)
{
	struct smb347_charger *smb = data;
	int stat_c, irqstat_e, irqstat_c;
	irqreturn_t ret = IRQ_NONE;

	stat_c = smb347_read(smb, STAT_C);
	if (stat_c < 0) {
		dev_warn(&smb->client->dev, "reading STAT_C failed\n");
		return IRQ_NONE;
	}

	irqstat_c = smb347_read(smb, IRQSTAT_C);
	if (irqstat_c < 0) {
		dev_warn(&smb->client->dev, "reading IRQSTAT_C failed\n");
		return IRQ_NONE;
	}

	irqstat_e = smb347_read(smb, IRQSTAT_E);
	if (irqstat_e < 0) {
		dev_warn(&smb->client->dev, "reading IRQSTAT_E failed\n");
		return IRQ_NONE;
	}

	/*
	 * If we get charger error we report the error back to user and
	 * disable charging.
	 */
	if (stat_c & STAT_C_CHARGER_ERROR) {
		dev_err(&smb->client->dev,
			"error in charger, disabling charging\n");

		smb347_charging_disable(smb);
		power_supply_changed(&smb->battery);

		ret = IRQ_HANDLED;
	}

	/*
	 * If we reached the termination current the battery is charged and
	 * we can update the status now. Charging is automatically
	 * disabled by the hardware.
	 */
	if (irqstat_c & (IRQSTAT_C_TERMINATION_IRQ | IRQSTAT_C_TAPER_IRQ)) {
		if (irqstat_c & IRQSTAT_C_TERMINATION_STAT)
			power_supply_changed(&smb->battery);
		ret = IRQ_HANDLED;
	}

	/*
	 * If we got an under voltage interrupt it means that AC/USB input
	 * was connected or disconnected.
	 */
	if (irqstat_e & (IRQSTAT_E_USBIN_UV_IRQ | IRQSTAT_E_DCIN_UV_IRQ)) {
		if (smb347_update_status(smb) > 0) {
			smb347_update_online(smb);
			power_supply_changed(&smb->mains);
			power_supply_changed(&smb->usb);
		}
		ret = IRQ_HANDLED;
	}

	return ret;
}

static int smb347_irq_set(struct smb347_charger *smb, bool enable)
{
	int ret;

	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		return ret;

	/*
	 * Enable/disable interrupts for:
	 *	- under voltage
	 *	- termination current reached
	 *	- charger error
	 */
	if (enable) {
		ret = smb347_write(smb, CFG_FAULT_IRQ, CFG_FAULT_IRQ_DCIN_UV);
		if (ret < 0)
			goto fail;

		ret = smb347_write(smb, CFG_STATUS_IRQ,
				   CFG_STATUS_IRQ_TERMINATION_OR_TAPER);
		if (ret < 0)
			goto fail;

		ret = smb347_read(smb, CFG_PIN);
		if (ret < 0)
			goto fail;

		ret |= CFG_PIN_EN_CHARGER_ERROR;

		ret = smb347_write(smb, CFG_PIN, ret);
	} else {
		ret = smb347_write(smb, CFG_FAULT_IRQ, 0);
		if (ret < 0)
			goto fail;

		ret = smb347_write(smb, CFG_STATUS_IRQ, 0);
		if (ret < 0)
			goto fail;

		ret = smb347_read(smb, CFG_PIN);
		if (ret < 0)
			goto fail;

		ret &= ~CFG_PIN_EN_CHARGER_ERROR;

		ret = smb347_write(smb, CFG_PIN, ret);
	}

fail:
	smb347_set_writable(smb, false);
	return ret;
}

static inline int smb347_irq_enable(struct smb347_charger *smb)
{
	return smb347_irq_set(smb, true);
}

static inline int smb347_irq_disable(struct smb347_charger *smb)
{
	return smb347_irq_set(smb, false);
}

static int smb347_irq_init(struct smb347_charger *smb)
{
	const struct smb347_charger_platform_data *pdata = smb->pdata;
	int ret, irq = gpio_to_irq(pdata->irq_gpio);

	ret = gpio_request_one(pdata->irq_gpio, GPIOF_IN, smb->client->name);
	if (ret < 0)
		goto fail;

	ret = request_threaded_irq(irq, NULL, smb347_interrupt,
				   IRQF_TRIGGER_FALLING, smb->client->name,
				   smb);
	if (ret < 0)
		goto fail_gpio;

	ret = smb347_set_writable(smb, true);
	if (ret < 0)
		goto fail_irq;

	/*
	 * Configure the STAT output to be suitable for interrupts: disable
	 * all other output (except interrupts) and make it active low.
	 */
	ret = smb347_read(smb, CFG_STAT);
	if (ret < 0)
		goto fail_readonly;

	ret &= ~CFG_STAT_ACTIVE_HIGH;
	ret |= CFG_STAT_DISABLED;

	ret = smb347_write(smb, CFG_STAT, ret);
	if (ret < 0)
		goto fail_readonly;

	ret = smb347_irq_enable(smb);
	if (ret < 0)
		goto fail_readonly;

	smb347_set_writable(smb, false);
	smb->client->irq = irq;
	return 0;

fail_readonly:
	smb347_set_writable(smb, false);
fail_irq:
	free_irq(irq, smb);
fail_gpio:
	gpio_free(pdata->irq_gpio);
fail:
	smb->client->irq = 0;
	return ret;
}

static int smb347_mains_get_property(struct power_supply *psy,
				     enum power_supply_property prop,
				     union power_supply_propval *val)
{
	struct smb347_charger *smb =
		container_of(psy, struct smb347_charger, mains);

	if (prop == POWER_SUPPLY_PROP_ONLINE) {
		val->intval = smb->mains_online;
		return 0;
	}
	return -EINVAL;
}

static enum power_supply_property smb347_mains_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int smb347_usb_get_property(struct power_supply *psy,
				   enum power_supply_property prop,
				   union power_supply_propval *val)
{
	struct smb347_charger *smb =
		container_of(psy, struct smb347_charger, usb);

	if (prop == POWER_SUPPLY_PROP_ONLINE) {
		val->intval = smb->usb_online;
		return 0;
	}
	return -EINVAL;
}

static enum power_supply_property smb347_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int smb347_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb347_charger *smb =
			container_of(psy, struct smb347_charger, battery);
	const struct smb347_charger_platform_data *pdata = smb->pdata;
	int ret;

	ret = smb347_update_status(smb);
	if (ret < 0)
		return ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!smb347_is_online(smb)) {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		}
		if (smb347_charging_status(smb))
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_FULL;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!smb347_is_online(smb))
			return -ENODATA;

		/*
		 * We handle trickle and pre-charging the same, and taper
		 * and none the same.
		 */
		switch (smb347_charging_status(smb)) {
		case 1:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case 2:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		}
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = pdata->battery_info.technology;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = pdata->battery_info.voltage_min_design;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = pdata->battery_info.voltage_max_design;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (!smb347_is_online(smb))
			return -ENODATA;
		ret = smb347_read(smb, STAT_A);
		if (ret < 0)
			return ret;

		ret &= STAT_A_FLOAT_VOLTAGE_MASK;
		if (ret > 0x3d)
			ret = 0x3d;

		val->intval = 3500000 + ret * 20000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (!smb347_is_online(smb))
			return -ENODATA;

		ret = smb347_read(smb, STAT_B);
		if (ret < 0)
			return ret;

		/*
		 * The current value is composition of FCC and PCC values
		 * and we can detect which table to use from bit 5.
		 */
		if (ret & 0x20) {
			val->intval = hw_to_current(fcc_tbl,
						    ARRAY_SIZE(fcc_tbl),
						    ret & 7);
		} else {
			ret >>= 3;
			val->intval = hw_to_current(pcc_tbl,
						    ARRAY_SIZE(pcc_tbl),
						    ret & 7);
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = pdata->battery_info.charge_full_design;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = pdata->battery_info.name;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property smb347_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_MODEL_NAME,
};

static int smb347_debugfs_show(struct seq_file *s, void *data)
{
	struct smb347_charger *smb = s->private;
	int ret;
	u8 reg;

	seq_printf(s, "Control registers:\n");
	seq_printf(s, "==================\n");
	for (reg = CFG_CHARGE_CURRENT; reg <= CFG_ADDRESS; reg++) {
		ret = smb347_read(smb, reg);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, ret);
	}
	seq_printf(s, "\n");

	seq_printf(s, "Command registers:\n");
	seq_printf(s, "==================\n");
	ret = smb347_read(smb, CMD_A);
	seq_printf(s, "0x%02x:\t0x%02x\n", CMD_A, ret);
	ret = smb347_read(smb, CMD_B);
	seq_printf(s, "0x%02x:\t0x%02x\n", CMD_B, ret);
	ret = smb347_read(smb, CMD_C);
	seq_printf(s, "0x%02x:\t0x%02x\n", CMD_C, ret);
	seq_printf(s, "\n");

	seq_printf(s, "Interrupt status registers:\n");
	seq_printf(s, "===========================\n");
	for (reg = IRQSTAT_A; reg <= IRQSTAT_F; reg++) {
		ret = smb347_read(smb, reg);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, ret);
	}
	seq_printf(s, "\n");

	seq_printf(s, "Status registers:\n");
	seq_printf(s, "=================\n");
	for (reg = STAT_A; reg <= STAT_E; reg++) {
		ret = smb347_read(smb, reg);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, ret);
	}

	return 0;
}

static int smb347_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, smb347_debugfs_show, inode->i_private);
}

static const struct file_operations smb347_debugfs_fops = {
	.open		= smb347_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* FUJITSU:2012-11-20 FJ_CHARGER start */
#ifdef FJ_CHARGER
int fj_smb_chg_queue_check(void)
{
	int quenum = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/* queue check */
	spin_lock(&fj_smb_chg_list_lock);
	if (!list_empty(&fj_smb_chg_list_head)) {
		quenum++;
	}
	spin_unlock(&fj_smb_chg_list_lock);
	FJ_CHARGER_DBGLOG("[%s] quenum = %d \n",__func__, quenum);

	return quenum;
}

void fj_smb_chg_push_queue(struct work_struct *work)
{
	struct fj_smb_chg_queue_t *chg_q;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	chg_q = kmalloc(sizeof(struct fj_smb_chg_queue_t), GFP_KERNEL);
	if (chg_q != NULL) {
		chg_q->work = work;
	} else {
		FJ_CHARGER_ERRLOG("[%s] cant get memory\n",__func__);
		return;
	}

	spin_lock(&fj_smb_chg_list_lock);
	list_add_tail(&chg_q->list, &fj_smb_chg_list_head);
	spin_unlock(&fj_smb_chg_list_lock);

	return;
}

struct work_struct * fj_smb_chg_pop_queue(void)
{
	struct work_struct *work = NULL;
	struct fj_smb_chg_queue_t *chg_q;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	spin_lock(&fj_smb_chg_list_lock);
	if (!list_empty(&fj_smb_chg_list_head)) {
		chg_q = list_entry(fj_smb_chg_list_head.next, struct fj_smb_chg_queue_t, list);
		work = chg_q->work;
		list_del(fj_smb_chg_list_head.next);
	}
	spin_unlock(&fj_smb_chg_list_lock);
	if (work != NULL) {
		kfree(chg_q);
	}
	FJ_CHARGER_DBGLOG("[%s] work = %p \n",__func__, work);

	return work;
}

static void fj_smb_chg_wakelock(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	wake_lock(&smb->charging_wake_lock);
}

static void fj_smb_chg_wakeunlock(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	wake_unlock(&smb->charging_wake_lock);
}

static void fj_smb_chg_param_chk_wakelock(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	wake_lock(&smb->param_chk_wake_lock);
}

static void fj_smb_chg_param_chk_wakeunlock(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	wake_unlock(&smb->param_chk_wake_lock);
}

static void fj_smb_chg_wakeup_wakelock(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	wake_lock_timeout(&smb->charging_wakeup_lock, HZ);
}

static enum power_supply_property fj_smb_chg_mains_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static int fj_smb_chg_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		spin_lock(&fj_smb_chg_info_lock);
		if (fj_smb_charger_src & FJ_CHG_SOURCE_HOLDER) {
			if ( (psy->type == POWER_SUPPLY_TYPE_MAINS) ||
				 (psy->type == POWER_SUPPLY_TYPE_USB)   ||
				 (psy->type == POWER_SUPPLY_TYPE_MHL)     ) {
					val->intval = 0;
			}
		} else {
			if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
				val->intval = ((fj_smb_charger_src & FJ_CHG_SOURCE_AC) ? 1 : 0);
			}
			if (psy->type == POWER_SUPPLY_TYPE_USB) {
				val->intval = ((fj_smb_charger_src & FJ_CHG_SOURCE_USB) ? 1 : 0);
			}
			if (psy->type == POWER_SUPPLY_TYPE_MHL) {
				val->intval = ((fj_smb_charger_src & FJ_CHG_SOURCE_MHL) ? 1 : 0);
			}
		}
		if (psy->type == POWER_SUPPLY_TYPE_HOLDER) {
			val->intval = ((fj_smb_charger_src & FJ_CHG_SOURCE_HOLDER) ? 1 : 0);
		}
		spin_unlock(&fj_smb_chg_info_lock);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fj_smb_chg_is_suspending(struct smb347_charger *smb)
{
	int ret = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (test_bit(CHG_SUSPENDING, &smb->suspend_flag)) {
		FJ_CHARGER_DBGLOG("[%s] detect suspending...\n", __func__);
		ret = 1;
	}
	return ret;
}

static void fj_smb_chg_power_supply_update_all(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (smb != 0) {
		FJ_CHARGER_REC_LOG("[%s] update online chg_src=0x%04x\n",
								__func__, (unsigned int)fj_smb_charger_src);
		power_supply_changed(&smb->usb);
		power_supply_changed(&smb->mains);
		power_supply_changed(&smb->mhl);
		power_supply_changed(&smb->holder);
	}
}

static void fj_smb_chg_start_monitor(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	schedule_delayed_work(&smb->charge_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(CHARGE_CHECK_PERIOD_MS)));
}
static void fj_smb_chg_stop_monitor(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	cancel_delayed_work(&smb->charge_monitor_work);
}

static void fj_smb_chg_start_safetimer(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	schedule_delayed_work(&smb->safety_charge_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(SAFETY_CHARGE_CHECK_PERIOD_MS)));
}

static void fj_smb_chg_stop_safetimer(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	cancel_delayed_work(&smb->safety_charge_monitor_work);
}

static void fj_smb_chg_start_aicl_restart_timer(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	schedule_delayed_work(&smb->aicl_restart_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(AICL_RESTART_CHECK_PERIOD_MS)));
}

static void fj_smb_chg_stop_aicl_restart_timer(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	cancel_delayed_work(&smb->aicl_restart_monitor_work);
}

static void fj_smb_chg_start_fullcharge1timer(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_smb_chg_full_check_state = FJ_CHG_FULL_CHECK1;
	schedule_delayed_work(&smb->full_charge_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(FULL_CHARGE_CHECK1_PERIOD_MS)));
}

static void fj_smb_chg_start_fullcharge2timer(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_smb_chg_full_check_state = FJ_CHG_FULL_CHECK2;
	schedule_delayed_work(&smb->full_charge_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(FULL_CHARGE_CHECK2_PERIOD_MS)));
}

static void fj_smb_chg_stop_fullchargetimer(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (fj_smb_chg_full_check_factor & FJ_CHG_FULL_CHECK_FACT_T2) {
		cancel_delayed_work(&smb->full_charge_monitor_work);
	}
}

static void fj_smb_chg_current_check(struct smb347_charger *smb, unsigned int *tmp_current)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_smb_chg_old_limit = smb->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK;
	if (fj_smb_chg_old_limit) {
		if (smb->charger_info.chg_current > FJ_CHG_LIMIT_CURRENT_900) {
			*tmp_current = FJ_CHG_LIMIT_CURRENT_900;
		} else {
			*tmp_current = FJ_CHG_LIMIT_CURRENT_500;
		}
	} else {
		*tmp_current = smb->charger_info.chg_current;
	}

	spin_lock(&fj_smb_chg_info_lock);
	/* a countermeasure against overheat */
	if ((fj_smb_chg_curr_limit == FJ_CHG_CURRENT_LIMITED_900) &&
		(*tmp_current > FJ_CHG_LIMIT_CURRENT_900)) {
		*tmp_current = FJ_CHG_LIMIT_CURRENT_900;
	} else if ((fj_smb_chg_curr_limit == FJ_CHG_CURRENT_LIMITED_500) &&
		(*tmp_current > FJ_CHG_LIMIT_CURRENT_500)) {
		*tmp_current = FJ_CHG_LIMIT_CURRENT_500;
	} else if ((fj_smb_chg_curr_limit == FJ_CHG_CURRENT_LIMITED_300) &&
		(*tmp_current > FJ_CHG_LIMIT_CURRENT_300)) {
		*tmp_current = FJ_CHG_LIMIT_CURRENT_300;
	}
	fj_smb_chg_change_curr_limit = FJ_CHG_NOT_CHANGE_CL;
	spin_unlock(&fj_smb_chg_info_lock);
}

static void fj_smb_chg_batt_current_check(int *current_check)
{
	int batt_curr = 0;
	int batt_curr_ave = 0;
	int result_ua = 0;
	int i = 0;
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV005)) ||
		((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV014))) {
		for (i = 0; i < 4; i++) {
			ret = pm8xxx_ccadc_get_battery_current(&result_ua);
			if (!ret) {
				if (!(result_ua >= -100000) && (result_ua <= 0)) {
					*current_check = 0;
					break;
				} else {
					*current_check = 1;
				}
			} else {
				FJ_CHARGER_ERRLOG("%s get battery current failed(%d)\n", __func__, ret);
				*current_check = 0;
				break;
			}
			msleep(20);
		}
	} else {
		ret = fj_battery_get_battery_curr(&batt_curr);
		if (ret) {
			FJ_CHARGER_DBGLOG("[%s] Battery current get NG\n",__func__);
		}

		ret = fj_battery_get_battery_ave(&batt_curr_ave);
		if (ret) {
			FJ_CHARGER_DBGLOG("[%s] Battery current average get NG\n",__func__);
		}

		if ((batt_curr     <= FJ_CHG_BATT_CURR_CHECK) &&
			(batt_curr_ave <= FJ_CHG_BATT_CURR_CHECK)) {
			*current_check = 1;
		} else {
			*current_check = 0;
		}
	}
}

static void fj_smb_chg_start_charge(struct smb347_charger *smb)
{
	unsigned int tmp_current = 0;
	int temp_age = 0;
	int i = 0;
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] mode = %d chg_current = %d\n", __func__, smb->charge_mode, smb->charger_info.chg_current);
	/* Charge Start */
	if (smb->charge_mode == CHARGE_MODE_MANUAL) {
		fj_smb_chg_current_check(smb, &tmp_current);
		FJ_CHARGER_INFOLOG("[%s] start current = %d\n",__func__, tmp_current);

		if (((system_rev & 0xF0) != (FJ_CHG_DEVICE_TYPE_FJDEV005)) &&
			((system_rev & 0xF0) != (FJ_CHG_DEVICE_TYPE_FJDEV014))) {
			result = fj_battery_get_battery_age(&temp_age);
			if (result != 0) {
				FJ_CHARGER_ERRLOG("[%s] AGE reg get error %d\n", __func__, result);
				temp_age = 100;
			}

			for (i = 0;i < FJ_SMB_CHG_FULL_CHK_PARAM_MAX;i++ ) {
				if ((fj_smb_chg_full_chk_param_table[i].age_upper >= temp_age) &&
					(fj_smb_chg_full_chk_param_table[i].age_lower <= temp_age)) {
					break;
				}
			}
			if (i == FJ_SMB_CHG_FULL_CHK_PARAM_MAX) {
				i = 0;
			}

			smb->vmax_age_adj = fj_smb_chg_full_chk_param_table[i].vmax_adj;
			smb->full_chg_percent = fj_smb_chg_full_chk_param_table[i].fullsoc_threshold;
		} else {
			smb->full_chg_percent = FJ_CHG_VALUE_FULL_SOC_LV2;
		}

		fj_smb_chg_hw_charge_start(tmp_current);
		if (((system_rev & 0xF0) != (FJ_CHG_DEVICE_TYPE_FJDEV005)) &&
			((system_rev & 0xF0) != (FJ_CHG_DEVICE_TYPE_FJDEV014))) {
		result = fj_battery_set_battery_fullsocthr((unsigned int)smb->full_chg_percent);
		if (result != 0) {
			FJ_CHARGER_ERRLOG("[%s] FULLSOCTHR set error %d\n", __func__, result);
			}
		}

	} else {
		/* not charging */
		FJ_CHARGER_ERRLOG("[%s] invalied mode %d\n", __func__, smb->charge_mode);
	}
}

static void fj_smb_chg_start_recharge(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_smb_chg_start_charge(smb);
}

static void fj_smb_chg_stop_charge(struct smb347_charger *smb)
{
	FJ_CHARGER_DBGLOG("[%s] mode = %d chg_current = %d\n", __func__, smb->charge_mode, smb->charger_info.chg_current);

	/* Charge stop process */
	if (smb->charge_mode == CHARGE_MODE_MANUAL) {
		fj_smb_chg_hw_charge_stop(fj_smb_charger_src);
	} else {
		/* not charging */
		FJ_CHARGER_ERRLOG("[%s] invalied mode %d\n", __func__, smb->charge_mode);
	}
}

static void fj_smb_chg_start_powerpath(struct smb347_charger *smb)
{
	unsigned int tmp_current = 0;

	FJ_CHARGER_DBGLOG("[%s] mode = %d chg_current = %d\n", __func__, smb->charge_mode, smb->charger_info.chg_current);

	/* Power path process */
	if (smb->charge_mode == CHARGE_MODE_MANUAL) {
		fj_smb_chg_current_check(smb, &tmp_current);
		FJ_CHARGER_INFOLOG("[%s] start current = %d\n",__func__, tmp_current);
		fj_smb_chg_hw_charge_powerpath(tmp_current);
	} else {
		/* not charging */
		FJ_CHARGER_ERRLOG("[%s] invalied mode %d\n", __func__, smb->charge_mode);
	}
}

static void fj_smb_chg_get_param(struct fj_smb_charger_param *charger_param)
{
	struct pm8xxx_adc_chan_result result_adp = {0};
	int result_vbat = 0;
	int batt_temp = 0;
	int mobile_temp = 0;
	int receiver_temp = 0;
	int chg_ic_temp = 0;
	int board_temp = 0;
	int sensor_temp = 0;
	int soc = 0;
	int soc_raw = 0;
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/* Supply Voltage */
	ret = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_3, ADC_MPP_2_AMUX6, &result_adp);
	if (ret) {
		FJ_CHARGER_DBGLOG("[%s] ADC Supply voltage value raw:%x physical:%lld\n",__func__, result_adp.adc_code, result_adp.physical);
	}
	charger_param->supply_volt = result_adp.adc_code;

	/* Battery Voltage */
	ret = fj_battery_get_battery_volt(&result_vbat);
	if (ret) {
		FJ_CHARGER_DBGLOG("[%s] VBAT value raw:0x%x\n",__func__, result_vbat);
	}
	charger_param->batt_volt = result_vbat;

	/* Battery Temp */
	ret = fj_battery_get_battery_temp(&batt_temp);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get BATT_TEMP error %d\n", __func__, ret);
	}
	charger_param->batt_temp = batt_temp;

	/* Mobile Temp */
	ret = fj_battery_get_terminal_temp(&mobile_temp);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get MOBILE_TEMP error %d\n", __func__, ret);
	}
	charger_param->mobile_temp = mobile_temp;

	/* Receiver Temp */
	ret = fj_battery_get_receiver_temp(&receiver_temp);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get RECEIVER_TEMP error %d\n", __func__, ret);
	}
	charger_param->receiver_temp = receiver_temp;

	/* Charge-IC Temp */
	ret = fj_battery_get_charge_temp(&chg_ic_temp);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get CHARGE_IC_TEMP error %d\n", __func__, ret);
	}
	charger_param->chg_ic_temp = chg_ic_temp;

	/* Board Temp */
	ret = fj_battery_get_center_temp(&board_temp);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get BOARD_TEMP error %d\n", __func__, ret);
	}
	charger_param->board_temp = board_temp;

	/* Sensor Temp */
	ret = fj_battery_get_sensor_temp(&sensor_temp);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get SENSOR_TEMP error %d\n", __func__, ret);
	}
	charger_param->sensor_temp = sensor_temp;

	/* Status of Charge */
	ret = fj_battery_get_battery_capa(&soc);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get BATT_CAPACITY error %d\n", __func__, ret);
	}
	charger_param->soc = soc;

	ret = fj_battery_get_battery_capa_vf(&soc_raw);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get BATT_CAPACITY error %d\n", __func__, ret);
	}
	charger_param->soc_raw = soc_raw;

	/* Charge IC Error */
	ret = fj_smb_chg_hw_get_charge_ic_error(&fj_smb_chg_ic_error, fj_smb_charger_src);

	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] get CHG_IC_ERROR  error %d\n", __func__, ret);
	} else {
		FJ_CHARGER_DBGLOG("[%s] fj_smb_chg_ic_error=%x\n", __func__, fj_smb_chg_ic_error);
		if (fj_smb_chg_ic_error == FJ_SMB_CHG_IC_ERROR_BIT_RESTART) {
			FJ_CHARGER_INFOLOG("[%s] CHG Restart\n", __func__);
		    FJ_CHARGER_REC_LOG("[%s] CHG Restart\n", __func__);
			if ((the_smb->charger_info.state == FJ_CHG_STATE_CHARGING) ||
				(the_smb->charger_info.state == FJ_CHG_STATE_FULL_CHECKING)) {
				fj_smb_chg_start_charge(the_smb);
			} else if (the_smb->charger_info.state == FJ_CHG_STATE_FULL) {
				fj_smb_chg_start_powerpath(the_smb);
			}
			fj_smb_chg_ic_error &= ~(FJ_SMB_CHG_IC_ERROR_BIT_RESTART);
		}
		charger_param->chg_ic_err = fj_smb_chg_ic_error;
	}
}

static void fj_smb_chg_get_aicl_param(struct fj_smb_charger_param *charger_param)
{
	struct pm8xxx_adc_chan_result result_adp = {0};
	int ret = 0;
	int i = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/* AICL */
	ret = fj_smb_chg_hw_aicl_check();
	FJ_CHARGER_DBGLOG("[%s] fj_smb_chg_hw_aicl_check() ret=%d\n", __func__, ret);

	if (fj_smb_charger_src & FJ_CHG_SOURCE_USB_PORT) {
		fj_smb_chg_aicl_restart = 1;
	} else {
		if (ret == FJ_SMB_CHG_IC_AICL_BIG) {
			for (i = 0; i < 10; i++) {
				ret = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_3, ADC_MPP_2_AMUX6, &result_adp);
				if (ret) {
					FJ_CHARGER_DBGLOG("[%s] ADC Supply voltage value raw:%x physical:%lld\n",__func__, result_adp.adc_code, result_adp.physical);
					break;
				}

				if (result_adp.adc_code >= FJ_CHG_VALUE_ADP_AICL_CHK_V) {
					fj_smb_chg_aicl_restart = 1;
					break;
				}
				msleep(25);
			}
			if (i == 10) {
				FJ_CHARGER_INFOLOG("[%s] AICL ERROR detect\n", __func__);
				fj_smb_chg_aicl_restart = 0;
				charger_param->aicl = 1;
			}
		} else if (ret == FJ_SMB_CHG_IC_AICL_SMALL) {
			fj_smb_chg_aicl_restart = 1;
		}
	}

	if (fj_smb_chg_aicl_restart) {
		ret = fj_smb_chg_hw_aicl_restart_check();
		if (!ret) {
			fj_smb_chg_aicl_restart = 0;
		}
	}
}

static void fj_smb_chg_aicl_error_recovery_check(struct fj_smb_charger_param *charger_param)
{
	struct pm8xxx_adc_chan_result result_adp = {0};
	int ret = 0;
	int i = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (!(the_smb->charger_info.current_err & FJ_CHG_PARAM_AICL_ERROR)) {
		fj_smb_chg_aicl_restart = 0;
	} else {
		charger_param->aicl = 1;
		if (!fj_smb_chg_aicl_error_recovery_check_flag) {
			fj_smb_chg_aicl_error_recovery_check_flag = 1;
			return;
		} else {
			fj_smb_chg_aicl_error_recovery_check_flag = 0;
		}

		ret = fj_smb_chg_hw_aicl_error_check_start();
		if (ret) {
			return;
		}
		fj_smb_chg_aicl_restart = 1;
		msleep(200);

		for (i = 0; i < 10; i++) {
			ret = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_3, ADC_MPP_2_AMUX6, &result_adp);
			if (ret) {
				FJ_CHARGER_DBGLOG("[%s] ADC Supply voltage value raw:%x physical:%lld\n",__func__, result_adp.adc_code, result_adp.physical);
				break;
			}

			if (result_adp.adc_code >= FJ_CHG_VALUE_ADP_AICL_CHK_V) {
				break;
			}
			msleep(25);
		}
		if (i != 10) {
			charger_param->aicl = 0;
		}
	}
}

static void fj_smb_chg_check_params(struct fj_smb_charger_param *param, struct smb347_charger *smb)
{
	int i;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (param->chg_ic_err != 0) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_CHG_IC_ERROR\n", __func__);
		smb->charger_info.current_err |= FJ_CHG_PARAM_CHG_IC_ERROR;
		for (i = FJ_SMB_CHG_IC_ERROR_DCIN_OV; i < FJ_SMB_CHG_IC_ERROR_MAX; i++) {
			if (param->chg_ic_err & (1 << i)) {
				if (fj_smb_chg_ic_error_cnt[i] < FJ_CHG_IC_ERR_DETECT_CNT) {
					fj_smb_chg_ic_error_cnt[i]++;
				}
			} else {
				fj_smb_chg_ic_error_cnt[i] = 0;
			}
		}
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_CHG_IC_ERROR);
		for (i = FJ_SMB_CHG_IC_ERROR_DCIN_OV; i < FJ_SMB_CHG_IC_ERROR_MAX; i++) {
			fj_smb_chg_ic_error_cnt[i] = 0;
		}
	}

	/* Supply voltage check */
	if ((param->supply_volt >= FJ_CHG_VALUE_ADP_VOLTAGE_OV) ||
		(fj_smb_chg_ic_error_cnt[FJ_SMB_CHG_IC_ERROR_DCIN_OV] >= FJ_CHG_IC_ERR_DETECT_CNT) ||
		(fj_smb_chg_ic_error_cnt[FJ_SMB_CHG_IC_ERROR_USBIN_OV] >= FJ_CHG_IC_ERR_DETECT_CNT)) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_ADP_VOLTAGE_OV(0x%08x)\n", __func__, param->supply_volt);

		smb->charger_info.current_err |= FJ_CHG_PARAM_ADP_VOLTAGE_OV;
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_ADP_VOLTAGE_OV);
	}

	if ((param->supply_volt < FJ_CHG_VALUE_ADP_VOLTAGE_UV) ||
		(fj_smb_chg_ic_error_cnt[FJ_SMB_CHG_IC_ERROR_DCIN_UV] >= FJ_CHG_IC_ERR_DETECT_CNT) ||
		(fj_smb_chg_ic_error_cnt[FJ_SMB_CHG_IC_ERROR_USBIN_UV] >= FJ_CHG_IC_ERR_DETECT_CNT)) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_ADP_VOLTAGE_UV(0x%08x)\n", __func__, param->supply_volt);

		smb->charger_info.current_err |= FJ_CHG_PARAM_ADP_VOLTAGE_UV;
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_ADP_VOLTAGE_UV);
	}

	/* Battery voltage check */
	if (param->batt_volt >= FJ_CHG_VALUE_BATT_VOLTAGE_OV) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_BATT_VOLTAGE_OV(0x%08x)\n", __func__, param->batt_volt);

		smb->charger_info.current_err |= FJ_CHG_PARAM_BATT_VOLTAGE_OV;
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_VOLTAGE_OV);
	}

	/* Battery temp check */
	if ((param->batt_temp >=  FJ_CHG_VALUE_BATT_TEMP_HOT) ||
		(fj_smb_chg_ic_error_cnt[FJ_SMB_CHG_IC_ERROR_TEMP_LIMIT] >= FJ_CHG_IC_ERR_DETECT_CNT)) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_BATT_TEMP_HOT(%d)\n", __func__, param->batt_temp);
		
		smb->charger_info.current_err |= FJ_CHG_PARAM_BATT_TEMP_HOT;
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_WARM |
											FJ_CHG_PARAM_BATT_TEMP_COOL |
											FJ_CHG_PARAM_BATT_TEMP_COLD);
	} else if ((param->batt_temp <  FJ_CHG_VALUE_BATT_TEMP_HOT) &&
				(param->batt_temp >  FJ_CHG_VALUE_BATT_TEMP_WARM)) {
		FJ_CHARGER_WARNLOG("[%s] FJ_CHG_VALUE_BATT_TEMP_WARM(%d)\n", __func__, param->batt_temp);
					
		smb->charger_info.current_err |= FJ_CHG_PARAM_BATT_TEMP_WARM;
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_HOT |
											FJ_CHG_PARAM_BATT_TEMP_COOL |
											FJ_CHG_PARAM_BATT_TEMP_COLD);
	} else if ((param->batt_temp <=  FJ_CHG_VALUE_BATT_TEMP_COOL) &&
				(param->batt_temp >  FJ_CHG_VALUE_BATT_TEMP_COLD)) {
		FJ_CHARGER_WARNLOG("[%s] FJ_CHG_VALUE_BATT_TEMP_COOL(%d)\n", __func__, param->batt_temp);

		smb->charger_info.current_err |= FJ_CHG_PARAM_BATT_TEMP_COOL;
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_HOT |
											FJ_CHG_PARAM_BATT_TEMP_WARM |
											FJ_CHG_PARAM_BATT_TEMP_COLD);
	} else if (param->batt_temp <= FJ_CHG_VALUE_BATT_TEMP_COLD) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_BATT_TEMP_COLD(%d)\n", __func__, param->batt_temp);

		smb->charger_info.current_err |= FJ_CHG_PARAM_BATT_TEMP_COLD;
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_HOT |
											FJ_CHG_PARAM_BATT_TEMP_WARM |
											FJ_CHG_PARAM_BATT_TEMP_COOL);
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_HOT |
											FJ_CHG_PARAM_BATT_TEMP_WARM |
											FJ_CHG_PARAM_BATT_TEMP_COOL |
											FJ_CHG_PARAM_BATT_TEMP_COLD);
	}

	if (fj_smb_chg_err_state != FJ_CHG_ERROR_NONE) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_OVP_STATE_ERROR\n", __func__);

		smb->charger_info.current_err |= FJ_CHG_PARAM_OVP_STATE_ERROR;
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_OVP_STATE_ERROR);
	}

	if (fj_smb_chg_enable != FJ_CHG_ENABLE) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_CHG_DISABLE\n", __func__);

		smb->charger_info.current_err |= FJ_CHG_PARAM_CHG_DISABLE;
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_CHG_DISABLE);
	}

	if (fj_smb_chg_curr_limit) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_LIMITED_CURR\n", __func__);

		smb->charger_info.current_err |= FJ_CHG_PARAM_LIMITED_CURR;
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_LIMITED_CURR);
	}

	if (!fj_battery_get_initialized_flag()) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_FGIC_NOT_INITIALIZE\n", __func__);

		smb->charger_info.current_err |= FJ_CHG_PARAM_FGIC_NOT_INITIALIZE;
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_FGIC_NOT_INITIALIZE);
	}

	if (param->aicl) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_AICL_ERROR\n", __func__);

		smb->charger_info.current_err |= FJ_CHG_PARAM_AICL_ERROR;
	} else {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_AICL_ERROR);
	}

	if (smb->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) {
		smb->charger_info.current_err &= ~(FJ_CHG_PARAM_CHG_IC_ERROR);
	}

}

static void fj_smb_chg_err_stateupdate(struct smb347_charger *smb)
{
	static unsigned int tmp_updated = 0;
	unsigned int up_bit = smb->charger_info.current_err;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/*
	 * priority
	 *     HIGH: SUPPLY VOLTAGE HIGH
	 *           OVER VOLTAGE
	 *     LOW:  SUPPLY VOLTAGE LOW
	 */
	if (up_bit & FJ_CHG_PARAM_UNSPEC_FAILURE) {
		if (!(tmp_updated & FJ_CHG_PARAM_UNSPEC_FAILURE)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
			tmp_updated = FJ_CHG_PARAM_UNSPEC_FAILURE;
		}
	} else if (up_bit & FJ_CHG_PARAM_OVP_STATE_ERROR) {
		if (!(tmp_updated & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_OVER_SUPPLY_VOLTAGE);
			tmp_updated = FJ_CHG_PARAM_OVP_STATE_ERROR;
		}
	} else if (up_bit & FJ_CHG_PARAM_ADP_VOLTAGE_OV) {
		if (!(tmp_updated & FJ_CHG_PARAM_ADP_VOLTAGE_OV)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_OVER_SUPPLY_VOLTAGE);
			tmp_updated = FJ_CHG_PARAM_ADP_VOLTAGE_OV;
		}
	} else if (up_bit & FJ_CHG_PARAM_BATT_VOLTAGE_OV) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_VOLTAGE_OV)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_OVERVOLTAGE);
			tmp_updated = FJ_CHG_PARAM_BATT_VOLTAGE_OV;
		}
	} else if (up_bit & FJ_CHG_PARAM_ADP_VOLTAGE_UV) {
		if (!(tmp_updated & FJ_CHG_PARAM_ADP_VOLTAGE_UV)) {
		/* !! supply volt low is NOT error !!*/
		fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
			tmp_updated = FJ_CHG_PARAM_ADP_VOLTAGE_UV;
		}
	} else if (up_bit & FJ_CHG_PARAM_AICL_ERROR) {
		if (!(tmp_updated & FJ_CHG_PARAM_AICL_ERROR)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_HOLDER_ERROR);
			tmp_updated = FJ_CHG_PARAM_AICL_ERROR;
		}
	} else if (up_bit & FJ_CHG_PARAM_BATT_TEMP_HOT) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_TEMP_HOT)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
			tmp_updated = FJ_CHG_PARAM_BATT_TEMP_HOT;
		}
	} else if (up_bit & FJ_CHG_PARAM_BATT_TEMP_WARM) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_TEMP_WARM)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
			tmp_updated = FJ_CHG_PARAM_BATT_TEMP_WARM;
		}
	} else if (up_bit & FJ_CHG_PARAM_BATT_TEMP_COLD) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_TEMP_COLD)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
			tmp_updated = FJ_CHG_PARAM_BATT_TEMP_COLD;
		}
	} else if (up_bit & FJ_CHG_PARAM_BATT_TEMP_COOL) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_TEMP_COOL)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
			tmp_updated = FJ_CHG_PARAM_BATT_TEMP_COOL;
		}
	} else {
		fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
		tmp_updated = FJ_CHG_PARAM_NO_ERROR;
	}
}

static int fj_smb_chg_batt_soc_nearly_full(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (soc >= the_smb->full_chg_percent) {
		ret = 1;
	}
	return ret;
}

static int fj_smb_chg_batt_soc_is_full(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (soc >= FJ_CHG_VALUE_FULL_SOC_LV1) {
		ret = 1;
	}
	return ret;
}

static int fj_smb_chg_batt_recharge(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (soc <= FJ_CHG_VALUE_RECHARGE_LV1) {
		ret = 1;
 	}
	return ret;
}

static int fj_smb_chg_batt_fullcheck2chg(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (soc <= FJ_CHG_VALUE_RECHARGE_LV2) {
		ret = 1;
 	}
	return ret;
}

static int fj_smb_chg_ind_full2chg(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/* Determine whether the state of indicator should be fully charged */
	if ((soc <= FJ_CHG_VALUE_RECHARGE_LV2) && fj_smb_chg_stay_full) {
		fj_smb_chg_stay_full = 0;
		ret = 1;
	}

	return ret;
}

static fj_smb_charger_state_type fj_smb_chg_func_not_charging(struct smb347_charger *smb, fj_smb_charger_event_type event)
{
	fj_smb_charger_state_type state = FJ_CHG_STATE_NOT_CHARGING;
	struct fj_smb_charger_param param = {0};

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		if (fj_smb_chg_err_state != FJ_CHG_ERROR_NONE) {
			fj_smb_chg_param_chk_wakelock(smb);
			fj_smb_chg_get_param(&param);
			fj_smb_chg_get_aicl_param(&param);
			fj_smb_chg_check_params(&param, smb);
			fj_smb_chg_err_stateupdate(smb);
			fj_smb_chg_param_chk_wakeunlock(smb);

			if (smb->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR) {
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
		}
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		if (smb->charge_mode == CHARGE_MODE_NOT_CHARGE) {
			fj_smb_chg_hw_charge_stop(fj_smb_charger_src);
		} else {
			fj_smb_chg_param_chk_wakelock(smb);
			fj_smb_chg_get_param(&param);
			fj_smb_chg_get_aicl_param(&param);
			fj_smb_chg_check_params(&param, smb);
			fj_smb_chg_err_stateupdate(smb);
			fj_smb_chg_param_chk_wakeunlock(smb);

			if (smb->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
				/* clear error status except FJ_CHG_PARAM_PAUSE_MASK */
				smb->charger_info.current_err = FJ_CHG_PARAM_PAUSE_MASK;
				fj_smb_chg_old_limit = 0;
				fj_smb_chg_err_stateupdate(smb);

				if (!fj_smb_chg_ic_error) {
					fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
				} else {
					fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
				}
				state = FJ_CHG_STATE_PAUSE;
			} else if ((smb->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) ||
						(smb->charger_info.current_err & FJ_CHG_PARAM_BATT_TEMP_WARM)) {
				if (smb->charger_info.current_err & (FJ_CHG_PARAM_ERR_MASK & ~(FJ_CHG_PARAM_BATT_ERR_MASK))) {
					fj_smb_chg_stop_charge(smb);
				} else {
					fj_smb_chg_start_powerpath(smb);
				}

				fj_smb_chg_wakelock(smb);
				if ((smb->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
					!(smb->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
					/* ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
				} else if (((smb->charger_info.current_err & FJ_CHG_PARAM_BATT_ERR_MASK) ||
							(smb->charger_info.current_err & FJ_CHG_PARAM_BATT_TEMP_WARM)) &&
						   (!charging_mode)) {
					/* BATT TEMP */
					fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
				} else {
					/* except ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
				}
				state = FJ_CHG_STATE_ERROR;
			} else {
				fj_smb_chg_wakelock(smb);
				fj_smb_chg_start_charge(smb);
				fj_smb_chg_start_safetimer(smb);

				fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
				state = FJ_CHG_STATE_CHARGING;
			}
			fj_smb_chg_start_monitor(smb);
		}
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		smb->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_smb_chg_old_limit = 0;
		fj_smb_chg_aicl_restart = 0;
		fj_smb_chg_aicl_expire = 0;
		fj_smb_chg_ic_error = 0;
		memset(fj_smb_chg_ic_error_cnt, 0, sizeof(fj_smb_chg_ic_error_cnt));
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_smb_chg_err_stateupdate(smb);
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

static fj_smb_charger_state_type fj_smb_chg_func_charging(struct smb347_charger *smb, fj_smb_charger_event_type event)
{
	fj_smb_charger_state_type state = FJ_CHG_STATE_CHARGING;
	struct fj_smb_charger_param param = {0};
	int result_vbat = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_smb_chg_param_chk_wakelock(smb);
		fj_smb_chg_get_param(&param);
		fj_smb_chg_get_aicl_param(&param);
		fj_smb_chg_check_params(&param, smb);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_param_chk_wakeunlock(smb);

		if ((fj_smb_chg_aicl_restart) &&
			(fj_smb_chg_aicl_expire)) {
			FJ_CHARGER_INFOLOG("[%s] AICL Restart\n", __func__);
			fj_smb_chg_hw_aicl_restart();
			fj_smb_chg_start_aicl_restart_timer(smb);
			fj_smb_chg_aicl_expire = 0;
			fj_smb_chg_aicl_restart = 0;
		}

		if (smb->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
			fj_smb_chg_stop_charge(smb);
			fj_smb_chg_stop_safetimer(smb);
			fj_smb_chg_wakeunlock(smb);

			/* clear error status except FJ_CHG_PARAM_PAUSE_MASK */
			smb->charger_info.current_err = FJ_CHG_PARAM_PAUSE_MASK;
			fj_smb_chg_old_limit = 0;
			fj_smb_chg_err_stateupdate(smb);

			if (!fj_smb_chg_ic_error) {
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
			state = FJ_CHG_STATE_PAUSE;
		} else if (smb->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) {
			if (smb->charger_info.current_err & (FJ_CHG_PARAM_ERR_MASK & ~(FJ_CHG_PARAM_BATT_ERR_MASK))) {
				fj_smb_chg_stop_charge(smb);
			} else {
				fj_smb_chg_start_powerpath(smb);
			}
			fj_smb_chg_stop_safetimer(smb);

			if ((smb->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
				!(smb->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
				/* ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
			} else if ((smb->charger_info.current_err & FJ_CHG_PARAM_BATT_ERR_MASK) &&
					   (!charging_mode)) {
				/* BATT TEMP */
				fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
			} else {
				/* except ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
			state = FJ_CHG_STATE_ERROR;
		} else if (fj_smb_chg_ind_full2chg(param.soc)) {
			fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
		} else if (((smb->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK) ^ fj_smb_chg_old_limit) ||
					(fj_smb_chg_change_curr_limit != FJ_CHG_NOT_CHANGE_CL)) {
			/* set limited current */
			fj_smb_chg_start_charge(smb);
		} else {
			fj_smb_chg_full_check_factor = 0;
			if (fj_smb_chg_batt_soc_nearly_full(param.soc_raw)) {
				fj_smb_chg_full_check_factor |= FJ_CHG_FULL_CHECK_FACT_T1;
				state = FJ_CHG_STATE_FULL_CHECKING;
			}
			if (fj_smb_chg_batt_soc_is_full(param.soc)) {
				fj_smb_chg_full_check_factor |= FJ_CHG_FULL_CHECK_FACT_T2;
				fj_smb_chg_start_fullcharge1timer(smb);
				state = FJ_CHG_STATE_FULL_CHECKING;
			}
		}
		fj_smb_chg_start_monitor(smb);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		fj_smb_chg_stop_charge(smb);
		fj_smb_chg_stop_safetimer(smb);
		fj_smb_chg_start_charge(smb);
		fj_smb_chg_start_safetimer(smb);
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_smb_chg_stop_charge(smb);
		fj_smb_chg_stop_safetimer(smb);
		fj_smb_chg_stop_monitor(smb);
		fj_smb_chg_stop_aicl_restart_timer(smb);

		smb->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_smb_chg_old_limit = 0;
		fj_smb_chg_aicl_restart = 0;
		fj_smb_chg_aicl_expire = 0;
		fj_smb_chg_ic_error = 0;
		memset(fj_smb_chg_ic_error_cnt, 0, sizeof(fj_smb_chg_ic_error_cnt));
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_wakeunlock(smb);
		state = FJ_CHG_STATE_NOT_CHARGING;
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		fj_smb_chg_stop_charge(smb);

		fj_battery_get_battery_volt(&result_vbat);
		if (result_vbat >= FJ_CHG_VALUE_UNSPEC_BATT_VOLTAGE) {
			fj_smb_chg_start_charge(smb);
			fj_smb_chg_start_safetimer(smb);
		} else {
			if (!charging_mode) {
				fj_smb_chg_start_powerpath(smb);
			}

			smb->charger_info.current_err |= FJ_CHG_PARAM_UNSPEC_FAILURE;
			fj_smb_chg_err_stateupdate(smb);
			fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			state = FJ_CHG_STATE_TIME_OUT;
		}
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

static fj_smb_charger_state_type fj_smb_chg_func_full_checking(struct smb347_charger *smb, fj_smb_charger_event_type event)
{
	fj_smb_charger_state_type state = FJ_CHG_STATE_FULL_CHECKING;
	struct fj_smb_charger_param param = {0};
	int current_check = 0;
	int result_vbat = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_smb_chg_param_chk_wakelock(smb);
		fj_smb_chg_get_param(&param);
		fj_smb_chg_get_aicl_param(&param);
		fj_smb_chg_check_params(&param, smb);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_param_chk_wakeunlock(smb);

		if ((fj_smb_chg_aicl_restart) &&
			(fj_smb_chg_aicl_expire)) {
			FJ_CHARGER_INFOLOG("[%s] AICL Restart\n", __func__);
			fj_smb_chg_hw_aicl_restart();
			fj_smb_chg_start_aicl_restart_timer(smb);
			fj_smb_chg_aicl_expire = 0;
			fj_smb_chg_aicl_restart = 0;
		}

		if (smb->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
			fj_smb_chg_stop_charge(smb);
			fj_smb_chg_stop_safetimer(smb);
			fj_smb_chg_stop_fullchargetimer(smb);

			fj_smb_chg_wakeunlock(smb);

			if (!fj_smb_chg_ic_error) {
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
			state = FJ_CHG_STATE_PAUSE;
		} else if (smb->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) {
			if ((smb->charger_info.current_err & (FJ_CHG_PARAM_ERR_MASK & ~(FJ_CHG_PARAM_BATT_ERR_MASK)))) {
				fj_smb_chg_stop_charge(smb);
			} else {
				fj_smb_chg_start_powerpath(smb);
			}
			fj_smb_chg_stop_safetimer(smb);
			fj_smb_chg_stop_fullchargetimer(smb);

			if ((smb->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
				!(smb->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
				/* ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
			} else if ((smb->charger_info.current_err & FJ_CHG_PARAM_BATT_ERR_MASK) &&
					   (!charging_mode)) {
				/* BATT TEMP */
				fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
			} else {
				/* except ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
			state = FJ_CHG_STATE_ERROR;
		} else if (((smb->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK) ^ fj_smb_chg_old_limit) ||
					(fj_smb_chg_change_curr_limit != FJ_CHG_NOT_CHANGE_CL)) {
			/* set limited current */
			fj_smb_chg_start_charge(smb);
		} else {
			if (fj_smb_chg_full_check_factor & FJ_CHG_FULL_CHECK_FACT_T1) {
				fj_smb_chg_batt_current_check(&current_check);
				if (current_check) {
					FJ_CHARGER_DBGLOG("[%s] battery full\n", __func__);

					/* battery current (0mA - 100mA) */
					fj_smb_chg_start_powerpath(smb);
					fj_smb_chg_stop_safetimer(smb);
					fj_smb_chg_stop_fullchargetimer(smb);

					fj_battery_set_status(POWER_SUPPLY_STATUS_FULL);
					fj_smb_chg_stay_full = 1;
					fj_smb_chg_wakeunlock(smb);
					state = FJ_CHG_STATE_FULL;
				}
			} else {
				if (fj_smb_chg_batt_soc_nearly_full(param.soc_raw)) {
					fj_smb_chg_full_check_factor |= FJ_CHG_FULL_CHECK_FACT_T1;
				}
			}

			if (((fj_smb_chg_full_check_factor & FJ_CHG_FULL_CHECK_FACT_T2) == 0) &&
				(state != FJ_CHG_STATE_FULL)) {
				if (fj_smb_chg_batt_soc_is_full(param.soc)) {
					fj_smb_chg_full_check_factor |= FJ_CHG_FULL_CHECK_FACT_T2;
					fj_smb_chg_start_fullcharge1timer(smb);
				}
			}

			if (fj_smb_chg_batt_fullcheck2chg(param.soc)) {
			fj_smb_chg_stop_fullchargetimer(smb);
			fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
			state = FJ_CHG_STATE_CHARGING;
			}
		}
		fj_smb_chg_start_monitor(smb);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		fj_smb_chg_stop_charge(smb);
		fj_smb_chg_stop_safetimer(smb);
		fj_smb_chg_start_charge(smb);
		fj_smb_chg_start_safetimer(smb);
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_smb_chg_stop_charge(smb);
		fj_smb_chg_stop_safetimer(smb);
		fj_smb_chg_stop_fullchargetimer(smb);
		fj_smb_chg_stop_monitor(smb);
		fj_smb_chg_stop_aicl_restart_timer(smb);

		smb->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_smb_chg_old_limit = 0;
		fj_smb_chg_aicl_restart = 0;
		fj_smb_chg_aicl_expire = 0;
		fj_smb_chg_ic_error = 0;
		memset(fj_smb_chg_ic_error_cnt, 0, sizeof(fj_smb_chg_ic_error_cnt));
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_wakeunlock(smb);
		state = FJ_CHG_STATE_NOT_CHARGING;
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		FJ_CHARGER_DBGLOG("[%s] additional charge\n", __func__);
		fj_battery_set_status(POWER_SUPPLY_STATUS_FULL);
		fj_smb_chg_start_fullcharge2timer(smb);
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		FJ_CHARGER_DBGLOG("[%s] battery full(full timer timeout)\n", __func__);
		fj_smb_chg_start_powerpath(smb);
		fj_smb_chg_stop_safetimer(smb);
		fj_battery_set_status(POWER_SUPPLY_STATUS_FULL);
		fj_smb_chg_stay_full = 1;
		fj_smb_chg_wakeunlock(smb);
		state = FJ_CHG_STATE_FULL;
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		fj_smb_chg_stop_charge(smb);

		fj_battery_get_battery_volt(&result_vbat);
		if (result_vbat >= FJ_CHG_VALUE_UNSPEC_BATT_VOLTAGE) {
			fj_smb_chg_start_charge(smb);
			fj_smb_chg_start_safetimer(smb);
		} else {
			if (!charging_mode) {
				fj_smb_chg_start_powerpath(smb);
			}

			smb->charger_info.current_err |= FJ_CHG_PARAM_UNSPEC_FAILURE;
			fj_smb_chg_err_stateupdate(smb);
			fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			state = FJ_CHG_STATE_TIME_OUT;
		}
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

static fj_smb_charger_state_type fj_smb_chg_func_full(struct smb347_charger *smb, fj_smb_charger_event_type event)
{
	fj_smb_charger_state_type state = FJ_CHG_STATE_FULL;
	struct fj_smb_charger_param param = {0};

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_smb_chg_param_chk_wakelock(smb);
		fj_smb_chg_get_param(&param);
		fj_smb_chg_get_aicl_param(&param);
		fj_smb_chg_check_params(&param, smb);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_param_chk_wakeunlock(smb);

		if ((fj_smb_chg_aicl_restart) &&
			(fj_smb_chg_aicl_expire)) {
			FJ_CHARGER_INFOLOG("[%s] AICL Restart\n", __func__);
			fj_smb_chg_hw_aicl_restart();
			fj_smb_chg_start_aicl_restart_timer(smb);
			fj_smb_chg_aicl_expire = 0;
			fj_smb_chg_aicl_restart = 0;
		}

		if (smb->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
			fj_smb_chg_stop_charge(smb);

			/* clear error status except FJ_CHG_PARAM_PAUSE_MASK */
			smb->charger_info.current_err = FJ_CHG_PARAM_PAUSE_MASK;
			fj_smb_chg_old_limit = 0;
			fj_smb_chg_err_stateupdate(smb);

			if (!fj_smb_chg_ic_error) {
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
			state = FJ_CHG_STATE_PAUSE;
		} else if (fj_smb_chg_batt_recharge(param.soc)) {
			if (smb->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) {
				fj_smb_chg_wakelock(smb);
			if ((smb->charger_info.current_err & (FJ_CHG_PARAM_ERR_MASK & ~(FJ_CHG_PARAM_BATT_ERR_MASK)))) {
					fj_smb_chg_stop_charge(smb);
				} else {
					fj_smb_chg_start_powerpath(smb);
				}

				if ((smb->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
					!(smb->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
					/* ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
				} else if ((smb->charger_info.current_err & FJ_CHG_PARAM_BATT_ERR_MASK) &&
						   (!charging_mode)) {
					/* BATT TEMP */
					fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
				} else {
					/* except ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
				}
				state = FJ_CHG_STATE_ERROR;
			} else {
				fj_smb_chg_wakelock(smb);
				fj_smb_chg_start_recharge(smb);
				fj_smb_chg_start_safetimer(smb);

				/* !! Notice !! */
				/* keep POWER_SUPPLY_STATUS_FULL status */
				state = FJ_CHG_STATE_CHARGING;
			}
		} else if (((smb->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK) ^ fj_smb_chg_old_limit) ||
					(fj_smb_chg_change_curr_limit != FJ_CHG_NOT_CHANGE_CL)) {
			/* set limited current */
			fj_smb_chg_start_powerpath(smb);
		} else {
			/* do nothing */
		}
		fj_smb_chg_start_monitor(smb);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_smb_chg_stop_charge(smb);
		fj_smb_chg_stop_monitor(smb);
		fj_smb_chg_stop_aicl_restart_timer(smb);

		smb->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_smb_chg_old_limit = 0;
		fj_smb_chg_aicl_restart = 0;
		fj_smb_chg_aicl_expire = 0;
		fj_smb_chg_ic_error = 0;
		memset(fj_smb_chg_ic_error_cnt, 0, sizeof(fj_smb_chg_ic_error_cnt));
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_smb_chg_err_stateupdate(smb);
		state = FJ_CHG_STATE_NOT_CHARGING;
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

static fj_smb_charger_state_type fj_smb_chg_func_error(struct smb347_charger *smb, fj_smb_charger_event_type event)
{
	fj_smb_charger_state_type state = FJ_CHG_STATE_ERROR;
	struct fj_smb_charger_param param = {0};

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_smb_chg_param_chk_wakelock(smb);
		fj_smb_chg_get_param(&param);
		fj_smb_chg_aicl_error_recovery_check(&param);
		fj_smb_chg_check_params(&param, smb);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_param_chk_wakeunlock(smb);

		if (smb->charger_info.current_err & FJ_CHG_PARAM_AICL_ERROR) {
			fj_smb_chg_stop_charge(smb);
		}

		if (smb->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
			/* clear error status except FJ_CHG_PARAM_PAUSE_MASK */
			smb->charger_info.current_err = FJ_CHG_PARAM_PAUSE_MASK;
			fj_smb_chg_old_limit = 0;
			fj_smb_chg_err_stateupdate(smb);

			fj_smb_chg_wakeunlock(smb);

			if (!fj_smb_chg_ic_error) {
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
			state = FJ_CHG_STATE_PAUSE;

		} else if (!(smb->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) &&
					!(smb->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK)) {
			if (!fj_smb_chg_aicl_restart) {
				fj_smb_chg_start_charge(smb);
			}
			fj_smb_chg_start_safetimer(smb);

			fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
			state = FJ_CHG_STATE_CHARGING;
		} else {
			if ((smb->charger_info.current_err & (FJ_CHG_PARAM_ERR_MASK & ~(FJ_CHG_PARAM_BATT_ERR_MASK)))) {
				fj_smb_chg_stop_charge(smb);
			} else {
				if (fj_smb_chg_change_curr_limit != FJ_CHG_NOT_CHANGE_CL) {
					/* set limited current */
					fj_smb_chg_start_powerpath(smb);
				}
			}

			if ((smb->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
				!(smb->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
				/* ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
			} else if (((smb->charger_info.current_err & FJ_CHG_PARAM_BATT_ERR_MASK) ||
					    (smb->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK)) &&
					   (!charging_mode)) {
				/* BATT TEMP */
				fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
			} else {
				/* except ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
		}
		fj_smb_chg_start_monitor(smb);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_smb_chg_stop_monitor(smb);
		fj_smb_chg_stop_aicl_restart_timer(smb);

		smb->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_smb_chg_old_limit = 0;
		fj_smb_chg_aicl_restart = 0;
		fj_smb_chg_aicl_expire = 0;
		fj_smb_chg_ic_error = 0;
		memset(fj_smb_chg_ic_error_cnt, 0, sizeof(fj_smb_chg_ic_error_cnt));
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_wakeunlock(smb);
		state = FJ_CHG_STATE_NOT_CHARGING;
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

static fj_smb_charger_state_type fj_smb_chg_func_pause(struct smb347_charger *smb, fj_smb_charger_event_type event)
{
	fj_smb_charger_state_type state = FJ_CHG_STATE_PAUSE;
	struct fj_smb_charger_param param = {0};

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_smb_chg_param_chk_wakelock(smb);
		fj_smb_chg_get_param(&param);
		fj_smb_chg_check_params(&param, smb);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_param_chk_wakeunlock(smb);

		if (!(smb->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK)) {
			if ((smb->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) ||
				(smb->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK)) {
				fj_smb_chg_wakelock(smb);
			if ((smb->charger_info.current_err & (FJ_CHG_PARAM_ERR_MASK & ~(FJ_CHG_PARAM_BATT_ERR_MASK)))) {
					fj_smb_chg_stop_charge(smb);
				} else {
					fj_smb_chg_start_powerpath(smb);
				}
				fj_smb_chg_stop_safetimer(smb);

				if ((smb->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
					!(smb->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
					/* ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
				} else if ((smb->charger_info.current_err & FJ_CHG_PARAM_BATT_ERR_MASK) &&
						   (!charging_mode)) {
					/* BATT TEMP */
					fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
				} else {
					/* except ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
				}
				state = FJ_CHG_STATE_ERROR;
			} else {
				fj_smb_chg_wakelock(smb);
				fj_smb_chg_start_charge(smb);
				fj_smb_chg_start_safetimer(smb);

				fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
				state = FJ_CHG_STATE_CHARGING;
			}
		}
		fj_smb_chg_start_monitor(smb);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_smb_chg_stop_charge(smb);
		fj_smb_chg_stop_monitor(smb);
		fj_smb_chg_stop_aicl_restart_timer(smb);

		smb->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_smb_chg_old_limit = 0;
		fj_smb_chg_aicl_restart = 0;
		fj_smb_chg_aicl_expire = 0;
		fj_smb_chg_ic_error = 0;
		memset(fj_smb_chg_ic_error_cnt, 0, sizeof(fj_smb_chg_ic_error_cnt));
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_smb_chg_err_stateupdate(smb);
		state = FJ_CHG_STATE_NOT_CHARGING;
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

static fj_smb_charger_state_type fj_smb_chg_func_timeout(struct smb347_charger *smb, fj_smb_charger_event_type event)
{
	fj_smb_charger_state_type state = FJ_CHG_STATE_TIME_OUT;
	struct fj_smb_charger_param param = {0};

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_smb_chg_param_chk_wakelock(smb);
		fj_smb_chg_get_param(&param);
		fj_smb_chg_check_params(&param, smb);
		fj_smb_chg_start_monitor(smb);
		fj_smb_chg_param_chk_wakeunlock(smb);

		/* notice! : There is no need to call fj_smb_chg_err_stateupdate, */
		/* because POWER_SUPPLY_HEALTH_UNSPEC_FAILURE is set already.    */
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_smb_chg_stop_monitor(smb);
		fj_smb_chg_stop_aicl_restart_timer(smb);

		smb->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_smb_chg_old_limit = 0;
		fj_smb_chg_aicl_restart = 0;
		fj_smb_chg_aicl_expire = 0;
		fj_smb_chg_ic_error = 0;
		memset(fj_smb_chg_ic_error_cnt, 0, sizeof(fj_smb_chg_ic_error_cnt));
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_smb_chg_err_stateupdate(smb);
		fj_smb_chg_wakeunlock(smb);
		state = FJ_CHG_STATE_NOT_CHARGING;
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

typedef fj_smb_charger_state_type (*fd_charge_func_type)(struct smb347_charger *smb, fj_smb_charger_event_type event);

fd_charge_func_type fj_smb_charge_state_func[] = {
	fj_smb_chg_func_not_charging,
	fj_smb_chg_func_charging,
	fj_smb_chg_func_full_checking,
	fj_smb_chg_func_full,
	fj_smb_chg_func_error,
	fj_smb_chg_func_pause,
	fj_smb_chg_func_timeout
};

static void fj_smb_chg_state_machine(struct smb347_charger *smb, fj_smb_charger_event_type event)
{
	unsigned int old_state;

	if (smb->charger_info.state >= FJ_CHG_STATE_NUM) {
		FJ_CHARGER_ERRLOG("[%s] charge_state Error:%d Fatal\n", __func__, smb->charger_info.state);
		emergency_restart();
	}

	mutex_lock(&smb->chg_lock);

	old_state = smb->charger_info.state;

	FJ_CHARGER_DBGLOG("[%s] charge_state = %d event = %d\n", __func__, smb->charger_info.state, event);
	smb->charger_info.state = fj_smb_charge_state_func[smb->charger_info.state](smb, event);

	if (old_state != smb->charger_info.state) {
		FJ_CHARGER_INFOLOG("[%s] event:%d old state:%d -> new state:%d\n", __func__, event, old_state, smb->charger_info.state);
	}
	mutex_unlock(&smb->chg_lock);
}

static void fj_smb_chg_work(struct work_struct *work)
{
	int chg_current_val = 0;
	int source;
	int old_source, new_source;
	struct smb347_charger *smb = container_of(work,
					struct smb347_charger, charge_work);

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	spin_lock(&fj_smb_chg_info_lock);
	chg_current_val = 0;
	old_source = smb->charger_info.chg_source;
	new_source = 0;

	for (source = 0; source < CHG_SOURCE_NUM; source++) {
		if (fj_smb_charger_src & (BIT(0) << source)) {
			new_source = (BIT(0) << source);
			smb->charger_info.chg_current = fj_smb_charger_src_current[source];
			chg_current_val = fj_smb_charger_src_current[source];
			break;
		}
	}

	if (!new_source) {
		smb->charger_info.chg_current = 0;
	}
	smb->charger_info.chg_source = new_source;
	spin_unlock(&fj_smb_chg_info_lock);

	FJ_CHARGER_DBGLOG("[%s] source = %d current = %d\n",
	   __func__, smb->charger_info.chg_source, smb->charger_info.chg_current);

	if (chg_current_val != 0) {
		/* charge start */
		if(new_source != old_source) {
			fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_CHARGE_START);
		}
	} else {
		if (fj_smb_chg_err_state == FJ_CHG_ERROR_NONE) {
			/* charge end */
			fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_CHARGE_STOP);
		} else {
			FJ_CHARGER_INFOLOG("[%s] Detected OVP. Not issue CHARGE_STOP event.\n", __func__);
		}
	}
	FJ_CHARGER_DBGLOG("[%s] new_source = %d old_source = %d\n",
						__func__, new_source, old_source);
	if (new_source != old_source) {
		fj_smb_chg_power_supply_update_all(smb);
	}
}

static void fj_smb_chg_err_work(struct work_struct *work)
{
	struct smb347_charger *smb = container_of(work,
					struct smb347_charger, charge_err_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, smb->charger_info.state);

	if ((fj_smb_charger_src == 0) && (fj_smb_chg_err_state == FJ_CHG_ERROR_NONE)) {
		fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_CHARGE_STOP);
	} else {
		fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_MONITOR_PARAM);
	}
}

static void fj_smb_chg_nodelay_monitor_work(struct work_struct *work)
{
	struct smb347_charger *smb = container_of(work,
					struct smb347_charger, nodelay_monitor_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, smb->charger_info.state);

	fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_MONITOR_PARAM);
}


static void fj_smb_chg_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct smb347_charger *smb = container_of(dwork,
					struct smb347_charger, charge_monitor_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, smb->charger_info.state);

	fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_MONITOR_PARAM);
}

static void fj_smb_chg_full_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct smb347_charger *smb = container_of(dwork,
					struct smb347_charger, full_charge_monitor_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, smb->charger_info.state);

	switch (fj_smb_chg_full_check_state) {
		case FJ_CHG_FULL_CHECK1:
			fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_FULL_CHECK);
			break;
		case FJ_CHG_FULL_CHECK2:
			fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_FULL_CHECK_END);
			break;
		default:
		break;
	}
}

static void fj_smb_chg_safety_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct smb347_charger *smb = container_of(dwork,
					struct smb347_charger, safety_charge_monitor_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, smb->charger_info.state);

	fj_smb_chg_state_machine(smb, FJ_CHG_EVENT_SAFE_TIMEOUT);
}

static void fj_smb_chg_aicl_restart_monitor(struct work_struct *work)
{

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_smb_chg_aicl_expire = 1;
}

static int fj_smb_chg_battery_present_check(struct smb347_charger *smb)
{
	int rc,cnt,bat_therm,bat_present;
	struct pm8xxx_adc_chan_result result;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	for (cnt = 0;cnt < 3;cnt++) {
		if (cnt != 0) {
			msleep(500);
		}

		rc = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_4, ADC_MPP_1_AMUX6, &result);
		if (rc) {
			FJ_CHARGER_ERRLOG("[%s] ADC MPP value raw:%x physical:%lld\n",
								__func__, result.adc_code, result.physical);
		}

		bat_therm = (int)result.physical / 10;

		if (bat_therm <= BATTERY_NOT_PRESENT_TEMP) {
			bat_present = 0;
			break;
		}
		else {
			bat_present = 1;
		}
	}

	return bat_present;
}

void fj_smb_chg_source_req(int source, unsigned int mA)
{
	struct smb347_charger *smb = the_smb;

	FJ_CHARGER_INFOLOG("[%s] %d mA=%d\n", __func__, source, mA);
    FJ_CHARGER_REC_LOG("[%s] %d mA=%d\n", __func__, source, mA);

	spin_lock(&fj_smb_chg_info_lock);
	if (mA != 0) {
		if (FJ_CHG_SOURCE_IS_USB_PORT(source)) {
			fj_smb_charger_src &= ~FJ_CHG_SOURCE_USB_PORT;
		}
		fj_smb_charger_src |= FJ_CHG_SOURCE(source);
		fj_smb_charger_src_current[source] = mA;
	} else {
		fj_smb_charger_src &= ~FJ_CHG_SOURCE(source);
		fj_smb_charger_src_current[source] = 0;
	}
	spin_unlock(&fj_smb_chg_info_lock);

	if (!smb) {
		FJ_CHARGER_WARNLOG("[%s] called before init\n",__func__);
		set_bit(CHG_INIT_EVENT, &fj_smb_chg_init_event);
	} else {
		mutex_lock(&smb->chg_quelock);
		if (fj_smb_chg_is_suspending(smb)) {
			fj_smb_chg_push_queue(&smb->charge_work);
		} else {
			schedule_work(&smb->charge_work);
		}
		mutex_unlock(&smb->chg_quelock);
	}
}
EXPORT_SYMBOL_GPL(fj_smb_chg_source_req);

/* for mc command */
int fj_smb_chg_get_adapter_voltage(char *buffer, struct kernel_param *kp)
{
	struct pm8xxx_adc_chan_result result_adc;
	int result;
	int rc;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	rc = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_3, ADC_MPP_2_AMUX6, &result_adc);
	if (rc) {
		FJ_CHARGER_ERRLOG("[%s] ADC MPP value raw:%x physical:%lld\n",
							__func__, result_adc.adc_code, result_adc.physical);
	}

	result = sprintf(buffer, "%d", result_adc.adc_code);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_adapter_voltage);

/* for mc command */
int fj_smb_chg_get_battery_voltage(char *buffer, struct kernel_param *kp)
{
	struct pm8xxx_adc_chan_result result_adc;
	int result;
	int rc;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	rc = pm8xxx_adc_read(CHANNEL_VBAT, &result_adc);
	if (!rc) {
		FJ_CHARGER_ERRLOG("[%s] ADC value raw:%x physical:%lld\n",
							__func__, result_adc.adc_code, result_adc.physical);
	}

	result = sprintf(buffer, "%d", result_adc.adc_code);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_battery_voltage);

int fj_smb_chg_get_battery_temperature(char *buffer, struct kernel_param *kp)
{
	struct pm8xxx_adc_chan_result result_adc;
	int result;
	int rc;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	rc = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_4, ADC_MPP_1_AMUX6, &result_adc);
	if (rc) {
		FJ_CHARGER_ERRLOG("[%s] ADC MPP value raw:%x physical:%lld\n",
							__func__, result_adc.adc_code, result_adc.physical);
	}

	result = sprintf(buffer, "%d", result_adc.adc_code);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_battery_temperature);

int fj_smb_chg_get_ambient_temperature(char *buffer, struct kernel_param *kp)
{
	struct pm8xxx_adc_chan_result result_adc;
	int result;
	int rc;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	rc = pm8xxx_adc_read(ADC_MPP_1_AMUX8, &result_adc);
	if (rc) {
		FJ_CHARGER_ERRLOG("[%s] ADC MPP value raw:%x physical:%lld\n",
							__func__, result_adc.adc_code, result_adc.physical);
	}

	result = sprintf(buffer, "%d", result_adc.adc_code);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_ambient_temperature);

int fj_smb_chg_get_receiver_temperature(char *buffer, struct kernel_param *kp)
{
	struct pm8xxx_adc_chan_result result_adc;
	int result;
	int rc;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	rc = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_7, ADC_MPP_1_AMUX7, &result_adc);
	if (rc) {
		FJ_CHARGER_ERRLOG("[%s] ADC MPP value raw:%x physical:%lld\n",
							__func__, result_adc.adc_code, result_adc.physical);
	}

	result = sprintf(buffer, "%d", result_adc.adc_code);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_receiver_temperature);

int fj_smb_chg_get_board_temperature(char *buffer, struct kernel_param *kp)
{
	struct pm8xxx_adc_chan_result result_adc;
	int result;
	int rc;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	rc = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_10, ADC_MPP_1_AMUX7, &result_adc);
	if (rc) {
		FJ_CHARGER_ERRLOG("[%s] ADC MPP value raw:%x physical:%lld\n",
							__func__, result_adc.adc_code, result_adc.physical);
	}

	result = sprintf(buffer, "%d", result_adc.adc_code);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_board_temperature);

int fj_smb_chg_get_charge_ic_temperature(char *buffer, struct kernel_param *kp)
{
	struct pm8xxx_adc_chan_result result_adc;
	int result;
	int rc;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	rc = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_9, ADC_MPP_1_AMUX7, &result_adc);
	if (rc) {
		FJ_CHARGER_ERRLOG("[%s] ADC MPP value raw:%x physical:%lld\n",
							__func__, result_adc.adc_code, result_adc.physical);
	}

	result = sprintf(buffer, "%d", result_adc.adc_code);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_charge_ic_temperature);

int fj_smb_chg_get_sensor_temperature(char *buffer, struct kernel_param *kp)
{
	struct pm8xxx_adc_chan_result result_adc;
	int result;
	int rc;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

    memset(&result_adc, 0, sizeof(result_adc));

	if ((system_rev & 0xF0) == FJ_CHG_DEVICE_TYPE_FJDEV002) {
		if ((system_rev & 0x0F) > 0x02) {
			gpio_set_value_cansleep(PM8921_GPIO_PM_TO_SYS(TEMP_FETON2_GPIO), 1);
			usleep(20);
			rc = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_10, ADC_MPP_1_AMUX7, &result_adc);
			
			gpio_set_value_cansleep(PM8921_GPIO_PM_TO_SYS(TEMP_FETON2_GPIO), 0);
		} else {
			rc = 0;
		}
	} else {
		rc = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_12, ADC_MPP_1_AMUX7, &result_adc);
	}
	if (rc) {
		FJ_CHARGER_ERRLOG("[%s] ADC MPP value raw:%x physical:%lld\n",
							__func__, result_adc.adc_code, result_adc.physical);
	}

	result = sprintf(buffer, "%d", result_adc.adc_code);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_sensor_temperature);

/* for mc command */
int fj_smb_chg_get_battery_present(char *buffer, struct kernel_param *kp)
{
	struct smb347_charger *smb = the_smb;
	int batt_present;
	int result;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	batt_present = fj_smb_chg_battery_present_check(smb);
	result = sprintf(buffer, "%d", batt_present);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_get_battery_present);

/* for mc command */
int fj_smb_chg_set_charger_enable(const char *val, struct kernel_param *kp, int charger_enable)
{
	int ret;
	struct smb347_charger *smb = the_smb;

	ret = param_set_int(val, kp);
	if (ret) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set charger_enable param to %d\n",
						__func__, charger_enable);
	if (smb) {
		fj_smb_chg_hw_set_charger_enable(charger_enable);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_set_charger_enable);

/* for mc command */
int fj_smb_chg_set_chg_voltage(const char *val, struct kernel_param *kp, int chg_voltage)
{
	int ret;
	struct smb347_charger *smb = the_smb;

	ret = param_set_int(val, kp);
	if (ret) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set chg_voltage param to %d\n",__func__, chg_voltage);
	if (smb) {
		fj_smb_chg_hw_set_voltage(chg_voltage);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_set_chg_voltage);

/* for mc command */
int fj_smb_chg_set_chg_current(const char *val, struct kernel_param *kp, int chg_current)
{
	int ret;
	struct smb347_charger *smb = the_smb;

	ret = param_set_int(val, kp);
	if (ret) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set chg_current param to %d\n",__func__, chg_current);
	if (smb) {
		fj_smb_chg_hw_set_current(chg_current);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_set_chg_current);

/* for mc command */
int fj_smb_chg_set_fj_chg_enable(const char *val, struct kernel_param *kp, int chg_enable)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set chg_enable param to %d\n",__func__, chg_enable);
	spin_lock(&fj_smb_chg_info_lock);

	switch (chg_enable) {
	case 0:
		fj_smb_chg_enable = FJ_CHG_DISABLE;
		fj_smb_chg_curr_limit = FJ_CHG_CURRENT_NOT_LIMITED;
		break;

	case 1:
		fj_smb_chg_enable = FJ_CHG_ENABLE;
		fj_smb_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_smb_chg_curr_limit = FJ_CHG_CURRENT_NOT_LIMITED;
		break;

	case 2:
		fj_smb_chg_enable = FJ_CHG_ENABLE;
		fj_smb_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_smb_chg_curr_limit = FJ_CHG_CURRENT_LIMITED_900;
		break;

	case 3:
		fj_smb_chg_enable = FJ_CHG_ENABLE;
		fj_smb_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_smb_chg_curr_limit = FJ_CHG_CURRENT_LIMITED_500;
		break;

	case 4:
		fj_smb_chg_enable = FJ_CHG_ENABLE;
		fj_smb_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_smb_chg_curr_limit = FJ_CHG_CURRENT_LIMITED_300;
		break;

	default:
		fj_smb_chg_enable = FJ_CHG_ENABLE;
		fj_smb_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_smb_chg_curr_limit = FJ_CHG_CURRENT_NOT_LIMITED;
		break;
	}

	spin_unlock(&fj_smb_chg_info_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_set_fj_chg_enable);

/* for mc command */
int fj_smb_chg_set_chg_vinmin(const char *val, struct kernel_param *kp, int chg_vinmin)
{
	int ret;
	struct smb347_charger *smb = the_smb;

	ret = param_set_int(val, kp);
	if (ret) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set chg_vinmin param to %d\n",__func__, chg_vinmin);
	if (smb) {
		fj_smb_chg_hw_set_vinmin(chg_vinmin);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_set_chg_vinmin);

void fj_smb_chg_notify_error(unsigned int err)
{
	struct smb347_charger *smb = the_smb;

	FJ_CHARGER_INFOLOG("[%s] in, err=%d\n", __func__, err);

	if (err != FJ_CHG_ERROR_NONE ) {
		fj_smb_chg_err_state = FJ_CHG_ERROR_DETECT;
	} else {
		fj_smb_chg_err_state = FJ_CHG_ERROR_NONE;
	}

	if (!smb) {
		FJ_CHARGER_WARNLOG("[%s] called before init\n",__func__);
		set_bit(CHG_INIT_ERR_EVENT, &fj_smb_chg_init_event);
	} else {
		schedule_work(&smb->charge_err_work);
	}
}
EXPORT_SYMBOL_GPL(fj_smb_chg_notify_error);

void fj_smb_chg_chgic_reset(void)
{
	struct smb347_charger *smb = the_smb;
	
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	if (smb) {
		fj_smb_chg_hw_chgic_reset();
	}
	
	return;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_chgic_reset);

void fj_smb_chg_emergency_current_off(void)
{
	struct smb347_charger *smb = the_smb;

	FJ_CHARGER_INFOLOG("[%s] -Emergency Current OFF\n", __func__);
	if (smb) {
		fj_smb_chg_hw_charge_powerpath(FJ_CHG_USB_CURRENT);
	}
}
EXPORT_SYMBOL_GPL(fj_smb_chg_emergency_current_off);

static int fj_smb_chg_get_vbat_adj(void)
{
	int vbat = 0;
	int ret;
	uint8_t	val = 0x00;

	ret = get_nonvolatile(&val, APNV_CHG_ADJ_VBAT_I, 1);
	FJ_CHARGER_DBGLOG("[%s] Call get_nonvolatile:APNV_CHG_ADJ_VBAT_I:val=%d\n",__func__, val);
	if (ret < 0) {
		FJ_CHARGER_ERRLOG("[%s] NV read err result=%d\n",__func__, ret);
		val = 0x00;
	}
	if (val < VBAT_TBL_MAX) {
		vbat = fj_vbat_tbl[val] * VBAT_ADJ_RATIO;
	} else {
		vbat = 0;
	}

	FJ_CHARGER_DBGLOG("[%s] vbat:%d\n", __func__, vbat);

	return vbat;
}

static int fj_smb_chg_get_charge_mode(void)
{
	int charge_mode = CHARGE_MODE_INVALID;
	u16 val;
	int result;
	result = get_nonvolatile((u8 *)&val, APNV_FUNC_LIMITS_I, 2);
	if (result < 0) {
		FJ_CHARGER_ERRLOG("[%s] NV read err result=%d\n",__func__, result);
		val = 0x0000;
	}

	if ((val & 0x0001) != 0) {
		charge_mode = CHARGE_MODE_NOT_CHARGE;
	} else if ( (fj_boot_mode == FJ_MODE_MAKER_MODE) || (fj_boot_mode == FJ_MODE_KERNEL_MODE) ) {
		charge_mode = CHARGE_MODE_NOT_CHARGE;
	} else {
		charge_mode = CHARGE_MODE_MANUAL;
	}
	FJ_CHARGER_DBGLOG("[%s] chg mode:%d\n", __func__, charge_mode);
	return charge_mode;
}

static int fj_smb_get_fj_mode(void)
{
	int ret = 1;

	if (FJ_CHG_DEVICE_SMB != fj_get_chg_device())
		ret = 0;

	return ret;
}

static int fj_smb_chg_param_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int fj_smb_chg_param_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t fj_smb_chg_param_read(struct file *filp, char __user *buf, size_t size, loff_t *pos)
{
	char out[512];
	unsigned int len, total_len;
	unsigned int i;
	struct fj_smb_charger_param param = {0};

	len = sprintf(&out[0], "State:%d\n", the_smb->charger_info.state);
	total_len = len;
	len = sprintf(&out[total_len], "Detect Error:0x%08x\n", the_smb->charger_info.current_err);
	total_len += len;

	len = sprintf(&out[total_len], "-------------\n");
	total_len += len;

	fj_smb_chg_get_param(&param);
	len = sprintf(&out[total_len], "Supply Volt:0x%08x\n", param.supply_volt);
	total_len += len;
	len = sprintf(&out[total_len], "Batt temp:%d\n", param.batt_temp);
	total_len += len;
	len = sprintf(&out[total_len], "Mobile temp:%d\n", param.mobile_temp);
	total_len += len;
	len = sprintf(&out[total_len], "Receiver temp:%d\n", param.receiver_temp);
	total_len += len;
	len = sprintf(&out[total_len], "Charge-IC temp:%d\n", param.chg_ic_temp);
	total_len += len;
	len = sprintf(&out[total_len], "Board temp:%d\n", param.board_temp);
	total_len += len;
	len = sprintf(&out[total_len], "Stete of charge:%d\n", param.soc);
	total_len += len;

	len = sprintf(&out[total_len], "-------------\n");
	total_len += len;

	len = sprintf(&out[total_len], "CHG Source:0x%08lx\n", fj_smb_charger_src);
	total_len += len;

	for (i = 0; i < CHG_SOURCE_NUM; i++) {
		len = sprintf(&out[total_len], "CHG[%d] %d mA\n", i, fj_smb_charger_src_current[i]);
    	total_len += len;
	}

	len = sprintf(&out[total_len], "CHG mode:%d\n", fj_boot_mode);
	total_len += len;
	if (copy_to_user(buf, &out[0], total_len)) {
		return -EFAULT;
	}

	return simple_read_from_buffer(buf, size, pos, &out[0], total_len);
}

struct file_operations fj_smb_chg_param_fops = {
	.owner = THIS_MODULE,
	.open = fj_smb_chg_param_open,
	.release = fj_smb_chg_param_close,
    .read = fj_smb_chg_param_read,
};
#endif /* FJ_CHARGER */
/* FUJITSU:2012-11-20 FJ_CHARGER end */

static void fj_smb_chg_late_resume(struct early_suspend *h)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (CHARGE_MODE_IS_FJ()) {
		fj_smb_chg_stop_aicl_restart_timer(the_smb);
		fj_smb_chg_aicl_expire = 0;
	}
}

static void fj_smb_chg_early_suspend(struct early_suspend *h)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (CHARGE_MODE_IS_FJ()) {
		fj_smb_chg_start_aicl_restart_timer(the_smb);
		fj_smb_chg_aicl_expire = 0;
	}
}
static struct early_suspend fj_chg_smb_early_suspend_handler = {
	.suspend = fj_smb_chg_early_suspend,
	.resume = fj_smb_chg_late_resume,
};

static void fj_smb_chg_shutdown(struct i2c_client *client)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (CHARGE_MODE_IS_FJ()) {
		fj_smb_chg_hw_charge_powerpath(FJ_CHG_USB_CURRENT);
	}

	return;
}

#ifdef CONFIG_PM
static int fj_smb_chg_resume(struct i2c_client *client)
{
	struct smb347_charger *smb = i2c_get_clientdata(client);

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (CHARGE_MODE_IS_FJ()) {
		schedule_work(&smb->nodelay_monitor_work);

		mutex_lock(&smb->chg_quelock);
		while (fj_smb_chg_queue_check()) {
			schedule_work(fj_smb_chg_pop_queue());
		}
		clear_bit(CHG_SUSPENDING, &smb->suspend_flag);
		mutex_unlock(&smb->chg_quelock);
	}

	return 0;
}

static int fj_smb_chg_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct smb347_charger *smb = i2c_get_clientdata(client);

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (CHARGE_MODE_IS_FJ()) {
		set_bit(CHG_SUSPENDING, &smb->suspend_flag);

		fj_smb_chg_stop_monitor(smb);
		fj_smb_chg_stop_aicl_restart_timer(smb);
		fj_smb_chg_aicl_expire = 0;

		if (fj_smb_chg_queue_check()) {
			fj_smb_chg_wakeup_wakelock(smb);
		}
	}

	return 0;
}
#endif

static int smb347_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	static char *battery[] = { "smb347-battery" };
	const struct smb347_charger_platform_data *pdata;
	struct device *dev = &client->dev;
	struct smb347_charger *smb;
	int ret;
/* FUJITSU:2012-11-20 FJ_CHARGER start */
#ifdef FJ_CHARGER
	int temp = 0;
	struct dentry *d;    /* for debugfs */
#endif /* FJ_CHARGER */
/* FUJITSU:2012-11-20 FJ_CHARGER end */

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	pdata = dev->platform_data;
	if (!pdata)
		return -EINVAL;

/* FUJITSU:2012-11-20 FJ_CHARGER start */
#ifndef FJ_CHARGER
	if (!pdata->use_mains && !pdata->use_usb)
		return -EINVAL;
#endif
/* FUJITSU:2012-11-20 FJ_CHARGER end */

	smb = devm_kzalloc(dev, sizeof(*smb), GFP_KERNEL);
	if (!smb)
		return -ENOMEM;

	i2c_set_clientdata(client, smb);

	mutex_init(&smb->lock);
	smb->client = client;
	smb->pdata = pdata;

/* FUJITSU:2012-11-20 FJ_CHARGER start */
#ifdef FJ_CHARGER
	the_smb = smb;

	smb->charge_mode = fj_smb_chg_get_charge_mode();
	if (CHARGE_MODE_IS_FJ()) {
		smb->suspend_flag = 0;
		smb->charger_info.state       = FJ_CHG_STATE_NOT_CHARGING;
		smb->charger_info.chg_source  = 0;
		smb->charger_info.chg_current = 0;
		smb->charger_info.current_err = 0;
		smb->vbat_adj = fj_smb_chg_get_vbat_adj();
		smb->vmax_age_adj = 0;
		smb->full_chg_percent = 99;
		ret = fj_smb_chg_hw_init(smb);
		if (ret) {
			pr_err("couldn't init hardware ret=%d\n", ret);
			return ret;
		}
		smb->usb.name = "usb",
		smb->usb.type = POWER_SUPPLY_TYPE_USB,
		smb->usb.get_property = fj_smb_chg_get_property;
		smb->usb.properties = fj_smb_chg_mains_properties;
		smb->usb.num_properties = ARRAY_SIZE(fj_smb_chg_mains_properties);
		smb->usb.supplied_to = battery;
		smb->usb.num_supplicants = ARRAY_SIZE(battery);

		smb->mains.name = "ac",
		smb->mains.type = POWER_SUPPLY_TYPE_MAINS,
		smb->mains.get_property = fj_smb_chg_get_property;
		smb->mains.properties = fj_smb_chg_mains_properties;
		smb->mains.num_properties = ARRAY_SIZE(fj_smb_chg_mains_properties);
		smb->mains.supplied_to = battery;
		smb->mains.num_supplicants = ARRAY_SIZE(battery);

		smb->mhl.name = "mhl",
		smb->mhl.type = POWER_SUPPLY_TYPE_MHL,
		smb->mhl.get_property = fj_smb_chg_get_property,
		smb->mhl.properties = fj_smb_chg_mains_properties,
		smb->mhl.num_properties = ARRAY_SIZE(fj_smb_chg_mains_properties),
		smb->mhl.supplied_to = battery,
		smb->mhl.num_supplicants = ARRAY_SIZE(battery),

		smb->holder.name = "holder",
		smb->holder.type = POWER_SUPPLY_TYPE_HOLDER,
		smb->holder.get_property = fj_smb_chg_get_property,
		smb->holder.properties = fj_smb_chg_mains_properties,
		smb->holder.num_properties = ARRAY_SIZE(fj_smb_chg_mains_properties),
		smb->holder.supplied_to = battery,
		smb->holder.num_supplicants = ARRAY_SIZE(battery),

		ret = power_supply_register(dev, &smb->usb);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] power_supply_register usb failed ret = %d\n",__func__, ret);
			return ret;
		}

		ret = power_supply_register(dev, &smb->mains);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] power_supply_register mains failed ret = %d\n",__func__, ret);
			power_supply_unregister(&smb->usb);
			return ret;
		}

		ret = power_supply_register(dev, &smb->mhl);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] power_supply_register mhl failed ret = %d\n",__func__, ret);
			power_supply_unregister(&smb->mains);
			power_supply_unregister(&smb->usb);
			return ret;
		}

		ret = power_supply_register(dev, &smb->holder);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] power_supply_register holder failed ret = %d\n",__func__, ret);
			power_supply_unregister(&smb->mhl);
			power_supply_unregister(&smb->mains);
			power_supply_unregister(&smb->usb);
			return ret;
		}

		wake_lock_init(&smb->charging_wake_lock, WAKE_LOCK_SUSPEND, "fj_charging");
		wake_lock_init(&smb->charging_wakeup_lock, WAKE_LOCK_SUSPEND, "fj_chg_wakeup");
		wake_lock_init(&smb->param_chk_wake_lock, WAKE_LOCK_SUSPEND, "fj_chg_param_chk");
		mutex_init(&smb->chg_lock);
		mutex_init(&smb->chg_quelock);

		INIT_WORK(&smb->charge_work, fj_smb_chg_work);
		INIT_WORK(&smb->charge_err_work, fj_smb_chg_err_work);
		INIT_WORK(&smb->nodelay_monitor_work, fj_smb_chg_nodelay_monitor_work);
		INIT_DELAYED_WORK(&smb->charge_monitor_work, fj_smb_chg_monitor);
		INIT_DELAYED_WORK(&smb->full_charge_monitor_work, fj_smb_chg_full_monitor);
		INIT_DELAYED_WORK(&smb->safety_charge_monitor_work, fj_smb_chg_safety_monitor);
		INIT_DELAYED_WORK(&smb->aicl_restart_monitor_work, fj_smb_chg_aicl_restart_monitor);

		ret = fj_battery_get_battery_temp(&temp);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] get BATT_TEMP error %d\n", __func__, ret);
		}

		ret = fj_battery_set_battery_temp(temp);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] set BATT_TEMP error %d\n", __func__, ret);
		}

		if (__test_and_clear_bit(CHG_INIT_ERR_EVENT, &fj_smb_chg_init_event)) {
			schedule_work(&smb->charge_err_work);
		}

		if (__test_and_clear_bit(CHG_INIT_EVENT, &fj_smb_chg_init_event)) {
			schedule_work(&smb->charge_work);
		}

		register_early_suspend(&fj_chg_smb_early_suspend_handler);

		/* Initialize Debug FS */
		fj_smb_chg_debug_root = debugfs_create_dir("fj_smb_chg", NULL);
		if (!fj_smb_chg_debug_root) {
			FJ_CHARGER_ERRLOG("[%s] debugfs_create_dir failed.\n", __func__);
		}

		d = debugfs_create_file("params", S_IRUGO,
								fj_smb_chg_debug_root, NULL,
								&fj_smb_chg_param_fops);
		if (!d) {
			FJ_CHARGER_ERRLOG("[%s] debugfs_create_file(params) failed.\n", __func__);
		}

		ovp_device_initialized(INITIALIZE_CHG);
		FJ_CHARGER_DBGLOG("[%s] probe complete\n", __func__);
	} else {
		ret = smb347_hw_init(smb);
		if (ret < 0)
			return ret;

		smb->mains.name = "smb347-mains";
		smb->mains.type = POWER_SUPPLY_TYPE_MAINS;
		smb->mains.get_property = smb347_mains_get_property;
		smb->mains.properties = smb347_mains_properties;
		smb->mains.num_properties = ARRAY_SIZE(smb347_mains_properties);
		smb->mains.supplied_to = battery;
		smb->mains.num_supplicants = ARRAY_SIZE(battery);

		smb->usb.name = "smb347-usb";
		smb->usb.type = POWER_SUPPLY_TYPE_USB;
		smb->usb.get_property = smb347_usb_get_property;
		smb->usb.properties = smb347_usb_properties;
		smb->usb.num_properties = ARRAY_SIZE(smb347_usb_properties);
		smb->usb.supplied_to = battery;
		smb->usb.num_supplicants = ARRAY_SIZE(battery);

		smb->battery.name = "smb347-battery";
		smb->battery.type = POWER_SUPPLY_TYPE_BATTERY;
		smb->battery.get_property = smb347_battery_get_property;
		smb->battery.properties = smb347_battery_properties;
		smb->battery.num_properties = ARRAY_SIZE(smb347_battery_properties);

		ret = power_supply_register(dev, &smb->mains);
		if (ret < 0)
			return ret;

		ret = power_supply_register(dev, &smb->usb);
		if (ret < 0) {
			power_supply_unregister(&smb->mains);
			return ret;
		}

		ret = power_supply_register(dev, &smb->battery);
		if (ret < 0) {
			power_supply_unregister(&smb->usb);
			power_supply_unregister(&smb->mains);
			return ret;
		}

		/*
		 * Interrupt pin is optional. If it is connected, we setup the
		 * interrupt support here.
		 */
		if (pdata->irq_gpio >= 0) {
			ret = smb347_irq_init(smb);
			if (ret < 0) {
				dev_warn(dev, "failed to initialize IRQ: %d\n", ret);
				dev_warn(dev, "disabling IRQ support\n");
			}
		}

		smb->dentry = debugfs_create_file("smb347-regs", S_IRUSR, NULL, smb,
						  &smb347_debugfs_fops);
	}
#endif
/* FUJITSU:2012-11-20 FJ_CHARGER end */

	return 0;
}

static int smb347_remove(struct i2c_client *client)
{
	struct smb347_charger *smb = i2c_get_clientdata(client);

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (!IS_ERR_OR_NULL(smb->dentry))
		debugfs_remove(smb->dentry);

/* FUJITSU:2012-11-20 FJ_CHARGER start */
#ifdef FJ_CHARGER
	power_supply_unregister(&smb->usb);
	power_supply_unregister(&smb->mains);
	power_supply_unregister(&smb->mhl);
	power_supply_unregister(&smb->holder);

	unregister_early_suspend(&fj_chg_smb_early_suspend_handler);
#else
	if (client->irq) {
		smb347_irq_disable(smb);
		free_irq(client->irq, smb);
		gpio_free(smb->pdata->irq_gpio);
	}
	power_supply_unregister(&smb->battery);
	power_supply_unregister(&smb->usb);
	power_supply_unregister(&smb->mains);
#endif
/* FUJITSU:2012-11-20 FJ_CHARGER end */

	return 0;
}

static const struct i2c_device_id smb347_id[] = {
	{ "smb347", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, smb347_id);

static struct i2c_driver smb347_driver = {
	.driver = {
		.name = "smb347",
	},
	.probe        = smb347_probe,
	.remove       = __devexit_p(smb347_remove),
/* FUJITSU:2013-02-05 FJ_CHARGER start */
#ifdef FJ_CHARGER
	.shutdown	= fj_smb_chg_shutdown,
#ifdef CONFIG_PM
	.suspend = fj_smb_chg_suspend,
	.resume = fj_smb_chg_resume,
#endif
#endif
/* FUJITSU:2013-02-05 FJ_CHARGER end */
	.id_table     = smb347_id,
};

static int __init smb347_init(void)
{
	return i2c_add_driver(&smb347_driver);
}
late_initcall(smb347_init);

static void __exit smb347_exit(void)
{
	i2c_del_driver(&smb347_driver);
}
module_exit(smb347_exit);

MODULE_AUTHOR("Bruce E. Robertson <bruce.e.robertson@intel.com>");
MODULE_AUTHOR("Mika Westerberg <mika.westerberg@linux.intel.com>");
MODULE_DESCRIPTION("SMB347 battery charger driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:smb347");
