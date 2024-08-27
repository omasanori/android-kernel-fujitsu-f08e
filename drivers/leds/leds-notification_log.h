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

#ifndef _LEDS_NOTIFICATION_LOG_H_
#define _LEDS_NOTIFICATION_LOG_H_

/* ------------------------------------------------------------------------ */
/* LOG MACRO                                                                */
/* ------------------------------------------------------------------------ */
#define CONFIG_NOTIFICATIN_GPIOS_LED_DEBUG

#if defined(CONFIG_NOTIFICATIN_GPIOS_LED_DEBUG)
#define	THIS_FILE	"LED driver: "
#define DBG_LOG_LED(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_FUNCENTER(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_FUNC_ENTER) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() Start. " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_FUNCLEAVE(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_FUNC_LEAVE) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() End. " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_LED_SET(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED_SET) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_LED_ARGB(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED_ARGB) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_LED_DELAY(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED_DELAY) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_LED_COLOR(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED_COLOR) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_LED_NOTIFY(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED_NOTIFY) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_LED_SUSPEND(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED_SUSPEND) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_LED_RESUME(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED_RESUME) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_LED_MODEM(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_LED_MODEM) { \
        printk(KERN_INFO THIS_FILE "L%d: %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)

#define DBG_LOG_ANY(fmt, args...) \
    do { if (led_notification_debug_mask & DBG_OPTN_ANY) { \
        printk(KERN_INFO THIS_FILE "@%d %s() " fmt "\n", \
            __LINE__, __FUNCTION__, ## args); \
    } } while (0)
#else
#define DBG_LOG_LED         (fmt, args...)   do {} while (0)
#define DBG_LOG_FUNCENTER   (fmt, args...)   do {} while (0)
#define DBG_LOG_FUNCLEAVE   (fmt, args...)   do {} while (0)
#define DBG_LOG_LED_SET     (fmt, args...)   do {} while (0)
#define DBG_LOG_LED_ARGB    (fmt, args...)   do {} while (0)
#define DBG_LOG_LED_DELAY   (fmt, args...)   do {} while (0)
#define DBG_LOG_LED_COLOR   (fmt, args...)   do {} while (0)
#define DBG_LOG_LED_NOTIFY  (fmt, args...)   do {} while (0)
#define DBG_LOG_LED_SUSPEND (fmt, args...)   do {} while (0)
#define DBG_LOG_LED_RESUME  (fmt, args...)   do {} while (0)
#define DBG_LOG_LED_MODEM   (fmt, args...)   do {} while (0)
#define DBG_LOG_ANY         (fmt, args...)   do {} while (0)
#endif


/* ------------------------------------------------------------------------ */
/* enum                                                                     */
/* ------------------------------------------------------------------------ */
/* for Attributes(Argument type) */
enum {
	DBG_OPTN_LED         = 1 <<   0,
	DBG_OPTN_FUNC_ENTER  = 1 <<   1,
	DBG_OPTN_FUNC_LEAVE  = 1 <<   2,
	DBG_OPTN_LED_SET     = 1 <<   3,
	DBG_OPTN_LED_ARGB    = 1 <<   4,
	DBG_OPTN_LED_DELAY   = 1 <<   5,
	DBG_OPTN_LED_COLOR   = 1 <<   6,
	DBG_OPTN_LED_NOTIFY  = 1 <<   7,
	DBG_OPTN_LED_SUSPEND = 1 <<   8,
	DBG_OPTN_LED_RESUME  = 1 <<   9,
	DBG_OPTN_LED_MODEM   = 1 <<  10,
	DBG_OPTN_ANY         = 1 <<  31,
};

static u32 led_notification_debug_mask
	= DBG_OPTN_LED
//	| DBG_OPTN_FUNC_ENTER
//	| DBG_OPTN_FUNC_LEAVE
//	| DBG_OPTN_LED_SET
//	| DBG_OPTN_LED_ARGB
//	| DBG_OPTN_LED_DELAY
//	| DBG_OPTN_LED_COLOR
//	| DBG_OPTN_LED_NOTIFY
//	| DBG_OPTN_LED_SUSPEND
//	| DBG_OPTN_LED_RESUME
//	| DBG_OPTN_LED_MODEM
//	| DBG_OPTN_ANY
	; /* End */

module_param_named(debug_mask, led_notification_debug_mask, uint, S_IWUSR | S_IRUGO);
#endif // _LEDS_NOTIFICATION_LOG_H_
