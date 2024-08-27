/*
 * Copyright(C) 2012 - 2013 FUJITSU LIMITED
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
/*
 * notification LED driver for GPIOs
 *
 * Base on leds-gpio.c, ledtrig-timer.c
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/syscalls.h>
#include <linux/nonvolatile_common.h>
#include <crypto/aes.h>
#include <mach/rpm.h>
#include <net/genetlink.h>
#include "../../arch/arm/mach-msm/smd_private.h"
#include "../../arch/arm/include/asm/system.h"
#include "leds.h"

#include "leds-notification.h"
#include "leds-notification_hw.h"
#include "leds-notification_log.h"


/* ------------------------------------------------------------------------ */
/*  Three LED                                                               */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/*  define                                                                   */
/* ------------------------------------------------------------------------ */
#define LED_DRIVER_NAME "notification"

/* non-volatile */
#define APNV_LED_SUSPEND_MODE_I    41055
#define APNV_SIZE_LED_SUSPEND_MODE     1

/* Notify type */
#define NOTIFY_TYPE_NONE       0x00000000
#define NOTIFY_TYPE_UNREAD     0x00000001
#define NOTIFY_TYPE_NOT_FAITH  0x00000002
#define NOTIFY_TYPE_ANSWERING  0x00000004
#define NOTIFY_TYPE_3RD_APL    0x00000008
#define NOTIFY_TYPE_GMAIL      0x00000010
#define NOTIFY_TYPE_MSG_UPDATE 0x00000020
#define NOTIFY_TYPE_NEW_MSG    0x00000040
#define NOTIFY_TYPE_ALARM      0x00000080
#define NOTIFY_TYPE_PREVIEW    0x00000100

/* Charge type */
#define CHARGE_TYPE_NONE       0x00000000
#define CHARGE_TYPE_CHARGING   0x00000001
#define CHARGE_TYPE_ERROR      0x00000002

/* Color type */
#define LED_COLOR_RED          0x00ff0000
#define LED_COLOR_GREEN        0x0000ff00
#define LED_COLOR_BLUE         0x000000ff

/* /Timer */
#define LED_BLINK_OFF_TIME       50
#define LED_SUSPEND_ON_TIME     300
#define LED_SUSPEND_OFF_TIME   9700

/* Led status */
#define LED_STATUS_NORMAL      0
#define LED_STATUS_SUSPEND     1

/* OffCharge Mode Flg*/
extern int charging_mode;
/* ------------------------------------------------------------------------ */
/* struct                                                                   */
/* ------------------------------------------------------------------------ */
struct notification_led_data {
	struct led_classdev cdev;
	unsigned int brightness_on;
	u32 argb;
	unsigned long delay_on;
	unsigned long delay_off;
	struct timer_list timer;
};

struct led_color_type
{
	uint8_t led_alpha;
	uint8_t led_red;
	uint8_t led_green;
	uint8_t led_blue;
	unsigned int on_time;
	unsigned int off_time;
};

/* ------------------------------------------------------------------------ */
/* global                                                                   */
/* ------------------------------------------------------------------------ */
int g_first_blink_flg = 0;
int g_chrg_count = 0;
int g_led_status;
struct led_color_type g_led_color;
led_control_info_type g_led_control_info_type[2];

/* ------------------------------------------------------------------------ */
/* static                                                                   */
/* ------------------------------------------------------------------------ */
static int led_notify = 0;
static int led_charge = 0;
static unsigned int tcxo_flg;


/* ------------------------------------------------------------------------ */
/*  ledctrld                                                                */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* define                                                                   */
/* ------------------------------------------------------------------------ */
#define CMDLINE_PATH    "/proc/%d/cmdline"
#define LEDCTRLD_NAME   "/system/xbin/ledctrld"
#define CRYPTO_ALG_NAME "ecb(aes)"
#define AES_ENCRYPT     1
#define AES_DECRYPT     2
#define LEDCTRLD_SEND_MODEM_TIMEOUT_MS (10 * 1000)

/* ------------------------------------------------------------------------ */
/* Prototype Func                                                           */
/* ------------------------------------------------------------------------ */
static int notification_led_if_ledctrld_init(struct sk_buff* skb_2, struct genl_info* info);
static int notification_led_if_ledctrld_response(struct sk_buff* skb_2, struct genl_info* info);
static int notification_led_if_ledctrld_send_modem(led_control_info_type* led_data, int led_data_length);

/* ------------------------------------------------------------------------ */
/* static                                                                   */
/* ------------------------------------------------------------------------ */
/* Parameters for "ledctrld" (Initialize in ledctrld_init()) */
static struct net*       sNet = NULL;
static int               sPid = 0;
static struct completion sSendCompletion;
static unsigned char        sCryptoKey[LEDCTRLD_AES_KEYSIZE];
static ledctrld_msg_content sCryptoBuf[2];

static struct mutex led_waitlock;

/* Type for Attributes(Argument type) */
static struct nla_policy ledctrld_genl_policy[LEDCTRLD_A_MAX + 1] = {
    [LEDCTRLD_A_INIT]       = { .type = NLA_BINARY },
    [LEDCTRLD_A_SEND_MODEM] = { .type = NLA_BINARY },
    [LEDCTRLD_A_RESPONSE]   = { .type = NLA_BINARY },
};

/* family definition */
static struct genl_family ledctrld_gnl_family = {
    .id      = GENL_ID_GENERATE, /* genetlink should generate an id */
    .hdrsize = 0,
    .name    = FAMILY_NAME,      /* the name of this family, used by userspace application */
    .version = 1,                /* version number */
    .maxattr = LEDCTRLD_A_MAX,
};

/* Table of communications(Command/Message) */
static struct genl_ops ledctrld_gnl_ops[] = {
    {
        .cmd    = LEDCTRLD_C_INIT,
        .flags  = 0,
        .policy = ledctrld_genl_policy,
        .doit   = notification_led_if_ledctrld_init,
        .dumpit = NULL,
    },
    {
        .cmd    = LEDCTRLD_C_RESPONSE,
        .flags  = 0,
        .policy = ledctrld_genl_policy,
        .doit   = notification_led_if_ledctrld_response,
        .dumpit = NULL,
    },
};

/* ------------------------------------------------------------------------ */
/* [Function] : LED                                                         */
/* ------------------------------------------------------------------------ */
static int notification_led_if_ledctrld_check_cmd_executor(void)
{
    int          rcode      = 0;
    int          err        = 0;
    char         procfs[32] = "";
    struct file* fp         = NULL;
    char executor[sizeof(LEDCTRLD_NAME)+1];

    DBG_LOG_FUNCENTER("");

    do {
        sprintf(procfs, CMDLINE_PATH, current->pid);

        fp = filp_open(procfs, O_RDONLY, 0);
        err = IS_ERR(fp);
        if (err) {
            DBG_LOG_LED("open failed(%s)\n", procfs);
            rcode = -1;
            break;
        }

        err = kernel_read(fp, 0, executor, sizeof(executor));
        if (err <= 0) {
            DBG_LOG_LED("read failed(%d)\n", err);
            rcode = -2;
            break;
        }

        err = strncmp(LEDCTRLD_NAME, executor, sizeof(executor));
        if (err != 0) {
            DBG_LOG_LED("bad cmd executor");
            rcode = 1;
            break;
        }
    } while(0);

    if (fp != NULL) {
        filp_close(fp, NULL);
    }

    DBG_LOG_FUNCLEAVE("rcode=%d", rcode);
    return rcode;
}

