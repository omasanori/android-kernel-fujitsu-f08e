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
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <asm-generic/uaccess.h>
#include <asm/system.h>
#include <../arch/arm/mach-msm/include/mach/gpio.h>
#include <linux/gpio_event.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include "../arch/arm/mach-msm/board-8960.h"
#include <linux/ovp.h>
#include "ovp_driver.h"
#include "../include/linux/mfd/fj_charger.h"
#include "../../../arch/arm/mach-msm/devices.h"
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/sched.h>  /* FUJITSU:2012-09-25 add */
#include <linux/mutex.h>  /* FUJITSU:2012-11-02 add */
#include <linux/miscdevice.h>

#ifndef OVP_LOCAL_BUILD
#include <linux/fta.h>
#endif

/* FUJITSU:2012-06-20 add_start */
#include <linux/leds-pm8xxx.h>
/* FUJITSU:2012-06-20 add_end */

/* FUJITSU:2012-11-15 led early-off add_start */
#include <linux/power_supply.h>
#include <linux/mfd/fj_charger.h>
/* FUJITSU:2012-11-15 led early-off add_end */

/* FUJITSU:2013-04-12 OVP MHL an examination add_start */
#include <mach/board_fj.h> 
/* FUJITSU:2013-04-12 OVP MHL an examination add_end */
 
/*===========================================================================
  MACROS
  ===========================================================================*/
#define DEBUG_OVP_D

#ifdef DEBUG_OVP_D
    #define OVP_DBG(x...)  printk(x)
#else
    #define OVP_DBG(x...)
#endif

#define OVPDRV_LSBM(width)                      ((width) <= 0 ? 0x0u : 8 <= (width) ? 0xffu : 0xffu >> (8 - (width)))
#define OVPDRV_MSKC(wreg,breg)                  (OVPDRV_LSBM(wreg##_##breg##_WIDTH) << wreg##_##breg##_POSI)
#define OVPDRV_ENCC(wreg,breg,bval)             ((wreg##_##breg##_##bval & OVPDRV_LSBM(wreg##_##breg##_WIDTH)) << wreg##_##breg##_POSI)
#define OVPDRV_MSKC_EQ(wreg,breg,bval,reg)      ((reg & OVPDRV_MSKC(wreg,breg)) == OVPDRV_ENCC(wreg,breg,bval))

#define OVPDRV_RDCW(wreg,ret)                   ovp_read(OVP_##wreg,ret)
#define OVPDRV_WRCW(wreg,val)                   ovp_write(OVP_##wreg,val)
#define OVPDRV_UDCW(wreg,mask,val)              ovpdrv_modcw(OVP_##wreg,OVP_##wreg##_RDM,mask,val)

#define OVPDRV_RDCB(wreg,breg,ret)              (OVPDRV_RDCW(wreg,ret) >> wreg##_##breg##_POSI & OVPDRV_LSBM(wreg##_##breg##_WIDTH))
#define OVPDRV_RDCB_EQ(wreg,breg,bval,ret)      ((OVPDRV_RDCW(wreg,ret) >> wreg##_##breg##_POSI & OVPDRV_LSBM(wreg##_##breg##_WIDTH)) == wreg##_##breg##_##bval)
#define OVPDRV_UDCB(wreg,breg,bval)             ovpdrv_modcw(OVP_##wreg,OVP_##wreg##_RDM,OVPDRV_LSBM(wreg##_##breg##_WIDTH) << wreg##_##breg##_POSI, \
                                                                 (wreg##_##breg##_##bval & OVPDRV_LSBM(wreg##_##breg##_WIDTH)) << wreg##_##breg##_POSI)
#ifndef OVP_LOCAL_BUILD
    #define FJ_OVP_REC_LOG(x, y...) {\
        char recbuf[128];\
        snprintf(recbuf, sizeof(recbuf), x, ## y);\
        ftadrv_send_str(recbuf);\
    }
#else
    #define FJ_OVP_REC_LOG(x, y...)
#endif

/* FUJITSU:2012-06-20 add_start */
/**********************************************************************/
/* macros copy from board-8064-pmic.c                                  */
/**********************************************************************/
struct pm8xxx_gpio_init {
    unsigned            gpio;
    struct pm_gpio            config;
};

struct pm8xxx_mpp_init {
    unsigned            mpp;
    struct pm8xxx_mpp_config_data    config;
};
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)

#define PM8921_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
            _func, _inv, _disable) \
{ \
    .gpio    = PM8921_GPIO_PM_TO_SYS(_gpio), \
    .config    = { \
        .direction    = _dir, \
        .output_buffer    = _buf, \
        .output_value    = _val, \
        .pull        = _pull, \
        .vin_sel    = _vin, \
        .out_strength    = _out_strength, \
        .function    = _func, \
        .inv_int_pol    = _inv, \
        .disable_pin    = _disable, \
    } \
}

#define PM8921_GPIO_DISABLE(_gpio) \
    PM8921_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM_GPIO_VIN_S4, \
             0, 0, 0, 1)

/* FUJITSU:2012-06-20 add_end */

/* FUJITSU:2012-09-25 add_start */
/**********************************************************************/
/* globals															  */
/**********************************************************************/
static DECLARE_WAIT_QUEUE_HEAD(ovp_wq);
static int ovp_initialize_flag = 0;
static int fj_earphone_ic_flag = 0;
static unsigned char g_id_sta07h = DEVICE_USB_CLIENT;

/* FUJITSU:2012-09-25 add_end */
/* ------------------------------------------------------------------------
 * STRUCT
 */
struct ovp_device_struct_for_count {
    unsigned char ovp_device_register_value;
    int ovp_device_count_value;
};
/* FUJITSU:2013-02-15 add_start */
struct ovp_driver_struct_for_xhsdet {
    int value;
    struct work_struct work_xhsdet;
};
/* FUJITSU:2013-02-15 add_end */

/* ------------------------------------------------------------------------
 * GLOBAL VARIABLES
 */
static const char * ovp_driver_name = "ovp_driver";

// i2c_adp desc.
//static struct i2c_adapter * i2c_ovp_driver;

//static struct regulator *vreg_l17;
static int timer_state;
static int ovp_device_interrupt_count;
static int ovp_otg_flag;
static struct timer_list ovp_timer;

extern int charging_mode;
// MSM_GPIO IRQ
static int ovp_vc_det_irq;
static int ovp_vb_det_irq;
static int ovp_intb_det_irq;

// WORK_STRUCT
static struct work_struct ovp_driver_work_timer;
static struct work_struct ovp_driver_work_data_pmic_vcdet;
static struct work_struct ovp_driver_work_data_pmic_vbdet;
static struct work_struct ovp_driver_work_data_pmic_intb;
/* FUJITSU:2012-09-25 add_start */
static struct work_struct ovp_driver_work_initialize;
/* FUJITSU:2012-09-25 add_end */
/* FUJITSU:2013-02-13 add_start */
static struct ovp_driver_struct_for_xhsdet ovp_driver_work_ioctl = {
    .value = 0,
};
/* FUJITSU:2013-02-13 add_end */

// workqueue_struct
static struct workqueue_struct *work_q_msm_timer;
static struct workqueue_struct *work_q_pmic_gpio_vcdet;
static struct workqueue_struct *work_q_pmic_gpio_vbdet;
static struct workqueue_struct *work_q_pmic_gpio_intb;
/* FUJITSU:2012-09-25 add_start */
static struct workqueue_struct *work_q_pmic_initialize;
/* FUJITSU:2012-09-25 add_end */
/* FUJITSU:2013-02-13 add_start */
static struct workqueue_struct *work_q_pmic_ioctl;
/* FUJITSU:2013-02-13 add_end */

//wake_lock
static struct wake_lock ovp_wlock;
static struct wake_lock ovp_wlock_gpio_7;
atomic_t usb_ocp_flag;
atomic_t stand_ocp_flag;
atomic_t usb_occur_flag;

atomic_t usb_interrupt_otg_flag;

/* FUJITSU:2012-11-02 add start */
static struct mutex ovp_lock;
/* FUJITSU:2012-11-02 add end */

/* FUJITSU:2013-01-08 OVP MHL an examination add_start */
static int ovp_mhl_examination_flag = 0;
/* FUJITSU:2013-01-08 OVP MHL an examination add_end */

/* ------------------------------------------------------------------------
 * FUNCTION PROTOTYPES
 */
//struct i2c_adapter * prodev_i2c_search_adapter(void);
int ovp_i2c_write(int reg, int bytes, void *src);
int ovp_i2c_read(int reg, int bytes, void *dest);

static int ovp_check_stand(void);
static int ovp_check_device(void);
static int ovp_check_device_id_chatt(void);
static int ovp_check_ocp(void);

static int ovp_driver_interrupt_device(void);
static int ovp_driver_interrupt_stand(void);

static int ovp_driver_remove_device(void);

static int ovp_driver_set_gpio(void);
static int ovp_init_device_counter(void);
static void ovp_check_device_count(struct ovp_device_struct_for_count *check_device_after);
static void ovp_update_timer(unsigned long time);

static void ovp_driver_work_timer_bh(struct work_struct *work);

static void ovp_driver_work_interrupt_gpio_vcdet_bh(struct work_struct *work);
static void ovp_driver_work_interrupt_gpio_vbdet_bh(struct work_struct *work);
static void ovp_driver_work_interrupt_gpio_intb_bh(struct work_struct *work);
/* FUJITSU:2012-09-25 add_start */
static void ovp_driver_work_initialize_wait(struct work_struct *work);
/* FUJITSU:2012-09-25 add_end */
/* FUJITSU:2013-02-13 add_start */
static void ovp_driver_work_ioctl_phi35(struct work_struct *work);
/* FUJITSU:2013-02-13 add_end */

static irqreturn_t ovp_driver_interrupt_pmic_gpio_vcdet(int irq, void *dev_id);
static irqreturn_t ovp_driver_interrupt_pmic_gpio_vbdet(int irq, void *dev_id);
static irqreturn_t ovp_driver_interrupt_pmic_gpio_intb(int irq, void *dev_id);
static int ovp_driver_probe(struct platform_device *pdev);
static int ovp_driver_remove(struct platform_device *pdev);
static int ovp_driver_resume(struct platform_device *pdev);
static int ovp_driver_suspend(struct platform_device *pdev, pm_message_t mesg);

static unsigned char ovp_read(unsigned char reg,int *ret);
static int ovp_write(unsigned char reg,unsigned char val);
static int ovpdrv_modcw(int wreg, unsigned char rdm, 
                                unsigned char udm, unsigned char val);
/*===========================================================================

  STRUCTS

  ===========================================================================*/
/*--------------------------------------------------------------------
  device module attribute (device desc.) setting
  ------------------------------------------------------------------*/
