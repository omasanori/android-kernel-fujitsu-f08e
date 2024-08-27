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
#include <../arch/arm/mach-msm/include/mach/gpio.h>
#include <asm-generic/uaccess.h>
#include <mach/pmic.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include "linux/mfd/fj_charger.h"
#include <linux/ovp.h>

#include "../arch/arm/include/asm/system.h"
#include "ovp_state.h"
#include "ovp_local.h"

#ifndef OVP_LOCAL_BUILD
#include <linux/mhl/fj_mhl.h>
#include <linux/ftdtv_drv.h>
#include <linux/fta.h>
#endif
#include <linux/switch.h>

extern int ovp_i2c_write(int reg, int bytes, void *src);

/*===========================================================================
  MACROS
  ===========================================================================*/

#define DEBUG_OVP_S
#ifdef DEBUG_OVP_S
#define OVP_STATE_DBG(x...)  printk(x)
#else
#define OVP_STATE_DBG(x...)
#endif

#ifndef OVP_LOCAL_BUILD
    #define FJ_OVP_REC_LOG(x, y...) {\
        char recbuf[128];\
        snprintf(recbuf, sizeof(recbuf), x, ## y);\
        ftadrv_send_str(recbuf);\
    }
#else
    #define FJ_OVP_REC_LOG(x, y...)
#endif

MODULE_PARM_DESC(usb_id_info, "USB State ID");
module_param_call(usb_id_info, ovp_debug_set_usb, ovp_debug_get_usb
    , &usb_id_info, 0644);

enum {
    NO_DEVICE	= 0,
    MSM_HEADSET	= 1,
};

static struct switch_dev sdev;
static int phi35_earphone_flag = 0;

/*===========================================================================
  PLATFORM DRIVER STRUCTURE
  ===========================================================================*/
/*--------------------------------------------------------------------*/
/*
 * device module attribute (device desc.) setting
 */
static struct platform_driver state_usb = {
    .driver = {
        .name   = OVP_STATE_NAME,
        .owner  = THIS_MODULE,
    },
};

/* FUJITSU:2013-04-12 add_start */
/* touch panel driver callback function */
static struct ovp_mhl_state_callback_info ovp_msm_touch_callback;
static bool set_callback_function_flag = false;
/* FUJITSU:2013-04-12 add_end */

/*===========================================================================

  LOCAL FUNCTIONS

  ===========================================================================*/
/* ----------------------------------------------------------------- */
/*
 * It is managed microUSB Connection state.
 * @param           :    USB State
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_state_manage_usb_state(int state)
{
    // Update microUSB connection state that usb_state manage
    if (state >= 0) {
        printk(KERN_INFO"[OVP_STATE] ovp_state_manage_usb_state "
            "USBConnection State Update %d -> %d \n",
            str_usb_state.state, state);
        str_usb_state.state = state;
    } else {
        return -1;
    }

    return 0;
}


/* ----------------------------------------------------------------- */
/*
 * It is managed Charging Stand Connection state.
 * @param           :    Charging Stand State
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_manage_stand_state(int state)
{
    // Update charging Stand connection state that usb_state manage
    if (state >= 0) {
        printk(KERN_INFO"[OVP_STATE] ovp_state_manage_stand_state start:"
            "Charge Stand State Update %d -> %d \n",
            str_usb_state.stand_state, state);
        str_usb_state.stand_state = state;
    } else {
        return -1;
    }

    return 0;
}


/*===========================================================================

    THIS FUNCTIONS IS GATHERING OF FUNCTION

===========================================================================*/
/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of function for ovp_driver API Call & 
 *  manage State of microUSB Connection
 * @param    sw_state       : GPIO_State
 *           usb_state_info : USB_State
 *           interrupt_set  : set value for ovp_set_allow_to_interrupt()
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_set_usb_function(int sw_state, int usb_state_info,
    int interrupt_set)
{
    /*int ret = 0; FUJITSU:2013-04-10 del */

    if (sw_state >= 0) {
        ovp_set_switch(sw_state);
    }
    /* FUJITSU:2013-04-10 del start*/
#if 0
    if (interrupt_set == 0) {
        ret = ovp_set_allow_to_interrupt(false);
    } else if (interrupt_set > 0) {
        ret = ovp_set_allow_to_interrupt(true);
    }
#endif
    /* FUJITSU:2013-04-10 del end*/
    if (usb_state_info >= 0) {
        ovp_state_manage_usb_state(usb_state_info);
    }

    return 0;
}


