
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

#include <mach/board_fj.h>
#include <linux/mfd/fj_charger.h>
#include <linux/mfd/fj_charger_local.h>
#include <linux/max17050_battery.h>
#include <linux/freezer.h>
#include <linux/irq.h>
#ifndef FJ_CHARGER_LOCAL
#include <linux/fta.h>
#endif /* FJ_CHARGER_LOCAL */

//#define MAX17050_DBG
#undef MAX17050_DBG

#ifdef MAX17050_DBG
#define MAX17050_DBGLOG(x, y...)            printk(KERN_ERR "[max17050_batt] " x, ## y)
#else  /* MAX17050_DBG */
#define MAX17050_DBGLOG(x, y...)
#endif /* MAX17050_DBG */

#define MAX17050_INFOLOG(x, y...)           printk(KERN_INFO "[max17050_batt] " x, ## y)
#define MAX17050_WARNLOG(x, y...)           printk(KERN_WARNING "[max17050_batt] " x, ## y)
#define MAX17050_ERRLOG(x, y...)            printk(KERN_ERR "[max17050_batt] " x, ## y)

#ifndef FJ_CHARGER_LOCAL
#define MAX17050_RECLOG(x, y...)            {                                               \
                                                char recbuf[128];                           \
                                                snprintf(recbuf, sizeof(recbuf), x, ## y);  \
                                                ftadrv_send_str(recbuf);                    \
                                            }
#else
#define MAX17050_RECLOG(x, y...)
#endif /* FJ_CHARGER_LOCAL */

/* REGISTER ADDRESS DEFINE */
#define MAX17050_STATUS_ADDR                0x00
#define MAX17050_VALRTTHRESHOLD_ADDR        0x01
#define MAX17050_TALRTTHRESHOLD_ADDR        0x02
#define MAX17050_SALRTTHRESHOLD_ADDR        0x03
#define MAX17050_ATRATE_ADDR                0x04
#define MAX17050_REMCAPREP_ADDR             0x05
#define MAX17050_SOCREP_ADDR                0x06
#define MAX17050_AGE_ADDR                   0x07
#define MAX17050_TEMPERATURE_ADDR           0x08
#define MAX17050_VCELL_ADDR                 0x09
#define MAX17050_CURRENT_ADDR               0x0A
#define MAX17050_AVERAGECURRENT_ADDR        0x0B
#define MAX17050_SOCMIX_ADDR                0x0D
#define MAX17050_SOCAV_ADDR                 0x0E
#define MAX17050_REMCAPMIX_ADDR             0x0F

#define MAX17050_FULLCAP_ADDR               0x10
#define MAX17050_TTE_ADDR                   0x11
#define MAX17050_QRESIDUAL_00_ADDR          0x12
#define MAX17050_FULLSOCTHR_ADDR            0x13
#define MAX17050_AVERAGETEMPERATURE_ADDR    0x16
#define MAX17050_CYCLES_ADDR                0x17
#define MAX17050_DESIGNCAP_ADDR             0x18
#define MAX17050_AVERAGEVCELL_ADDR          0x19
#define MAX17050_MAXMINTEMPERATURE_ADDR     0x1A
#define MAX17050_MAXMINVCELL_ADDR           0x1B
#define MAX17050_MAXMINCURRENT_ADDR         0x1C
#define MAX17050_CONFIG_ADDR                0x1D
#define MAX17050_ICHGTERM_ADDR              0x1E
#define MAX17050_REMCAPAV_ADDR              0x1F

#define MAX17050_VERSION_ADDR               0x21
#define MAX17050_QRESIDUAL_10_ADDR          0x22
#define MAX17050_FULLCAPNOM_ADDR            0x23
#define MAX17050_TEMPNOM_ADDR               0x24
#define MAX17050_TEMPLIM_ADDR               0x25
#define MAX17050_AIN_ADDR                   0x27
#define MAX17050_LEARNCFG_ADDR              0x28
#define MAX17050_FILTERCFG_ADDR             0x29
#define MAX17050_RELAXCFG_ADDR              0x2A
#define MAX17050_MISCCFG_ADDR               0x2B
#define MAX17050_TGAIN_ADDR                 0x2C
#define MAX17050_TOFF_ADDR                  0x2D
#define MAX17050_CGAIN_ADDR                 0x2E
#define MAX17050_COFF_ADDR                  0x2F

#define MAX17050_QRESIDUAL_20_ADDR          0x32
#define MAX17050_FULLCAP0_ADDR              0x35
#define MAX17050_IAVG_EMPTY_ADDR            0x36
#define MAX17050_FCTC_ADDR                  0x37
#define MAX17050_RCOMP0_ADDR                0x38
#define MAX17050_TEMPCO_ADDR                0x39
#define MAX17050_V_EMPTY_ADDR               0x3A
#define MAX17050_FSTAT_ADDR                 0x3D
#define MAX17050_TIMER_ADDR                 0x3E
#define MAX17050_SHDNTIMER_ADDR             0x3F

#define MAX17050_QRESIDUAL_30_ADDR          0x42
#define MAX17050_DQACC_ADDR                 0x45
#define MAX17050_DPACC_ADDR                 0x46
#define MAX17050_VFSOC0_ADDR                0x48
#define MAX17050_QH0_ADDR                   0x4C
#define MAX17050_QH_ADDR                    0x4D

#define MAX17050_VFSOC0_MASK_ADDR           0x60
#define MAX17050_WRITE_MASK1_ADDR           0x62
#define MAX17050_WRITE_MASK2_ADDR           0x63

#define MAX17050_BATTERYTABLE_ADDR          0x80

#define MAX17050_VFOCV_ADDR                 0xFB
#define MAX17050_SOCVF_ADDR                 0xFF

/* SET PARAMETER */
#define MAX17050_CONFIG_INIT_DATA           0x5104
#define MAX17050_FILTERCFG_INIT_DATA        0x87A4
#define MAX17050_RELAXCFG_INIT_DATA         0x20EB
#define MAX17050_LEARNCFG_INIT_DATA         0x2602
#define MAX17050_FULLSOCTHR_INIT_DATA       0x6300
#define MAX17050_VALRTTHRESHOLD_INIT_DATA   0xDEA0

#define MAX17050_WRITE_UNMASK1_DATA         0x0059
#define MAX17050_WRITE_MASK1_DATA           0x0000
#define MAX17050_WRITE_UNMASK2_DATA         0x00C4
#define MAX17050_WRITE_MASK2_DATA           0x0000

#define MAX17050_VFSOC0_UNMASK_DATA         0x0080
#define MAX17050_VFSOC0_MASK_DATA           0x0000

#define MAX17050_CYCLES_INIT_DATA           0x0000
#define MAX17050_DPACC_INIT_DATA            0x3200

#define MAX17050_IAVG_EMPTY_INIT_DATA       0x0780
#define MAX17050_AGE_INIT_DATA              0x6400

#define MAX17050_MISCCFG_INIT_DATA          0x0BF0

/* STATUS REGISTER PARAMETER */
#define MAX17050_STATUS_BR                  0x8000
#define MAX17050_STATUS_SMX                 0x4000
#define MAX17050_STATUS_TMX                 0x2000
#define MAX17050_STATUS_VMX                 0x1000
#define MAX17050_STATUS_BI                  0x0800
#define MAX17050_STATUS_SMN                 0x0400
#define MAX17050_STATUS_TMN                 0x0200
#define MAX17050_STATUS_VMN                 0x0100
#define MAX17050_STATUS_BST                 0x0008
#define MAX17050_STATUS_POR                 0x0002

/* MONITOR TIME */
#define MAX17050_MONITOR_DELAY_100MS        msecs_to_jiffies(100)
#define MAX17050_MONITOR_DELAY_300MS        msecs_to_jiffies(300)
#define MAX17050_MONITOR_DELAY_500MS        msecs_to_jiffies(500)
#define MAX17050_MONITOR_DELAY_1S           msecs_to_jiffies(1000)
#define MAX17050_MONITOR_DELAY_10S          msecs_to_jiffies(10000)
#define MAX17050_MONITOR_DELAY_60S          msecs_to_jiffies(60000)

/* battery capacity */
#define MAX17050_BATTERY_FULL               100
#define MAX17050_BATTERY_99                 99
#define MAX17050_BATTERY_10                 10

/* NV value ID*/
#define APNV_CHG_ADJ_VBAT_I                 41040
#define APNV_FUNC_LIMITS_I                  41053

#define MAX17050_VBAT_ADJ_RATIO             40
#define MAX17050_VBAT_TBL_MAX               32

#define MAX17050_LOWBATT_VOLT               3200
#define MAX17050_LOWBATT_RECOVER_VOLT       3500
#define MAX17050_LOWBATT_SOC                0
#define BATTERY_NOT_PRESENT_TEMP            (-300)

/* MAX17050 thread events */
#define MAX17050_EVENT_PRESENT_MONITOR      0
#define MAX17050_EVENT_BATT_MONITOR         1
#define MAX17050_EVENT_SUSPEND              2
#define MAX17050_EVENT_MT_MONITOR           3
#define MAX17050_EVENT_ALERT_ISR            4

/* MAX17050 related GPIOs */
#define MAX17050_INT_GPIO                   36
#define TEMP_FETON2_GPIO                    17              /* GPIO Number 17 */
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)      (pm_gpio - 1 + NR_GPIO_IRQS)

#define MAX17050_IRQ_CHECK_CNT_LIMIT        100

/* GCF Support */
#define MAX17050_FUNC_NO_LIMIT              0x0000
enum max17050_func_limit {
    MAX17050_FUNC_LIMIT_CHARGE = 0,         /*  0 */
    MAX17050_FUNC_LIMIT_BATTERY_PRESENT,    /*  1 */
    MAX17050_FUNC_LIMIT_LOW_BATTERY,        /*  2 */
    MAX17050_FUNC_LIMIT_RESERVE_01,         /*  3 */
    MAX17050_FUNC_LIMIT_BATTERY_TEMP,       /*  4 */
    MAX17050_FUNC_LIMIT_TERMINAL_TEMP,      /*  5 */
    MAX17050_FUNC_LIMIT_RECEIVER_TEMP,      /*  6 */
    MAX17050_FUNC_LIMIT_CHARGE_TEMP,        /*  7 */
    MAX17050_FUNC_LIMIT_CENTER_TEMP,        /*  8 */
    MAX17050_FUNC_LIMIT_NUM,
};

typedef enum {
    MAX17050_BATTERY_CAPA_3020 = 0,         /* 3020mAh */
    MAX17050_BATTERY_CAPA_2600,             /* 2600mAh */
    MAX17050_BATTERY_CAPA_NUM,
} MAX17050_BATTERY_TYPE;

typedef enum {
    MAX17050_REGISTER_TEMPNOM = 0,  /* 0x24:TEMPNOM */
    MAX17050_REGISTER_RCOMP0,       /* 0x38:RCOMP0 */
    MAX17050_REGISTER_TEMPCO,       /* 0x39:TEMPCO */
    MAX17050_REGISTER_ICHGTERM,     /* 0x1E:ICHGTERM */
    MAX17050_REGISTER_V_EMPTY,      /* 0x3A:V_EMPTY */
    MAX17050_REGISTER_QRESIDUAL_00, /* 0x12:QRESIDUAL_00 */
    MAX17050_REGISTER_QRESIDUAL_10, /* 0x22:QRESIDUAL_10 */
    MAX17050_REGISTER_QRESIDUAL_20, /* 0x32:QRESIDUAL_20 */
    MAX17050_REGISTER_QRESIDUAL_30, /* 0x42:QRESIDUAL_30 */
    MAX17050_REGISTER_FULLCAP,      /* 0x10:FULLCAP */
    MAX17050_REGISTER_DESIGNCAP,    /* 0x18:DESIGNCAP */
    MAX17050_REGISTER_FULLCAPNOM,   /* 0x23:FULLCAPNOM */
    MAX17050_REGISTER_NUM,          
} MAX17050_REGISTER_LABEL;

typedef enum { 
    MAX17050_MODEL_TYPE_NONE = FJ_CHG_MODEL_TYPE_NONE,
    MAX17050_MODEL_TYPE_FJDEV001,
    MAX17050_MODEL_TYPE_FJDEV002,
    MAX17050_MODEL_TYPE_FJDEV003,
    MAX17050_MODEL_TYPE_FJDEV004,
    MAX17050_MODEL_TYPE_FJDEV005,
    MAX17050_MODEL_TYPE_FJDEV014
} MAX17050_MODEL_TYPE; 

struct max17050_chip {
    struct i2c_client               *client;
    
    struct delayed_work             battery_monitor;
    struct delayed_work             battery_present_monitor;
    struct delayed_work             mt_monitor;
    struct power_supply             battery;
    struct max17050_platform_data   *pdata;
    
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
    /* sensor temp */
    int temp_sensor;
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
    /* GCF function limit */
    bool check_disable[MAX17050_FUNC_LIMIT_NUM];
    /* monitor event state */
    unsigned long event_state;
    /* low battery detect flag */
    unsigned char lowbattery;
    
    struct mutex lock;
    wait_queue_head_t wq;
    void* thread;
};

typedef struct{
    unsigned short  td_rcomp0;
    unsigned short  td_tempco;
    unsigned short  td_fullcap;
    unsigned short  td_cycles;
    unsigned short  td_fullcapnom;
    unsigned short  td_iavg_empty;
    unsigned short  td_qrtable00;
    unsigned short  td_qrtable10;
    unsigned short  td_qrtable20;
    unsigned short  td_qrtable30;
    unsigned short  td_age;
}max17050_learning_data;

typedef void (*max17050_handler_t)(unsigned long cmd, unsigned long value, void *data);

/* vattery voltage adjust table */
static int max17050_vbat_tbl[MAX17050_VBAT_TBL_MAX] =
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

static int max17050_capacity_convert[101] =
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

// MSM_GPIO IRQ
static int max17050_irq;
static int max17050_irq_enable_flag = 0;
static int max17050_irq_lowbat_flag = 0;

static int max17050_queue_stat = 0;
static struct max17050_chip *the_chip = NULL;
static max17050_learning_data the_learning_data;
static int max17050_init_skip_flag = 0;     /* max17050 init skip flag(0:disable,1:enable) */
static int max17050_init_no_skip_flag = 0;  /* max17050 init no skip flag(0:disable,1:enable) */
static int max17050_initialized = 0;        /* max17050 init flag(0:no initialize,1:initialized) */

static int battery_health = POWER_SUPPLY_HEALTH_GOOD;
static int battery_status = POWER_SUPPLY_STATUS_DISCHARGING;

static unsigned char fj_soc_mask_start = 0;
static unsigned long fj_soc_mask_expire = 0;
static unsigned char fj_lowbatt_volt_detect_cnt = 3;
static unsigned char fj_lowbatt_soc_detect_cnt = 3;
static unsigned char fj_keep_full_status = 0;
//static int max17050_435V_chg_flag = 1;
static unsigned int max17050_irq_check_counter = 0;

