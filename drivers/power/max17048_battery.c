
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/ovp.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/reboot.h>
#include <linux/wait.h>
#include <linux/nonvolatile_common.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <asm/system_info.h>
#include "../arch/arm/mach-msm/board-8960.h"

#include <mach/board_fj.h>
#include <linux/mfd/fj_charger.h>
#include <linux/mfd/fj_charger_local.h>
#include <linux/max17048_battery.h>
#include <linux/freezer.h>
#include <linux/irq.h>
#ifndef FJ_CHARGER_LOCAL
#include <linux/fta.h>
#endif /* FJ_CHARGER_LOCAL */

//#define MAX17048_DBG
#undef MAX17048_DBG

#ifdef MAX17048_DBG
#define MAX17048_DBGLOG(x, y...)            printk(KERN_ERR "[max17048_batt]" x, ## y)
#else  /* MAX17048_DBG */
#define MAX17048_DBGLOG(x, y...)
#endif /* MAX17048_DBG */

#define MAX17048_INFOLOG(x, y...)           printk(KERN_INFO "[max17048_batt]" x, ## y)
#define MAX17048_WARNLOG(x, y...)           printk(KERN_WARNING "[max17048_batt]" x, ## y)
#define MAX17048_ERRLOG(x, y...)            printk(KERN_ERR "[max17048_batt]" x, ## y)

#ifndef FJ_CHARGER_LOCAL
#define MAX17048_RECLOG(x, y...)            {                                               \
                                                char recbuf[128];                           \
                                                snprintf(recbuf, sizeof(recbuf), x, ## y);  \
                                                ftadrv_send_str(recbuf);                    \
                                            }
#else
#define MAX17048_RECLOG(x, y...)
#endif /* FJ_CHARGER_LOCAL */

/* MAX17048 register address */
#define MAX17048_VCELL_MSB					0x02
#define MAX17048_VCELL_LSB					0x03
#define MAX17048_SOC_MSB					0x04
#define MAX17048_SOC_LSB					0x05
#define MAX17048_MODE_MSB					0x06
#define MAX17048_MODE_LSB					0x07
#define MAX17048_VER_MSB					0x08
#define MAX17048_VER_LSB					0x09
#define MAX17048_HIBRT_MSB					0x0A
#define MAX17048_HIBRT_LSB					0x0B
#define MAX17048_CONFIG_MSB					0x0C
#define MAX17048_CONFIG_LSB					0x0D
#define MAX17048_OCV_MSB					0x0E
#define MAX17048_OCV_LSB					0x0F
#define MAX17048_VALRT_MSB					0x14
#define MAX17048_VALRT_LSB					0x15
#define MAX17048_CRATE_MSB					0x16
#define MAX17048_CRARE_LSB					0x17
#define MAX17048_VRESET_MSB					0x18
#define MAX17048_VRESET_LSB					0x19
#define MAX17048_STATUS_MSB					0x1A
#define MAX17048_STATUS_LSB					0x1B
#define MAX17048_ACCESS_MSB					0x3E
#define MAX17048_ACCESS_LSB					0x3F
#define MAX17048_CMD_MSB					0xFE
#define MAX17048_CMD_LSB					0xFF

#define MAX17048_RCOMP_TEST					0x00FF
#define MAX17048_RCOMP_INIT_42				0x0082
#define MAX17048_RCOMP_INIT_435				0x008C
#define MAX17048_CONFIG_INIT_42				(0x4000|MAX17048_RCOMP_INIT_42)
#define MAX17048_CONFIG_INIT_435			(0x4000|MAX17048_RCOMP_INIT_435)
#define MAX17048_HIBRT_INIT 				0x0000
#define MAX17048_VALRT_INIT 				0xFFA0
#define MAX17048_STATUS_INIT 				0x0000
#define MAX17048_STATUS_SOC 				0x0020
#define MAX17048_STATUS_VL	 				0x0004
#define MAX17048_OCV_TEST_42				0x90DA
#define MAX17048_OCV_TEST_435				0xB0E1
#define MAX17048_ACCESS_UNMASK				0x574A
#define MAX17048_ACCESS_MASK				0x0000

/* monitor time */
#define MAX17048_MONITOR_DELAY_100MS        msecs_to_jiffies(100)
#define MAX17048_MONITOR_DELAY_300MS        msecs_to_jiffies(300)
#define MAX17048_MONITOR_DELAY_500MS        msecs_to_jiffies(500)
#define MAX17048_MONITOR_DELAY_1S           msecs_to_jiffies(1000)
#define MAX17048_MONITOR_DELAY_10S          msecs_to_jiffies(10000)
#define MAX17048_MONITOR_DELAY_60S          msecs_to_jiffies(60000)

/* battery capacity */
#define MAX17048_BATTERY_FULL				100
#define MAX17048_BATTERY_99					99
#define MAX17048_BATTERY_10					10

/* NV value ID*/
#define APNV_CHG_ADJ_VBAT_I					41040
#define APNV_FUNC_LIMITS_I					41053

#define MAX17048_VBAT_ADJ_RATIO				40
#define MAX17048_VBAT_TBL_MAX				32

#define MAX17048_LOWBATT_VOLT				3200
#define MAX17048_LOWBATT_RECOVER_VOLT		3500
#define MAX17048_LOWBATT_SOC				0
#define BATTERY_NOT_PRESENT_TEMP			(-300)

/* max17048 thread events */
#define MAX17048_EVENT_PRESENT_MONITOR		0
#define MAX17048_EVENT_BATT_MONITOR			1
#define MAX17048_EVENT_SUSPEND				2
#define MAX17048_EVENT_MT_MONITOR			3
#define MAX17048_EVENT_ALERT_ISR			4

/* max17048 related GPIOs */
#define MAX17048_INT_GPIO                   36
#define MAX17048_VC_DET_GPIO                7
#define MAX17048_VB_DET_GPIO                28

#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)

/* GCF Support */
#define MAX17048_FUNC_NO_LIMIT				0x0000
enum max17048_func_limit {
    MAX17048_FUNC_LIMIT_CHARGE = 0,         /*  0 */
    MAX17048_FUNC_LIMIT_BATTERY_PRESENT,    /*  1 */
    MAX17048_FUNC_LIMIT_LOW_BATTERY,        /*  2 */
    MAX17048_FUNC_LIMIT_RESERVE_01,         /*  3 */
    MAX17048_FUNC_LIMIT_BATTERY_TEMP,       /*  4 */
    MAX17048_FUNC_LIMIT_TERMINAL_TEMP,      /*  5 */
    MAX17048_FUNC_LIMIT_RECEIVER_TEMP,      /*  6 */
    MAX17048_FUNC_LIMIT_CHARGE_TEMP,        /*  7 */
    MAX17048_FUNC_LIMIT_CENTER_TEMP,        /*  8 */
    MAX17048_FUNC_LIMIT_NUM,
};

typedef enum { 
    MAX17048_MODEL_TYPE_NONE = FJ_CHG_MODEL_TYPE_NONE,
    MAX17048_MODEL_TYPE_FJDEV001,
    MAX17048_MODEL_TYPE_FJDEV002,
    MAX17048_MODEL_TYPE_FJDEV003,
    MAX17048_MODEL_TYPE_FJDEV004,
    MAX17048_MODEL_TYPE_FJDEV005,
    MAX17048_MODEL_TYPE_FJDEV014
} MAX17048_MODEL_TYPE; 

typedef enum {
    MAX17048_MODEL_VER_NONE = 0,
    MAX17048_MODEL_VER_001,
    MAX17048_MODEL_VER_002,
    MAX17048_MODEL_VER_003
} MAX17048_MODEL_VER;

struct max17048_chip {
    struct i2c_client               *client;
    
    struct delayed_work             battery_monitor;
    struct delayed_work             battery_present_monitor;
    struct delayed_work             mt_monitor;
    struct power_supply             battery;
    struct max17048_platform_data   *pdata;
    
    /* battery voltage */
    int vcell;
    /* battery voltage raw */
    int vbat_raw;
    /* battery capacity */
    int soc;
    /* State Of Charge */
    int status;
    /* battery health */
    int health;
    /* battery present */
    int present;
    /* battery temp */
    int temp_batt;
    /* terminal temp */
    int temp_ambient;
    /* receiver temp */
    int temp_case;
    /* charger temp */
    int temp_charge;
    /* center temp */
    int temp_center;
    /* vbat adjust */
    uint8_t vbat_adj;
    /* battery monitor keepalive */
    int monitor_keepalive;
    /* battery monitor interval */
    unsigned long monitor_interval;
    /* battery present check monitor interval */
    unsigned long present_monitor_interval;
    /* mobile(receiver,terminal) temperature monitor interval */
    unsigned long mt_interval;
    /* update battery information interval */
    unsigned long update_interval;
    /* GCF function limit */
    bool check_disable[MAX17048_FUNC_LIMIT_NUM];
    /* monitor event state */
    unsigned long event_state;
    /* low battery detect flag */
    unsigned char lowbattery;
    
    struct mutex lock;
    wait_queue_head_t wq;
    void* thread;
};

typedef void (*max17048_handler_t)(unsigned long cmd, unsigned long value, void *data);

/* vattery voltage adjust table */
static int max17048_vbat_tbl[MAX17048_VBAT_TBL_MAX] =
{
    0,          /* +0  */ /* NV setting  = 0x00 */
    1,          /* +1  */ /*             = 0x01 */
    2,          /* +2  */ /*             = 0x02 */
    3,          /* +3  */ /*             = 0x03 */
    4,          /* +4  */ /*             = 0x04 */
    5,          /* +5  */ /*             = 0x05 */
    6,          /* +6  */ /*             = 0x06 */
    7,          /* +7  */ /*             = 0x07 */
    8,          /* +8  */ /*             = 0x08 */
    9,          /* +9  */ /*             = 0x09 */
    10,         /* +10 */ /*             = 0x0A */
    11,         /* +11 */ /*             = 0x0B */
    12,         /* +12 */ /*             = 0x0C */
    13,         /* +13 */ /*             = 0x0D */
    14,         /* +14 */ /*             = 0x0E */
    15,         /* +15 */ /*             = 0x0F */
    16,         /* -16 */ /*             = 0x10 */
    15,         /* -15 */ /*             = 0x11 */
    14,         /* -14 */ /*             = 0x12 */
    13,         /* -13 */ /*             = 0x13 */
    12,         /* -12 */ /*             = 0x14 */
    11,         /* -11 */ /*             = 0x15 */
    10,         /* -10 */ /*             = 0x16 */
    9,          /* -9  */ /*             = 0x17 */
    8,          /* -8  */ /*             = 0x18 */
    7,          /* -7  */ /*             = 0x19 */
    6,          /* -6  */ /*             = 0x1A */
    5,          /* -5  */ /*             = 0x1B */
    4,          /* -4  */ /*             = 0x1C */
    3,          /* -3  */ /*             = 0x1D */
    2,          /* -2  */ /*             = 0x1E */
    1           /* -1  */ /*             = 0x1F */
};