static struct platform_driver switch_usb = {
    .probe      = ovp_driver_probe,
    .remove     = ovp_driver_remove,
    .resume     = ovp_driver_resume,
    .suspend    = ovp_driver_suspend,
    .driver = {
        .name   = OVP_DRIVER_NAME,
        .owner  = THIS_MODULE,
    },
};

/*--------------------------------------------------------------------
  device check and count for GPIO interrupt
  ------------------------------------------------------------------*/
static struct ovp_device_struct_for_count ovp_device_count_now_device = {
    .ovp_device_register_value = DEVICE_UNKNOWN,
    .ovp_device_count_value = 0,
};
static struct ovp_device_struct_for_count ovp_device_count_before_device = {
    .ovp_device_register_value = DEVICE_UNKNOWN,
    .ovp_device_count_value = 0,
};

/*===========================================================================

  LOCAL FUNCTIONS

  ===========================================================================*/
static struct i2c_client *ovp_i2c_client;

static int __devinit ovp_i2c_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
{
    ovp_i2c_client = client;

    return 0;
}

static int __devexit ovp_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id ovp_i2c_id[] = {
    { "ovp-i2c", 0 },  { }
};

static struct i2c_driver ovp_i2c_driver = {
    .driver = {
        .name = "ovp-i2c",
        .owner = THIS_MODULE,
    },
    .probe    = ovp_i2c_probe,
    .remove   = __devexit_p(ovp_i2c_remove),
    .id_table = ovp_i2c_id,
};

MODULE_DEVICE_TABLE(i2c, ovp_i2c_id);

int ovp_i2c_write(int reg, int bytes, void *src)
{
    unsigned char buf[bytes + 1];
    int ret = 0;

    buf[0] = (unsigned char)reg;
    memcpy(&buf[1], src, bytes);

    ret = i2c_master_send(ovp_i2c_client, buf, bytes + 1);
    if (ret < 0) {
        printk(KERN_ERR"ovp_i2c_write err = %d \n", ret);
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(ovp_i2c_write);

int ovp_i2c_read(int reg, int bytes, void *dest)
{
    struct i2c_msg msg[I2C_READ_SIZE];
    unsigned char data;
    int ret = 0;

    data = (unsigned char)reg;
    msg[0].addr  = ovp_i2c_client->addr;
    msg[0].flags = 0;
    msg[0].buf   = &data;
    msg[0].len   = 1;

    msg[1].addr  = ovp_i2c_client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].buf   = dest;
    msg[1].len   = bytes;
    
    ret = i2c_transfer(ovp_i2c_client->adapter, msg, I2C_TARNS_READ);
    if (ret < 0)
        return ret;

    return 0;
}
EXPORT_SYMBOL(ovp_i2c_read);

static unsigned char ovp_read(unsigned char reg, int *ret)
{
    unsigned char readreg[READ_SIZE];
    *ret = ovp_i2c_read(reg,1,readreg);
    if (*ret < 0) {
        printk(KERN_ERR "%s error\n", __func__);
    }
    return readreg[0];
}

static int ovp_write(unsigned char reg,unsigned char val)
{
    int ret = 0;
    ret = ovp_i2c_write(reg, 1, &val);
    if (ret < 0) {
        printk(KERN_ERR "%s error\n", __func__);
    }
    return ret;
}

static int ovpdrv_modcw(int wreg, unsigned char rdm, 
                                unsigned char udm, unsigned char val)
{
    unsigned char sample;
    int ret;

    sample = ovp_read(wreg,&ret);
    if (ret < 0) {
        printk(KERN_ERR "%s read error\n", __func__);
        return ret;
    }
    ret = ovp_write(wreg,(sample & rdm & ~udm) | (val & udm));
    if (ret < 0) {
        printk(KERN_ERR "%s write error\n", __func__);
    }
    return ret;
}

/**********************************************************************/
/*
 *    ovp_delete_confg
 *
 *    Delete GPIO configration<br>
 *
 */
void ovp_delete_confg(void)
{

    // IRQInterrupt Release
    free_irq(ovp_vc_det_irq, NULL);
    free_irq(ovp_vb_det_irq, NULL);
    free_irq(ovp_intb_det_irq, NULL);

    /* free GPIO_OVP_VC_DET */
    gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VC_DET));
    /* free GPIO_OVP_VB_DET */
    gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET));
    /* free GPIO_OVP_INTB_DET */
    gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_INTB_DET));

    return;
}

/* ----------------------------------------------------------------- */
/*
 * set GPIO INIT
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_driver_set_gpio(void)
{
    int ret = 0;

    /* setup VC_DET */
    ret = gpio_request(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VC_DET), "OVP_VC_DET");
    if (ret) {
        printk("ERROR:gpio_request %d: ret=%d\n",GPIO_OVP_VC_DET , ret);
        goto ON_ERR;
    }
    ret = gpio_direction_input(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VC_DET));
    if (ret) {
        printk("ERROR:gpio_direction_input %d: ret=%d\n",GPIO_OVP_VC_DET, ret);
        goto ON_ERR;
    }
    ovp_vc_det_irq = PM8921_GPIO_IRQ(PM8921_IRQ_BASE, GPIO_OVP_VC_DET);
    ret = request_irq(ovp_vc_det_irq,
                      &ovp_driver_interrupt_pmic_gpio_vcdet, /* Thread function */
                      /* FUJITSU:2012-10-31 level detect add_start */
                      IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND,
                      /* FUJITSU:2012-10-31 level detect add_end */
                      ovp_driver_name,
                      NULL);
    if (ret < 0) {
        printk("ERROR:request_any_context_irq %d: ret=%d\n",GPIO_OVP_VC_DET, ret);
        goto ON_ERR;
    }
    /* setup VB_DET */
    ret = gpio_request(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET), "OVP_VB_DET");
    if (ret) {
        printk("ERROR:gpio_request %d: ret=%d\n",GPIO_OVP_VB_DET , ret);
        goto ON_ERR;
    }
    ret = gpio_direction_input(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET));
    if (ret) {
        printk("ERROR:gpio_direction_input %d: ret=%d\n",GPIO_OVP_VB_DET, ret);
        goto ON_ERR;
    }
    ovp_vb_det_irq = PM8921_GPIO_IRQ(PM8921_IRQ_BASE, GPIO_OVP_VB_DET);
    ret = request_irq(ovp_vb_det_irq,
                      &ovp_driver_interrupt_pmic_gpio_vbdet, /* Thread function */
                      IRQF_TRIGGER_FALLING  | IRQF_TRIGGER_RISING,
                      ovp_driver_name,
                      NULL);
    if (ret < 0) {
        printk("ERROR:request_any_context_irq %d: ret=%d\n",GPIO_OVP_VB_DET, ret);
        goto ON_ERR;
    }
    /* setup INTB_DET */
    ret = gpio_request(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_INTB_DET), "OVP_INTB_DET");
    if (ret) {
        printk("ERROR:gpio_request %d: ret=%d\n",GPIO_OVP_INTB_DET , ret);
        goto ON_ERR;
    }
    ret = gpio_direction_input(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_INTB_DET));
    if (ret) {
        printk("ERROR:gpio_direction_input %d: ret=%d\n",GPIO_OVP_INTB_DET, ret);
        goto ON_ERR;
    }
    ovp_intb_det_irq = PM8921_GPIO_IRQ(PM8921_IRQ_BASE, GPIO_OVP_INTB_DET);
    ret = request_irq(ovp_intb_det_irq,
                      &ovp_driver_interrupt_pmic_gpio_intb, /* Thread function */
                      IRQF_TRIGGER_FALLING  | IRQF_NO_SUSPEND,
                      ovp_driver_name,
                      NULL);
    if (ret < 0) {
        printk("ERROR:request_any_context_irq %d: ret=%d\n",GPIO_OVP_INTB_DET, ret);
        goto ON_ERR;
    }

    enable_irq_wake(ovp_intb_det_irq);
    enable_irq_wake(ovp_vc_det_irq);

    return 0;
ON_ERR:
    ovp_delete_confg();
    return ret;
}

/* ----------------------------------------------------------------- */
/*
 * set register for EARPHONE
 * @return   0  : register value changed for USB OTG HOST
 *          !0  : not changed
 */
int ovp_set_for_earphone(int set_value)
{
    int ret = 0;

    if (set_value == 0) {
        OVP_DBG(KERN_DEBUG"%s:03H write off for earphone\n", __func__);
        ret = OVPDRV_WRCW(SW_CONTROL,0x00);//03h All bit Clear
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 03H "
                "error is %d \n", ret);
            return ret;
        }
    } else if (set_value == 1) {
        OVP_DBG(KERN_DEBUG"%s:03H write on for earphone\n", __func__);
        ret = OVPDRV_WRCW(SW_CONTROL,OVPDRV_ENCC(SW_CONTROL,MICSWEN,ENA)     |
                                     OVPDRV_ENCC(SW_CONTROL,MUXSW_HDPR,EARR) |
                                     OVPDRV_ENCC(SW_CONTROL,MUXSW_HDML,EARL));//03h <- 0x92
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 03H "
                "error is %d\n", ret);
            return ret;
        }
    } else {
        printk(KERN_ERR"[OVP_DRIVER] %s set_value is error %d\n"
            , __func__, set_value);
        return -1;
    }

    return 0;
}
EXPORT_SYMBOL(ovp_set_for_earphone);

/* ----------------------------------------------------------------- */
/*
 * check charge_stand
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_check_stand(void)
{
    int ret = 0;
    /* FUJITSU:2012-12-17 vc chattering add start */
    int pre_vc_value = -1;
    int now_vc_value = 0;
    int vc_chattering_cnt = 0;
    int count;

    for (count=0; count < DEVICE_CHECK_COUNT; count++ ) {
       now_vc_value = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VC_DET));
       if (pre_vc_value == -1) {
           pre_vc_value = now_vc_value;
       }
       if ( vc_chattering_cnt >= DEVICE_CHATTERING_COUNT) {
           printk(KERN_DEBUG "ovp_driver: VCDET=%d %d count chatt total=%d \n"
                                          ,now_vc_value, vc_chattering_cnt, count);
           break;
       }
       if (pre_vc_value == now_vc_value) {
           vc_chattering_cnt++;
       } else {
           pre_vc_value = now_vc_value;
           vc_chattering_cnt = 0;
       }
       usleep(WAIT_TIME);
    }
    /* FUJITSU:2013-04-12 add start */
    ret = ovp_check_ocp();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_check_ocp error is %d\n", __func__, ret);
    }
    /* FUJITSU:2013-04-12 add end */
    if (count >= DEVICE_CHECK_COUNT) {
        printk(KERN_ERR "ovp_driver: VCDET chattering over=%d \n", count);
        return 0;
    }
    /* FUJITSU:2012-12-17 vc chattering add start */
    /* FUJITSU:2012-10-31 level detect add_start */
    if (!now_vc_value) {
        OVP_DBG("[OVP_DRIVER] ovp_check_stand call "
            "ovp_state_detect_stand(DETECT_IN_VC) \n");
        // detect Charging Stand
        irq_set_irq_type(ovp_vc_det_irq,IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND);
        ret = ovp_state_detect_stand(DETECT_IN_VC);
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: %s: called ovp_state_detect_stand "
                "(DETECT_IN_VC) is error is %d\n", __func__, ret);
            goto i2c_error;
        }
    } else {
        OVP_DBG("[OVP_DRIVER] ovp_check_stand call "
            "ovp_state_detect_stand(DETECT_OUT_VC) \n");
        irq_set_irq_type(ovp_vc_det_irq,IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND);
        ret = ovp_state_detect_stand(DETECT_OUT_VC);
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: %s: called ovp_state_detect_stand "
                "(DETECT_OUT_VC) is error is %d\n", __func__, ret);
            goto i2c_error;
        }
    }

    /* FUJITSU:2012-10-31 level detect add_end */
    return 0;
