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

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <../board-8064.h>
#include <asm/mach-types.h>
#include "vibdrv_i2c.h"
#include "vibdrv.h"
#include "vibdrv_vib.h"

/*============================================================================
                        INTERNAL FEATURES
============================================================================*/

/*============================================================================
                        CONSTANTS
============================================================================*/
/* Name of the R5F104CFLA board */
#define R5F104CFALA_BOARD_NAME     "r5f104cfala"

/*============================================================================
                        STRUCTURE
============================================================================*/


/*============================================================================
                        EXTERNAL VARIABLES DEFINITIONS
============================================================================*/
static struct    i2c_client* g_pTheClient = NULL;
static struct    mutex i2c_mutex;

/*============================================================================
                        INTERNAL API DECLARATIONS
============================================================================*/
/* r5f104cfala/I2C structs and forward declarations */
static int r5f104cfala_probe(struct i2c_client* client, const struct i2c_device_id* id);
static int r5f104cfala_remove(struct i2c_client* client);


/*===========================================================================
    STATIC FUNCTION      r5f104cfala_probe
===========================================================================*/
static int r5f104cfala_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
    int retval = 0;

    VIBDRV_LOGI("r5f104cfala_probe start.\n");
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        retval = -ENODEV;
    }
    else
    {
        g_pTheClient = client;
    }
    VIBDRV_LOGI("r5f104cfala_probe end. retval:%d\n",retval);
    return retval;
}

/*===========================================================================
    STATIC FUNCTION      r5f104cfala_remove
===========================================================================*/
static int r5f104cfala_remove(struct i2c_client *client)
{
    VIBDRV_LOGI("r5f104cfala_remove.\n");
    g_pTheClient = NULL;
    return 0;
}