static int max17048_capacity_convert[101] =
{
     0,
     1,  2,  3,  4,  5,  6,  7,  9, 10, 11,
    12, 14, 15, 16, 17, 19, 20, 21, 22, 24,
    25, 26, 28, 29, 30, 31, 32, 33, 34, 35,
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
    46, 47, 48, 48, 49, 50, 51, 52, 53, 54,
    55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
    65, 66, 67, 67, 68, 69, 70, 71, 72, 73,
    74, 75, 76, 77, 78, 79, 80, 81, 82, 83,
    83, 84, 85, 86, 87, 88, 89, 90, 90, 91,
    92, 93, 94, 95, 96, 96, 97, 98, 99, 100
};

/* touch panel driver callback function */
static struct msm_battery_callback_info msm_touch_callback;
static bool set_callback_function_flag = false;

static int max17048_queue_stat;
static struct max17048_chip *the_chip = NULL;
static int max17048_initialized = 0;	/* max17048 init flag(0:no initialize,1:initialized)*/

static int battery_health = POWER_SUPPLY_HEALTH_GOOD;
static int battery_status = POWER_SUPPLY_STATUS_DISCHARGING;

static unsigned char fj_soc_mask_start = 0;
static unsigned long fj_soc_mask_expire = 0;
static unsigned char fj_lowbatt_volt_detect_cnt = 3;
static unsigned char fj_lowbatt_soc_detect_cnt = 3;
static unsigned char fj_keep_full_status = 0;
const static int max17048_435V_chg_flag = 1;

/* max17048 battery data */
static const u16 max17048_table_42[] = {
    0x00AF, 0x50B7, 0x60B9, 0x90BA, 0x20BB, 0xC0BB, 0xE0BB, 0x10BC,     /* 0x40 ------ 0x4F */
    0x70BC, 0x80BD, 0x60BE, 0x50BF, 0xB0C3, 0x10C8, 0xF0CB, 0x90D0,     /* 0x50 ------ 0x5F */
    0xB004, 0x000F, 0x000D, 0xC00D, 0x600C, 0x1058, 0xB03A, 0xB03A,     /* 0x60 ------ 0x6F */
    0x0027, 0xE01B, 0x001A, 0xF00A, 0xF00A, 0xE008, 0x7007, 0x7007,     /* 0x70 ------ 0x7F */
};

static const u16 max17048_table_435[] = {
    0xD0AE, 0x80B1, 0xE0B4, 0x50BB, 0xA0BB, 0x20BC, 0x80BC, 0x80BD,     /* 0x40 ------ 0x4F */
    0xD0BF, 0x50C1, 0xF0C4, 0x50C7, 0xA0C9, 0x00CD, 0x80D1, 0xB0D7,     /* 0x50 ------ 0x5F */
    0x0003, 0x0002, 0x0009, 0x2034, 0x001B, 0x0039, 0x6026, 0xF010,     /* 0x60 ------ 0x6F */
    0xE00C, 0xF008, 0xE008, 0x300A, 0x3008, 0xF007, 0x0006, 0x0006,     /* 0x70 ------ 0x7F */
};

static struct wake_lock max17048_chg_status;	/* for change power supply status */

static char fj_ps_status_str[5][16] = {
	"UNKNOWN",
	"CHARGING",
	"DISCHARGING",
	"NOT_CHARGING",
	"FULL",
};

static char fj_ps_health_str[10][32] = {
	"UNKNOWN",
	"GOOD",
	"OVERHEAT",
	"DEAD",
	"OVERVOLTAGE",
	"UNSPEC_FAILURE",
	"COLD",
	"UNDER_SUPPLY_VOLTAGE",
	"OVER_SUPPLY_VOLTAGE",
	"HOLDER_ERROR",
};

static enum power_supply_property max17048_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_TEMP_CASE,
	POWER_SUPPLY_PROP_TEMP_CHARGE,
	POWER_SUPPLY_PROP_TEMP_CENTER,
};

static MAX17048_MODEL_TYPE max17048_model_type = MAX17048_MODEL_TYPE_NONE;
static MAX17048_MODEL_VER max17048_model_ver = MAX17048_MODEL_VER_NONE;


/* internal function prototype */
static int max17048_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val);
static int max17048_read_reg(struct i2c_client *client, int reg);
static int max17048_write_reg(struct i2c_client *client, int reg, int data);
static int max17048_get_soc(struct max17048_chip* chip, int *battery_capacity);
static void max17048_set_queue_state(void);
static int max17048_get_queue_state(void);
static void max17048_clear_queue_state(void);
static void max17048_wakeup(struct max17048_chip* chip);
static void max17048_convert_batt_voltage(struct max17048_chip* chip);
static int max17048_get_vbat(struct max17048_chip *chip, int *battery_voltage);
static int max17048_get_battery_voltage(struct max17048_chip *chip, int *battery_voltage);
static int max17048_set_battery_voltage(struct max17048_chip *chip, int battery_voltage);
static int max17048_get_battery_temperature(struct max17048_chip *chip, int *battery_temperature);
static int max17048_set_battery_temperature(struct max17048_chip *chip, int battery_temperature);
static int max17048_get_terminal_temperature(struct max17048_chip *chip, int *terminal_temperature);
static int max17048_set_terminal_temperature(struct max17048_chip *chip, int terminal_temperature);
static int max17048_get_receiver_temperature(struct max17048_chip *chip, int *receiver_temperature);
static int max17048_set_receiver_temperature(struct max17048_chip *chip, int receiver_temperature);
static int max17048_get_charge_temperature(struct max17048_chip *chip, int *charge_temperature);
static int max17048_set_charge_temperature(struct max17048_chip *chip, int charge_temperature);
static int max17048_get_center_temperature(struct max17048_chip *chip, int *center_temperature);
static int max17048_set_center_temperature(struct max17048_chip *chip, int center_temperature);
static int max17048_get_battery_capacity(struct max17048_chip *chip, int *battery_capacity);
static int max17048_effect_battery_capacity(struct max17048_chip *chip, int battery_capacity);
static int max17048_set_battery_capacity(struct max17048_chip *chip, int battery_capacity);
static int max17048_get_vbat_adjust(struct max17048_chip *chip, int *batt_offset);
static int max17048_set_vbat_adjust(struct max17048_chip *chip, int batt_offset);
static void max17048_set_rcomp(struct i2c_client *client, int battery_temperature);
static void max17048_battery_monitor(struct work_struct *work);
static void max17048_mobile_temp_monitor(struct work_struct *work);
static void max17048_delayed_init(struct max17048_chip *chip);
static void max17048_monitor_work(struct max17048_chip *chip);
static void max17048_alert_work(struct max17048_chip *chip);
static void max17048_monitor_mobile_temp(struct max17048_chip *chip);
static int max17048_is_update_params(struct max17048_chip *chip);
static void max17048_charge_function_limits_check(struct max17048_chip *chip);
static int max17048_charge_battery_present_check(struct max17048_chip *chip);
static void max17048_battery_present_monitor(struct work_struct *work);
static irqreturn_t max17048_alert_isr(int irq_no, void *dev_id);
static void max17048_setup(struct max17048_chip *chip);
static void max17048_interrupt_init(struct max17048_chip *chip);
static void max17048_error_sequence(void);
static int max17048_thread(void * ptr);

static int max17048_check_fgic_type(void)
{
    int result = 0;
    unsigned int work_dev;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    work_dev = system_rev & 0xF0;
    
    switch (work_dev)
    {
        case FJ_CHG_DEVICE_TYPE_FJDEV005:
        case FJ_CHG_DEVICE_TYPE_FJDEV014:
            result = 1;
            break;
        case FJ_CHG_DEVICE_TYPE_FJDEV001:
        case FJ_CHG_DEVICE_TYPE_FJDEV002:
        case FJ_CHG_DEVICE_TYPE_FJDEV003:
        case FJ_CHG_DEVICE_TYPE_FJDEV004:
        default:
            result = 0;
            break;
    }
    
    return result;
}

static int max17048_get_model_type(struct max17048_chip *chip)
{
    int result = 0;
    unsigned int work_dev;
    unsigned int work_ver;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    max17048_model_type = MAX17048_MODEL_TYPE_NONE;
    max17048_model_ver = MAX17048_MODEL_VER_NONE;
    
    work_dev = system_rev & 0xF0;
    work_ver = system_rev & 0x0F;
    
    switch (work_dev)
    {
        case FJ_CHG_DEVICE_TYPE_FJDEV001 :
            max17048_model_type = MAX17048_MODEL_TYPE_FJDEV001;
            max17048_model_ver = MAX17048_MODEL_VER_001;
            break;
            
        case FJ_CHG_DEVICE_TYPE_FJDEV002 :
            max17048_model_type = MAX17048_MODEL_TYPE_FJDEV002;
            max17048_model_ver = MAX17048_MODEL_VER_001;
            break;
            
        case FJ_CHG_DEVICE_TYPE_FJDEV003 :
            max17048_model_type = MAX17048_MODEL_TYPE_FJDEV003;
            max17048_model_ver = MAX17048_MODEL_VER_001;
            break;
            
        case FJ_CHG_DEVICE_TYPE_FJDEV004 :
            max17048_model_type = MAX17048_MODEL_TYPE_FJDEV004;
            max17048_model_ver = MAX17048_MODEL_VER_001;
            break;
            
        case FJ_CHG_DEVICE_TYPE_FJDEV005 :
            
            max17048_model_type = MAX17048_MODEL_TYPE_FJDEV005;
            
            if(work_ver == 0x01){
                max17048_model_ver = MAX17048_MODEL_VER_001;
            } else {
                max17048_model_ver = MAX17048_MODEL_VER_002;
            }
            break;
            
        case FJ_CHG_DEVICE_TYPE_FJDEV014 :
            
            max17048_model_type = MAX17048_MODEL_TYPE_FJDEV014;
            max17048_model_ver = MAX17048_MODEL_VER_001;
            break;
            
        default :
            max17048_model_type = MAX17048_MODEL_TYPE_FJDEV005;
            max17048_model_ver = MAX17048_MODEL_VER_001;
            break;
    }
    
    MAX17048_DBGLOG("[%s] work_dev %X work_ver %X\n", __func__, work_dev, work_ver);
    MAX17048_DBGLOG("[%s] model_type %X model_ver %X\n", __func__, max17048_model_type, max17048_model_ver);
    
    return result;
}