/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of function for ovp_driver API Call & 
 *  manage State of Charging Stand Connection 
 * @param    stand_state_info       : Charge Stand State
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_set_stand_function(int stand_state_info)
{
    int ret = 0;

    if (stand_state_info >= 0) {
        ret = ovp_state_manage_stand_state(stand_state_info);
    } else {
        ret = -1;
    }

    return 0;
}

/*===========================================================================

    FUNCTIONS THAT OTHER DRIVER'S API CALL 

===========================================================================*/
/* ----------------------------------------------------------------- */
/*
 * It is No Carry out when device detected or removed.
 * @return   0   :    function success
 *          !0   :    function fail
 *
 */
static int ovp_state_no_action_detect(void)
{
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] ovp_state_no_action_detect \n");
/* FUJITSU:2012-06-07 OVP start */
    ovp_set_otg_flag(0);
/* FUJITSU:2012-06-07 OVP end */
    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of functions for USB Driver API Call
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_usb_detect(void)
{
    int interrupt_set;
    int ret = 0;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] ovp_state_usb_detect,DETECT_IN_USB_CLIENT or"
        "DETECT_IN_CABLE_VBUS \n");
    FJ_OVP_REC_LOG("OVP:OVP_STATE_USB_CLIENT_AC\n");

    interrupt_set = NO_CARRY_OUT;
    ovp_state_set_usb_function(SWITCH_STATE_USB_CLIENT, STATE_USB_CLIENT_AC,
        interrupt_set);
    // USB_Driver_API/detect_usb
    ret = msm_otg_enable_irq(0x01);
    if (ret) {
        printk(KERN_ERR"%s:msm_otg_enable_irq(1) is error(%d)  \n"
            , __func__,ret);
    }
    return 0;
}

/* FUJITSU:2013-02-27 OVP defult id sdp charging add_start */
static int ovp_state_usb_unknown_detect(void)
{
    int ret = 0;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s,DETECT_IN_USB_UNKNOWN_VBUSON sdp_charge \n",__func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_USB_UNKNOWN_VBUSON\n");

    if (str_usb_state.state == STATE_USB_CLIENT_AC) {
        // Call Other Driver API
        OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s: Call msm_otg_disable_irq(0x01) \n"
            , __func__);
        ret = msm_otg_disable_irq(0x01);
        if (ret < 0) {
            printk(KERN_ERR"%s:msm_otg_disable_irq(1) is error(%d)  \n"
                 , __func__,ret);
            return ret;
        }
    }
    ovp_state_manage_usb_state(STATE_USB_UNKNOWN_VBUSON);
    fj_chg_other_vbus_draw(FJ_CHG_ENABLE);
    return 0;
}
static int ovp_state_usb_unknown_detect_out(void)
{
    int interrupt_set;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s , DETECT_OUT_USB_CONNECT sdp_charge \n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_NO_CONNECT\n");

    interrupt_set = NO_CARRY_OUT;
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_NO_CONNECT,
        interrupt_set);
    fj_chg_other_vbus_draw(FJ_CHG_DISABLE);
    return 0;
}
/* FUJITSU:2013-02-27 OVP defult id sdp charging add_end */

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of functions for MHL Driver API Call
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_mhl_or_host_detect(void)
{
    int interrupt_set;
//    int ret = 0;
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] ovp_state_mhl_or_host_detect, DETECT_IN_USB_HOST_MHL\n");

    interrupt_set = INTERRUPT_OFF;
    ovp_state_set_usb_function(SWITCH_STATE_MHL, STATE_USB_HOST_OR_MHL,
        interrupt_set);

    // MHL_Driver_API/detect usb_or_mhl
