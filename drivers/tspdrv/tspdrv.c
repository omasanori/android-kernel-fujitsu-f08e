/*
  * Copyright(C) 2013 FUJITSU LIMITED
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
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <../board-8064.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/delay.h>

#include "tspdrv.h"
#include "tspdrv_i2c.h"

//#define TSPDRV_LOG_DEBUG

#if ((LINUX_VERSION_CODE & 0xFFFF00) < KERNEL_VERSION(2,6,0))
#error Unsupported Kernel version
#endif

#define LOGE(fmt, args...)        printk(KERN_ERR "[tspdrv]: " fmt, ##args)
#define LOGI(fmt, args...)        printk(KERN_INFO "[tspdrv]: " fmt, ##args)

#ifdef TSPDRV_LOG_DEBUG
#define LOGD(fmt, args...)        printk(KERN_DEBUG "[tspdrv]: " fmt, ##args)
#else
#define LOGD(fmt, args...)        do {} while (0)
#endif

#define TSPDRV_HAPTICS_VALID            1      // valid
#define TSPDRV_HAPTICS_INVALID          0      // invalid

/* Device name and version information */
#define TSPDRV_VERSION_STR_LEN          16     // account extra space for future extra digits in version number
#define TSPDRV_NUM_ACTUATORS            1
#define TSPDRV_MAX_DEVICE_NAME_LENGTH   64
static char g_szDeviceName[ (TSPDRV_MAX_DEVICE_NAME_LENGTH
                           + TSPDRV_VERSION_STR_LEN)
                           * TSPDRV_NUM_ACTUATORS];   //initialized in init_module

/* define the AMP ID */
#define TSPDRV_AMP_ENABLE_VALUE         1
#define TSPDRV_AMP_DISBALE_VALUE        2

/* define the Haptics ID */
#define TSPDRV_HAPTICS1_VALUE           3
#define TSPDRV_HAPTICS2_VALUE           4

/* define the theme ID */
#define TSPDRV_HAPTIC_THEME_STANDARD    0
#define TSPDRV_HAPTIC_THEME_STRONG      1
#define TSPDRV_HAPTIC_THEME_MILD        2

/* define the goup attribute */
#define TSPDRV_GROUP_ATTR_175           0x00
#define TSPDRV_GROUP_ATTR_172           0x01
#define TSPDRV_GROUP_ATTR_178           0x02

/* define the in(waveID) ID */
#define TSPDRV_HAP_IN_PRESS_STD         0x10
#define TSPDRV_HAP_IN_RELEASE_STD       0x11
#define TSPDRV_HAP_IN_PRESS_STRG        0x20
#define TSPDRV_HAP_IN_RELEASE_STRG      0x21
#define TSPDRV_CAL_IN_PRESS_STD         0x30
#define TSPDRV_CAL_IN_PRESS_STRG        0x31
#define TSPDRV_CAL_IN_RELEASE_STD       0x32
#define TSPDRV_CAL_IN_RELEASE_STRG      0x33

/* define the MAX freq */
#define TSPDRV_MAX_FREQ_VAL             0x28

/* define the I2C CMD for SET_THEME_H */
#define TSPDRV_I2C_THEME_CMD_USIZE      0x00
#define TSPDRV_I2C_THEME_CMD_DSIZE      0x05
#define TSPDRV_I2C_THEME_CMD            0x10
#define TSPDRV_I2C_THEME_STD            0x00    // 175 MID
#define TSPDRV_I2C_THEME_STRG           0x01    // 175 STRONG
#define TSPDRV_I2C_THEME_MILD           0x02    // 175 MILD
#define TSPDRV_I2C_THEME_STD2           0x04    // 172 MID
#define TSPDRV_I2C_THEME_STRG2          0x05    // 172 STRONG
#define TSPDRV_I2C_THEME_STD8           0x08    // 178 MID
#define TSPDRV_I2C_THEME_STRG8          0x09    // 178 STRONG

/* define the I2C CMD for GET_CAL */
#define TSPDRV_I2C_CAL_CMD_USIZE        0x00
#define TSPDRV_I2C_CAL_CMD_DSIZE        0x04
#define TSPDRV_I2C_CAL_CMD              0x17

/* define the I2C CMD for SET_FSTAT */
#define TSPDRV_I2C_MODE_CMD_USIZE       0x00
#define TSPDRV_I2C_MODE_CMD_DSIZE       0x05
#define TSPDRV_I2C_MODE_CMD             0x18
#define TSPDRV_I2C_MODE_NOR             0x00    // normal mode
#define TSPDRV_I2C_MODE_FST             0x01    // fast mode

/* define the I2C CMD for GET_VER */
#define TSPDRV_GET_VER_CMD_USIZE        0x00
#define TSPDRV_GET_VER_CMD_DSIZE        0x04
#define TSPDRV_GET_VER_CMD              0x14

/* for I2C */
struct _write_req {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   command;
    int8_t   data;
    int8_t   sum;
}__attribute__((packed));

/* i2c command structure */
struct _command_req {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   command;
    int8_t   sum;
}__attribute__((packed));

/* i2c command structure for GET_CAL */
struct _get_cal_transfer {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   cmd;
    int8_t   group_attr;
    int8_t   sum;
}__attribute__((packed));

/* i2c command structure for GET_VER */
struct _firm_transfer {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   cmd;
    int8_t   uver;
    int8_t   dver;
    int8_t   sum;
}__attribute__((packed));