i2c_error:
    if (atomic_read(&stand_ocp_flag) > 0) {
        atomic_set(&stand_ocp_flag, 0);
        if (atomic_read(&usb_ocp_flag) == 0) {
            printk(KERN_ERR"[OVP_DRIVER] %s: Call Charge Driver API"
            "fj_chg_notify_error(NO ERROR) \n"
                    , __func__);
            fj_chg_notify_error(FJ_CHG_ERROR_NONE);
        }
    }
    ovp_state_detect_stand(DETECT_OUT_VC);
    return -1;
}

/* ----------------------------------------------------------------- */
/*
 * init device check counter
 * @return   0   :    device check counter initialization success
 *           !0  :    device check counter initialization fail
 */
static int ovp_init_device_counter(void)
{
    ovp_device_count_now_device.ovp_device_count_value = 0;
    timer_state = 0;
    ovp_device_interrupt_count = 0;
    return 0;
}
 
/* ----------------------------------------------------------------- */
/*
 * check value of device check counter
 * 
 */
static void ovp_check_device_count(struct ovp_device_struct_for_count *check_device_after)
{
    if (check_device_after->ovp_device_count_value == 0) {
        ovp_device_count_before_device.ovp_device_register_value
                = check_device_after->ovp_device_register_value;
        check_device_after->ovp_device_count_value++;
    } else {
        if (ovp_device_count_before_device.ovp_device_register_value
                == check_device_after->ovp_device_register_value) {
            check_device_after->ovp_device_count_value++;
        } else {
            printk(KERN_INFO"[OVP_DRIVER] device_sta = 0x%02X before_sta = 0x%02X "
                "ovp_device_interrupt_count = %d\n",
                    check_device_after->ovp_device_register_value,
                        ovp_device_count_before_device.ovp_device_register_value,
                            ovp_device_interrupt_count);
            ovp_device_count_before_device.ovp_device_register_value
                    = check_device_after->ovp_device_register_value;
            check_device_after->ovp_device_count_value = 1;
        }
    }
}

static int ovp_check_device(void)
{
    int ret = 0;
    unsigned char int_sta05h;
    unsigned char status06h;
    unsigned char id_sta07h;
    
    int pre_ocp_value = 0;
    int now_ocp_value = 0;
    struct usb_state usb_sts;

    /* FUJITSU:2012-12-17 vbus chattering add start */
    int pre_vb_value = -1;
    int now_vb_value = 0;
    int vb_chattering_cnt = 0;
    int count;
    /* FUJITSU:2012-12-17 vbus chattering add start */

    id_sta07h = OVPDRV_RDCW(ID_STA,&ret);// 07h
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 07H error is %d \n", ret);
        goto check_device_error;
    }

    int_sta05h = OVPDRV_RDCW(INT_STA,&ret);// 05h
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 05H error is %d \n", ret);
        goto check_device_error;
    }

    status06h = OVPDRV_RDCW(STATUS,&ret);// 06h
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 06H error is %d \n", ret);
        goto check_device_error;
    }

    /* FUJITSU:2012-12-17 vbus chattering add start */
    if (!(OVPDRV_MSKC_EQ(ID_STA,INDO,USB_OTG,id_sta07h))) {
       for (count=0; count < DEVICE_CHECK_COUNT; count++ ) {
           if (timer_state == 0) {
               now_vb_value = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET));
               if (pre_vb_value == -1) {
                   pre_vb_value = now_vb_value;
               }
               if ( vb_chattering_cnt >= DEVICE_CHATTERING_COUNT) {
                   /* FUJITSU:2013-01-21 vbus chattering add start */
                   int_sta05h = OVPDRV_RDCW(INT_STA,&ret);// 05h
                   if (ret < 0) {
                       printk(KERN_ERR "ovp_driver: als status I2C access to 05H error is %d \n", ret);
                       break;
                   }
                   printk(KERN_DEBUG "ovp_driver: VBDET=%d %d count chatt total=%d 05h=%x\n"
                                                  ,now_vb_value, vb_chattering_cnt, count, int_sta05h);
                   /* FUJITSU:2013-01-21 vbus chattering add end */
                   break;
               }
               if (pre_vb_value == now_vb_value) {
                   vb_chattering_cnt++;
               } else {
                   pre_vb_value = now_vb_value;
                   vb_chattering_cnt = 0;
               }
           } else {
               break;
           }
           usleep(WAIT_TIME);
       }
       if (count >= DEVICE_CHECK_COUNT) {
           printk(KERN_ERR "ovp_driver: VBDET chattering over=%d \n", count);
           goto check_device_error;
       }
    }
    /* FUJITSU:2012-12-17 vbus chattering add end */

    // check over_current and voltage
    ret = ovp_check_ocp();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_check_ocp detect ovp or ocp \n", __func__);
        ovp_init_device_counter();
        return ret;
    }

    if (OVPDRV_MSKC_EQ(ID_STA,IDRDET,UNDET,id_sta07h)) {
        // USB remove check
        if (((OVPDRV_MSKC_EQ(INT_STA,VBUSDET,UNDET,int_sta05h)) && OVPDRV_MSKC_EQ(ID_STA,INDO,USB_CLIENT,id_sta07h)) ||
            (OVPDRV_MSKC_EQ(INT_STA,COMPH,REMOVE_DET,int_sta05h) && OVPDRV_MSKC_EQ(ID_STA,INDO,USB_CLIENT,id_sta07h))) {
            ovp_init_device_counter();

            pre_ocp_value = atomic_read(&usb_ocp_flag);

            if (OVPDRV_MSKC_EQ(STATUS,OVP_VB,DET,status06h) || OVPDRV_MSKC_EQ(STATUS,OCP_VB,DET,status06h)) {
                now_ocp_value = 1;
            }

            OVP_DBG(KERN_DEBUG"%s pre_ocp = %d, now_ocp = %d\n"
                    , __func__, pre_ocp_value, now_ocp_value);
            if ((pre_ocp_value > 0) && (now_ocp_value > 0)) {
                printk(KERN_ERR"%s no action\n", __func__);
            } else {
                ovp_check_ocp();
            }
            ret = ovp_driver_remove_device();
            return ret;
        }
    }
    /* FUJITSU:2012-10-15 OVP start */
    ovp_state_show_state(&usb_sts);
    if (OVPDRV_MSKC_EQ(ID_STA,INDO,USB_MHL,id_sta07h) && (usb_sts.state == STATE_USB_CLIENT_AC)) {
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s STATE_USB_CLIENT_AC retry ID detect \n", __func__);
        ovp_state_detect(DETECT_OUT_USB_CONNECT, NULL);
    }
    /* FUJITSU:2012-10-15 OVP end */

    ret = ovp_check_device_id_chatt();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_check_device_id_chatt error is %d\n", __func__, ret);
        ovp_init_device_counter();
        return ret;
    }
    return 0;

/* FUJITSU:2012-05-01 OVP start */
check_device_error:
    ovp_init_device_counter();
    ovp_driver_remove_device();
    return -1;
/* FUJITSU:2012-05-01 OVP end */
}