#if 0
    ret = fj_mhl_detect_device(ovp_state_detect, NULL);
    if (ret < 0) {
        printk(KERN_ERR "[OVP_STATE]: %s fj_mhl_detect_device is  error \n"
            , __func__);
    }
    return ret;
#else
    return 0;
#endif
}


/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of functions for Audio Driver API Call
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_stereo_earphone_detect(void)
{
    int interrupt_set;
    
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s,DETECT_IN_STEREO_EARPHONE\n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_USB_EARPHONE\n");

    interrupt_set = NO_CARRY_OUT;
    if (phi35_earphone_flag == 0) {
        ovp_state_set_usb_function(SWITCH_STATE_STEREO_EARPHONE, STATE_STEREO_EARPHONE,
            interrupt_set);
#ifndef OVP_LOCAL_BUILD
        printk(KERN_INFO"[OVP_STATE] %s UsbHeadset Call switch_set_state(1)\n", __func__);
        switch_set_state(&sdev, 0x0001);
#endif
    } else {
        ovp_state_set_usb_function(SWITCH_STATE_PHI35_EARPHONE, STATE_STEREO_EARPHONE,
            interrupt_set);
    }
#ifndef OVP_LOCAL_BUILD
    printk(KERN_INFO"[OVP_STATE] %s dtv_ant_usb_insert_detect() \n", __func__);
    // Call DTV API
    dtv_ant_usb_insert_detect();
#endif
    return 0;
}
static int ovp_state_phi35_earphone_detect(void)
{
    phi35_earphone_flag = 1;
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s,DETECT_IN_PHI35_EARPHONE\n", __func__);
    return 0;
}
static int ovp_state_usb_and_phi35_earphone_detect(void)
{
    phi35_earphone_flag = 1;
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s,DETECT_IN_PHI35_EARPHONE\n", __func__);
/* FUJITSU:2013-03-28 3.5Headset detect */
    printk(KERN_INFO"[OVP_STATE] %s NotUsbHeadset Call switch_set_state(0)\n", __func__);
    switch_set_state(&sdev, 0x0000);
/* FUJITSU:2013-03-28 3.5Headset detect */
    ovp_set_switch(SWITCH_STATE_PHI35_EARPHONE);
    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of functions for Charge Driver API Call
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_start_charge(void)
{
    ovp_state_set_stand_function(STATE_STAND_CONNECT);

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s Call Charge Driver " 
        "fj_chg_holder_vbus_draw(FJ_CHG_ENABLE) \n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_STAND_CONNECT\n");

    // Charge_Driver_API/start_charge
    fj_chg_holder_vbus_draw(FJ_CHG_ENABLE);
    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of functions for Charge Driver API Call
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_end_charge(void)
{
    ovp_state_set_stand_function(STATE_STAND_NO_CONNECT);

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s Call Charge Driver " 
        "fj_chg_holder_vbus_draw(FJ_CHG_DISABLE) \n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_STAND_NO_CONNECT\n");

    // Charge_Driver_API/end_charge
    fj_chg_holder_vbus_draw(FJ_CHG_DISABLE);
    
    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of functions for Other Driver API Call
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_usb_detect_out(void)
{
    int interrupt_set;
    int ret = 0;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s , DETECT_OUT_USB_CONNECT \n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_NO_CONNECT\n");

    interrupt_set = NO_CARRY_OUT;
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_NO_CONNECT,
        interrupt_set);
    // Call Other Driver API
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s: Call msm_otg_disable_irq(0x01) \n"
        , __func__);
    ret = msm_otg_disable_irq(0x01);
    if (ret < 0) {
        printk(KERN_ERR"%s:msm_otg_disable_irq(1) is error(%d)  \n"
            , __func__,ret);
        return ret;
    }

    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is call when Usb-host or mhl is removed
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_host_or_mhl_detect_out(void)
{
    int interrupt_set;

    interrupt_set = INTERRUPT_ON;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s DETECT_OUT_MHL\n", __func__);
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_NO_CONNECT,
        interrupt_set);

    // No API Call

    return 0;

}