static const u16 max17050_setup_init_table[MAX17050_REGISTER_NUM][MAX17050_BATTERY_CAPA_NUM] = {
     /* 3020mAh 2600mAh */
      { 0x1400, 0x1400 },   /* 0x24:TEMPNOM */
      { 0x004C, 0x0045 },   /* 0x38:RCOMP0 */
      { 0x1A27, 0x1B29 },   /* 0x39:TEMPCO */
      { 0x0280, 0x0280 },   /* 0x1E:ICHGTERM */
      { 0xACDA, 0xACDA },   /* 0x3A:V_EMPTY */
      { 0x530F, 0x5607 },   /* 0x12:QRESIDUAL_00 */
      { 0x3380, 0x3284 },   /* 0x22:QRESIDUAL_10 */
      { 0x2480, 0x2185 },   /* 0x32:QRESIDUAL_20 */
      { 0x1C00, 0x1785 },   /* 0x42:QRESIDUAL_30 */
      { 0x17FA, 0x1468 },   /* 0x10:FULLCAP */
      { 0x1798, 0x1450 },   /* 0x18:DESIGNCAP */
      { 0x1798, 0x1450 },   /* 0x23:FULLCAPNOM */
};
/* max17050 battery data */
static const u16 max17050_table_42[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     /* 0x80 ------ 0x87 */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     /* 0x88 ------ 0x8F */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     /* 0x90 ------ 0x97 */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     /* 0x98 ------ 0x9F */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     /* 0xA0 ------ 0xA7 */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     /* 0xA8 ------ 0xAF */
};

static const u16 max17050_table_435_2600[] = {
    0x9670, 0xB550, 0xB8B0, 0xBB30, 0xBC10, 0xBC60, 0xBCB0, 0xBDF0,     /* 0x80 ------ 0x87 */
    0xBEF0, 0xBF50, 0xC110, 0xC310, 0xC800, 0xCB50, 0xD180, 0xD760,     /* 0x88 ------ 0x8F */
    0x0110, 0x0B00, 0x0C00, 0x0B00, 0x3460, 0x3F00, 0x1900, 0x0C40,     /* 0x90 ------ 0x97 */
    0x38E0, 0x08E0, 0x08F0, 0x08D0, 0x08E0, 0x06F0, 0x0680, 0x0680,     /* 0x98 ------ 0x9F */
    0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180,     /* 0xA0 ------ 0xA7 */
    0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180,     /* 0xA8 ------ 0xAF */
};

static const u16 max17050_table_435_3020[] = {
    0x9170, 0xA6A0, 0xB6A0, 0xB860, 0xBB20, 0xBC40, 0xBC90, 0xBCE0,     /* 0x80 ------ 0x87 */
    0xBDE0, 0xBED0, 0xC050, 0xC390, 0xC620, 0xCB10, 0xD0F0, 0xD700,     /* 0x88 ------ 0x8F */
    0x0100, 0x0150, 0x1110, 0x0C00, 0x0F20, 0x3900, 0x3950, 0x1410,     /* 0x90 ------ 0x97 */
    0x1550, 0x10E0, 0x08C0, 0x08A0, 0x08E0, 0x06E0, 0x0690, 0x0690,     /* 0x98 ------ 0x9F */
    0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180,     /* 0xA0 ------ 0xA7 */
    0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180,     /* 0xA8 ------ 0xAF */
};

static struct wake_lock max17050_chg_status;	/* for change power supply status */

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

static enum power_supply_property max17050_battery_props[] = {
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
    POWER_SUPPLY_PROP_TEMP_SENSOR,
};

static MAX17050_MODEL_TYPE max17050_model_type = MAX17050_MODEL_TYPE_NONE;
static MAX17050_MODEL_VER max17050_model_ver = MAX17050_MODEL_VER_NONE;
static MAX17050_BATTERY_TYPE max17050_battery_type = MAX17050_BATTERY_CAPA_3020;

/* internal function prototype */
static int max17050_read_reg(struct i2c_client *client, int reg);
static int max17050_write_reg(struct i2c_client *client, int reg, int data);
static int max17050_get_soc(struct max17050_chip* chip, int *battery_capacity);
static void max17050_set_queue_state(void);
static int max17050_get_queue_state(void);
static void max17050_clear_queue_state(void);
static void max17050_wakeup(struct max17050_chip* chip);
static void max17050_convert_batt_voltage(struct max17050_chip* chip);
static int max17050_get_vbat(struct max17050_chip *chip, int *battery_voltage);
static int max17050_get_battery_voltage(struct max17050_chip *chip, int *battery_voltage);
static int max17050_set_battery_voltage(struct max17050_chip *chip, int battery_voltage);
static int max17050_get_battery_temperature(struct max17050_chip *chip, int *battery_temperature);
static int max17050_set_battery_temperature(struct max17050_chip *chip, int battery_temperature);
static int max17050_get_terminal_temperature(struct max17050_chip *chip, int *terminal_temperature);
static int max17050_set_terminal_temperature(struct max17050_chip *chip, int terminal_temperature);
static int max17050_get_receiver_temperature(struct max17050_chip *chip, int *receiver_temperature);
static int max17050_set_receiver_temperature(struct max17050_chip *chip, int receiver_temperature);
static int max17050_get_charge_temperature(struct max17050_chip *chip, int *charge_temperature);
static int max17050_set_charge_temperature(struct max17050_chip *chip, int charge_temperature);
static int max17050_get_center_temperature(struct max17050_chip *chip, int *center_temperature);
static int max17050_set_center_temperature(struct max17050_chip *chip, int center_temperature);
static int max17050_get_battery_capacity(struct max17050_chip *chip, int *battery_capacity);
static int max17050_effect_battery_capacity(struct max17050_chip *chip, int battery_capacity);
static int max17050_set_battery_capacity(struct max17050_chip *chip, int battery_capacity);
static int max17050_set_battery_capacity_threshold(struct max17050_chip *chip, int battery_capacity);
static int max17050_get_vbat_adjust(struct max17050_chip *chip, int *batt_offset);
static int max17050_set_vbat_adjust(struct max17050_chip *chip, int batt_offset);
static void max17050_set_temperature(struct max17050_chip *chip, int temperature);
static void max17050_battery_monitor(struct work_struct *work);
static void max17050_mobile_temp_monitor(struct work_struct *work);
static void max17050_delayed_init(struct max17050_chip *chip);
static void max17050_monitor_work(struct max17050_chip *chip);
static void max17050_alert_work(struct max17050_chip *chip);
static void max17050_monitor_mobile_temp(struct max17050_chip *chip);
static int max17050_is_update_params(struct max17050_chip *chip);
static void max17050_charge_function_limits_check(struct max17050_chip *chip);
static int max17050_charge_battery_present_check(struct max17050_chip *chip);
static void max17050_battery_present_monitor(struct work_struct *work);
static irqreturn_t max17050_alert_isr(int irq_no, void *dev_id);
static int max17050_setup(struct max17050_chip *chip);
static void max17050_learning_data_restore(struct max17050_chip *chip);
static void max17050_interrupt_enable(void);
static void max17050_interrupt_disable(void);
static void max17050_interrupt_init(struct max17050_chip *chip);
static void max17050_error_sequence(void);
static int max17050_thread(void * ptr);

static int max17050_check_fgic_type(void)
{
    int result = 0;
    unsigned int work_dev;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    work_dev = system_rev & 0xF0;
    
    switch (work_dev)
    {
        case FJ_CHG_DEVICE_TYPE_FJDEV001:
        case FJ_CHG_DEVICE_TYPE_FJDEV002:
        case FJ_CHG_DEVICE_TYPE_FJDEV003:
        case FJ_CHG_DEVICE_TYPE_FJDEV004:
            result = 1;
            break;
        case FJ_CHG_DEVICE_TYPE_FJDEV005:
        case FJ_CHG_DEVICE_TYPE_FJDEV014:
        default:
            result = 0;
            break;
    }
    
    return result;
}

static int max17050_get_model_type(struct max17050_chip *chip)
{
    int result = 0;
    int temp = 0;
    unsigned int work_dev;
    unsigned int work_ver;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    max17050_model_type = MAX17050_MODEL_TYPE_NONE;
    max17050_model_ver = MAX17050_MODEL_VER_NONE;
    
    max17050_get_battery_temperature(chip, &temp);
    
    work_dev = system_rev & 0xF0;
    work_ver = system_rev & 0x0F;
    
    switch (work_dev)
    {
        case FJ_CHG_DEVICE_TYPE_FJDEV001 :
            
            max17050_model_type = MAX17050_MODEL_TYPE_FJDEV001;
            max17050_battery_type = MAX17050_BATTERY_CAPA_3020;
            
            if (((work_ver == 0x00) ||
                 (work_ver == 0x01) ||
                 (work_ver == 0x02) ||
                 (work_ver == 0x03)) &&
                 (temp <= BATTERY_NOT_PRESENT_TEMP)) {
                max17050_model_ver = MAX17050_MODEL_VER_001;
            } else if((work_ver == 0x00) ||
                      (work_ver == 0x01) ||
                      (work_ver == 0x02) ||
                      (work_ver == 0x03) ||
                      (work_ver == 0x04) ||
                      (work_ver == 0x05)) {
                max17050_model_ver = MAX17050_MODEL_VER_002;
            } else {
                max17050_model_ver = MAX17050_MODEL_VER_003;
            }
            break;
            
        case FJ_CHG_DEVICE_TYPE_FJDEV002 :
            
            max17050_model_type = MAX17050_MODEL_TYPE_FJDEV002;
            max17050_battery_type = MAX17050_BATTERY_CAPA_2600;
            
            if (((work_ver == 0x00) ||
                 (work_ver == 0x01)) &&
                 (temp <= BATTERY_NOT_PRESENT_TEMP)) {
                max17050_model_ver = MAX17050_MODEL_VER_001;
            } else if ((work_ver == 0x00) ||
                       (work_ver == 0x01) ||
                       (work_ver == 0x02)) {
                max17050_model_ver = MAX17050_MODEL_VER_002;
            } else {
                max17050_model_ver = MAX17050_MODEL_VER_003;
            }
            break;
            
        case FJ_CHG_DEVICE_TYPE_FJDEV004 :
            
            max17050_model_type = MAX17050_MODEL_TYPE_FJDEV004;
            max17050_battery_type = MAX17050_BATTERY_CAPA_3020;
            
            if (((work_ver == 0x00) ||
                 (work_ver == 0x01)) &&
                 (temp <= BATTERY_NOT_PRESENT_TEMP)) {
                max17050_model_ver = MAX17050_MODEL_VER_001;
            } else if ((work_ver == 0x00) ||
                       (work_ver == 0x01) ||
                       (work_ver == 0x02)) {
                max17050_model_ver = MAX17050_MODEL_VER_002;
            } else {
                max17050_model_ver = MAX17050_MODEL_VER_003;
            }
            break;
            
        case FJ_CHG_DEVICE_TYPE_FJDEV003 :
        default :
            max17050_model_type = MAX17050_MODEL_TYPE_NONE;
            max17050_model_ver = MAX17050_MODEL_VER_NONE;
            result = -1;
            break;
    }
    
    MAX17050_DBGLOG("[%s] work_dev %X work_ver %X\n", __func__, work_dev, work_ver);
    MAX17050_DBGLOG("[%s] model_type %X model_ver %X\n", __func__, max17050_model_type, max17050_model_ver);
    
    return result;
}