/* i2c command structure for GET_STATUS */
struct _status {
    int8_t   ubytes;
    int8_t   dbytes;
    int8_t   type;
    int8_t   status;
    int8_t   sum;
}__attribute__((packed));

#define TSPDRV_GPIO_HIGH_PERIOD         10      // 10ms
#define TSPDRV_ADJUST_TIMER             4       // 4ms
#define TSPDRV_WAIT_TIME                1       // 1ms  

#define TSPDRV_WORKQUEUE_SIZE           7       // regist type array size

/* Define global variable in a struct */
struct tspdrv {
#ifdef IMPLEMENT_AS_CHAR_DRIVER
    int nMajor = 0;
#endif
    int nHapticsValidFlag;
    size_t cchDeviceName;
    struct early_suspend    early_suspend;

    /* tspdrv run in a thread. */
    struct workqueue_struct *pHaptics_Wq;       // haptics thread
    struct work_struct sHaptics_Work[TSPDRV_WORKQUEUE_SIZE];    // haptics work queue1

    char cRegist_Type[TSPDRV_WORKQUEUE_SIZE];   // regist type buffer
    unsigned int nRegist_Position;              // regist position
    unsigned int nHandle_Position;              // handle position
    struct mutex data_lock;                     // mutex
    int nCurrentTheme;                          // Current Theme
    enum tspdrv_power_mode nCurrentPowerMode;   // Current Power Mode
    int nResumeChangeThemeFlag;                 // When resume, theme change

    long long oldTime;
    long long curTime;
};

static struct tspdrv gHap;                      // tspdrv struct

/* File IO */
static int tspdrv_open(struct inode *inode, struct file *file);
static int tspdrv_close(struct inode *inode, struct file *file);
static ssize_t tspdrv_read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t tspdrv_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
static long tspdrv_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int tspdrv_get_firmware_version(int8_t *major, int8_t *minor);
static int tspdrv_change_haptic_theme(int theme, char cGroupId);

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .read           = tspdrv_read,
    .write          = tspdrv_write,
    .unlocked_ioctl = tspdrv_ioctl,
    .open           = tspdrv_open,
    .release        = tspdrv_close
};

#ifndef IMPLEMENT_AS_CHAR_DRIVER
static struct miscdevice miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = TSPDRV_MODULE_NAME,
    .fops  = &fops
};
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tspdrv_early_suspend(struct early_suspend *h);
static void tspdrv_late_resume(struct early_suspend *h);
#else
static int suspend(struct platform_device *pdev, pm_message_t state);
static int resume(struct platform_device *pdev);
#endif

static struct platform_driver platdrv = {
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = tspdrv_suspend,
    .resume  = tspdrv_resume,
#endif
    .driver = {
        .name = TSPDRV_MODULE_NAME,
    },
};

static void tspdrv_platform_release(struct device *dev);
static struct platform_device platdev = {
    .name =     TSPDRV_MODULE_NAME,
    .id   =       -1,                       // means that there is only one device
    .dev  = {
        .platform_data = NULL,
        .release = tspdrv_platform_release, // a warning is thrown during rmmod if this is absent
    },
};

/*===========================================================================
    FUNCTION  TSPDRV_CHANGE_AMP_MODE
===========================================================================*/
static int tspdrv_change_amp_mode(int mode)
{
    struct tspdrb_i2c_cmd_type sHaptics_I2c_Data;   /* for I2C transfer */
    struct _write_req sI2c_Write_Req;               /* for I2C write command data */
    struct _status  status;                         /* for I2C read status */

    /* setting write command parameter */
    sI2c_Write_Req.ubytes   = TSPDRV_I2C_MODE_CMD_USIZE;
    sI2c_Write_Req.dbytes   = TSPDRV_I2C_MODE_CMD_DSIZE;
    sI2c_Write_Req.command  = TSPDRV_I2C_MODE_CMD;
    sI2c_Write_Req.data     = mode;
    sI2c_Write_Req.sum      = sI2c_Write_Req.ubytes + sI2c_Write_Req.dbytes + sI2c_Write_Req.command + sI2c_Write_Req.data;

    sHaptics_I2c_Data.pwdata    = (unsigned char*)&sI2c_Write_Req;
    sHaptics_I2c_Data.wlen      = sizeof(struct _write_req);
    sHaptics_I2c_Data.prdata    = (unsigned char*)&status;
    sHaptics_I2c_Data.rlen      = sizeof(struct _status);

    if (tspdrv_i2c_read_one_transaction(&sHaptics_I2c_Data) != TSPDRV_RET_OK
        || status.status != 0) {
        LOGE("%s: tspdrv_i2c_read_one_transaction() AMP_MODE_%s error, status=0x%02X, Regist_No=%d, Handle_No=%d\n",
            __func__, ((mode == TSPDRV_I2C_MODE_NOR) ? "NORMAL" : "FAST"),
            status.status, gHap.nRegist_Position, gHap.nHandle_Position);
        return TSPDRV_RET_NG;
    }

    /* AMP ENABLE / DISABLE */
    LOGD("%s: AMP_MODE_%s is set. Regist_No=%d, Handle_No=%d\n", __func__,
        ((mode == TSPDRV_I2C_MODE_NOR) ? "NORMAL" : "FAST"),
        gHap.nRegist_Position, gHap.nHandle_Position);

    return TSPDRV_RET_OK;
}

