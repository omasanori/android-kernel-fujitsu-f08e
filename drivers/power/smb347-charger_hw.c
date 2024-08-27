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
#include <linux/power/smb347-charger.h>
#include <linux/power/smb347-charger_hw.h>
#include <linux/seq_file.h>
#include <asm/system_info.h>

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
static int fj_smb_charger_debug = 1;
#define FJ_CHARGER_DBGLOG(x, y...) \
			if (fj_smb_charger_debug){ \
				printk(KERN_WARNING "[smb347_chg_hw] " x, ## y); \
			}
#else
#define FJ_CHARGER_DBGLOG(x, y...)
#endif /* FJ_CHARGER_DBG */

#define FJ_CHARGER_INFOLOG(x, y...)			printk(KERN_INFO "[smb347_chg_hw] " x, ## y)
#define FJ_CHARGER_WARNLOG(x, y...)			printk(KERN_WARNING "[smb347_chg_hw] " x, ## y)
#define FJ_CHARGER_ERRLOG(x, y...)			printk(KERN_ERR "[smb347_chg_hw] " x, ## y)

//hw access
#define FJ_SMB_BIT_ENC(reg, bit, param)				(SMB_##reg##_##bit##_##param)
#define FJ_SMB_BIT_ENC_MASK(reg, bit)				(SMB_##reg##_##bit##_MASK)

#define FJ_SMB_IC_READ_BYTE(reg)					fj_smb_chg_hw_reg_read_byte(SMB_##reg##_REG);
#define FJ_SMB_IC_WRITE_BYTE(reg, data)				fj_smb_chg_hw_reg_write_byte(SMB_##reg##_REG, data);
#define FJ_SMB_IC_MASK_WRITE_BYTE(reg, mask, data)	fj_smb_chg_hw_reg_mask_write_byte(SMB_##reg##_REG, mask, data);

#define APNV_CHG_ADJ_VMAX_I					(41041)

#define FJ_SMB_CHG_CURR_PARAM_COLS			(10)

struct fj_smb_chg_current_param_table {
	int curr[FJ_SMB_CHG_CURR_PARAM_COLS];
	int value[FJ_SMB_CHG_CURR_PARAM_COLS];
	int cols;
};

static struct fj_smb_chg_current_param_table fj_smb_chg_current_cnv_table = {
	.curr	= { 300,  500,  700,  900, 1200,
				   1500, 1800, 2000, 2200, 2500
				},
	.value		= {0x00, 0x01, 0x02, 0x03, 0x04,
				   0x05, 0x06, 0x07, 0x08, 0x09
				},
	.cols		= FJ_SMB_CHG_CURR_PARAM_COLS,
};

static int fj_smb347_chg_aicl_val_1 = 0;
static int fj_smb347_chg_aicl_val_2 = 0;
static int fj_smb_chg_hw_charging_type = FJ_SMB_CHG_TYPE_DISCHARGING;
static int fj_smb_chg_hw_charging_current = 0;

extern struct smb347_charger *the_smb;
extern int fj_smb_chg_435V_flag;

/* internal function prototype */
static int fj_smb_chg_hw_reg_read_byte(u8 reg);
static int fj_smb_chg_hw_reg_write_byte(u8 reg, u8 data);
static int fj_smb_chg_hw_reg_mask_write_byte(u8 reg, u8 mask, u8 data);

static int fj_smb_chg_hw_set_max_current(int max_current);
static int fj_smb_chg_hw_set_max_voltage(int max_voltage);
static int fj_smb_chg_hw_set_min_voltage(int min_voltage);


static int fj_smb_chg_hw_reg_read_byte(u8 reg)
{
	int result = 0;
	int i = 0;

	if (the_smb->client == NULL) {
		FJ_CHARGER_WARNLOG("[%s] client pointer is NULL\n", __func__);
		return -1;
	}

	FJ_CHARGER_DBGLOG("[%s] in. reg=0x%x\n", __func__, reg);

	result = i2c_smbus_read_byte_data(the_smb->client, reg);

	if (result < 0) {
		/* fail safe */
		FJ_CHARGER_ERRLOG("[%s] i2c read err : reg = 0x%x, result = %d \n",
							__func__, reg, result);
		for (i = 0;i < 5;i++) {
			result = i2c_smbus_read_byte_data(the_smb->client, reg);
			if (result >= 0) {
				break;
			}
			msleep(100);
		}
		if (result < 0) {
			FJ_CHARGER_ERRLOG("[%s] i2c read retry err : reg = 0x%x, result = %d \n",
								__func__, reg, result);
		}
	}

	FJ_CHARGER_DBGLOG("[%s] read data=0x%x\n", __func__, result);
	return result;
}

static int fj_smb_chg_hw_reg_write_byte(u8 reg, u8 data)
{
	int result = 0;
	int i = 0;

	if (the_smb->client == NULL) {
		FJ_CHARGER_WARNLOG("[%s] client pointer is NULL\n", __func__);
		return -1;
	}

	FJ_CHARGER_DBGLOG("[%s] in. reg=0x%x, data=0x%x\n", __func__, reg, data);

	result = i2c_smbus_write_byte_data(the_smb->client, reg, data);

	if (result < 0) {
		/* fail safe */
		FJ_CHARGER_ERRLOG("[%s] i2c write err : reg = 0x%x, result = %d \n",
							__func__, reg, result);
		for (i = 0;i < 5;i++) {
			result = i2c_smbus_write_byte_data(the_smb->client, reg, data);
			if (result >= 0) {
				break;
			}
			msleep(100);
		}
		if (result < 0) {
			FJ_CHARGER_ERRLOG("[%s] i2c write retry err : reg = 0x%x, result = %d \n",
								__func__, reg, result);
		}
	}
	return result;
}

static int fj_smb_chg_hw_reg_mask_write_byte(u8 reg, u8 mask, u8 data)
{
	int tmp_data = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	tmp_data = fj_smb_chg_hw_reg_read_byte(reg);
	if (tmp_data < 0) 
		return tmp_data;

	tmp_data = ((tmp_data & ~(mask)) | ((mask)&(data)));
	FJ_CHARGER_DBGLOG("[%s] write mask=0x%x, write data=0x%x\n", __func__, mask, data);

	return fj_smb_chg_hw_reg_write_byte(reg, (u8)tmp_data);
}

#define SMB_CHG_SET_CHG_MAX_CURRENT_MIN			0x00
#define SMB_CHG_SET_CHG_MAX_CURRENT_MAX			0x09
static int fj_smb_chg_hw_set_max_current(int max_current)
{
	u8  temp = 0;
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (max_current < SMB_CHG_SET_CHG_MAX_CURRENT_MIN
			|| max_current > SMB_CHG_SET_CHG_MAX_CURRENT_MAX) {
		FJ_CHARGER_ERRLOG("[%s] parameter error:0x%x\n", __func__, max_current);
		return -EINVAL;
	}

	temp = (max_current << 4) | max_current;

	do {
		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(INPUT_CURRENT_LIMIT,
					FJ_SMB_BIT_ENC_MASK(INPUT_CURRENT_LIMIT, DCIN_LIMIT) |
					FJ_SMB_BIT_ENC_MASK(INPUT_CURRENT_LIMIT, USBIN_LIMIT),
					temp);
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, USB_5_1_MODE) |
					FJ_SMB_BIT_ENC_MASK(CMD_B, USB_HC_MODE),
					FJ_SMB_BIT_ENC(CMD_B, USB_5_1_MODE, USB5_OR_USB9) |
					FJ_SMB_BIT_ENC(CMD_B, USB_HC_MODE, HIGH_CURRENT_MODE));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(STAT_TIMER,
					FJ_SMB_BIT_ENC_MASK(STAT_TIMER, COMP_CHG_TIMEOUT),
					FJ_SMB_BIT_ENC(STAT_TIMER, COMP_CHG_TIMEOUT, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
	} while(0);

	return result;
}

#define SMB_CHG_SET_CHG_MAX_VOLTAGE_MIN			0x00
#define SMB_CHG_SET_CHG_MAX_VOLTAGE_MAX			0x32
static int fj_smb_chg_hw_set_max_voltage(int max_voltage)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if ((max_voltage < SMB_CHG_SET_CHG_MAX_VOLTAGE_MIN) ||
		(max_voltage > SMB_CHG_SET_CHG_MAX_VOLTAGE_MAX)) {
		FJ_CHARGER_ERRLOG("[%s] parameter error:0x%x\n", __func__, max_voltage);
		return -EINVAL;
	}

	do {
		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(FLOAT_VOLTAGE,
					FJ_SMB_BIT_ENC_MASK(FLOAT_VOLTAGE, FLOAT_VOLTAGE),
					max_voltage);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
	} while(0);

	return result;
}

