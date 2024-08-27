/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/

/* INCLUDE */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/reboot.h>
#include <linux/wait.h>
#include <linux/nonvolatile_common.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <mach/board_fj.h>
#include <linux/mfd/fj_charger.h>
#include <linux/mfd/fj_charger_local.h>
#include <linux/freezer.h>
#include <linux/irq.h>
#include <asm/system_info.h>

#include <linux/max17048_battery.h>
#include <linux/max17050_battery.h>

//#define FJ_BATTERY_DBG
#undef FJ_BATTERY_DBG

#ifdef FJ_BATTERY_DBG
#define FJ_BATTERY_DBGLOG(x, y...)          printk(KERN_ERR "[fj_batt] " x, ## y)
#else  /* FJ_BATTERY_DBG */
#define FJ_BATTERY_DBGLOG(x, y...)
#endif /* FJ_BATTERY_DBG */

#define FJ_BATTERY_INFOLOG(x, y...)         printk(KERN_INFO "[fj_batt] " x, ## y)
#define FJ_BATTERY_WARNLOG(x, y...)         printk(KERN_WARNING "[fj_batt] " x, ## y)
#define FJ_BATTERY_ERRLOG(x, y...)          printk(KERN_ERR "[fj_batt] " x, ## y)


typedef enum {
    FJ_BATT_FGIC_TYPE_NONE = 0,
    FJ_BATT_FGIC_TYPE_MAX17048,
    FJ_BATT_FGIC_TYPE_MAX17050
} FJ_BATT_FGIC_TYPE;

/* external reference */
/* max17048 --------------------------------------------------------------------*/
extern int max17048_mc_quick_start(void);
extern int max17048_mc_get_vcell(void);
extern int max17048_mc_get_soc(void);

extern int max17048_get_property_func(enum power_supply_property psp, union power_supply_propval *val);
extern int max17048_set_status(int status);
extern int max17048_set_health(int health);
extern int max17048_get_terminal_temp(int *terminal_temp);
extern int max17048_get_receiver_temp(int *receiver_temp);
extern int max17048_get_charge_temp(int *charge_temp);
extern int max17048_get_center_temp(int *center_temp);
extern int max17048_get_battery_temp(int *batt_temp);
extern int max17048_set_battery_temp(int batt_temp);
extern int max17048_get_batt_vbat_raw(int *batt_vbat_raw);
extern int max17048_get_batt_capacity(int *batt_capacity);
extern int max17048_set_callback_info(struct msm_battery_callback_info *info);
extern int max17048_unset_callback_info(void);

/* max17050 --------------------------------------------------------------------*/
extern int max17050_mc_initialize(void);
extern int max17050_mc_quick_start(void);
extern int max17050_mc_get_vcell(void);
extern int max17050_mc_get_socrep(void);
extern int max17050_mc_get_socmix(void);
extern int max17050_mc_get_socav(void);
extern int max17050_mc_get_socvf(void);
extern int max17050_mc_get_fullcap(void);
extern int max17050_mc_get_age(void);
extern int max17050_mc_get_current(void);
extern int max17050_mc_get_current_ave(void);
extern int max17050_mc_set_shutdown_config(void);

extern int max17050_get_property_func(enum power_supply_property psp, union power_supply_propval *val);
extern int max17050_set_status(int status);
extern int max17050_set_health(int health);
extern int max17050_get_terminal_temp(int *terminal_temp);
extern int max17050_get_receiver_temp(int *receiver_temp);
extern int max17050_get_charge_temp(int *charge_temp);
extern int max17050_get_center_temp(int *center_temp);
extern int max17050_get_sensor_temp(int *center_temp);
extern int max17050_get_battery_temp(int *batt_temp);
extern int max17050_set_battery_temp(int batt_temp);
extern int max17050_get_batt_vbat_raw(int *batt_vbat_raw);
extern int max17050_get_batt_capacity(int *batt_capacity);
extern int max17050_get_batt_capacity_vf(int *batt_capacity);
extern int max17050_get_batt_current(int *batt_current);
extern int max17050_get_batt_average_current(int *batt_average);
extern int max17050_get_batt_age(int *batt_age);
extern int max17050_set_batt_fullsocthr(unsigned int batt_fullsocthr);
extern int max17050_drv_initialized(void);
extern int max17050_set_callback_info(struct msm_battery_callback_info *info);
extern int max17050_unset_callback_info(void);

