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

#ifndef _LINUX_OVP_STATE_H
#define _LINUX_OVP_STATE_H

/* ------------------------------------------------------------------------
 * Constants
 */
#define USB_EVENT_COUNT                     15
#define STAND_EVENT_COUNT                   2
#define STATE_COUNT                         9

#define NO_CARRY_OUT                        -1

#define INTERRUPT_ON                        1
#define INTERRUPT_OFF                       0

#define OVP_STATE_NAME                      "ovp_state"

static char *usb_event_log[USB_EVENT_COUNT] =
{
    "DETECT_IN_USB_CLIENT",
    "DETECT_IN_CABLE_NO_VBUS",
    "DETECT_IN_CABLE_VBUS",
    "DETECT_IN_USB_HOST_MHL",
    "DETECT_IN_STEREO_EARPHONE",
    "DETECT_OUT_USB_CONNECT",
    "CONFIRM_USB_HOST",
    "CONFIRM_MHL",
    "ERROR_MHL",
    "DETECT_OUT_MHL",
    "DETECT_OFF_CHARGER_MHL",
    "DETECT_OUT_OFF_CHARGER_MHL",
    "DETECT_IN_PHI35_EARPHONE",
    "DETECT_OUT_PHI35_EARPHONE",
    "DETECT_IN_USB_UNKNOWN_VBUSON",
};

static char *stand_event_log[STAND_EVENT_COUNT] =
{
    "DETECT_IN_VC",
    "DETECT_OUT_VC",
};

/* ------------------------------------------------------------------------
 * GLOBAL VARIABLES
 */
static struct usb_state str_usb_state;
unsigned int usb_id_info = 0;

/* ------------------------------------------------------------------------
 * FUNCTION PROTOTYPES
 */
static int ovp_state_manage_usb_state(int state);
static int ovp_state_manage_stand_state(int state);
static int ovp_state_set_usb_function(int sw_state, int usb_state_info,
    int interrupt_set);
static int ovp_state_set_stand_function(int stand_state_info);
static int ovp_state_no_action_detect(void);
static int ovp_state_usb_detect(void);
static int ovp_state_usb_unknown_detect(void);
static int ovp_state_mhl_or_host_detect(void);
static int ovp_state_stereo_earphone_detect(void);
static int ovp_state_phi35_earphone_detect(void);
static int ovp_state_usb_and_phi35_earphone_detect(void);
static int ovp_state_phi35_earphone_detect_out(void);
static int ovp_state_usb_and_phi35_earphone_detect_out(void);
static int ovp_state_start_charge(void);
static int ovp_state_end_charge(void);
static int ovp_state_usb_detect_out(void);
static int ovp_state_usb_unknown_detect_out(void);
static int ovp_state_host_or_mhl_detect_out(void);
static int ovp_state_usb_host_detect_out(void);
static int ovp_state_earphone_detect_out(void);
static int ovp_state_usb_host_confirm(void);
static int ovp_state_mhl_confirm(void);
static int ovp_state_mhl_error(void);
static int ovp_state_mhl_detect_out(void);
static int ovp_state_mhl_off_charger_detect(void);
static int ovp_state_mhl_off_charger_detect_out(void);
static int ovp_debug_set_usb(const char *val, struct kernel_param *kp);
static int ovp_debug_get_usb(char *val, struct kernel_param *kp);
/* ------------------------------------------------------------------------
 * Static Function Pointer
 */
// detect event for Charge Stand
static int (*ovp_state_stand_func[STAND_EVENT_COUNT][STAND_EVENT_COUNT])
	(void) =
{
    /* DETECT_IN_VC */
    { ovp_state_start_charge, ovp_state_no_action_detect },
    /* DETECT_OUT_VC */
    { ovp_state_no_action_detect, ovp_state_end_charge }
};

// detect event for microUSB
static int (*ovp_state_usb_func[USB_EVENT_COUNT][STATE_COUNT]) (void) =
{
    /* DETECT_IN_USB_CLIENT  */
    { ovp_state_usb_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_usb_detect,ovp_state_no_action_detect
    }, 
    /* DETECT_IN_CABLE_NO_VBUS */
    { ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect,ovp_state_no_action_detect
    },
    /* DETECT_IN_CABLE_VBUS */
    { ovp_state_usb_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_usb_detect,ovp_state_no_action_detect
    },
    /* DETECT_IN_USB_HOST_MHL */
    { ovp_state_mhl_or_host_detect, ovp_state_mhl_or_host_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_mhl_or_host_detect,ovp_state_no_action_detect
    },
    /* DETECT_IN_STEREO_EARPHONE */
    { ovp_state_stereo_earphone_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_stereo_earphone_detect,ovp_state_no_action_detect
    },
    /* DETECT_OUT_USB_CONNECT */
    { ovp_state_no_action_detect, ovp_state_usb_detect_out, ovp_state_no_action_detect
        , ovp_state_usb_host_detect_out, ovp_state_no_action_detect, ovp_state_earphone_detect_out
        , ovp_state_no_action_detect, ovp_state_usb_detect_out,ovp_state_usb_unknown_detect_out
    },
    /* CONFIRM_USB_HOST */
    { ovp_state_usb_host_confirm, ovp_state_no_action_detect, ovp_state_usb_host_confirm
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_usb_host_confirm,ovp_state_no_action_detect
    },
    /* CONFIRM_MHL */
    { ovp_state_mhl_confirm, ovp_state_no_action_detect, ovp_state_mhl_confirm
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect,ovp_state_no_action_detect
    },
    /* ERROR_MHL */
    { ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_mhl_error
        , ovp_state_no_action_detect, ovp_state_mhl_error, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect,ovp_state_no_action_detect
    },
    /* DETECT_OUT_MHL */
    { ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_host_or_mhl_detect_out
        , ovp_state_no_action_detect, ovp_state_mhl_detect_out, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect,ovp_state_no_action_detect
    },
    /* DETECT_OFF_CHARGER_MHL */
    { ovp_state_mhl_off_charger_detect, ovp_state_mhl_off_charger_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect,ovp_state_no_action_detect
    },
    /* DETECT_OUT_OFF_CHARGER_MHL */
    { ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_mhl_off_charger_detect_out, ovp_state_no_action_detect,ovp_state_no_action_detect
    },
    /* DETECT_IN_PHI35_EARPHONE */
    { ovp_state_phi35_earphone_detect, ovp_state_phi35_earphone_detect, ovp_state_phi35_earphone_detect
        , ovp_state_phi35_earphone_detect, ovp_state_phi35_earphone_detect, ovp_state_usb_and_phi35_earphone_detect
        , ovp_state_phi35_earphone_detect, ovp_state_phi35_earphone_detect,ovp_state_phi35_earphone_detect
    },
    /* DETECT_OUT_PHI35_EARPHONE */
    { ovp_state_phi35_earphone_detect_out, ovp_state_phi35_earphone_detect_out, ovp_state_phi35_earphone_detect_out
        , ovp_state_phi35_earphone_detect_out, ovp_state_phi35_earphone_detect_out, ovp_state_usb_and_phi35_earphone_detect_out
        , ovp_state_phi35_earphone_detect_out, ovp_state_phi35_earphone_detect_out,ovp_state_phi35_earphone_detect_out
    },
    /* DETECT_IN_USB_UNKNOWN_VBUSON */
    { ovp_state_usb_unknown_detect, ovp_state_usb_unknown_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect, ovp_state_no_action_detect
        , ovp_state_no_action_detect, ovp_state_no_action_detect,ovp_state_no_action_detect
    },
};


#endif /* _LINUX_OVP_STATE_H */ 