static int max17050_get_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    union power_supply_propval *val)
{
    static int old_status = POWER_SUPPLY_STATUS_UNKNOWN;
    static int old_health = POWER_SUPPLY_HEALTH_UNKNOWN;
    static int old_soc = 0;
    
    struct max17050_chip *chip;
    
    if (psy == NULL) {
        MAX17050_ERRLOG("[%s] psy pointer is NULL\n", __func__);
        return -1;
    }
    chip = container_of(psy, struct max17050_chip, battery);
    
    switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
            val->intval = chip->status;
            if (old_status != chip->status) {
                MAX17050_INFOLOG("[%s]old status(%s) -> new status(%s)\n", __func__,
                                    fj_ps_status_str[old_status],
                                    fj_ps_status_str[chip->status]);
                old_status = chip->status;
            }
            MAX17050_DBGLOG("[%s] chip->status = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_HEALTH:
            val->intval = chip->health;
            if (old_health != chip->health) {
                MAX17050_INFOLOG("[%s]old health(%s) -> new health(%s)\n", __func__,
                                    fj_ps_health_str[old_health],
                                    fj_ps_health_str[chip->health]);
                old_health = chip->health;
            }
            break;
        case POWER_SUPPLY_PROP_PRESENT:
            if (chip->check_disable[MAX17050_FUNC_LIMIT_BATTERY_PRESENT] == false) {
                val->intval = chip->present;
            } else {
                val->intval = 1;
            }
            MAX17050_DBGLOG("[%s] chip->present = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TECHNOLOGY:
            val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
            break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            if (chip->check_disable[MAX17050_FUNC_LIMIT_LOW_BATTERY] == false) {
                val->intval = chip->vcell;
            } else {
                val->intval = 3900;
            }
            MAX17050_DBGLOG("[%s] chip->vcell = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_CAPACITY:
            if (chip->check_disable[MAX17050_FUNC_LIMIT_LOW_BATTERY] == false) {
                val->intval = chip->soc;
                
                if ((chip->status != POWER_SUPPLY_STATUS_FULL) &&
                    (val->intval >= 50)) {
                    val->intval++;
                    if (val->intval >= 100) {
                        val->intval = 100;
                    }
                    if ((chip->status == POWER_SUPPLY_STATUS_CHARGING) && 
                        (val->intval > MAX17050_BATTERY_99)) {
                        val->intval = MAX17050_BATTERY_99;
                    }
                } else if (chip->status == POWER_SUPPLY_STATUS_FULL) {
                    val->intval = 100;
                }
                
                val->intval = max17050_capacity_convert[val->intval];
                MAX17050_DBGLOG("[%s]convert soc(%d)\n", __func__, val->intval);
                
                if (old_soc != chip->soc) {
                    MAX17050_INFOLOG("[%s]old soc(%d) -> new soc(%d)\n", __func__,
                                        old_soc, chip->soc);
                    old_soc = chip->soc;
                }
            } else {
                val->intval = 90;
            }
            MAX17050_DBGLOG("[%s] chip->soc = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP:
            if (chip->check_disable[MAX17050_FUNC_LIMIT_BATTERY_TEMP] == false) {
                val->intval = (chip->temp_batt * 10);
            } else {
                val->intval = 250;
            }
            MAX17050_DBGLOG("[%s] chip->temp_batt = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_AMBIENT:
            if (chip->check_disable[MAX17050_FUNC_LIMIT_TERMINAL_TEMP] == false) {
                val->intval = (chip->temp_ambient * 10);
            } else {
                val->intval = 250;
            }
            MAX17050_DBGLOG("[%s] chip->temp_ambient = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_CASE:
            if (chip->check_disable[MAX17050_FUNC_LIMIT_RECEIVER_TEMP] == false) {
                val->intval = (chip->temp_case * 10);
            } else {
                val->intval = 250;
            }
            MAX17050_DBGLOG("[%s] chip->temp_case = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_CHARGE:
            val->intval = (chip->temp_charge * 10);
            MAX17050_DBGLOG("[%s] chip->temp_charge = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_CENTER:
            val->intval = (chip->temp_center * 10);
            MAX17050_DBGLOG("[%s] chip->temp_center = %d\n",__func__, val->intval);
            break;
        case POWER_SUPPLY_PROP_TEMP_SENSOR:
            val->intval = chip->temp_sensor;
            MAX17050_DBGLOG("[%s] chip->temp_sensor = %d\n",__func__, val->intval);
            break;
        default:
            MAX17050_WARNLOG("[%s]not handle power_supply_property(%d)\n", __func__, psp);
            return -EINVAL;
    }
    return 0;
}

static int max17050_read_reg(struct i2c_client *client, int reg)
{
    int ret = 0;
    int i = 0;
    
    if (client == NULL) {
        MAX17050_WARNLOG("[%s] client pointer is NULL\n", __func__);
        return -1;
    }
    
    ret = i2c_smbus_read_word_data(client, reg);
    
    if (ret < 0) {
        /* fail safe */
        MAX17050_ERRLOG("[%s] i2c word read err : reg = 0x%x, result = %d \n",
                        __func__, reg, ret);
        for (i = 0;i < 5;i++) {
            ret = i2c_smbus_read_word_data(client, reg);
            if (ret >= 0) {
                break;
            }
            msleep(100);
        }
        if (ret < 0) {
            MAX17050_ERRLOG("[%s] i2c word read retry err : reg = 0x%x, result = %d \n",
                                __func__, reg, ret);
        }
    }
    return ret;
}

static int max17050_write_reg(struct i2c_client *client, int reg, int data)
{
    int ret = 0;
    int i = 0;
    
    if (client == NULL) {
        MAX17050_WARNLOG("[%s] client pointer is NULL\n", __func__);
        return -1;
    }
    
    ret = i2c_smbus_write_word_data(client, reg, data);
    
    if (ret < 0) {
        /* fail safe */
        MAX17050_ERRLOG("[%s] i2c word write err : reg = 0x%x, data = 0x%x, result = %d \n",
                            __func__, reg, data, ret);
        for (i = 0;i < 5;i++) {
            ret = i2c_smbus_write_word_data(client, reg, data);
            if (ret >= 0) {
                break;
            }
            msleep(100);
        }
        if (ret < 0) {
            MAX17050_ERRLOG("[%s] i2c word write retry err : reg = 0x%x, data = 0x%x, result = %d \n",
                                 __func__, reg, data, ret);
        }
    }
    return ret;
}

static int max17050_get_soc(struct max17050_chip* chip, int *battery_capacity)
{
    int result = 0;
    u16 socrep = 0;
    u16 socmsb = 0;
    u16 soclsb = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    mutex_lock(&chip->lock);
    result = max17050_read_reg(chip->client, MAX17050_SOCREP_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    socrep = result;
    MAX17050_DBGLOG("[%s] READ ACCESS SOCREP : 0x%04X\n", __func__, socrep);
    
    socmsb = socrep >> 8;
    *battery_capacity = socmsb;
    if ((chip->status == POWER_SUPPLY_STATUS_CHARGING) && (socmsb == 0)) {
        soclsb = socrep & 0x00FF;
        if ((soclsb * 39) >= 5000) {
            *battery_capacity = 1;
        }
    }
    MAX17050_DBGLOG("[%s] battery_capacity = %d\n", __func__, *battery_capacity);
    
max17050_err:
    mutex_unlock(&chip->lock);
    return result;
}

static void max17050_set_queue_state(void)
{
    MAX17050_DBGLOG("[%s] in\n", __func__);
    max17050_queue_stat = 1;
}

static int max17050_get_queue_state(void)
{
    MAX17050_DBGLOG("[%s] in\n", __func__);
    return max17050_queue_stat;
}

static void max17050_clear_queue_state(void)
{
    MAX17050_DBGLOG("[%s] in\n", __func__);
    max17050_queue_stat = 0;
}

static void max17050_wakeup(struct max17050_chip* chip)
{
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    max17050_set_queue_state();
    
    wake_up(&chip->wq);
}

static void max17050_convert_batt_voltage(struct max17050_chip* chip)
{
    int result = 0;
    int vcell_work = 0;
    u16 status = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    vcell_work = chip->vbat_raw >> 3;
    vcell_work = vcell_work * 625;
    chip->vcell = vcell_work / 1000;
    
    if (chip->vcell <= MAX17050_LOWBATT_VOLT) {
        if (fj_lowbatt_volt_detect_cnt > 0) {
            fj_lowbatt_volt_detect_cnt--;
        }
        if (fj_lowbatt_volt_detect_cnt == 0) {
            chip->lowbattery = 1;
            fj_lowbatt_volt_detect_cnt = 3;
            MAX17050_ERRLOG("[%s] low battery detect volt:%dmV\n", __func__, chip->vcell);
            MAX17050_RECLOG("[%s] low battery detect volt:%dmV\n", __func__, chip->vcell);
        }
    } else if ((chip->vcell >= MAX17050_LOWBATT_RECOVER_VOLT) &&
               (chip->status == POWER_SUPPLY_STATUS_CHARGING)) {
        fj_lowbatt_volt_detect_cnt = 3;
        fj_lowbatt_soc_detect_cnt = 3;
        chip->lowbattery = 0;
        
        result = max17050_read_reg(chip->client, MAX17050_STATUS_ADDR);
        if (result < 0){
            MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
            goto max17050_err;
        }
        status = result;
        
        if (status & (MAX17050_STATUS_VMN)) {
            status &= ~(MAX17050_STATUS_VMN);
            result = max17050_write_reg(chip->client, MAX17050_STATUS_ADDR, status);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] ACCESS: ret:%d\n", __func__, result);
                goto max17050_err;
            }
            max17050_irq_lowbat_flag = 0;
        }
    }
    
    MAX17050_DBGLOG("[%s] chip->vcell = %d \n", __func__, chip->vcell);
    MAX17050_DBGLOG("[%s] chip->vbat_raw = 0x%04x\n", __func__, chip->vbat_raw);
    
    return;
    
max17050_err:
    max17050_error_sequence();
    return;
}

static int max17050_get_vbat(struct max17050_chip *chip, int *battery_voltage)
{
    int result = 0;
    u16 vcell = 0;
    unsigned int vbat_raw = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        chip = the_chip;
    }
    
    if (battery_voltage == NULL) {
        MAX17050_ERRLOG("[%s] battery_voltage pointer is NULL\n", __func__);
        return -1;
    }
    
    mutex_lock(&chip->lock);
    result = max17050_read_reg(chip->client, MAX17050_VCELL_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    vcell = result;
    
    if( chip->vbat_adj < 0x10 ) {
        vbat_raw = vcell + (max17050_vbat_tbl[chip->vbat_adj] * MAX17050_VBAT_ADJ_RATIO);
    } else {
        vbat_raw = vcell - (max17050_vbat_tbl[chip->vbat_adj] * MAX17050_VBAT_ADJ_RATIO);
    }
    
    *battery_voltage = vbat_raw;
    MAX17050_DBGLOG("[%s] from ADC battery_voltage = 0x%04x\n", __func__, *battery_voltage);
    
max17050_err:
    mutex_unlock(&chip->lock);
    return result;
}

static int max17050_get_battery_voltage(struct max17050_chip *chip, int *battery_voltage)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (battery_voltage == NULL) {
        MAX17050_ERRLOG("[%s] battery_voltage pointer is NULL\n", __func__);
        return -1;
    }
    
    result = max17050_get_vbat(chip, battery_voltage);
    if (result < 0) {
        MAX17050_ERRLOG("[%s]get vbat error:%d\n", __func__, result);
        goto max17050_err;
    }
    
    MAX17050_DBGLOG("[%s] battery_voltage = 0x%04x\n", __func__, *battery_voltage);
    return result;
    
max17050_err:
    max17050_error_sequence();
    return result;
}

static int max17050_set_battery_voltage(struct max17050_chip *chip, int battery_voltage)
{
    MAX17050_DBGLOG("[%s] in\n", __func__);
    MAX17050_DBGLOG("[%s] battery_voltage = %d \n", __func__, battery_voltage);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->vbat_raw = battery_voltage;
    return 0;
}

static int max17050_get_battery_temperature(struct max17050_chip *chip, int *battery_temperature)
{
    struct pm8xxx_adc_chan_result result_batt_temp;
    int result = 0;
    int i = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (battery_temperature == NULL) {
        MAX17050_ERRLOG("[%s] battery_temperature pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17050_model_ver == MAX17050_MODEL_VER_001) {
        *battery_temperature = 250;
        MAX17050_DBGLOG("[%s] battery_temperature = %d\n", __func__, *battery_temperature);
        return 0;
    }
    
    memset(&result_batt_temp, 0, sizeof(result_batt_temp));
    *battery_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_4, ADC_MPP_1_AMUX6, &result_batt_temp);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] read err ADC MPP4 value raw:0x%x physical:%lld\n", __func__,
                                                                result_batt_temp.adc_code,
                                                                result_batt_temp.physical);
        } else {
            *battery_temperature = (int)result_batt_temp.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17050_error_sequence();
    }
    
    MAX17050_DBGLOG("[%s] battery_temperature = %d\n", __func__, *battery_temperature);
    
    return result;
}

static int max17050_set_battery_temperature(struct max17050_chip *chip, int battery_temperature)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    MAX17050_DBGLOG("[%s] battery_temperature = %d \n", __func__, battery_temperature);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_batt = (battery_temperature / 10);
    
    return result;
}

static int max17050_get_terminal_temperature(struct max17050_chip *chip, int *terminal_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_ambient;
    int result = 0;
    int i = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (terminal_temperature == NULL) {
        MAX17050_ERRLOG("[%s] terminal_temperature pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17050_model_ver == MAX17050_MODEL_VER_001) {
        *terminal_temperature = 250;
        MAX17050_DBGLOG("[%s] from ADC terminal_temperature = %d\n", __func__, *terminal_temperature);
        return 0;
    }
    
    memset(&result_temp_ambient, 0, sizeof(result_temp_ambient));
    *terminal_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_read(ADC_MPP_1_AMUX8, &result_temp_ambient);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] read err ADC MPP8 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_ambient.adc_code,
                                                                    result_temp_ambient.physical);
        } else {
            *terminal_temperature = (int)result_temp_ambient.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17050_error_sequence();
    }
    
    MAX17050_DBGLOG("[%s] from ADC terminal_temperature = %d\n", __func__, *terminal_temperature);
    
    return result;
}

static int max17050_set_terminal_temperature(struct max17050_chip *chip, int terminal_temperature)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    MAX17050_DBGLOG("[%s] terminal_temperature = %d \n", __func__, terminal_temperature);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_ambient = (terminal_temperature / 10);
    
    return result;
}

static int max17050_get_receiver_temperature(struct max17050_chip *chip, int *receiver_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_case;
    int result = 0;
    int i = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (receiver_temperature == NULL) {
        MAX17050_ERRLOG("[%s] receiver_temperature pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17050_model_ver == MAX17050_MODEL_VER_001) {
        *receiver_temperature = 250;
        MAX17050_DBGLOG("[%s] from ADC receiver_temperature = %d\n", __func__, *receiver_temperature);
        return 0;
    }
    
    memset(&result_temp_case, 0, sizeof(result_temp_case));
    *receiver_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_7, ADC_MPP_1_AMUX7, &result_temp_case);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] read err ADC MPP7 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_case.adc_code,
                                                                    result_temp_case.physical);
        } else {
            *receiver_temperature = (int)result_temp_case.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17050_error_sequence();
    }
    
    MAX17050_DBGLOG("[%s] from ADC receiver_temperature = %d\n", __func__, *receiver_temperature);
    
    return result;
}

static int max17050_set_receiver_temperature(struct max17050_chip *chip, int receiver_temperature)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    MAX17050_DBGLOG("[%s] receiver_temperature = %d \n", __func__, receiver_temperature);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_case = (receiver_temperature / 10);
    
    return result;
}

static int max17050_get_charge_temperature(struct max17050_chip *chip, int *charge_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_charge;
    int result = 0;
    int i = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (charge_temperature == NULL) {
        MAX17050_ERRLOG("[%s] charge_temperature is NULL\n", __func__);
        return -1;
    }
    
    if (max17050_model_ver == MAX17050_MODEL_VER_001) {
        *charge_temperature = 250;
        MAX17050_DBGLOG("[%s] from ADC Result:%d charge_temperature = %d\n", __func__, result, *charge_temperature);
        return 0;
    }
    
    memset(&result_temp_charge, 0, sizeof(result_temp_charge));
    *charge_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_9, ADC_MPP_1_AMUX7, &result_temp_charge);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] read err ADC MPP9 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_charge.adc_code,
                                                                    result_temp_charge.physical);
        } else {
            *charge_temperature = (int)result_temp_charge.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17050_error_sequence();
    }
    
    MAX17050_DBGLOG("[%s] from ADC Result:%d charge_temperature = %d\n", __func__, result, *charge_temperature);
    
    return result;
}

static int max17050_set_charge_temperature(struct max17050_chip *chip, int charge_temperature)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    MAX17050_DBGLOG("[%s] charge_temperature = %d \n", __func__, charge_temperature);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_charge = (charge_temperature / 10);
    
    return result;
}

static int max17050_get_center_temperature(struct max17050_chip *chip, int *center_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_center;
    int result = 0;
    int i = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (center_temperature == NULL) {
        MAX17050_ERRLOG("[%s] center_temperature is NULL\n", __func__);
        return -1;
    }
    
    if (max17050_model_ver == MAX17050_MODEL_VER_001) {
        *center_temperature = 250;
        MAX17050_DBGLOG("[%s] from ADC Result:%d center_temperature = %d\n", __func__, result, *center_temperature);
        return 0;
    }
    
    memset(&result_temp_center, 0, sizeof(result_temp_center));
    *center_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_10, ADC_MPP_1_AMUX7, &result_temp_center);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] read err ADC MPP9 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_center.adc_code,
                                                                    result_temp_center.physical);
        } else {
            *center_temperature = (int)result_temp_center.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17050_error_sequence();
    }
    
    MAX17050_DBGLOG("[%s] from ADC Result:%d center_temperature = %d\n", __func__, result, *center_temperature);
    
    return result;
}

static int max17050_set_center_temperature(struct max17050_chip *chip, int center_temperature)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    MAX17050_DBGLOG("[%s] center_temperature = %d \n", __func__, center_temperature);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_center = (center_temperature / 10);
    
    return result;
}

static int max17050_get_sensor_temperature(struct max17050_chip *chip, int *sensor_temperature)
{
    struct pm8xxx_adc_chan_result result_temp_sensor;
    int result = 0;
    int i = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (sensor_temperature == NULL) {
        MAX17050_ERRLOG("[%s] sensor_temperature is NULL\n", __func__);
        return -1;
    }
    
    if (max17050_model_ver == MAX17050_MODEL_VER_001) {
        *sensor_temperature = 250;
        MAX17050_DBGLOG("[%s] from ADC Result:%d sensor_temperature = %d\n", __func__, result, *sensor_temperature);
        return 0;
    }
    
    memset(&result_temp_sensor, 0, sizeof(result_temp_sensor));
    *sensor_temperature = 0;
    
    for (i = 0; i < 3; i++) {
        if ((max17050_model_type == MAX17050_MODEL_TYPE_FJDEV002)) {
            if ((max17050_model_ver == MAX17050_MODEL_VER_003)) {
                gpio_set_value_cansleep(PM8921_GPIO_PM_TO_SYS(TEMP_FETON2_GPIO), 1);
                usleep(20);
                result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_10, ADC_MPP_1_AMUX7, &result_temp_sensor);
                
                gpio_set_value_cansleep(PM8921_GPIO_PM_TO_SYS(TEMP_FETON2_GPIO), 0);
            } else {
                break;
            }
        } else {
            result = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_12, ADC_MPP_1_AMUX7, &result_temp_sensor);
        }
        if (result < 0) {
            MAX17050_ERRLOG("[%s] read err ADC MPP9 value raw:0x%x physical:%lld\n", __func__,
                                                                    result_temp_sensor.adc_code,
                                                                    result_temp_sensor.physical);
        } else {
            *sensor_temperature = (int)result_temp_sensor.physical;
            break;
        }
    }
    
    if (result < 0) {
        max17050_error_sequence();
    }
    
    MAX17050_DBGLOG("[%s] from ADC Result:%d sensor_temperature = %d\n", __func__, result, *sensor_temperature);
    
    return result;
}

