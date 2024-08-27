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
#include <asm/system.h>

#include <mach/board_fj.h>
#include <linux/mfd/fj_charger.h>
#include <linux/mfd/fj_charger_local.h>
#include <linux/nonvolatile_common.h>
#include <linux/power/smb347-charger.h>
#include <linux/power/bq24192-charger.h>
#include <linux/max17050_battery.h>

//#define FJ_CHARGER_DBG
#undef FJ_CHARGER_DBG

#ifdef FJ_CHARGER_DBG
static int fj_charger_debug = 1;
#define FJ_CHARGER_DBGLOG(x, y...) \
			if (fj_charger_debug){ \
				printk(KERN_WARNING "[fj_chg_func] " x, ## y); \
			}
#else
#define FJ_CHARGER_DBGLOG(x, y...)
#endif /* FJ_CHARGER_DBG */

#define FJ_CHARGER_INFOLOG(x, y...)			printk(KERN_INFO "[fj_chg_func] " x, ## y)
#define FJ_CHARGER_WARNLOG(x, y...)			printk(KERN_WARNING "[fj_chg_func] " x, ## y)
#define FJ_CHARGER_ERRLOG(x, y...)			printk(KERN_ERR "[fj_chg_func] " x, ## y)

#define APNV_CHG_DESKTOP_FOLDER_CHARGE_I	(41042)

static int fj_chg_ic_flag = FJ_CHG_DEVICE_NONE;
static int mc_charger_enable = 0;
static int mc_chg_voltage = 0;
static int mc_chg_current = 0;
static int mc_chg_vinmin = 0;
static int chg_enable = 0;

int fj_get_chg_device(void)
{
	static int initialize = 0;

	if (!initialize) {
		if (((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV002)) ||
			((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV003)) ||
			((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV004)) ||
			((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV005)) ||
			((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV014))) {
			fj_chg_ic_flag = FJ_CHG_DEVICE_SMB;
		} else if ((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV001)) {
			if ((system_rev & 0x0F) <= (0x02)) {
				fj_chg_ic_flag = FJ_CHG_DEVICE_BQ;
			} else {
				fj_chg_ic_flag = FJ_CHG_DEVICE_SMB;
			}
		} else {
			fj_chg_ic_flag = FJ_CHG_DEVICE_NONE;
		}
		initialize = 1;
		FJ_CHARGER_DBGLOG("[%s] initialize end chg-ic=%d\n", __func__, fj_chg_ic_flag);
	}
	return fj_chg_ic_flag;
}
EXPORT_SYMBOL_GPL(fj_get_chg_device);

void fj_chg_usb_vbus_draw(unsigned int onoff)
{
	unsigned int mA = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (onoff != 0) {
		mA = FJ_CHG_USB_CURRENT;
	} else {
		mA = FJ_CHG_OFF_CURRENT;
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			fj_smb_chg_source_req(CHG_SOURCE_USB, mA);
			break;
		case FJ_CHG_DEVICE_BQ:
			fj_bq_chg_source_req(CHG_SOURCE_USB, mA);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}
}
EXPORT_SYMBOL_GPL(fj_chg_usb_vbus_draw);

void fj_chg_ac_vbus_draw(unsigned int onoff)
{
	unsigned int mA = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (onoff != 0) {
		mA = FJ_CHG_AC_CURRENT;
	} else {
		mA = FJ_CHG_OFF_CURRENT;
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			fj_smb_chg_source_req(CHG_SOURCE_AC, mA);
			break;
		case FJ_CHG_DEVICE_BQ:
			fj_bq_chg_source_req(CHG_SOURCE_AC, mA);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}
}
EXPORT_SYMBOL_GPL(fj_chg_ac_vbus_draw);

void fj_chg_holder_vbus_draw(unsigned int onoff)
{
	u8  val;
	int result;
	unsigned int mA = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (onoff != 0) {
		if (fj_chg_ic_flag == FJ_CHG_DEVICE_SMB) {
			mA = FJ_CHG_HOLDER_CURRENT;
		} else {
			mA = FJ_CHG_HOLDER_CURRENT2;
		}
	} else {
		mA = FJ_CHG_OFF_CURRENT;
	}

	if (mA) {
		/* Change desktop holder */
		result = get_nonvolatile(&val, APNV_CHG_DESKTOP_FOLDER_CHARGE_I, 1);
		if (result < 0) {
			FJ_CHARGER_ERRLOG("[%s] read nonvolatile failed:%d\n", __func__, result);
			val = 0x00;
		}

		/* Hispeed mode? */
		if (val == 0x01) {
			/* Change Hispeed current */
			mA = FJ_CHG_USB_CURRENT;
			FJ_CHARGER_INFOLOG("[%s] Change Current mA=%d NV=%d\n", __func__, mA, val);
		}
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			fj_smb_chg_source_req(CHG_SOURCE_HOLDER, mA);
			break;
		case FJ_CHG_DEVICE_BQ:
			fj_bq_chg_source_req(CHG_SOURCE_HOLDER, mA);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}
}
EXPORT_SYMBOL_GPL(fj_chg_holder_vbus_draw);

void fj_chg_mhl_vbus_draw(unsigned int onoff)
{
	unsigned int mA = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (onoff != 0) {
		mA = FJ_CHG_MHL_CURRENT;
	} else {
		mA = FJ_CHG_OFF_CURRENT;
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			fj_smb_chg_source_req(CHG_SOURCE_MHL, mA);
			break;
		case FJ_CHG_DEVICE_BQ:
			fj_bq_chg_source_req(CHG_SOURCE_MHL, mA);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}
}
EXPORT_SYMBOL_GPL(fj_chg_mhl_vbus_draw);

void fj_chg_other_vbus_draw(unsigned int onoff)
{
	unsigned int mA = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (onoff != 0) {
		mA = FJ_CHG_OTHER_CURRENT;
	} else {
		mA = FJ_CHG_OFF_CURRENT;
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			fj_smb_chg_source_req(CHG_SOURCE_AC, mA);
			break;
		case FJ_CHG_DEVICE_BQ:
			fj_bq_chg_source_req(CHG_SOURCE_AC, mA);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}
}
EXPORT_SYMBOL_GPL(fj_chg_other_vbus_draw);

void fj_chg_notify_error(unsigned int err)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			fj_smb_chg_notify_error(err);
			break;
		case FJ_CHG_DEVICE_BQ:
			fj_bq_chg_notify_error(err);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}
}
EXPORT_SYMBOL_GPL(fj_chg_notify_error);