static int fj_smb_chg_hw_set_min_voltage(int min_voltage)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	/* Do Nothing */

	return result;
}

int fj_smb_chg_hw_set_charger_enable(int charger_enable)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	switch(charger_enable) {
		case 0: /* charge OFF */
			do {
				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
							FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
							FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, DISABLED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
							FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, ENABLED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
							FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
							FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, ENABLED));
				msleep(20);
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
							FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
			} while(0);
			break;
		case 1: /* charge ON */
			do {
				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
							FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
							FJ_SMB_BIT_ENC_MASK(CMD_B, USB_5_1_MODE) |
							FJ_SMB_BIT_ENC_MASK(CMD_B, USB_HC_MODE),
							FJ_SMB_BIT_ENC(CMD_B, USB_5_1_MODE, USB5_OR_USB9) |
							FJ_SMB_BIT_ENC(CMD_B, USB_HC_MODE, HIGH_CURRENT_MODE));
				msleep(20);
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CHG_CTRL,
							FJ_SMB_BIT_ENC_MASK(CHG_CTRL, CURRENT_TERM),
							FJ_SMB_BIT_ENC(CHG_CTRL, CURRENT_TERM, NOT_ALLOWED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(VARIOUS_FUNCTIONS,
							FJ_SMB_BIT_ENC_MASK(VARIOUS_FUNCTIONS, OPTICHG),
							FJ_SMB_BIT_ENC(VARIOUS_FUNCTIONS, OPTICHG, DISABLED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
							FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
							FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, DISABLED));
				msleep(20);
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
							FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, DISABLED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
							FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, ENABLED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
							FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
			} while(0);
			break;
		case 2: /* power path */
			do {
				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
							FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
							FJ_SMB_BIT_ENC_MASK(CMD_B, USB_5_1_MODE) |
							FJ_SMB_BIT_ENC_MASK(CMD_B, USB_HC_MODE),
							FJ_SMB_BIT_ENC(CMD_B, USB_5_1_MODE, USB5_OR_USB9) |
							FJ_SMB_BIT_ENC(CMD_B, USB_HC_MODE, HIGH_CURRENT_MODE));
				msleep(20);
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(VARIOUS_FUNCTIONS,
							FJ_SMB_BIT_ENC_MASK(VARIOUS_FUNCTIONS, OPTICHG),
							FJ_SMB_BIT_ENC(VARIOUS_FUNCTIONS, OPTICHG, DISABLED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
							FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
							FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, DISABLED));
				msleep(20);
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
							FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, DISABLED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
							FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, DISABLED));
				if (result < 0) break;

				result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
							FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
							FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
			} while(0);
			break;
		default:
			FJ_CHARGER_ERRLOG("[%s] parameter error:%d\n", __func__, charger_enable);
			result = -1;
			break;
	}

	if (result < 0)
		FJ_CHARGER_ERRLOG("[%s] error:%d\n", __func__, result);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_set_charger_enable);

