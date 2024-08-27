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
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/

#ifndef SMB347_CHARGER_H
#define SMB347_CHARGER_H

#include <linux/types.h>
#include <linux/power_supply.h>

enum {
	/* use the default compensation method */
	SMB347_SOFT_TEMP_COMPENSATE_DEFAULT = -1,

	SMB347_SOFT_TEMP_COMPENSATE_NONE,
	SMB347_SOFT_TEMP_COMPENSATE_CURRENT,
	SMB347_SOFT_TEMP_COMPENSATE_VOLTAGE,
};

/* Use default factory programmed value for hard/soft temperature limit */
#define SMB347_TEMP_USE_DEFAULT		-273

/*
 * Charging enable can be controlled by software (via i2c) by
 * smb347-charger driver or by EN pin (active low/high).
 */
enum smb347_chg_enable {
	SMB347_CHG_ENABLE_SW,
	SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	SMB347_CHG_ENABLE_PIN_ACTIVE_HIGH,
};

/**
 * struct smb347_charger_platform_data - platform data for SMB347 charger
 * @battery_info: Information about the battery
 * @max_charge_current: maximum current (in uA) the battery can be charged
 * @max_charge_voltage: maximum voltage (in uV) the battery can be charged
 * @pre_charge_current: current (in uA) to use in pre-charging phase
 * @termination_current: current (in uA) used to determine when the
 *			 charging cycle terminates
 * @pre_to_fast_voltage: voltage (in uV) treshold used for transitioning to
 *			 pre-charge to fast charge mode
 * @mains_current_limit: maximum input current drawn from AC/DC input (in uA)
 * @usb_hc_current_limit: maximum input high current (in uA) drawn from USB
 *			  input
 * @chip_temp_threshold: die temperature where device starts limiting charge
 *			 current [%100 - %130] (in degree C)
 * @soft_cold_temp_limit: soft cold temperature limit [%0 - %15] (in degree C),
 *			  granularity is 5 deg C.
 * @soft_hot_temp_limit: soft hot temperature limit [%40 - %55] (in degree  C),
 *			 granularity is 5 deg C.
 * @hard_cold_temp_limit: hard cold temperature limit [%-5 - %10] (in degree C),
 *			  granularity is 5 deg C.
 * @hard_hot_temp_limit: hard hot temperature limit [%50 - %65] (in degree C),
 *			 granularity is 5 deg C.
 * @suspend_on_hard_temp_limit: suspend charging when hard limit is hit
 * @soft_temp_limit_compensation: compensation method when soft temperature
 *				  limit is hit
 * @charge_current_compensation: current (in uA) for charging compensation
 *				 current when temperature hits soft limits
 * @use_mains: AC/DC input can be used
 * @use_usb: USB input can be used
 * @use_usb_otg: USB OTG output can be used (not implemented yet)
 * @irq_gpio: GPIO number used for interrupts (%-1 if not used)
 * @enable_control: how charging enable/disable is controlled
 *		    (driver/pin controls)
 *
 * @use_main, @use_usb, and @use_usb_otg are means to enable/disable
 * hardware support for these. This is useful when we want to have for
 * example OTG charging controlled via OTG transceiver driver and not by
 * the SMB347 hardware.
 *
 * Hard and soft temperature limit values are given as described in the
 * device data sheet and assuming NTC beta value is %3750. Even if this is
 * not the case, these values should be used. They can be mapped to the
 * corresponding NTC beta values with the help of table %2 in the data
 * sheet. So for example if NTC beta is %3375 and we want to program hard
 * hot limit to be %53 deg C, @hard_hot_temp_limit should be set to %50.
 *
 * If zero value is given in any of the current and voltage values, the
 * factory programmed default will be used. For soft/hard temperature
 * values, pass in %SMB347_TEMP_USE_DEFAULT instead.
 */
struct smb347_charger_platform_data {
	struct power_supply_info battery_info;
	unsigned int	max_charge_current;
	unsigned int	max_charge_voltage;
	unsigned int	pre_charge_current;
	unsigned int	termination_current;
	unsigned int	pre_to_fast_voltage;
	unsigned int	mains_current_limit;
	unsigned int	usb_hc_current_limit;
	unsigned int	chip_temp_threshold;
	int		soft_cold_temp_limit;
	int		soft_hot_temp_limit;
	int		hard_cold_temp_limit;
	int		hard_hot_temp_limit;
	bool		suspend_on_hard_temp_limit;
	unsigned int	soft_temp_limit_compensation;
	unsigned int	charge_current_compensation;
	bool		use_mains;
	bool		use_usb;
	bool		use_usb_otg;
	int		irq_gpio;
	enum smb347_chg_enable enable_control;
};

