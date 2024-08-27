/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012-2013
/*----------------------------------------------------------------------------*/

#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/power/bq24192-charger.h>
#include <linux/power/bq24192-charger_hw.h>
#include <linux/seq_file.h>
#include <asm/system.h>

#include <linux/uaccess.h>
#include <mach/board_fj.h>
#include <linux/delay.h>
#include <linux/mfd/fj_charger.h>
#include <linux/mfd/fj_charger_local.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/mfd/pm8xxx/ccadc.h>
#include <linux/power_supply.h>
#include <linux/max17050_battery.h>
#include <linux/nonvolatile_common.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/ovp.h>
#ifndef FJ_CHARGER_LOCAL
#include <linux/fta.h>
#endif /* FJ_CHARGER_LOCAL */

//#define FJ_CHARGER_DBG
#undef FJ_CHARGER_DBG

#ifdef FJ_CHARGER_DBG
static int fj_bq_charger_debug = 1;
#define FJ_CHARGER_DBGLOG(x, y...) \
			if (fj_bq_charger_debug){ \
				printk(KERN_WARNING "[bq24192_chg] " x, ## y); \
			}
#else
#define FJ_CHARGER_DBGLOG(x, y...)
#endif /* FJ_CHARGER_DBG */

#define FJ_CHARGER_INFOLOG(x, y...)			printk(KERN_INFO "[bq24192_chg] " x, ## y)
#define FJ_CHARGER_WARNLOG(x, y...)			printk(KERN_WARNING "[bq24192_chg] " x, ## y)
#define FJ_CHARGER_ERRLOG(x, y...)			printk(KERN_ERR "[bq24192_chg] " x, ## y)

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
#define CHARGE_CHECK_PERIOD_MS				(10 * 1000)				/* 10sec */
#define FULL_CHARGE_CHECK1_PERIOD_MS		( 3 * 1000 * 60)		/*  3min */
#define FULL_CHARGE_CHECK2_PERIOD_MS		(27 * 1000 * 60)		/* 27min */
#define SAFETY_CHARGE_CHECK_PERIOD_MS		(13 * 1000 * 60 * 60)	/* 13h   */

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

#define CHARGE_MODE_IS_FJ()					fj_bq_get_fj_mode()

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
											FJ_CHG_PARAM_FGIC_NOT_INITIALIZE)

#define FJ_CHG_PARAM_PAUSE_MASK				FJ_CHG_PARAM_CHG_DISABLE
#define FJ_CHG_PARAM_LIMIT_MASK				(FJ_CHG_PARAM_BATT_TEMP_WARM	| \
											FJ_CHG_PARAM_BATT_TEMP_COOL)

#define FJ_CHG_VALUE_BATT_TEMP_HOT			(50)
#define FJ_CHG_VALUE_BATT_TEMP_WARM			(45)
#define FJ_CHG_VALUE_BATT_TEMP_COOL			(10)
#define FJ_CHG_VALUE_BATT_TEMP_COLD			(0)

#define FJ_CHG_VALUE_ADP_VOLTAGE_OV			0x75C2		/* 6.409V */
#define FJ_CHG_VALUE_ADP_VOLTAGE_UV			0x6D94		/* 4.000V */

#define FJ_CHG_VALUE_LOW_BATT_VOLTAGE		0xA780		/* 3.35V  */
#define FJ_CHG_VALUE_BATT_VOLTAGE_OV		0xDE80		/* 4.45V  */

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

struct fj_bq_charger_param
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
};

struct fj_bq_chg_queue_t {
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
} fj_bq_charger_state_type;

typedef enum {
	FJ_CHG_EVENT_MONITOR_PARAM = 0,			/* period monitor			*/
	FJ_CHG_EVENT_CHARGE_START,				/* charge start				*/
	FJ_CHG_EVENT_CHARGE_STOP,				/* charge end				*/
	FJ_CHG_EVENT_FULL_CHECK,				/* additional charge		*/
	FJ_CHG_EVENT_FULL_CHECK_END,			/* full charge				*/
	FJ_CHG_EVENT_SAFE_TIMEOUT,				/* safe timer expire		*/
	FJ_CHG_EVENT_NUM
} fj_bq_charger_event_type;

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
} fj_bq_charger_err_type;
typedef enum {
	FJ_CHG_FULL_CHECK_NON = 0,
	FJ_CHG_FULL_CHECK1,
	FJ_CHG_FULL_CHECK2,
	FJ_CHG_FULL_CHECK_NUM
} fj_bq_charger_full_check_type;


static int fj_bq_chg_stay_full = 0;
static struct dentry *fj_bq_chg_debug_root;    /* for debugfs */
static unsigned int fj_bq_chg_old_limit = 0;
struct bq24192_charger *the_bq = NULL;
static unsigned long fj_bq_charger_src = 0;
static unsigned int fj_bq_charger_src_current[CHG_SOURCE_NUM] = {0};
static unsigned int fj_bq_chg_err_state = FJ_CHG_ERROR_NONE;
static unsigned int fj_bq_chg_enable = FJ_CHG_ENABLE;
static unsigned long fj_bq_chg_init_event = 0;
static unsigned int fj_bq_chg_full_check_factor = 0;
static unsigned char fj_bq_chg_change_curr_limit = 0;	/* change charge current limit */
static int fj_bq_chg_curr_limit = 0;					/* Enable/Disable charge current limit */
static fj_bq_charger_full_check_type fj_bq_chg_full_check_state = FJ_CHG_FULL_CHECK_NON;

static DEFINE_SPINLOCK(fj_bq_chg_info_lock);
static DEFINE_SPINLOCK(fj_bq_chg_list_lock);
static LIST_HEAD(fj_bq_chg_list_head);


int fj_bq_chg_queue_check(void)
{
	int quenum = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/* queue check */
	spin_lock(&fj_bq_chg_list_lock);
	if (!list_empty(&fj_bq_chg_list_head)) {
		quenum++;
	}
	spin_unlock(&fj_bq_chg_list_lock);
	FJ_CHARGER_DBGLOG("[%s] quenum = %d \n",__func__, quenum);

	return quenum;
}

void fj_bq_chg_push_queue(struct work_struct *work)
{
	struct fj_bq_chg_queue_t *chg_q;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	chg_q = kmalloc(sizeof(struct fj_bq_chg_queue_t), GFP_KERNEL);
	if (chg_q != NULL) {
		chg_q->work = work;
	} else {
		FJ_CHARGER_ERRLOG("[%s] cant get memory\n",__func__);
		return;
	}

	spin_lock(&fj_bq_chg_list_lock);
	list_add_tail(&chg_q->list, &fj_bq_chg_list_head);
	spin_unlock(&fj_bq_chg_list_lock);

	return;
}

struct work_struct * fj_bq_chg_pop_queue(void)
{
	struct work_struct *work = NULL;
	struct fj_bq_chg_queue_t *chg_q;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	spin_lock(&fj_bq_chg_list_lock);
	if (!list_empty(&fj_bq_chg_list_head)) {
		chg_q = list_entry(fj_bq_chg_list_head.next, struct fj_bq_chg_queue_t, list);
		work = chg_q->work;
		list_del(fj_bq_chg_list_head.next);
	}
	spin_unlock(&fj_bq_chg_list_lock);
	if (work != NULL) {
		kfree(chg_q);
	}
	FJ_CHARGER_DBGLOG("[%s] work = %p \n",__func__, work);

	return work;
}

static void fj_bq_chg_wakelock(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	wake_lock(&bq->charging_wake_lock);
}

static void fj_bq_chg_wakeunlock(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	wake_unlock(&bq->charging_wake_lock);
}

static void fj_bq_chg_wakeup_wakelock(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	wake_lock_timeout(&bq->charging_wakeup_lock, HZ);
}