int fj_smb_chg_hw_set_voltage(int chg_voltage)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	result = fj_smb_chg_hw_set_max_voltage(chg_voltage);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_set_voltage);

int fj_smb_chg_hw_set_current(int chg_current)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	result = fj_smb_chg_hw_set_max_current(chg_current);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_set_current);

int fj_smb_chg_hw_set_vinmin(int chg_vinmin)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	result = fj_smb_chg_hw_set_min_voltage(chg_vinmin);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_set_vinmin);

int fj_smb_chg_hw_chgic_reset(void)
{
	int result = 0;
	
	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	do {
		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, POWER_ON_RESET),
					FJ_SMB_BIT_ENC(CMD_B, POWER_ON_RESET, RESET));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
		if (result < 0) break;
	} while(0);


	return 0;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_chgic_reset);

int fj_smb_chg_hw_aicl_restart_check(void)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	if (fj_smb347_chg_aicl_val_1 != fj_smb347_chg_aicl_val_2) {
		result = 1;
	} else {
		result = 0;
	}

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_aicl_restart_check);

int fj_smb_chg_hw_aicl_restart(void)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	do {
		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(VARIOUS_FUNCTIONS,
					FJ_SMB_BIT_ENC_MASK(VARIOUS_FUNCTIONS, OPTICHG),
					FJ_SMB_BIT_ENC(VARIOUS_FUNCTIONS, OPTICHG, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(VARIOUS_FUNCTIONS,
					FJ_SMB_BIT_ENC_MASK(VARIOUS_FUNCTIONS, OPTICHG),
					FJ_SMB_BIT_ENC(VARIOUS_FUNCTIONS, OPTICHG, ENABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
	} while(0);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_aicl_restart);

int fj_smb_chg_hw_aicl_error_check_start(void)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	do {
		result = fj_smb_chg_hw_set_max_current(fj_smb347_chg_aicl_val_2);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, DISABLED));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
					FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, ENABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));

	} while(0);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_aicl_error_check_start);