/*===========================================================================
    FUNCTION  TSPDRV_HAPTIC_GPIO
===========================================================================*/
static void tspdrv_haptic_gpio(int nGpio_Num, int nHaptics_Value)
{
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(nGpio_Num), TSPDRV_GPIO_LEVEL_HIGH);
    udelay(TSPDRV_DELAY_US_TIME);
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(nGpio_Num), TSPDRV_GPIO_LEVEL_LOW);
    udelay(TSPDRV_DELAY_US_TIME);

    /* ON HAPTICS1/2(PRESS/RELEASE) */
    LOGD("%s: HAPTICS_%s is set. Regist_No=%d, Handle_No=%d\n", __func__,
        (nHaptics_Value == TSPDRV_HAPTICS1_VALUE ? "PRESS" : "RELEASE"),
        gHap.nRegist_Position, gHap.nHandle_Position);
}

/*===========================================================================
    FUNCTION  TSPDRV_WORKQUEUE_FUNC
===========================================================================*/
static void tspdrv_workqueue_func(struct work_struct *sHaptics_Work)
{
    int nGpio_Num = TSPDRV_GPIO_HAPTICS_PRESS;
    int nAmp_Mode = TSPDRV_I2C_MODE_NOR;
    int nCommand_Value = TSPDRV_HAPTICS1_VALUE;

    LOGD("%s: START\n", __func__);

    if (gHap.nHandle_Position < gHap.nRegist_Position) {
        nCommand_Value = gHap.cRegist_Type[gHap.nHandle_Position % TSPDRV_WORKQUEUE_SIZE];
        gHap.nHandle_Position++;
        switch (nCommand_Value) {
        case TSPDRV_AMP_ENABLE_VALUE:
        case TSPDRV_AMP_DISBALE_VALUE:
            if (gHap.oldTime >= 0) {
                long waitTime = 0;
                gHap.curTime = cpu_clock(UINT_MAX);
                do_div(gHap.curTime, 1000000);
                waitTime = TSPDRV_WAIT_TIME - ((gHap.curTime - gHap.oldTime));
                if (0 < waitTime && waitTime <= TSPDRV_WAIT_TIME ) {
                    msleep(waitTime);
                }
            }
            nAmp_Mode = (nCommand_Value == TSPDRV_AMP_ENABLE_VALUE) ? TSPDRV_I2C_MODE_FST : TSPDRV_I2C_MODE_NOR;
            tspdrv_change_amp_mode(nAmp_Mode);
            gHap.oldTime = cpu_clock(UINT_MAX);
            do_div(gHap.oldTime, 1000000);
            break;

        case TSPDRV_HAPTICS1_VALUE:
        case TSPDRV_HAPTICS2_VALUE:
            nGpio_Num = (nCommand_Value == TSPDRV_HAPTICS1_VALUE) ?
                        TSPDRV_GPIO_HAPTICS_PRESS : TSPDRV_GPIO_HAPTICS_RELEASE;
            tspdrv_haptic_gpio(nGpio_Num, nCommand_Value);
            break;
        default:
            LOGE("%s: invalid command value, value=%d\n", __func__, nCommand_Value);
        }
    }

    LOGD("%s: OK END\n", __func__);
    return;
}

/*===========================================================================
    FUNCTION  TSPDRV_INIT
===========================================================================*/
static int __init tspdrv_init(void)
{
    int nRet = TSPDRV_RET_OK ;              // initialize this return value.
    int8_t cMajor = 0, cMinor = 0;
    int i = 0;

    LOGI("%s: START\n", __func__);

    /*initial */
    gHap.nHapticsValidFlag = TSPDRV_HAPTICS_VALID;    // haptics valid
    gHap.nRegist_Position = 0;
    gHap.nHandle_Position = 0;
    memset(gHap.cRegist_Type, 0x00, sizeof(gHap.cRegist_Type));
    gHap.nCurrentTheme = TSPDRV_HAPTIC_THEME_STANDARD;
    gHap.nCurrentPowerMode = NORMAL_MODE;
    gHap.nResumeChangeThemeFlag = 0;
    gHap.oldTime = -1;
    gHap.curTime = -1;

    // mutex init
    mutex_init(&gHap.data_lock);

    /* make a haptics thread */
    gHap.pHaptics_Wq = create_singlethread_workqueue("tspdrv_queue");
    if (!gHap.pHaptics_Wq) {
        LOGE("%s: create_singlethread_workqueue() error\n", __func__);
        goto register_err;
    }

    /* tspdrv thread valid */
    for (i = 0; i < TSPDRV_WORKQUEUE_SIZE; i++) {
        INIT_WORK(&(gHap.sHaptics_Work[i]), tspdrv_workqueue_func);
    }

    nRet = misc_register(&miscdev);
    if (nRet) {
        LOGE("%s: misc_register() error\n", __func__);
        goto register_err;
    }

    nRet = platform_device_register(&platdev);
    if (nRet) {
        LOGE("%s: platform_device_register() error\n", __func__);
        goto device_register_err;
    }

    nRet = platform_driver_register(&platdrv);
    if (nRet) {
        LOGE("%s: platform_driver_register() error\n", __func__);
        goto driver_register_err;
    }

    /* i2c initialize */
    nRet = tspdrv_i2c_initialize();
    if (nRet) {
        LOGE("%s: tspdrv_i2c_initialize() error\n", __func__);
        goto i2c_init_err;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    gHap.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 5;
    gHap.early_suspend.suspend = tspdrv_early_suspend;
    gHap.early_suspend.resume = tspdrv_late_resume;
    register_early_suspend(&gHap.early_suspend);
#endif

    /* FW version  */
    if (tspdrv_get_firmware_version(&cMajor, &cMinor) == TSPDRV_RET_OK) {
        LOGI("%s: FW version [%02X%02X]\n", __func__, cMajor, cMinor);
    } else {
        LOGE("%s: tspdrv_get_firmware_version() error\n", __func__);
    }
    LOGI("%s: OK END\n", __func__);
    return TSPDRV_RET_OK;

i2c_init_err:
    platform_driver_unregister(&platdrv);

driver_register_err:
    platform_device_unregister(&platdev);

device_register_err:
#ifdef IMPLEMENT_AS_CHAR_DRIVER
    unregister_chrdev(0, TSPDRV_MODULE_NAME);
#else
    misc_deregister(&miscdev);
#endif

register_err:
    mutex_destroy(&gHap.data_lock);
    if (gHap.pHaptics_Wq)
        destroy_workqueue(gHap.pHaptics_Wq);

    LOGI("%s: NG END\n", __func__);
    return TSPDRV_RET_NG;
}

/*===========================================================================
    FUNCTION  TSPDRV_EXIT
===========================================================================*/
static void __exit tspdrv_exit(void)
{
    int nRet_Val=0;
    LOGI("%s: START\n", __func__);

    /* i2c terminate */
    nRet_Val = tspdrv_i2c_terminate();

    platform_driver_unregister(&platdrv);
    platform_device_unregister(&platdev);

#ifdef IMPLEMENT_AS_CHAR_DRIVER
    unregister_chrdev(nMajor, TSPDRV_MODULE_NAME);
#else
    misc_deregister(&miscdev);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&gHap.early_suspend);
#endif

    mutex_destroy(&gHap.data_lock);

    /* delete thread */
    if (gHap.pHaptics_Wq)
        destroy_workqueue(gHap.pHaptics_Wq);

    LOGI("%s: OK END\n", __func__);
}