static FJ_BATT_FGIC_TYPE fj_battery_get_fgic_type(void)
{
    FJ_BATT_FGIC_TYPE result = FJ_BATT_FGIC_TYPE_NONE;
    unsigned int work_dev;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    work_dev = system_rev & 0xF0;
    
    switch (work_dev)
    {
        case FJ_CHG_DEVICE_TYPE_FJDEV005:
        case FJ_CHG_DEVICE_TYPE_FJDEV014:
            result = FJ_BATT_FGIC_TYPE_MAX17048;
            break;
        case FJ_CHG_DEVICE_TYPE_FJDEV001:
        case FJ_CHG_DEVICE_TYPE_FJDEV002:
        case FJ_CHG_DEVICE_TYPE_FJDEV003:
        case FJ_CHG_DEVICE_TYPE_FJDEV004:
            result = FJ_BATT_FGIC_TYPE_MAX17050;
            break;
        default:
            result = FJ_BATT_FGIC_TYPE_MAX17050;
            FJ_BATTERY_ERRLOG("[%s] Unknown DEVICE TYPE 0x%02X\n", __func__, work_dev);
            break;
    }
    return result;
}

static int fj_battery_initialize(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = 0;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_mc_initialize();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Initialize Error!:%d\n", __func__, result);
    } else {
        result = sprintf(buffer, "%d", 0x0001);
    }
    
    return result;
}
module_param_call(battery_initialize, NULL, fj_battery_initialize, NULL, 0444);

static int fj_battery_quick_start(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_mc_quick_start();
    } else {
        result = max17050_mc_quick_start();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Quick Start Error!:%d\n", __func__, result);
    }
    
    return result;
}
module_param_call(quick_start, NULL, fj_battery_quick_start, NULL, 0444);

static int fj_battery_mc_get_vcell(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    u16 vcell;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_mc_get_vcell();
    } else {
        result = max17050_mc_get_vcell();
    }
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get vcell Error!:%d\n", __func__, result);
    } else {
        vcell = result;
        result = sprintf(buffer, "%d", vcell);
    }
    
    return result;
}
module_param_call(battery_voltage, NULL, fj_battery_mc_get_vcell, NULL, 0444);

static int fj_battery_mc_get_socrep(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    u16 socrep;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_mc_get_soc();
    } else {
        result = max17050_mc_get_socrep();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get socrep Error!:%d\n", __func__, result);
    } else {
        socrep = result;
        result = sprintf(buffer, "%d", socrep);
    }
    
    return result;
}
module_param_call(battery_socrep, NULL, fj_battery_mc_get_socrep, NULL, 0444);

static int fj_battery_mc_get_socmix(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    u16 socmix;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_mc_get_soc();
    } else {
        result = max17050_mc_get_socmix();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get socmix Error!:%d\n", __func__, result);
    } else {
        socmix = result;
        result = sprintf(buffer, "%d", socmix);
    }
    
    return result;
}
module_param_call(battery_socmix, NULL, fj_battery_mc_get_socmix, NULL, 0444);

static int fj_battery_mc_get_socav(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    u16 socav;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_mc_get_soc();
    } else {
        result = max17050_mc_get_socav();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get socav Error!:%d\n", __func__, result);
    } else {
        socav = result;
        result = sprintf(buffer, "%d", socav);
    }
    
    return result;
}
module_param_call(battery_socav, NULL, fj_battery_mc_get_socav, NULL, 0444);