/* ENCRYPT: SRC=sCryptoBuf[0], DST=sCryptoBuf[1], KEY=sCryptoKey */
static int notification_led_if_ledctrld_encrypt(void)
{
    int                      rcode = 0;
    int                      err   = 0;
    struct crypto_blkcipher* tfm   = NULL;
    struct scatterlist       sg_src;
    struct scatterlist       sg_dst;
    struct blkcipher_desc    desc;

    DBG_LOG_FUNCENTER("");

    do {
        /* add "authorization-key" */
        memcpy(sCryptoBuf[0].s.crypto_key, sCryptoKey, LEDCTRLD_AES_KEYSIZE);

        tfm = crypto_alloc_blkcipher(CRYPTO_ALG_NAME, 0, CRYPTO_ALG_ASYNC);
        err = IS_ERR(tfm);
        if (err) {
            DBG_LOG_LED("crypto_alloc_blkcipher()=%ld", PTR_ERR(tfm));
            rcode = -1;
            tfm = NULL;
            break;
        }
        desc.tfm = tfm;

        err = crypto_blkcipher_setkey(tfm, sCryptoKey, LEDCTRLD_AES_KEYSIZE);
        if (err < 0) {
            DBG_LOG_LED("crypto_blkcipher_setkey()=%d", err);
            rcode = -2;
            break;
        }

        sg_init_one(&sg_src, &sCryptoBuf[0], sizeof(sCryptoBuf[0]));
        sg_init_one(&sg_dst, &sCryptoBuf[1], sizeof(sCryptoBuf[1]));

        /* encrypt */
        err = crypto_blkcipher_encrypt(&desc, &sg_dst, &sg_src, sizeof(sCryptoBuf[0]));
        if (err < 0) {
            DBG_LOG_LED("crypto_blkcipher_encrypt()=%d", err);
            rcode = -3;
            break;
        }
    } while(0);

    /* free */
    if (tfm != NULL) {
        crypto_free_blkcipher(tfm);
    }

    DBG_LOG_FUNCLEAVE("rcode=%d", rcode);
    return rcode;
}

/* DECRYPT: SRC=sCryptoBuf[0], DST=sCryptoBuf[1], KEY=sCryptoKey */
static int notification_led_if_ledctrld_decrypt(void)
{
    int                      rcode = 0;
    int                      err   = 0;
    struct crypto_blkcipher* tfm   = NULL;
    struct scatterlist       sg_src;
    struct scatterlist       sg_dst;
    struct blkcipher_desc    desc;

    DBG_LOG_FUNCENTER("");

    do {
        tfm = crypto_alloc_blkcipher(CRYPTO_ALG_NAME, 0, CRYPTO_ALG_ASYNC);
        err = IS_ERR(tfm);
        if (err) {
            DBG_LOG_LED("crypto_alloc_blkcipher()=%ld", PTR_ERR(tfm));
            rcode = -1;
            tfm = NULL;
            break;
        }
        desc.tfm = tfm;

        err = crypto_blkcipher_setkey(tfm, sCryptoKey, LEDCTRLD_AES_KEYSIZE);
        if (err < 0) {
            DBG_LOG_LED("crypto_blkcipher_setkey()=%d", err);
            rcode = -2;
            break;
        }

        sg_init_one(&sg_src, &sCryptoBuf[0], sizeof(sCryptoBuf[0]));
        sg_init_one(&sg_dst, &sCryptoBuf[1], sizeof(sCryptoBuf[1]));

        /* decrypt */
        err = crypto_blkcipher_decrypt(&desc, &sg_dst, &sg_src, sizeof(sCryptoBuf[0]));
        if (err < 0) {
            DBG_LOG_LED("crypto_blkcipher_decrypt()=%d", err);
            rcode = -3;
            break;
        }

        /* check "authorization-key" */
        err = memcmp(sCryptoKey, sCryptoBuf[1].s.crypto_key, LEDCTRLD_AES_KEYSIZE);
        if (err != 0) {
            DBG_LOG_LED("authorization failed");
            rcode = -4;
            break;
        }
    } while(0);

    /* free */
    if (tfm != NULL) {
        crypto_free_blkcipher(tfm);
    }

    DBG_LOG_FUNCLEAVE("rcode=%d", rcode);
    return rcode;
}

/* Initialize LED driver. */
static int notification_led_if_ledctrld_init(struct sk_buff* skb_2, struct genl_info* info)
{
    struct nlattr* attr;
    unsigned char* dat;
    int err, len;

    DBG_LOG_FUNCENTER("skb_2=%p, info=%p", skb_2, info);

    err = notification_led_if_ledctrld_check_cmd_executor();
    if (err != 0) {
        DBG_LOG_LED("bad cmd executor: err=%d", err);
        DBG_LOG_FUNCLEAVE("bad cmd executor: err=%d", err);
        return -1;
    }

    /* CHECK: arguments */
    if (info == NULL) {
        DBG_LOG_LED("invalid argument. [skb_2=%p, info=%p]", skb_2, info);
        DBG_LOG_FUNCLEAVE("invalid argument. [skb_2=%p, info=%p]", skb_2, info);
        return -1;
    }

    /* Initialize */
    sNet = genl_info_net(info);
    sPid = info->snd_pid;
    init_completion(&sSendCompletion);

    /* Get crypto_key from "ledctrld" */
    attr = info->attrs[LEDCTRLD_A_INIT];
    if (attr == NULL) {
        DBG_LOG_LED("ledctrld: attr=NULL");
        DBG_LOG_FUNCLEAVE("ledctrld: attr=NULL");
        return -1;
    }
    dat = (unsigned char*)nla_data(attr);
    len = nla_len(attr);
    if (dat == NULL || len != LEDCTRLD_AES_KEYSIZE) {
        DBG_LOG_LED("ledctrld: dat=%p, len=%d, LEDCTRLD_AES_KEYSIZE=%d",
            dat, len, LEDCTRLD_AES_KEYSIZE);
        DBG_LOG_FUNCLEAVE("ledctrld: dat=%p, len=%d, LEDCTRLD_AES_KEYSIZE=%d",
            dat, len, LEDCTRLD_AES_KEYSIZE);
        return -1;
    }
    memcpy(sCryptoKey, dat, len);

    DBG_LOG_FUNCLEAVE("");
    return 0;
}