static int max17048_get_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    union power_supply_propval *val)
{
    static int old_status = POWER_SUPPLY_STATUS_UNKNOWN;
    static int old_health = POWER_SUPPLY_HEALTH_UNKNOWN;
    static int old_soc = 0;
    
    struct max17048_chip *chip;
    
    if (psy == NULL) {
        MAX17048_ERRLOG("[%s] psy pointer is NULL\n", __func__);
        return -1;
    }
    chip = container_of(psy, struct max17048_chip, battery);
    
    switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
            val->intval = chip->status;
            if (old_status != chip->status) {
                MAX17048_INFOLOG("[%s]old status(%s) -> new status(%s)\n", __func__,
                                    fj_ps_status_str[old_status],
                                    fj_ps_status_str[chip->status]);
                old_status = chip->status;
            }
            MAX17048_DBGLOG("[%s] chip->status = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_HEALTH:
            val->intval = chip->health;
            if (old_health != chip->health) {
                MAX17048_INFOLOG("[%s]old health(%s) -> new health(%s)\n", __func__,
                                    fj_ps_health_str[old_health],
                                    fj_ps_health_str[chip->health]);
                old_health = chip->health;
            }
            break;
        case POWER_SUPPLY_PROP_PRESENT:
            if (chip->check_disable[MAX17048_FUNC_LIMIT_BATTERY_PRESENT] == false) {
                val->intval = chip->present;
            } else {
                val->intval = 1;
            }
            MAX17048_DBGLOG("[%s] chip->present = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TECHNOLOGY:
            val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
            break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            if (chip->check_disable[MAX17048_FUNC_LIMIT_LOW_BATTERY] == false) {
                val->intval = chip->vcell;
            } else {
                val->intval = 3900;
            }
            MAX17048_DBGLOG("[%s] chip->vcell = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_CAPACITY:
            if (chip->check_disable[MAX17048_FUNC_LIMIT_LOW_BATTERY] == false) {
                val->intval = chip->soc;
                
                if (val->intval > 100) {
                    val->intval = 100;
                }
                val->intval = max17048_capacity_convert[val->intval];
                if (old_soc != chip->soc) {
                    MAX17048_INFOLOG("[%s]old soc(%d) -> new soc(%d)\n", __func__,
                                        old_soc, chip->soc);
                    old_soc = chip->soc;
                }
            } else {
                val->intval = 90;
            }
            MAX17048_DBGLOG("[%s] chip->soc = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP:
            if (chip->check_disable[MAX17048_FUNC_LIMIT_BATTERY_TEMP] == false) {
                val->intval = (chip->temp_batt * 10);
            } else {
                val->intval = 250;
            }
            MAX17048_DBGLOG("[%s] chip->temp_batt = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_AMBIENT:
            if (chip->check_disable[MAX17048_FUNC_LIMIT_TERMINAL_TEMP] == false) {
                val->intval = (chip->temp_ambient * 10);
            } else {
                val->intval = 250;
            }
            MAX17048_DBGLOG("[%s] chip->temp_ambient = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_CASE:
            if (chip->check_disable[MAX17048_FUNC_LIMIT_RECEIVER_TEMP] == false) {
                val->intval = (chip->temp_case * 10);
            } else {
                val->intval = 250;
            }
            MAX17048_DBGLOG("[%s] chip->temp_case = %d\n", __func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_CHARGE:
            val->intval = (chip->temp_charge * 10);
            MAX17048_DBGLOG("[%s] chip->temp_charge = %d\n", __func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_CENTER:
            val->intval = (chip->temp_center * 10);
            MAX17048_DBGLOG("[%s] chip->temp_center = %d\n", __func__, val->intval);
            break;
        default:
            MAX17048_WARNLOG("[%s]not handle power_supply_property(%d)\n", __func__, psp);
            return -EINVAL;
    }
    return 0;
}

static int max17048_read_reg(struct i2c_client *client, int reg)
{
    int ret = 0;
    int i = 0;
    
    if (client == NULL) {
        MAX17048_WARNLOG("[%s] client pointer is NULL\n", __func__);
        return -1;
    }
    
    ret = i2c_smbus_read_word_data(client, reg);
    
    if (ret < 0) {
        /* fail safe */
        MAX17048_ERRLOG("[%s] i2c word read err : reg = 0x%x, result = %d \n",
                        __func__, reg, ret);
        for (i = 0;i < 5;i++) {
            ret = i2c_smbus_read_word_data(client, reg);
            if (ret >= 0) {
                break;
            }
            msleep(100);
        }
        if (ret < 0) {
            MAX17048_ERRLOG("[%s] i2c word read retry err : reg = 0x%x, result = %d \n",
                                __func__, reg, ret);
        }
    }
    return ret;
}

static int max17048_write_reg(struct i2c_client *client, int reg, int data)
{
    int ret = 0;
    int i = 0;
    
    if (client == NULL) {
        MAX17048_WARNLOG("[%s] client pointer is NULL\n", __func__);
        return -1;
    }
    
    ret = i2c_smbus_write_word_data(client, reg, data);
    
    if (ret < 0) {
        /* fail safe */
        MAX17048_ERRLOG("[%s] i2c word write err : reg = 0x%x, data = 0x%x, result = %d \n",
                            __func__, reg, data, ret);
        for (i = 0;i < 5;i++) {
            ret = i2c_smbus_write_word_data(client, reg, data);
            if (ret >= 0) {
                break;
            }
            msleep(100);
        }
        if (ret < 0) {
            MAX17048_ERRLOG("[%s] i2c word write retry err : reg = 0x%x, data = 0x%x, result = %d \n",
                                 __func__, reg, data, ret);
        }
    }
    return ret;
}

static int max17048_get_soc(struct max17048_chip* chip, int *battery_capacity)
{
    u8 msb = 0;
    u8 lsb = 0;
    int soc = 0;
    int result = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    mutex_lock(&chip->lock);
    soc = max17048_read_reg(chip->client, MAX17048_SOC_MSB);
    if (soc < 0) {
        result = soc;
        MAX17048_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17048_err;
    }
    
    msb = (u8)(soc&0x00ff);
    lsb = (u8)((soc&0xff00)>>8);
    
    MAX17048_DBGLOG("[%s] SOC msb = 0x%x, lsb = 0x%x\n", __func__, msb, lsb);
    
    *battery_capacity = msb;
    if ((*battery_capacity == 0) &&
        (chip->status == POWER_SUPPLY_STATUS_CHARGING) &&
        (lsb >= 0x80)) {
        *battery_capacity = 1;
    }
    
    MAX17048_DBGLOG("[%s] battery_capacity = %d\n", __func__, *battery_capacity);
    
max17048_err:
    mutex_unlock(&chip->lock);
    return result;
}

static void max17048_set_queue_state(void)
{
    MAX17048_DBGLOG("[%s] in\n", __func__);
    max17048_queue_stat = 1;
}

static int max17048_get_queue_state(void)
{
    MAX17048_DBGLOG("[%s] in\n", __func__);
    return max17048_queue_stat;
}

static void max17048_clear_queue_state(void)
{
    MAX17048_DBGLOG("[%s] in\n", __func__);
    max17048_queue_stat = 0;
}

static void max17048_wakeup(struct max17048_chip* chip)
{
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    max17048_set_queue_state();
    
    wake_up(&chip->wq);
}

static void max17048_convert_batt_voltage(struct max17048_chip* chip)
{
    int result = 0;
    int vcell_work = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    vcell_work = chip->vbat_raw;
    vcell_work += chip->vbat_raw>>2;
    chip->vcell = vcell_work>>4;
    
    if (chip->vcell <= MAX17048_LOWBATT_VOLT) {
        if (fj_lowbatt_volt_detect_cnt > 0) {
            fj_lowbatt_volt_detect_cnt--;
        }
        if (fj_lowbatt_volt_detect_cnt == 0) {
            chip->lowbattery = 1;
            fj_lowbatt_volt_detect_cnt = 3;
            MAX17048_ERRLOG("[%s] low battery detect volt:%dmV\n", __func__, chip->vcell);
            MAX17048_RECLOG("[%s] low battery detect volt:%dmV\n", __func__, chip->vcell);
        }
    } else if ((chip->vcell >= MAX17048_LOWBATT_RECOVER_VOLT) &&
               (chip->status == POWER_SUPPLY_STATUS_CHARGING)) {
        fj_lowbatt_volt_detect_cnt = 3;
        chip->lowbattery = 0;
        
        /* unmask max17048 registers */
        result = max17048_write_reg(chip->client, MAX17048_ACCESS_MSB, MAX17048_ACCESS_UNMASK);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
            goto max17048_err;
        }
        /* clear interrupt status */
        result = max17048_write_reg(chip->client, MAX17048_STATUS_MSB, MAX17048_STATUS_VL);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
            goto max17048_err;
        }
        /* mask max17048 registers */
        result = max17048_write_reg(chip->client, MAX17048_ACCESS_MSB, MAX17048_ACCESS_MASK);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
            goto max17048_err;
        }
    }
    
    MAX17048_DBGLOG("[%s] chip->vcell = %d \n", __func__, chip->vcell);
    MAX17048_DBGLOG("[%s] chip->vbat_raw = 0x%04x\n", __func__, chip->vbat_raw);
    
    return;
    
max17048_err:
    max17048_error_sequence();
    return;
}

static int max17048_get_vbat(struct max17048_chip *chip, int *battery_voltage)
{
    int result = 0;
    int vcell = 0;
    unsigned long vbat_raw = 0;
    unsigned long temp = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        chip = the_chip;
    }
    
    if (battery_voltage == NULL) {
        MAX17048_ERRLOG("[%s] battery_voltage pointer is NULL\n", __func__);
        return -1;
    }
    
    mutex_lock(&chip->lock);
    vcell = max17048_read_reg(chip->client, MAX17048_VCELL_MSB);
    if (vcell < 0) {
        result = vcell;
        MAX17048_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17048_err;
    } else {
        temp = be16_to_cpu(vcell);
    }
    
    if( chip->vbat_adj < 0x10 ) {
        vbat_raw = temp + (max17048_vbat_tbl[chip->vbat_adj] * MAX17048_VBAT_ADJ_RATIO);
    } else {
        vbat_raw = temp - (max17048_vbat_tbl[chip->vbat_adj] * MAX17048_VBAT_ADJ_RATIO);
    }
    
    *battery_voltage = vbat_raw;
    MAX17048_DBGLOG("[%s] from ADC battery_voltage = 0x%04x\n", __func__, *battery_voltage);
    
max17048_err:
    mutex_unlock(&chip->lock);
    return result;
}

static int max17048_get_battery_voltage(struct max17048_chip *chip, int *battery_voltage)
{
    int result = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (battery_voltage == NULL) {
        MAX17048_ERRLOG("[%s] battery_voltage pointer is NULL\n", __func__);
        return -1;
    }
    
    result = max17048_get_vbat(chip, battery_voltage);
    if (result < 0) {
        MAX17048_ERRLOG("[%s]get vbat error:%d\n", __func__, result);
        goto max17048_err;
    }
    
    MAX17048_DBGLOG("[%s] battery_voltage = 0x%04x\n", __func__, *battery_voltage);
    return result;
    
max17048_err:
    max17048_error_sequence();
    return result;
}

static int max17048_set_battery_voltage(struct max17048_chip *chip, int battery_voltage)
{
	int result = 0;

	MAX17048_DBGLOG("[%s] in\n", __func__);
	MAX17048_DBGLOG("[%s] battery_voltage = %d \n", __func__, battery_voltage);

	if (chip == NULL) {
		MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
		if (the_chip == NULL) {
			MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
			return -1;
		}
		chip = the_chip;
	}

	chip->vbat_raw = battery_voltage;
	return result;
}

static int max17048_get_battery_temperature(struct max17048_chip *chip, int *battery_temperature)
{
    struct pm8xxx_adc_chan_result result_batt_temp;
    int result = 0;
    int i = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (battery_temperature == NULL) {
        MAX17048_ERRLOG("[%s] battery_temperature pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17048_model_ver == MAX17048_MODEL_VER_001) {
        *battery_temperature = 250;
        MAX17048_DBGLOG("[%s] battery_temperature = %d\n", __func__, *battery_temperature);
        return 0;
    }
    
    memset(&result_batt_temp, 0, sizeof(result_batt_temp));
    *battery_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_4, ADC_MPP_1_AMUX6, &result_batt_temp);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] read err ADC MPP4 value raw:0x%x physical:%lld\n", __func__,
                                                                result_batt_temp.adc_code,
                                                                result_batt_temp.physical);
        } else {
            *battery_temperature = (int)result_batt_temp.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17048_error_sequence();
    }
    
    MAX17048_DBGLOG("[%s] battery_temperature = %d\n", __func__, *battery_temperature);
    
    return result;
}