int fj_smb_chg_hw_aicl_check(void)
{
	int result = -1;
	int int_sts_d = 0;
	int sts_e = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	do {
		int_sts_d = FJ_SMB_IC_READ_BYTE(INTERRUPT_STATUS_D);
		if (int_sts_d < 0) {
			FJ_CHARGER_ERRLOG("[%s] INTERRUPT_STATUS_C read err[%d]\n",__func__, int_sts_d);
			break;
		}

		sts_e = FJ_SMB_IC_READ_BYTE(STATUS_E);
		if (sts_e < 0) {
			FJ_CHARGER_ERRLOG("[%s] INTERRUPT_STATUS_E read err[%d]\n",__func__, sts_e);
			break;
		}

		if (!(int_sts_d & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_D, AICL_COMPLETE, STATUS))) {
			break;
		}

		if (!((sts_e & FJ_SMB_BIT_ENC_MASK(STATUS_E, AICL_STATUS)) == 
						FJ_SMB_BIT_ENC(STATUS_E, AICL_STATUS, COMPLETED))) {
			break;
		}
		if (fj_smb347_chg_aicl_val_1 == 0xFF) {
			fj_smb347_chg_aicl_val_1 = (sts_e & FJ_SMB_BIT_ENC_MASK(STATUS_E, AICL_RESULTS));
		}
		fj_smb347_chg_aicl_val_2 = (sts_e & FJ_SMB_BIT_ENC_MASK(STATUS_E, AICL_RESULTS));

		if (fj_smb347_chg_aicl_val_2 >= FJ_SMB_BIT_ENC(STATUS_E, AICL_RESULTS, 1200mA)) {
			result = FJ_SMB_CHG_IC_AICL_BIG;
		} else {
			result = FJ_SMB_CHG_IC_AICL_SMALL;
		}
	} while(0);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_aicl_check);