/* ----------------------------------------------------------------- */
/*
 *  This function is call when Usb-host is removed
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_usb_host_detect_out(void)
{
    int interrupt_set;
    int ret = 0;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s DETECT_OUT_USB_CONNECT\n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_NO_CONNECT\n");

    interrupt_set = NO_CARRY_OUT;
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_NO_CONNECT,
            interrupt_set);
    OVP_STATE_DBG(KERN_DEBUG"%s call  msm_otg_disable_irq(0x00)\n", __func__);
    ret = msm_otg_disable_irq(0x00);
    if (ret < 0) {
        printk(KERN_ERR"%s:msm_otg_disable_irq(0) is error(%d)  \n"
            , __func__,ret);
        return ret;
    }

    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is call when Earphone is removed
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_state_earphone_detect_out(void)
{
    int interrupt_set;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s,DETECT_OUT_USB_CONNECT\n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_NO_CONNECT\n");

    interrupt_set = NO_CARRY_OUT;
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_NO_CONNECT,
            interrupt_set);

#ifndef OVP_LOCAL_BUILD
    if (phi35_earphone_flag == 0) {
        printk(KERN_INFO"[OVP_STATE] %s NotUsbHeadset Call switch_set_state(0)\n", __func__);
        switch_set_state(&sdev, 0x0000);
    }
    printk(KERN_INFO"[OVP_STATE] %s dtv_ant_usb_remove_detect() \n", __func__);
    // Call DTV API
    dtv_ant_usb_remove_detect();
#endif
    return 0;
}
static int ovp_state_phi35_earphone_detect_out(void)
{
    phi35_earphone_flag = 0;
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s,DETECT_OUT_PHI35_EARPHONE\n", __func__);
    printk(KERN_INFO"[OVP_STATE] %s NotUsbHeadset Call switch_set_state(0)\n", __func__);
    switch_set_state(&sdev, 0x0000);
    return 0;
}
static int ovp_state_usb_and_phi35_earphone_detect_out(void)
{
    phi35_earphone_flag = 0;
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s,DETECT_OUT_PHI35_EARPHONE\n", __func__);
#ifndef OVP_LOCAL_BUILD
    printk(KERN_INFO"[OVP_STATE] %s NotUsbHeadset Call switch_set_state(0)\n", __func__);
    switch_set_state(&sdev, 0x0000);
    printk(KERN_INFO"[OVP_STATE] %s UsbHeadset Call switch_set_state(1)\n", __func__);
    switch_set_state(&sdev, 0x0001);
#endif
    ovp_set_switch(SWITCH_STATE_STEREO_EARPHONE);
    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of functions for USB Driver API Call
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_state_usb_host_confirm(void)
{
    int interrupt_set;
    int ret = 0;
    
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] ovp_state_usb_host_confirm: CONFIRM_USB_HOST \n");
    FJ_OVP_REC_LOG("OVP:OVP_STATE_USB_HOST\n");

    interrupt_set = INTERRUPT_ON;
    ovp_state_set_usb_function(SWITCH_STATE_USB_OTG_HOST, STATE_USB_HOST, interrupt_set);
    // USB_Driver_API/detect_usb_host
    OVP_STATE_DBG(KERN_DEBUG"ovp_state_usb_host_confirm:call msm_otg_enable_irq(0x00) \n");
    ret = msm_otg_enable_irq(0x00);
    if (ret) {
        printk(KERN_ERR"ovp_state_usb_host_confirm:msm_otg_enable_irq(0) is error(%d)  \n",ret);
        return ret;
    }
    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of function for manage state of USB Connection
 *    State
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_state_mhl_confirm(void)
{
#ifndef OVP_LOCAL_BUILD
    int interrupt_set;
    int ret = 0;
#endif

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s ,CONFIRM_MHL  \n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_MHL\n");

    // MHL_Driver_API/detect usb_or_mhl
    
/* FUJITSU:2013-04-11 add_start */
    if (ovp_msm_touch_callback.callback != NULL) {
        OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s touch_callback MHL_DETECT \n", __func__);
        ovp_msm_touch_callback.callback(OVP_MHL_DETECT, ovp_msm_touch_callback.data);
    }