/* Send control information (LED driver -> "ledctrld" -> Modem) */
static int notification_led_if_ledctrld_send_modem(led_control_info_type* led_data, int led_data_length)
{
    ledctrld_led_info led_info;
    struct sk_buff* skb;
    void* msg_head;
    int rc;

    /* CHECK: Initialized */
    if (sNet == NULL || sPid == 0) {
        DBG_LOG_LED("initialize error. [sNet=%p, sPid=%d]", sNet, sPid);
        DBG_LOG_FUNCLEAVE("initialize error. [sNet=%p, sPid=%d]", sNet, sPid);
        return -1;
    }

    /* CHECK: arguments */
    if (led_data == NULL || led_data_length <= 0) {
        DBG_LOG_LED("invalid argument. [led_data=%p, led_data_length=%d]", led_data, led_data_length);
        DBG_LOG_FUNCLEAVE("invalid argument. [led_data=%p, led_data_length=%d]", led_data, led_data_length);
        return -1;
    }

    DBG_LOG_FUNCENTER("led_data=%p, led_data_length=%d", led_data, led_data_length);

    DBG_LOG_LED_MODEM("led_data[0]={brightness=%08lX, on=%lu, off=%lu}",
        led_data[0].brightness, led_data[0].flashOnMS, led_data[0].flashOffMS);
    DBG_LOG_LED_MODEM("led_data[1]={brightness=%08lX, on=%lu, off=%lu}",
        led_data[1].brightness, led_data[1].flashOnMS, led_data[1].flashOffMS);

    /**/
    led_info.num = led_data_length;
    memcpy(led_info.info, led_data, led_data_length);
    init_completion(&sSendCompletion);

    /* create message */
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if (skb == NULL) {
        DBG_LOG_LED("genlmsg_new()=NULL error.");
        DBG_LOG_FUNCLEAVE("genlmsg_new()=NULL error.");
        return -1;
    }

    /* create message header */
    msg_head = genlmsg_put(skb, 0, 0, &ledctrld_gnl_family, 0, LEDCTRLD_C_SEND_MODEM);
    if (msg_head == NULL) {
        DBG_LOG_LED("genlmsg_put()=NULL error.");
		nlmsg_free(skb);
        DBG_LOG_FUNCLEAVE("genlmsg_put()=NULL error.");
        return -1;
    }

    /* encrypt */
    memset(&sCryptoBuf[0], 0, sizeof(sCryptoBuf[0]));
    memcpy(&sCryptoBuf[0].s.content.info, &led_info, sizeof(led_info));
    rc = notification_led_if_ledctrld_encrypt();
    if (rc != 0) {
        DBG_LOG_LED("encrypt error: rc=%d", rc);
		nlmsg_free(skb);
        DBG_LOG_FUNCLEAVE("encrypt error: rc=%d", rc);
        return -1;
    }

    /* set message */
    rc = nla_put(skb, LEDCTRLD_A_SEND_MODEM, sizeof(sCryptoBuf[1]), &sCryptoBuf[1]);
    if (rc != 0) {
        DBG_LOG_LED("nla_put() error. [rc=%d]", rc);
		nlmsg_free(skb);
        DBG_LOG_FUNCLEAVE("nla_put() error. [rc=%d]", rc);
        return -1;
    }

    /* finalize message */
    genlmsg_end(skb, msg_head);

    /* send */
    rc = genlmsg_unicast(sNet, skb, sPid);
    if (rc != 0) {
        DBG_LOG_LED("genlmsg_unicast() error. [rc=%d]", rc);
        DBG_LOG_FUNCLEAVE("genlmsg_unicast() error. [rc=%d]", rc);
        return -1;
    }

    /* Wait for response -> ledctrld_response() */
    rc = wait_for_completion_timeout(&sSendCompletion,
        msecs_to_jiffies(LEDCTRLD_SEND_MODEM_TIMEOUT_MS));
    if (!rc) {
        DBG_LOG_LED("wait_for_completion_timeout() is TIMEOUT(%1dms).",
            LEDCTRLD_SEND_MODEM_TIMEOUT_MS);
    }

    DBG_LOG_FUNCLEAVE("");
    return 0;
}

/* Get response from "ledctrld" */
static int notification_led_if_ledctrld_response(struct sk_buff* skb_2, struct genl_info* info)
{
    struct nlattr* attr;
    unsigned char* dat;
    int err, len;

    DBG_LOG_FUNCENTER("skb_2=%p, info=%p", skb_2, info);

    err = notification_led_if_ledctrld_check_cmd_executor();
    if (err != 0) {
        DBG_LOG_LED("bad cmd executor: err=%d", err);
        DBG_LOG_FUNCLEAVE("bad cmd executor: err=%d", err);
        return -1;
    }

    /* CHECK: arguments */
    if (info == NULL) {
        DBG_LOG_LED("invalid argument. [skb_2=%p, info=%p]", skb_2, info);
        DBG_LOG_FUNCLEAVE("invalid argument. [skb_2=%p, info=%p]", skb_2, info);
        return -1;
    }

    /* Send completion-signal -> ledctrld_send_modem() */
    complete(&sSendCompletion);

    /* Get response from "ledctrld" */
    attr = info->attrs[LEDCTRLD_A_RESPONSE];
    if (attr == NULL) {
        DBG_LOG_LED("ledctrld: attr=NULL");
        DBG_LOG_FUNCLEAVE("ledctrld: attr=NULL");
        return -1;
    }
    dat = (unsigned char*)nla_data(attr);
    len = nla_len(attr);
    if (dat == NULL || len <= 0) {
        DBG_LOG_LED("ledctrld: dat=%p, len=%d", dat, len);
        DBG_LOG_FUNCLEAVE("ledctrld: dat=%p, len=%d", dat, len);
        return -1;
    }

    /* decrypt */
    memcpy(&sCryptoBuf[0], dat, len);
    err = notification_led_if_ledctrld_decrypt();
    if (err != 0) {
        DBG_LOG_LED("decrypt error: err=%d", err);
        DBG_LOG_FUNCLEAVE("decrypt error: err=%d", err);
        return -1;
    } else if (sCryptoBuf[1].s.content.response != 0) {
        DBG_LOG_LED("ledctrld: response=%08X", sCryptoBuf[1].s.content.response);
        DBG_LOG_FUNCLEAVE("ledctrld: response=%08X", sCryptoBuf[1].s.content.response);
        return -1;
    }

    DBG_LOG_FUNCLEAVE("");
    return 0;
}

static int notification_led_if_init_ledctrld(void)
{
	int ret = 0;
	
	DBG_LOG_FUNCENTER("");
    ret = genl_register_family_with_ops(&ledctrld_gnl_family,
        ledctrld_gnl_ops, ARRAY_SIZE(ledctrld_gnl_ops));
    if (ret != 0) {
        DBG_LOG_LED("Register family & ops error. [ret=%d]", ret);
    }
	
	mutex_init(&led_waitlock);
	
	DBG_LOG_FUNCLEAVE("");
	return ret;
}

static int notification_led_if_exit_ledctrld(void)
{
    int ret;

	DBG_LOG_FUNCENTER("");

    ret = genl_unregister_family(&ledctrld_gnl_family);
    if (ret != 0) {
        DBG_LOG_LED("Unregister family error. [ret=%d]", ret);
    }

	DBG_LOG_FUNCLEAVE("");
	
	return ret;
}