static enum power_supply_property fj_bq_chg_mains_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static int fj_bq_chg_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		spin_lock(&fj_bq_chg_info_lock);
		if (fj_bq_charger_src & FJ_CHG_SOURCE_HOLDER) {
			if ( (psy->type == POWER_SUPPLY_TYPE_MAINS) ||
				 (psy->type == POWER_SUPPLY_TYPE_USB)   ||
				 (psy->type == POWER_SUPPLY_TYPE_MHL)     ) {
					val->intval = 0;
			}
		} else {
			if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
				val->intval = ((fj_bq_charger_src & FJ_CHG_SOURCE_AC) ? 1 : 0);
			}
			if (psy->type == POWER_SUPPLY_TYPE_USB) {
				val->intval = ((fj_bq_charger_src & FJ_CHG_SOURCE_USB) ? 1 : 0);
			}
			if (psy->type == POWER_SUPPLY_TYPE_MHL) {
				val->intval = ((fj_bq_charger_src & FJ_CHG_SOURCE_MHL) ? 1 : 0);
			}
		}
		if (psy->type == POWER_SUPPLY_TYPE_HOLDER) {
			val->intval = ((fj_bq_charger_src & FJ_CHG_SOURCE_HOLDER) ? 1 : 0);
		}
		spin_unlock(&fj_bq_chg_info_lock);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int fj_bq_chg_is_suspending(struct bq24192_charger *bq)
{
	int ret = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (test_bit(CHG_SUSPENDING, &bq->suspend_flag)) {
		FJ_CHARGER_DBGLOG("[%s] detect suspending...\n", __func__);
		ret = 1;
	}
	return ret;
}

static void fj_bq_chg_power_supply_update_all(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (bq != 0) {
		FJ_CHARGER_REC_LOG("[%s] update online chg_src=0x%04x\n",
								__func__, (unsigned int)fj_bq_charger_src);
		power_supply_changed(&bq->usb);
		power_supply_changed(&bq->mains);
		power_supply_changed(&bq->mhl);
		power_supply_changed(&bq->holder);
	}
}

static void fj_bq_chg_start_monitor(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	schedule_delayed_work(&bq->charge_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(CHARGE_CHECK_PERIOD_MS)));
}
static void fj_bq_chg_stop_monitor(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	cancel_delayed_work(&bq->charge_monitor_work);
}

static void fj_bq_chg_start_safetimer(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	schedule_delayed_work(&bq->safety_charge_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(SAFETY_CHARGE_CHECK_PERIOD_MS)));
}

static void fj_bq_chg_stop_safetimer(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	cancel_delayed_work(&bq->safety_charge_monitor_work);
}

static void fj_bq_chg_start_fullcharge1timer(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_bq_chg_full_check_state = FJ_CHG_FULL_CHECK1;
	schedule_delayed_work(&bq->full_charge_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(FULL_CHARGE_CHECK1_PERIOD_MS)));
}

static void fj_bq_chg_start_fullcharge2timer(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_bq_chg_full_check_state = FJ_CHG_FULL_CHECK2;
	schedule_delayed_work(&bq->full_charge_monitor_work,
	                      round_jiffies_relative(msecs_to_jiffies(FULL_CHARGE_CHECK2_PERIOD_MS)));
}

static void fj_bq_chg_stop_fullchargetimer(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (fj_bq_chg_full_check_factor & FJ_CHG_FULL_CHECK_FACT_T2) {
		cancel_delayed_work(&bq->full_charge_monitor_work);
	}
}

static void fj_bq_chg_current_check(struct bq24192_charger *bq, unsigned int *tmp_current)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_bq_chg_old_limit = bq->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK;
	if (fj_bq_chg_old_limit) {
		if (bq->charger_info.chg_current > FJ_CHG_LIMIT_CURRENT_900) {
			*tmp_current = FJ_CHG_LIMIT_CURRENT_900;
		} else {
			*tmp_current = FJ_CHG_LIMIT_CURRENT_500;
		}
	} else {
		*tmp_current = bq->charger_info.chg_current;
	}

	spin_lock(&fj_bq_chg_info_lock);
	/* a countermeasure against overheat */
	if ((fj_bq_chg_curr_limit == FJ_CHG_CURRENT_LIMITED_900) &&
		(*tmp_current > FJ_CHG_LIMIT_CURRENT_900)) {
		*tmp_current = FJ_CHG_LIMIT_CURRENT_900;
	} else if ((fj_bq_chg_curr_limit == FJ_CHG_CURRENT_LIMITED_500) &&
		(*tmp_current > FJ_CHG_LIMIT_CURRENT_500)) {
		*tmp_current = FJ_CHG_LIMIT_CURRENT_500;
	} else if ((fj_bq_chg_curr_limit == FJ_CHG_CURRENT_LIMITED_300) &&
		(*tmp_current > FJ_CHG_LIMIT_CURRENT_300)) {
		*tmp_current = FJ_CHG_LIMIT_CURRENT_300;
	}
	fj_bq_chg_change_curr_limit = FJ_CHG_NOT_CHANGE_CL;
	spin_unlock(&fj_bq_chg_info_lock);
}

static void fj_bq_chg_batt_current_check(int *current_check)
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

static void fj_bq_chg_start_charge(struct bq24192_charger *bq)
{
	unsigned int tmp_current = 0;

	FJ_CHARGER_DBGLOG("[%s] mode = %d chg_current = %d\n", __func__, bq->charge_mode, bq->charger_info.chg_current);
	/* Charge Start */
	if (bq->charge_mode == CHARGE_MODE_MANUAL) {
		fj_bq_chg_current_check(bq, &tmp_current);
		FJ_CHARGER_INFOLOG("[%s] start current = %d\n",__func__, tmp_current);
		fj_bq_chg_hw_charge_start(tmp_current);
	} else {
		/* not charging */
		FJ_CHARGER_ERRLOG("[%s] invalied mode %d\n", __func__, bq->charge_mode);
	}
}

static void fj_bq_chg_start_recharge(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	fj_bq_chg_start_charge(bq);
}

static void fj_bq_chg_stop_charge(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] mode = %d chg_current = %d\n", __func__, bq->charge_mode, bq->charger_info.chg_current);

	/* Charge stop process */
	if (bq->charge_mode == CHARGE_MODE_MANUAL) {
		fj_bq_chg_hw_charge_stop();
	} else {
		/* not charging */
		FJ_CHARGER_ERRLOG("[%s] invalied mode %d\n", __func__, bq->charge_mode);
	}
}

static void fj_bq_chg_stop_chargefull(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] mode = %d chg_current = %d\n", __func__, bq->charge_mode, bq->charger_info.chg_current);
	/* Charge stop process */
	if (bq->charge_mode == CHARGE_MODE_MANUAL) {
		fj_bq_chg_hw_charge_stop();
	} else {
		/* not charging */
		FJ_CHARGER_ERRLOG("[%s] invalied mode %d\n", __func__, bq->charge_mode);
	}
}

static void fj_bq_chg_get_param(struct fj_bq_charger_param *charger_param)
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
		FJ_CHARGER_ERRLOG("[%s] getBATT_CAPACITY error %d\n", __func__, ret);
	}
	charger_param->soc = soc;
}

