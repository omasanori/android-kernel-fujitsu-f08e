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

#ifndef _LEDS_NOTIFICATION_H_
#define _LEDS_NOTIFICATION_H_

/* ------------------------------------------------------------------------ */
/* define                                                                   */
/* ------------------------------------------------------------------------ */
#define FAMILY_NAME "ledctrld"

/* FUJITSU:2012.08.29 LEDCTRLD_CRYPTO Add Start */
// -> AES_KEYSIZE_128 in "kernel/include/crypto/aes.h"
#define LEDCTRLD_AES_KEYSIZE (16)
/* FUJITSU:2012.08.29 LEDCTRLD_CRYPTO Add Enf */

/* ------------------------------------------------------------------------ */
/* enum                                                                     */
/* ------------------------------------------------------------------------ */
/* for Attributes(Argument type) */
enum {
    LEDCTRLD_A_UNSPEC,      //
/* FUJITSU:2012.08.29 LEDCTRLD_CRYPTO Add Start */
    LEDCTRLD_A_INIT,        // crypto key (16bytes - unsigned char array)
/* FUJITSU:2012.08.29 LEDCTRLD_CRYPTO Add End */
    LEDCTRLD_A_SEND_MODEM,  // ledctrld_led_info
    LEDCTRLD_A_RESPONSE,    // int
    __LEDCTRLD_A_MAX,
};
#define LEDCTRLD_A_MAX (__LEDCTRLD_A_MAX - 1)

/* for Communications(Command/Message) */
enum {
    LEDCTRLD_C_UNSPEC,
    LEDCTRLD_C_INIT,        // Initialize LED driver
    LEDCTRLD_C_SEND_MODEM,  // LED driver -> "ledctrld" -> Modem
    LEDCTRLD_C_RESPONSE,    // LED driver <- "ledctrld" <- Modem
    __LEDCTRLD_C_MAX,
};
#define LEDCTRLD_C_MAX (__LEDCTRLD_C_MAX - 1)

/* ------------------------------------------------------------------------ */
/* struct                                                                   */
/* ------------------------------------------------------------------------ */
/* led control information */
typedef struct {
    unsigned long   brightness;
    unsigned long   flashOnMS;
    unsigned long   flashOffMS;
} led_control_info_type;

typedef struct {
    unsigned long         aarm_lock;
    unsigned long         marm_lock;
    led_control_info_type info[2];
    unsigned long         isupdate;
    unsigned long         tcxo_led_enable;
} oem_led_control_type;

typedef struct {
    int num;
    led_control_info_type info[2];
} ledctrld_led_info;

/* FUJITSU:2012.08.29 LEDCTRLD_CRYPTO Add Start */
typedef union {
    struct {
        unsigned char         crypto_key[LEDCTRLD_AES_KEYSIZE];
        union {
            ledctrld_led_info info;
            int               response;
        } content;
    } s;
    /* for crypto block boundary (enough to large size) */
    char dummy[16*16]; // = AES_BLOCK_SIZE(16)*16
} ledctrld_msg_content;
/* FUJITSU:2012.08.29 LEDCTRLD_CRYPTO Add End */

#endif // _LEDS_NOTIFICATION_H_

