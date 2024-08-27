/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/

#ifndef __MAX17050_BATTERY_H_
#define __MAX17050_BATTERY_H_

typedef enum {
    MAX17050_MODEL_VER_NONE = 0,
    MAX17050_MODEL_VER_001,
    MAX17050_MODEL_VER_002,
    MAX17050_MODEL_VER_003
} MAX17050_MODEL_VER;

struct max17050_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
};

/**
 * max17050_set_status - set charger status
 *
 * @param  status - charger status
 *
 * @retval result - result this function (0:success)
 */
extern int max17050_set_status(int status);

/**
 * max17050_set_health - set charger health
 *
 * @param  health - charger health
 *
 * @retval result - result this function (0:success)
 */
extern int max17050_set_health(int health);

/**
 * max17050_get_receiver_temp - get receiver temperature
 *
 * @param  receiver_temp - receiver temperature
 *
 * @retval result - result this function (0:success)
 */
extern int max17050_get_receiver_temp(int *receiver_temp);

/**
 * max17050_get_terminal_temp - get terminal temperature
 *
 * @param  terminal_temp - terminal temperature
 *
 * @retval result - result this function (0:success)
 */
extern int max17050_get_terminal_temp(int *terminal_temp);

/**
 * max17050_get_battery_temp - get battery temperature
 *
 * @param  batt_temp - battery temperature
 *
 * @retval result - result this function (0:success)
 */
extern int max17050_get_battery_temp(int *batt_temp);

/**
 * max17050_set_battery_temp - set battery temperature
 *
 * @param  batt_temp - battery temperature
 *
 * @retval result - result this function (0:success)
 */
extern int max17050_set_battery_temp(int batt_temp);

/**
 * max17050_get_batt_vbat_raw - get battery ADC value
 *
 * @param  batt_vbat_raw - battery voltage
 *
 * @retval result - result this function (0:success)
 */
extern int max17050_get_batt_vbat_raw(int *batt_vbat_raw);

/**
 * max17050_get_batt_capacity - get battery capacity
 *
 * @param  batt_temp - battery temperature
 *
 * @retval result - result this function (0:success)
 */
extern int max17050_get_batt_capacity(int *batt_capacity);

/**
 * max17050_drv_initialized - check driver is initialized
 *
 * @param  void
 *
 * @retval result - 0: not initialized  1:initialized
 */
extern int max17050_drv_initialized(void);

#ifndef __MAX17050_BATTERY_H_KARI_
#define __MAX17050_BATTERY_H_KARI_
struct msm_battery_callback_info{
    int (*callback)(unsigned int batt_status, void *data);  /* callback function */
    void *data;                                             /* touch senser data */
};

/**
 * set_msm_battery_callback_info - set call back function
 *
 * @param  *info  - callback info
 *
 * @retval result - result this function (0:success, -1:fail)
 */
extern int set_msm_battery_callback_info(struct msm_battery_callback_info *info);

/**
 * unset_set_msm_battery_callback_info - unset call back function
 *
 * @param  none
 *
 * @retval result - result this function (0:success)
 */
extern int unset_set_msm_battery_callback_info(void);
#endif	/* __MAX17050_BATTERY_H_KARI_ */

#endif