/* FUJITSU:2013-04-11 add_end */

#ifndef OVP_LOCAL_BUILD
    interrupt_set = INTERRUPT_OFF;
    ovp_state_set_usb_function(SWITCH_STATE_MHL, STATE_MHL, interrupt_set);
    ret = fj_mhl_detect_device(ovp_state_detect, NULL);
    if (ret < 0) {
        printk(KERN_ERR "[OVP_STATE]: %s fj_mhl_detect_device is  error \n"
            , __func__);
    }
    return ret;
#else
    return 0;
#endif
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of function for Error MHL
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_state_mhl_error(void)
{
    int interrupt_set;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s ERROR_MHL\n", __func__);
    FJ_OVP_REC_LOG("OVP:ERROR_MHL\n");

/* FUJITSU:2013-04-11 add_start */
    if (ovp_msm_touch_callback.callback != NULL) {
        OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s touch_callback MHL_DETECT_OUT \n", __func__);
        ovp_msm_touch_callback.callback(OVP_MHL_DETECT_OUT, ovp_msm_touch_callback.data);
    }
/* FUJITSU:2013-04-11 add_end */

    interrupt_set = INTERRUPT_ON;
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_NO_CONNECT,
        interrupt_set);

    // No API Call

    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of function for MHL is removed
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_state_mhl_detect_out(void)
{
    int interrupt_set;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s DETECT_OUT_MHL\n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_NO_CONNECT\n");

/* FUJITSU:2013-04-11 add_start */
    if (ovp_msm_touch_callback.callback != NULL) {
        OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s touch_callback MHL_DETECT_OUT \n", __func__);
        ovp_msm_touch_callback.callback(OVP_MHL_DETECT_OUT, ovp_msm_touch_callback.data);
    }
/* FUJITSU:2013-04-11 add_end */

    interrupt_set = INTERRUPT_ON;
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_NO_CONNECT,
        interrupt_set);
    // No API Call

    return 0;
}

/* FUJITSU:2012-04-23 OVP start */
/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of function for MHL OFF Charger Mode
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_state_mhl_off_charger_detect(void)
{
    int interrupt_set;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s DETECT_OFF_CHARGER_MHL\n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_OFF_CHARGER_MHL\n");

    interrupt_set = NO_CARRY_OUT;
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_OFF_CHARGER_MHL,
        interrupt_set);
    // Charger Driver API Call
    fj_chg_mhl_vbus_draw(FJ_CHG_ENABLE);
    return 0;
}

/* ----------------------------------------------------------------- */
/*
 *  This function is gathering of function for MHL OFF Charger Mode is removed
 * @return   0   :    function success
 *          !0   :    function fail 
 */
static int ovp_state_mhl_off_charger_detect_out(void)
{
    int interrupt_set;

    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s DETECT_OUT_OFF_CHARGER_MHL\n", __func__);
    FJ_OVP_REC_LOG("OVP:OVP_STATE_NO_CONNECT\n");

    interrupt_set = NO_CARRY_OUT;
    ovp_state_set_usb_function(SWITCH_STATE_NO_CONNECT, STATE_NO_CONNECT,
        interrupt_set);
    // Charger Driver API Call
    fj_chg_mhl_vbus_draw(FJ_CHG_DISABLE);
    return 0;
}