static int fj_battery_mc_get_socvf(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    u16 socvf;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_mc_get_soc();
    } else {
        result = max17050_mc_get_socvf();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get socvf Error!:%d\n", __func__, result);
    } else {
        socvf = result;
        result = sprintf(buffer, "%d", socvf);
    }
    
    return result;
}
module_param_call(battery_socvf, NULL, fj_battery_mc_get_socvf, NULL, 0444);

static int fj_battery_mc_get_fullcap(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    u16 fullcap = 0;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = 0;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_mc_get_fullcap();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get fullcap Error!:%d\n", __func__, result);
    } else {
        fullcap = result;
        result = sprintf(buffer, "%d", fullcap);
    }
    
    return result;
}
module_param_call(battery_fullcap, NULL, fj_battery_mc_get_fullcap, NULL, 0444);

static int fj_battery_mc_get_age(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    u16 age = 0;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = 100;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_mc_get_age();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get age Error!:%d\n", __func__, result);
    } else {
        age = result;
        result = sprintf(buffer, "%d", age);
    }
    
    return result;
}
module_param_call(battery_age, NULL, fj_battery_mc_get_age, NULL, 0444);

static int fj_battery_mc_get_current(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    signed short curr = 0;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = 0;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_mc_get_current();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get Current Error!:%d\n", __func__, result);
    } else {
        curr = (signed short)result;
        result = sprintf(buffer, "%d\n", curr);
    }
    
    return result;
}
module_param_call(battery_current, NULL, fj_battery_mc_get_current, NULL, 0444);

static int fj_battery_mc_get_current_ave(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    signed short curr_ave = 0;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = 0;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_mc_get_current_ave();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Get Current_ave Error!:%d\n", __func__, result);
    } else {
        curr_ave = (signed short)result;
        result = sprintf(buffer, "%d\n", curr_ave);
    }
    
    return result;
}
module_param_call(battery_current_ave, NULL, fj_battery_mc_get_current_ave, NULL, 0444);

static int fj_battery_mc_set_shutdown_config(char *buffer, struct kernel_param *kp)
{
    int result = 0;
    
    FJ_BATTERY_DBGLOG("[%s] in\n", __func__);
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = 0;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_mc_set_shutdown_config();
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] Set Shutdown Config Error!:%d\n", __func__, result);
    }
    
    return result;
}
module_param_call(battery_shdn, NULL, fj_battery_mc_set_shutdown_config, NULL, 0444);

int msm_battery_get_property(enum power_supply_property psp,
                                union power_supply_propval *val)
{
    int result = 0;
    
    if (val == NULL) {
        FJ_BATTERY_ERRLOG("[%s] val pointer is NULL\n", __func__);
        return -EINVAL;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        if(max17048_drv_initialized() == 0) {
            FJ_BATTERY_WARNLOG("[%s] error msm_battery_get_property called before init\n", __func__);
            return -EINVAL;
        }
        result = max17048_get_property_func(psp, val);
    } else {
        if(max17050_drv_initialized() == 0) {
            FJ_BATTERY_WARNLOG("[%s] error msm_battery_get_property called before init\n", __func__);
            return -EINVAL;
        }
        result = max17050_get_property_func(psp, val);
    }
    
    return result;
}
EXPORT_SYMBOL(msm_battery_get_property);

int fj_battery_set_status(int status)
{
    int result = 0;
    
    if (status < POWER_SUPPLY_STATUS_MAX) {
        if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
            result = max17048_set_status(status);
        } else {
            result = max17050_set_status(status);
        }
    } else {
        FJ_BATTERY_ERRLOG("[%s] Parameter Error! status = %d\n", __func__, status);
        result = -1;
    }
    
    return result;
}
EXPORT_SYMBOL(fj_battery_set_status);