int fj_smb_chg_hw_get_charge_ic_error(int *ic_err, unsigned long charger_src)
{
	int result = 0;
	int int_sts_c = 0;
	int int_sts_e = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	*ic_err = 0;

FJ_CHARGER_DBGLOG("[%s] charger_src = 0x%x\n", __func__, (int)charger_src);
	int_sts_c = FJ_SMB_IC_READ_BYTE(INTERRUPT_STATUS_C);
	if (int_sts_c < 0) {
		FJ_CHARGER_ERRLOG("[%s] INTERRUPT_STATUS_C read err[%d]\n",__func__, int_sts_c);
		result = -1;
		int_sts_c = 0;
	}

	int_sts_e = FJ_SMB_IC_READ_BYTE(INTERRUPT_STATUS_E);
	if (int_sts_e < 0) {
		FJ_CHARGER_ERRLOG("[%s] INTERRUPT_STATUS_E read err[%d]\n",__func__, int_sts_e);
		result = -1;
		int_sts_e = 0;
	}

	if (int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, DCIN_OVER_VOLTAGE, STATUS)) {
		*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_DCIN_OV;
	}

	if ((int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, DCIN_UNDER_VOLTAGE, STATUS)) &&
		(charger_src & FJ_CHG_SOURCE_HOLDER)) {
		*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_DCIN_UV;
	}

	if ((((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV002)) && ((system_rev & 0x0F) <= 0x01)) ||
	    (((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV004)) && ((system_rev & 0x0F) == 0x00))) {
		if (int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, DCIN_OVER_VOLTAGE, STATUS)) {
			*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_USBIN_OV;
		}

		if ((int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, DCIN_UNDER_VOLTAGE, STATUS)) &&
			(charger_src & FJ_CHG_SOURCE_USB_PORT)) {
			*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_USBIN_UV;
		}
	} else {
		if (int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, USBIN_OVER_VOLTAGE, STATUS)) {
			*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_USBIN_OV;
		}

		if ((int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, USBIN_UNDER_VOLTAGE, STATUS)) &&
			(charger_src & FJ_CHG_SOURCE_USB_PORT)) {
			*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_USBIN_UV;
		}
	}

	if (int_sts_c & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_C, INTERNAL_TEMP_LIMIT, STATUS)) {
		*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_TEMP_LIMIT;
	}

	if (!(*ic_err)) {
		if ((int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, DCIN_UNDER_VOLTAGE, IRQ)) &&
			(!(int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, DCIN_UNDER_VOLTAGE, STATUS))) &&
			(charger_src & FJ_CHG_SOURCE_HOLDER)) {
			*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_RESTART;
			fj_smb_chg_hw_charging_type = FJ_SMB_CHG_TYPE_DISCHARGING;
			fj_smb_chg_hw_charging_current = 0;
		}

		if ((((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV002)) && ((system_rev & 0x0F) <= 0x01)) ||
		    (((system_rev & 0xF0) == (FJ_CHG_DEVICE_TYPE_FJDEV004)) && ((system_rev & 0x0F) == 0x00))) {
			if ((int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, DCIN_UNDER_VOLTAGE, IRQ)) &&
				(!(int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, DCIN_UNDER_VOLTAGE, STATUS))) &&
				(charger_src & FJ_CHG_SOURCE_USB_PORT)) {
				*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_RESTART;
				fj_smb_chg_hw_charging_type = FJ_SMB_CHG_TYPE_DISCHARGING;
				fj_smb_chg_hw_charging_current = 0;
			}
		} else {
			if ((int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, USBIN_UNDER_VOLTAGE, IRQ)) &&
				(!(int_sts_e & FJ_SMB_BIT_ENC(INTERRUPT_STATUS_E, USBIN_UNDER_VOLTAGE, STATUS))) &&
				(charger_src & FJ_CHG_SOURCE_USB_PORT)) {
				*ic_err |= FJ_SMB_CHG_IC_ERROR_BIT_RESTART;
				fj_smb_chg_hw_charging_type = FJ_SMB_CHG_TYPE_DISCHARGING;
				fj_smb_chg_hw_charging_current = 0;
			}
		}
	}