static int max17050_set_sensor_temperature(struct max17050_chip *chip, int sensor_temperature)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    MAX17050_DBGLOG("[%s] sensor_temperature = %d \n", __func__, sensor_temperature);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    chip->temp_sensor = sensor_temperature;
    
    return result;
}

static int max17050_get_battery_capacity(struct max17050_chip *chip, int *battery_capacity)
{
    int result = 0;
    int capacity_now = 0;
    int i = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (battery_capacity == NULL) {
        MAX17050_ERRLOG("[%s] battery_capacity pointer is NULL\n", __func__);
        return -1;
    }
    
    for (i = 0; i < 3; i++) {
        result = max17050_get_soc(chip, battery_capacity);
        capacity_now = *battery_capacity;
        
        if (result < 0) {
            MAX17050_ERRLOG("[%s]get soc error:%d\n", __func__, result);
        } else {
            break;
        }
    }
    
    if (result < 0) {
        max17050_error_sequence();
    }
    
    if (capacity_now > MAX17050_BATTERY_FULL) {
        *battery_capacity = MAX17050_BATTERY_FULL;
    } else if (capacity_now <= 0) {
        *battery_capacity = 0;
    } else {
        *battery_capacity = capacity_now;
    }
    
    if (*battery_capacity == MAX17050_LOWBATT_SOC) {
        if (fj_lowbatt_soc_detect_cnt > 0) {
            fj_lowbatt_soc_detect_cnt--;
        }
        if (fj_lowbatt_soc_detect_cnt == 0) {
            chip->lowbattery = 1;
            fj_lowbatt_soc_detect_cnt = 3;
            MAX17050_ERRLOG("[%s] low battery detect\n", __func__);
            MAX17050_RECLOG("[%s] low battery detect\n", __func__);
        }
    } else {
        if (chip->status == POWER_SUPPLY_STATUS_CHARGING) {
            fj_lowbatt_volt_detect_cnt = 3;
            fj_lowbatt_soc_detect_cnt = 3;
            chip->lowbattery = 0;
        }
    }
    
    MAX17050_DBGLOG("[%s] battery_capacity = %d \n", __func__, *battery_capacity);
    
    return result;
}

static int max17050_effect_battery_capacity(struct max17050_chip *chip, int battery_capacity)
{
    int capacity = 0;
    unsigned long now = jiffies;
    
    MAX17050_DBGLOG("[%s] status:%d\n", __func__, chip->status);
    MAX17050_DBGLOG("[%s] new soc:%d, now soc:%d\n", __func__, battery_capacity, chip->soc);
    MAX17050_DBGLOG("[%s] exp = %lu, now = %lu, mask:%d\n", __func__, fj_soc_mask_expire, now, fj_soc_mask_start);
    
    if( (fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE) ) {
        if (chip->lowbattery != 0) {
            /* low battery -> set capacity to 0 */
            capacity = 0;
            fj_soc_mask_start = 0;
        } else if (chip->status == POWER_SUPPLY_STATUS_FULL) {
            if ((fj_keep_full_status != 0) &&
                (battery_capacity < MAX17050_BATTERY_99)) {
                capacity = chip->soc - 1;
                fj_keep_full_status = 0;
                chip->status = POWER_SUPPLY_STATUS_CHARGING;
                MAX17050_INFOLOG("[%s] return to charging status\n", __func__);
            } else {
                capacity = MAX17050_BATTERY_FULL;
            }
            fj_soc_mask_start = 0;
        } else if (time_before(now, fj_soc_mask_expire) && fj_soc_mask_start) {
            /* not update soc from start charging or detect full (60secs) */
            MAX17050_DBGLOG("[%s] not update soc indicator\n", __func__);
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
                (capacity > MAX17050_BATTERY_99)) {
                capacity = MAX17050_BATTERY_99;
            }
        }
    } else {
        capacity = battery_capacity;
    }
    
    MAX17050_INFOLOG("[%s] capacity = %d \n", __func__, capacity);
    
    return capacity;
}

static int max17050_set_battery_capacity(struct max17050_chip *chip, int battery_capacity)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    MAX17050_DBGLOG("[%s] battery_capacity = %d \n", __func__, battery_capacity);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    chip->soc = max17050_effect_battery_capacity(chip, battery_capacity);
    
    return result;
}

static int max17050_set_battery_capacity_threshold(struct max17050_chip *chip, int battery_capacity)
{
    int result = 0;
    int capacity = 0;
    u16 status = 0;
    u16 salrt_raw = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    capacity = battery_capacity;
    salrt_raw = (capacity + 1) << 8;
    salrt_raw |= capacity;
    
    mutex_lock(&the_chip->lock);
    
    result = max17050_write_reg(chip->client, MAX17050_SALRTTHRESHOLD_ADDR, salrt_raw);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] ACCESS: ret:%d\n", __func__, result);
        goto max17050_err;
    }
    
    result = max17050_read_reg(chip->client, MAX17050_STATUS_ADDR);
    if (result < 0){
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    status = result;
    status &= ~(MAX17050_STATUS_SMX | MAX17050_STATUS_SMN);
    
    result = max17050_write_reg(chip->client, MAX17050_STATUS_ADDR, status);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] ACCESS: ret:%d\n", __func__, result);
        goto max17050_err;
    }
    
    mutex_unlock(&the_chip->lock);
    
    return result;
    
max17050_err:
    mutex_unlock(&the_chip->lock);
    max17050_error_sequence();
    
    return result;
}

static int max17050_get_vbat_adjust(struct max17050_chip *chip, int *batt_offset)
{
    int result = 0;
    uint8_t val = 0x00;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    if (batt_offset == NULL) {
        MAX17050_ERRLOG("[%s] batt_offset pointer is NULL\n", __func__);
        return -1;
    }
    
    if ((fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE)) {
        result = get_nonvolatile(&val, APNV_CHG_ADJ_VBAT_I, 1);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV read err : result = %d set value = 0x00\n", __func__, result);
            val = 0x00;
        }
        if (val > 0x1F) {
            MAX17050_ERRLOG("[%s] NV read err : value = 0x%02x set value = 0x00\n", __func__, val);
            val = 0x00;
        }
    } else {
        val = 0x00;
    }
    
    *batt_offset = val;
    MAX17050_INFOLOG("[%s] get NV value batt_offset = 0x%02x\n", __func__, *batt_offset);
    return result;
}

static int max17050_set_vbat_adjust(struct max17050_chip *chip, int batt_offset)
{
	int result = 0;

	MAX17050_DBGLOG("[%s] in\n", __func__);
	MAX17050_INFOLOG("[%s] batt_offset = 0x%02x \n", __func__, batt_offset);

	if (chip == NULL) {
		MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
		if (the_chip == NULL) {
			MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
			return -1;
		}
		chip = the_chip;
	}
	chip->vbat_adj = batt_offset;
	MAX17050_DBGLOG("[%s] set dev info value chip->vbat_adj = 0x%02x\n", __func__, chip->vbat_adj);
	return result;
}

static void max17050_set_temperature(struct max17050_chip *chip, int temperature)
{
    int result = 0;
    int work_temp;
    u16 write_temp = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    MAX17050_DBGLOG("[%s] temperature = %d\n", __func__, temperature);
    
    work_temp = temperature;
    write_temp = work_temp / 10;
    write_temp = write_temp << 8;
    work_temp = work_temp % 10;
    work_temp = (work_temp * 1000) / 39;
    write_temp = write_temp | work_temp;
    
    MAX17050_DBGLOG("[%s] temperature = %04x\n", __func__, write_temp);
    result = max17050_write_reg(chip->client, MAX17050_TEMPERATURE_ADDR, write_temp);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] ACCESS: ret:%d\n", __func__, result);
        max17050_error_sequence();
    }
    
    return;
}

static void max17050_battery_monitor(struct work_struct *work)
{
    struct delayed_work *dwork;
    struct max17050_chip *chip;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (work == NULL) {
        MAX17050_ERRLOG("[%s] work pointer is NULL\n", __func__);
        return;
    }
    
    dwork = to_delayed_work(work);
    if (dwork == NULL) {
        MAX17050_ERRLOG("[%s] dwork pointer is NULL\n", __func__);
        return;
    }
    
    chip = container_of(dwork, struct max17050_chip, battery_monitor);
    if (chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return;
    }
    
    set_bit(MAX17050_EVENT_BATT_MONITOR, &chip->event_state);
    max17050_wakeup(chip);
}

static void max17050_mobile_temp_monitor(struct work_struct *work)
{
    struct delayed_work *dwork;
    struct max17050_chip *chip;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (work == NULL) {
        MAX17050_ERRLOG("[%s] work pointer is NULL\n", __func__);
        return;
    }
    
    dwork = to_delayed_work(work);
    if (dwork == NULL) {
        MAX17050_ERRLOG("[%s] dwork pointer is NULL\n", __func__);
        return;
    }
    
    chip = container_of(dwork, struct max17050_chip, mt_monitor);
    if (chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return;
    }
    
    set_bit(MAX17050_EVENT_MT_MONITOR, &chip->event_state);
    max17050_wakeup(chip);
}

static void max17050_delayed_init(struct max17050_chip *chip)
{
    int batt_offset = 0;
    int i = 0;
    int temp = 0;
    int temp_batt = 0;
    int battery_capacity = 0;
    int vbat_raw = 0;
    int temp_ambient = 0;
    int temp_case = 0;
    int temp_charge = 0;
    int temp_center = 0;
    int temp_sensor = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    if (max17050_write_reg(chip->client, MAX17050_VALRTTHRESHOLD_ADDR, MAX17050_VALRTTHRESHOLD_INIT_DATA) < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : ADDR = 0x%02X \n", __func__, MAX17050_VALRTTHRESHOLD_ADDR);
        MAX17050_RECLOG("[%s] i2c write err : ADDR = 0x%02X \n", __func__, MAX17050_VALRTTHRESHOLD_ADDR);
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_VALRTTHRESHOLD_ADDR, max17050_read_reg(chip->client, MAX17050_VALRTTHRESHOLD_ADDR));
    
    /* miscCFG setting */
    if (max17050_write_reg(chip->client, MAX17050_MISCCFG_ADDR, MAX17050_MISCCFG_INIT_DATA) < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : ADDR = 0x%02X \n", __func__, MAX17050_MISCCFG_ADDR);
        MAX17050_RECLOG("[%s] i2c write err : ADDR = 0x%02X \n", __func__, MAX17050_MISCCFG_ADDR);
    }
    
    max17050_charge_function_limits_check(chip);
    max17050_get_vbat_adjust(chip, &batt_offset);
    max17050_set_vbat_adjust(chip, batt_offset);
    
    chip->lowbattery = 0;
    
    /* set temp data */
    for (i = 0;i < 3;i++) {
        max17050_get_battery_temperature(chip, &temp);
        temp_batt = temp_batt + temp;
    }
    temp_batt = temp_batt / 3;
    max17050_set_temperature(chip, temp_batt);
    max17050_set_battery_temperature(chip, temp_batt);
    
    msleep(300);    /* wait before 1st get soc */
    max17050_get_soc(chip, &battery_capacity);
    
    if (battery_capacity > MAX17050_BATTERY_FULL) {
        battery_capacity = MAX17050_BATTERY_FULL;
    } else if (battery_capacity <= 0) {
        battery_capacity = 0;
    }
    /* set 1st soc here */
    chip->soc = battery_capacity;
    max17050_set_battery_capacity(chip, battery_capacity);
    max17050_set_battery_capacity_threshold(chip, battery_capacity);
    
    max17050_get_battery_voltage(chip, &vbat_raw);
    max17050_set_battery_voltage(chip, vbat_raw);
    
    max17050_get_terminal_temperature(chip, &temp_ambient);
    max17050_set_terminal_temperature(chip, temp_ambient);
    
    max17050_get_receiver_temperature(chip, &temp_case);
    max17050_set_receiver_temperature(chip, temp_case);
    
    max17050_get_charge_temperature(chip, &temp_charge);
    max17050_set_charge_temperature(chip, temp_charge);
    
    max17050_get_center_temperature(chip, &temp_center);
    max17050_set_center_temperature(chip, temp_center);
    
    max17050_get_sensor_temperature(chip, &temp_sensor);
    max17050_set_sensor_temperature(chip, temp_sensor);
    
    max17050_convert_batt_voltage(chip);
    
    power_supply_changed(&chip->battery);
    
    max17050_initialized = 1;
    
    MAX17050_INFOLOG("[%s] chip->soc = %d \n", __func__, chip->soc);
    MAX17050_INFOLOG("[%s] chip->temp_batt = %d chip->vbat_raw = 0x%04x \n",
                         __func__, chip->temp_batt, chip->vbat_raw);
    MAX17050_INFOLOG("[%s] chip->temp_ambient = %d chip->temp_case = %d\n",
                         __func__, chip->temp_ambient, chip->temp_case);
    return;
}

static void max17050_monitor_work(struct max17050_chip *chip)
{
    int i = 0;
    int temp = 0;
    int batt_temp = 0;
    int vbat = 0;
    int vbat_raw = 0;
    int capacity = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    /* avarage 4 times value */
    for (i = 0;i < 4;i++) {
        max17050_get_battery_temperature(chip, &temp);
        batt_temp += temp;
        
        max17050_get_battery_voltage(chip, &vbat);
        vbat_raw += vbat;
        
        msleep(25);
    }
    batt_temp = batt_temp / 4;
    vbat_raw = vbat_raw / 4;
    
    max17050_set_temperature(chip, batt_temp);
    max17050_set_battery_temperature(chip, batt_temp);
    
    max17050_get_battery_capacity(chip, &capacity);
    max17050_set_battery_capacity(chip, capacity);
    max17050_set_battery_capacity_threshold(chip, capacity);
    
    max17050_set_battery_voltage(chip, vbat_raw);
    max17050_convert_batt_voltage(chip);
    
    MAX17050_DBGLOG("[%s] chip->temp_batt = %d \n", __func__, chip->temp_batt);
    MAX17050_DBGLOG("[%s] chip->vbat_raw = 0x%04x \n", __func__, chip->vbat_raw);
    
    return;
}