/*===========================================================================
    FUNCTION  TSPDRV_OPEN
===========================================================================*/
static int tspdrv_open(struct inode *inode, struct file *file)
{
    if (!try_module_get(THIS_MODULE)) {
        LOGE("%s: try_module_get() error\n", __func__);
        return -ENODEV;
    }
    return TSPDRV_RET_OK;
}

/*===========================================================================
    FUNCTION  TSPDRV_CLOSE
===========================================================================*/
static int tspdrv_close(struct inode *inode, struct file *file) 
{
    /*
    ** Clear the variable used to store the magic number to prevent 
    ** unauthorized caller to write data. Tspdrv service is the only 
    ** valid caller.
    */
    file->private_data = (void*)NULL;

    module_put(THIS_MODULE);
    return TSPDRV_RET_OK;
}

/*===========================================================================
    FUNCTION  TSPDRV_READ
===========================================================================*/
static ssize_t tspdrv_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    const size_t nBufSize = (gHap.cchDeviceName > (size_t)(*ppos)) ? min(count, gHap.cchDeviceName - (size_t)(*ppos)) : 0;

    LOGD("%s: START\n", __func__);
    /* End of buffer, exit */
    if (TSPDRV_RET_OK == nBufSize) return 0;

    if (TSPDRV_RET_OK != copy_to_user(buf, g_szDeviceName + (*ppos), nBufSize)) {
        /* Failed to copy all the data, exit */
        LOGE("%s: copy_to_user() error\n", __func__);
        return TSPDRV_RET_OK;
    }

    /* Update file position and return copied buffer size */
    *ppos += nBufSize;
    
    LOGD("%s: OK END\n", __func__);
    return nBufSize;
}

/*===========================================================================
    FUNCTION  TSPDRV_WRITE
===========================================================================*/
static ssize_t tspdrv_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    *ppos = 0;  // file position not used, always set to 0

    LOGD("%s: START\n", __func__);
    /*
    ** Prevent unauthorized caller to write data. 
    ** Tspdrv service is the only valid caller.
    */
    if (file->private_data != (void*)TSPDRV_MAGIC_NUMBER) {
        LOGE("%s: unauthorized write\n", __func__);
        return 0;
    }

    LOGD("%s: OK END\n", __func__);
    return TSPDRV_RET_OK;
}

/*===========================================================================
    FUNCTION  TSPDRV_SUSPEND
===========================================================================*/
static int tspdrv_suspend(struct platform_device *pdev, pm_message_t state)
{
    int i = 0;

    LOGI("%s: START\n", __func__);

    // haptics invalid
    gHap.nHapticsValidFlag = TSPDRV_HAPTICS_INVALID;  // haptics invalid

    /* initial */
    gHap.nRegist_Position = 0;
    gHap.nHandle_Position = 0;
    gHap.oldTime = -1;
    gHap.curTime = -1;
    memset(gHap.cRegist_Type, 0x00, sizeof(gHap.cRegist_Type));

    /* cancel work queue */
    for (i = 0; i < TSPDRV_WORKQUEUE_SIZE; i++) {
        cancel_work_sync(&(gHap.sHaptics_Work[i]));
    }

    LOGI("%s: OK END\n", __func__);
    return TSPDRV_RET_OK;
}