int notification_led_sw_change_control(int led_status)
{
	int ret = -1;
	int i  = 0;
	DBG_LOG_FUNCENTER("[led_status=%d]", led_status);

	DBG_LOG_LED_SUSPEND("[led_notify=0x%08x, led_charge=0x%08x]"
						, led_notify, led_charge);
	DBG_LOG_LED_SUSPEND("[led_alpha=0x%04x, led_red=0x%04x, led_green=0x%04x, led_blue=0x%04x]"
						, g_led_color.led_alpha, g_led_color.led_red, g_led_color.led_green, g_led_color.led_blue);
	DBG_LOG_LED_SUSPEND("[on_time=%d(ms), off_time=%d(ms)]"
						, g_led_color.on_time, g_led_color.off_time);

	mutex_lock(&led_waitlock);

	memset(g_led_control_info_type, 0x00, sizeof(g_led_control_info_type));

	if (led_status == LED_STATUS_SUSPEND) {
		/* [Priority Level : 0] */
		if ((led_charge == CHARGE_TYPE_NONE) && (led_notify == NOTIFY_TYPE_NONE)) {
			g_led_control_info_type[0].brightness |= (g_led_color.led_alpha << 24);
			g_led_control_info_type[0].brightness |= (g_led_color.led_red   << 16);
			g_led_control_info_type[0].brightness |= (g_led_color.led_green <<  8);
			g_led_control_info_type[0].brightness |= (g_led_color.led_blue  <<  0);
			if ((g_led_color.on_time != 0) && (g_led_color.off_time != 0)) {
				g_led_control_info_type[0].flashOnMS   =  g_led_color.on_time;
				g_led_control_info_type[0].flashOffMS  =  g_led_color.off_time;
			} else {
				g_led_control_info_type[0].flashOnMS   = 1;
				g_led_control_info_type[0].flashOffMS  = 0;
			}
			i=1;

		/* [Priority Level : 1] */
		} else if (led_charge == CHARGE_TYPE_ERROR) {
			g_led_control_info_type[0].brightness |= (g_led_color.led_alpha << 24);
			g_led_control_info_type[0].brightness |= (g_led_color.led_red   << 16);
			g_led_control_info_type[0].brightness |= (g_led_color.led_green <<  8);
			g_led_control_info_type[0].brightness |= (g_led_color.led_blue  <<  0);
			g_led_control_info_type[0].flashOnMS   =  g_led_color.on_time;
			g_led_control_info_type[0].flashOffMS  =  g_led_color.off_time;
			i=1;

		/* [Priority Level : 2] */
		} else if ((led_charge == CHARGE_TYPE_NONE)
			   &&  (led_notify  & NOTIFY_TYPE_PREVIEW)) {
			g_led_control_info_type[0].brightness = LED_OFF;
			g_led_control_info_type[0].flashOnMS  = 1;
			g_led_control_info_type[0].flashOffMS = 0;
			i=1;

		} else if ((led_charge == CHARGE_TYPE_CHARGING) 
			   &&  (led_notify  & NOTIFY_TYPE_PREVIEW )) {
			g_led_control_info_type[0].brightness = LED_COLOR_RED;
			g_led_control_info_type[0].flashOnMS  = 1;
			g_led_control_info_type[0].flashOffMS = 0;
			i=1;

		/* [Priority Level : 3] */
		} else if ((led_charge == CHARGE_TYPE_NONE   )
			   && ((led_notify  & NOTIFY_TYPE_3RD_APL)
			   ||  (led_notify  & NOTIFY_TYPE_GMAIL  ))) {
			g_led_control_info_type[0].brightness |= (g_led_color.led_alpha << 24);
			g_led_control_info_type[0].brightness |= (g_led_color.led_red   << 16);
			g_led_control_info_type[0].brightness |= (g_led_color.led_green <<  8);
			g_led_control_info_type[0].brightness |= (g_led_color.led_blue  <<  0);
			g_led_control_info_type[0].flashOnMS   = g_led_color.on_time;
			g_led_control_info_type[0].flashOffMS  = LED_SUSPEND_OFF_TIME;
			i=1;
		
			if (g_led_color.on_time == 0
					|| (g_led_color.on_time > 0 && g_led_color.off_time == 0)) {
				g_led_control_info_type[0].flashOnMS   = 1;
				g_led_control_info_type[0].flashOffMS  = 0;
			}

			i=1;

		} else if ((led_charge == CHARGE_TYPE_CHARGING)
			   && ((led_notify  & NOTIFY_TYPE_3RD_APL )
			   ||  (led_notify  & NOTIFY_TYPE_GMAIL   ))) {
			g_led_control_info_type[0].brightness |= (g_led_color.led_alpha << 24);
			g_led_control_info_type[0].brightness |= (g_led_color.led_red   << 16);
			g_led_control_info_type[0].brightness |= (g_led_color.led_green <<  8);
			g_led_control_info_type[0].brightness |= (g_led_color.led_blue  <<  0);
			g_led_control_info_type[0].flashOnMS   = g_led_color.on_time;
			g_led_control_info_type[0].flashOffMS  = LED_BLINK_OFF_TIME;

			g_led_control_info_type[1].brightness = LED_COLOR_RED;
			g_led_control_info_type[1].flashOnMS  = LED_SUSPEND_OFF_TIME - (LED_BLINK_OFF_TIME * 2);
			g_led_control_info_type[1].flashOffMS = LED_BLINK_OFF_TIME;

			if (g_led_color.on_time == 0
					|| (g_led_color.on_time > 0 && g_led_color.off_time == 0)) {
				if (g_led_control_info_type[0].brightness == LED_OFF)
						g_led_control_info_type[0].brightness = LED_COLOR_RED;
				g_led_control_info_type[0].flashOnMS   = 1;
				g_led_control_info_type[0].flashOffMS  = 0;
				i=1;
			} else
				i=2;
			
		/* [Priority Level : 4] */
		} else if ((led_charge == CHARGE_TYPE_NONE)
			   && ((led_notify  & NOTIFY_TYPE_NOT_FAITH )
			   ||  (led_notify  & NOTIFY_TYPE_ANSWERING )
			   ||  (led_notify  & NOTIFY_TYPE_MSG_UPDATE)
			   ||  (led_notify  & NOTIFY_TYPE_NEW_MSG   )
			   ||  (led_notify  & NOTIFY_TYPE_ALARM     ))) {
			g_led_control_info_type[0].brightness |= (g_led_color.led_alpha << 24);
			g_led_control_info_type[0].brightness |= (g_led_color.led_red   << 16);
			g_led_control_info_type[0].brightness |= (g_led_color.led_green <<  8);
			g_led_control_info_type[0].brightness |= (g_led_color.led_blue  <<  0);
			g_led_control_info_type[0].flashOnMS   = LED_SUSPEND_ON_TIME;
			g_led_control_info_type[0].flashOffMS  = LED_SUSPEND_OFF_TIME;
			i=1;
		
		} else if ((led_charge == CHARGE_TYPE_CHARGING)
			   && ((led_notify  & NOTIFY_TYPE_NOT_FAITH )
			   ||  (led_notify  & NOTIFY_TYPE_ANSWERING )
			   ||  (led_notify  & NOTIFY_TYPE_MSG_UPDATE)
			   ||  (led_notify  & NOTIFY_TYPE_NEW_MSG   )
			   ||  (led_notify  & NOTIFY_TYPE_ALARM     ))) {
			g_led_control_info_type[0].brightness |= (g_led_color.led_alpha << 24);
			g_led_control_info_type[0].brightness |= (g_led_color.led_red   << 16);
			g_led_control_info_type[0].brightness |= (g_led_color.led_green <<  8);
			g_led_control_info_type[0].brightness |= (g_led_color.led_blue  <<  0);
			g_led_control_info_type[0].flashOnMS   = LED_SUSPEND_ON_TIME;
			g_led_control_info_type[0].flashOffMS  = LED_BLINK_OFF_TIME;

			g_led_control_info_type[1].brightness = LED_COLOR_RED;
			g_led_control_info_type[1].flashOnMS  = LED_SUSPEND_OFF_TIME - (LED_BLINK_OFF_TIME * 2);
			g_led_control_info_type[1].flashOffMS = LED_BLINK_OFF_TIME;
			i=2;		

		/* [Priority Level : 5] */
		} else if (led_charge == CHARGE_TYPE_CHARGING) {
			g_led_control_info_type[0].brightness |= (g_led_color.led_alpha << 24);
			g_led_control_info_type[0].brightness |= (g_led_color.led_red   << 16);
			g_led_control_info_type[0].brightness |= (g_led_color.led_green <<  8);
			g_led_control_info_type[0].brightness |= (g_led_color.led_blue  <<  0);
			g_led_control_info_type[0].flashOnMS   = 1;
			g_led_control_info_type[0].flashOffMS  = 0;
			i=1;

		} else {
			DBG_LOG_LED("[led_notify=0x%08x, led_charge=0x%08x]"
						, led_notify, led_charge);
			DBG_LOG_LED("[led_alpha=0x%04x, led_red=0x%04x, led_green=0x%04x, led_blue=0x%04x]"
						, g_led_color.led_alpha, g_led_color.led_red, g_led_color.led_green, g_led_color.led_blue);	
			DBG_LOG_LED("[on_time=%d(ms), off_time=%d(ms)]"
						, g_led_color.on_time, g_led_color.off_time);
			DBG_LOG_LED("irregular else{}.");
			DBG_LOG_LED("oem_led_state_send_modem() none call.");
			DBG_LOG_FUNCLEAVE("irregular. oem_led_state_send_modem() none call.");
			return ret;
		}
	} else {
		g_led_control_info_type[0].brightness = 0;
		g_led_control_info_type[0].flashOnMS  = 0;
		g_led_control_info_type[0].flashOffMS = 0;
		i=1;
	}

	DBG_LOG_LED_SUSPEND("set smem data. [LedLightingPattern=1][brightness=0x%08lx, flashOnMS=%ld(ms), flashOffMS=%ld(ms)]"
		, g_led_control_info_type[0].brightness
		, g_led_control_info_type[0].flashOnMS
		, g_led_control_info_type[0].flashOffMS );

	DBG_LOG_LED_SUSPEND("set smem data. [LedLightingPattern=2][brightness=0x%08lx, flashOnMS=%ld(ms), flashOffMS=%ld(ms)]"
		, g_led_control_info_type[1].brightness
		, g_led_control_info_type[1].flashOnMS
		, g_led_control_info_type[1].flashOffMS );

    ret = notification_led_if_ledctrld_send_modem(g_led_control_info_type, sizeof(led_control_info_type)*i);
    mutex_unlock(&led_waitlock);
    if (unlikely(ret)) {
        DBG_LOG_LED("ledctrld_send_modem() error. [ret=%d]", ret);
        DBG_LOG_FUNCLEAVE("ledctrld_send_modem() error. [ret=%d]", ret);
    }
	
	DBG_LOG_FUNCLEAVE("");
	return ret;
}