static void max17050_alert_work(struct max17050_chip *chip)
{
    int result = 0;
    u16 status = 0;
    int i = 0;
    int temp = 0;
    int batt_temp = 0;
    int vbat = 0;
    int vbat_raw = 0;
    int capacity = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    /* read interrupt status */
    result = max17050_read_reg(chip->client, MAX17050_STATUS_ADDR);
    if (result < 0){
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    status = result;
    MAX17050_DBGLOG("[%s] READ ACCESS: 0x%04X\n", __func__, status);
    
    if (!(status & (MAX17050_STATUS_SMX | MAX17050_STATUS_SMN | MAX17050_STATUS_VMN))){
        max17050_irq_check_counter++;
        if (max17050_irq_check_counter >= MAX17050_IRQ_CHECK_CNT_LIMIT){
            max17050_init_no_skip_flag = 1;
            /* set hardware specific data */
            result = max17050_setup(chip);
            
            /* learning data restore */
            max17050_learning_data_restore(chip);
            
            max17050_init_no_skip_flag = 0;
            max17050_irq_check_counter = 0;
        }
    }
    
    /* soc interrupt */
    if (status & (MAX17050_STATUS_SMX | MAX17050_STATUS_SMN)) {
        /* avarage 4 times value */
        for (i = 0;i < 4;i++) {
            max17050_get_battery_temperature(chip, &temp);
            batt_temp += temp;
            
            max17050_get_battery_voltage(chip, &vbat);
            vbat_raw += vbat;
            
            msleep(25);
        }
        batt_temp = batt_temp / 4;
        vbat_raw = vbat_raw / 4;
        
        max17050_set_temperature(chip, batt_temp);
        max17050_set_battery_temperature(chip, batt_temp);
        
        max17050_get_battery_capacity(chip, &capacity);
        max17050_set_battery_capacity(chip, capacity);
        max17050_set_battery_capacity_threshold(chip, capacity);
        
        max17050_set_battery_voltage(chip, vbat_raw);
        max17050_convert_batt_voltage(chip);
    }
    
    /* low battery */
    if (status & (MAX17050_STATUS_VMN)) {
        max17050_irq_lowbat_flag = 1;
        cancel_delayed_work(&chip->battery_monitor);
        chip->monitor_interval = MAX17050_MONITOR_DELAY_100MS;
        schedule_delayed_work(&chip->battery_monitor, chip->monitor_interval);
        MAX17050_DBGLOG("[%s] low battery interrupt \n", __func__);
        MAX17050_RECLOG("[%s] low battery interrupt \n", __func__);
    }
    
    return;
    
max17050_err:
    max17050_error_sequence();
    return;
}

static void max17050_monitor_mobile_temp(struct max17050_chip *chip)
{
    int temp_ambient = 0;
    int temp_case = 0;
    int temp_charge = 0;
    int temp_center = 0;
    int temp_sensor = 0;
    int temp_update_flag = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    max17050_get_terminal_temperature(chip, &temp_ambient);
    if (chip->temp_ambient != (temp_ambient/10)) {
        temp_update_flag |= 0x0001;
    }
    max17050_set_terminal_temperature(chip, temp_ambient);
    
    max17050_get_receiver_temperature(chip, &temp_case);
    if (chip->temp_case != (temp_case/10)) {
        temp_update_flag |= 0x0002;
    }
    max17050_set_receiver_temperature(chip, temp_case);
    
    max17050_get_charge_temperature(chip, &temp_charge);
    if (chip->temp_charge != (temp_charge/10)) {
        temp_update_flag |= 0x0004;
    }
    max17050_set_charge_temperature(chip, temp_charge);
    
    max17050_get_center_temperature(chip, &temp_center);
    if (chip->temp_center != (temp_center/10)) {
        temp_update_flag |= 0x0008;
    }
    max17050_set_center_temperature(chip, temp_center);
    
    max17050_get_sensor_temperature(chip, &temp_sensor);
    if (chip->temp_sensor != temp_sensor) {
        temp_update_flag |= 0x0010;
    }
    max17050_set_sensor_temperature(chip, temp_sensor);
    
    power_supply_changed(&chip->battery);
    MAX17050_DBGLOG("[%s] temp_update!! = %x\n", __func__, temp_update_flag);
    
    MAX17050_DBGLOG("[%s] chip->temp_ambient = %d\n", __func__, chip->temp_ambient);
    MAX17050_DBGLOG("[%s] chip->temp_case = %d\n", __func__, chip->temp_case);
    MAX17050_DBGLOG("[%s] chip->temp_charge = %d\n", __func__, chip->temp_charge);
    MAX17050_DBGLOG("[%s] chip->temp_center = %d\n", __func__, chip->temp_center);
    MAX17050_DBGLOG("[%s] chip->temp_sensor = %d\n", __func__, chip->temp_sensor);
    
    return;
}

static int max17050_is_update_params(struct max17050_chip *chip)
{
	int ret = 0;
	static unsigned char old_lowbatt = 0;

	MAX17050_DBGLOG("[%s] in\n", __func__);

	if (chip == NULL) {
		MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
		if (the_chip == NULL) {
			MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
			return -1;
		}
		chip = the_chip;
	}

	if (old_lowbatt != chip->lowbattery) {
		MAX17050_WARNLOG("[%s] update low batt info(%d -> %d)\n", __func__, old_lowbatt, chip->lowbattery);
		old_lowbatt = chip->lowbattery;
		ret = 1;
	}

	MAX17050_DBGLOG("[%s] ret:%d\n", __func__, ret);
	return ret;
}

static void max17050_charge_function_limits_check(struct max17050_chip *chip)
{
    int result = 0;
    int i = 0;
    u16 val = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    /* get nv value */
    result = get_nonvolatile((uint8_t*)&val, APNV_FUNC_LIMITS_I, 2);
    MAX17050_DBGLOG("[%s] NV read : get value = 0x%04x \n", __func__, val);
    if (result < 0) {
        val = 0x0000;
        MAX17050_ERRLOG("[%s] NV read err : set value = 0x%x \n", __func__, val);
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
    if (val != MAX17050_FUNC_NO_LIMIT) {
        for (i = 0;i < MAX17050_FUNC_LIMIT_NUM;i++) {
            if (val & 0x0001) {
                chip->check_disable[i] = true;
            } else {
                chip->check_disable[i] = false;
            }
            val = val >> 1;
            MAX17050_DBGLOG("[%s] check_disable[%d] = %d\n", __func__, i, chip->check_disable[i]);
        }
    }
}

static int max17050_charge_battery_present_check(struct max17050_chip *chip)
{
    static int batt_absent_count = 0;
    int batt_present = 1;
    int temp = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    max17050_get_battery_temperature(chip, &temp);
    
    if (temp <= BATTERY_NOT_PRESENT_TEMP) {
        MAX17050_WARNLOG("[%s]battery temp(%d)\n", __func__, temp);
        batt_absent_count++;
    } else {
        batt_absent_count = 0;
    }
    
    if (batt_absent_count < 3) {
        batt_present = 1;
    } else {
        batt_present = 0;
        if ((fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE)) {
            MAX17050_ERRLOG("[%s]remove battery\n", __func__);
            /* GCF Support */
            if (chip->check_disable[MAX17050_FUNC_LIMIT_BATTERY_PRESENT] == false) {
                max17050_error_sequence();
            }
        }
    }
    
    max17050_set_battery_temperature(chip, temp);
    MAX17050_DBGLOG("[%s] batt_present = %d\n", __func__, batt_present);
    
    return batt_present;
}

static void max17050_battery_present_monitor(struct work_struct *work)
{
    struct delayed_work *dwork;
    struct max17050_chip *chip;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (work == NULL) {
        MAX17050_ERRLOG("[%s] work pointer is NULL\n", __func__);
        return;
    }
    dwork = to_delayed_work(work);
    
    if (dwork == NULL) {
        MAX17050_ERRLOG("[%s] dwork pointer is NULL\n", __func__);
        return;
    }
    chip = container_of(dwork, struct max17050_chip, battery_present_monitor);
    
    set_bit(MAX17050_EVENT_PRESENT_MONITOR, &chip->event_state);
    max17050_wakeup(chip);
}

static irqreturn_t max17050_alert_isr(int irq_no, void *data)
{
    struct max17050_chip *chip =(struct max17050_chip *)data;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    max17050_interrupt_disable();
    set_bit(MAX17050_EVENT_ALERT_ISR, &chip->event_state);
    max17050_wakeup(chip);
    
    return IRQ_HANDLED;
}

static int max17050_setup(struct max17050_chip *chip)
{
    int result = 0;
    int i = 0;
    u16 val = 0;
    u16 *p_batterydata_table = 0;
    u16 batterydata_tablesize = 0;
    u16 status_work = 0;
    u16 verify_work = 0;
    u16 vfsoc_work = 0;
    u16 qh_work = 0;
    u16 remcap_work = 0;
    u16 repcap_work = 0;
    u16 dq_acc_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    /* Step 0. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    max17050_init_skip_flag = 0;
    result = max17050_read_reg(chip->client, MAX17050_STATUS_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    status_work = result;
    MAX17050_DBGLOG("[%s] Step 0.Check for POR ACCESS: 0x%04X\n", __func__, max17050_read_reg(chip->client, MAX17050_STATUS_ADDR));
    
    /* Step 1. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    msleep(500);
    
    if (max17050_get_model_type(chip) != 0) {
        MAX17050_ERRLOG("[%s] Error! Unknown model\n", __func__);
    }
    status_work &= MAX17050_STATUS_POR;
    if ((status_work == 0) && (max17050_init_no_skip_flag == 0)) {
        result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_AGE_I, 2);
        if (result < 0) {
            the_learning_data.td_age = MAX17050_AGE_INIT_DATA;
        } else {
            the_learning_data.td_age = val;
        }
        max17050_init_skip_flag = 1;
        return 0;
    }
    
    /* Step 2. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    MAX17050_DBGLOG("[%s] Step 2.\n", __func__);
    result = max17050_write_reg(chip->client, MAX17050_CONFIG_ADDR, MAX17050_CONFIG_INIT_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_CONFIG_ADDR, max17050_read_reg(chip->client, MAX17050_CONFIG_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_FILTERCFG_ADDR, MAX17050_FILTERCFG_INIT_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FILTERCFG_ADDR, max17050_read_reg(chip->client, MAX17050_FILTERCFG_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_RELAXCFG_ADDR, MAX17050_RELAXCFG_INIT_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_RELAXCFG_ADDR, max17050_read_reg(chip->client, MAX17050_RELAXCFG_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_LEARNCFG_ADDR, MAX17050_LEARNCFG_INIT_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_LEARNCFG_ADDR, max17050_read_reg(chip->client, MAX17050_LEARNCFG_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_FULLSOCTHR_ADDR, MAX17050_FULLSOCTHR_INIT_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLSOCTHR_ADDR, max17050_read_reg(chip->client, MAX17050_FULLSOCTHR_ADDR));
    
    /* Step 4. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    MAX17050_DBGLOG("[%s] Step 4.\n", __func__);
    /* unmask max17050 registers */
    result = max17050_write_reg(chip->client, MAX17050_WRITE_MASK1_ADDR, MAX17050_WRITE_UNMASK1_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    result = max17050_write_reg(chip->client, MAX17050_WRITE_MASK2_ADDR, MAX17050_WRITE_UNMASK2_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ACCESS: 0x%04X\n", __func__, max17050_read_reg(chip->client, MAX17050_WRITE_MASK1_ADDR));
    MAX17050_DBGLOG("[%s] ACCESS: 0x%04X\n", __func__, max17050_read_reg(chip->client, MAX17050_WRITE_MASK2_ADDR));
    
    /* Step 5. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* write battery data 1 */
    if (max17050_battery_type == MAX17050_BATTERY_CAPA_2600) {
        p_batterydata_table   = (u16 *)max17050_table_435_2600;
        batterydata_tablesize = ARRAY_SIZE(max17050_table_435_2600);
    } else {
        p_batterydata_table   = (u16 *)max17050_table_435_3020;
        batterydata_tablesize = ARRAY_SIZE(max17050_table_435_3020);
    }
    
    for (i = 0; i < batterydata_tablesize; i++) {
        result = max17050_write_reg(chip->client, (MAX17050_BATTERYTABLE_ADDR + i), p_batterydata_table[i]);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
            goto max17050_setup_err;
        }
    }
    
    /* Verify Check */
    for (i = 0; i < batterydata_tablesize; i++) {
        result = max17050_read_reg(chip->client, MAX17050_BATTERYTABLE_ADDR + i);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
            goto max17050_setup_err;
        }
        verify_work = result;
        
        MAX17050_DBGLOG("[%s] ACCESS: 0x%04X\n", __func__, verify_work);
        if (verify_work != p_batterydata_table[i]) {
            MAX17050_ERRLOG("[%s] verify Error!!! ADDR:0x%04x ACCESS:0x%04x \n", __func__, (MAX17050_BATTERYTABLE_ADDR + i), verify_work);
            goto max17050_setup_err;
        }
    }
    
    /* Step 8. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* mask max17050 registers */
    result = max17050_write_reg(chip->client, MAX17050_WRITE_MASK1_ADDR, MAX17050_WRITE_MASK1_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    result = max17050_write_reg(chip->client, MAX17050_WRITE_MASK2_ADDR, MAX17050_WRITE_MASK2_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ACCESS: 0x%04X\n", __func__, max17050_read_reg(chip->client, MAX17050_WRITE_MASK1_ADDR));
    MAX17050_DBGLOG("[%s] ACCESS: 0x%04X\n", __func__, max17050_read_reg(chip->client, MAX17050_WRITE_MASK2_ADDR));
    
    /* Step 9. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* Verify Check */
    for (i = 0; i < batterydata_tablesize; i++) {
        result = max17050_read_reg(chip->client, MAX17050_BATTERYTABLE_ADDR + i);
        if (result != 0x0000) {
            MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
            goto max17050_setup_err;
        }
    }
    
    /* Step 10. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    result = max17050_write_reg(chip->client, MAX17050_TEMPNOM_ADDR, max17050_setup_init_table[MAX17050_REGISTER_TEMPNOM][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_TEMPNOM_ADDR, max17050_read_reg(chip->client, MAX17050_TEMPNOM_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_RCOMP0_ADDR, max17050_setup_init_table[MAX17050_REGISTER_RCOMP0][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_RCOMP0_ADDR, max17050_read_reg(chip->client, MAX17050_RCOMP0_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_TEMPCO_ADDR, max17050_setup_init_table[MAX17050_REGISTER_TEMPCO][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_TEMPCO_ADDR, max17050_read_reg(chip->client, MAX17050_TEMPCO_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_ICHGTERM_ADDR, max17050_setup_init_table[MAX17050_REGISTER_ICHGTERM][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_ICHGTERM_ADDR, max17050_read_reg(chip->client, MAX17050_ICHGTERM_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_V_EMPTY_ADDR, max17050_setup_init_table[MAX17050_REGISTER_V_EMPTY][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_V_EMPTY_ADDR, max17050_read_reg(chip->client, MAX17050_V_EMPTY_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_QRESIDUAL_00_ADDR, max17050_setup_init_table[MAX17050_REGISTER_QRESIDUAL_00][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QRESIDUAL_00_ADDR, max17050_read_reg(chip->client, MAX17050_QRESIDUAL_00_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_QRESIDUAL_10_ADDR, max17050_setup_init_table[MAX17050_REGISTER_QRESIDUAL_10][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QRESIDUAL_10_ADDR, max17050_read_reg(chip->client, MAX17050_QRESIDUAL_10_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_QRESIDUAL_20_ADDR, max17050_setup_init_table[MAX17050_REGISTER_QRESIDUAL_20][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QRESIDUAL_20_ADDR, max17050_read_reg(chip->client, MAX17050_QRESIDUAL_20_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_QRESIDUAL_30_ADDR, max17050_setup_init_table[MAX17050_REGISTER_QRESIDUAL_30][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QRESIDUAL_30_ADDR, max17050_read_reg(chip->client, MAX17050_QRESIDUAL_30_ADDR));
    
    /* Step 11. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    result = max17050_write_reg(chip->client, MAX17050_FULLCAP_ADDR, max17050_setup_init_table[MAX17050_REGISTER_FULLCAP][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLCAP_ADDR, max17050_read_reg(chip->client, MAX17050_FULLCAP_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_DESIGNCAP_ADDR, max17050_setup_init_table[MAX17050_REGISTER_DESIGNCAP][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_DESIGNCAP_ADDR, max17050_read_reg(chip->client, MAX17050_DESIGNCAP_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_FULLCAPNOM_ADDR, max17050_setup_init_table[MAX17050_REGISTER_FULLCAPNOM][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLCAPNOM_ADDR, max17050_read_reg(chip->client, MAX17050_FULLCAPNOM_ADDR));
    
    /* Step 13. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    msleep(350);
    
    /* Step 14. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    result = max17050_read_reg(chip->client, MAX17050_SOCVF_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    vfsoc_work = result;
    
    result = max17050_write_reg(chip->client, MAX17050_VFSOC0_MASK_ADDR, MAX17050_VFSOC0_UNMASK_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_VFSOC0_MASK_ADDR, max17050_read_reg(chip->client, MAX17050_VFSOC0_MASK_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_VFSOC0_ADDR, vfsoc_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_VFSOC0_ADDR, max17050_read_reg(chip->client, MAX17050_VFSOC0_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_VFSOC0_MASK_ADDR, MAX17050_VFSOC0_MASK_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_VFSOC0_MASK_ADDR, max17050_read_reg(chip->client, MAX17050_VFSOC0_MASK_ADDR));
    
    result = max17050_read_reg(chip->client, MAX17050_QH_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    qh_work = result;
    
    result = max17050_write_reg(chip->client, MAX17050_QH0_ADDR, qh_work);
        if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QH0_ADDR, max17050_read_reg(chip->client, MAX17050_QH0_ADDR));
    
    /* Step 15. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    result = max17050_write_reg(chip->client, MAX17050_CYCLES_ADDR, MAX17050_CYCLES_INIT_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_CYCLES_ADDR, max17050_read_reg(chip->client, MAX17050_CYCLES_ADDR));
    
    /* Step 16. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    remcap_work = (vfsoc_work * max17050_setup_init_table[MAX17050_REGISTER_DESIGNCAP][max17050_battery_type]) / 25600;
    result = max17050_write_reg(chip->client, MAX17050_REMCAPMIX_ADDR, remcap_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_REMCAPMIX_ADDR, max17050_read_reg(chip->client, MAX17050_REMCAPMIX_ADDR));
    
    repcap_work = (remcap_work * ( (max17050_setup_init_table[MAX17050_REGISTER_FULLCAP][max17050_battery_type] * 10) /  max17050_setup_init_table[MAX17050_REGISTER_DESIGNCAP][max17050_battery_type] ) ) / 10;
    result = max17050_write_reg(chip->client, MAX17050_REMCAPREP_ADDR, repcap_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_REMCAPREP_ADDR, max17050_read_reg(chip->client, MAX17050_REMCAPREP_ADDR));
    
    dq_acc_work = (max17050_setup_init_table[MAX17050_REGISTER_DESIGNCAP][max17050_battery_type] / 4);
    result = max17050_write_reg(chip->client, MAX17050_DQACC_ADDR, dq_acc_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_DQACC_ADDR, max17050_read_reg(chip->client, MAX17050_DQACC_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_DPACC_ADDR, MAX17050_DPACC_INIT_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_DPACC_ADDR, max17050_read_reg(chip->client, MAX17050_DPACC_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_FULLCAP_ADDR, max17050_setup_init_table[MAX17050_REGISTER_FULLCAP][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLCAP_ADDR, max17050_read_reg(chip->client, MAX17050_FULLCAP_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_DESIGNCAP_ADDR, max17050_setup_init_table[MAX17050_REGISTER_DESIGNCAP][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_DESIGNCAP_ADDR, max17050_read_reg(chip->client, MAX17050_DESIGNCAP_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_FULLCAPNOM_ADDR, max17050_setup_init_table[MAX17050_REGISTER_DESIGNCAP][max17050_battery_type]);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLCAPNOM_ADDR, max17050_read_reg(chip->client, MAX17050_FULLCAPNOM_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_SOCREP_ADDR, vfsoc_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_SOCREP_ADDR, max17050_read_reg(chip->client, MAX17050_SOCREP_ADDR));
    
    /* Step 17. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    result = max17050_read_reg(chip->client, MAX17050_STATUS_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    status_work = result;
    status_work &= ~(MAX17050_STATUS_POR);
    result = max17050_write_reg(chip->client, MAX17050_STATUS_ADDR, status_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_setup_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_STATUS_ADDR, max17050_read_reg(chip->client, MAX17050_STATUS_ADDR));
    
    return 0;
    
max17050_setup_err:
    return -1;
}

static void max17050_learning_data_backup(struct max17050_chip *chip)
{
    int result = 0;
    u16 val = 0;
    u16 backup_work = 0;
    u16 fullcap0_work = 0;
    u16 socmix_work = 0;
    u16 remcap_work = 0;
    u16 aq_acc_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    result = max17050_read_reg(chip->client, MAX17050_RCOMP0_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_RCOMP0_I, 2);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_RCOMP0_I, val);
        } else {
            the_learning_data.td_rcomp0 = val;
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_TEMPCO_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_TEMPCO_I, 2);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_TEMPCO_I, val);
        } else {
            the_learning_data.td_tempco = val;
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_FULLCAPNOM_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_FULLCAPNOM_I, 2);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_FULLCAPNOM_I, val);
        } else {
            the_learning_data.td_fullcapnom = val;
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_FULLCAP_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        if (val >= (the_learning_data.td_fullcapnom + 0xc8)) {
            val = the_learning_data.td_fullcapnom + 0xc8;
            result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_FULLCAP_I, 2);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_FULLCAP_I, val);
            } else {
                the_learning_data.td_fullcap = val;
            }
            
            result = max17050_read_reg(chip->client, MAX17050_FULLCAP0_ADDR);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
                goto max17050_err;
            }
            fullcap0_work = result;
            
            result = max17050_read_reg(chip->client, MAX17050_SOCMIX_ADDR);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
                goto max17050_err;
            }
            socmix_work = result;
            
            remcap_work = (socmix_work * fullcap0_work) / 25600;
            result = max17050_write_reg(chip->client, MAX17050_REMCAPMIX_ADDR, remcap_work);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
                goto max17050_err;
            }
            MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_REMCAPMIX_ADDR, max17050_read_reg(chip->client, MAX17050_REMCAPMIX_ADDR));
            
            result = max17050_write_reg(chip->client, MAX17050_FULLCAP_ADDR, the_learning_data.td_fullcap);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
                goto max17050_err;
            }
            MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLCAP_ADDR, max17050_read_reg(chip->client, MAX17050_FULLCAP_ADDR));
            
            aq_acc_work = (the_learning_data.td_fullcapnom / 4);
            result = max17050_write_reg(chip->client, MAX17050_DQACC_ADDR, aq_acc_work);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
                goto max17050_err;
            }
            MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_DQACC_ADDR, max17050_read_reg(chip->client, MAX17050_DQACC_ADDR));
            
            result = max17050_write_reg(chip->client, MAX17050_DPACC_ADDR, MAX17050_DPACC_INIT_DATA);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
                goto max17050_err;
            }
            MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_DPACC_ADDR, max17050_read_reg(chip->client, MAX17050_DPACC_ADDR));
        } else {
            result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_FULLCAP_I, 2);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_FULLCAP_I, val);
            } else {
                the_learning_data.td_fullcap = val;
            }
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_CYCLES_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        if (the_learning_data.td_cycles < val) {
            result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_CYCLES_I, 2);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_CYCLES_I, val);
            } else {
                the_learning_data.td_cycles = val;
            }
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_IAVG_EMPTY_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_IAVG_EMPTY_I, 2);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_IAVG_EMPTY_I, val);
        } else {
            the_learning_data.td_iavg_empty = val;
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_QRESIDUAL_00_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_QRTABLE00_I, 2);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_QRTABLE00_I, val);
        } else {
            the_learning_data.td_qrtable00 = val;
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_QRESIDUAL_10_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_QRTABLE10_I, 2);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_QRTABLE10_I, val);
        } else {
            the_learning_data.td_qrtable10 = val;
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_QRESIDUAL_20_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_QRTABLE20_I, 2);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_QRTABLE20_I, val);
        } else {
            the_learning_data.td_qrtable20 = val;
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_QRESIDUAL_30_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_QRTABLE30_I, 2);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_QRTABLE30_I, val);
        } else {
            the_learning_data.td_qrtable30 = val;
        }
    }
    
    result = max17050_read_reg(chip->client, MAX17050_AGE_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    } else {
        val = result;
        result = max17050_read_reg(chip->client, MAX17050_TEMPERATURE_ADDR);
        if (result < 0) {
            MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
            goto max17050_err;
        }
        backup_work = result;
        
        if ((val < the_learning_data.td_age) && ((backup_work > 0x0A00) && (backup_work < 0x8000))) {
            if ((the_learning_data.td_age - val) > 0x0300) {
                val = the_learning_data.td_age - 0x0300;
            }
            result = set_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_AGE_I, 2);
            if (result < 0) {
                MAX17050_ERRLOG("[%s] NV write err : item ID %d set value = 0x%x \n", __func__, APNV_CHARGE_FG_AGE_I, val);
            } else {
                the_learning_data.td_age = val;
                MAX17050_RECLOG("[%s] AGE UPDATE set value = 0x%04X\n", __func__, the_learning_data.td_age);
                MAX17050_RECLOG("[%s] CYCLES UPDATE set value = 0x%04X\n", __func__, the_learning_data.td_cycles);
            }
        }
    }
    
    return;
    
max17050_err:
    max17050_error_sequence();
    return;
}

static void max17050_learning_data_restore(struct max17050_chip *chip)
{
    int result = 0;
    u16 val = 0;
    u16 fullcap0_work = 0;
    u16 socmix_work = 0;
    u16 remcap_work = 0;
    u16 aq_acc_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    memset(&the_learning_data, 0, sizeof(the_learning_data));
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_RCOMP0_I, 2);
    if (result < 0) {
        the_learning_data.td_rcomp0 = max17050_setup_init_table[MAX17050_REGISTER_RCOMP0][max17050_battery_type];
    } else {
        the_learning_data.td_rcomp0 = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_TEMPCO_I, 2);
    if (result < 0) {
        the_learning_data.td_tempco = max17050_setup_init_table[MAX17050_REGISTER_TEMPCO][max17050_battery_type];
    } else {
        the_learning_data.td_tempco = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_FULLCAP_I, 2);
    if (result < 0) {
        the_learning_data.td_fullcap = max17050_setup_init_table[MAX17050_REGISTER_FULLCAP][max17050_battery_type];
    } else {
        the_learning_data.td_fullcap = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_CYCLES_I, 2);
    if (result < 0) {
        the_learning_data.td_cycles = MAX17050_CYCLES_INIT_DATA;
    } else {
        the_learning_data.td_cycles = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_FULLCAPNOM_I, 2);
    if (result < 0) {
        the_learning_data.td_fullcapnom = max17050_setup_init_table[MAX17050_REGISTER_FULLCAPNOM][max17050_battery_type];
    } else {
        the_learning_data.td_fullcapnom = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_IAVG_EMPTY_I, 2);
    if (result < 0) {
        the_learning_data.td_iavg_empty = MAX17050_IAVG_EMPTY_INIT_DATA;
    } else {
        the_learning_data.td_iavg_empty = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_QRTABLE00_I, 2);
    if (result < 0) {
        the_learning_data.td_qrtable00 = max17050_setup_init_table[MAX17050_REGISTER_QRESIDUAL_00][max17050_battery_type];
    } else {
        the_learning_data.td_qrtable00 = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_QRTABLE10_I, 2);
    if (result < 0) {
        the_learning_data.td_qrtable10 = max17050_setup_init_table[MAX17050_REGISTER_QRESIDUAL_10][max17050_battery_type];
    } else {
        the_learning_data.td_qrtable10 = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_QRTABLE20_I, 2);
    if (result < 0) {
        the_learning_data.td_qrtable20 = max17050_setup_init_table[MAX17050_REGISTER_QRESIDUAL_20][max17050_battery_type];
    } else {
        the_learning_data.td_qrtable20 = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_QRTABLE30_I, 2);
    if (result < 0) {
        the_learning_data.td_qrtable30 = max17050_setup_init_table[MAX17050_REGISTER_QRESIDUAL_30][max17050_battery_type];
    } else {
        the_learning_data.td_qrtable30 = val;
    }
    
    result = get_nonvolatile((uint8_t*)&val, APNV_CHARGE_FG_AGE_I, 2);
    if (result < 0) {
        the_learning_data.td_age = MAX17050_AGE_INIT_DATA;
    } else {
        the_learning_data.td_age = val;
    }
    
    /* Step 21. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    MAX17050_DBGLOG("[%s] Step 21.\n", __func__);
    
    result = max17050_write_reg(chip->client, MAX17050_RCOMP0_ADDR, the_learning_data.td_rcomp0);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_RCOMP0_ADDR, max17050_read_reg(chip->client, MAX17050_RCOMP0_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_TEMPCO_ADDR, the_learning_data.td_tempco);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_TEMPCO_ADDR, max17050_read_reg(chip->client, MAX17050_TEMPCO_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_FULLCAPNOM_ADDR, the_learning_data.td_fullcapnom);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLCAPNOM_ADDR, max17050_read_reg(chip->client, MAX17050_FULLCAPNOM_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_IAVG_EMPTY_ADDR, the_learning_data.td_iavg_empty);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_IAVG_EMPTY_ADDR, max17050_read_reg(chip->client, MAX17050_IAVG_EMPTY_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_QRESIDUAL_00_ADDR, the_learning_data.td_qrtable00);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QRESIDUAL_00_ADDR, max17050_read_reg(chip->client, MAX17050_QRESIDUAL_00_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_QRESIDUAL_10_ADDR, the_learning_data.td_qrtable10);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QRESIDUAL_10_ADDR, max17050_read_reg(chip->client, MAX17050_QRESIDUAL_10_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_QRESIDUAL_20_ADDR, the_learning_data.td_qrtable20);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QRESIDUAL_20_ADDR, max17050_read_reg(chip->client, MAX17050_QRESIDUAL_20_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_QRESIDUAL_30_ADDR, the_learning_data.td_qrtable30);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_QRESIDUAL_30_ADDR, max17050_read_reg(chip->client, MAX17050_QRESIDUAL_30_ADDR));
    
    /* Step 22. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    MAX17050_DBGLOG("[%s] Step 22.\n", __func__);
    msleep(350);
    
    /* Step 23. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    MAX17050_DBGLOG("[%s] Step 23.\n", __func__);
    result = max17050_read_reg(chip->client, MAX17050_FULLCAP0_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    fullcap0_work = result;
    
    result = max17050_read_reg(chip->client, MAX17050_SOCMIX_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    socmix_work = result;
    
    remcap_work = (socmix_work * fullcap0_work) / 25600;
    result = max17050_write_reg(chip->client, MAX17050_REMCAPMIX_ADDR, remcap_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_REMCAPMIX_ADDR, max17050_read_reg(chip->client, MAX17050_REMCAPMIX_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_FULLCAP_ADDR, the_learning_data.td_fullcap);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLCAP_ADDR, max17050_read_reg(chip->client, MAX17050_FULLCAP_ADDR));
    
    aq_acc_work = (the_learning_data.td_fullcapnom / 4);
    result = max17050_write_reg(chip->client, MAX17050_DQACC_ADDR, aq_acc_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_DQACC_ADDR, max17050_read_reg(chip->client, MAX17050_DQACC_ADDR));
    
    result = max17050_write_reg(chip->client, MAX17050_DPACC_ADDR, MAX17050_DPACC_INIT_DATA);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    MAX17050_DBGLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_DPACC_ADDR, max17050_read_reg(chip->client, MAX17050_DPACC_ADDR));
    
    /* Step 24. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    MAX17050_DBGLOG("[%s] Step 24.\n", __func__);
    msleep(350);
    
    /* Step 25. - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    MAX17050_DBGLOG("[%s] Step 25.\n", __func__);
    result = max17050_write_reg(chip->client, MAX17050_CYCLES_ADDR, the_learning_data.td_cycles);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    return;
    
max17050_err:
    max17050_error_sequence();
    return;
}

static void max17050_interrupt_enable(void)
{
    if ((max17050_irq_enable_flag == 0) && (max17050_irq_lowbat_flag == 0)) {
        max17050_irq_enable_flag = 1;
        enable_irq(max17050_irq);
    }
}

static void max17050_interrupt_disable(void)
{
    if (max17050_irq_enable_flag != 0) {
        disable_irq_nosync(max17050_irq);
        max17050_irq_enable_flag = 0;
    }
}

static void max17050_interrupt_init(struct max17050_chip *chip)
{
    int error;
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return;
        }
        chip = the_chip;
    }
    
    if(gpio_request(MAX17050_INT_GPIO, "MAX17050_INT") < 0) {
        MAX17050_ERRLOG("%s:GPIO INT Request failed\n", __func__);
        return;
    }
    max17050_irq_enable_flag = 1;
    gpio_direction_input(MAX17050_INT_GPIO);
    max17050_irq = gpio_to_irq(MAX17050_INT_GPIO);
    error = request_irq(max17050_irq, max17050_alert_isr,
                        (IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND),
                        "MAX17050_INT", chip);
                        
    MAX17050_DBGLOG("[%s] request_irq: %d \n", __func__, error);
    enable_irq_wake(max17050_irq);
}

static void max17050_error_sequence(void)
{
    MAX17050_ERRLOG("[%s] max17050 Error!!! -> Shutdown\n", __func__);
    fj_chg_emergency_current_off();
    kernel_power_off();
}

static int max17050_thread(void * ptr)
{
    struct max17050_chip *chip = ptr;
    int result = 0;
    
    MAX17050_DBGLOG("[%s] wakeup\n", __func__);
    
    if (chip == NULL) {
        MAX17050_WARNLOG("[%s] chip pointer is NULL\n", __func__);
        if (the_chip == NULL) {
            MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
            return -1;
        }
        chip = the_chip;
    }
    
    /* set hardware specific data */
    max17050_setup(chip);
    
    if (max17050_init_skip_flag == 0) {
        /* learning data restore */
        max17050_learning_data_restore(chip);
    }
    
    if (battery_status != POWER_SUPPLY_STATUS_DISCHARGING) {
        MAX17050_INFOLOG("[%s] status %d > %d\n", __func__, chip->status, battery_status);
        chip->status = battery_status;
    }
    
    if (battery_health != POWER_SUPPLY_HEALTH_GOOD) {
        MAX17050_INFOLOG("[%s] health %d > %d\n", __func__, chip->health, battery_health);
        chip->health = battery_health;
    }
    
    max17050_delayed_init(chip);
    
    /* set wakeup interrupt */
    max17050_interrupt_init(chip);
    
    chip->present = 1;
    chip->monitor_interval = MAX17050_MONITOR_DELAY_300MS;
    chip->present_monitor_interval = MAX17050_MONITOR_DELAY_10S;
    chip->mt_interval = MAX17050_MONITOR_DELAY_1S;
    
    ovp_device_initialized(INITIALIZE_SUPPLY);
    
    schedule_delayed_work(&chip->battery_present_monitor, chip->present_monitor_interval);
    schedule_delayed_work(&chip->mt_monitor, chip->mt_interval);
    
    set_freezable();
    
    do {
        wait_event_freezable(chip->wq, (max17050_get_queue_state() || kthread_should_stop()));
        
        if (kthread_should_stop()) {
            MAX17050_INFOLOG("[%s] while loop break\n", __func__);
            break;
        }
        
        if (test_bit(MAX17050_EVENT_SUSPEND, &chip->event_state) == 0) {
            max17050_clear_queue_state();
            MAX17050_DBGLOG("[%s] event:0x%08x\n", __func__, (int)chip->event_state);
            
            if (test_bit(MAX17050_EVENT_PRESENT_MONITOR, &chip->event_state)) {
                MAX17050_DBGLOG("[%s] event %s\n", __func__, "PRESENT_MONITOR");
                max17050_charge_battery_present_check(chip);
                clear_bit(MAX17050_EVENT_PRESENT_MONITOR, &chip->event_state);
                schedule_delayed_work(&chip->battery_present_monitor, chip->present_monitor_interval);
            }
            
            if (test_bit(MAX17050_EVENT_BATT_MONITOR, &chip->event_state)) {
                MAX17050_DBGLOG("[%s] event %s\n", __func__, "BATT_MONITOR");
                max17050_monitor_work(chip);
                if (max17050_is_update_params(chip)) {
                    power_supply_changed(&chip->battery);
                }
                if (chip->lowbattery != 0) {
                    /* low battery */
                    if ((fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE)) {
                        chip->monitor_interval = MAX17050_MONITOR_DELAY_300MS;
                        schedule_delayed_work(&chip->battery_monitor, chip->monitor_interval);
                    } else {
                        MAX17050_INFOLOG("[%s]lowbatt Shutdown\n", __func__);
                        fj_chg_emergency_current_off();
                        kernel_power_off();
                    }
                } else if ((fj_lowbatt_volt_detect_cnt != 3) ||
                           (fj_lowbatt_soc_detect_cnt != 3)  ||
                           (max17050_irq_lowbat_flag != 0)) {
                    /* low battery check*/
                    chip->monitor_interval = MAX17050_MONITOR_DELAY_100MS;
                    schedule_delayed_work(&chip->battery_monitor, chip->monitor_interval);
                }
                clear_bit(MAX17050_EVENT_BATT_MONITOR, &chip->event_state);
            }
            
            if (test_bit(MAX17050_EVENT_MT_MONITOR, &chip->event_state)) {
                MAX17050_DBGLOG("[%s] event %s\n", __func__, "MT_MONITOR");
                max17050_monitor_mobile_temp(chip);
                clear_bit(MAX17050_EVENT_MT_MONITOR, &chip->event_state);
                chip->mt_interval = MAX17050_MONITOR_DELAY_1S;
                schedule_delayed_work(&chip->mt_monitor, chip->mt_interval);
            }
            
            if (test_bit(MAX17050_EVENT_ALERT_ISR, &chip->event_state)) {
                MAX17050_DBGLOG("[%s] event %s\n", __func__, "ALERT_ISR");
                clear_bit(MAX17050_EVENT_ALERT_ISR, &chip->event_state);
                max17050_alert_work(chip);
            }
            max17050_interrupt_enable();
        }
    } while(1);
    
    return result;
}

int max17050_mc_initialize(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] START!\n", __func__);
    
    /* parameter check */
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    max17050_init_no_skip_flag = 1;
    
    /* set hardware specific data */
    result = max17050_setup(the_chip);
    
    /* learning data restore */
    max17050_learning_data_restore(the_chip);
    
    max17050_init_no_skip_flag = 0;
    
max17050_err:
    return result;
}

int max17050_mc_quick_start(void)
{
    int result = 0;
    int retry_count = 0;
    s32 res = 0;
    u16 work_data;
    
    MAX17050_DBGLOG("[%s] START!\n", __func__);
    
    /* parameter check */
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    retry_count = 0;
    
    while(1){
        /* Step T1 */
        work_data = max17050_read_reg(the_chip->client, MAX17050_MISCCFG_ADDR);
        MAX17050_DBGLOG("[%s] Step T1 ACCESS: 0x%04X\n", __func__, work_data);
        work_data |= 0x1400;
        res = max17050_write_reg(the_chip->client, MAX17050_MISCCFG_ADDR, work_data);
        if (res < 0) {
            MAX17050_ERRLOG("[%s] WRITE ACCESS ERROR!!: ret:%d\n", __func__, res);
        }
        
        /* Step T2 */
        work_data = max17050_read_reg(the_chip->client, MAX17050_MISCCFG_ADDR);
        MAX17050_DBGLOG("[%s] Step T2 ACCESS: 0x%04X\n", __func__, work_data);
        work_data &= 0x1000;
        if ( work_data == 0x1000 ) {
            /* goto step T3 */
            break;
        }else{
            /* goto step T1 */
            retry_count++;
            if ( retry_count >= 3 ) {
                MAX17050_ERRLOG("[%s] Retry Count Over!!\n", __func__);
                result = -1;
                goto max17050_err;
            }
        }
    }
    
    retry_count = 0;
    
    while(1){
        /* Step T3 */
        work_data = max17050_read_reg(the_chip->client, MAX17050_MISCCFG_ADDR);
        MAX17050_DBGLOG("[%s] Step T3 ACCESS: 0x%04X\n", __func__, work_data);
        work_data &= 0xEFFF;
        res = max17050_write_reg(the_chip->client, MAX17050_MISCCFG_ADDR, work_data);
        if (res < 0) {
            MAX17050_ERRLOG("[%s] WRITE ACCESS ERROR!!: ret:%d\n", __func__, res);
        }
        
        /* Step T4 */
        work_data = max17050_read_reg(the_chip->client, MAX17050_MISCCFG_ADDR);
        MAX17050_DBGLOG("[%s] Step T4 ACCESS: 0x%04X\n", __func__, work_data);
        work_data &= 0x1000;
        if ( work_data == 0x0000 ) {
            /* goto step T5 */
            break;
        }else{
            /* goto step T3 */
            if ( retry_count >= 3 ) {
                MAX17050_ERRLOG("[%s] Retry Count Over!!\n", __func__);
                result = -1;
                goto max17050_err;
            }
            retry_count++;
        }
    }
    
    /* Step T5 */
    msleep(500);
    
    /* Step T6 */
    res = max17050_write_reg(the_chip->client, MAX17050_FULLCAP_ADDR, max17050_setup_init_table[MAX17050_REGISTER_FULLCAP][max17050_battery_type]);
    if (res < 0) {
        result = -1;
        MAX17050_ERRLOG("[%s] WRITE ACCESS ERROR!!: ret:%d\n", __func__, res);
        goto max17050_err;
    }
    MAX17050_INFOLOG("[%s] ADDR:0x%02X ACCESS: 0x%04X\n", __func__, MAX17050_FULLCAP_ADDR, max17050_read_reg(the_chip->client, MAX17050_FULLCAP_ADDR));
    /* Step T7 */
    msleep(500);
    
max17050_err:
    return result;
}

int max17050_mc_get_vcell(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_VCELL_ADDR);
    mutex_unlock(&the_chip->lock);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] max17050_read_reg Failure!!!\n", __func__);
        goto max17050_err;
    }
    result = result >> 3;
    
max17050_err:
    return result;
}