/* FUJITSU:2012-04-23 OVP end */
/* ----------------------------------------------------------------- */
/*
 * It is detected microUSB connection when device detected or removed.
 * @param   event: USB event
 *          *arg : none
 * @return   0   :    function success
 *          !0   :    function fail 
 */
int ovp_state_detect(int event, void *arg)
{
    int ret = 0;

    event -= 1;
/* FUJITSU:2012-04-23 OVP start */
    if ((event >= 0) && (event < USB_EVENT_COUNT)) {
/* FUJITSU:2012-04-23 OVP end */
        OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] ovp_state_detect start: event is %s state is %d\n"
            , usb_event_log[event]
            , str_usb_state.state);
        (*ovp_state_usb_func[event][str_usb_state.state]) ();
    } else {
        printk(KERN_ERR "[OVP_STATE] %s unknown event is %d \n"
            , __func__, event);
        ret = -1;
    }

    return ret;
}
EXPORT_SYMBOL(ovp_state_detect);

/* ----------------------------------------------------------------- */
/*
 * It is detected microUSB connection for ChargeDriver when device 
 * detected or removed.
 * @param   event: Stand event
 *          *arg : none
 * @return   0   :    function success
 *          !0   :    function fail
 */
int ovp_state_detect_stand(unsigned int event)
{
    int ret = 0;
    
    event -= 1;
/* FUJITSU:2012-04-23 OVP start */
    if (event < STAND_EVENT_COUNT) {
/* FUJITSU:2012-04-23 OVP end */
        OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] ovp_state_detect_stand start:event is %s state is %d\n"
                , stand_event_log[event]
                , str_usb_state.stand_state);
        (*ovp_state_stand_func[event][str_usb_state.stand_state]) ();
    } else {
        ret = -1;
        printk(KERN_ERR "[OVP_STATE] %s"
            "unknown event is %d \n", __func__, event);
        return ret;
    }

    return ret;
}
EXPORT_SYMBOL(ovp_state_detect_stand);


/* ----------------------------------------------------------------- */
/*
 * ovp_state provide microUSB connection state that this driver manage
 * @param   *str_state: Usb State Info
 * @return   0   :    function success
 *          !0   :    function fail
 */
int ovp_state_show_state(struct usb_state *str_state)
{
    memcpy(str_state, &str_usb_state, sizeof(str_usb_state));
    return 0;
}
EXPORT_SYMBOL(ovp_state_show_state);

/* ----------------------------------------------------------------- */
/*
 */
static ssize_t msm_usbheadset_print_name(struct switch_dev *sdev_t, char *buf)
{
    switch (switch_get_state(&sdev)) 
    {
    case NO_DEVICE:
        printk(KERN_INFO "[OVP_STATE] %s No Device \n", __func__);
        return sprintf(buf, "No Device\n");
    case MSM_HEADSET:
        printk(KERN_INFO "[OVP_STATE] %s UsbHeadset \n", __func__);
        return sprintf(buf, "UsbHeadset\n");
    }
    return -EINVAL;
}
/*===========================================================================

  DEBUG FUNCTIONS

  ===========================================================================*/