FJ_CHARGER_DBGLOG("[%s] ic_err = 0x%x\n", __func__, (int)*ic_err);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_get_charge_ic_error);

int fj_smb_chg_hw_charge_start(unsigned int tmp_current)
{
	int result = 0;
	int cnt = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	for (cnt = 0;cnt < fj_smb_chg_current_cnv_table.cols;cnt++) {
		if (tmp_current == fj_smb_chg_current_cnv_table.curr[cnt]) {
			break;
		}
	}

	if (cnt == fj_smb_chg_current_cnv_table.cols) {
		cnt = 0x01;	/* 500mA */
	}

	do {
		if ((fj_smb_chg_hw_charging_current == cnt) &&
			(fj_smb_chg_hw_charging_type == FJ_SMB_CHG_TYPE_CHARGING)) {
			 break;
		} else {
			fj_smb_chg_hw_charging_type = FJ_SMB_CHG_TYPE_CHARGING;
			fj_smb_chg_hw_charging_current = cnt;
		}

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(STAT_TIMER,
					FJ_SMB_BIT_ENC_MASK(STAT_TIMER, COMP_CHG_TIMEOUT),
					FJ_SMB_BIT_ENC(STAT_TIMER, COMP_CHG_TIMEOUT, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(PIN_ENABLE_CTRL,
					FJ_SMB_BIT_ENC_MASK(PIN_ENABLE_CTRL, CHG_ERROR) |
					FJ_SMB_BIT_ENC_MASK(PIN_ENABLE_CTRL, APSD_DONE),
					FJ_SMB_BIT_ENC(PIN_ENABLE_CTRL, CHG_ERROR, NOT_TRIGGER_IRQ) |
					FJ_SMB_BIT_ENC(PIN_ENABLE_CTRL, APSD_DONE, NOT_TRIGGER_IRQ));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(FAULT_INTERRUPT,
					FJ_SMB_BIT_ENC_MASK(FAULT_INTERRUPT, ALL),
					FJ_SMB_BIT_ENC(FAULT_INTERRUPT, USB_OVER_VOLTAGE, DETECT) |
					FJ_SMB_BIT_ENC(FAULT_INTERRUPT, USB_UNDER_VOLTAGE, DETECT) |
					FJ_SMB_BIT_ENC(FAULT_INTERRUPT, AICL_COMPLETE, DETECT) |
					FJ_SMB_BIT_ENC(FAULT_INTERRUPT, INTERNAL_OVER_TEMP, DETECT));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(STATUS_INTERRUPT,
					FJ_SMB_BIT_ENC_MASK(STATUS_INTERRUPT, ALL),
					0x00);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(THERM_SYSTEM_CTRL_A,
					FJ_SMB_BIT_ENC_MASK(THERM_SYSTEM_CTRL_A, THERM_MONITOR),
					FJ_SMB_BIT_ENC(THERM_SYSTEM_CTRL_A, THERM_MONITOR, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
					FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CHG_CTRL,
					FJ_SMB_BIT_ENC_MASK(CHG_CTRL, CURRENT_TERM),
					FJ_SMB_BIT_ENC(CHG_CTRL, CURRENT_TERM, NOT_ALLOWED));
		if (result < 0) break;

		msleep(100);

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
					FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, ENABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, DISABLED));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
		if (result < 0) break;

		result = fj_smb_chg_hw_set_max_voltage((the_smb->vmax_adj)-(the_smb->vmax_age_adj));
		if (result < 0) break;

		result = fj_smb_chg_hw_set_max_current(cnt);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, USB_5_1_MODE) |
					FJ_SMB_BIT_ENC_MASK(CMD_B, USB_HC_MODE),
					FJ_SMB_BIT_ENC(CMD_B, USB_5_1_MODE, USB5_OR_USB9) |
					FJ_SMB_BIT_ENC(CMD_B, USB_HC_MODE, HIGH_CURRENT_MODE));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CHG_CTRL,
					FJ_SMB_BIT_ENC_MASK(CHG_CTRL, CURRENT_TERM),
					FJ_SMB_BIT_ENC(CHG_CTRL, CURRENT_TERM, NOT_ALLOWED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(VARIOUS_FUNCTIONS,
					FJ_SMB_BIT_ENC_MASK(VARIOUS_FUNCTIONS, OPTICHG),
					FJ_SMB_BIT_ENC(VARIOUS_FUNCTIONS, OPTICHG, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(VARIOUS_FUNCTIONS,
					FJ_SMB_BIT_ENC_MASK(VARIOUS_FUNCTIONS, OPTICHG),
					FJ_SMB_BIT_ENC(VARIOUS_FUNCTIONS, OPTICHG, ENABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, DISABLED));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
					FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, ENABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
	} while(0);

	fj_smb347_chg_aicl_val_1 = 0xFF;

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_charge_start);

int fj_smb_chg_hw_charge_stop(unsigned long charger_src)
{
	int result = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	do {
		fj_smb_chg_hw_charging_type = FJ_SMB_CHG_TYPE_DISCHARGING;
		fj_smb_chg_hw_charging_current = 0;

		if (!charger_src) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
					FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, ENABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, ENABLED));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
	} while(0);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_charge_stop);