/*============================================================================
           r5f104cfala/I2C structs and forward declarations
============================================================================*/
static struct i2c_device_id r5f104cfala_id[] =
{
    {R5F104CFALA_BOARD_NAME, 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, r5f104cfala_id);

static struct i2c_driver r5f104cfala_driver =
{
    .driver =
    {
        .name  = R5F104CFALA_BOARD_NAME,
        .owner = THIS_MODULE,
    },
    .probe = r5f104cfala_probe,
    .remove = __devexit_p(r5f104cfala_remove),
    .id_table = r5f104cfala_id,
};

/*===========================================================================
    FUNCTION      vibdrv_i2c_lock
===========================================================================*/
int vibdrv_i2c_lock(void)
{
    VIBDRV_LOGI("vibdrv_i2c_lock.\n");
    mutex_lock(&i2c_mutex);

    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_i2c_lock
===========================================================================*/
int vibdrv_i2c_unlock(void)
{
    VIBDRV_LOGI("vibdrv_i2c_unlock.\n");
    mutex_unlock(&i2c_mutex);

    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_i2c_initialize
===========================================================================*/
int vibdrv_i2c_initialize(void)
{
    int  retval = 0;

    VIBDRV_LOGI("vibdrv_i2c_initialize start.\n");

    mutex_init(&i2c_mutex);

    /* Add r5f104cfala driver */
    if( g_pTheClient ){
        VIBDRV_LOGI("vibdrv_i2c_initialize already.\n");
        return VIBDRV_RET_OK;
    }
    retval = i2c_add_driver(&r5f104cfala_driver);
    if (retval)
    {
        VIBDRV_LOGE("%s(%d) fault (driver initialization error)\n", __func__, __LINE__);
        i2c_unregister_device(g_pTheClient);
        return VIBDRV_RET_NG;
    }

    VIBDRV_LOGI("vibdrv_i2c_initialize end.\n");

    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_i2c_terminate
===========================================================================*/
int vibdrv_i2c_terminate(void)
{

    VIBDRV_LOGI("vibdrv_i2c_terminate.\n");

    /* Remove r5f104cfala driver */
    if( !g_pTheClient ){
        VIBDRV_LOGI("vibdrv_i2c_terminate already.\n");
        return VIBDRV_RET_OK;
    }
    i2c_del_driver(&r5f104cfala_driver);
    mutex_destroy(&i2c_mutex);
    g_pTheClient = NULL;

    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_i2c_read
===========================================================================*/
int vibdrv_i2c_read(struct vibdrv_i2c_cmd_type *pI2CCmd)
{
    int            rc = 0;
    struct i2c_msg msg;
#if VIBDRV_DEBUG
    int            i = 0;
#endif
    int            retrycnt=0;
    
    VIBDRV_LOGI("vibdrv_i2c_read start.\n");

retry:

    if (!g_pTheClient->adapter) {
        VIBDRV_LOGE("%s(%d) fault (Adapter Error)\n", __func__, __LINE__);
        return VIBDRV_RET_NG;
    }

    msg.addr = g_pTheClient->addr;
    msg.flags = 0;
    msg.len = pI2CCmd->wlen;
#if VIBDRV_DEBUG
    for (i = 0; i < pI2CCmd->wlen; i++) {
        VIBDRV_LOGI("%s(%d) I2C Transfer_normal 0x%02x \n", __func__, __LINE__, pI2CCmd->pwdata[i]);
    }
#endif
    msg.buf = pI2CCmd->pwdata;
    rc = i2c_transfer(g_pTheClient->adapter, &msg, 1);
    if (rc < 0) {
        VIBDRV_LOGE("%s(%d) I2C Transfer Error %d write8:[%02x-%02x]\n",
            __func__, __LINE__, rc, msg.addr, msg.len);
        if( retrycnt < VIBDRV_WRITE_RETRY ){
            retrycnt++;
            mdelay(VIBDRV_DELAY_MS_TIME_150);
            VIBDRV_LOGE("%s(%d) I2C Transfer write Retry Start%d \n",
                __func__, __LINE__, retrycnt);
            goto retry;
        }
        return VIBDRV_RET_NG;
    }

    retrycnt = 0;
retry2:
    msg.addr  = g_pTheClient->addr;
    msg.flags = I2C_M_RD;
    msg.len   = pI2CCmd->rlen;
    msg.buf   = pI2CCmd->prdata;
    rc = i2c_transfer(g_pTheClient->adapter, &msg, 1);
    if (rc < 0) {
        VIBDRV_LOGE("%s(%d) I2C Transfer Error %d Read8:[%02x-%02x]\n",
            __func__, __LINE__, rc, msg.addr, msg.len);
        if( retrycnt < VIBDRV_WRITE_RETRY ){
            retrycnt++;
            mdelay(VIBDRV_DELAY_MS_TIME_150);
            VIBDRV_LOGE("%s(%d) I2C Transfer read Retry Start%d \n",
                __func__, __LINE__, retrycnt);
            goto retry2;
        }
        return VIBDRV_RET_NG;
    }

    VIBDRV_LOGI("%s(%d) I2C Transfer OK %d Read: 0x%02x\n",
        __func__, __LINE__, rc, pI2CCmd->prdata[0]);

    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_i2c_write
===========================================================================*/
int vibdrv_i2c_write(struct vibdrv_i2c_cmd_type * pI2CCmd)
{
    int            rc = 0;
    struct i2c_msg msg;
#if VIBDRV_DEBUG
    int            i = 0;
#endif
    int            retrycnt=0;

    VIBDRV_LOGI("vibdrv_i2c_write start.\n");
retry:

    if (!g_pTheClient->adapter) {
        VIBDRV_LOGE("%s(%d) fault (Adapter Error)\n", __func__, __LINE__);
        return VIBDRV_RET_NG;
    }

    msg.addr = g_pTheClient->addr;
    msg.flags = 0;
    msg.len = pI2CCmd->wlen;  // add address bytes
#if VIBDRV_DEBUG
    for (i = 0; i < pI2CCmd->wlen; i++) {
        VIBDRV_LOGI("%s(%d) I2C Transfer_normal 0x%02x \n", __func__, __LINE__, pI2CCmd->pwdata[i]);
    }
#endif
    msg.buf = pI2CCmd->pwdata;

    rc = i2c_transfer(g_pTheClient->adapter, &msg, 1);
    if (rc < 0) {
        VIBDRV_LOGE("%s(%d) I2C Transfer Error %d write8:[%02x-%02x]\n",
            __func__, __LINE__, rc, msg.addr, msg.len);
        if( retrycnt < VIBDRV_WRITE_RETRY ){
            retrycnt++;
            mdelay(VIBDRV_DELAY_MS_TIME_150);
            VIBDRV_LOGE("%s(%d) I2C Transfer write Retry Start%d \n",
                __func__, __LINE__, retrycnt);
            goto retry;
        }
        return VIBDRV_RET_NG;
    }

    VIBDRV_LOGI("%s(%d) I2C Transfer OK %d write \n", __func__, __LINE__, rc);

    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_i2c_one_read
===========================================================================*/
int vibdrv_i2c_one_read(struct vibdrv_i2c_cmd_type * pI2CCmd)
{
    int            rc = 0;
    struct i2c_msg msg;
    int            retrycnt=0;

    VIBDRV_LOGI("vibdrv_i2c_one_read start.\n");

retry:

    if (!g_pTheClient->adapter) {
        VIBDRV_LOGE("%s(%d) fault (Adapter Error)\n", __func__, __LINE__);
        return VIBDRV_RET_NG;
    }

    msg.addr  = g_pTheClient->addr;
    msg.flags = I2C_M_RD;
    msg.len   = pI2CCmd->rlen;  // add address bytes
    msg.buf   = pI2CCmd->prdata;

    rc = i2c_transfer(g_pTheClient->adapter, &msg, 1);
    if (rc < 0) {
        VIBDRV_LOGE("%s(%d) I2C Transfer Error %d write8:[%02x-%02x]\n",
            __func__, __LINE__, rc, msg.addr, msg.len);
        if( retrycnt < VIBDRV_WRITE_RETRY ){
            retrycnt++;
            mdelay(VIBDRV_DELAY_MS_TIME_150);
            VIBDRV_LOGE("%s(%d) I2C Transfer read Retry Start%d \n",
                __func__, __LINE__, retrycnt);
            goto retry;
        }
        return VIBDRV_RET_NG;
    }

    VIBDRV_LOGI("%s(%d) I2C Transfer OK %d Read: 0x%02x\n",
        __func__, __LINE__, rc, pI2CCmd->prdata[0]);

    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_i2c_read_one_transaction
===========================================================================*/
int vibdrv_i2c_read_one_transaction(struct vibdrv_i2c_cmd_type *pI2CCmd)
{
    int            rc = 0;
    struct i2c_msg msg[2];
#if VIBDRV_DEBUG
    int            i = 0;
#endif
    int            retrycnt=0;

    VIBDRV_LOGI("vibdrv_i2c_read_one_transaction start.\n");

retry:

    if (!g_pTheClient->adapter) {
        VIBDRV_LOGE("%s(%d) fault (Adapter Error)\n", __func__, __LINE__);
        return VIBDRV_RET_NG;
    }

    msg[0].addr = g_pTheClient->addr;
    msg[0].flags = 0;
    msg[0].len = pI2CCmd->wlen;
#if VIBDRV_DEBUG
    for (i = 0; i < pI2CCmd->wlen; i++) {
        VIBDRV_LOGI("%s(%d) I2C Transfer_normal 0x%02x \n", __func__, __LINE__, pI2CCmd->pwdata[i]);
    }
#endif
    msg[0].buf = pI2CCmd->pwdata;

    msg[1].addr  = g_pTheClient->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len   = pI2CCmd->rlen;
    msg[1].buf   = pI2CCmd->prdata;
    rc = i2c_transfer(g_pTheClient->adapter, msg, 2);
    if (rc != 2) {
        VIBDRV_LOGE("%s(%d) I2C Transfer Error %d write8:[%02x-%02x], Read8:[%02x-%02x]\n",
            __func__, __LINE__, rc, msg[0].addr, msg[0].len, msg[1].addr, msg[1].len);
        if( retrycnt < VIBDRV_WRITE_RETRY ){
            retrycnt++;
            mdelay(VIBDRV_DELAY_MS_TIME_150);
            VIBDRV_LOGE("%s(%d) I2C Transfer read Retry Start%d \n",
                __func__, __LINE__, retrycnt);
            goto retry;
        }
        return VIBDRV_RET_NG;
    }

    VIBDRV_LOGI("%s(%d) I2C Transfer OK %d Read: 0x%02x\n",
        __func__, __LINE__, rc, pI2CCmd->prdata[0]);

    return VIBDRV_RET_OK;
}