ssize_t notification_led_sw_notify_store(struct notification_led_data * led_data, const char *buf, size_t size)
{
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	ssize_t ret = -EINVAL;

	DBG_LOG_FUNCENTER("");

	DBG_LOG_LED_NOTIFY("update before. [InputBuf=\"%s\"], [led_notify=0x%08x, led_charge=0x%08x], [count=%d ,size=%d]"
						, buf, led_notify, led_charge, count ,size);

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		led_notify = (int)(state & 0x0000ffff);
		led_charge = (unsigned char)((state & 0x00ff0000) >> 16);
		led_charge = led_charge & 0x07;
	}
	
	DBG_LOG_LED_NOTIFY("update after.  [InputBuf=\"%s\"], [led_notify=0x%08x, led_charge=0x%08x], [count=%d ,size=%d]"
						, buf, led_notify, led_charge, count ,size);

	DBG_LOG_FUNCLEAVE("");
	return ret;
}
ssize_t notification_led_sw_notify_show (struct notification_led_data * led_data, char *buf)
{
	int state = led_charge;
	state = (int)(state << 16);
	state = (int)(state | (led_notify & 0x0000ffff)); 
	
	DBG_LOG_FUNCENTER("");
	DBG_LOG_FUNCLEAVE("");

	return sprintf (buf, "%u\n", state);
}

ssize_t notification_led_sw_color_store(const char *buf, size_t size)
{
	char red[3];
	char green[3];
	char blue[3];
	char onTime[9];
	char offTime[9];

	unsigned long red_state;
	unsigned long green_state;
	unsigned long blue_state;
	unsigned long onTime_count;
	unsigned long offTime_count;

	char *after;
	
	int ret = 0;

	DBG_LOG_FUNCENTER("");

	DBG_LOG_LED_COLOR("update before. SmemData [InputBuf=\"0x%s\"], [led_red=%d, led_green=%d, led_blue=%d, on_time=%d, off_time=%d]"
					, buf
					, g_led_color.led_red, g_led_color.led_green, g_led_color.led_blue
					, g_led_color.on_time, g_led_color.off_time );
	
	memset(red,     0x00, 3);
	memset(green,   0x00, 3);
	memset(blue,    0x00, 3);
	memset(onTime,  0x00, 9);
	memset(offTime, 0x00, 9);

	strncpy(offTime, buf,    8);
	strncpy(onTime,  buf+ 8, 8);
	strncpy(red,     buf+16, 2);
	strncpy(green,   buf+18, 2);
	strncpy(blue,    buf+20, 2);

	red_state     = simple_strtoul (red,     &after, 16);
	green_state   = simple_strtoul (green,   &after, 16);
	blue_state    = simple_strtoul (blue,    &after, 16);
	onTime_count  = simple_strtoul (onTime,  &after, 16);
	offTime_count = simple_strtoul (offTime, &after, 16);

	g_led_color.led_red   = (unsigned char)red_state;
	g_led_color.led_green = (unsigned char)green_state;
	g_led_color.led_blue  = (unsigned char)blue_state;
	g_led_color.on_time   = (unsigned int)onTime_count;
	g_led_color.off_time  = (unsigned int)offTime_count;

	if (g_led_status == LED_STATUS_SUSPEND) {
		ret = notification_led_sw_change_control(g_led_status);
		if (unlikely(ret)) {
			printk(KERN_ERR "%s: notification_led_sw_change_control failed.[%d]\n", __func__, ret);
			return ret;
		}
	}
	
	ret = size;

	DBG_LOG_LED_COLOR("update after.  SmemData [InputBuf=\"0x%s\"], [led_red=%d, led_green=%d, led_blue=%d, on_time=%d, off_time=%d]"
					, buf
					, g_led_color.led_red, g_led_color.led_green, g_led_color.led_blue
					, g_led_color.on_time, g_led_color.off_time );
	
	DBG_LOG_FUNCLEAVE("");

	return ret;
}
ssize_t notification_led_sw_color_show(char *buf)
{
	DBG_LOG_FUNCENTER("");
	DBG_LOG_FUNCLEAVE("");

	return sprintf(buf, "r= %x g= %x b=%x on=%x off=%x\n",
				g_led_color.led_red,
				g_led_color.led_green,
				g_led_color.led_blue,
				g_led_color.on_time,
				g_led_color.off_time);
}

ssize_t notification_led_sw_argb_store(struct notification_led_data * led_data, const char * buf, size_t size)
{
	char * after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	ssize_t ret = -EINVAL;

	DBG_LOG_FUNCENTER("");
	DBG_LOG_LED_ARGB("update before. [argb=0x%08x], [delay_on=%ld, delay_off=%ld]"
			    	, led_data->argb, led_data->delay_on, led_data->delay_off);
	
	if (*after && isspace(*after))
		count++;

	if (count == size) {
		led_data->argb = state;
		ret = count;
	}

	DBG_LOG_LED_ARGB("update after.  [argb=0x%08x], [delay_on=%ld, delay_off=%ld]"
			    	, led_data->argb, led_data->delay_on, led_data->delay_off);
	
	DBG_LOG_FUNCLEAVE("");

	return ret;
}
ssize_t notification_led_sw_argb_show(struct notification_led_data * led_data, char * buf)
{
	DBG_LOG_FUNCENTER("");
	DBG_LOG_FUNCLEAVE("");

	return sprintf(buf, "%d\n", led_data->argb);
}