/* ----------------------------------------------------------------- */
/*
 * check outer device
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_check_device_id_chatt(void)
{
    int ret = 0;
    int for_cnt;
    int vbdet_value = 0;/*FUJITSU:2013-04-12 add */
    unsigned char int_sta05h;
    unsigned char status06h;
    unsigned char id_sta07h;
    
    struct usb_state usb_sts;

    for (for_cnt = 0; for_cnt < 100; for_cnt++) {
        // accessory detect process start
        id_sta07h = OVPDRV_RDCW(ID_STA,&ret);// 07h
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 07H error is %d \n", ret);
            break;
        }

        int_sta05h = OVPDRV_RDCW(INT_STA,&ret);// 05h
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 05H error is %d \n", ret);
            break;
        }

        status06h = OVPDRV_RDCW(STATUS,&ret);// 06h
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 06H error is %d \n", ret);
            break;
        }

        if (gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_INTB_DET))) {
            break;
        }
        usleep(5000);
    }
    if (for_cnt >= 100) {
        printk(KERN_ERR "%s for cnt error %dcnt \n", __func__, for_cnt);
        goto check_device_error;
    }

    if (ret < 0) {
        goto check_device_error;
    }

    if (!(OVPDRV_MSKC_EQ(ID_STA,INDO,USB_CLIENT,id_sta07h))) {
        timer_state = 2;
    }

    switch (timer_state) {
    case 0:
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s: timer_state is %d \n",
        __func__, timer_state);
        timer_state = 1;

        OVP_DBG(KERN_DEBUG"%s wake_lock ovp_wlock_gpio_7\n", __func__);
        wake_lock(&ovp_wlock_gpio_7);
        mod_timer(&ovp_timer, jiffies + msecs_to_jiffies(CLIENT_CHECK_COUNT));

        return 0;
    case 1:
        timer_state = 2;
    case 2:
        break;
    default:
        printk(KERN_ERR"[OVP_DRIVER] %s: timer_state is error %d \n"
                , __func__, timer_state);
        return -1;
    }

    switch (id_sta07h & OVPDRV_MSKC(ID_STA,INDO)) {
        /* FUJITSU:2012-12-17 vbus chattering add start */
    case DEVICE_USB_CLIENT:
        ovp_device_count_now_device.ovp_device_register_value = DEVICE_USB_CLIENT;
        ovp_device_count_now_device.ovp_device_count_value = CONTINUE_COUNT;
        break;
        /* FUJITSU:2012-12-17 vbus chattering add start */
    case DEVICE_CABLE_VBUS:
        /* FUJITSU:2012-11-16 ACA-SDP charging start */
    case DEVICE_USB_ACA_RIDA:
    case DEVICE_USB_ACA_RIDB:
    case DEVICE_USB_ACA_RIDC:
        /* FUJITSU:2012-11-16 ACA-SDP charging end */
        ovp_device_count_now_device.ovp_device_register_value = DEVICE_USB_CLIENT;
        ovp_check_device_count(&ovp_device_count_now_device);
        break;
    case DEVICE_USB_OTG:
        ovp_device_count_now_device.ovp_device_register_value = DEVICE_USB_OTG;
        ovp_device_count_now_device.ovp_device_count_value = CONTINUE_COUNT;
        break;
    case DEVICE_USB_MHL:
        /* FUJITSU:2013-01-08 OVP MHL an examination add_start */
        if (ovp_mhl_examination_flag == 1) {
            printk(KERN_INFO "%s: ovp_mhl_examination_flag=%d next chattering once \n", __func__,ovp_mhl_examination_flag);
            ovp_device_count_now_device.ovp_device_count_value = CONTINUE_COUNT;
            ovp_device_count_before_device.ovp_device_register_value = DEVICE_USB_MHL;
            ovp_mhl_examination_flag = 0;
        }
        /* FUJITSU:2013-01-08 OVP MHL an examination add_end */
        ovp_device_count_now_device.ovp_device_register_value = DEVICE_USB_MHL;
        ovp_check_device_count(&ovp_device_count_now_device);
        break;
    case DEVICE_STEREOEARPHONE:
        ovp_device_count_now_device.ovp_device_register_value = DEVICE_STEREOEARPHONE;
        ovp_check_device_count(&ovp_device_count_now_device);
        break;
    case DEVICE_EARPHONESWITCH:
        printk(KERN_INFO"[OVP_DRIVER] %s: ON EARPHONE SWITCH\n", __func__);
        ovp_init_device_counter();
        return 0;
    default:
        printk(KERN_WARNING"[OVP_DRIVER] %s: REGISTER 07H is unknown 0x%02X\n"
                , __func__, id_sta07h);
        ovp_device_count_now_device.ovp_device_register_value = DEVICE_UNKNOWN;
        ovp_check_device_count(&ovp_device_count_now_device);
        break;
    }

    ovp_device_interrupt_count++;

    if (ovp_device_interrupt_count >= (CONTINUE_COUNT * 2)) {
        printk(KERN_ERR"[OVP_DRIVER] %s: CONTINUE_COUNT error\n", __func__);
        ovp_init_device_counter();
        return 0;
    }
    if (ovp_device_count_now_device.ovp_device_count_value >= CONTINUE_COUNT) {
        switch (ovp_device_count_now_device.ovp_device_register_value) {
        case DEVICE_USB_CLIENT:        // 0x0D
            printk(KERN_DEBUG"[OVP_DRIVER] %s: detect"
                    "DEVICE_USB_CLIENT value is 0x%02X ,chargemode is %d\n"
                    , __func__, id_sta07h,charging_mode);
            int_sta05h = OVPDRV_RDCW(INT_STA,&ret);
            if (OVPDRV_MSKC_EQ(INT_STA,VBUSDET,DET,int_sta05h)) {
                ovp_state_detect(DETECT_IN_USB_CLIENT, NULL);
            } else {
                ovp_state_detect(DETECT_OUT_USB_CONNECT, NULL);
            }
            break;
        case DEVICE_USB_MHL:               // 0x1e
            /*FUJITSU:2013-04-12 add start*/
            vbdet_value = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET));
            /*FUJITSU:2013-04-12 add end*/
            printk(KERN_DEBUG"[OVP_DRIVER] %s: detect "
                                   "DEVICE_USB_MHL value is 0x%02X  ,chargemode is %d\n"
                   , __func__, id_sta07h,charging_mode);
            if (charging_mode) {                         // check OFF charger mode charging_mode
                /*FUJITSU:2013-04-12 add start*/
                if (!vbdet_value) {
                    printk(KERN_INFO"[OVP_DRIVER] %s:VBUS On & OFF charge ON \n"
                        ,__func__);
                    ovp_state_detect(DETECT_OFF_CHARGER_MHL, NULL);
                } else {
                    printk(KERN_INFO"[OVP_DRIVER] %s:VBUS Off & OFF charge OFF \n"
                        ,__func__);
                    ovp_state_detect(DETECT_OUT_OFF_CHARGER_MHL, NULL);
                }
            } else {                         //  OFF charger mode OFF
                if ((system_rev & OVP_FJDEV_MASK) == (OVP_FJDEV005)) {
                    if (!vbdet_value) {
                        printk(KERN_INFO"%s:VBUSON MHL_ID SDPcharge\n",__func__);
                        ovp_state_detect(DETECT_IN_USB_UNKNOWN_VBUSON, NULL);
                    } else {
                        printk(KERN_INFO"%s:VBUSOFF MHL_ID chargeOFF\n",__func__);
                        ovp_state_detect(DETECT_OUT_USB_CONNECT, NULL);
                    }
                /*FUJITSU:2013-04-12 add end*/
                } else {
                    ovp_state_detect(CONFIRM_MHL, NULL);
                }
            }
            break;
        case DEVICE_USB_OTG:       // 0x10
            printk(KERN_DEBUG"[OVP_DRIVER] %s: detect "
                                   "DEVICE_USB_OTG value is 0x%02X  ,chargemode is %d\n"
                   , __func__, id_sta07h,charging_mode);
            ovp_state_detect(CONFIRM_USB_HOST, NULL);
            break;
        case DEVICE_STEREOEARPHONE:
            printk(KERN_DEBUG"[OVP_DRIVER] %s: detect "
                                   "DEVICE_USB_EARPHONE value is 0x%02X  ,chargemode is %d\n"
                   , __func__, id_sta07h,charging_mode);
            if (fj_earphone_ic_flag) {
                ovp_state_detect(DETECT_IN_STEREO_EARPHONE, NULL);
            }
            break;
        default:
            printk(KERN_INFO"[OVP_DRIVER] %s: get value of REGISTER 07H "
                      "is default 0x%02X\n", __func__, id_sta07h);/* FUJITSU:2013-02-27 OVP defult id sdp charging */
            FJ_OVP_REC_LOG("OVP:OVP_UNKNOWN_ID 0x%02X\n", id_sta07h);
            ovp_state_show_state(&usb_sts);
            if (usb_sts.state == STATE_OFF_CHARGER_MHL) {
                ovp_state_detect(DETECT_OUT_OFF_CHARGER_MHL, NULL);
            } else if (usb_sts.state == STATE_MHL) {
                ovp_state_detect(DETECT_OUT_MHL, NULL);
            } else {
                /* FUJITSU:2013-02-27 OVP defult id sdp charging add_start */
                if (!gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET))) {
                    printk(KERN_INFO "REGISTER 07H is default VBUSON SDPcharge\n");
                    ovp_state_detect(DETECT_IN_USB_UNKNOWN_VBUSON, NULL);
                } else {
                    printk(KERN_ERR "REGISTER 07H is default VBUSOFF chargeOFF\n");
                    FJ_OVP_REC_LOG("OVP:OVP_UNKNOWN_ID VBUSOFF chargeOFF\n");
                    ovp_state_detect(DETECT_OUT_USB_CONNECT, NULL);
                }
                /* FUJITSU:2013-02-27 OVP defult id sdp charging add_end */
            }
            break;
        }
        g_id_sta07h = (id_sta07h & OVPDRV_MSKC(ID_STA,INDO));
        ovp_init_device_counter();
    } else {
        ret = OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,DEFAULT) |
                                OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 0 bit4 <- 1
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 01H "
                "error is %d\n", ret);
            goto check_device_error;
        }
        ret = OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,RETRY) |
                                OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h bit1 <- 1 bit4 <- 1
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 01H "
                "error is %d\n", ret);
            goto check_device_error;
        }
    }
    return 0;

/* FUJITSU:2012-05-01 OVP start */
check_device_error:
    ovp_init_device_counter();
    ovp_driver_remove_device();
    return -1;
/* FUJITSU:2012-05-01 OVP end */
}

/* ----------------------------------------------------------------- */
/*
 * check over current
 * @return   0   :    found device(expection DEVICE_CABLE_NO_VBUS)
 *          !0   :    over current or i2c error
 */
static int ovp_check_ocp(void)
{
    // check over_current
    int ret = 0;
    unsigned char status06h;

    status06h = OVPDRV_RDCW(STATUS,&ret);// 06h
    if (ret < 0) {
        printk(KERN_ERR "[OVP_DRIVER] %s: als status I2C access "
            "to 06H error is %d\n", __func__, ret);
        return ret;
    }

    // when over_current
    if (OVPDRV_MSKC_EQ(STATUS,OCP_VB,DET,status06h) || OVPDRV_MSKC_EQ(STATUS,OCP_VC,DET,status06h)
        || OVPDRV_MSKC_EQ(STATUS,OVP_VB,DET,status06h) || OVPDRV_MSKC_EQ(STATUS,OVP_VC,DET,status06h)) { // when over voltage
        printk(KERN_ERR"[OVP_DRIVER] %s: Call Charge Driver API "
            "fj_chg_notify_error(ERROR), 06H is 0x%02X\n"
            , __func__, status06h);

        // Call Charge Driver API (stop charge)
        FJ_OVP_REC_LOG("OVP:Call Charge Driver API fj_chg_notify_error(ERROR), 06H is 0x%02X\n", status06h);
        fj_chg_notify_error(FJ_CHG_ERROR_DETECT);

        if (OVPDRV_MSKC_EQ(STATUS,OCP_VB,DET,status06h) || OVPDRV_MSKC_EQ(STATUS,OVP_VB,DET,status06h)) {
            printk(KERN_INFO"[OVP_DRIVER]%s  usb_ocp_flag = 1 \n",__func__);
            atomic_set(&usb_ocp_flag, 1);
        } else {
            printk(KERN_INFO"[OVP_DRIVER]%s  usb_ocp_flag = 0 \n",__func__);
            atomic_set(&usb_ocp_flag, 0);
        }
        if (OVPDRV_MSKC_EQ(STATUS,OCP_VC,DET,status06h) || OVPDRV_MSKC_EQ(STATUS,OVP_VC,DET,status06h)) {
            printk(KERN_INFO"[OVP_DRIVER]%s  stand_ocp_flag = 1 \n",__func__);
            atomic_set(&stand_ocp_flag, 1);
        } else {
            printk(KERN_INFO"[OVP_DRIVER]%s  stand_ocp_flag = 0 \n",__func__);
            atomic_set(&stand_ocp_flag, 0);
        }
        return -1;
    } else {
        if ((atomic_read(&usb_ocp_flag) > 0) || (atomic_read(&stand_ocp_flag) > 0)) {
            atomic_set(&usb_ocp_flag, 0);
            atomic_set(&stand_ocp_flag, 0);
            printk(KERN_INFO"[OVP_DRIVER] %s: Call Charge Driver API"
            "fj_chg_notify_error(NO ERROR), 06H is 0x%02X\n"
                    , __func__, status06h);
            FJ_OVP_REC_LOG("OVP:Call Charge Driver API fj_chg_notify_error(NO ERROR), 06H is 0x%02X\n", status06h);
            fj_chg_notify_error(FJ_CHG_ERROR_NONE);
        }
    }

    return 0;
}

