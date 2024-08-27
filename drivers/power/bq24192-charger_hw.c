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

//#define FJ_CHARGER_DBG
#undef FJ_CHARGER_DBG

#ifdef FJ_CHARGER_DBG
static int fj_bq_charger_debug = 1;
#define FJ_CHARGER_DBGLOG(x, y...) \
			if (fj_bq_charger_debug){ \
				printk(KERN_WARNING "[bq24192_chg_hw] " x, ## y); \
			}
#else
#define FJ_CHARGER_DBGLOG(x, y...)
#endif /* FJ_CHARGER_DBG */

#define FJ_CHARGER_INFOLOG(x, y...)			printk(KERN_INFO "[bq24192_chg_hw] " x, ## y)
#define FJ_CHARGER_WARNLOG(x, y...)			printk(KERN_WARNING "[bq24192_chg_hw] " x, ## y)
#define FJ_CHARGER_ERRLOG(x, y...)			printk(KERN_ERR "[bq24192_chg_hw] " x, ## y)


/* internal function prototype */
static int fj_bq_chg_hw_set_max_current(int max_current);
static int fj_bq_chg_hw_set_max_voltage(int max_voltage);
static int fj_bq_chg_hw_set_min_voltage(int min_voltage);

static int fj_bq_chg_hw_set_max_current(int max_current)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	/* Do Nothing */

	return result;
}

static int fj_bq_chg_hw_set_max_voltage(int max_voltage)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	/* Do Nothing */

	return result;
}

static int fj_bq_chg_hw_set_min_voltage(int min_voltage)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	/* Do Nothing */

	return result;
}

int fj_bq_chg_hw_set_charger_enable(int charger_enable)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	/* Do Nothing */

	return result;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_hw_set_charger_enable);

int fj_bq_chg_hw_set_voltage(int chg_voltage)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	result = fj_bq_chg_hw_set_max_voltage(chg_voltage);

	return result;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_hw_set_voltage);

int fj_bq_chg_hw_set_current(int chg_current)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	result = fj_bq_chg_hw_set_max_current(chg_current);

	return result;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_hw_set_current);

int fj_bq_chg_hw_set_vinmin(int chg_vinmin)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	result = fj_bq_chg_hw_set_min_voltage(chg_vinmin);

	return result;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_hw_set_vinmin);


int fj_bq_chg_hw_charge_start(unsigned int tmp_current)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	/* Do Nothing */

	return result;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_hw_charge_start);


int fj_bq_chg_hw_charge_stop(void)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	/* Do Nothing */

	return result;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_hw_charge_stop);

int fj_bq_chg_hw_init(struct bq24192_charger *bq)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	/* Do Nothing */

	return 0;
}
EXPORT_SYMBOL_GPL(fj_bq_chg_hw_init);


MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("BQ24192 battery charger driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:bq24192");
