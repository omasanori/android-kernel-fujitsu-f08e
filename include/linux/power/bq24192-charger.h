/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/

#ifndef BQ24192_CHARGER_H
#define BQ24192_CHARGER_H

#include <linux/types.h>
#include <linux/power_supply.h>

struct fj_bq_charger_info
{
	unsigned int	state;
	unsigned int	chg_source;
	unsigned int	chg_current;
	unsigned int	current_err;
};

struct bq24192_charger {
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
	struct work_struct			nodelay_monitor_work;
	struct delayed_work			charge_monitor_work;
	struct delayed_work			full_charge_monitor_work;
	struct delayed_work			safety_charge_monitor_work;
	struct wake_lock			charging_wake_lock;
	struct wake_lock			charging_wakeup_lock;
	int							vbat_adj;
	unsigned long				suspend_flag;
	struct fj_bq_charger_info	charger_info;
};

extern void fj_bq_chg_source_req(int source, unsigned int mA);

extern int fj_bq_chg_get_adapter_voltage(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_get_battery_voltage(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_get_battery_temperature(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_get_ambient_temperature(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_get_receiver_temperature(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_get_board_temperature(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_get_charge_ic_temperature(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_get_sensor_temperature(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_get_battery_present(char *buffer, struct kernel_param *kp);
extern int fj_bq_chg_set_charger_enable(const char *val, struct kernel_param *kp, int charger_enable);
extern int fj_bq_chg_set_chg_voltage(const char *val, struct kernel_param *kp, int chg_voltage);
extern int fj_bq_chg_set_chg_current(const char *val, struct kernel_param *kp, int chg_current);
extern int fj_bq_chg_set_fj_chg_enable(const char *val, struct kernel_param *kp, int chg_enable);
extern int fj_bq_chg_set_chg_vinmin(const char *val, struct kernel_param *kp, int chg_vinmin);
extern void fj_bq_chg_notify_error(unsigned int err);
extern void fj_bq_chg_emergency_current_off(void);
#endif /* BQ24192_CHARGER_H */