int max17050_mc_get_socrep(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_SOCREP_ADDR);
    mutex_unlock(&the_chip->lock);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] max17050_read_reg Failure!!!\n", __func__);
        goto max17050_err;
    }
    result = result >> 8;
    
max17050_err:
    return result;
}

int max17050_mc_get_socmix(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_SOCMIX_ADDR);
    mutex_unlock(&the_chip->lock);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] max17050_read_reg Failure!!!\n", __func__);
        goto max17050_err;
    }
    result = result >> 8;
    
max17050_err:
    return result;
}

int max17050_mc_get_socav(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_SOCAV_ADDR);
    mutex_unlock(&the_chip->lock);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] max17050_read_reg Failure!!!\n", __func__);
        goto max17050_err;
    }
    result = result >> 8;
    
max17050_err:
    return result;
}

int max17050_mc_get_socvf(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_SOCVF_ADDR);
    mutex_unlock(&the_chip->lock);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] max17050_read_reg Failure!!!\n", __func__);
        goto max17050_err;
    }
    result = result >> 8;
    
max17050_err:
    return result;
}

int max17050_mc_get_fullcap(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_FULLCAP_ADDR);
    mutex_unlock(&the_chip->lock);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] max17050_read_reg Failure!!!\n", __func__);
        goto max17050_err;
    }
    