static void fj_bq_chg_check_params(struct fj_bq_charger_param *param, struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/* Supply voltage check */
	if (param->supply_volt >= FJ_CHG_VALUE_ADP_VOLTAGE_OV) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_ADP_VOLTAGE_OV(0x%08x)\n", __func__, param->supply_volt);

		bq->charger_info.current_err |= FJ_CHG_PARAM_ADP_VOLTAGE_OV;
	} else {
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_ADP_VOLTAGE_OV);
	}

	if (param->supply_volt < FJ_CHG_VALUE_ADP_VOLTAGE_UV) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_ADP_VOLTAGE_UV(0x%08x)\n", __func__, param->supply_volt);

		bq->charger_info.current_err |= FJ_CHG_PARAM_ADP_VOLTAGE_UV;
	} else {
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_ADP_VOLTAGE_UV);
	}

	/* Battery voltage check */
	if (param->batt_volt >= FJ_CHG_VALUE_BATT_VOLTAGE_OV) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_BATT_VOLTAGE_OV(0x%08x)\n", __func__, param->batt_volt);

		bq->charger_info.current_err |= FJ_CHG_PARAM_BATT_VOLTAGE_OV;
	} else {
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_VOLTAGE_OV);
	}

	/* Battery temp check */
	if (param->batt_temp >=  FJ_CHG_VALUE_BATT_TEMP_HOT) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_BATT_TEMP_HOT(%d)\n", __func__, param->batt_temp);
		
		bq->charger_info.current_err |= FJ_CHG_PARAM_BATT_TEMP_HOT;
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_WARM |
											FJ_CHG_PARAM_BATT_TEMP_COOL |
											FJ_CHG_PARAM_BATT_TEMP_COLD);
	} else if ((param->batt_temp <  FJ_CHG_VALUE_BATT_TEMP_HOT) &&
				(param->batt_temp >  FJ_CHG_VALUE_BATT_TEMP_WARM)) {
		FJ_CHARGER_WARNLOG("[%s] FJ_CHG_VALUE_BATT_TEMP_WARM(%d)\n", __func__, param->batt_temp);
					
		bq->charger_info.current_err |= FJ_CHG_PARAM_BATT_TEMP_WARM;
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_HOT |
											FJ_CHG_PARAM_BATT_TEMP_COOL |
											FJ_CHG_PARAM_BATT_TEMP_COLD);
	} else if ((param->batt_temp <=  FJ_CHG_VALUE_BATT_TEMP_COOL) &&
				(param->batt_temp >  FJ_CHG_VALUE_BATT_TEMP_COLD)) {
		FJ_CHARGER_WARNLOG("[%s] FJ_CHG_VALUE_BATT_TEMP_COOL(%d)\n", __func__, param->batt_temp);

		bq->charger_info.current_err |= FJ_CHG_PARAM_BATT_TEMP_COOL;
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_HOT |
											FJ_CHG_PARAM_BATT_TEMP_WARM |
											FJ_CHG_PARAM_BATT_TEMP_COLD);
	} else if (param->batt_temp <= FJ_CHG_VALUE_BATT_TEMP_COLD) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_VALUE_BATT_TEMP_COLD(%d)\n", __func__, param->batt_temp);

		bq->charger_info.current_err |= FJ_CHG_PARAM_BATT_TEMP_COLD;
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_HOT |
											FJ_CHG_PARAM_BATT_TEMP_WARM |
											FJ_CHG_PARAM_BATT_TEMP_COOL);
	} else {
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_BATT_TEMP_HOT |
											FJ_CHG_PARAM_BATT_TEMP_WARM |
											FJ_CHG_PARAM_BATT_TEMP_COOL |
											FJ_CHG_PARAM_BATT_TEMP_COLD);
	}

	if (fj_bq_chg_err_state != FJ_CHG_ERROR_NONE) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_OVP_STATE_ERROR\n", __func__);

		bq->charger_info.current_err |= FJ_CHG_PARAM_OVP_STATE_ERROR;
	} else {
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_OVP_STATE_ERROR);
	}

	if (fj_bq_chg_enable != FJ_CHG_ENABLE) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_CHG_DISABLE\n", __func__);

		bq->charger_info.current_err |= FJ_CHG_PARAM_CHG_DISABLE;
	} else {
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_CHG_DISABLE);
	}

	if (fj_bq_chg_curr_limit) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_LIMITED_CURR\n", __func__);

		bq->charger_info.current_err |= FJ_CHG_PARAM_LIMITED_CURR;
	} else {
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_LIMITED_CURR);
	}

	if (!fj_battery_get_initialized_flag()) {
		FJ_CHARGER_ERRLOG("[%s] FJ_CHG_PARAM_FGIC_NOT_INITIALIZE\n", __func__);

		bq->charger_info.current_err |= FJ_CHG_PARAM_FGIC_NOT_INITIALIZE;
	} else {
		bq->charger_info.current_err &= ~(FJ_CHG_PARAM_FGIC_NOT_INITIALIZE);
	}
}

static void fj_bq_chg_err_stateupdate(struct bq24192_charger *bq)
{
	static unsigned int tmp_updated = 0;
	unsigned int state = bq->charger_info.state;
	unsigned int up_bit = bq->charger_info.current_err;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/*
	 * priority
	 *     HIGH: SUPPLY VOLTAGE HIGH
	 *           OVER VOLTAGE
	 *           BATT TEMP ERROR
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
	} else if (up_bit & FJ_CHG_PARAM_BATT_TEMP_HOT) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_TEMP_HOT)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_OVERHEAT);
			tmp_updated = FJ_CHG_PARAM_BATT_TEMP_HOT;
		}
	} else if (up_bit & FJ_CHG_PARAM_BATT_TEMP_WARM) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_TEMP_WARM)) {
			if ((state == FJ_CHG_STATE_CHARGING) ||
				(state == FJ_CHG_STATE_FULL_CHECKING) ||
				(state == FJ_CHG_STATE_FULL)) {
				fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
			} else {
				fj_battery_set_health(POWER_SUPPLY_HEALTH_OVERHEAT);
			}
			tmp_updated = FJ_CHG_PARAM_BATT_TEMP_WARM;
		}
	} else if (up_bit & FJ_CHG_PARAM_BATT_TEMP_COLD) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_TEMP_COLD)) {
			fj_battery_set_health(POWER_SUPPLY_HEALTH_COLD);
			tmp_updated = FJ_CHG_PARAM_BATT_TEMP_COLD;
		}
	} else if (up_bit & FJ_CHG_PARAM_BATT_TEMP_COOL) {
		if (!(tmp_updated & FJ_CHG_PARAM_BATT_TEMP_COOL)) {
			if ((state == FJ_CHG_STATE_NOT_CHARGING) ||
				(state == FJ_CHG_STATE_CHARGING) ||
				(state == FJ_CHG_STATE_FULL_CHECKING) ||
				(state == FJ_CHG_STATE_FULL)) {
				fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
			} else {
				fj_battery_set_health(POWER_SUPPLY_HEALTH_COLD);
			}
			tmp_updated = FJ_CHG_PARAM_BATT_TEMP_COOL;
		}
	} else if (up_bit & FJ_CHG_PARAM_ADP_VOLTAGE_UV) {
		if (!(tmp_updated & FJ_CHG_PARAM_ADP_VOLTAGE_UV)) {
		/* !! supply volt low is NOT error !!*/
		fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
			tmp_updated = FJ_CHG_PARAM_ADP_VOLTAGE_UV;
		}
	} else {
		fj_battery_set_health(POWER_SUPPLY_HEALTH_GOOD);
		tmp_updated = FJ_CHG_PARAM_NO_ERROR;
	}
}

static int fj_bq_chg_batt_soc_nearly_full(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (soc >= FJ_CHG_VALUE_FULL_SOC_LV2) {
		ret = 1;
	}
	return ret;
}

static int fj_bq_chg_batt_soc_is_full(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (soc >= FJ_CHG_VALUE_FULL_SOC_LV1) {
		ret = 1;
	}
	return ret;
}

static int fj_bq_chg_batt_recharge(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (soc <= FJ_CHG_VALUE_RECHARGE_LV1) {
		ret = 1;
 	}
	return ret;
}

static int fj_bq_chg_batt_fullcheck2chg(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (soc <= FJ_CHG_VALUE_RECHARGE_LV2) {
		ret = 1;
 	}
	return ret;
}

static int fj_bq_chg_ind_full2chg(int soc)
{
	int ret = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	/* Determine whether the state of indicator should be fully charged */
	if ((soc <= FJ_CHG_VALUE_RECHARGE_LV2) && fj_bq_chg_stay_full) {
		fj_bq_chg_stay_full = 0;
		ret = 1;
	}

	return ret;
}