ssize_t notification_led_sw_delay_on_store(struct notification_led_data * led_data, const char * buf, size_t size)
{

	char * after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	ssize_t ret = -EINVAL;

	DBG_LOG_FUNCENTER("");

	DBG_LOG_LED_DELAY("update before. [argb=0x%08x], delay_on=%ld, delay_off=%ld]"
				    , led_data->argb, led_data->delay_on, led_data->delay_off);

	if (isspace(*after))
		count++;

	if (count == size) {
		if (led_data->delay_on != state) {
			/* the new value differs from the previous */
			led_data->delay_on = state;

			if ((led_data->delay_on > 0) && (led_data->delay_off > 0)) {
				g_first_blink_flg = 1;
			}

			/* deactivate previous settings */
			del_timer_sync(&led_data->timer);

			/* no hardware acceleration, blink via timer */
			mod_timer(&led_data->timer, jiffies + 1);
		}
		ret = count;
	}

	DBG_LOG_LED_DELAY("update after.  [argb=0x%08x], delay_on=%ld, delay_off=%ld]"
				    , led_data->argb, led_data->delay_on, led_data->delay_off);
	
	DBG_LOG_FUNCLEAVE("");

	return ret;
}
ssize_t notification_led_sw_delay_on_show(struct notification_led_data * led_data, char * buf)
{
	DBG_LOG_FUNCENTER("");
	DBG_LOG_FUNCLEAVE("");
	return sprintf(buf, "%lu\n", led_data->delay_on);
}

ssize_t notification_led_sw_delay_off_store(struct notification_led_data * led_data, const char * buf, size_t size)
{
	char * after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	
	ssize_t ret = -EINVAL;

	DBG_LOG_FUNCENTER("");
	DBG_LOG_LED_DELAY("update before. [argb=0x%08x], delay_on=%ld, delay_off=%ld], [led_charge=%d, g_chrg_count=%d, state=%ld], [count=%d, size=%d]"
				    , led_data->argb, led_data->delay_on, led_data->delay_off
					, led_charge, g_chrg_count, state, count, size);

	if (isspace(*after))
		count++;

	if (count == size) {
		if (led_data->delay_off != state) {
			/* the new value differs from the previous */
			if ((led_charge == 1) && (state > 150)){
				state -= 100;
				g_chrg_count = 1;
			} else {
				g_chrg_count = 0;
			}
			led_data->delay_off = state;

			if ((led_data->delay_on > 0) && (led_data->delay_off > 0)) {
				g_first_blink_flg = 1;
			}

			/* deactivate previous settings */
			del_timer_sync(&led_data->timer);

			/* no hardware acceleration, blink via timer */
			mod_timer(&led_data->timer, jiffies + 1);
		}
		ret = count;
	}

	DBG_LOG_LED_DELAY("update after.  [argb=0x%08x], delay_on=%ld, delay_off=%ld], [led_charge=%d, g_chrg_count=%d, state=%ld], [count=%d, size=%d]"
				    , led_data->argb, led_data->delay_on, led_data->delay_off
					, led_charge, g_chrg_count, state, count, size);

	DBG_LOG_FUNCLEAVE("");

	return ret;
}
ssize_t notification_led_sw_delay_off_show(struct notification_led_data * led_data, char * buf)
{
	DBG_LOG_FUNCENTER("");
	DBG_LOG_FUNCLEAVE("");

	return sprintf(buf, "%lu\n", led_data->delay_off);
}


int notification_led_sw_set(struct notification_led_data * led_data, enum led_brightness value)
{
	int red;
	int green;
	int blue;
	int ret = 0;
	
	DBG_LOG_FUNCENTER("[led_data=0x%p, value=%u]", led_data, value);

	/* LED fale safe */
	if (g_led_status == LED_STATUS_SUSPEND) {
	    DBG_LOG_LED("[g_led_status=%d], [argb=0x%x, value=%d]", g_led_status, led_data->argb, value);
	    DBG_LOG_FUNCLEAVE("[g_led_status=%d], [argb=0x%x, value=%d]", g_led_status, led_data->argb, value);
		return ret;
	}

	if (value != LED_OFF) {
		red   = ((led_data->argb >> 16) & 255) ? 1 : 0;
		green = ((led_data->argb >>  8) & 255) ? 1 : 0;
		blue  = ((led_data->argb      ) & 255) ? 1 : 0;
	} else if (led_notify & 0x00000100) {
		red   = 0;
		green = 0;
		blue  = 0;
	} else if ((led_charge == CHARGE_TYPE_CHARGING) && (g_chrg_count != 2)) {
		red   = 1;
		green = 0;
		blue  = 0;
	} else {
		red   = 0;
		green = 0;
		blue  = 0;
	}
	DBG_LOG_LED_SET("[R/G/B=%d/%d/%d], [argb=0x%x, value=%d, delay_on=%ld, delay_off=%ld], [led_charge=0x%08x, led_notify=0x%08x], [led_charge=%d, g_chrg_count=%d]"
					, red, green, blue, led_data->argb, value, led_data->delay_on, led_data->delay_off, led_charge, led_notify, led_charge, g_chrg_count);
	
	ret = notification_led_hw_set(red, green, blue);

	DBG_LOG_FUNCLEAVE("");

	return ret;
}
static void
notification_led_sw_timer_function(unsigned long data)
{
	struct notification_led_data * led_data
		= (struct notification_led_data *)data;
	unsigned long brightness;
	unsigned long delay;

	DBG_LOG_FUNCENTER("[data=%lx]", data);

	if (g_led_status == LED_STATUS_SUSPEND) {
	    DBG_LOG_LED_DELAY("[g_led_status=%d], [argb=0x%x, delay_on=%ld, delay_off=%ld]", g_led_status, led_data->argb, led_data->delay_on, led_data->delay_off);
	    DBG_LOG_FUNCLEAVE("[g_led_status=%d], [argb=0x%x, delay_on=%ld, delay_off=%ld]", g_led_status, led_data->argb, led_data->delay_on, led_data->delay_off);
		return;
	}

	DBG_LOG_LED_DELAY("update before. [led_charge=%d, g_chrg_count=%d],[delay_on=%ld, delay_off=%ld]"
						, led_charge, g_chrg_count, led_data->delay_on, led_data->delay_off);

	if (!led_data->delay_on || !led_data->delay_off) {
		DBG_LOG_FUNCLEAVE("");
		return;
	}
	if(g_first_blink_flg == 0){
		if(g_chrg_count){
			if(g_chrg_count == 1){
				g_chrg_count = 2;
				led_set_brightness(&led_data->cdev, LED_OFF);
				mod_timer(&led_data->timer, jiffies + msecs_to_jiffies(50));
				return;
			}
			g_chrg_count = 1;
		}
	}
	brightness = led_get_brightness(&led_data->cdev);
	if (g_first_blink_flg == 1){
		if(brightness){
			delay = led_data->delay_on;
		}else{
			delay = led_data->delay_off;
		}
		g_first_blink_flg = 0;
	} else if (!brightness) {
		/* Time to switch the LED on. */
		brightness = led_data->brightness_on;
		delay = led_data->delay_on;
	} else {
		/* Store the current brightness value to be able
		 * to restore it when the delay_off period is over.
		 */
		led_data->brightness_on = brightness;
		brightness = LED_OFF;
		delay = led_data->delay_off;
	}

	led_set_brightness(&led_data->cdev, brightness);

	mod_timer(&led_data->timer, jiffies + msecs_to_jiffies(delay));

	DBG_LOG_LED_DELAY("update after.  [led_charge=%d, g_chrg_count=%d],[delay_on=%ld, delay_off=%ld]"
						, led_charge, g_chrg_count, led_data->delay_on, led_data->delay_off);
	
	DBG_LOG_FUNCLEAVE("[mod_timer() call.]");
}

int notification_led_sw_init(struct platform_device * pdev)
{
	int ret;
	
	DBG_LOG_FUNCENTER("[pdev=0x%p]", pdev);
	
	ret = notification_led_hw_init(pdev);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s: notification_led_hw_init failed.[%d]\n", __func__, ret);
	}
	
	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	
	return ret;
}