max17050_err:
    return result;
}

int max17050_mc_get_age(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    result = the_learning_data.td_age;
    result = result >> 8;
    
max17050_err:
    return result;
}

int max17050_mc_get_current(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_CURRENT_ADDR);
    mutex_unlock(&the_chip->lock);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] max17050_read_reg Failure!!!\n", __func__);
        goto max17050_err;
    }
    
max17050_err:
    return result;
}

int max17050_mc_get_current_ave(void)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_AVERAGECURRENT_ADDR);
    mutex_unlock(&the_chip->lock);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] max17050_read_reg Failure!!!\n", __func__);
        goto max17050_err;
    }
    
max17050_err:
    return result;
}

int max17050_mc_set_shutdown_config(void)
{
    int result = 0;
    u16 config_temp = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        return result;
    }
    
    mutex_lock(&the_chip->lock);
    result = max17050_read_reg(the_chip->client, MAX17050_CONFIG_ADDR);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c read err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    config_temp = result;
    config_temp |= 0x0040;
    result = max17050_write_reg(the_chip->client, MAX17050_CONFIG_ADDR, config_temp);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    
max17050_err:
    mutex_unlock(&the_chip->lock);
    return result;
}

int max17050_get_property_func(enum power_supply_property psp,
                                union power_supply_propval *val)
{
    int result = 0;
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        return -EINVAL;
    }
    
    if (val == NULL) {
        MAX17050_ERRLOG("[%s] val pointer is NULL\n", __func__);
        return -EINVAL;
    }
    
    if(max17050_drv_initialized() == 0) {
        MAX17050_ERRLOG("[%s] error get_property called before init\n", __func__);
        return -EINVAL;
    }
    
    result = max17050_get_property(&the_chip->battery, psp, val);
    
    return result;
}