static fj_bq_charger_state_type fj_bq_chg_func_not_charging(struct bq24192_charger *bq, fj_bq_charger_event_type event)
{
	fj_bq_charger_state_type state = FJ_CHG_STATE_NOT_CHARGING;
	struct fj_bq_charger_param param;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		if (fj_bq_chg_err_state != FJ_CHG_ERROR_NONE) {
			fj_bq_chg_get_param(&param);
			fj_bq_chg_check_params(&param, bq);
			fj_bq_chg_err_stateupdate(bq);

			if (bq->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR) {
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
		}
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		if (bq->charge_mode == CHARGE_MODE_NOT_CHARGE) {
			/* nothing to do */
		} else {
			fj_bq_chg_get_param(&param);
			fj_bq_chg_check_params(&param, bq);
			fj_bq_chg_err_stateupdate(bq);

			if (bq->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
				/* clear error status except FJ_CHG_PARAM_PAUSE_MASK */
				bq->charger_info.current_err = FJ_CHG_PARAM_PAUSE_MASK;
				fj_bq_chg_old_limit = 0;
				fj_bq_chg_err_stateupdate(bq);

				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
				state = FJ_CHG_STATE_PAUSE;
			} else if ((bq->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) ||
						(bq->charger_info.current_err & FJ_CHG_PARAM_BATT_TEMP_WARM)) {
				fj_bq_chg_wakelock(bq);
				if ((bq->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
					!(bq->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
					/* ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
				} else {
					/* except ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
				}
				state = FJ_CHG_STATE_ERROR;
			} else {
				fj_bq_chg_wakelock(bq);
				fj_bq_chg_start_charge(bq);
				fj_bq_chg_start_safetimer(bq);

				fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
				state = FJ_CHG_STATE_CHARGING;
			}
			fj_bq_chg_start_monitor(bq);
		}
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		bq->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_bq_chg_old_limit = 0;
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_bq_chg_err_stateupdate(bq);
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

static fj_bq_charger_state_type fj_bq_chg_func_charging(struct bq24192_charger *bq, fj_bq_charger_event_type event)
{
	fj_bq_charger_state_type state = FJ_CHG_STATE_CHARGING;
	struct fj_bq_charger_param param;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_bq_chg_get_param(&param);
		fj_bq_chg_check_params(&param, bq);
		fj_bq_chg_err_stateupdate(bq);

		if (bq->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
			fj_bq_chg_stop_charge(bq);
			fj_bq_chg_stop_safetimer(bq);
			fj_bq_chg_wakeunlock(bq);

			/* clear error status except FJ_CHG_PARAM_PAUSE_MASK */
			bq->charger_info.current_err = FJ_CHG_PARAM_PAUSE_MASK;
			fj_bq_chg_old_limit = 0;
			fj_bq_chg_err_stateupdate(bq);

			fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			state = FJ_CHG_STATE_PAUSE;
		} else if (bq->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) {
			fj_bq_chg_stop_charge(bq);
			fj_bq_chg_stop_safetimer(bq);

			if ((bq->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
				!(bq->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
				/* ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
			} else {
				/* except ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
			state = FJ_CHG_STATE_ERROR;
		} else if (fj_bq_chg_ind_full2chg(param.soc)) {
			fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
		} else if (((bq->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK) ^ fj_bq_chg_old_limit) ||
					(fj_bq_chg_change_curr_limit != FJ_CHG_NOT_CHANGE_CL)) {
			/* set limited current */
			fj_bq_chg_start_charge(bq);
		} else {
			fj_bq_chg_full_check_factor = 0;
			if (fj_bq_chg_batt_soc_nearly_full(param.soc)) {
				fj_bq_chg_full_check_factor |= FJ_CHG_FULL_CHECK_FACT_T1;
				state = FJ_CHG_STATE_FULL_CHECKING;
			}
			if (fj_bq_chg_batt_soc_is_full(param.soc)) {
				fj_bq_chg_full_check_factor |= FJ_CHG_FULL_CHECK_FACT_T2;
				fj_bq_chg_start_fullcharge1timer(bq);
				state = FJ_CHG_STATE_FULL_CHECKING;
			}
		}
		fj_bq_chg_start_monitor(bq);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		fj_bq_chg_stop_charge(bq);
		fj_bq_chg_stop_safetimer(bq);
		fj_bq_chg_start_charge(bq);
		fj_bq_chg_start_safetimer(bq);
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_bq_chg_stop_charge(bq);
		fj_bq_chg_stop_safetimer(bq);
		fj_bq_chg_stop_monitor(bq);

		bq->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_bq_chg_old_limit = 0;
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_bq_chg_err_stateupdate(bq);
		fj_bq_chg_wakeunlock(bq);
		state = FJ_CHG_STATE_NOT_CHARGING;
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		fj_bq_chg_stop_charge(bq);
		bq->charger_info.current_err |= FJ_CHG_PARAM_UNSPEC_FAILURE;
		fj_bq_chg_err_stateupdate(bq);
		fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
		state = FJ_CHG_STATE_TIME_OUT;
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

static fj_bq_charger_state_type fj_bq_chg_func_full_checking(struct bq24192_charger *bq, fj_bq_charger_event_type event)
{
	fj_bq_charger_state_type state = FJ_CHG_STATE_FULL_CHECKING;
	struct fj_bq_charger_param param;
	int current_check = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_bq_chg_get_param(&param);
		fj_bq_chg_check_params(&param, bq);
		fj_bq_chg_err_stateupdate(bq);

		if (bq->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
			fj_bq_chg_stop_charge(bq);
			fj_bq_chg_stop_safetimer(bq);
			fj_bq_chg_stop_fullchargetimer(bq);

			fj_bq_chg_wakeunlock(bq);

			fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			state = FJ_CHG_STATE_PAUSE;
		} else if (bq->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) {
			fj_bq_chg_stop_charge(bq);
			fj_bq_chg_stop_safetimer(bq);
			fj_bq_chg_stop_fullchargetimer(bq);

			if ((bq->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
				!(bq->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
				/* ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
			} else {
				/* except ADP UV */
				fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			}
			state = FJ_CHG_STATE_ERROR;
		} else if (((bq->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK) ^ fj_bq_chg_old_limit) ||
					(fj_bq_chg_change_curr_limit != FJ_CHG_NOT_CHANGE_CL)) {
			/* set limited current */
			fj_bq_chg_start_charge(bq);
		} else if (fj_bq_chg_batt_fullcheck2chg(param.soc)) {
			fj_bq_chg_stop_fullchargetimer(bq);
			fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
			state = FJ_CHG_STATE_CHARGING;
		} else {
			if (fj_bq_chg_full_check_factor & FJ_CHG_FULL_CHECK_FACT_T1) {
				fj_bq_chg_batt_current_check(&current_check);
				if (current_check) {
					FJ_CHARGER_DBGLOG("[%s] battery full\n", __func__);

					/* battery current (0mA - 100mA) */
					fj_bq_chg_stop_chargefull(bq);
					fj_bq_chg_stop_safetimer(bq);
					fj_bq_chg_stop_fullchargetimer(bq);

					fj_battery_set_status(POWER_SUPPLY_STATUS_FULL);
					fj_bq_chg_stay_full = 1;
					fj_bq_chg_wakeunlock(bq);
					state = FJ_CHG_STATE_FULL;
				}
			}

			if (((fj_bq_chg_full_check_factor & FJ_CHG_FULL_CHECK_FACT_T2) == 0) &&
				(state != FJ_CHG_STATE_FULL)) {
				if (fj_bq_chg_batt_soc_is_full(param.soc)) {
					fj_bq_chg_full_check_factor |= FJ_CHG_FULL_CHECK_FACT_T2;
					fj_bq_chg_start_fullcharge1timer(bq);
				}
			}
		}
		fj_bq_chg_start_monitor(bq);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		fj_bq_chg_stop_charge(bq);
		fj_bq_chg_stop_safetimer(bq);
		fj_bq_chg_start_charge(bq);
		fj_bq_chg_start_safetimer(bq);
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_bq_chg_stop_charge(bq);
		fj_bq_chg_stop_safetimer(bq);
		fj_bq_chg_stop_fullchargetimer(bq);
		fj_bq_chg_stop_monitor(bq);

		bq->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_bq_chg_old_limit = 0;
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_bq_chg_err_stateupdate(bq);
		fj_bq_chg_wakeunlock(bq);
		state = FJ_CHG_STATE_NOT_CHARGING;
		break;

	case FJ_CHG_EVENT_FULL_CHECK:
		FJ_CHARGER_DBGLOG("[%s] additional charge\n", __func__);
		fj_battery_set_status(POWER_SUPPLY_STATUS_FULL);
		fj_bq_chg_start_fullcharge2timer(bq);
		break;

	case FJ_CHG_EVENT_FULL_CHECK_END:
		FJ_CHARGER_DBGLOG("[%s] battery full(full timer timeout)\n", __func__);
		fj_bq_chg_stop_chargefull(bq);
		fj_bq_chg_stop_safetimer(bq);
		fj_battery_set_status(POWER_SUPPLY_STATUS_FULL);
		fj_bq_chg_stay_full = 1;
		fj_bq_chg_wakeunlock(bq);
		state = FJ_CHG_STATE_FULL;
		break;

	case FJ_CHG_EVENT_SAFE_TIMEOUT:
		fj_bq_chg_stop_charge(bq);
		bq->charger_info.current_err |= FJ_CHG_PARAM_UNSPEC_FAILURE;
		fj_bq_chg_err_stateupdate(bq);
		fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
		state = FJ_CHG_STATE_TIME_OUT;
		break;

	default:
		FJ_CHARGER_ERRLOG("[%s] Irregular event %d\n", __func__, event);
		break;
	}
	return state;
}

static fj_bq_charger_state_type fj_bq_chg_func_full(struct bq24192_charger *bq, fj_bq_charger_event_type event)
{
	fj_bq_charger_state_type state = FJ_CHG_STATE_FULL;
	struct fj_bq_charger_param param;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_bq_chg_get_param(&param);
		fj_bq_chg_check_params(&param, bq);
		fj_bq_chg_err_stateupdate(bq);

		if (bq->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
			fj_bq_chg_stop_charge(bq);

			/* clear error status except FJ_CHG_PARAM_PAUSE_MASK */
			bq->charger_info.current_err = FJ_CHG_PARAM_PAUSE_MASK;
			fj_bq_chg_old_limit = 0;
			fj_bq_chg_err_stateupdate(bq);

			fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
			state = FJ_CHG_STATE_PAUSE;
		} else if (fj_bq_chg_batt_recharge(param.soc)) {
			if (bq->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) {
				fj_bq_chg_wakelock(bq);
				fj_bq_chg_stop_charge(bq);

				if ((bq->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
					!(bq->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
					/* ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
				} else {
					/* except ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
				}
				state = FJ_CHG_STATE_ERROR;
			} else {
				fj_bq_chg_wakelock(bq);
				fj_bq_chg_start_recharge(bq);
				fj_bq_chg_start_safetimer(bq);

				/* !! Notice !! */
				/* keep POWER_SUPPLY_STATUS_FULL status */
				state = FJ_CHG_STATE_CHARGING;
			}
		} else {
			/* do nothing */
		}
		fj_bq_chg_start_monitor(bq);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_bq_chg_stop_charge(bq);
		fj_bq_chg_stop_monitor(bq);

		bq->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_bq_chg_old_limit = 0;
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_bq_chg_err_stateupdate(bq);
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

static fj_bq_charger_state_type fj_bq_chg_func_error(struct bq24192_charger *bq, fj_bq_charger_event_type event)
{
	fj_bq_charger_state_type state = FJ_CHG_STATE_ERROR;
	struct fj_bq_charger_param param;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_bq_chg_get_param(&param);
		fj_bq_chg_check_params(&param, bq);
		fj_bq_chg_err_stateupdate(bq);

		if (bq->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK) {
			/* clear error status except FJ_CHG_PARAM_PAUSE_MASK */
			bq->charger_info.current_err = FJ_CHG_PARAM_PAUSE_MASK;
			fj_bq_chg_old_limit = 0;
			fj_bq_chg_err_stateupdate(bq);

			fj_bq_chg_wakeunlock(bq);
			state = FJ_CHG_STATE_PAUSE;
		} else if (!(bq->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) &&
					!(bq->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK)) {
			fj_bq_chg_start_charge(bq);
			fj_bq_chg_start_safetimer(bq);

			fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
			state = FJ_CHG_STATE_CHARGING;
		}
		fj_bq_chg_start_monitor(bq);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_bq_chg_stop_monitor(bq);

		bq->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_bq_chg_old_limit = 0;
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_bq_chg_err_stateupdate(bq);
		fj_bq_chg_wakeunlock(bq);
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

static fj_bq_charger_state_type fj_bq_chg_func_pause(struct bq24192_charger *bq, fj_bq_charger_event_type event)
{
	fj_bq_charger_state_type state = FJ_CHG_STATE_PAUSE;
	struct fj_bq_charger_param param;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_bq_chg_get_param(&param);
		fj_bq_chg_check_params(&param, bq);
		fj_bq_chg_err_stateupdate(bq);

		if (!(bq->charger_info.current_err & FJ_CHG_PARAM_PAUSE_MASK)) {
			if ((bq->charger_info.current_err & FJ_CHG_PARAM_ERR_MASK) ||
				(bq->charger_info.current_err & FJ_CHG_PARAM_LIMIT_MASK)) {
				fj_bq_chg_wakelock(bq);
				fj_bq_chg_stop_charge(bq);
				fj_bq_chg_stop_safetimer(bq);

				if ((bq->charger_info.current_err & FJ_CHG_PARAM_ADP_VOLTAGE_UV) &&
					!(bq->charger_info.current_err & FJ_CHG_PARAM_OVP_STATE_ERROR)) {
					/* ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
				} else {
					/* except ADP UV */
					fj_battery_set_status(POWER_SUPPLY_STATUS_NOT_CHARGING);
				}
				state = FJ_CHG_STATE_ERROR;
			} else {
				fj_bq_chg_wakelock(bq);
				fj_bq_chg_start_charge(bq);
				fj_bq_chg_start_safetimer(bq);

				fj_battery_set_status(POWER_SUPPLY_STATUS_CHARGING);
				state = FJ_CHG_STATE_CHARGING;
			}
		}
		fj_bq_chg_start_monitor(bq);
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_bq_chg_stop_charge(bq);
		fj_bq_chg_stop_monitor(bq);

		bq->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_bq_chg_old_limit = 0;
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_bq_chg_err_stateupdate(bq);
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

static fj_bq_charger_state_type fj_bq_chg_func_timeout(struct bq24192_charger *bq, fj_bq_charger_event_type event)
{
	fj_bq_charger_state_type state = FJ_CHG_STATE_TIME_OUT;
	struct fj_bq_charger_param param;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (event) {
	case FJ_CHG_EVENT_MONITOR_PARAM:
		fj_bq_chg_get_param(&param);
		fj_bq_chg_check_params(&param, bq);
		fj_bq_chg_start_monitor(bq);

		/* notice! : There is no need to call fj_bq_chg_err_stateupdate, */
		/* because POWER_SUPPLY_HEALTH_UNSPEC_FAILURE is set already.    */
		break;

	case FJ_CHG_EVENT_CHARGE_START:
		break;

	case FJ_CHG_EVENT_CHARGE_STOP:
		fj_bq_chg_stop_monitor(bq);

		bq->charger_info.current_err = FJ_CHG_PARAM_NO_ERROR;
		fj_bq_chg_old_limit = 0;
		fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
		fj_bq_chg_err_stateupdate(bq);
		fj_bq_chg_wakeunlock(bq);
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

typedef fj_bq_charger_state_type (*fd_charge_func_type)(struct bq24192_charger *bq, fj_bq_charger_event_type event);

fd_charge_func_type fj_bq_charge_state_func[] = {
	fj_bq_chg_func_not_charging,
	fj_bq_chg_func_charging,
	fj_bq_chg_func_full_checking,
	fj_bq_chg_func_full,
	fj_bq_chg_func_error,
	fj_bq_chg_func_pause,
	fj_bq_chg_func_timeout
};

static void fj_bq_chg_state_machine(struct bq24192_charger *bq, fj_bq_charger_event_type event)
{
	unsigned int old_state;

	if (bq->charger_info.state >= FJ_CHG_STATE_NUM) {
		FJ_CHARGER_ERRLOG("[%s] charge_state Error:%d Fatal\n", __func__, bq->charger_info.state);
		emergency_restart();
	}

	mutex_lock(&bq->chg_lock);

	old_state = bq->charger_info.state;

	FJ_CHARGER_DBGLOG("[%s] charge_state = %d event = %d\n", __func__, bq->charger_info.state, event);
	bq->charger_info.state = fj_bq_charge_state_func[bq->charger_info.state](bq, event);

	if (old_state != bq->charger_info.state) {
		FJ_CHARGER_INFOLOG("[%s] event:%d old state:%d -> new state:%d\n", __func__, event, old_state, bq->charger_info.state);
	}
	mutex_unlock(&bq->chg_lock);
}

static void fj_bq_chg_work(struct work_struct *work)
{
	int chg_current_val = 0;
	int source;
	int old_source, new_source;
	struct bq24192_charger *bq = container_of(work,
					struct bq24192_charger, charge_work);

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	spin_lock(&fj_bq_chg_info_lock);
	chg_current_val = 0;
	old_source = bq->charger_info.chg_source;
	new_source = 0;

	for (source = 0; source < CHG_SOURCE_NUM; source++) {
		if (fj_bq_charger_src & (BIT(0) << source)) {
			new_source = (BIT(0) << source);
			bq->charger_info.chg_current = fj_bq_charger_src_current[source];
			chg_current_val = fj_bq_charger_src_current[source];
			break;
		}
	}

	if (!new_source) {
		bq->charger_info.chg_current = 0;
	}
	bq->charger_info.chg_source = new_source;
	spin_unlock(&fj_bq_chg_info_lock);

	FJ_CHARGER_DBGLOG("[%s] source = %d current = %d\n",
	   __func__, bq->charger_info.chg_source, bq->charger_info.chg_current);

	if (chg_current_val != 0) {
		/* charge start */
		if(new_source != old_source) {
			fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_CHARGE_START);
		}
	} else {
		if (fj_bq_chg_err_state == FJ_CHG_ERROR_NONE) {
			/* charge end */
			fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_CHARGE_STOP);
		} else {
			FJ_CHARGER_INFOLOG("[%s] Detected OVP. Not issue CHARGE_STOP event.\n", __func__);
		}
	}
	FJ_CHARGER_DBGLOG("[%s] new_source = %d old_source = %d\n",
						__func__, new_source, old_source);
	if (new_source != old_source) {
		fj_bq_chg_power_supply_update_all(bq);
	}
}

static void fj_bq_chg_err_work(struct work_struct *work)
{
	struct bq24192_charger *bq = container_of(work,
					struct bq24192_charger, charge_err_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, bq->charger_info.state);

	if ((fj_bq_charger_src == 0) && (fj_bq_chg_err_state == FJ_CHG_ERROR_NONE)) {
		fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_CHARGE_STOP);
	} else {
		fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_MONITOR_PARAM);
	}
}

static void fj_bq_chg_nodelay_monitor_work(struct work_struct *work)
{
	struct bq24192_charger *bq = container_of(work,
					struct bq24192_charger, nodelay_monitor_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, bq->charger_info.state);

	fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_MONITOR_PARAM);
}

static void fj_bq_chg_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq24192_charger *bq = container_of(dwork,
					struct bq24192_charger, charge_monitor_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, bq->charger_info.state);

	fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_MONITOR_PARAM);
}

static void fj_bq_chg_full_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq24192_charger *bq = container_of(dwork,
					struct bq24192_charger, full_charge_monitor_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, bq->charger_info.state);

	switch (fj_bq_chg_full_check_state) {
		case FJ_CHG_FULL_CHECK1:
			fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_FULL_CHECK);
			break;
		case FJ_CHG_FULL_CHECK2:
			fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_FULL_CHECK_END);
			break;
		default:
		break;
	}
}

static void fj_bq_chg_safety_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq24192_charger *bq = container_of(dwork,
					struct bq24192_charger, safety_charge_monitor_work);

	FJ_CHARGER_DBGLOG("[%s] state=%x\n", __func__, bq->charger_info.state);

	fj_bq_chg_state_machine(bq, FJ_CHG_EVENT_SAFE_TIMEOUT);
}

static int fj_bq_chg_battery_present_check(struct bq24192_charger *bq)
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

void fj_bq_chg_source_req(int source, unsigned int mA)
{
	struct bq24192_charger *bq = the_bq;

	FJ_CHARGER_INFOLOG("[%s] %d mA=%d\n", __func__, source, mA);
    FJ_CHARGER_REC_LOG("[%s] %d mA=%d\n", __func__, source, mA);

	spin_lock(&fj_bq_chg_info_lock);
	if (mA != 0) {
		if (FJ_CHG_SOURCE_IS_USB_PORT(source)) {
			fj_bq_charger_src &= ~FJ_CHG_SOURCE_USB_PORT;
		}
		fj_bq_charger_src |= FJ_CHG_SOURCE(source);
		fj_bq_charger_src_current[source] = mA;
	} else {
		fj_bq_charger_src &= ~FJ_CHG_SOURCE(source);
		fj_bq_charger_src_current[source] = 0;
	}
	spin_unlock(&fj_bq_chg_info_lock);

	if (!bq) {
		FJ_CHARGER_WARNLOG("[%s] called before init\n",__func__);
		set_bit(CHG_INIT_EVENT, &fj_bq_chg_init_event);
	} else {
		mutex_lock(&bq->chg_quelock);
		if (fj_bq_chg_is_suspending(bq)) {
			fj_bq_chg_push_queue(&bq->charge_work);
		} else {
			schedule_work(&bq->charge_work);
		}
		mutex_unlock(&bq->chg_quelock);
	}
}
EXPORT_SYMBOL_GPL(fj_bq_chg_source_req);

/* for mc command */
int fj_bq_chg_get_adapter_voltage(char *buffer, struct kernel_param *kp)
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
EXPORT_SYMBOL_GPL(fj_bq_chg_get_adapter_voltage);

/* for mc command */
int fj_bq_chg_get_battery_voltage(char *buffer, struct kernel_param *kp)
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
EXPORT_SYMBOL_GPL(fj_bq_chg_get_battery_voltage);

int fj_bq_chg_get_battery_temperature(char *buffer, struct kernel_param *kp)
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
EXPORT_SYMBOL_GPL(fj_bq_chg_get_battery_temperature);

int fj_bq_chg_get_ambient_temperature(char *buffer, struct kernel_param *kp)
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
EXPORT_SYMBOL_GPL(fj_bq_chg_get_ambient_temperature);

int fj_bq_chg_get_receiver_temperature(char *buffer, struct kernel_param *kp)
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
EXPORT_SYMBOL_GPL(fj_bq_chg_get_receiver_temperature);

int fj_bq_chg_get_board_temperature(char *buffer, struct kernel_param *kp)
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
EXPORT_SYMBOL_GPL(fj_bq_chg_get_board_temperature);

int fj_bq_chg_get_charge_ic_temperature(char *buffer, struct kernel_param *kp)
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
EXPORT_SYMBOL_GPL(fj_bq_chg_get_charge_ic_temperature);

int fj_bq_chg_get_sensor_temperature(char *buffer, struct kernel_param *kp)
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
EXPORT_SYMBOL_GPL(fj_bq_chg_get_sensor_temperature);

/* for mc command */
int fj_bq_chg_get_battery_present(char *buffer, struct kernel_param *kp)
{
	struct bq24192_charger *bq = the_bq;
	int batt_present;
	int result;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	batt_present = fj_bq_chg_battery_present_check(bq);
	result = sprintf(buffer, "%d", batt_present);

	return result;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_get_battery_present);

/* for mc command */
int fj_bq_chg_set_charger_enable(const char *val, struct kernel_param *kp, int charger_enable)
{
	int ret;
	struct bq24192_charger *bq = the_bq;

	ret = param_set_int(val, kp);
	if (ret) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set charger_enable param to %d\n",
						__func__, charger_enable);
	if (bq) {
		fj_bq_chg_hw_set_charger_enable(charger_enable);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_set_charger_enable);

/* for mc command */
int fj_bq_chg_set_chg_voltage(const char *val, struct kernel_param *kp, int chg_voltage)
{
	int ret;
	struct bq24192_charger *bq = the_bq;

	ret = param_set_int(val, kp);
	if (ret) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set chg_voltage param to %d\n",__func__, chg_voltage);
	if (bq) {
		fj_bq_chg_hw_set_voltage(chg_voltage);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_set_chg_voltage);

/* for mc command */
int fj_bq_chg_set_chg_current(const char *val, struct kernel_param *kp, int chg_current)
{
	int ret;
	struct bq24192_charger *bq = the_bq;

	ret = param_set_int(val, kp);
	if (ret) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set chg_current param to %d\n",__func__, chg_current);
	if (bq) {
		fj_bq_chg_hw_set_current(chg_current);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_set_chg_current);

/* for mc command */
int fj_bq_chg_set_fj_chg_enable(const char *val, struct kernel_param *kp, int chg_enable)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set chg_enable param to %d\n",__func__, chg_enable);
	spin_lock(&fj_bq_chg_info_lock);

	switch (chg_enable) {
	case 0:
		fj_bq_chg_enable = FJ_CHG_DISABLE;
		fj_bq_chg_curr_limit = FJ_CHG_CURRENT_NOT_LIMITED;
		break;

	case 1:
		fj_bq_chg_enable = FJ_CHG_ENABLE;
		fj_bq_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_bq_chg_curr_limit = FJ_CHG_CURRENT_NOT_LIMITED;
		break;

	case 2:
		fj_bq_chg_enable = FJ_CHG_ENABLE;
		fj_bq_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_bq_chg_curr_limit = FJ_CHG_CURRENT_LIMITED_900;
		break;

	case 3:
		fj_bq_chg_enable = FJ_CHG_ENABLE;
		fj_bq_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_bq_chg_curr_limit = FJ_CHG_CURRENT_LIMITED_500;
		break;

	case 4:
		fj_bq_chg_enable = FJ_CHG_ENABLE;
		fj_bq_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_bq_chg_curr_limit = FJ_CHG_CURRENT_LIMITED_300;
		break;

	default:
		fj_bq_chg_enable = FJ_CHG_ENABLE;
		fj_bq_chg_change_curr_limit = FJ_CHG_CHANGE_CL;
		fj_bq_chg_curr_limit = FJ_CHG_CURRENT_NOT_LIMITED;
		break;
	}

	spin_unlock(&fj_bq_chg_info_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_set_fj_chg_enable);

/* for mc command */
int fj_bq_chg_set_chg_vinmin(const char *val, struct kernel_param *kp, int chg_vinmin)
{
	int ret;
	struct bq24192_charger *bq = the_bq;

	ret = param_set_int(val, kp);
	if (ret) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, ret);
		return ret;
	}
	FJ_CHARGER_INFOLOG("[%s] set chg_vinmin param to %d\n",__func__, chg_vinmin);
	if (bq) {
		fj_bq_chg_hw_set_vinmin(chg_vinmin);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_set_chg_vinmin);

void fj_bq_chg_notify_error(unsigned int err)
{
	struct bq24192_charger *bq = the_bq;

	FJ_CHARGER_INFOLOG("[%s] in, err=%d\n", __func__, err);

	if (err != FJ_CHG_ERROR_NONE ) {
		fj_bq_chg_err_state = FJ_CHG_ERROR_DETECT;
	} else {
		fj_bq_chg_err_state = FJ_CHG_ERROR_NONE;
	}

	if (!bq) {
		FJ_CHARGER_WARNLOG("[%s] called before init\n",__func__);
		set_bit(CHG_INIT_ERR_EVENT, &fj_bq_chg_init_event);
	} else {
		mutex_lock(&bq->chg_quelock);
		schedule_work(&bq->charge_err_work);
		mutex_unlock(&bq->chg_quelock);
	}
}
EXPORT_SYMBOL_GPL(fj_bq_chg_notify_error);

void fj_bq_chg_emergency_current_off(void)
{
	FJ_CHARGER_INFOLOG("[%s] -Emergency Current OFF\n", __func__);
	fj_bq_chg_hw_charge_stop();
}
EXPORT_SYMBOL_GPL(fj_bq_chg_emergency_current_off);

static int fj_bq_chg_get_vbat_adj(void)
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

static int fj_bq_chg_get_charge_mode(void)
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

static int fj_bq_get_fj_mode(void)
{
	int ret = 1;

	if (FJ_CHG_DEVICE_BQ != fj_get_chg_device())
		ret = 0;

	return ret;
}

static int fj_bq_chg_param_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int fj_bq_chg_param_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t fj_bq_chg_param_read(struct file *filp, char __user *buf, size_t size, loff_t *pos)
{
	char out[512];
	unsigned int len, total_len;
	unsigned int i;
	struct fj_bq_charger_param param;

	len = sprintf(&out[0], "State:%d\n", the_bq->charger_info.state);
	total_len = len;
	len = sprintf(&out[total_len], "Detect Error:0x%08x\n", the_bq->charger_info.current_err);
	total_len += len;

	len = sprintf(&out[total_len], "-------------\n");
	total_len += len;

	fj_bq_chg_get_param(&param);
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

	len = sprintf(&out[total_len], "CHG Source:0x%08lx\n", fj_bq_charger_src);
	total_len += len;

	for (i = 0; i < CHG_SOURCE_NUM; i++) {
		len = sprintf(&out[total_len], "CHG[%d] %d mA\n", i, fj_bq_charger_src_current[i]);
    	total_len += len;
	}

	len = sprintf(&out[total_len], "CHG mode:%d\n", fj_boot_mode);
	total_len += len;
	if (copy_to_user(buf, &out[0], total_len)) {
		return -EFAULT;
	}

	return simple_read_from_buffer(buf, size, pos, &out[0], total_len);
}

struct file_operations fj_bq_chg_param_fops = {
	.owner = THIS_MODULE,
	.open = fj_bq_chg_param_open,
	.release = fj_bq_chg_param_close,
    .read = fj_bq_chg_param_read,
};
#ifdef CONFIG_PM
static int fj_bq_chg_resume(struct i2c_client *client)
{
	struct bq24192_charger *bq = i2c_get_clientdata(client);

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (CHARGE_MODE_IS_FJ()) {
		schedule_work(&bq->nodelay_monitor_work);

		mutex_lock(&bq->chg_quelock);
		while (fj_bq_chg_queue_check()) {
			schedule_work(fj_bq_chg_pop_queue());
		}
		clear_bit(CHG_SUSPENDING, &bq->suspend_flag);
		mutex_unlock(&bq->chg_quelock);
	}

	return 0;
}

static int fj_bq_chg_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct bq24192_charger *bq = i2c_get_clientdata(client);

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (CHARGE_MODE_IS_FJ()) {
		set_bit(CHG_SUSPENDING, &bq->suspend_flag);

		fj_bq_chg_stop_monitor(bq);

		if (fj_bq_chg_queue_check()) {
			fj_bq_chg_wakeup_wakelock(bq);
		}
	}

	return 0;
}
#endif

static int bq24192_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	static char *battery[] = { "bq24192-battery" };
	struct device *dev = &client->dev;
	struct bq24192_charger *bq;
	int ret;
	int temp = 0;
	struct dentry *d;    /* for debugfs */

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	i2c_set_clientdata(client, bq);

	mutex_init(&bq->lock);
	bq->client = client;

	the_bq = bq;

	bq->charge_mode = fj_bq_chg_get_charge_mode();
	if (CHARGE_MODE_IS_FJ()) {
		bq->suspend_flag = 0;
		bq->charger_info.state       = FJ_CHG_STATE_NOT_CHARGING;
		bq->charger_info.chg_source  = 0;
		bq->charger_info.chg_current = 0;
		bq->charger_info.current_err = 0;
		bq->vbat_adj = fj_bq_chg_get_vbat_adj();
		ret = fj_bq_chg_hw_init(bq);
		if (ret) {
			pr_err("couldn't init hardware ret=%d\n", ret);
			return ret;
		}
		bq->usb.name = "usb",
		bq->usb.type = POWER_SUPPLY_TYPE_USB,
		bq->usb.get_property = fj_bq_chg_get_property;
		bq->usb.properties = fj_bq_chg_mains_properties;
		bq->usb.num_properties = ARRAY_SIZE(fj_bq_chg_mains_properties);
		bq->usb.supplied_to = battery;
		bq->usb.num_supplicants = ARRAY_SIZE(battery);

		bq->mains.name = "ac",
		bq->mains.type = POWER_SUPPLY_TYPE_MAINS,
		bq->mains.get_property = fj_bq_chg_get_property;
		bq->mains.properties = fj_bq_chg_mains_properties;
		bq->mains.num_properties = ARRAY_SIZE(fj_bq_chg_mains_properties);
		bq->mains.supplied_to = battery;
		bq->mains.num_supplicants = ARRAY_SIZE(battery);

		bq->mhl.name = "mhl",
		bq->mhl.type = POWER_SUPPLY_TYPE_MHL,
		bq->mhl.get_property = fj_bq_chg_get_property,
		bq->mhl.properties = fj_bq_chg_mains_properties,
		bq->mhl.num_properties = ARRAY_SIZE(fj_bq_chg_mains_properties),
		bq->mhl.supplied_to = battery,
		bq->mhl.num_supplicants = ARRAY_SIZE(battery),

		bq->holder.name = "holder",
		bq->holder.type = POWER_SUPPLY_TYPE_HOLDER,
		bq->holder.get_property = fj_bq_chg_get_property,
		bq->holder.properties = fj_bq_chg_mains_properties,
		bq->holder.num_properties = ARRAY_SIZE(fj_bq_chg_mains_properties),
		bq->holder.supplied_to = battery,
		bq->holder.num_supplicants = ARRAY_SIZE(battery),

		ret = power_supply_register(dev, &bq->usb);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] power_supply_register usb failed ret = %d\n",__func__, ret);
			return ret;
		}

		ret = power_supply_register(dev, &bq->mains);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] power_supply_register mains failed ret = %d\n",__func__, ret);
			power_supply_unregister(&bq->usb);
			return ret;
		}

		ret = power_supply_register(dev, &bq->mhl);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] power_supply_register mhl failed ret = %d\n",__func__, ret);
			power_supply_unregister(&bq->mains);
			power_supply_unregister(&bq->usb);
			return ret;
		}

		ret = power_supply_register(dev, &bq->holder);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] power_supply_register holder failed ret = %d\n",__func__, ret);
			power_supply_unregister(&bq->mhl);
			power_supply_unregister(&bq->mains);
			power_supply_unregister(&bq->usb);
			return ret;
		}

		wake_lock_init(&bq->charging_wake_lock, WAKE_LOCK_SUSPEND, "fj_charging");
		wake_lock_init(&bq->charging_wakeup_lock, WAKE_LOCK_SUSPEND, "fj_chg_wakeup");
		mutex_init(&bq->chg_lock);
		mutex_init(&bq->chg_quelock);

		INIT_WORK(&bq->charge_work, fj_bq_chg_work);
		INIT_WORK(&bq->charge_err_work, fj_bq_chg_err_work);
		INIT_WORK(&bq->nodelay_monitor_work, fj_bq_chg_nodelay_monitor_work);
		INIT_DELAYED_WORK(&bq->charge_monitor_work, fj_bq_chg_monitor);
		INIT_DELAYED_WORK(&bq->full_charge_monitor_work, fj_bq_chg_full_monitor);
		INIT_DELAYED_WORK(&bq->safety_charge_monitor_work, fj_bq_chg_safety_monitor);


		ret = fj_battery_get_battery_temp(&temp);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] get BATT_TEMP error %d\n", __func__, ret);
		}

		ret = fj_battery_set_battery_temp(temp);
		if (ret < 0) {
			FJ_CHARGER_ERRLOG("[%s] set BATT_TEMP error %d\n", __func__, ret);
		}

		if (__test_and_clear_bit(CHG_INIT_ERR_EVENT, &fj_bq_chg_init_event)) {
			schedule_work(&bq->charge_err_work);
		}

		if (__test_and_clear_bit(CHG_INIT_EVENT, &fj_bq_chg_init_event)) {
			schedule_work(&bq->charge_work);
		}

		/* Initialize Debug FS */
		fj_bq_chg_debug_root = debugfs_create_dir("fj_bq_chg", NULL);
		if (!fj_bq_chg_debug_root) {
			FJ_CHARGER_ERRLOG("[%s] debugfs_create_dir failed.\n", __func__);
		}

		d = debugfs_create_file("params", S_IRUGO,
								fj_bq_chg_debug_root, NULL,
								&fj_bq_chg_param_fops);
		if (!d) {
			FJ_CHARGER_ERRLOG("[%s] debugfs_create_file(params) failed.\n", __func__);
		}

		ovp_device_initialized(INITIALIZE_CHG);
		FJ_CHARGER_DBGLOG("[%s] probe complete\n", __func__);
	}

	return 0;
}

static int bq24192_remove(struct i2c_client *client)
{
	struct bq24192_charger *bq = i2c_get_clientdata(client);

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (!IS_ERR_OR_NULL(bq->dentry))
		debugfs_remove(bq->dentry);

	power_supply_unregister(&bq->usb);
	power_supply_unregister(&bq->mains);
	power_supply_unregister(&bq->mhl);
	power_supply_unregister(&bq->holder);

	return 0;
}

static const struct i2c_device_id bq24192_id[] = {
	{ "bq24192", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bq24192_id);

static struct i2c_driver bq24192_driver = {
	.driver = {
		.name = "bq24192",
	},
	.probe        = bq24192_probe,
	.remove       = __devexit_p(bq24192_remove),
#ifdef CONFIG_PM
	.suspend = fj_bq_chg_suspend,
	.resume = fj_bq_chg_resume,
#endif
	.id_table     = bq24192_id,
};

static int __init bq24192_init(void)
{
	return i2c_add_driver(&bq24192_driver);
}
late_initcall(bq24192_init);

static void __exit bq24192_exit(void)
{
	i2c_del_driver(&bq24192_driver);
}
module_exit(bq24192_exit);

MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("BQ24192 battery charger driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:bq24192");