/* ----------------------------------------------------------------- */
/*
 * remove decice
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_driver_remove_device(void)
{

    unsigned char int_sta05h;
    unsigned char status06h;
    unsigned char id_sta07h;

    int ret = 0;
    struct usb_state usb_sts;
    ovp_state_show_state(&usb_sts);
    switch (usb_sts.state) {
    case STATE_OFF_CHARGER_MHL:
        ovp_state_detect(DETECT_OUT_OFF_CHARGER_MHL, NULL);
        break;
    case STATE_MHL:
        ovp_state_detect(DETECT_OUT_MHL, NULL);
        break;
    default:
        ovp_state_detect(DETECT_OUT_USB_CONNECT, NULL);
        break;
    }

    msleep(25);

    int_sta05h = OVPDRV_RDCW(INT_STA,&ret);// 05h
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access 05h error is %d \n"
            , ret);
        return ret;
    }

    status06h = OVPDRV_RDCW(STATUS,&ret);// 06h
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access 06h error is %d \n"
            , ret);
        return ret;
    }

    id_sta07h = OVPDRV_RDCW(ID_STA,&ret);// 07h
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access 07h error is %d \n"
            , ret);
        return ret;
    }
    g_id_sta07h = (id_sta07h & OVPDRV_MSKC(ID_STA,INDO));

    if (OVPDRV_MSKC_EQ(ID_STA,IDRDET,DET,id_sta07h)) {
        ret = OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,DEFAULT) |
                                OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 0 bit4 <- 1
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 01H <- 0 "
                " error is %d\n", ret);
            return ret;
        }

        ret = OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,RETRY) |
                                OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 1 bit4 <- 1
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 01H <- 1"
                " error  is %d \n", ret);
            return ret;
        }
    }

    ovp_init_device_counter();
/* FUJITSU:2012-05-01 OVP start */
    if (atomic_read(&usb_ocp_flag) > 0) {
        if (OVPDRV_MSKC_EQ(STATUS,OVP_VB,UNDET,status06h) && OVPDRV_MSKC_EQ(STATUS,OCP_VB,UNDET,status06h)) {
            atomic_set(&usb_ocp_flag, 0);
            if (atomic_read(&stand_ocp_flag) == 0) {
                printk(KERN_INFO"[OVP_DRIVER] %s: Call Charge Driver API"
                "fj_chg_notify_error(NO ERROR), 06H is 0x%02X\n"
                        , __func__, status06h);
                fj_chg_notify_error(FJ_CHG_ERROR_NONE);
            }
        }
    }
/* FUJITSU:2012-05-01 OVP end */
    return 0;
}

/* ----------------------------------------------------------------- */
/*
 * interrput device
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_driver_interrupt_device(void)
{
    int ret = 0;

    // check device
    ret = ovp_check_device();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_check_device error is %d\n", __func__, ret);
        ovp_init_device_counter();
        return ret;
    }

    return 0;
};

/* ----------------------------------------------------------------- */
/*
 * interrput charge stand
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_driver_interrupt_stand(void)
{
    int ret = 0;

    /*FUJITSU:2013-04-12 del start*/
#if 0
    ret = ovp_check_ocp();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_check_ocp error is %d\n", __func__, ret);
    }
#endif
    /*FUJITSU:2013-04-12 del end*/
    // check charging stand
    ret = ovp_check_stand();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_check_stand error is %d\n", __func__, ret);
        return ret;
    }

    return 0;
};

/*===========================================================================

  EXTERN FUNCTIONS

  ===========================================================================*/
/* ----------------------------------------------------------------- */
/*
 * does allow interrupt to the ovp_driver
 * @return  0      :    normal end
 *          !0     :    error
 */
int ovp_set_allow_to_interrupt(bool permission)
{
    if (permission) {
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s  interrupt on \n", __func__);
        enable_irq(ovp_vc_det_irq);
    } else {
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s  interrupt off \n", __func__);
        disable_irq(ovp_vc_det_irq);
    }
    return 0;
}
EXPORT_SYMBOL(ovp_set_allow_to_interrupt);

/* ----------------------------------------------------------------- */
/*
 * ovp_driver  gpio switch setting
 * @return   0   :    function success
 *          !0   :    function fail 
 */
int ovp_set_switch(int state)
{
    int ret = 0;
    struct usb_state usb_sts;  /* FUJITSU:2012-11-02 OVP-MHL add */
    /* FUJITSU:2013-01-08 OVP MHL an examination add_start */
    int i;
    int vbdet_value;
    unsigned char int_sta05h;
    unsigned char id_sta07h;
    /* FUJITSU:2013-01-08 OVP MHL an examination add_end */

    switch (state) {
    case SWITCH_STATE_NO_CONNECT: 
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),DEVICE NO CONNECT\n", __func__);
        if (ovp_otg_flag) {
            msleep(10);
        }
        break;
    case SWITCH_STATE_USB_CLIENT:
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),DEVICE CLI\n", __func__);
        if (ovp_otg_flag) {
            msleep(10);
        }
        if ((system_rev & OVP_FJDEV_MASK) != (OVP_FJDEV005)) {
            gpio_set_value_cansleep(PM8921_GPIO_PM_TO_SYS(OVP_PM_GPIO_12), 0);
        }
        gpio_set_value_cansleep(PM8921_GPIO_PM_TO_SYS(OVP_PM_GPIO_42), 0);
        break;
    case SWITCH_STATE_USB_OTG_HOST:
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),DEVICE OTG\n", __func__);
        break;
    case SWITCH_STATE_MHL:
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),DEVICE MHL\n", __func__);
        break;

    case SWITCH_STATE_STEREO_EARPHONE:
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),DEVICE STEREO_EARPHONE\n", __func__);
        break;
    case SWITCH_STATE_PHI35_EARPHONE:
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),DEVICE PHI35_EARPHONE\n", __func__);
        break;
    default:
        printk(KERN_ERR"[OVP_DRIVER] %s: event is error state = %d\n"
                , __func__, state);
        ret = -1;
        return ret;
    }
    /* FUJITSU:2013-01-08 OVP MHL an examination add_start */
    if (state != SWITCH_STATE_MHL) {
        if (ovp_mhl_examination_flag == 1) {
            printk(KERN_INFO "%s: ovp_mhl_examination_flag clear \n", __func__);
            ovp_mhl_examination_flag = 0;
        }
    }
    /* FUJITSU:2013-01-08 OVP MHL an examination add_end */

    ret = OVPDRV_WRCW(UCDCNT,OVPDRV_ENCC(UCDCNT,INTBEN,ENA) |
                             OVPDRV_ENCC(UCDCNT,USBDETCTL,DIS));//02h 0x44
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 02H "
            "error ret = %d\n", ret);
        return ret;
    }
    ret = OVPDRV_WRCW(SW_CONTROL,0x00);//03h All bit Clear
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 03H <- 00 "
            " error is %d\n", ret);
        return ret;
    }
    ret = OVPDRV_WRCW(OVP_CONTROL,0x00);//04h All bit Clear
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 04H <- 00 "
            " error is %d\n", ret);
        return ret;
    }

    /* OVPIC setting */
    switch (state) {
    case SWITCH_STATE_NO_CONNECT:
        /* FUJITSU:2013-01-08 OVP MHL an examination add_start */
        ovp_state_show_state(&usb_sts);
        if (usb_sts.state == STATE_MHL) {
            ovp_mhl_examination_flag = 1;
            OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),MHL ADC retry \n", __func__);
            vbdet_value = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET));
            if ( !vbdet_value ) {
                /* VBDET ON */
                disable_irq(ovp_intb_det_irq);
                disable_irq(ovp_vb_det_irq);
                for ( i=0; i < MHL_CHATTERING_COUNT; i++ ) {
                    /* ADC_RETRY */
                    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),VBDET ON adc retry \n", __func__);
                    OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,DEFAULT) |
                                      OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 0 bit4 <- 1
                    OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,RETRY) |
                                      OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 1 bit4 <- 1
                    msleep(70);
                    int_sta05h = OVPDRV_RDCW(INT_STA,&ret);// 05h
                    id_sta07h = OVPDRV_RDCW(ID_STA,&ret);// 07h
                    if (OVPDRV_MSKC_EQ(ID_STA,INDO,USB_MHL,id_sta07h)) {
                        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),VBDET ON MHL_ID detect! \n", __func__);
                        enable_irq(ovp_intb_det_irq);
                        enable_irq(ovp_vb_det_irq);
                        OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,DEFAULT) |
                                          OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 0 bit4 <- 1
                        OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,RETRY) |
                                          OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 1 bit4 <- 1
                        break;
                    }
                    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),VBDET ON MHL_ID 1sec wait \n", __func__);
                    msleep(1000);
                    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),VBDET ON MHL_ID 1sec wait end \n", __func__);
                }
                if (i >= MHL_CHATTERING_COUNT) {
                    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),VBDET ON MHL_ID not detect 7sec T.O 07=%x \n", __func__,id_sta07h);
                    enable_irq(ovp_intb_det_irq);
                    enable_irq(ovp_vb_det_irq);
                }
            /* FUJITSU:2013-04-12 OVP MHL an examination add_end */
            } else {
                if ((fj_boot_mode != FJ_MODE_MAKER_MODE) && (fj_boot_mode != FJ_MODE_KERNEL_MODE)) {
                    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s(),MHL VBDET OFF ADC RETRY\n", __func__);
                    ret = OVPDRV_WRCW(S_RST,S_RST_SOFT_RESET_RESET);//08h <- 0x01
                    if (ret < 0) {
                        printk(KERN_ERR "ovp_driver: als status I2C access to 08H "
                            "error ret = %d\n", ret);
                    }

                    ret = OVPDRV_WRCW(UCDCNT,OVPDRV_ENCC(UCDCNT,INTBEN,ENA) |
                                 OVPDRV_ENCC(UCDCNT,USBDETCTL,DIS));//02h 0x44
                    if (ret < 0) {
                        printk(KERN_ERR "ovp_driver: als status I2C access to 02H "
                        "error ret = %d\n", ret);
                    }
                    OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,DEFAULT) |
                                      OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 0 bit4 <- 1
                    OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,RETRY) |
                                      OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 1 bit4 <- 1
                }
            /* FUJITSU:2013-04-12 OVP MHL an examination add_end */
            }
        }
        /* FUJITSU:2013-01-08 OVP MHL an examination add_end */
        break;
    case SWITCH_STATE_USB_CLIENT:
        break;
    case SWITCH_STATE_USB_OTG_HOST:
        ret = OVPDRV_UDCW(OVP_CONTROL,OVPDRV_MSKC(OVP_CONTROL,OTGSWEN)         |
                                      OVPDRV_MSKC(OVP_CONTROL,VBSW_DISEN),
                                      OVPDRV_ENCC(OVP_CONTROL,OTGSWEN,ENA)     |
                                      OVPDRV_ENCC(OVP_CONTROL,VBSW_DISEN,DIS));//04h 0x09
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 04H <- 1 "
                " error is %d\n", ret);
            return ret;
        }
        break;
    case SWITCH_STATE_MHL:
        ret = OVPDRV_UDCB(UCDCNT,INTBEN,DIS);//INTBEN@02h <- 0
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 02H "
                "error ret = %d\n", ret);
            return ret;
        }
        ret = OVPDRV_WRCW(SW_CONTROL,OVPDRV_ENCC(SW_CONTROL,CBUSSWEN,ENA)     |
                                     OVPDRV_ENCC(SW_CONTROL,MUXSW_HDPR,DSS_H) |
                                     OVPDRV_ENCC(SW_CONTROL,MUXSW_HDML,DSS_H));//03h <- 0x49
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 03H "
                "error ret = %d\n", ret);
            return ret;
        }
        break;
    case SWITCH_STATE_STEREO_EARPHONE:
        ret = OVPDRV_WRCW(UCDCNT,OVPDRV_ENCC(UCDCNT,PDREN,ENA) |
                                 OVPDRV_ENCC(UCDCNT,PDLEN,ENA) |
                                 OVPDRV_ENCC(UCDCNT,INTBEN,ENA));//02h <- 0x70
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 02H "
                "error is %d\n", ret);
            return ret;
        }
        ret = OVPDRV_WRCW(SW_CONTROL,OVPDRV_ENCC(SW_CONTROL,MUXSW_HDML,EARL) |
                                     OVPDRV_ENCC(SW_CONTROL,MUXSW_HDPR,EARR) |
                                     OVPDRV_ENCC(SW_CONTROL,MICSWEN,ENA));//03h <- 0x92
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 03H "
                "error is %d\n", ret);
            return ret;
        }
        break;
    case SWITCH_STATE_PHI35_EARPHONE:
        ret = OVPDRV_WRCW(UCDCNT,OVPDRV_ENCC(UCDCNT,PDREN,ENA) |
                                 OVPDRV_ENCC(UCDCNT,PDLEN,ENA) |
                                 OVPDRV_ENCC(UCDCNT,INTBEN,ENA));//02h <- 0x70
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 02H "
                "error is %d\n", ret);
            return ret;
        }
        ret = OVPDRV_WRCW(SW_CONTROL,OVPDRV_ENCC(SW_CONTROL,MUXSW_HDML,Hi_Z) |
                                     OVPDRV_ENCC(SW_CONTROL,MUXSW_HDPR,Hi_Z));//03h <- 0x3f
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 03H "
                "error is %d\n", ret);
            return ret;
        }
        break;
    default:
        break;
    }

    return 0;
}
EXPORT_SYMBOL(ovp_set_switch);