static int max17048_set_battery_temperature(struct max17048_chip *chip, int battery_temperature)
{
    int result = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    MAX17048_DBGLOG("[%s] battery_temperature = %d \n", __func__, battery_temperature);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_batt = (battery_temperature / 10);
    
    return result;
}

static int max17048_get_terminal_temperature(struct max17048_chip *chip, int *terminal_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_ambient;
    int result = 0;
    int i = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (terminal_temperature == NULL) {
        MAX17048_ERRLOG("[%s] terminal_temperature pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17048_model_ver == MAX17048_MODEL_VER_001) {
        *terminal_temperature = 250;
        MAX17048_DBGLOG("[%s] from ADC terminal_temperature = %d\n", __func__, *terminal_temperature);
        return 0;
    }
    
    memset(&result_temp_ambient, 0, sizeof(result_temp_ambient));
    *terminal_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_read(ADC_MPP_1_AMUX8, &result_temp_ambient);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] read err ADC MPP8 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_ambient.adc_code,
                                                                    result_temp_ambient.physical);
        } else {
            *terminal_temperature = (int)result_temp_ambient.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17048_error_sequence();
    }
    
    MAX17048_DBGLOG("[%s] from ADC terminal_temperature = %d\n", __func__, *terminal_temperature);
    
    return result;
}

static int max17048_set_terminal_temperature(struct max17048_chip *chip, int terminal_temperature)
{
    int result = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    MAX17048_DBGLOG("[%s] terminal_temperature = %d \n", __func__, terminal_temperature);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_ambient = (terminal_temperature / 10);
    
    return result;
}

static int max17048_get_receiver_temperature(struct max17048_chip *chip, int *receiver_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_case;
    int result = 0;
    int i = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (receiver_temperature == NULL) {
        MAX17048_ERRLOG("[%s] receiver_temperature pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17048_model_ver == MAX17048_MODEL_VER_001) {
        *receiver_temperature = 250;
        MAX17048_DBGLOG("[%s] from ADC receiver_temperature = %d\n", __func__, *receiver_temperature);
        return 0;
    }
    
    memset(&result_temp_case, 0, sizeof(result_temp_case));
    *receiver_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_7, ADC_MPP_1_AMUX7, &result_temp_case);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] read err ADC MPP7 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_case.adc_code,
                                                                    result_temp_case.physical);
        } else {
            *receiver_temperature = (int)result_temp_case.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17048_error_sequence();
    }
    
    MAX17048_DBGLOG("[%s] from ADC receiver_temperature = %d\n", __func__, *receiver_temperature);
    
    return result;
}

static int max17048_set_receiver_temperature(struct max17048_chip *chip, int receiver_temperature)
{
    int result = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    MAX17048_DBGLOG("[%s] receiver_temperature = %d \n", __func__, receiver_temperature);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_case = (receiver_temperature / 10);
    
    return result;
}

static int max17048_get_charge_temperature(struct max17048_chip *chip, int *charge_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_charge;
    int result = 0;
    int i = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (charge_temperature == NULL) {
        MAX17048_ERRLOG("[%s] charge_temperature pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17048_model_ver == MAX17048_MODEL_VER_001) {
        *charge_temperature = 250;
        MAX17048_DBGLOG("[%s] from ADC Result:%d charge_temperature = %d\n", __func__, result, *charge_temperature);
        return 0;
    }
    
    memset(&result_temp_charge, 0, sizeof(result_temp_charge));
    *charge_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_9, ADC_MPP_1_AMUX7, &result_temp_charge);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] read err ADC MPP9 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_charge.adc_code,
                                                                    result_temp_charge.physical);
        } else {
            *charge_temperature = (int)result_temp_charge.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17048_error_sequence();
    }
    
    MAX17048_DBGLOG("[%s] from ADC Result:%d charge_temperature = %d\n", __func__, result, *charge_temperature);
    
    return result;
}

static int max17048_set_charge_temperature(struct max17048_chip *chip, int charge_temperature)
{
    int result = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    MAX17048_DBGLOG("[%s] charge_temperature = %d \n", __func__, charge_temperature);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_charge = (charge_temperature / 10);
    
    return result;
}

static int max17048_get_center_temperature(struct max17048_chip *chip, int *center_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_center;
    int result = 0;
    int i = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (center_temperature == NULL) {
        MAX17048_ERRLOG("[%s] center_temperature is NULL\n", __func__);
        return -1;
    }
    
    if (max17048_model_ver == MAX17048_MODEL_VER_001) {
        *center_temperature = 250;
        MAX17048_DBGLOG("[%s] from ADC Result:%d center_temperature = %d\n", __func__, result, *center_temperature);
        return 0;
    }
    
    memset(&result_temp_center, 0, sizeof(result_temp_center));
    *center_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_10, ADC_MPP_1_AMUX7, &result_temp_center);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] read err ADC MPP9 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_center.adc_code,
                                                                    result_temp_center.physical);
        } else {
            *center_temperature = (int)result_temp_center.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17048_error_sequence();
    }
    
    MAX17048_DBGLOG("[%s] from ADC Result:%d center_temperature = %d\n", __func__, result, *center_temperature);
    
    return result;
}

static int max17048_set_center_temperature(struct max17048_chip *chip, int center_temperature)
{
    int result = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    MAX17048_DBGLOG("[%s] center_temperature = %d \n", __func__, center_temperature);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_center = (center_temperature / 10);
    
    return result;
}

static int max17048_get_battery_capacity(struct max17048_chip *chip, int *battery_capacity)
{
	int result = 0;
	int capacity_now = 0;
	int i = 0;

	MAX17048_DBGLOG("[%s] in\n", __func__);

	if (chip == NULL) {
		MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
		if (the_chip == NULL) {
			MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
			return -1;
		}
		chip = the_chip;
	}

	if (battery_capacity == NULL) {
		MAX17048_ERRLOG("[%s] battery_capacity pointer is NULL\n", __func__);
		return -1;
	}

	for (i = 0; i < 3; i++) {
		result = max17048_get_soc(chip, battery_capacity);
		capacity_now = *battery_capacity;

		if (result < 0) {
			MAX17048_ERRLOG("[%s]get soc error:%d\n", __func__, result);
		} else {
			break;
		}
	}

	if (result < 0) {
		max17048_error_sequence();
	}

	if (capacity_now > MAX17048_BATTERY_FULL) {
		*battery_capacity = MAX17048_BATTERY_FULL;
	} else if (capacity_now <= 0) {
		*battery_capacity = 0;
	} else {
		*battery_capacity = capacity_now;
	}

	if (*battery_capacity == MAX17048_LOWBATT_SOC) {
		if (fj_lowbatt_soc_detect_cnt > 0) {
			fj_lowbatt_soc_detect_cnt--;
		}
		if (fj_lowbatt_soc_detect_cnt == 0) {
			chip->lowbattery = 1;
			fj_lowbatt_soc_detect_cnt = 3;
			MAX17048_ERRLOG("[%s] low battery detect\n", __func__);
			MAX17048_RECLOG("[%s] low battery detect\n", __func__);
		}
	} else {
        if (chip->status == POWER_SUPPLY_STATUS_CHARGING) {
            fj_lowbatt_soc_detect_cnt = 3;
            chip->lowbattery = 0;
        }
	}

	MAX17048_DBGLOG("[%s] battery_capacity = %d \n", __func__, *battery_capacity);

	return result;
}

static int max17048_effect_battery_capacity(struct max17048_chip *chip, int battery_capacity)
{
	int capacity = chip->soc;
	unsigned long now = jiffies;

	MAX17048_DBGLOG("[%s] status:%d\n", __func__, chip->status);
	MAX17048_DBGLOG("[%s] new soc:%d, now soc:%d\n", __func__, battery_capacity, chip->soc);
	MAX17048_DBGLOG("[%s] exp = %lu, now = %lu, mask:%d\n", __func__, fj_soc_mask_expire, now, fj_soc_mask_start);

	if( (fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE) ) {
		if (chip->lowbattery != 0) {
			/* low battery -> set capacity to 0 */
			capacity = 0;
			fj_soc_mask_start = 0;
		} else if (chip->status == POWER_SUPPLY_STATUS_FULL) {
			if ((fj_keep_full_status != 0) &&
				(battery_capacity < MAX17048_BATTERY_99)) {
				capacity = chip->soc - 1;
				fj_keep_full_status = 0;
				chip->status = POWER_SUPPLY_STATUS_CHARGING;
				MAX17048_INFOLOG("[%s] return to charging status\n", __func__);
			} else {
				capacity = MAX17048_BATTERY_FULL;
			}
			fj_soc_mask_start = 0;
		} else if (time_before(now, fj_soc_mask_expire) && fj_soc_mask_start) {
			/* not update soc from start charging or detect full (60secs) */
			MAX17048_DBGLOG("[%s] not update soc indicator\n", __func__);
			capacity = chip->soc;
		} else {
			fj_soc_mask_start = 0;
			fj_soc_mask_expire = 0;

			if (battery_capacity < chip->soc) {
				capacity = chip->soc - 1;
			} else if (chip->soc < battery_capacity) {
				if (chip->status == POWER_SUPPLY_STATUS_CHARGING) {
					capacity = battery_capacity;
				} else {
					capacity = chip->soc;
				}
			} else {
				capacity = chip->soc;
			}
			if ((chip->status == POWER_SUPPLY_STATUS_CHARGING) && 
				(capacity > MAX17048_BATTERY_99)) {
				capacity = MAX17048_BATTERY_99;
			}
		}
	} else {
		capacity = battery_capacity;
	}

	MAX17048_INFOLOG("[%s] capacity = %d \n", __func__, capacity);

	return capacity;
}

static int max17048_set_battery_capacity(struct max17048_chip *chip, int battery_capacity)
{
	int result = 0;

	MAX17048_DBGLOG("[%s] in\n", __func__);
	MAX17048_DBGLOG("[%s] battery_capacity = %d \n", __func__, battery_capacity);

	if (chip == NULL) {
		MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
		if (the_chip == NULL) {
			MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
			return -1;
		}
		chip = the_chip;
	}
	chip->soc = max17048_effect_battery_capacity(chip, battery_capacity);

	return result;
}

static int max17048_get_vbat_adjust(struct max17048_chip *chip, int *batt_offset)
{
    int result = 0;
    uint8_t val = 0x00;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (batt_offset == NULL) {
        MAX17048_ERRLOG("[%s] batt_offset pointer is NULL\n", __func__);
        return -1;
    }
    
    if ((fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE)) {
        result = get_nonvolatile(&val, APNV_CHG_ADJ_VBAT_I, 1);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] NV read err : result = %d set value = 0x00\n", __func__, result);
            val = 0x00;
        }
        if (val > 0x1F) {
            MAX17048_ERRLOG("[%s] NV read err : value = 0x%02x set value = 0x00\n", __func__, val);
            val = 0x00;
        }
    } else {
        val = 0x00;
    }
    
    *batt_offset = val;
    MAX17048_INFOLOG("[%s] get NV value batt_offset = 0x%02x\n", __func__, *batt_offset);
    return result;
}