int fj_battery_set_health(int health)
{
    int result = 0;
    
    if (health < POWER_SUPPLY_HEALTH_MAX) {
        if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
            result = max17048_set_health(health);
        } else {
            result = max17050_set_health(health);
        }
    } else {
        FJ_BATTERY_ERRLOG("[%s] Parameter Error! health = %d\n", __func__, health);
        result = -1;
    }
    
    return result;
}
EXPORT_SYMBOL(fj_battery_set_health);

int fj_battery_get_terminal_temp(int *terminal_temp)
{
    int result = 0;
    int temp_work = 0;
    
    if (terminal_temp == NULL) {
        FJ_BATTERY_ERRLOG("[%s] terminal_temp pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_get_terminal_temp(&temp_work);
    } else {
        result = max17050_get_terminal_temp(&temp_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Terminal Temperature\n", __func__);
        goto fj_battery_err;
    }
    
    *terminal_temp = temp_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_terminal_temp);

int fj_battery_get_receiver_temp(int *receiver_temp)
{
    int result = 0;
    int temp_work = 0;
    
    if (receiver_temp == NULL) {
        FJ_BATTERY_ERRLOG("[%s] receiver_temp pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_get_receiver_temp(&temp_work);
    } else {
        result = max17050_get_receiver_temp(&temp_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Receiver Temperature\n", __func__);
        goto fj_battery_err;
    }
    
    *receiver_temp = temp_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_receiver_temp);

int fj_battery_get_charge_temp(int *charge_temp)
{
    int result = 0;
    int temp_work = 0;
    
    if (charge_temp == NULL) {
        FJ_BATTERY_ERRLOG("[%s] charge_temp pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_get_charge_temp(&temp_work);
    } else {
        result = max17050_get_charge_temp(&temp_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Charge Temperature\n", __func__);
        goto fj_battery_err;
    }
    
    *charge_temp = temp_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_charge_temp);

int fj_battery_get_center_temp(int *center_temp)
{
    int result = 0;
    int temp_work = 0;
    
    if (center_temp == NULL) {
        FJ_BATTERY_ERRLOG("[%s] center_temp pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_get_center_temp(&temp_work);
    } else {
        result = max17050_get_center_temp(&temp_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Center Temperature\n", __func__);
        goto fj_battery_err;
    }
    
    *center_temp = temp_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_center_temp);

int fj_battery_get_sensor_temp(int *sensor_temp)
{
    int result = 0;
    int temp_work = 0;
    
    if (sensor_temp == NULL) {
        FJ_BATTERY_ERRLOG("[%s] sensor_temp pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {

    } else {
        result = max17050_get_sensor_temp(&temp_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get sensor Temperature\n", __func__);
        goto fj_battery_err;
    }
    
    *sensor_temp = temp_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_sensor_temp);

int fj_battery_get_battery_temp(int *battery_temp)
{
    int result = 0;
    int temp_work = 0;
    
    if (battery_temp == NULL) {
        FJ_BATTERY_ERRLOG("[%s] battery_temp pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_get_battery_temp(&temp_work);
    } else {
        result = max17050_get_battery_temp(&temp_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Battery Temperature %d\n", __func__, result);
        goto fj_battery_err;
    }
    
    *battery_temp = temp_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_battery_temp);

int fj_battery_set_battery_temp(int battery_temp)
{
    int result = 0;
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_set_battery_temp(battery_temp);
    } else {
        result = max17050_set_battery_temp(battery_temp);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Set Battery Temperature %d\n", __func__, result);
    }
    
    return result;
}
EXPORT_SYMBOL(fj_battery_set_battery_temp);

int fj_battery_get_battery_volt(int *battery_volt)
{
    int result = 0;
    int volt_work = 0;
    
    if (battery_volt == NULL) {
        FJ_BATTERY_ERRLOG("[%s] battery_volt pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_get_batt_vbat_raw(&volt_work);
    } else {
        result = max17050_get_batt_vbat_raw(&volt_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Battery Voltage\n", __func__);
        goto fj_battery_err;
    }
    
    *battery_volt = volt_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_battery_volt);

int fj_battery_get_battery_capa(int *battery_capa)
{
    int result = 0;
    int capa_work = 0;
    
    if (battery_capa == NULL) {
        FJ_BATTERY_ERRLOG("[%s] battery_capa pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_get_batt_capacity(&capa_work);
    } else {
        result = max17050_get_batt_capacity(&capa_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Battery Capacity\n", __func__);
        goto fj_battery_err;
    }
    
    *battery_capa = capa_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_battery_capa);

int fj_battery_get_battery_capa_vf(int *battery_capa)
{
    int result = 0;
    int capa_work = 0;
    
    if (battery_capa == NULL) {
        FJ_BATTERY_ERRLOG("[%s] battery_capa pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_get_batt_capacity(&capa_work);
    } else {
        result = max17050_get_batt_capacity_vf(&capa_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Battery Capacity VF\n", __func__);
        goto fj_battery_err;
    }
    
    *battery_capa = capa_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_battery_capa_vf);

int fj_battery_get_battery_curr(int *battery_curr)
{
    int result = 0;
    int curr_work = 0;
    
    if (battery_curr == NULL) {
        FJ_BATTERY_ERRLOG("[%s] battery_curr pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = -1;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_get_batt_current(&curr_work);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Battery Current\n", __func__);
        goto fj_battery_err;
    }
    
    *battery_curr = curr_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_battery_curr);

int fj_battery_get_battery_ave(int *battery_ave)
{
    int result = 0;
    int ave_work = 0;
    
    if (battery_ave == NULL) {
        FJ_BATTERY_ERRLOG("[%s] battery_ave pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = -1;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_get_batt_average_current(&ave_work);
    }
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Battery Average Current\n", __func__);
        goto fj_battery_err;
    }
    
    *battery_ave = ave_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_battery_ave);

int fj_battery_get_battery_age(int *battery_age)
{
    int result = 0;
    int age_work = 0;
    
    if (battery_age == NULL) {
        FJ_BATTERY_ERRLOG("[%s] battery_ave pointer is NULL\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = -1;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_get_batt_age(&age_work);
    }
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Get Battery Age\n", __func__);
        goto fj_battery_err;
    }
    
    *battery_age = age_work;
    
fj_battery_err:
    return result;
}
EXPORT_SYMBOL(fj_battery_get_battery_age);

int fj_battery_set_battery_fullsocthr(unsigned int battery_fullsocthr)
{
    int result = 0;
    
    if (battery_fullsocthr > 100) {
        FJ_BATTERY_ERRLOG("[%s] fullsocthr error!!!\n", __func__);
        result = -1;
        goto fj_battery_err;
    }
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = -1;
        FJ_BATTERY_ERRLOG("[%s] MAX17048 Not Support!!!\n", __func__);
    } else {
        result = max17050_set_batt_fullsocthr(battery_fullsocthr);
    }
    
    if (result < 0) {
        FJ_BATTERY_ERRLOG("[%s] FAILURE!! Set Battery FULLSOCTHR\n", __func__);
        goto fj_battery_err;
    }
    
fj_battery_err:
    return result;
}

int fj_battery_get_initialized_flag(void)
{
    int result = 0;
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_drv_initialized();
    } else {
        result = max17050_drv_initialized();
    }
    return result;
}
EXPORT_SYMBOL(fj_battery_get_initialized_flag);

int set_msm_battery_callback_info(struct msm_battery_callback_info *info)
{
    int result = 0;
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_set_callback_info(info);
    } else {
        result = max17050_set_callback_info(info);
    }
    return result;
}
EXPORT_SYMBOL(set_msm_battery_callback_info);

int unset_set_msm_battery_callback_info(void)
{
    int result = 0;
    
    if (fj_battery_get_fgic_type() == FJ_BATT_FGIC_TYPE_MAX17048) {
        result = max17048_unset_callback_info();
    } else {
        result = max17050_unset_callback_info();
    }
    return result;
}
EXPORT_SYMBOL(unset_set_msm_battery_callback_info);