/*===========================================================================
    FUNCTION  TSPDRV_RESUME
===========================================================================*/
static int tspdrv_resume(struct platform_device *pdev)
{
    LOGI("%s: START\n", __func__);

    // Theme Change
    mutex_lock(&gHap.data_lock);
    if (gHap.nResumeChangeThemeFlag) {
        gHap.nResumeChangeThemeFlag = 0;
        tspdrv_change_haptic_theme((gHap.nCurrentPowerMode == LOW_MODE ? TSPDRV_HAPTIC_THEME_MILD : gHap.nCurrentTheme), TSPDRV_GROUP_ATTR_175);
    }
    mutex_unlock(&gHap.data_lock);

    // Force AMP Disable
    tspdrv_change_amp_mode(TSPDRV_I2C_MODE_NOR);

    // haptics valid
    gHap.nHapticsValidFlag = TSPDRV_HAPTICS_VALID;    // haptics valid

    /* initial */
    gHap.nRegist_Position = 0;
    gHap.nHandle_Position = 0;
    gHap.oldTime = -1;
    gHap.curTime = -1;
    memset(gHap.cRegist_Type, 0x00, sizeof(gHap.cRegist_Type));

    LOGI("%s: OK END\n", __func__);
    return TSPDRV_RET_OK;

}

#ifdef CONFIG_HAS_EARLYSUSPEND
/*===========================================================================
    FUNCTION  TSPDRV_EARLY_SUSPEND
===========================================================================*/
static void tspdrv_early_suspend(struct early_suspend *h)
{
    tspdrv_suspend(&platdev, PMSG_SUSPEND);
}

/*===========================================================================
    FUNCTION  TSPDRV_LATE_RESUME
===========================================================================*/
static void tspdrv_late_resume(struct early_suspend *h)
{
    tspdrv_resume(&platdev);
}
#endif

/*===========================================================================
    FUNCTION  TSPDRV_PLATFORM_RELEASE
===========================================================================*/
static void tspdrv_platform_release(struct device *dev)
{
    return;
}

/*===========================================================================
    FUNCTION  TSPDRV_CHANGE_HAPTIC_THEME
===========================================================================*/
static int tspdrv_change_haptic_theme(int theme, char cGroupId)
{
    struct tspdrb_i2c_cmd_type sHaptics_I2c_Data;   /* for I2C transfer */
    struct _write_req sI2c_Write_Req;               /* for I2C write command data */
    struct _status  status;                         /* for I2C read status */

    LOGD("%s: START\n", __func__);
    /* setting write command parameter */
    sI2c_Write_Req.ubytes   = TSPDRV_I2C_THEME_CMD_USIZE;
    sI2c_Write_Req.dbytes   = TSPDRV_I2C_THEME_CMD_DSIZE;
    sI2c_Write_Req.command  = TSPDRV_I2C_THEME_CMD;

    // branch from the parameter
    switch (cGroupId) {
        case TSPDRV_GROUP_ATTR_175:     // 175 group
            sI2c_Write_Req.data = theme;
            break;
        case TSPDRV_GROUP_ATTR_172:     // 172 group
            sI2c_Write_Req.data = (theme == TSPDRV_HAPTIC_THEME_STANDARD) ? TSPDRV_I2C_THEME_STD2 : TSPDRV_I2C_THEME_STRG2;
            break;
        case TSPDRV_GROUP_ATTR_178:     // 178 group
            sI2c_Write_Req.data = (theme == TSPDRV_HAPTIC_THEME_STANDARD) ? TSPDRV_I2C_THEME_STD8 : TSPDRV_I2C_THEME_STRG8;
            break;
        default:
            LOGE("%s: parameter error\n", __func__);
            return TSPDRV_RET_NG;
    }
    sI2c_Write_Req.sum = sI2c_Write_Req.ubytes + sI2c_Write_Req.dbytes + sI2c_Write_Req.command + sI2c_Write_Req.data;

    sHaptics_I2c_Data.pwdata    = (unsigned char*)&sI2c_Write_Req;
    sHaptics_I2c_Data.wlen      = sizeof(struct _write_req);
    sHaptics_I2c_Data.prdata    = (unsigned char*)&status;
    sHaptics_I2c_Data.rlen      = sizeof(struct _status);

    if (tspdrv_i2c_read_one_transaction(&sHaptics_I2c_Data) != TSPDRV_RET_OK
        || status.status != 0) {
        LOGE("%s: tspdrv_i2c_read_one_transaction() error, status=0x%02X, theme=0x%02X\n",
            __func__, status.status, sI2c_Write_Req.data);
        return TSPDRV_RET_NG;
    }
    LOGI("%s: change theme ok, theme=0x%02X\n", __func__, sI2c_Write_Req.data);

    LOGD("%s: OK END\n", __func__);
    return TSPDRV_RET_OK;
}