/* ----------------------------------------------------------------- */
/*
 * ovp_driver  set otg flag
 * @return   0   :    function success
 *          !0   :    function fail 
 */
int ovp_set_otg_flag(int flag)
{
    if (flag == 0 || flag == 1) {
        ovp_otg_flag = flag;
    }
    return 0;
}
EXPORT_SYMBOL(ovp_set_otg_flag);

/* ----------------------------------------------------------------- */
/*
 * ovp_driver  occurrence of un interrupt
 * @return   0   :    function success
 *          !0   :    function fail 
 */
int ovp_occure_interrupt(void)
{
    int ret = 0;

    if (atomic_read(&usb_occur_flag) == 0) {
        atomic_set(&usb_occur_flag, 1);
        ret = OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,DEFAULT) |
                                OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 0 bit4 <- 1
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 01H <- 0 "
                " error is %d\n", ret);
            goto occur_i2c_error;
        }
        ret = OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,RETRY) |
                                OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 1 bit4 <- 1
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 01H <- 1"
                " error  is %d \n", ret);
            goto occur_i2c_error;
        }
        atomic_set(&usb_occur_flag, 0);
    }

    return 0;

occur_i2c_error:
    atomic_set(&usb_occur_flag, 0);
    return ret;
}
EXPORT_SYMBOL(ovp_occure_interrupt);

/* ----------------------------------------------------------------- */
/*
 * does ovp_driver irq handler for gpio_vcdet(VCDET)
 * @return    IRQ_HANDLED
 */
static irqreturn_t ovp_driver_interrupt_pmic_gpio_vcdet(int irq, void *dev_id)
{
    disable_irq_nosync(ovp_vc_det_irq);

    queue_work(work_q_pmic_gpio_vcdet, &ovp_driver_work_data_pmic_vcdet);

    return IRQ_HANDLED;
}

/* ----------------------------------------------------------------- */
/*
 * does ovp_driver irq handler for gpio_vbde(VBDET)
 * @return    IRQ_HANDLED
 */
static irqreturn_t ovp_driver_interrupt_pmic_gpio_vbdet(int irq, void *dev_id)
{
    disable_irq_nosync(ovp_vb_det_irq);

    queue_work(work_q_pmic_gpio_vbdet, &ovp_driver_work_data_pmic_vbdet);

    return IRQ_HANDLED;
}

/* ----------------------------------------------------------------- */
/*
 * does ovp_driver irq handler for _pmic_gpio_intb(INTB)
 * @return    IRQ_HANDLED
 */ 
static irqreturn_t ovp_driver_interrupt_pmic_gpio_intb(int irq, void *dev_id)
{
    wake_lock(&ovp_wlock);
    if (timer_state == 1) {
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s del_timer\n", __func__);
        del_timer(&ovp_timer);
        OVP_DBG(KERN_DEBUG"%s wake_unlock ovp_wlock_gpio_7\n", __func__);
        wake_unlock(&ovp_wlock_gpio_7);
    }

//    disable_irq_nosync(ovp_intb_det_irq); /* FUJITSU:2012-11-02 del */
    queue_work(work_q_pmic_gpio_intb, &ovp_driver_work_data_pmic_intb);
    
    return IRQ_HANDLED;
}

/* ----------------------------------------------------------------- */
/*
 * does ovp_driver block hander for gpio_intb
 * @param    work_struct
 */
static void ovp_driver_work_interrupt_gpio_intb_bh(struct work_struct *work)
{
    int ret = 0;
    unsigned char int_sta05h;
    unsigned char id_sta07h;
    struct usb_state usb_sts;

    mutex_lock(&ovp_lock);    /* FUJITSU:2012-11-02 add */

    if (timer_state == 0) {
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s start \n", __func__);
    }

    ovp_state_show_state(&usb_sts);
    if ((ovp_otg_flag == 1) && (usb_sts.state == STATE_NO_CONNECT)) {

        if (atomic_read(&usb_interrupt_otg_flag) == 0) {
            atomic_set(&usb_interrupt_otg_flag, 1);
            printk(KERN_INFO"<%s usb_interrupt_otg_flag 0 -> 1\n",__func__);
        } else {
            atomic_set(&usb_interrupt_otg_flag, 0);
            printk(KERN_INFO"<%s usb_interrupt_otg_flag 1 -> 0 ovp_otg_flag 1 --> 0\n",__func__);
            ovp_otg_flag = 0;
        }

        id_sta07h = OVPDRV_RDCW(ID_STA,&ret);// 07h
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 07H is"
                    " error %d\n", ret);
        }

        int_sta05h = OVPDRV_RDCW(INT_STA,&ret);// 05h
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: als status I2C access to 05H is"
                    " error %d\n", ret);
        }

//        enable_irq(ovp_intb_det_irq); /* FUJITSU:2012-11-02 del */
        wake_unlock(&ovp_wlock);
        mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
        return;
    }
    ret = ovp_driver_interrupt_device();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_driver_interrupt_device error ret is %d\n"
                , __func__, ret);
    }
//    enable_irq(ovp_intb_det_irq);  /* FUJITSU:2012-11-02 del */
    wake_unlock(&ovp_wlock);
    mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
}

/* ----------------------------------------------------------------- */
/*
 * does ovp_driver block hander for gpio_vcdet
 * @param    work_struct 
 */
static void ovp_driver_work_interrupt_gpio_vcdet_bh(struct work_struct *work)
{
    int ret = 0;
    unsigned char int_sta05h;
    unsigned char id_sta07h;

    mutex_lock(&ovp_lock);    /* FUJITSU:2012-11-02 add */
    int_sta05h = OVPDRV_RDCW(INT_STA,&ret);// 05h
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver:%s als status I2C access to 05H is"
                " error %d\n", __func__, ret);
    }

    id_sta07h = OVPDRV_RDCW(ID_STA,&ret);// 07h
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 07H "
            "error is %d \n", ret);
    }

    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s start \n", __func__);
    ret = ovp_driver_interrupt_stand();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_driver_interrupt_stand error ret = %d\n"
                , __func__, ret);
    }

    enable_irq(ovp_vc_det_irq);
    mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
}

/* ----------------------------------------------------------------- */
/*
 * does ovp_driver block hander for gpio_vbdet
 * @param    work_struct 
 */
static void ovp_driver_work_interrupt_gpio_vbdet_bh(struct work_struct *work)
{
    int ret = 0;/* FUJITSU:2013-04-12 add */

    mutex_lock(&ovp_lock);    /* FUJITSU:2012-11-02 add */
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s start \n", __func__);
    /* FUJITSU:2013-04-12 add_start */
    ret = ovp_check_ocp();
    if (ret < 0) {
        printk(KERN_ERR "%s: ovp_check_ocp error is %d\n", __func__, ret);
    }
    /* FUJITSU:2013-04-12 add_end */
    enable_irq(ovp_vb_det_irq);
    mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
}

/* ----------------------------------------------------------------- */
/*
 * does ovp_driver block hander for timer
 * @param    work_struct
 */