int fj_smb_chg_hw_charge_powerpath(unsigned int tmp_current)
{
	int result = 0;
	int cnt = 0;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);
	for (cnt = 0;cnt < fj_smb_chg_current_cnv_table.cols;cnt++) {
		if (tmp_current == fj_smb_chg_current_cnv_table.curr[cnt]) {
			break;
		}
	}

	if (cnt == fj_smb_chg_current_cnv_table.cols) {
		cnt = 0x01;	/* 500mA */
	}

	do {
		if ((fj_smb_chg_hw_charging_current == cnt) &&
			(fj_smb_chg_hw_charging_type == FJ_SMB_CHG_TYPE_POWERPATH)) {
			 break;
		} else {
			fj_smb_chg_hw_charging_type = FJ_SMB_CHG_TYPE_POWERPATH;
			fj_smb_chg_hw_charging_current = cnt;
		}

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(STAT_TIMER,
					FJ_SMB_BIT_ENC_MASK(STAT_TIMER, COMP_CHG_TIMEOUT),
					FJ_SMB_BIT_ENC(STAT_TIMER, COMP_CHG_TIMEOUT, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(PIN_ENABLE_CTRL,
					FJ_SMB_BIT_ENC_MASK(PIN_ENABLE_CTRL, CHG_ERROR) |
					FJ_SMB_BIT_ENC_MASK(PIN_ENABLE_CTRL, APSD_DONE),
					FJ_SMB_BIT_ENC(PIN_ENABLE_CTRL, CHG_ERROR, NOT_TRIGGER_IRQ) |
					FJ_SMB_BIT_ENC(PIN_ENABLE_CTRL, APSD_DONE, NOT_TRIGGER_IRQ));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(FAULT_INTERRUPT,
					FJ_SMB_BIT_ENC_MASK(FAULT_INTERRUPT, ALL),
					FJ_SMB_BIT_ENC(FAULT_INTERRUPT, USB_OVER_VOLTAGE, DETECT) |
					FJ_SMB_BIT_ENC(FAULT_INTERRUPT, USB_UNDER_VOLTAGE, DETECT) |
					FJ_SMB_BIT_ENC(FAULT_INTERRUPT, INTERNAL_OVER_TEMP, DETECT));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(STATUS_INTERRUPT,
					FJ_SMB_BIT_ENC_MASK(STATUS_INTERRUPT, ALL),
					0x00);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(THERM_SYSTEM_CTRL_A,
					FJ_SMB_BIT_ENC_MASK(THERM_SYSTEM_CTRL_A, THERM_MONITOR),
					FJ_SMB_BIT_ENC(THERM_SYSTEM_CTRL_A, THERM_MONITOR, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
					FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CHG_CTRL,
					FJ_SMB_BIT_ENC_MASK(CHG_CTRL, CURRENT_TERM),
					FJ_SMB_BIT_ENC(CHG_CTRL, CURRENT_TERM, NOT_ALLOWED));
		if (result < 0) break;

		msleep(100);

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
					FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, ENABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, DISABLED));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
		if (result < 0) break;

		result = fj_smb_chg_hw_set_max_voltage(the_smb->vmax_adj);
		if (result < 0) break;

		result = fj_smb_chg_hw_set_max_current(cnt);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, ALLOW));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, USB_5_1_MODE) |
					FJ_SMB_BIT_ENC_MASK(CMD_B, USB_HC_MODE),
					FJ_SMB_BIT_ENC(CMD_B, USB_5_1_MODE, USB5_OR_USB9) |
					FJ_SMB_BIT_ENC(CMD_B, USB_HC_MODE, HIGH_CURRENT_MODE));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CHG_CTRL,
					FJ_SMB_BIT_ENC_MASK(CHG_CTRL, CURRENT_TERM),
					FJ_SMB_BIT_ENC(CHG_CTRL, CURRENT_TERM, NOT_ALLOWED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(VARIOUS_FUNCTIONS,
					FJ_SMB_BIT_ENC_MASK(VARIOUS_FUNCTIONS, OPTICHG),
					FJ_SMB_BIT_ENC(VARIOUS_FUNCTIONS, OPTICHG, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(VARIOUS_FUNCTIONS,
					FJ_SMB_BIT_ENC_MASK(VARIOUS_FUNCTIONS, OPTICHG),
					FJ_SMB_BIT_ENC(VARIOUS_FUNCTIONS, OPTICHG, ENABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_B,
					FJ_SMB_BIT_ENC_MASK(CMD_B, DCIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_B, DCIN_SUSPEND_MODE, DISABLED));
		msleep(20);
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, USBIN_SUSPEND_MODE),
					FJ_SMB_BIT_ENC(CMD_A, USBIN_SUSPEND_MODE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, CHARGING_ENABLE),
					FJ_SMB_BIT_ENC(CMD_A, CHARGING_ENABLE, DISABLED));
		if (result < 0) break;

		result = FJ_SMB_IC_MASK_WRITE_BYTE(CMD_A,
					FJ_SMB_BIT_ENC_MASK(CMD_A, WRITE_PERMISSION),
					FJ_SMB_BIT_ENC(CMD_A, WRITE_PERMISSION, PREVENT));
	} while(0);

	return result;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_charge_powerpath);

int fj_smb_chg_hw_init(struct smb347_charger *smb)
{
	int result = 0;
	uint8_t chg_vol_adj = 0x00;

	FJ_CHARGER_DBGLOG("[%s] in\n", __func__);

	result = get_nonvolatile(&chg_vol_adj, APNV_CHG_ADJ_VMAX_I, 1);
	FJ_CHARGER_DBGLOG("[%s] chg_vol_adj = 0x%x\n",__func__, chg_vol_adj);
	if ((result < 0) ||
		(chg_vol_adj < SMB_FLOAT_VOLTAGE_FLOAT_VOLTAGE_4_04V) ||
		(chg_vol_adj > SMB_FLOAT_VOLTAGE_FLOAT_VOLTAGE_4_50V)) {
		chg_vol_adj = SMB_FLOAT_VOLTAGE_FLOAT_VOLTAGE_4_36V;
	}

	if (!fj_smb_chg_435V_flag)
		chg_vol_adj -= 0x07;

	smb->vmax_adj = chg_vol_adj;

	FJ_CHARGER_DBGLOG("[%s] out\n",__func__);

	return 0;
}
EXPORT_SYMBOL_GPL(fj_smb_chg_hw_init);


MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("SMB347 battery charger driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:smb347");