void fj_chg_chgic_reset(void)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			fj_smb_chg_chgic_reset();
			break;
		case FJ_CHG_DEVICE_BQ:
			/* TBD */
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}
}
EXPORT_SYMBOL_GPL(fj_chg_chgic_reset);

void fj_chg_emergency_current_off(void)
{
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			fj_smb_chg_emergency_current_off();
			break;
		case FJ_CHG_DEVICE_BQ:
			fj_bq_chg_emergency_current_off();
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}
}
EXPORT_SYMBOL_GPL(fj_chg_emergency_current_off);

static int get_chg_adapter_voltage(char *buffer, struct kernel_param *kp)
{
	int result = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_adapter_voltage(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_adapter_voltage(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(adapter_voltage, NULL, get_chg_adapter_voltage, NULL, 0444);

static int get_chg_battery_voltage(char *buffer, struct kernel_param *kp)
{
	int result = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_battery_voltage(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_battery_voltage(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(battery_voltage, NULL, get_chg_battery_voltage, NULL, 0444);

static int get_chg_battery_temperature(char *buffer, struct kernel_param *kp)
{
	int result = 0;

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_battery_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_battery_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(battery_temperature, NULL, get_chg_battery_temperature, NULL, 0444);

static int get_chg_ambient_temperature(char *buffer, struct kernel_param *kp)
{
	int result = 0;

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_ambient_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_ambient_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(ambient_temperature, NULL, get_chg_ambient_temperature, NULL, 0444);

static int get_chg_receiver_temperature(char *buffer, struct kernel_param *kp)
{
	int result = 0;

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_receiver_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_receiver_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(receiver_temperature, NULL, get_chg_receiver_temperature, NULL, 0444);

static int get_chg_board_temperature(char *buffer, struct kernel_param *kp)
{
	int result = 0;

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_board_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_board_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(board_temperature, NULL, get_chg_board_temperature, NULL, 0444);

static int get_chg_charge_ic_temperature(char *buffer, struct kernel_param *kp)
{
	int result = 0;

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_charge_ic_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_charge_ic_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(charge_ic_temperature, NULL, get_chg_charge_ic_temperature, NULL, 0444);

static int get_chg_sensor_temperature(char *buffer, struct kernel_param *kp)
{
	int result = 0;

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_sensor_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_sensor_temperature(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(sensor_temperature, NULL, get_chg_sensor_temperature, NULL, 0444);

/* for mc command */
static int get_chg_battery_present(char *buffer, struct kernel_param *kp)
{
	int result = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_get_battery_present(buffer, kp);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_get_battery_present(buffer, kp);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(battery_present, NULL, get_chg_battery_present, NULL, 0444);

/* for mc command */
static int set_charger_enable(const char *val, struct kernel_param *kp)
{
	int result = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	result = param_set_int(val, kp);
	if (result) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, result);
		return result;
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_set_charger_enable(val, kp, mc_charger_enable);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_set_charger_enable(val, kp, mc_charger_enable);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(mc_charger_enable, set_charger_enable, param_get_uint, &mc_charger_enable, 0644);

/* for mc command */
static int set_chg_voltage(const char *val, struct kernel_param *kp)
{
	int result = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	result = param_set_int(val, kp);
	if (result) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, result);
		return result;
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_set_chg_voltage(val, kp, mc_chg_voltage);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_set_chg_voltage(val, kp, mc_chg_voltage);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(mc_chg_voltage, set_chg_voltage, param_get_uint, &mc_chg_voltage, 0644);

/* for mc command */
static int set_chg_current(const char *val, struct kernel_param *kp)
{
	int result = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	result = param_set_int(val, kp);
	if (result) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, result);
		return result;
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_set_chg_current(val, kp, mc_chg_current);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_set_chg_current(val, kp, mc_chg_current);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(mc_chg_current, set_chg_current, param_get_uint, &mc_chg_current, 0644);

static int set_fj_chg_enable(const char *val, struct kernel_param *kp)
{
	int result = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	result = param_set_int(val, kp);
	if (result) {
		FJ_CHARGER_ERRLOG("[%s] error setting value %d\n",__func__, result);
		return result;
	}

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_set_fj_chg_enable(val, kp, chg_enable);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_set_fj_chg_enable(val, kp, chg_enable);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(chg_enable, set_fj_chg_enable, param_get_uint,
					&chg_enable, 0644);

/* for mc command */
static int set_chg_vinmin(const char *val, struct kernel_param *kp)
{
	int result = 0;
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch (fj_chg_ic_flag) {
		case FJ_CHG_DEVICE_SMB:
			result = fj_smb_chg_set_chg_vinmin(val, kp, mc_chg_vinmin);
			break;
		case FJ_CHG_DEVICE_BQ:
			result = fj_bq_chg_set_chg_vinmin(val, kp, mc_chg_vinmin);
			break;
		case FJ_CHG_DEVICE_NONE:
		default:
			FJ_CHARGER_ERRLOG("[%s] chg-ic device error\n", __func__);
			break;
	}

	return result;
}
module_param_call(mc_chg_vinmin, set_chg_vinmin, param_get_uint, &mc_chg_vinmin, 0644);

MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("charger driver");
MODULE_LICENSE("GPL");
