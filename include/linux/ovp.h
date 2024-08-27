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

#ifndef __LINUX_OVP_H__
#define __LINUX_OVP_H__

#include <linux/ioctl.h>

/* ------------------------------------------------------------------------
 * Constants
 */
#define OVP_USBEARPHONE_PATH_OFF        _IOW('w', 0x01, int)
#define OVP_USBEARPHONE_PATH_ON         _IOW('w', 0x02, int)
#define OVP_USBEARPHONE_GET_STATUS      _IOW('w', 0x03, int)
#define OVP_ID_GET_STATUS               _IOW('w', 0x04, int)

/*  Ovp_Driver   Switch_State */ 

#define SWITCH_STATE_NO_CONNECT         0
#define SWITCH_STATE_USB_CLIENT         1
#define SWITCH_STATE_USB_OTG_HOST       2
#define SWITCH_STATE_MHL                3
#define SWITCH_STATE_STEREO_EARPHONE    4
#define SWITCH_STATE_PHI35_EARPHONE     5

/*  Ovp_State : microUSB Connection State  */
#define STATE_NO_CONNECT                0
#define STATE_USB_CLIENT_AC             1
#define STATE_USB_HOST_OR_MHL           2
#define STATE_USB_HOST                  3
#define STATE_MHL                       4
#define STATE_STEREO_EARPHONE           5
#define STATE_OFF_CHARGER_MHL           6
#define STATE_USB_HOST_INVESTIGATION    7
#define STATE_USB_UNKNOWN_VBUSON        8

/*  Ovp_State : Stand for Charge Connection State  */
#define STATE_STAND_NO_CONNECT          0
#define STATE_STAND_CONNECT             1

/*  Ovp_State : microUSB Event when device detected or removed  */
#define DETECT_IN_USB_CLIENT            1
#define DETECT_IN_CABLE_NO_VBUS         2
#define DETECT_IN_CABLE_VBUS            3
#define DETECT_IN_USB_HOST_MHL          4
#define DETECT_IN_STEREO_EARPHONE       5
#define DETECT_OUT_USB_CONNECT          6
#define CONFIRM_USB_HOST                7
#define CONFIRM_MHL                     8
#define ERROR_MHL                       9
#define DETECT_OUT_MHL                  10
#define DETECT_OFF_CHARGER_MHL          11
#define DETECT_OUT_OFF_CHARGER_MHL      12
#define DETECT_IN_PHI35_EARPHONE        13
#define DETECT_OUT_PHI35_EARPHONE       14
#define DETECT_IN_USB_UNKNOWN_VBUSON    15

/*  Ovp_State : Charge Stand Event when device detected or removed  */
#define DETECT_IN_VC                    1
#define DETECT_OUT_VC                   2

/* FUJITSU:2012-09-25 add_start */
#define INITIALIZE_MHL                  0x01
#define INITIALIZE_OTG                  0x02
#define INITIALIZE_CHG                  0x04
#define INITIALIZE_SUPPLY               0x08    /* FUJITSU:2012-11-15 led early-off add */

/* FUJITSU:2012-09-25 add_end */

/* ------------------------------------------------------------------------
 * Struct
 */
struct usb_state {
    int state;
    int stand_state;
};


/* ------------------------------------------------------------------------
 * Extern Functions for Usb
 */
/* from msm_otg */
extern int msm_otg_disable_irq(int pType);
extern int msm_otg_enable_irq(int pType);

/* ------------------------------------------------------------------------
 * Extern Functions
 */

/*  Ovp_Driver    */ 
extern int ovp_set_allow_to_interrupt(bool permission);
extern int ovp_set_switch(int state);
extern int ovp_set_for_usb_otg_host(int set_value);
extern int ovp_set_for_earphone(int set_value);
extern int ovp_occure_interrupt(void);
extern int ovp_set_otg_flag(int flag);

/* FUJITSU:2012-09-25 add_start */
extern void ovp_device_initialized(int device);
/* FUJITSU:2012-09-25 add_end */

/*  Ovp_State     */
extern int ovp_state_detect(int event, void *arg);
extern int ovp_state_detect_stand(unsigned int event);
extern int ovp_state_show_state(struct usb_state *str_state);

/* FUJITSU:2013-04-11 add_start */
struct ovp_mhl_state_callback_info{
    int (*callback)(unsigned int mhl_state, void *data);   /* callback function */
    void *data;                                            /* touch senser data */
};

extern int ovp_set_mhl_state_callback_info(struct ovp_mhl_state_callback_info *info);
extern int ovp_unset_mhl_state_callback_info(void);
#define OVP_MHL_DETECT_OUT  0
#define OVP_MHL_DETECT      1
/* FUJITSU:2013-04-11 add_start */

#endif /* __LINUX_OVP_H__ */