static int max17048_set_vbat_adjust(struct max17048_chip *chip, int batt_offset)
{
	MAX17048_DBGLOG("[%s] in\n", __func__);
	MAX17048_INFOLOG("[%s] batt_offset = 0x%02x \n", __func__, batt_offset);

	if (chip == NULL) {
		MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
		if (the_chip == NULL) {
			MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
			return -1;
		}
		chip = the_chip;
	}
	chip->vbat_adj = batt_offset;
	MAX17048_DBGLOG("[%s] set dev info value chip->vbat_adj = 0x%02x\n", __func__, chip->vbat_adj);
	return 0;
}

static void max17048_set_rcomp(struct i2c_client *client, int battery_temperature)
{
	s32 result = 0;
	u8  rcomp[2] = {0};
	u16 comp = 0;
	u8  initial = 0;
	int tempcohot = 0;
	int tempcocold = 0;
	
	MAX17048_DBGLOG("[%s] in\n", __func__);
	
	if (client == NULL) {
		MAX17048_WARNLOG("[%s] client pointer is NULL\n", __func__);
		if (the_chip == NULL) {
			MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
			return;
		}
		client = the_chip->client;
	}
	
	battery_temperature = battery_temperature / 10;
	
	if (battery_temperature < -16) {
		if ((fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE)) {
			MAX17048_WARNLOG("[%s] battery_temperature param is invalid : battery_temperature = %d \n", 
								__func__, battery_temperature);
		}
		battery_temperature = 20;
	}
	
	mutex_lock(&the_chip->lock);
	/*
	 * rcomp value is calculated in accordance with the following formula.
	 *   InitialRCOMP = 0x008C(4.35V) / 0x0082(4.20V)
	 *   TempCoHot    = -0.70 (4.35V) / -0.75 (4.20V)
	 *   TempCoCold   = -1.75 (4.35V) / -0.00 (4.20V)
	 *     rcomp = Initial RCOMP                                (TEMP = 20C)
	 *     rcomp = Initial RCOMP + ((TEMP - 20) * TempCoHot)    (TEMP > 20C)
	 *     rcomp = Initial RCOMP + ((TEMP - 20) * TempCoCold)   (TEMP < 20C)
	 */
	if (max17048_435V_chg_flag) {
	
		initial    = MAX17048_RCOMP_INIT_435;
		tempcohot  = -70;
		tempcocold = -175;
	} else {
		initial    = MAX17048_RCOMP_INIT_42;
		tempcohot  = -75;
		tempcocold = 0;
	}
	
	if (battery_temperature == 20) {
		rcomp[0] = initial;
	} else if (battery_temperature > 20) {
		rcomp[0] = ((initial*100) + ((battery_temperature - 20) * tempcohot)) / 100;
	} else {
		rcomp[0] = ((initial*100) + ((battery_temperature - 20) * tempcocold)) / 100;
	}
	rcomp[1] = 0x40;
	
	memcpy(&comp, rcomp, sizeof(u16));
	
	/* unmask max17048 registers */
	result = max17048_write_reg(client, MAX17048_ACCESS_MSB, MAX17048_ACCESS_UNMASK);
	if (result < 0) {
		MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
		goto max17048_set_rcomp_err;
	}
	
	/* write rcomp */
	result = max17048_write_reg(client, MAX17048_CONFIG_MSB, comp);
	if (result < 0) {
		MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
		goto max17048_set_rcomp_err;
	}
	
	MAX17048_DBGLOG("[%s] comp = 0x%04X\n", __func__, comp);
	
	MAX17048_DBGLOG("[%s] RCOMP 0x%x\n", __func__,
						be16_to_cpu(max17048_read_reg(client, MAX17048_CONFIG_MSB)));
	
	/* mask max17048 registers */
	result = max17048_write_reg(client, MAX17048_ACCESS_MSB, MAX17048_ACCESS_MASK);
	if (result < 0) {
		MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
		goto max17048_set_rcomp_err;
	}
	
	mutex_unlock(&the_chip->lock);
	
	return;

max17048_set_rcomp_err:
	mutex_unlock(&the_chip->lock);
	max17048_error_sequence();
	
	return;
}

static void max17048_battery_monitor(struct work_struct *work)
{
    struct delayed_work *dwork;
    struct max17048_chip *chip;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (work == NULL) {
        MAX17048_ERRLOG("[%s] work pointer is NULL\n", __func__);
        return;
    }
    
    dwork = to_delayed_work(work);
    if (dwork == NULL) {
        MAX17048_ERRLOG("[%s] dwork pointer is NULL\n", __func__);
        return;
    }
    
    chip = container_of(dwork, struct max17048_chip, battery_monitor);
    if (chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return;
    }
    
    set_bit(MAX17048_EVENT_BATT_MONITOR, &chip->event_state);
    max17048_wakeup(chip);
}

static void max17048_mobile_temp_monitor(struct work_struct *work)
{
    struct delayed_work *dwork;
    struct max17048_chip *chip;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (work == NULL) {
        MAX17048_ERRLOG("[%s] work pointer is NULL\n", __func__);
        return;
    }
    
    dwork = to_delayed_work(work);
    if (dwork == NULL) {
        MAX17048_ERRLOG("[%s] dwork pointer is NULL\n", __func__);
        return;
    }
    
    chip = container_of(dwork, struct max17048_chip, mt_monitor);
    if (chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return;
    }
    
    set_bit(MAX17048_EVENT_MT_MONITOR, &chip->event_state);
    max17048_wakeup(chip);
}

static void max17048_delayed_init(struct max17048_chip *chip)
{
    int batt_offset = 0x00;
    int i = 0;
    int temp = 0;
    int temp_batt = 0;
    int battery_capacity = 0;
    int vbat_raw = 0;
    int temp_ambient = 0;
    int temp_case = 0;
    int temp_charge = 0;
    int temp_center = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    max17048_charge_function_limits_check(chip);
    max17048_get_vbat_adjust(chip, &batt_offset);
    max17048_set_vbat_adjust(chip, batt_offset);
    
    chip->lowbattery = 0;
    
    /* set suitable rcomp data */
    for (i = 0;i < 3;i++) {
        max17048_get_battery_temperature(chip, &temp);
        temp_batt = temp_batt + temp;
    }
    
    temp_batt = temp_batt / 3;
    max17048_set_rcomp(chip->client, temp_batt);
    max17048_set_battery_temperature(chip, temp_batt);
    
    msleep(300);    /* wait before 1st get soc */
    max17048_get_soc(chip, &battery_capacity);
    
    if (battery_capacity > MAX17048_BATTERY_FULL) {
        battery_capacity = MAX17048_BATTERY_FULL;
    } else if (battery_capacity <= 0) {
        battery_capacity = 0;
    }
    /* set 1st soc here */
    chip->soc = battery_capacity;
    max17048_set_battery_capacity(chip, battery_capacity);
    
    max17048_get_battery_voltage(chip, &vbat_raw);
    max17048_set_battery_voltage(chip, vbat_raw);
    
    max17048_get_terminal_temperature(chip, &temp_ambient);
    max17048_set_terminal_temperature(chip, temp_ambient);
    
    max17048_get_receiver_temperature(chip, &temp_case);
    max17048_set_receiver_temperature(chip, temp_case);
    
    max17048_get_charge_temperature(chip, &temp_charge);
    max17048_set_charge_temperature(chip, temp_charge);
    
    max17048_get_center_temperature(chip, &temp_center);
    max17048_set_center_temperature(chip, temp_center);
    
    max17048_convert_batt_voltage(chip);
    
    power_supply_changed(&chip->battery);
    
    max17048_initialized = 1;
    
    MAX17048_INFOLOG("[%s] chip->soc = %d \n", __func__, chip->soc);
    MAX17048_INFOLOG("[%s] chip->temp_batt = %d chip->vbat_raw = 0x%04x \n",
                         __func__, chip->temp_batt, chip->vbat_raw);
    MAX17048_INFOLOG("[%s] chip->temp_ambient = %d chip->temp_case = %d\n",
                         __func__, chip->temp_ambient, chip->temp_case);
    return;
}

static void max17048_monitor_work(struct max17048_chip *chip)
{
    int i = 0;
    int temp = 0;
    int batt_temp = 0;
    int vbat = 0;
    int vbat_raw = 0;
    int capacity = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    /* avarage 4 times value */
    for (i = 0;i < 4;i++) {
        max17048_get_battery_temperature(chip, &temp);
        batt_temp += temp;
        
        max17048_get_battery_voltage(chip, &vbat);
        vbat_raw += vbat;
        
        msleep(25);
    }
    batt_temp = batt_temp / 4;
    vbat_raw = vbat_raw / 4;
    
    max17048_set_rcomp(chip->client, batt_temp);
    max17048_set_battery_temperature(chip, batt_temp);
    
    max17048_get_battery_capacity(chip, &capacity);
    max17048_set_battery_capacity(chip, capacity);
    
    max17048_set_battery_voltage(chip, vbat_raw);
    max17048_convert_batt_voltage(chip);
    
    MAX17048_DBGLOG("[%s] chip->temp_batt = %d \n", __func__, chip->temp_batt);
    MAX17048_DBGLOG("[%s] chip->vbat_raw = 0x%04x \n", __func__, chip->vbat_raw);
    
    return;
}

static void max17048_alert_work(struct max17048_chip *chip)
{
    int result = 0;
    u16 status = 0;
    int i = 0;
    int temp = 0;
    int batt_temp = 0;
    int vbat = 0;
    int vbat_raw = 0;
    int capacity = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    /* read interrupt status */
    result = max17048_read_reg(chip->client, MAX17048_STATUS_MSB);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17048_alert_work_err;
    }
    status = result;
    MAX17048_DBGLOG("[%s] interrupt status: 0x%04x\n", __func__, status);
    
    /* soc interrupt */
    if (status & MAX17048_STATUS_SOC) {
        mutex_lock(&chip->lock);
        /* unmask max17048 registers */
        result = max17048_write_reg(chip->client, MAX17048_ACCESS_MSB, MAX17048_ACCESS_UNMASK);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
            goto max17048_alert_work_err;
        }
        /* clear interrupt status */
        result = max17048_write_reg(chip->client, MAX17048_STATUS_MSB, MAX17048_STATUS_SOC);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
            goto max17048_alert_work_err;
        }
        /* mask max17048 registers */
        result = max17048_write_reg(chip->client, MAX17048_ACCESS_MSB, MAX17048_ACCESS_MASK);
        if (result < 0) {
            MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
            goto max17048_alert_work_err;
        }
        mutex_unlock(&chip->lock);
        
        /* avarage 4 times value */
        for (i = 0;i < 4;i++) {
            max17048_get_battery_temperature(chip, &temp);
            batt_temp += temp;
            
            max17048_get_battery_voltage(chip, &vbat);
            vbat_raw += vbat;
            
            msleep(25);
        }
        batt_temp = batt_temp / 4;
        vbat_raw = vbat_raw / 4;
        
        max17048_set_rcomp(chip->client, batt_temp);
        max17048_set_battery_temperature(chip, batt_temp);
        
        max17048_get_battery_capacity(chip, &capacity);
        max17048_set_battery_capacity(chip, capacity);
        
        max17048_set_battery_voltage(chip, vbat_raw);
        max17048_convert_batt_voltage(chip);
        
        MAX17048_DBGLOG("[%s] chip->temp_batt = %d \n", __func__, chip->temp_batt);
        MAX17048_DBGLOG("[%s] chip->vbat_raw = 0x%04x \n", __func__, chip->vbat_raw);
    }
    /* low battery */
    if (status & MAX17048_STATUS_VL) {
        cancel_delayed_work(&chip->battery_monitor);
        chip->monitor_interval = MAX17048_MONITOR_DELAY_100MS;
        schedule_delayed_work(&chip->battery_monitor, chip->monitor_interval);
        MAX17048_DBGLOG("[%s] low battery interrupt \n", __func__);
        MAX17048_RECLOG("[%s] low battery interrupt \n", __func__);
    }
    
    return;
    