int notification_led_sw_exit(struct platform_device * pdev)
{
	int ret;
	
	DBG_LOG_FUNCENTER("[pdev=0x%p]", pdev);
	
	ret = notification_led_hw_exit(pdev);
	
	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	
	return ret;
}

static ssize_t
notification_led_if_notify_store(struct device * dev, struct device_attribute * attr, const char *buf, size_t size)
{
	struct led_classdev * led_cdev = dev_get_drvdata(dev);
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	
	ssize_t ret;

	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\"), size=%d]", dev, attr, buf, buf, size);
	
	ret = notification_led_sw_notify_store(led_data, buf, size);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_notify_store failed.[%d]\n", __func__, ret);
	}
	
	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static ssize_t
notification_led_if_notify_show(struct device * dev, struct device_attribute * attr, char *buf)
{
	struct led_classdev * led_cdev = dev_get_drvdata(dev);
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	
	ssize_t ret;
	
	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\")]", dev, attr, buf, buf);
	ret = notification_led_sw_notify_show(led_data, buf);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_notify_show failed.[%d]\n", __func__, ret);
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static ssize_t
notification_led_if_color_store(struct device * dev, struct device_attribute * attr, const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;

	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"0x%s\"), size=%d]", dev, attr, buf, buf, size);

	ret = notification_led_sw_color_store(buf, size);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_color_store failed.[%d]\n", __func__, ret);
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;

}

static ssize_t
notification_led_if_color_show(struct device * dev, struct device_attribute * attr, char *buf)
{
	ssize_t ret;
	
	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\")]", dev, attr, buf, buf);
	ret = notification_led_sw_color_show(buf);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_color_show failed.[%d]\n", __func__, ret);
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static ssize_t
notification_led_if_argb_store(
	struct device * dev, struct device_attribute * attr,
	const char * buf, size_t size)
{
	struct led_classdev * led_cdev = dev_get_drvdata(dev);
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	
	ssize_t ret;
	
	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\"), size=%d]", dev, attr, buf, buf, size);

	ret = notification_led_sw_argb_store(led_data, buf, size);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_argb_store failed.[%d]\n", __func__, ret);
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static ssize_t
notification_led_if_argb_show(struct device * dev, struct device_attribute * attr, char * buf)
{
	struct led_classdev * led_cdev = dev_get_drvdata(dev);
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	
	ssize_t ret;

	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\")]", dev, attr, buf, buf);
	
	ret = notification_led_sw_argb_show(led_data, buf);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_argb_show failed.[%d]\n", __func__, ret);
	}
	
	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	
	return ret;
}

static ssize_t
notification_led_if_delay_on_store(
	struct device * dev, struct device_attribute * attr,
	const char * buf, size_t size)
{
	struct led_classdev * led_cdev = dev_get_drvdata(dev);
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	ssize_t ret;
	
	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\"), size=%d]", dev, attr, buf, buf, size);

	ret = notification_led_sw_delay_on_store(led_data, buf, size);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_delay_on_store failed.[%d]\n", __func__, ret);
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static ssize_t
notification_led_if_delay_on_show(struct device * dev, struct device_attribute * attr, char * buf)
{
	struct led_classdev * led_cdev = dev_get_drvdata(dev);
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	ssize_t ret;
	
	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\")]", dev, attr, buf, buf);

	ret = notification_led_sw_delay_on_show(led_data, buf);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_delay_on_show failed.[%d]\n", __func__, ret);
	}
	
	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}


static ssize_t
notification_led_if_delay_off_store(
	struct device * dev, struct device_attribute * attr,
	const char * buf, size_t size)
{
	struct led_classdev * led_cdev = dev_get_drvdata(dev);
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	ssize_t ret = -EINVAL;

	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\"), size=%d]", dev, attr, buf, buf, size);

	ret = notification_led_sw_delay_off_store(led_data, buf, size);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_delay_off_store failed.[%d]\n", __func__, ret);
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static ssize_t
notification_led_if_delay_off_show(
	struct device * dev, struct device_attribute * attr, char * buf)
{
	struct led_classdev * led_cdev = dev_get_drvdata(dev);
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	
	ssize_t ret;

	if (unlikely(buf == NULL)) {
		printk(KERN_ERR "%s: parameter err.\n", __func__);
		return -EINVAL;
	}
	
	DBG_LOG_FUNCENTER("[dev=0x%p, attr=0x%p, buf=0x%p(\"%s\")]", dev, attr, buf, buf);
	
	ret = notification_led_sw_delay_off_show(led_data, buf);
	if (unlikely(ret < 0)) {
		printk(KERN_ERR "%s: notification_led_sw_delay_off_show failed.[%d]\n", __func__, ret);
	}
	
	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static const struct device_attribute notification_led_attrs[] = {
	__ATTR(argb,       0644, notification_led_if_argb_show,      notification_led_if_argb_store),
	__ATTR(led_color,  0644, notification_led_if_color_show,     notification_led_if_color_store),
	__ATTR(led_notify, 0644, notification_led_if_notify_show,    notification_led_if_notify_store),
	__ATTR(delay_on,   0644, notification_led_if_delay_on_show,  notification_led_if_delay_on_store),
	__ATTR(delay_off,  0644, notification_led_if_delay_off_show, notification_led_if_delay_off_store),
	__ATTR_NULL,
};

static void
notification_led_if_set(struct led_classdev * led_cdev, enum led_brightness value)
{
	struct notification_led_data * led_data
		= container_of(led_cdev, struct notification_led_data, cdev);
	int ret;

	DBG_LOG_FUNCENTER("[led_cdev=0x%p, value=%u]", led_cdev, value);

	/* suspend notification mode (TCXO shutdown) */
	if (tcxo_flg) {
	    DBG_LOG_LED("[SuspendNotificationMode=%d]", tcxo_flg);
		DBG_LOG_FUNCLEAVE("[SuspendNotificationMode=%d]", tcxo_flg);
		return;
	}
	
	ret = notification_led_sw_set(led_data, value);
	DBG_LOG_FUNCLEAVE("");
}

static int
notification_led_if_write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;

    fd = sys_open(path, O_RDWR, 0);
    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = sys_write(fd, buffer, bytes);
        sys_close(fd);
        return amt == -1 ? -1 : 0;
    } else {
        if (already_warned == 0) {
            already_warned = 1;
        }
        return -1;
    }
}

static void notification_led_early_suspend(struct early_suspend *h)
{
	int ret;
	DBG_LOG_FUNCENTER("[pdev=0x%p]", h);
	
	if (charging_mode) {
		DBG_LOG_FUNCLEAVE("charging_mode is true");
		return;
	}

	g_led_status = LED_STATUS_SUSPEND;

	ret = notification_led_sw_change_control(g_led_status);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s: notification_led_sw_change_control failed.[%d]\n", __func__, ret);
	}

    /* LED turn off (LED control switch to Modem) */
    if (!tcxo_flg) {
        notification_led_hw_set(LED_OFF, LED_OFF, LED_OFF);
    }

	DBG_LOG_LED_SUSPEND("[g_led_status=%d]", g_led_status);

	DBG_LOG_FUNCLEAVE("");
}

