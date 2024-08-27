/*
 * Copyright(C) 2012 FUJITSU LIMITED
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
#ifndef __FJ_CHARGER_H
#define __FJ_CHARGER_H


/* mA */
#define FJ_CHG_USB_CURRENT      500  // for USB charger
#define FJ_CHG_AC_CURRENT      1500  // for AC charger
#define FJ_CHG_HOLDER_CURRENT  1500  // for Desktop holder (Default)
#define FJ_CHG_HOLDER_CURRENT2 2000  // for Desktop holder (faster)
#define FJ_CHG_MHL_CURRENT      500  // for MHL Adaptor
#define FJ_CHG_OTHER_CURRENT    500  // for Others
#define FJ_CHG_OFF_CURRENT        0  // for Disconnect charger

/* notify error */
#define FJ_CHG_ERROR_NONE      0  // no error detection
#define FJ_CHG_ERROR_DETECT    1  // error detection

#define FJ_CHG_DISABLE         0  // disable charging
#define FJ_CHG_ENABLE          1  // enable charging

#define APNV_CHARGE_FG_RCOMP0_I             47140
#define APNV_CHARGE_FG_TEMPCO_I             47141
#define APNV_CHARGE_FG_FULLCAP_I            47142
#define APNV_CHARGE_FG_CYCLES_I             47143
#define APNV_CHARGE_FG_FULLCAPNOM_I         47144
#define APNV_CHARGE_FG_IAVG_EMPTY_I         47145
#define APNV_CHARGE_FG_QRTABLE00_I          47146
#define APNV_CHARGE_FG_QRTABLE10_I          47147
#define APNV_CHARGE_FG_QRTABLE20_I          47148
#define APNV_CHARGE_FG_QRTABLE30_I          47149
#define APNV_CHARGE_FG_AGE_I                47150


extern int  fj_get_chg_device(void);
extern void fj_chg_emergency_current_off(void);

extern void fj_chg_usb_vbus_draw(unsigned int onoff);
extern void fj_chg_ac_vbus_draw(unsigned int onoff);
extern void fj_chg_holder_vbus_draw(unsigned int onoff);
extern void fj_chg_mhl_vbus_draw(unsigned int onoff);
extern void fj_chg_other_vbus_draw(unsigned int onoff);

extern void fj_chg_notify_error(unsigned int err);
extern void fj_chg_chgic_reset(void);

/* fj_battery Prototype */
extern int fj_battery_set_status(int status);
extern int fj_battery_set_health(int health);
extern int fj_battery_get_terminal_temp(int *terminal_temp);
extern int fj_battery_get_receiver_temp(int *receiver_temp);
extern int fj_battery_get_charge_temp(int *charge_temp);
extern int fj_battery_get_center_temp(int *center_temp);
extern int fj_battery_get_sensor_temp(int *sensor_temp);
extern int fj_battery_get_battery_temp(int *battery_temp);
extern int fj_battery_set_battery_temp(int battery_temp);
extern int fj_battery_get_battery_volt(int *battery_volt);
extern int fj_battery_get_battery_capa(int *battery_capa);
extern int fj_battery_get_battery_capa_vf(int *battery_capa);
extern int fj_battery_get_battery_curr(int *battery_curr);
extern int fj_battery_get_battery_ave(int *battery_ave);
extern int fj_battery_get_battery_age(int *battery_age);
extern int fj_battery_set_battery_fullsocthr(unsigned int battery_fullsocthr);
extern int fj_battery_get_initialized_flag(void);

#ifndef __MAX17050_BATTERY_H_KARI_
#define __MAX17050_BATTERY_H_KARI_
struct msm_battery_callback_info{
    int (*callback)(unsigned int batt_status, void *data);  /* callback function */
    void *data;                                             /* touch senser data */
};
extern int set_msm_battery_callback_info(struct msm_battery_callback_info *info);
extern int unset_set_msm_battery_callback_info(void);
#endif  /* __MAX17050_BATTERY_H_KARI_ */

#endif /* __FJ_CHARGER_H */