max17048_alert_work_err:
    mutex_unlock(&chip->lock);
    max17048_error_sequence();
    return;
}

static void max17048_monitor_mobile_temp(struct max17048_chip *chip)
{
    int temp_ambient = 0;
    int temp_case = 0;
    int temp_charge = 0;
    int temp_center = 0;
    int temp_update_flag = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    max17048_get_terminal_temperature(chip, &temp_ambient);
    if (chip->temp_ambient != (temp_ambient/10)) {
        temp_update_flag |= 0x0001;
    }
    max17048_set_terminal_temperature(chip, temp_ambient);
    
    max17048_get_receiver_temperature(chip, &temp_case);
    if (chip->temp_case != (temp_case/10)) {
        temp_update_flag |= 0x0002;
    }
    max17048_set_receiver_temperature(chip, temp_case);
    
    max17048_get_charge_temperature(chip, &temp_charge);
    if (chip->temp_charge != (temp_charge/10)) {
        temp_update_flag |= 0x0004;
    }
    max17048_set_charge_temperature(chip, temp_charge);
    
    max17048_get_center_temperature(chip, &temp_center);
    if (chip->temp_center != (temp_center/10)) {
        temp_update_flag |= 0x0008;
    }
    max17048_set_center_temperature(chip, temp_center);
    
    power_supply_changed(&chip->battery);
    MAX17048_DBGLOG("[%s] temp_update!! = %x\n", __func__, temp_update_flag);
    
    MAX17048_DBGLOG("[%s] chip->temp_ambient = %d\n", __func__, chip->temp_ambient);
    MAX17048_DBGLOG("[%s] chip->temp_case = %d\n", __func__, chip->temp_case);
    MAX17048_DBGLOG("[%s] chip->temp_charge = %d\n", __func__, chip->temp_charge);
    MAX17048_DBGLOG("[%s] chip->temp_center = %d\n", __func__, chip->temp_center);
    
    return;
}

static int max17048_is_update_params(struct max17048_chip *chip)
{
	int ret = 0;
	static unsigned char old_lowbatt = 0;

	MAX17048_DBGLOG("[%s] in\n", __func__);

	if (chip == NULL) {
		MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
		if (the_chip == NULL) {
			MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
			return -1;
		}
		chip = the_chip;
	}

	if (old_lowbatt != chip->lowbattery) {
		MAX17048_WARNLOG("[%s] update low batt info(%d -> %d)\n", __func__, old_lowbatt, chip->lowbattery);
		old_lowbatt = chip->lowbattery;
		ret = 1;
	}

	MAX17048_DBGLOG("[%s] ret:%d\n", __func__, ret);
	return ret;
}

static void max17048_charge_function_limits_check(struct max17048_chip *chip)
{
    int result = 0;
    int i = 0;
    u16 val = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    /* get nv value */
    result = get_nonvolatile((uint8_t*)&val, APNV_FUNC_LIMITS_I, 2);
    MAX17048_DBGLOG("[%s] NV read : get value = 0x%04x \n", __func__, val);
    if (result < 0) {
        val = 0x0000;
        MAX17048_ERRLOG("[%s] NV read err : set value = 0x%x \n", __func__, val);
    }
    
    /*
     * Limits function classification
     * check_disable[0] : charge
     * check_disable[1] : battery present
     * check_disable[2] : low battery
     * check_disable[3] : reserve
     * check_disable[4] : battery temperature
     * check_disable[5] : terminal temperature
     * check_disable[6] : receiver temperature
     * check_disable[7] : charge temperature
     * check_disable[8] : center temperature
     */
    if (val != MAX17048_FUNC_NO_LIMIT) {
        for (i = 0;i < MAX17048_FUNC_LIMIT_NUM;i++) {
            if (val & 0x0001) {
                chip->check_disable[i] = true;
            } else {
                chip->check_disable[i] = false;
            }
            val = val >> 1;
            MAX17048_DBGLOG("[%s] check_disable[%d] = %d\n", __func__, i, chip->check_disable[i]);
        }
    }
}

static int max17048_charge_battery_present_check(struct max17048_chip *chip)
{
    static int batt_absent_count = 0;
    int batt_present = 1;
    int temp = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    max17048_get_battery_temperature(chip, &temp);
    
    if (temp <= BATTERY_NOT_PRESENT_TEMP) {
        MAX17048_WARNLOG("[%s]battery temp(%d)\n", __func__, temp);
        batt_absent_count++;
    } else {
        batt_absent_count = 0;
    }
    
    if (batt_absent_count < 3) {
        batt_present = 1;
    } else {
        batt_present = 0;
        if ((fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE)) {
            MAX17048_ERRLOG("[%s]remove battery\n", __func__);
            /* GCF Support */
            if (chip->check_disable[MAX17048_FUNC_LIMIT_BATTERY_PRESENT] == false) {
                fj_chg_emergency_current_off();
                kernel_power_off();
            }
        }
    }
    
    max17048_set_battery_temperature(chip, temp);
    MAX17048_DBGLOG("[%s] batt_present = %d\n", __func__, batt_present);
    
    return batt_present;
}

static void max17048_battery_present_monitor(struct work_struct *work)
{
    struct delayed_work *dwork;
    struct max17048_chip *chip;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (work == NULL) {
        MAX17048_ERRLOG("[%s] work pointer is NULL\n", __func__);
        return;
    }
    
    dwork = to_delayed_work(work);
    if (dwork == NULL) {
        MAX17048_ERRLOG("[%s] dwork pointer is NULL\n", __func__);
        return;
    }
    
    chip = container_of(dwork, struct max17048_chip, battery_present_monitor);
    if (chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return;
    }
    
    set_bit(MAX17048_EVENT_PRESENT_MONITOR, &chip->event_state);
    max17048_wakeup(chip);
}

static irqreturn_t max17048_alert_isr(int irq_no, void *data)
{
    struct max17048_chip *chip =(struct max17048_chip *)data;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    set_bit(MAX17048_EVENT_ALERT_ISR, &chip->event_state);
    max17048_wakeup(chip);
	
    wake_lock_timeout(&max17048_chg_status, (3 * HZ));
    
    return IRQ_HANDLED;
}

static void max17048_setup(struct max17048_chip *chip)
{
    int result = 0;
    int i = 0;
    int soc = 0;
    u16 rcomp = 0;
    u16 ocv = 0;
    u16 ocv1 = 0;
    u16 ocv2 = 0;
    u16 tmp = 0;
    u8 soc1 = 0;
    u8 soc2 = 0;
    u8 soc_check1 = 0;
    u8 soc_check2 = 0;
    u16 *p_batterydata_table = 0;
    u16 batterydata_tablesize = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    /* unmask max17048 registers */
    result = max17048_write_reg(chip->client, MAX17048_ACCESS_MSB, MAX17048_ACCESS_UNMASK);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    MAX17048_DBGLOG("[%s] ACCESS: 0x%x\n", __func__, be16_to_cpu(max17048_read_reg(chip->client, MAX17048_ACCESS_MSB)));
    
    /* get Initial OCV */
    result = max17048_read_reg(chip->client, MAX17048_OCV_MSB);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    ocv1 = result;
    MAX17048_DBGLOG("[%s] OCV 0x%04x\n", __func__, ocv1);
	
	/* set TEST OCV */
	if (max17048_435V_chg_flag) {
		ocv = MAX17048_OCV_TEST_435;
	} else {
		ocv = MAX17048_OCV_TEST_42;
	}
	result = max17048_write_reg(chip->client, MAX17048_OCV_MSB, ocv);
	if (result < 0) {
		MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
		goto max17048_setup_err;
	}
	MAX17048_DBGLOG("[%s] OCV: 0x%x\n", __func__, be16_to_cpu(max17048_read_reg(chip->client, MAX17048_OCV_MSB)));
	
	/* TEST RCOMP setup */
	result = max17048_write_reg(chip->client, MAX17048_CONFIG_MSB, MAX17048_RCOMP_TEST);
	if (result < 0) {
		MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
		goto max17048_setup_err;
	}
	MAX17048_DBGLOG("[%s] RCOMP: 0x%x\n", __func__, be16_to_cpu(max17048_read_reg(chip->client, MAX17048_CONFIG_MSB)));
	
	/* write battery data */
	if (max17048_435V_chg_flag) {
		p_batterydata_table   = (u16 *)max17048_table_435;
		batterydata_tablesize = ARRAY_SIZE(max17048_table_435);
	} else {
		p_batterydata_table   = (u16 *)max17048_table_42;
		batterydata_tablesize = ARRAY_SIZE(max17048_table_42);
	}
	for (i = 0; i < batterydata_tablesize; i++) {
		result = max17048_write_reg(chip->client, 0x40 + (2 * i), p_batterydata_table[i]);
		if (result < 0) {
			MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
			goto max17048_setup_err;
		}
	}
	
	msleep(150);
	
	result = max17048_write_reg(chip->client, MAX17048_OCV_MSB, ocv);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    
    /* we must wait 150~600ms */
    msleep(150);
    
    /* read soc */
    soc = max17048_read_reg(chip->client, MAX17048_SOC_MSB);
    if (soc < 0) {
        MAX17048_ERRLOG("[%s] i2c read err : result = %d \n", __func__, soc);
        goto max17048_setup_err;
    }
    soc1 = (u8)(soc&0x00ff);
    soc2 = (u8)((soc&0xff00)>>8);
    MAX17048_DBGLOG("[%s] SOC1: 0x%x SOC2: 0x%x\n", __func__, soc1, soc2);
    
    if (max17048_435V_chg_flag) {
        soc_check1 = 0x71;
        soc_check2 = 0x73;
    } else {
        soc_check1 = 0x75;
        soc_check2 = 0x77;
    }
    
    if (soc1 >= soc_check1 && soc1 <= soc_check2) {
        MAX17048_DBGLOG("[%s] model data write OK\n", __func__);
    } else {
        MAX17048_ERRLOG("[%s] model data write NG soc1 = 0x%02X\n", __func__, soc1);
        goto max17048_setup_err;
    }
    
    /* Quick Start */
    result = max17048_write_reg(chip->client, MAX17048_MODE_MSB, 0x0040);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    
    /* we must wait 300ms */
    msleep(300);
    
    result = max17048_read_reg(chip->client, MAX17048_OCV_MSB);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    ocv2 = result;
    MAX17048_DBGLOG("[%s] OCV %04x\n", __func__, ocv2);
    
    if (gpio_get_value(PM8921_GPIO_PM_TO_SYS(MAX17048_VC_DET_GPIO)) &&
        gpio_get_value(PM8921_GPIO_PM_TO_SYS(MAX17048_VB_DET_GPIO))) {
        tmp = (ocv2 & 0x00FF) << 4 | (ocv2 & 0xF000) >> 12;
        /* ocv2 + 40mV/1.25mV = 35(dec) = + 0x20(hex)*/
        tmp += 0x20;
        ocv2 = (tmp & 0x000F) << 12 | (tmp & 0x0FF0) >> 4;
        
        if(be16_to_cpu(ocv1) >= be16_to_cpu(ocv2)) {
            ocv = ocv1;
        } else {
            ocv = ocv2;
        }
    } else {
        ocv = ocv2;
    }
    
    /* restore initial OCV */
    result = max17048_write_reg(chip->client, MAX17048_OCV_MSB, ocv);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    
    /* restore initial RCOMP */
    if (max17048_435V_chg_flag) {
        rcomp = MAX17048_CONFIG_INIT_435;
    } else {
        rcomp = MAX17048_CONFIG_INIT_42;
    }
    
    result = max17048_write_reg(chip->client, MAX17048_CONFIG_MSB, rcomp);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    
    /* disable hibernate mode  */
    result = max17048_write_reg(chip->client, MAX17048_HIBRT_MSB, MAX17048_HIBRT_INIT);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    
    /* write interrupt value */
    result = max17048_write_reg(chip->client, MAX17048_VALRT_MSB, MAX17048_VALRT_INIT);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    
    /* init interrupt status */
    result = max17048_write_reg(chip->client, MAX17048_STATUS_MSB, MAX17048_STATUS_INIT);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    
    /* mask max17048 registers */
    result = max17048_write_reg(chip->client, MAX17048_ACCESS_MSB, MAX17048_ACCESS_MASK);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17048_setup_err;
    }
    
    return;
    