/*===========================================================================
    FUNCTION  TSPDRV_GET_CAL_INFO
===========================================================================*/
static int tspdrv_get_cal_info(char *cGroupId)
{
    struct tspdrb_i2c_cmd_type sHaptics_I2c_Data;   /* for I2C transfer */
    struct _command_req sI2c_Read_Req;              /* for I2C read command data */
    struct _get_cal_transfer sI2c_Get_Cal;          /* for I2C get data */

    LOGI("%s: START\n", __func__);
    *cGroupId = TSPDRV_GROUP_ATTR_175;
    /* setting write command parameter */
    sI2c_Read_Req.ubytes   = TSPDRV_I2C_CAL_CMD_USIZE;
    sI2c_Read_Req.dbytes   = TSPDRV_I2C_CAL_CMD_DSIZE;
    sI2c_Read_Req.command  = TSPDRV_I2C_CAL_CMD;
    sI2c_Read_Req.sum      = sI2c_Read_Req.ubytes + sI2c_Read_Req.dbytes + sI2c_Read_Req.command;
    sHaptics_I2c_Data.pwdata       = (unsigned char*)&sI2c_Read_Req;
    sHaptics_I2c_Data.wlen         = sizeof(struct _command_req);
    sHaptics_I2c_Data.prdata       = (unsigned char*)&sI2c_Get_Cal;
    sHaptics_I2c_Data.rlen         = sizeof(struct _get_cal_transfer);

    /* data transfer */
    if (tspdrv_i2c_read_one_transaction(&sHaptics_I2c_Data) != TSPDRV_RET_OK) {
        LOGE("%s: tspdrv_i2c_read_one_transaction() error\n", __func__);
        return TSPDRV_RET_NG;
    }
    *cGroupId =  sI2c_Get_Cal.group_attr;

    LOGI("%s: OK END, GROUP_ID=0x%02X\n", __func__, *cGroupId);
    return TSPDRV_RET_OK;
}

/*===========================================================================
    FUNCTION  TSPDRV_GET_FIRMWARE_VERSION
===========================================================================*/
static int tspdrv_get_firmware_version(int8_t *major, int8_t *minor)
{
    int    nRet = TSPDRV_RET_OK;
    struct tspdrb_i2c_cmd_type sI2c_Cmd;
    struct _command_req sGet_Ver_Req;
    struct _firm_transfer sGet_Ver_Ans;

    /* command transfer(set data) */
    sGet_Ver_Req.ubytes  = TSPDRV_GET_VER_CMD_USIZE;
    sGet_Ver_Req.dbytes  = TSPDRV_GET_VER_CMD_DSIZE;
    sGet_Ver_Req.command = TSPDRV_GET_VER_CMD;
    sGet_Ver_Req.sum     = sGet_Ver_Req.ubytes + sGet_Ver_Req.dbytes + sGet_Ver_Req.command;
    sI2c_Cmd.pwdata      = (unsigned char*)&sGet_Ver_Req;
    sI2c_Cmd.wlen        = sizeof(struct _command_req);
    sI2c_Cmd.prdata      = (unsigned char*)&sGet_Ver_Ans;
    sI2c_Cmd.rlen        = sizeof(struct _firm_transfer);

    /* data transfer */
    nRet = tspdrv_i2c_read_one_transaction(&sI2c_Cmd);
    if (nRet != TSPDRV_RET_OK) {
        LOGE("%s: tspdrv_i2c_read_one_transaction() error\n", __func__);
        nRet = TSPDRV_RET_NG;
    } else {
        *major = sGet_Ver_Ans.uver;
        *minor = sGet_Ver_Ans.dver;
    }

    return nRet;
}