/* FUJITSU:2012-11-20 FJ_CHARGER start */
enum fj_smb_chg_ic_err_type {
	FJ_SMB_CHG_IC_ERROR_DCIN_OV = 0,
	FJ_SMB_CHG_IC_ERROR_DCIN_UV,
	FJ_SMB_CHG_IC_ERROR_USBIN_OV,
	FJ_SMB_CHG_IC_ERROR_USBIN_UV,
	FJ_SMB_CHG_IC_ERROR_TEMP_LIMIT,
	FJ_SMB_CHG_IC_ERROR_RESTART,

	FJ_SMB_CHG_IC_ERROR_MAX
};

enum fj_smb_chg_ic_err_bit_type {
	FJ_SMB_CHG_IC_ERROR_BIT_DCIN_OV		= (1 << FJ_SMB_CHG_IC_ERROR_DCIN_OV),
	FJ_SMB_CHG_IC_ERROR_BIT_DCIN_UV		= (1 << FJ_SMB_CHG_IC_ERROR_DCIN_UV),
	FJ_SMB_CHG_IC_ERROR_BIT_USBIN_OV	= (1 << FJ_SMB_CHG_IC_ERROR_USBIN_OV),
	FJ_SMB_CHG_IC_ERROR_BIT_USBIN_UV	= (1 << FJ_SMB_CHG_IC_ERROR_USBIN_UV),
	FJ_SMB_CHG_IC_ERROR_BIT_TEMP_LIMIT	= (1 << FJ_SMB_CHG_IC_ERROR_TEMP_LIMIT),
	FJ_SMB_CHG_IC_ERROR_BIT_RESTART		= (1 << FJ_SMB_CHG_IC_ERROR_RESTART),

	FJ_SMB_CHG_IC_ERROR_BIT_MAX
};

enum fj_smb_chg_aicl_check_type {
	FJ_SMB_CHG_IC_AICL_SMALL = 0,
	FJ_SMB_CHG_IC_AICL_BIG,

	FJ_SMB_CHG_IC_AICL_ERROR_MAX
};

enum fj_smb_chg_charging_type {
	FJ_SMB_CHG_TYPE_DISCHARGING = 0,
	FJ_SMB_CHG_TYPE_CHARGING,
	FJ_SMB_CHG_TYPE_POWERPATH,

	FJ_SMB_CHG_TYPE_MAX
};

struct fj_smb_charger_info
{
	unsigned int	state;
	unsigned int	chg_source;
	unsigned int	chg_current;
	unsigned int	current_err;
};


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
	const struct smb347_charger_platform_data *pdata;

	struct mutex				lock;
	struct i2c_client			*client;
	struct power_supply			mains;
	struct power_supply			usb;
	struct power_supply			battery;
	struct power_supply			holder;
	struct power_supply			mhl;

	bool						mains_online;
	bool						usb_online;
	bool						charging_enabled;

	struct dentry				*dentry;
	int							charge_mode;
	struct mutex				chg_lock;
	struct mutex				chg_quelock;
	struct work_struct			charge_work;
	struct work_struct			charge_err_work;
	struct work_struct			charge_ic_err_work;
	struct work_struct			nodelay_monitor_work;
	struct delayed_work			charge_monitor_work;
	struct delayed_work			full_charge_monitor_work;
	struct delayed_work			safety_charge_monitor_work;
	struct delayed_work			aicl_restart_monitor_work;
	struct wake_lock			charging_wake_lock;
	struct wake_lock			charging_wakeup_lock;
	struct wake_lock			param_chk_wake_lock;
	int							vbat_adj;
	int							vmax_adj;
	int							vmax_age_adj;
	int							full_chg_percent;
	unsigned long				suspend_flag;
	struct fj_smb_charger_info	charger_info;
};

extern void fj_smb_chg_source_req(int source, unsigned int mA);

extern int fj_smb_chg_get_adapter_voltage(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_get_battery_voltage(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_get_battery_temperature(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_get_ambient_temperature(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_get_receiver_temperature(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_get_board_temperature(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_get_charge_ic_temperature(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_get_sensor_temperature(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_get_battery_present(char *buffer, struct kernel_param *kp);
extern int fj_smb_chg_set_charger_enable(const char *val, struct kernel_param *kp, int charger_enable);
extern int fj_smb_chg_set_chg_voltage(const char *val, struct kernel_param *kp, int chg_voltage);
extern int fj_smb_chg_set_chg_current(const char *val, struct kernel_param *kp, int chg_current);
extern int fj_smb_chg_set_fj_chg_enable(const char *val, struct kernel_param *kp, int chg_enable);
extern int fj_smb_chg_set_chg_vinmin(const char *val, struct kernel_param *kp, int chg_vinmin);
extern void fj_smb_chg_notify_error(unsigned int err);
extern void fj_smb_chg_chgic_reset(void);
extern void fj_smb_chg_emergency_current_off(void);
/* FUJITSU:2012-11-20 FJ_CHARGER end */
#endif /* SMB347_CHARGER_H */