/* ----------------------------------------------------------------- */
/*
 * Change USB State for Debug
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_debug_set_usb(const char *val, struct kernel_param *kp)
{
    int ret = 0;
    int event = -1;
    char *e;
    event = simple_strtoul(val, &e, 10);

    if ((event >= 1) && (event <= USB_EVENT_COUNT)) {
        printk(KERN_DEBUG"[OVP_STATE] %s start: event is %s state is %d\n"
            , __func__, usb_event_log[event - 1]
            , str_usb_state.state);
    } else {
        printk(KERN_ERR "[OVP_STATE] %s unknown event is %d \n"
            , __func__, event);
        ret = -1;
        return ret;
    }
    if (str_usb_state.state == STATE_USB_HOST) {
        ovp_set_otg_flag(1);
    }
    (*ovp_state_usb_func[event - 1][str_usb_state.state]) ();

    return 0;
}


/* ----------------------------------------------------------------- */
/*
 * Show USB State for Debug
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int ovp_debug_get_usb(char *val, struct kernel_param *kp)
{
    usb_id_info = str_usb_state.state;
    return sprintf(val, "%x", usb_id_info);
}

/* FUJITSU:2013-04-11 add_start */
static int ovp_set_callback_info(struct ovp_mhl_state_callback_info *info)
{
    int result = 0;
    
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] ovp_set_callback_info \n");
    if (info->callback == NULL) {
        printk("[%s] set callback function NULL pointar\n",  __func__);
        result = -1;
    } else if (set_callback_function_flag == 1) {
        printk("[%s] already callback function\n",  __func__);
        result = -1;
    } else {
        ovp_msm_touch_callback.callback = info->callback;
        ovp_msm_touch_callback.data     = info->data;
        set_callback_function_flag = 1;
        result = 0;
    }
    
    return result;
}

static int ovp_unset_callback_info(void)
{
    int result = 0;
    
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] ovp_unset_callback_info \n");
    ovp_msm_touch_callback.callback = NULL;
    ovp_msm_touch_callback.data     = NULL;
    set_callback_function_flag = 0;

    return result;
}

int ovp_set_mhl_state_callback_info(struct ovp_mhl_state_callback_info *info)
{
    int result = 0;
    
    result = ovp_set_callback_info(info);

    return result;
}
EXPORT_SYMBOL(ovp_set_mhl_state_callback_info);

int ovp_unset_mhl_state_callback_info(void)
{
    int result = 0;
    
    result = ovp_unset_callback_info();

    return result;
}
EXPORT_SYMBOL(ovp_unset_mhl_state_callback_info);
/* FUJITSU:2013-04-11 add_end */

/*===========================================================================

  INIT AND EXIT FUNCTIONS

  ===========================================================================*/
/* ----------------------------------------------------------------- */
/*
 * ovp_state initializeation
 * @return   0   :    function success
 *          !0   :    function fail
 */
static int __init ovp_state_init (void)
{
    int ret = 0;
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s \n", __func__);

    str_usb_state.state = STATE_NO_CONNECT;
    str_usb_state.stand_state = STATE_STAND_NO_CONNECT;

    ret = platform_driver_register(&state_usb);
    if (ret < 0) {
        printk(KERN_ERR "[OVP_STATE] %s platform_driver_register() fail \n"
                , __func__);
        return ret;
    }
    // ******* switch file Initial Setting Start
    sdev.name	= "usb_h2w";
    sdev.print_name = msm_usbheadset_print_name;
    ret = switch_dev_register(&sdev);
    if (ret)
    {
        printk(KERN_ERR "[OVP_STATE] %s switch_dev_register() fail \n", __func__);
        return ret;
    }
    printk(KERN_INFO"[OVP_STATE] %s NotUsbHeadset Call switch_set_state(0)\n", __func__);
    switch_set_state(&sdev, 0x0000);
    // ******* switch file Initial Setting End
    return 0;

}
module_init(ovp_state_init);


/* ----------------------------------------------------------------- */
/*
 * ovp_state shutdown
 * @return : none
 */
static void __exit ovp_state_exit (void)
{
    OVP_STATE_DBG(KERN_DEBUG"[OVP_STATE] %s \n", __func__);

    switch_dev_unregister(&sdev);
    platform_driver_unregister(&state_usb);
}
module_exit(ovp_state_exit);

MODULE_ALIAS("platform:ovp_state");
MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("Fujitsu OVP State Driver");
MODULE_LICENSE("GPL");