/*===========================================================================
    FUNCTION  TSPDRV_IOCTL
===========================================================================*/
static long tspdrv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int nRtn = TSPDRV_RET_OK;   // function call return value
    int nCount = 0;             // nCount--wave number
    int nPeriod = 0;            // period of 1000ms
    int i;                      // i--loop
    int nThemeType = TSPDRV_HAPTIC_THEME_STANDARD;
    int nGpioNo = TSPDRV_GPIO_HAPTICS_PRESS;
    char cGroupId = TSPDRV_GROUP_ATTR_175;
    enum tspdrv_power_mode nPowerMode;

    /* struct to save parameter */
    haptics_io_val_t sHaptics_Io;

    LOGD("%s: START\n", __func__);

    if (gHap.nHapticsValidFlag == TSPDRV_HAPTICS_INVALID && cmd != TSPDRV_POWER_MODE) {
        LOGI("%s: is invalid because of suspend\n", __func__);
        LOGD("%s: OK End\n", __func__);
        return nRtn;
    }

    switch (cmd) {
        /* AMP ENABLED / DISABLE */
        case TSPDRV_AMP_ENABLE:
        case TSPDRV_AMP_DISABLE:
            LOGD("%s: In TSPDRV_AMP_%s\n", __func__, (cmd == TSPDRV_AMP_ENABLE ? "ENABLE" : "DISABLE"));
            i = gHap.nRegist_Position % TSPDRV_WORKQUEUE_SIZE;
            gHap.cRegist_Type[i] =
                    (cmd == TSPDRV_AMP_ENABLE) ? TSPDRV_AMP_ENABLE_VALUE : TSPDRV_AMP_DISBALE_VALUE;
            gHap.nRegist_Position++;
            if (queue_work(gHap.pHaptics_Wq, &(gHap.sHaptics_Work[i])) == 0) {
                LOGE("%s: TSPDRV_AMP_%s queue_work() error", __func__, (cmd == TSPDRV_AMP_ENABLE ? "ENABLE" : "DISABLE"));
                gHap.nRegist_Position--;
            }

            break;

        /* HAPTICS PRESS / RELEASE */
        case TSPDRV_HAPTICS_PRESS:
        case TSPDRV_HAPTICS_RELEASE:
            LOGD("%s: In TSPDRV_HAPTICS_%s\n", __func__, (cmd == TSPDRV_HAPTICS_PRESS ? "PRESS" : "RELEASE"));
            i = gHap.nRegist_Position % TSPDRV_WORKQUEUE_SIZE;
            gHap.cRegist_Type[i] =
                    (cmd == TSPDRV_HAPTICS_PRESS) ? TSPDRV_HAPTICS1_VALUE : TSPDRV_HAPTICS2_VALUE;
            gHap.nRegist_Position++;
            if (queue_work(gHap.pHaptics_Wq, &(gHap.sHaptics_Work[i])) == 0) {
                LOGE("%s: TSPDRV_HAPTICS_%s queue_work() error", __func__, (cmd == TSPDRV_HAPTICS_PRESS ? "PRESS" : "RELEASE"));
                gHap.nRegist_Position--;
            }
            break;

        /* THEME ON STANDAD */
        case TSPDRV_THEME_STANDARD:
            mutex_lock(&gHap.data_lock);
            LOGD("%s: In TSPDRV_THEME_STANDARD\n", __func__);
            if (gHap.nCurrentPowerMode == NORMAL_MODE) {
                nRtn = tspdrv_change_haptic_theme(TSPDRV_HAPTIC_THEME_STANDARD, TSPDRV_GROUP_ATTR_175);
                gHap.oldTime = cpu_clock(UINT_MAX);
                do_div(gHap.oldTime, 1000000);
            }
            if (nRtn == TSPDRV_RET_OK) {
                gHap.nCurrentTheme = TSPDRV_HAPTIC_THEME_STANDARD;
            }
            mutex_unlock(&gHap.data_lock);
            break;

        /* THEME ON STRONG */
        case TSPDRV_THEME_STRONG:
            mutex_lock(&gHap.data_lock);
            LOGD("%s: In TSPDRV_THEME_STRONG\n", __func__);
            if (gHap.nCurrentPowerMode == NORMAL_MODE) {
                nRtn = tspdrv_change_haptic_theme(TSPDRV_HAPTIC_THEME_STRONG, TSPDRV_GROUP_ATTR_175);
                gHap.oldTime = cpu_clock(UINT_MAX);
                do_div(gHap.oldTime, 1000000);
            }
            if (nRtn == TSPDRV_RET_OK) {
                gHap.nCurrentTheme = TSPDRV_HAPTIC_THEME_STRONG;
            }
            mutex_unlock(&gHap.data_lock);
            break;

        /* POWER MODE */
        case TSPDRV_POWER_MODE:
            mutex_lock(&gHap.data_lock);
            LOGD("%s: In TSPDRV_POWER_MODE\n", __func__);

            nRtn = copy_from_user(&nPowerMode, (int __user *)arg, sizeof(int));
            if (nRtn != TSPDRV_RET_OK) {
                LOGE("%s: copy_from_user() error\n", __func__);
                nRtn = -EFAULT;
            } else {
                LOGI("%s: parameter:powermode=%s\n", __func__, (nPowerMode == LOW_MODE ? "LOW" : "NORMAL"));
                if (gHap.nHapticsValidFlag == TSPDRV_HAPTICS_VALID) {
                    nRtn = tspdrv_change_haptic_theme((nPowerMode == LOW_MODE ? TSPDRV_HAPTIC_THEME_MILD : gHap.nCurrentTheme), TSPDRV_GROUP_ATTR_175);
                    gHap.oldTime = cpu_clock(UINT_MAX);
                    do_div(gHap.oldTime, 1000000);
                    if (nRtn == TSPDRV_RET_OK) {
                        gHap.nCurrentPowerMode = nPowerMode;
                    }
                } else {
                    gHap.nCurrentPowerMode = nPowerMode;
                    gHap.nResumeChangeThemeFlag = 1;
                }
            }
            mutex_unlock(&gHap.data_lock);
            break;

        /* Maker Command for Haptics */
        case TSPDRV_MK_CMD_HAPTICS:
            LOGI("%s: In TSPDRV_MK_CMD_HAPTICS\n", __func__);
            /* Get the parameter:in, cycle, freq */
            nRtn = copy_from_user(&sHaptics_Io, (int __user *)arg, sizeof(sHaptics_Io)); 
            if (nRtn != TSPDRV_RET_OK) {
                sHaptics_Io.return_val = TSPDRV_ERR_NOANW;
                LOGE("%s: copy_from_user() error\n", __func__);
                goto mc_error;
            }

            LOGI("%s: parameter:wave-id=0x%02X, cycle_val=%d, freq_val=%d\n",
                    __func__, sHaptics_Io.wave_id, sHaptics_Io.cycle_val, sHaptics_Io.freq_val);

            /* return_val Initial */
            sHaptics_Io.return_val = TSPDRV_RET_OK;

            // parameter check---wave
            switch (sHaptics_Io.wave_id) {
                case TSPDRV_HAP_IN_PRESS_STD:       // wave in 10
                case TSPDRV_HAP_IN_PRESS_STRG:      // wave in 20
                    /* theme */
                    nThemeType = (sHaptics_Io.wave_id == TSPDRV_HAP_IN_PRESS_STD)
                                    ? TSPDRV_HAPTIC_THEME_STANDARD : TSPDRV_HAPTIC_THEME_STRONG;
                    /* Press -- Haptics1 */
                    nGpioNo = TSPDRV_GPIO_HAPTICS_PRESS;
                    /* GroupId */
                    cGroupId = TSPDRV_GROUP_ATTR_175;
                    break;

                case TSPDRV_HAP_IN_RELEASE_STD:     // wave in 11
                case TSPDRV_HAP_IN_RELEASE_STRG:    // wave in 21
                    /* theme */
                    nThemeType = (sHaptics_Io.wave_id == TSPDRV_HAP_IN_RELEASE_STD)
                                    ? TSPDRV_HAPTIC_THEME_STANDARD : TSPDRV_HAPTIC_THEME_STRONG;
                    /* Release -- Haptics2 */
                    nGpioNo = TSPDRV_GPIO_HAPTICS_RELEASE;
                    /* GroupId */
                    cGroupId = TSPDRV_GROUP_ATTR_175;
                    break;

                case TSPDRV_CAL_IN_PRESS_STD:       // wave in 30
                case TSPDRV_CAL_IN_PRESS_STRG:      // wave in 31
                    /* theme */
                    nThemeType = (sHaptics_Io.wave_id == TSPDRV_CAL_IN_PRESS_STD)
                                    ? TSPDRV_HAPTIC_THEME_STANDARD : TSPDRV_HAPTIC_THEME_STRONG;
                    /* Press -- Haptics1 */
                    nGpioNo = TSPDRV_GPIO_HAPTICS_PRESS;
                    /* GroupId */
                    tspdrv_get_cal_info(&cGroupId);
                    break;


                case TSPDRV_CAL_IN_RELEASE_STD:     // wave in 32
                case TSPDRV_CAL_IN_RELEASE_STRG:    // wave in 33
                    /* theme */
                    nThemeType = (sHaptics_Io.wave_id == TSPDRV_CAL_IN_RELEASE_STD)
                                    ? TSPDRV_HAPTIC_THEME_STANDARD : TSPDRV_HAPTIC_THEME_STRONG;
                    /* Press -- Haptics2 */
                    nGpioNo = TSPDRV_GPIO_HAPTICS_RELEASE;
                    /* GroupId */
                    tspdrv_get_cal_info(&cGroupId);
                    break;

                default:
                    sHaptics_Io.return_val = TSPDRV_ERR_NOANW;
                    LOGE("%s: parameter error, wave_id=0x%02X\n", __func__, sHaptics_Io.wave_id);
                    goto mc_error;
            }

            // parameter check---cycle
            if (sHaptics_Io.cycle_val < 0) {
                sHaptics_Io.return_val = TSPDRV_ERR_NOANW;
                LOGE("%s: parameter error, cycle_val=%d\n", __func__, sHaptics_Io.cycle_val);
                goto mc_error;
            }

            // parameter check---freq_val
            if (sHaptics_Io.freq_val > TSPDRV_MAX_FREQ_VAL) {
                sHaptics_Io.return_val = TSPDRV_ERR_OVER;
                LOGE("%s: parameter error, freq_val=%d\n", __func__, sHaptics_Io.freq_val);
                goto mc_error;
            } else if (sHaptics_Io.freq_val < 0) {
                sHaptics_Io.return_val = TSPDRV_ERR_NOANW;
                LOGE("%s: parameter error, freq_val=%d\n", __func__, sHaptics_Io.freq_val);
                goto mc_error;
            }

            // change theme
            if (TSPDRV_RET_NG == tspdrv_change_haptic_theme(nThemeType, cGroupId)) {
                LOGE("%s: change theme error, wave_id=0x%02X\n", __func__, sHaptics_Io.wave_id);
                sHaptics_Io.return_val = TSPDRV_ERR_NOANW;
                goto mc_error;
            };

            nCount = sHaptics_Io.cycle_val;
            if (sHaptics_Io.freq_val != 0) {
                nPeriod = 1000 / sHaptics_Io.freq_val;
            } else {
                /* do not loop */
                nCount = 0;
            }

            for (i = 0; i < nCount; i++) {
                /* start the time */
                LOGD("%s: calculate haptic time start, PIN=%d\n", __func__, nGpioNo);
                gpio_set_value(PM8921_GPIO_PM_TO_SYS(nGpioNo), TSPDRV_GPIO_LEVEL_HIGH);
                mdelay(TSPDRV_GPIO_HIGH_PERIOD); // 10ms delay
                gpio_set_value(PM8921_GPIO_PM_TO_SYS(nGpioNo), TSPDRV_GPIO_LEVEL_LOW);
                /* end the time */
                LOGD("%s: calculate haptic time end\n", __func__);
                mdelay(nPeriod - TSPDRV_GPIO_HIGH_PERIOD - TSPDRV_ADJUST_TIMER); //(nPeriod- 10ms) delay
                LOGD("%s: calculate delay time end\n", __func__);
            }

mc_error:
            /* member of the array mc_cmd from array */
            nRtn = copy_to_user((int __user *)arg, &sHaptics_Io, sizeof(sHaptics_Io));
            if (nRtn != TSPDRV_RET_OK) {
                LOGE("%s: copy_to_user() error\n", __func__);
                sHaptics_Io.return_val = TSPDRV_ERR_NOANW;
            } else {
                nRtn = TSPDRV_RET_OK;
            }
            LOGD("%s: MC COMMAND END, return_val=0x%02X\n", __func__, sHaptics_Io.return_val);
            nRtn = sHaptics_Io.return_val;
            break;

        default:
            LOGE("%s: In DEFAULT(NOT SUPPORT COMMAND)\n", __func__);
            nRtn = -EFAULT;
    }

    LOGD("%s: %s END\n", __func__, (nRtn == TSPDRV_RET_OK ? "OK" : "NG"));
    return nRtn;
}

module_init(tspdrv_init);
module_exit(tspdrv_exit);

/* Module info */
MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("tspdrv device");
MODULE_LICENSE("GPL v2");