max17048_setup_err:
    max17048_error_sequence();
    
    return;
}

static void max17048_interrupt_init(struct max17048_chip *chip)
{
    int irq, error;
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    if(gpio_request(MAX17048_INT_GPIO, "MAX17048_INT") < 0) {
        MAX17048_ERRLOG("%s:GPIO INT Request failed\n", __func__);
        return;
    }
    
    gpio_direction_input(MAX17048_INT_GPIO);
    irq = gpio_to_irq(MAX17048_INT_GPIO);
    error = request_irq(irq, max17048_alert_isr,
                        (IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND),
                        "MAX17048_INT", chip);
    enable_irq_wake(irq);
}

static void max17048_error_sequence(void)
{
	MAX17048_ERRLOG("[%s] max17048 I2C error -> Shutdown\n", __func__);
	fj_chg_emergency_current_off();
	kernel_power_off();
}

static int max17048_thread(void * ptr)
{
    struct max17048_chip *chip = ptr;
    int result = 0;
    
    MAX17048_DBGLOG("[%s] wakeup\n", __func__);
    
    if (chip == NULL) {
        MAX17048_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
	
	fj_chg_chgic_reset();
	
	max17048_get_model_type(chip);
	/* set hardware specific data */
	max17048_setup(chip);
	
	if (battery_status != POWER_SUPPLY_STATUS_DISCHARGING) {
		MAX17048_INFOLOG("[%s] status %d > %d\n", __func__, chip->status, battery_status);
		chip->status = battery_status;
	}
	
	if (battery_health != POWER_SUPPLY_HEALTH_GOOD) {
		MAX17048_INFOLOG("[%s] health %d > %d\n", __func__, chip->health, battery_health);
		chip->health = battery_health;
	}
	/* set wakeup interrupt */
	max17048_interrupt_init(chip);
	
	max17048_delayed_init(chip);
	
	chip->present = 1;
	chip->monitor_interval = MAX17048_MONITOR_DELAY_300MS;
	chip->present_monitor_interval = MAX17048_MONITOR_DELAY_10S;
	chip->mt_interval = MAX17048_MONITOR_DELAY_1S;
	
	ovp_device_initialized(INITIALIZE_SUPPLY);
	
	schedule_delayed_work(&chip->battery_present_monitor, chip->present_monitor_interval);
	schedule_delayed_work(&chip->mt_monitor, chip->mt_interval);
	
	set_freezable();

    do {
        wait_event_freezable(chip->wq, (max17048_get_queue_state() || kthread_should_stop()));
        
        if (kthread_should_stop()) {
            MAX17048_INFOLOG("[%s] while loop break\n", __func__);
            break;
        }
        
        if (test_bit(MAX17048_EVENT_SUSPEND, &chip->event_state) == 0) {
            max17048_clear_queue_state();
            MAX17048_DBGLOG("[%s] event:0x%08x\n", __func__, (int)chip->event_state);
            
            if (test_bit(MAX17048_EVENT_PRESENT_MONITOR, &chip->event_state)) {
                MAX17048_DBGLOG("[%s] event %s\n", __func__, "PRESENT_MONITOR");
                max17048_charge_battery_present_check(chip);
                clear_bit(MAX17048_EVENT_PRESENT_MONITOR, &chip->event_state);
                schedule_delayed_work(&chip->battery_present_monitor, chip->present_monitor_interval);
            }
            
            if (test_bit(MAX17048_EVENT_BATT_MONITOR, &chip->event_state)) {
                MAX17048_DBGLOG("[%s] event %s\n", __func__, "BATT_MONITOR");
                max17048_monitor_work(chip);
                if (max17048_is_update_params(chip)) {
                    power_supply_changed(&chip->battery);
                }
                if (chip->lowbattery != 0) {
                    /* low battery */
                    chip->monitor_interval = MAX17048_MONITOR_DELAY_300MS;
                    schedule_delayed_work(&chip->battery_monitor, chip->monitor_interval);
                } else  if ((fj_lowbatt_volt_detect_cnt != 3) ||
                            (fj_lowbatt_soc_detect_cnt != 3)) {
                    chip->monitor_interval = MAX17048_MONITOR_DELAY_100MS;
                    schedule_delayed_work(&chip->battery_monitor, chip->monitor_interval);
                }
                clear_bit(MAX17048_EVENT_BATT_MONITOR, &chip->event_state);
            }
            
            if (test_bit(MAX17048_EVENT_MT_MONITOR, &chip->event_state)) {
                MAX17048_DBGLOG("[%s] event %s\n", __func__, "MT_MONITOR");
                max17048_monitor_mobile_temp(chip);
                clear_bit(MAX17048_EVENT_MT_MONITOR, &chip->event_state);
                chip->mt_interval = MAX17048_MONITOR_DELAY_1S;
                schedule_delayed_work(&chip->mt_monitor, chip->mt_interval);
            }
            
            if (test_bit(MAX17048_EVENT_ALERT_ISR, &chip->event_state)) {
                MAX17048_DBGLOG("[%s] event %s\n", __func__, "ALERT_ISR");
                clear_bit(MAX17048_EVENT_ALERT_ISR, &chip->event_state);
                max17048_alert_work(chip);
            }
        }
    } while(1);
    
    return result;
}

int max17048_mc_quick_start(void)
{
    int result = 0;
    int temp = 0;
    int battery_capacity = 0;
    s32 rc = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        return -1;
    }
    
    max17048_get_battery_temperature(the_chip, &temp);
    max17048_set_rcomp(the_chip->client, temp);
    
    rc = max17048_write_reg(the_chip->client, MAX17048_MODE_MSB, 0x0040);
    
    msleep(300);
    
    max17048_get_battery_capacity(the_chip, &battery_capacity);
    max17048_set_battery_capacity(the_chip, battery_capacity);
    
    result = (int)rc;
    
    return result;
}

int max17048_mc_get_vcell(void)
{
    int result = 0;
    u16 vcell = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        return -1;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17048_read_reg(the_chip->client, MAX17048_VCELL_MSB);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] max17048_read_reg Failure!!!\n", __func__);
        goto max17048_err;
    }
    vcell = result;
    result = be16_to_cpu(vcell);
    
    mutex_unlock(&the_chip->lock);
    
max17048_err:
    return result;
}
module_param_call(maxim_battery_voltage, NULL, max17048_mc_get_vcell, NULL, 0444);

int max17048_mc_get_soc(void)
{
    int result = 0;
    int capacity = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    result = max17048_get_soc(the_chip, &capacity);
    if (result < 0) {
        MAX17048_ERRLOG("[%s] max17048_get_soc Failure!!!\n", __func__);
        goto max17048_err;
    }
    result = capacity;
    
max17048_err:
    return result;
}

int max17048_get_property_func(enum power_supply_property psp,
                                union power_supply_propval *val)
{
    int result = 0;
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return -EINVAL;
    }
    
    if (val == NULL) {
        MAX17048_ERRLOG("[%s] val pointer is NULL\n", __func__);
        return -EINVAL;
    }
    
    if(max17048_drv_initialized() == 0) {
        MAX17048_WARNLOG("[%s] error msm_battery_get_property called before init\n", __func__);
        return -EINVAL;
    }
    
    result = max17048_get_property(&the_chip->battery, psp, val);
    
    return result;
}

int max17048_set_status(int status)
{
    int old_status = 0;
    int status_update_flag = 0;
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        return -1;
    }
    
    old_status = the_chip->status;
    MAX17048_DBGLOG("[%s] in (status:%s)\n", __func__, fj_ps_status_str[status]);
    if (max17048_initialized == 1) {
        if(old_status != status) {
            MAX17048_INFOLOG("[%s] status %s > %s\n", __func__,
                                fj_ps_status_str[the_chip->status],
                                fj_ps_status_str[status]);
            MAX17048_RECLOG("[%s] status %s > %s capa:%d volt:%dmV\n", __func__,
                                fj_ps_status_str[the_chip->status],
                                fj_ps_status_str[status],
                                the_chip->soc,
                                the_chip->vcell);
            status_update_flag++;
        }
        
        if ((old_status == POWER_SUPPLY_STATUS_DISCHARGING) &&
            (status == POWER_SUPPLY_STATUS_CHARGING) &&
            (the_chip->soc > 0)) {
            fj_soc_mask_expire = jiffies + (60 * HZ);   /* 60 sec */
            fj_soc_mask_start = 1;
        }
        
        if (status == POWER_SUPPLY_STATUS_FULL) {
            the_chip->soc = MAX17048_BATTERY_FULL;
            fj_soc_mask_expire = jiffies + (30 * HZ);   /* 30 sec */
            fj_soc_mask_start = 1;
        }
        
        if (status == POWER_SUPPLY_STATUS_DISCHARGING) {
            fj_soc_mask_expire = jiffies + (30 * HZ);   /* 30 sec */
            fj_soc_mask_start = 1;
        }
        
        if ((status == POWER_SUPPLY_STATUS_CHARGING) &&
            (the_chip->soc == MAX17048_BATTERY_FULL)) {
            MAX17048_INFOLOG("[%s] keep full status\n", __func__);
            fj_keep_full_status = 1;
            the_chip->status = POWER_SUPPLY_STATUS_FULL;
        } else {
            fj_keep_full_status = 0;
            the_chip->status = status;
        }
        
        if (msm_touch_callback.callback != NULL) {
            msm_touch_callback.callback(status, msm_touch_callback.data);
        }
        
        MAX17048_DBGLOG("[%s] status %d, soc %d\n", __func__, the_chip->status, the_chip->soc);
        
        wake_lock_timeout(&max17048_chg_status, (3 * HZ));  /* for change power supply status */
        if (status_update_flag != 0) {
            power_supply_changed(&the_chip->battery);
        }
        
    } else {
        MAX17048_WARNLOG("[%s] called before init\n", __func__);
        battery_status = status;
    }
    return 0;
}
EXPORT_SYMBOL(max17048_set_status);