static void
notification_led_late_resume(struct early_suspend *h)
{
	int ret;
    unsigned int colorRGB =0;

	DBG_LOG_FUNCENTER("[h=0x%p]", h);

	if (charging_mode) {
		DBG_LOG_FUNCLEAVE("charging_mode is true");
		return;
	}

	g_led_status = LED_STATUS_NORMAL;
	DBG_LOG_LED_RESUME("[g_led_status=%d]", g_led_status);

	colorRGB = g_led_color.led_red;
	colorRGB = (colorRGB << 8) | g_led_color.led_green;
	colorRGB = (colorRGB << 8) | g_led_color.led_blue;

	notification_led_if_write_int("/sys/class/leds/notification/delay_on",  0);
	notification_led_if_write_int("/sys/class/leds/notification/delay_off", 0);
	notification_led_if_write_int("/sys/class/leds/notification/argb", colorRGB);
	notification_led_if_write_int("/sys/class/leds/notification/brightness", colorRGB ? 255 : 0);

    if ((g_led_color.on_time > 0) && (g_led_color.off_time > 0)) {
        notification_led_if_write_int("/sys/class/leds/notification/delay_on",  g_led_color.on_time);
        notification_led_if_write_int("/sys/class/leds/notification/delay_off", g_led_color.off_time);
    }

	ret = notification_led_sw_change_control(g_led_status);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s: notification_led_sw_change_control failed.[%d]\n", __func__, ret);
	}

	DBG_LOG_FUNCLEAVE("");
}

static int __devinit
notification_led_if_sysfs_create_files(
	struct device * dev, const struct device_attribute * attrs)
{
	int ret = 0;
	int i;

	DBG_LOG_FUNCENTER("[dev=0x%p, attrs=0x%p]", dev, attrs);

	for (i = 0; attrs[i].attr.name != NULL ; i++) {
		ret = sysfs_create_file(&dev->kobj, &attrs[i].attr);
		if (ret) {
			dev_err(dev, "failure sysfs_create_file \"%s\"\n",
				attrs[i].attr.name);
			while (--i >= 0)
				sysfs_remove_file(&dev->kobj, &attrs[i].attr);
			break;
		}
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static int
notification_led_if_sysfs_remove_files(
	struct device * dev, const struct device_attribute * attrs)
{
	DBG_LOG_FUNCENTER("[dev=0x%p, attrs=0x%p]", dev, attrs);

	for ( ; attrs->attr.name != NULL; attrs++)
		sysfs_remove_file(&dev->kobj, &attrs->attr);

	DBG_LOG_FUNCLEAVE("");
	
	return 0;
}

int notification_led_if_init(struct platform_device * pdev)
{
	int ret = 0;
	DBG_LOG_FUNCENTER("[pdev=0x%p]", pdev);
	
	ret = notification_led_sw_init(pdev);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s: notification_led_sw_init failed.[%d]\n", __func__, ret);
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);

	return ret;
}

int notification_led_if_exit(struct platform_device * pdev)
{
	int ret;
	DBG_LOG_FUNCENTER("[pdev=0x%p]", pdev);

	ret = notification_led_sw_exit(pdev);

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static struct early_suspend notification_led_early_suspend_handler = {
	.suspend = notification_led_early_suspend,
	.resume = notification_led_late_resume,
};


static int __devinit notification_led_probe(struct platform_device * pdev)
{
	struct notification_led_data * led_data;
	unsigned char led_getNvData[APNV_SIZE_LED_SUSPEND_MODE];
	int ret = 0;

	DBG_LOG_FUNCENTER("[pdev=0x%p]", pdev);

	ret = notification_led_if_init_ledctrld();
	if (unlikely(ret)) {
		printk(KERN_ERR "%s: notification_led_if_init_ledctrld failed.[%d]\n", __func__, ret);
	}
	
	led_data = kzalloc(sizeof(*led_data), GFP_KERNEL);
	if (unlikely(!led_data)) {
		dev_err(&pdev->dev, "failure kzalloc\n");
		DBG_LOG_FUNCLEAVE("[return=%d(-ENOMEM)]", -ENOMEM);
		return -ENOMEM;
	}

	led_data->cdev.name = LED_DRIVER_NAME;
	led_data->cdev.brightness_set = notification_led_if_set;
	led_data->cdev.brightness = LED_OFF;

	init_timer(&led_data->timer);
	led_data->timer.function = notification_led_sw_timer_function;
	led_data->timer.data = (unsigned long)led_data;
	led_data->brightness_on = LED_FULL;

	ret = led_classdev_register(&pdev->dev, &led_data->cdev);
	if (unlikely(ret < 0)) {
		dev_err(&pdev->dev, "L%d: %s() led_classdev_register() error. [ret=%d]\n", __LINE__, __FUNCTION__, ret);
		kfree(led_data);
		return ret;
	}

	register_early_suspend(&notification_led_early_suspend_handler);

	ret = notification_led_if_sysfs_create_files(led_data->cdev.dev, notification_led_attrs);
	if (unlikely(ret)) {
		unregister_early_suspend(&notification_led_early_suspend_handler);
		led_classdev_unregister(&led_data->cdev);
		kfree(led_data);
		return ret;
	}

	memset(led_getNvData, 0x00, sizeof(led_getNvData));
	ret = get_nonvolatile(led_getNvData, APNV_LED_SUSPEND_MODE_I, APNV_SIZE_LED_SUSPEND_MODE);
	if (unlikely(ret < 0)) {
		dev_err(&pdev->dev, "L%d: get_nonvolatile() error. [ret=%d]\n", __LINE__, ret);
		led_getNvData[0] = 0;
		DBG_LOG_LED("led_getNvData set default value. [led_getNvData=%d]", led_getNvData[0]);
	}

	DBG_LOG_LED("[led_getNvData=%d]", led_getNvData[0]);
	
	if (led_getNvData[0] > 0 && led_getNvData[0] < 15) {
		tcxo_flg = 1;
	} else {
		tcxo_flg = 0;
	}

	g_led_status = LED_STATUS_NORMAL;

	platform_set_drvdata(pdev, led_data);


	ret = notification_led_if_init(pdev);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s: notification_led_if_init failed.[%d]\n", __func__, ret);
		unregister_early_suspend(&notification_led_early_suspend_handler);
		led_classdev_unregister(&led_data->cdev);
		kfree(led_data);
	}

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static int __devexit notification_led_remove(struct platform_device * pdev)
{
	struct notification_led_data * led_data = platform_get_drvdata(pdev);
	int ret = 0;

	DBG_LOG_FUNCENTER("[pdev=0x%p]", pdev);

	ret = notification_led_if_exit_ledctrld();
	if (unlikely(ret)) {
		printk(KERN_ERR "%s: notification_led_if_ledctrld_exit failed.[%d]\n", __func__, ret);
	}

	notification_led_if_sysfs_remove_files(led_data->cdev.dev, notification_led_attrs);
	unregister_early_suspend(&notification_led_early_suspend_handler);
	led_classdev_unregister(&led_data->cdev);
	kfree(led_data);

	ret = notification_led_if_exit(pdev);

	DBG_LOG_FUNCLEAVE("[return=%d]", ret);
	return ret;
}

static struct platform_driver notification_led_driver = {
	.probe		= notification_led_probe,
	.remove		= __devexit_p(notification_led_remove),
	.driver		= {
		.name	= "notification-led",
		.owner	= THIS_MODULE,
	},
};

static int __init notification_led_init(void)
{
	int ret;
	
	DBG_LOG_FUNCENTER("");
	
	ret = platform_driver_register(&notification_led_driver);
	if (unlikely(ret)) {
		printk(KERN_ERR "%s: platform_driver_register failed.[%d]\n", __func__, ret);
	}
	
	DBG_LOG_FUNCLEAVE("");
	return ret;
}

static void __exit notification_led_exit(void)
{
	DBG_LOG_FUNCENTER("");
	
	platform_driver_unregister(&notification_led_driver);

	DBG_LOG_FUNCLEAVE("");
}

module_init(notification_led_init);
module_exit(notification_led_exit);

MODULE_ALIAS("platform:notification-led");
MODULE_DESCRIPTION("Notification LED driver");
MODULE_LICENSE("GPL v2");