static void ovp_driver_work_timer_bh(struct work_struct *work)
{
    unsigned char int_sta05h;
    unsigned char status06h;
    unsigned char id_sta07h;
    
    int ret = 0;

    mutex_lock(&ovp_lock);    /* FUJITSU:2012-11-02 add */
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] ovp_driver_work_timer_bh start \n");

    int_sta05h = OVPDRV_RDCW(INT_STA,&ret);// 05h
    if (ret < 0) {
        printk(KERN_ERR"%s ovp_driver: als status I2C access to 05H error"
            " ret = %d; wake_unlock ovp_wlock_gpio_7\n", __func__, ret);
        wake_unlock(&ovp_wlock_gpio_7);
        mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
        return ;
    }

    status06h = OVPDRV_RDCW(STATUS,&ret);// 06h
    if (ret < 0) {
        printk(KERN_ERR"%s ovp_driver: als status I2C access to 06H error"
            " ret = %d; wake_unlock ovp_wlock_gpio_7\n", __func__, ret);
        wake_unlock(&ovp_wlock_gpio_7);
        mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
        return;
    }

    id_sta07h = OVPDRV_RDCW(ID_STA,&ret);// 07h
    if (ret < 0) {
        printk(KERN_ERR"%s ovp_driver: als status I2C access to 07H error"
            " ret = %d; wake_unlock ovp_wlock_gpio_7\n", __func__, ret);
        wake_unlock(&ovp_wlock_gpio_7);
        mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
        return;
    }

    // check over_current & over_voltage
    if (OVPDRV_MSKC_EQ(ID_STA,IDRDET,UNDET,id_sta07h)) {
        ret = ovp_check_ocp();
        if (ret < 0) {
            printk(KERN_ERR"%s ovp_check_ocp is error; wake_unlock ovp_wlock_gpio_7\n", __func__);
            ovp_init_device_counter();
            wake_unlock(&ovp_wlock_gpio_7);
            mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
            return;
        }
    }

    // USB remove check
    if (OVPDRV_MSKC_EQ(INT_STA,VBUSDET,UNDET,int_sta05h) && OVPDRV_MSKC_EQ(ID_STA,INDO,USB_CLIENT,id_sta07h)) {
        ovp_init_device_counter();
        // remove microUSB device
        printk(KERN_INFO"[OVP_DRIVER] %s call ovp_driver_remove_device() "
            "05h is 0x%02X 07h is 0x%02X\n", __func__, int_sta05h, id_sta07h);

        ret = ovp_driver_remove_device();
        if (ret < 0) {
            printk(KERN_ERR "ovp_driver: ovp_driver_remove_device error"
                    " ret = %d\n"
                    , ret);
        }
        printk(KERN_INFO"%s wake_unlock ovp_wlock_gpio_7\n", __func__);
        wake_unlock(&ovp_wlock_gpio_7);
        mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
        return;
    }
    ovp_state_detect(DETECT_IN_USB_CLIENT, NULL);
    g_id_sta07h = (id_sta07h & OVPDRV_MSKC(ID_STA,INDO));
    ovp_init_device_counter();

    OVP_DBG(KERN_DEBUG"%s wake_unlock ovp_wlock_gpio_7\n", __func__);
    wake_unlock(&ovp_wlock_gpio_7);
    mutex_unlock(&ovp_lock);  /* FUJITSU:2012-11-02 add */
    return;

}

/* ----------------------------------------------------------------- */
/*
 * does ovp_driver block hander for timer
 * @param    wait_time(ms)
 */
static void ovp_update_timer(unsigned long time)
{
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s start \n", __func__);
    queue_work(work_q_msm_timer, &ovp_driver_work_timer);
}

/* ----------------------------------------------------------------- */
/*
 */
static int ovp_open(struct inode *inode, struct file *filp)
{
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s()\n", __func__);
    return 0;
}
static int ovp_release(struct inode *inode, struct file *filp)
{
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s()\n", __func__);
    return 0;
}
static long ovp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    int error = 0;
    int usb_earphone_flag = 0;
    struct usb_state usb_sts;

    ovp_state_show_state(&usb_sts);
    switch ( cmd ) {
        case OVP_USBEARPHONE_PATH_OFF :
            ovp_driver_work_ioctl.value = 1;
            queue_work(work_q_pmic_ioctl, &ovp_driver_work_ioctl.work_xhsdet);
            break;
        case OVP_USBEARPHONE_PATH_ON :
            ovp_driver_work_ioctl.value = 0;
            queue_work(work_q_pmic_ioctl, &ovp_driver_work_ioctl.work_xhsdet);
            break;
        case OVP_USBEARPHONE_GET_STATUS :
            if (usb_sts.state == STATE_STEREO_EARPHONE) {
                usb_earphone_flag = 1;
            }
            error = copy_to_user((int __user *)arg, &usb_earphone_flag, sizeof(int));
            if ( error != 0 ) {
                printk(KERN_ERR "%s: copy_to_user Failed. [%d]\n", __func__, error);
                ret = (-EINVAL);
            }
            break;
        case OVP_ID_GET_STATUS :
            error = copy_to_user((unsigned char __user *)arg, &g_id_sta07h, sizeof(unsigned char));
            if ( error != 0 ) {
                printk(KERN_ERR "%s: copy_to_user Failed. [%d]\n", __func__, error);
                ret = (-EINVAL);
            }
            break;
        default:
            printk(KERN_ERR"%s ioctl cmd error ", __func__);
            ret = (-EINVAL);
    }
    return ret;
}
/* OVP driver FOPS */
static struct file_operations ovp_fops = {
    .unlocked_ioctl = ovp_ioctl,        /* ioctl Entry */
    .open           = ovp_open,         /* open Entry */
    .release        = ovp_release,      /* release Entry */
};
/* driver definition */
static struct miscdevice ovp_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,        /* auto */
    .name = "ovp",                      /* Driver name */
    .fops = &ovp_fops,                  /* FOPS */
};

/* ----------------------------------------------------------------- */
/*
 * remove interface
 * @return 0
 */
static int ovp_driver_remove(struct platform_device *pdev)
{
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s  \n", __func__);
    return 0;
};

/* FUJITSU:2012-09-25 add_start */

/* ----------------------------------------------------------------- */
/*
 * device initialize wait
 * @param    work_struct
 */
static void ovp_driver_work_initialize_wait(struct work_struct *work)
{
    int ret = 0;

    printk(KERN_INFO "ovp_driver: wait_event start \n");
    ret = wait_event_interruptible_timeout(ovp_wq,
                         ovp_initialize_flag == INITIALIZED_MASK,
                         msecs_to_jiffies(OVP_INITIALIZE_WAIT));

    printk(KERN_INFO "ovp_driver: wait_event end ret=%d \n",ret);

    if (ret == 0) {
        printk(KERN_ERR "ovp_driver: wait_event T.O 1min not device=%x initialized!!\n",ovp_initialize_flag);
    }
    
    ret = ovp_driver_set_gpio();
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: ovp_driver_set_gpio() is "
            "error ret = %d\n", ret);
    }

    mutex_lock(&ovp_lock);
    ret = OVPDRV_WRCW(S_RST,S_RST_SOFT_RESET_RESET);//08h <- 0x01
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 08H "
            "error ret = %d\n", ret);
    }

    ret = OVPDRV_UDCB(UCDCNT,USBDETCTL,DIS);//USBDETCTL@02h <- 1
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 02H "
            "error ret = %d\n", ret);
    }

    // accessory detect process start
    disable_irq(ovp_intb_det_irq);

    ret = OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,DEFAULT) |
                            OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 0 bit4 <- 1
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver:01H - ADCEN <- 0  als status I2C access "
            "error ret = %d\n", ret);
    }
    ret = OVPDRV_WRCW(IDDET,OVPDRV_ENCC(IDDET,ADCRETRY,RETRY) |
                            OVPDRV_ENCC(IDDET,IDRDETEN,ENA));//01h  bit1 <- 1 bit4 <- 1
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver:01H - ADCEN <- 1 als status I2C access "
            "error ret = %d\n", ret);
    }

    usleep(30000); /* FUJITSU:2012-12-17 vbus chattering add start */

    ret = ovp_check_device();
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: ovp_check_device is "
            "error ret = %d\n", ret);
    }
    enable_irq(ovp_intb_det_irq);

    ret = ovp_check_stand();
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: ovp_check_stand is "
            "error ret = %d\n", ret);
    }
    mutex_unlock(&ovp_lock);
    /* FUJITSU:2012-11-15 led early-off add */
    if (gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VC_DET)) &&
       gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET))) {
        printk(KERN_INFO "ovp_driver: set fj_battery DISCHARGING\n");
        fj_battery_set_status(POWER_SUPPLY_STATUS_DISCHARGING);
    }
    /* FUJITSU:2012-11-15 led early-off end */
}

/* FUJITSU:2013-02-13 add_start */
/* ----------------------------------------------------------------- */
/*
 * device 
 * @param
 */
static void ovp_driver_work_ioctl_phi35(struct work_struct *work)
{
    mutex_lock(&ovp_lock);
    if (ovp_driver_work_ioctl.value) {
        ovp_state_detect(DETECT_IN_PHI35_EARPHONE, NULL);
    } else {
        ovp_state_detect(DETECT_OUT_PHI35_EARPHONE, NULL);
    }
    mutex_unlock(&ovp_lock);
}
/* FUJITSU:2013-02-13 add_end */

/* ----------------------------------------------------------------- */
/*
 * remove interface
 * @param    device(INITIALIZE_MHL/INITIALIZE_OTG/INITIALIZE_CHG)
 */
void ovp_device_initialized(int device)
{
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s dev = %d\n", __func__, device);

    ovp_initialize_flag = ovp_initialize_flag | device;

    wake_up_interruptible(&ovp_wq);

}
EXPORT_SYMBOL(ovp_device_initialized);
/* FUJITSU:2012-09-25 add_end */

/* ----------------------------------------------------------------- */
/*
 * resume interface
 * @return 0
 */
static int ovp_driver_resume(struct platform_device *pdev)
{
    return 0;
};

/* ----------------------------------------------------------------- */
/*
 * suspend interface
 * @return 0
 */
static int ovp_driver_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    /* FUJITSU:2013-01-07 OVP MHL an examination add_start */
    if (ovp_mhl_examination_flag == 1) {
        OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s examination_flag = OFF \n", __func__);
        ovp_mhl_examination_flag = 0;
    }
    /* FUJITSU:2013-01-07 OVP MHL an examination add_end */
    return 0;
};

/* ----------------------------------------------------------------- */
/*
 * probe interface
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_driver_probe(struct platform_device *pdev)
{
/* FUJITSU:2012-09-25 del_start */
#if 0
    int ret;
    unsigned char data;
    unsigned char ucdcnt02h[READ_SIZE];
#endif
/* FUJITSU:2012-09-25 del_end */
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s start \n", __func__);

    queue_work(work_q_pmic_initialize, &ovp_driver_work_initialize); /* FUJITSU:2012-09-25 add_start */