int max17050_set_status(int status)
{
    int old_status = 0;
    int status_update_flag = 0;
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        return -1;
    }
    
    old_status = the_chip->status;
    MAX17050_DBGLOG("[%s] in (status:%s)\n", __func__, fj_ps_status_str[status]);
    if (max17050_initialized == 1) {
        if(old_status != status) {
            MAX17050_INFOLOG("[%s] status %s > %s\n", __func__,
                                fj_ps_status_str[the_chip->status],
                                fj_ps_status_str[status]);
            MAX17050_RECLOG("[%s] status %s > %s\n", __func__,
                                fj_ps_status_str[the_chip->status],
                                fj_ps_status_str[status]);
            status_update_flag++;
        }
        
        if ((old_status == POWER_SUPPLY_STATUS_CHARGING) &&
             status == POWER_SUPPLY_STATUS_FULL){
            max17050_learning_data_backup(the_chip);
        }
        
        if ((old_status == POWER_SUPPLY_STATUS_DISCHARGING) &&
            (status == POWER_SUPPLY_STATUS_CHARGING) &&
            (the_chip->soc > 0)) {
            fj_soc_mask_expire = jiffies + (60 * HZ);   /* 60 sec */
            fj_soc_mask_start = 1;
        }
        
        if (status == POWER_SUPPLY_STATUS_FULL) {
            the_chip->soc = MAX17050_BATTERY_FULL;
            fj_soc_mask_expire = jiffies + (30 * HZ);   /* 30 sec */
            fj_soc_mask_start = 1;
        }
        
        if (status == POWER_SUPPLY_STATUS_DISCHARGING) {
            fj_soc_mask_expire = jiffies + (30 * HZ);   /* 30 sec */
            fj_soc_mask_start = 1;
        }
        
        if ((status == POWER_SUPPLY_STATUS_CHARGING) &&
            (the_chip->soc >= MAX17050_BATTERY_99)) {
            MAX17050_INFOLOG("[%s] keep full status\n", __func__);
            fj_keep_full_status = 1;
            the_chip->status = POWER_SUPPLY_STATUS_FULL;
        } else {
            fj_keep_full_status = 0;
            the_chip->status = status;
        }
        
        if (msm_touch_callback.callback != NULL) {
            msm_touch_callback.callback(status, msm_touch_callback.data);
        }
        
        MAX17050_DBGLOG("[%s] status %d, soc %d\n", __func__, the_chip->status, the_chip->soc);
        
        wake_lock_timeout(&max17050_chg_status, (3 * HZ));  /* for change power supply status */
        if (status_update_flag != 0) {
            power_supply_changed(&the_chip->battery);
        }
        
    } else {
        MAX17050_WARNLOG("[%s] called before init\n", __func__);
        battery_status = status;
    }
    
    return 0;
}
EXPORT_SYMBOL(max17050_set_status);

int max17050_set_health(int health)
{
    int result = 0;
    int health_update_flag = 0;
    
    MAX17050_DBGLOG("[%s] in (health:%s)\n", __func__, fj_ps_health_str[health]);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return -1;
    }
    
    if (max17050_initialized == 1) {
        if(the_chip->health != health) {
            MAX17050_INFOLOG("[%s] health %s > %s\n", __func__,
                                fj_ps_health_str[the_chip->health],
                                fj_ps_health_str[health]);
            MAX17050_RECLOG("[%s] health %s > %s\n", __func__,
                                fj_ps_health_str[the_chip->health],
                                fj_ps_health_str[health]);
            health_update_flag++;
        }
        the_chip->health = health;
        if (health_update_flag != 0) {
            power_supply_changed(&the_chip->battery);
        }
    } else {
        MAX17050_WARNLOG("%s called before init\n", __func__);
        battery_health = health;
    }
    
    return result;
}
EXPORT_SYMBOL(max17050_set_health);

int max17050_get_terminal_temp(int *terminal_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (terminal_temp == NULL) {
        MAX17050_ERRLOG("[%s] terminal_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    result = max17050_get_terminal_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s]get terminal temp error:%d\n", __func__, result);
        goto max17050_err;
    }
    *terminal_temp = (temp_work / 10);
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_terminal_temp);

int max17050_get_receiver_temp(int *receiver_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (receiver_temp == NULL) {
        MAX17050_ERRLOG("[%s] receiver_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    result = max17050_get_receiver_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s]get terminal temp error:%d\n", __func__, result);
        goto max17050_err;
    }
    *receiver_temp = (temp_work / 10);
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_receiver_temp);

int max17050_get_charge_temp(int *charge_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (charge_temp == NULL) {
        MAX17050_ERRLOG("[%s] charge_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    result = max17050_get_charge_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s]get charge temp error:%d\n", __func__, result);
        goto max17050_err;
    }
    *charge_temp = (temp_work / 10);
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_charge_temp);

int max17050_get_center_temp(int *center_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (center_temp == NULL) {
        MAX17050_ERRLOG("[%s] center_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    result = max17050_get_center_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s]get center temp error:%d\n", __func__, result);
        goto max17050_err;
    }
    *center_temp = (temp_work / 10);
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_center_temp);

int max17050_get_sensor_temp(int *sensor_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (sensor_temp == NULL) {
        MAX17050_ERRLOG("[%s] sensor_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    result = max17050_get_sensor_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s]get sensor temp error:%d\n", __func__, result);
        goto max17050_err;
    }
    *sensor_temp = temp_work;
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_sensor_temp);

int max17050_get_battery_temp(int *batt_temp)
{
    int result = 0;
    int temp_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (batt_temp == NULL) {
        MAX17050_ERRLOG("[%s] battery_temp pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    result = max17050_get_battery_temperature(the_chip, &temp_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s]get battery temp error:%d\n", __func__, result);
        goto max17050_err;
    }
    
    *batt_temp = (temp_work / 10);
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_battery_temp);

int max17050_set_battery_temp(int batt_temp)
{
    int result = 0;
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        return -1;
    }
    
    the_chip->temp_batt = batt_temp;
    
    return result;
}
EXPORT_SYMBOL(max17050_set_battery_temp);

int max17050_get_batt_vbat_raw(int *batt_vbat_raw)
{
    int result = 0;
    int volt_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (batt_vbat_raw == NULL) {
        MAX17050_ERRLOG("[%s] batt_vbat_raw pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    result = max17050_get_battery_voltage(the_chip, &volt_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s]get battery voltage error:%d\n", __func__, result);
        goto max17050_err;
    }
    
    *batt_vbat_raw = volt_work;
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_batt_vbat_raw);

int max17050_get_batt_capacity(int *batt_capacity)
{
    int i = 0;
    int result = 0;
    int capa_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (batt_capacity == NULL) {
        MAX17050_ERRLOG("[%s] battery_capacity pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    for (i = 0; i < 3; i++) {
        result = max17050_get_soc(the_chip, &capa_work);
        
        if (result < 0) {
            MAX17050_ERRLOG("[%s]get soc error:%d\n", __func__, result);
        } else {
            break;
        }
    }
    
    if (result < 0) {
        goto max17050_err;
    }
    
    *batt_capacity = capa_work;
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_batt_capacity);

int max17050_get_batt_capacity_vf(int *batt_capacity)
{
    int i = 0;
    int result = 0;
    int capa_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (batt_capacity == NULL) {
        MAX17050_ERRLOG("[%s] battery_capacity pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    for (i = 0; i < 3; i++) {
        result = max17050_read_reg(the_chip->client, MAX17050_SOCVF_ADDR);
        if (result < 0) {
            MAX17050_ERRLOG("[%s]get soc error:%d\n", __func__, result);
        } else {
            break;
        }
    }
    
    if (result < 0) {
        goto max17050_err;
    }
    
    capa_work = result;
    capa_work = capa_work >> 8;
    *batt_capacity = capa_work;
    
max17050_err:
    return result;
}
EXPORT_SYMBOL(max17050_get_batt_capacity_vf);

int max17050_get_batt_current(int *batt_current)
{
    int i = 0;
    int result = 0;
    int curr_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (batt_current == NULL) {
        MAX17050_ERRLOG("[%s] batt_current pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    for (i = 0; i < 3; i++) {
        result = max17050_read_reg(the_chip->client, MAX17050_CURRENT_ADDR);
        if (result < 0) {
            MAX17050_ERRLOG("[%s]get current error:%d\n", __func__, result);
        } else {
            break;
        }
    }
    
    if (result < 0) {
        goto max17050_err;
    }
    
    curr_work = result;
    *batt_current = curr_work;
    
max17050_err:
    return result;
}

int max17050_get_batt_average_current(int *batt_average)
{
    int i = 0;
    int result = 0;
    int ave_work = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (batt_average == NULL) {
        MAX17050_ERRLOG("[%s] batt_average pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    for (i = 0; i < 3; i++) {
        result = max17050_read_reg(the_chip->client, MAX17050_AVERAGECURRENT_ADDR);
        if (result < 0) {
            MAX17050_ERRLOG("[%s]get current error:%d\n", __func__, result);
        } else {
            break;
        }
    }
    
    if (result < 0) {
        goto max17050_err;
    }
    
    ave_work = result;
    *batt_average = ave_work;
    
max17050_err:
    return result;
}

int max17050_get_batt_age(int *batt_age)
{
    int result = 0;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (batt_age == NULL) {
        MAX17050_ERRLOG("[%s] batt_age pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    *batt_age = the_learning_data.td_age >> 8;
    
max17050_err:
    return result;
}

int max17050_set_batt_fullsocthr(unsigned int batt_fullsocthr)
{
    int result = 0;
    unsigned int fullsocthr_work;
    
    MAX17050_DBGLOG("[%s] in\n", __func__);
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] the_chip pointer is NULL\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    if (batt_fullsocthr > 100) {
        MAX17050_ERRLOG("[%s] fullsocthr error!!!\n", __func__);
        result = -1;
        goto max17050_err;
    }
    
    fullsocthr_work = batt_fullsocthr << 8;
    result = max17050_write_reg(the_chip->client, MAX17050_FULLSOCTHR_ADDR, fullsocthr_work);
    if (result < 0) {
        MAX17050_ERRLOG("[%s] i2c write err : result = %d \n", __func__, result);
        goto max17050_err;
    }
    
max17050_err:
    return result;
}

int max17050_drv_initialized(void)
{
    MAX17050_DBGLOG("[%s] in\n", __func__);
    return max17050_initialized;
}
EXPORT_SYMBOL(max17050_drv_initialized);

int max17050_set_callback_info(struct msm_battery_callback_info *info)
{
    int result = 0;
    
    if (info->callback == NULL) {
        MAX17050_ERRLOG("[%s] set callback function NULL pointar\n",  __func__);
        result = -1;
    } else if (set_callback_function_flag == 1) {
        MAX17050_ERRLOG("[%s] already callback function\n",  __func__);
        result = -1;
    } else {
        msm_touch_callback.callback = info->callback;
        msm_touch_callback.data     = info->data;
        set_callback_function_flag = 1;
        result = 0;
    }
    
    return result;
}

int max17050_unset_callback_info(void)
{
    int result = 0;
    
    if (the_chip == NULL) {
        MAX17050_ERRLOG("[%s] chip pointer is NULL\n", __func__);
        result = -1;
    } else {
        msm_touch_callback.callback = NULL;
        msm_touch_callback.data     = NULL;
        set_callback_function_flag = 0;
    }
    
    return result;
}

extern int charging_mode;
static int __devinit max17050_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17050_chip *chip;
	int ret;

	MAX17050_DBGLOG("[%s] in\n", __func__);

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
	chip->battery.get_property		= max17050_get_property;
	chip->battery.properties		= max17050_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17050_battery_props);

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
	
	INIT_DELAYED_WORK(&chip->battery_monitor, max17050_battery_monitor);
	INIT_DELAYED_WORK_DEFERRABLE(&chip->battery_present_monitor, max17050_battery_present_monitor);
	INIT_DELAYED_WORK_DEFERRABLE(&chip->mt_monitor, max17050_mobile_temp_monitor);
	
	wake_lock_init(&max17050_chg_status, WAKE_LOCK_SUSPEND, "max17050_chg_status");
	
	mutex_init(&chip->lock);
	max17050_clear_queue_state();
	
	/* init wait queue */
	init_waitqueue_head(&chip->wq);
    
    the_chip = chip;
    
	/* kthread start */
	chip->thread = kthread_run(max17050_thread, chip, "max17050");
	
	return 0;
}

static int __devexit max17050_remove(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->battery_monitor);
	cancel_delayed_work(&chip->battery_present_monitor);
	cancel_delayed_work(&chip->mt_monitor);
	power_supply_unregister(&chip->battery);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM
static int max17050_suspend(struct device *dev)
{
	struct max17050_chip *chip = dev_get_drvdata(dev);

	MAX17050_DBGLOG("[%s] in\n", __func__);
    set_bit(MAX17050_EVENT_SUSPEND, &chip->event_state);

	cancel_delayed_work(&chip->battery_monitor);
	cancel_delayed_work(&chip->battery_present_monitor);
	cancel_delayed_work(&chip->mt_monitor);
	return 0;
}

static int max17050_resume(struct device *dev)
{
	struct max17050_chip *chip = dev_get_drvdata(dev);
	int battery_capacity = 0;
    int capacity_update_flag = 0;

	MAX17050_DBGLOG("[%s] in\n", __func__);
	max17050_get_battery_capacity(chip, &battery_capacity);
	if (battery_capacity < chip->soc) {
		chip->soc = battery_capacity;
        capacity_update_flag++;
	}

	max17050_set_battery_capacity(chip, battery_capacity);
    max17050_set_battery_capacity_threshold(chip, battery_capacity);

    if (capacity_update_flag != 0) {
        power_supply_changed(&chip->battery);
    }

	max17050_clear_queue_state();
	clear_bit(MAX17050_EVENT_SUSPEND, &chip->event_state);

	schedule_delayed_work(&chip->battery_monitor, MAX17050_MONITOR_DELAY_500MS);
	schedule_delayed_work(&chip->battery_present_monitor, MAX17050_MONITOR_DELAY_500MS);
	schedule_delayed_work(&chip->mt_monitor, MAX17050_MONITOR_DELAY_500MS);
	return 0;
}

static const struct dev_pm_ops max17050_pm_ops = {
	.suspend = max17050_suspend,
	.resume = max17050_resume,
};

#else  /* CONFIG_PM */

#define max17050_suspend NULL
#define max17050_resume NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id max17050_id[] = {
	{ "max17050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17050_id);

static struct i2c_driver max17050_i2c_driver = {
	.driver	= {
		.name	= "max17050",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &max17050_pm_ops,
#endif /* CONFIG_PM */
	},
	.probe		= max17050_probe,
	.remove		= __devexit_p(max17050_remove),
#ifndef CONFIG_PM
	.suspend	= max17050_suspend,
	.resume		= max17050_resume,
#endif /* !CONFIG_PM */
	.id_table	= max17050_id,
};

static int __init max17050_init(void)
{
    int work = 0;
    if (max17050_check_fgic_type() == 0) {
        MAX17050_INFOLOG("[%s] MAX17050 is not implemented.\n", __func__);
        return -1;
    }
    MAX17050_DBGLOG("[%s] in\n", __func__);
    work = i2c_add_driver(&max17050_i2c_driver);
    MAX17050_DBGLOG("[%s] i2c_add_driver out work = %d\n", __func__, work);
    return work;
}
module_init(max17050_init);

static void __exit max17050_exit(void)
{
    i2c_del_driver(&max17050_i2c_driver);
}
module_exit(max17050_exit);

MODULE_AUTHOR("Fujitsu");
MODULE_DESCRIPTION("MAX17050 Fuel Gauge");
MODULE_LICENSE("GPL");