int max17048_set_health(int health)
{
    int result = 0;
    int health_update_flag = 0;
    
    MAX17048_DBGLOG("[%s] in (health:%s)\n", __func__, fj_ps_health_str[health]);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17048_initialized == 1) {
        if(the_chip->health != health) {
            MAX17048_INFOLOG("[%s] health %s > %s\n", __func__,
                                fj_ps_health_str[the_chip->health],
                                fj_ps_health_str[health]);
            MAX17048_RECLOG("[%s] health %s > %s capa:%d volt:%dmV\n", __func__,
                                fj_ps_health_str[the_chip->health],
                                fj_ps_health_str[health],
                                the_chip->soc,
                                the_chip->vcell);
            health_update_flag++;
        }
        the_chip->health = health;
        if (health_update_flag != 0) {
            power_supply_changed(&the_chip->battery);
        }
    } else {
        MAX17048_WARNLOG("%s called before init\n", __func__);
        battery_health = health;
    }
    
    return result;
}
EXPORT_SYMBOL(max17048_set_health);

int max17048_get_terminal_temp(int *terminal_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    if (terminal_temp == NULL) {
        MAX17048_ERRLOG("[%s] terminal_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    result = max17048_get_terminal_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17048_ERRLOG("[%s]get terminal temp error:%d\n", __func__, result);
        goto max17048_err;
    }
    *terminal_temp = (temp_work / 10);
    
max17048_err:
    return result;
}
EXPORT_SYMBOL(max17048_get_terminal_temp);

int max17048_get_receiver_temp(int *receiver_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    if (receiver_temp == NULL) {
        MAX17048_ERRLOG("[%s] receiver_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    result = max17048_get_receiver_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17048_ERRLOG("[%s]get receiver temp error:%d\n", __func__, result);
        goto max17048_err;
    }
    *receiver_temp = (temp_work / 10);
    
max17048_err:
    return result;
}
EXPORT_SYMBOL(max17048_get_receiver_temp);

int max17048_get_charge_temp(int *charge_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    if (charge_temp == NULL) {
        MAX17048_ERRLOG("[%s] charge_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    result = max17048_get_charge_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17048_ERRLOG("[%s]get charge temp error:%d\n", __func__, result);
        goto max17048_err;
    }
    *charge_temp = (temp_work / 10);
    
max17048_err:
    return result;
}
EXPORT_SYMBOL(max17048_get_charge_temp);

int max17048_get_center_temp(int *center_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    if (center_temp == NULL) {
        MAX17048_ERRLOG("[%s] center_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    result = max17048_get_center_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17048_ERRLOG("[%s]get center temp error:%d\n", __func__, result);
        goto max17048_err;
    }
    *center_temp = (temp_work / 10);
    
max17048_err:
    return result;
}
EXPORT_SYMBOL(max17048_get_center_temp);

int max17048_get_battery_temp(int *batt_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    if (batt_temp == NULL) {
        MAX17048_ERRLOG("[%s] battery_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    result = max17048_get_battery_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17048_ERRLOG("[%s]get battery temp error:%d\n", __func__, result);
        goto max17048_err;
    }
    
    *batt_temp = (temp_work / 10);
    
max17048_err:
    return result;
}
EXPORT_SYMBOL(max17048_get_battery_temp);

int max17048_set_battery_temp(int batt_temp)
{
    int result = 0;
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return -1;
    }
    
    the_chip->temp_batt = batt_temp;
    
    return result;
}
EXPORT_SYMBOL(max17048_set_battery_temp);

int max17048_get_batt_vbat_raw(int *batt_vbat_raw)
{
    int result = 0;
    int volt_work = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    if (batt_vbat_raw == NULL) {
        MAX17048_ERRLOG("[%s] batt_vbat_raw pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    result = max17048_get_battery_voltage(the_chip, &volt_work);
    if (result < 0) {
        MAX17048_ERRLOG("[%s]get vbat error:%d\n", __func__, result);
        goto max17048_err;
    }
    
    *batt_vbat_raw = volt_work;
    
max17048_err:
    return result;
}
EXPORT_SYMBOL(max17048_get_batt_vbat_raw);

int max17048_get_batt_capacity(int *batt_capacity)
{
    int i = 0;
    int result = 0;
    int capa_work = 0;
    
    MAX17048_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    if (batt_capacity == NULL) {
        MAX17048_ERRLOG("[%s] battery_capacity pointer is NULL\n", __func__);
        result = -1;
        goto max17048_err;
    }
    
    for (i = 0; i < 3; i++) {
        result = max17048_get_soc(the_chip, &capa_work);
        
        if (result < 0) {
            MAX17048_ERRLOG("[%s]get soc error:%d\n", __func__, result);
        } else {
            break;
        }
    }
    
    if (result < 0) {
        goto max17048_err;
    }
    
    *batt_capacity = capa_work;
    
max17048_err:
    return result;
}
EXPORT_SYMBOL(max17048_get_batt_capacity);

int max17048_drv_initialized(void)
{
    MAX17048_DBGLOG("[%s] in\n", __func__);
    return max17048_initialized;
}
EXPORT_SYMBOL(max17048_drv_initialized);

int max17048_set_callback_info(struct msm_battery_callback_info *info)
{
    int result = 0;
    
    if (info->callback == NULL) {
        MAX17048_ERRLOG("[%s] set callback function NULL pointar\n",  __func__);
        result = -1;
    } else if (set_callback_function_flag == 1) {
        MAX17048_ERRLOG("[%s] already callback function\n",  __func__);
        result = -1;
    } else {
        msm_touch_callback.callback = info->callback;
        msm_touch_callback.data     = info->data;
        set_callback_function_flag = 1;
        result = 0;
    }
    
    return result;
}

int max17048_unset_callback_info(void)
{
    int result = 0;
    
    if (the_chip == NULL) {
        MAX17048_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
    } else {
        msm_touch_callback.callback = NULL;
        msm_touch_callback.data     = NULL;
        set_callback_function_flag = 0;
    }
    
    return result;
}

extern int charging_mode;
static int __devinit max17048_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17048_chip *chip;
	int ret;

	MAX17048_DBGLOG("[%s] in\n", __func__);

	gpio_tlmm_config( GPIO_CFG(87, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	gpio_tlmm_config( GPIO_CFG(50, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, chip);

	chip->battery.name				= "battery";
	chip->battery.type				= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property		= max17048_get_property;
	chip->battery.properties		= max17048_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17048_battery_props);

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
		return ret;
	}

	chip->event_state = 0;	/* initialize */

	if (charging_mode)
		chip->status = POWER_SUPPLY_STATUS_CHARGING;
	else
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	chip->health = POWER_SUPPLY_HEALTH_GOOD;

	INIT_DELAYED_WORK(&chip->battery_monitor, max17048_battery_monitor);
	INIT_DELAYED_WORK_DEFERRABLE(&chip->battery_present_monitor, max17048_battery_present_monitor);
	INIT_DELAYED_WORK_DEFERRABLE(&chip->mt_monitor, max17048_mobile_temp_monitor);

	wake_lock_init(&max17048_chg_status, WAKE_LOCK_SUSPEND, "max17048_chg_status");

	mutex_init(&chip->lock);
	max17048_clear_queue_state();

	/* init wait queue */
	init_waitqueue_head(&chip->wq);

	the_chip = chip;

	/* kthread start */
	chip->thread = kthread_run(max17048_thread, chip, "max17048");

	return 0;
}

static int __devexit max17048_remove(struct i2c_client *client)
{
    struct max17048_chip *chip = i2c_get_clientdata(client);
    
    cancel_delayed_work(&chip->battery_monitor);
    cancel_delayed_work(&chip->battery_present_monitor);
    cancel_delayed_work(&chip->mt_monitor);
    power_supply_unregister(&chip->battery);
    kfree(chip);
    return 0;
}

#ifdef CONFIG_PM
static int max17048_suspend(struct device *dev)
{
	struct max17048_chip *chip = dev_get_drvdata(dev);
	
	MAX17048_DBGLOG("[%s] in\n", __func__);
	set_bit(MAX17048_EVENT_SUSPEND, &chip->event_state);
	
	cancel_delayed_work(&chip->battery_monitor);
	cancel_delayed_work(&chip->battery_present_monitor);
	cancel_delayed_work(&chip->mt_monitor);
	return 0;
}

static int max17048_resume(struct device *dev)
{
	struct max17048_chip *chip = dev_get_drvdata(dev);
	int battery_capacity = 0;
    int capacity_update_flag = 0;
	
	MAX17048_DBGLOG("[%s] in\n", __func__);
	max17048_get_battery_capacity(chip, &battery_capacity);
	if (battery_capacity < chip->soc) {
		chip->soc = battery_capacity;
        capacity_update_flag++;
	}
	
	max17048_set_battery_capacity(chip, battery_capacity);
	
    if (capacity_update_flag != 0) {
        power_supply_changed(&chip->battery);
    }
	
	max17048_clear_queue_state();
	clear_bit(MAX17048_EVENT_SUSPEND, &chip->event_state);
	
	schedule_delayed_work(&chip->battery_monitor, MAX17048_MONITOR_DELAY_500MS);
	schedule_delayed_work(&chip->battery_present_monitor, MAX17048_MONITOR_DELAY_500MS);
	schedule_delayed_work(&chip->mt_monitor, MAX17048_MONITOR_DELAY_500MS);
	return 0;
}

static const struct dev_pm_ops max17048_pm_ops = {
	.suspend = max17048_suspend,
	.resume = max17048_resume,
};

#else  /* CONFIG_PM */

#define max17048_suspend NULL
#define max17048_resume NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id max17048_id[] = {
	{ "max17048", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17048_id);

static struct i2c_driver max17048_i2c_driver = {
	.driver	= {
		.name	= "max17048",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &max17048_pm_ops,
#endif /* CONFIG_PM */
	},
	.probe		= max17048_probe,
	.remove		= __devexit_p(max17048_remove),
#ifndef CONFIG_PM
	.suspend	= max17048_suspend,
	.resume		= max17048_resume,
#endif /* !CONFIG_PM */
	.id_table	= max17048_id,
};

static int __init max17048_init(void)
{
    int work = 0;
    if (max17048_check_fgic_type() == 0) {
        MAX17048_INFOLOG("[%s] MAX17048 is not implemented.\n", __func__);
        return -1;
    }
    MAX17048_DBGLOG("[%s] in\n", __func__);
    work = i2c_add_driver(&max17048_i2c_driver);
    MAX17048_DBGLOG("[%s] i2c_add_driver out work = %d\n", __func__, work);
    return work;
}
module_init(max17048_init);

static void __exit max17048_exit(void)
{
	i2c_del_driver(&max17048_i2c_driver);
}
module_exit(max17048_exit);

MODULE_AUTHOR("Fujitsu");
MODULE_DESCRIPTION("MAX17048 Fuel Gauge");
MODULE_LICENSE("GPL");