/* FUJITSU:2012-09-25 del_start */
#if 0
    ret = ovp_driver_set_gpio();
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: ovp_driver_set_gpio() is "
            "error ret = %d\n", ret);
        return ret;
    }

    data = S_RST_SOFT_RESET_1;
    ret = ovp_i2c_write(I2C_OVP_S_RST, 1, &data);     // 08h     <- 0x01
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 08H "
            "error ret = %d\n", ret);
        return ret;
    }

    ret = ovp_i2c_read(I2C_OVP_UCDCNT, 1, ucdcnt02h); // 02h
    ucdcnt02h[0] = (ucdcnt02h[0] | UCDCNT_USBDETCTL_1);
    ret = ovp_i2c_write(I2C_OVP_UCDCNT, 1, ucdcnt02h);     // 02h     <- 0x04
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: als status I2C access to 08H "
            "error ret = %d\n", ret);
        return ret;
    }

    data = IDDET_IDRDETEN_1;
    ret = ovp_i2c_write(I2C_OVP_IDDET, 1, &data); // 01h  bit1 <- 0
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver:01H - ADCEN <- 0  als status I2C access "
            "error ret = %d\n", ret);
        return ret;
    }
    data = (IDDET_ADCRETRY_1 | IDDET_IDRDETEN_1);
    ret = ovp_i2c_write(I2C_OVP_IDDET, 1, &data); // 01h  bit1 <- 1
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver:01H - ADCEN <- 1 als status I2C access "
            "error ret = %d\n", ret);
        return ret;
    }

    // accessory detect process start
    disable_irq(ovp_intb_det_irq);

    ret = ovp_check_device();
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: ovp_check_device is "
            "error ret = %d\n", ret);
        enable_irq(ovp_intb_det_irq);
        return ret;
    }
    enable_irq(ovp_intb_det_irq);

    ret = ovp_check_stand();
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver: ovp_check_stand is "
            "error ret = %d\n", ret);
        return ret;
    }
#endif
/* FUJITSU:2012-09-25 del_end */
    return 0;
};

/* ----------------------------------------------------------------- */
/*
 * ovp_driver initializeation
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int __init ovp_driver_init (void)
{
    int ret = 0;
    timer_state = 0;
    ovp_device_interrupt_count = 0;
    ovp_otg_flag = 0;

    printk(KERN_DEBUG"[OVP_DRIVER] %s start \n", __func__);

    atomic_set(&usb_ocp_flag, 0);
    atomic_set(&stand_ocp_flag, 0);
    atomic_set(&usb_interrupt_otg_flag, 0);

    wake_lock_init(&ovp_wlock, WAKE_LOCK_SUSPEND
            , "ovp_driver_wake_lock");
    wake_lock_init(&ovp_wlock_gpio_7, WAKE_LOCK_SUSPEND
            , "ovp_driver_wake_lock_gpio7");

/* FUJITSU:2012-09-25 add_start */
    init_waitqueue_head(&ovp_wq);
/* FUJITSU:2012-09-25 add_end */

    mutex_init(&ovp_lock); /* FUJITSU:2012-11-02 add */

    work_q_msm_timer = alloc_workqueue("ovp_msm_timer", WQ_FREEZABLE, 0);
    if (!work_q_msm_timer) {
        printk(KERN_ERR"[OVP_DRIVER] %s start \n", __func__);
        ret = -ENOMEM;
        goto out_work_q_timer;
    }

    work_q_pmic_gpio_vcdet = alloc_workqueue("ovp_pmic_gpio_vcdet", WQ_FREEZABLE, 0);
    if (!work_q_pmic_gpio_vcdet) {
        printk(KERN_ERR"[OVP_DRIVER] alloc_workqueue Error \n");
        ret = -ENOMEM;
        goto out_work_q_vcdet;
    }
    work_q_pmic_gpio_vbdet = alloc_workqueue("ovp_pmic_gpio_vbdet", WQ_FREEZABLE, 0);
    if (!work_q_pmic_gpio_vbdet) {
        printk(KERN_ERR"[OVP_DRIVER] alloc_workqueue Error \n");
        ret = -ENOMEM;
        goto out_work_q_vbdet;
    }
    work_q_pmic_gpio_intb = alloc_workqueue("ovp_pmic_gpio_intb", WQ_FREEZABLE, 0);
    if (!work_q_pmic_gpio_intb) {
        ret = -ENOMEM;
        goto out_work_q_intb;
    }
/* FUJITSU:2012-09-25 add_start */
    work_q_pmic_initialize = alloc_workqueue("ovp_pmic_initialize", WQ_FREEZABLE, 0);
    if (!work_q_pmic_initialize) {
        ret = -ENOMEM;
        goto out_work_q_init;
    }
/* FUJITSU:2012-09-25 add_end */
/* FUJITSU:2013-02-13 add_start */
    work_q_pmic_ioctl = alloc_workqueue("ovp_pmic_ioctl", WQ_FREEZABLE, 0);
    if (!work_q_pmic_ioctl) {
        ret = -ENOMEM;
        goto out_work_q_ioctl;
    }
/* FUJITSU:2013-02-13 add_end */

    setup_timer(&ovp_timer, ovp_update_timer, 0);
    INIT_WORK(&ovp_driver_work_timer,
            ovp_driver_work_timer_bh);
    INIT_WORK(&ovp_driver_work_data_pmic_vcdet,
            ovp_driver_work_interrupt_gpio_vcdet_bh);
    INIT_WORK(&ovp_driver_work_data_pmic_vbdet,
            ovp_driver_work_interrupt_gpio_vbdet_bh);
    INIT_WORK(&ovp_driver_work_data_pmic_intb,
            ovp_driver_work_interrupt_gpio_intb_bh);
/* FUJITSU:2012-09-25 add_start */
    INIT_WORK(&ovp_driver_work_initialize,
              ovp_driver_work_initialize_wait);
/* FUJITSU:2012-09-25 add_end */
/* FUJITSU:2013-02-13 add_start */
    INIT_WORK(&ovp_driver_work_ioctl.work_xhsdet,
              ovp_driver_work_ioctl_phi35);
/* FUJITSU:2013-02-13 add_end */

    ret = i2c_add_driver(&ovp_i2c_driver);
    if (ret) {
        printk(KERN_ERR "ovp_driver:%s i2c_add_driver fail\n"
                , __func__);
        return -1;
    }

    ret = platform_driver_register(&switch_usb);
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver:%s platform_driver_register fail\n"
                , __func__);
        return ret;
    }

    ret = misc_register(&ovp_miscdev);
    if (ret < 0) {
        printk(KERN_ERR "ovp_driver:%s misc_register fail\n"
                , __func__);
        return ret;
    }

    if (((system_rev & OVP_FJDEV_MASK) == (OVP_FJDEV001)) || 
        ((system_rev & OVP_FJDEV_MASK) == (OVP_FJDEV002))) {
            fj_earphone_ic_flag = 1;
    }

    return 0;

out_work_q_timer:
    flush_workqueue(work_q_msm_timer);
    destroy_workqueue(work_q_msm_timer);
out_work_q_vcdet:
    flush_workqueue(work_q_pmic_gpio_vcdet);
    destroy_workqueue(work_q_pmic_gpio_vcdet);
out_work_q_vbdet:
    flush_workqueue(work_q_pmic_gpio_vbdet);
    destroy_workqueue(work_q_pmic_gpio_vbdet);
out_work_q_intb:
    flush_workqueue(work_q_pmic_gpio_intb);
    destroy_workqueue(work_q_pmic_gpio_intb);
/* FUJITSU:2012-09-25 add_start */
out_work_q_init:
    flush_workqueue(work_q_pmic_initialize);
    destroy_workqueue(work_q_pmic_initialize);
/* FUJITSU:2012-09-25 add_end */
/* FUJITSU:2013-02-13 add_start */
out_work_q_ioctl:
    flush_workqueue(work_q_pmic_ioctl);
    destroy_workqueue(work_q_pmic_ioctl);
/* FUJITSU:2013-02-13 add_end */
    wake_lock_destroy(&ovp_wlock);
    wake_lock_destroy(&ovp_wlock_gpio_7);
    printk(KERN_ERR "ovp_driver:%s alloc_workqueue fail\n"
            , __func__);
    return ret;
}
module_init(ovp_driver_init);

/* ----------------------------------------------------------------- */
/*
 * ovp_driver shutdown
 * @param   (none)
 * @return  (none)
 */
static void __exit ovp_driver_exit (void)
{
    OVP_DBG(KERN_DEBUG"[OVP_DRIVER] %s start\n", __func__);

    i2c_del_driver(&ovp_i2c_driver);
    // timer clear
    del_timer_sync(&ovp_timer);

    // wake_lock clear
    wake_lock_destroy(&ovp_wlock);
    wake_lock_destroy(&ovp_wlock_gpio_7);

    // work_struct clear
    flush_work(&ovp_driver_work_data_pmic_vcdet);
    flush_workqueue(work_q_pmic_gpio_vcdet);
    destroy_workqueue(work_q_pmic_gpio_vcdet);
    flush_work(&ovp_driver_work_data_pmic_vbdet);
    flush_workqueue(work_q_pmic_gpio_vbdet);
    destroy_workqueue(work_q_pmic_gpio_vbdet);
    flush_work(&ovp_driver_work_data_pmic_intb);
    flush_workqueue(work_q_pmic_gpio_intb);
    destroy_workqueue(work_q_pmic_gpio_intb);
/* FUJITSU:2012-09-25 add_start */
    flush_work(&ovp_driver_work_initialize);
    flush_workqueue(work_q_pmic_initialize);
    destroy_workqueue(work_q_pmic_initialize);
/* FUJITSU:2012-09-25 add_end */
/* FUJITSU:2013-02-13 add_start */
    flush_work(&ovp_driver_work_ioctl.work_xhsdet);
    flush_workqueue(work_q_pmic_ioctl);
    destroy_workqueue(work_q_pmic_ioctl);
/* FUJITSU:2013-02-13 add_end */

    flush_work(&ovp_driver_work_timer);
    flush_workqueue(work_q_msm_timer);
    destroy_workqueue(work_q_msm_timer);

    // request_irq clear
    free_irq(ovp_vc_det_irq, NULL);
    gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VC_DET));
    free_irq(ovp_vb_det_irq, NULL);
    gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_VB_DET));
    free_irq(ovp_intb_det_irq, NULL);
    gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_OVP_INTB_DET));

    misc_deregister(&ovp_miscdev);

    platform_driver_unregister(&switch_usb);

}
module_exit(ovp_driver_exit);

MODULE_ALIAS("platform:ovp_driver");
MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("Fujitsu OVP Driver");
MODULE_LICENSE("GPL");
