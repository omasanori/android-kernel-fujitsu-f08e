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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <crypto/hash.h>
#include <crypto/md5.h>

#include <../board-8064.h>

#include <vibdrv.h>
#include <vibdrv_vib.h>
#include <vibdrv_i2c.h>
#include <asm/system_info.h>

/*============================================================================
                        INTERNAL FEATURES
============================================================================*/

/*============================================================================
                        CONSTANTS
============================================================================*/
static const struct _waveform_table waveformtbl[] = {
    {0x28, 0, VIBDRV_WF_TRG_VER, 0xFF, VIBDRV_WF_ID_ERR},
    {0x29, 1, VIBDRV_WF_TRG_VIB, 0x00, VIBDRV_WF_ID_175},
    {0x2B, 2, VIBDRV_WF_TRG_HP1, 0x00, VIBDRV_WF_ID_175},
    {0x2D, 2, VIBDRV_WF_TRG_HP2, 0x00, VIBDRV_WF_ID_175},
    {0x2F, 2, VIBDRV_WF_TRG_HP1, 0x01, VIBDRV_WF_ID_175},
    {0x31, 2, VIBDRV_WF_TRG_HP2, 0x01, VIBDRV_WF_ID_175},
    {0x33, 2, VIBDRV_WF_TRG_HP1, 0x02, VIBDRV_WF_ID_175},
    {0x35, 2, VIBDRV_WF_TRG_HP2, 0x02, VIBDRV_WF_ID_175},
    {0x39, 1, VIBDRV_WF_TRG_VIB, 0x04, VIBDRV_WF_ID_172},
    {0x3B, 2, VIBDRV_WF_TRG_HP1, 0x04, VIBDRV_WF_ID_172},
    {0x3D, 2, VIBDRV_WF_TRG_HP2, 0x04, VIBDRV_WF_ID_172},
    {0x3F, 2, VIBDRV_WF_TRG_HP1, 0x05, VIBDRV_WF_ID_172},
    {0x41, 2, VIBDRV_WF_TRG_HP2, 0x05, VIBDRV_WF_ID_172},
    {0x43, 2, VIBDRV_WF_TRG_HP1, 0x06, VIBDRV_WF_ID_172},
    {0x45, 2, VIBDRV_WF_TRG_HP2, 0x06, VIBDRV_WF_ID_172},
    {0x49, 1, VIBDRV_WF_TRG_VIB, 0x08, VIBDRV_WF_ID_178},
    {0x4B, 2, VIBDRV_WF_TRG_HP1, 0x08, VIBDRV_WF_ID_178},
    {0x4D, 2, VIBDRV_WF_TRG_HP2, 0x08, VIBDRV_WF_ID_178},
    {0x4F, 2, VIBDRV_WF_TRG_HP1, 0x09, VIBDRV_WF_ID_178},
    {0x51, 2, VIBDRV_WF_TRG_HP2, 0x09, VIBDRV_WF_ID_178},
    {0x53, 2, VIBDRV_WF_TRG_HP1, 0x0A, VIBDRV_WF_ID_178},
    {0x55, 2, VIBDRV_WF_TRG_HP2, 0x0A, VIBDRV_WF_ID_178},
};

/*============================================================================
                        STRUCTURE
============================================================================*/


/*============================================================================
                        EXTERNAL VARIABLES DEFINITIONS
============================================================================*/
static bool      g_bAmpEnabled = false;
static int g_Firmware_state = VIBDRV_WRITE_COMPLETE;

/*============================================================================
                        INTERNAL API DECLARATIONS
============================================================================*/

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_firmeware_check_md5
===========================================================================*/
static int _vibdrv_firmeware_check_md5(char* data, int size)
{
    int             ret = VIBDRV_RET_MD5_NG;
    unsigned char   md5_digest_calc[HAPTICS_FIRMWARE_MD5];
    unsigned char   md5_digest_bkup[HAPTICS_FIRMWARE_MD5];
    struct crypto_shash *md5;
    struct shash_desc *sdescmd5;
    int rc;
    unsigned int worksize;

    memcpy( &md5_digest_bkup[0], data+HAPTICS_FIRMWARE_MD5_POS, HAPTICS_FIRMWARE_MD5 );
    
    md5 = crypto_alloc_shash("md5", 0, 0);
    if (IS_ERR(md5)) {
        ret = VIBDRV_RET_MD5_NG;
        VIBDRV_LOGE("_vibdrv_firmeware_check_md5(crypto_alloc_shash).\n");
        goto ON_ERR;
    }

    worksize = sizeof(struct shash_desc) + crypto_shash_descsize(md5);
    sdescmd5 = kmalloc(worksize, GFP_KERNEL);
    if (!sdescmd5) {
        ret = VIBDRV_RET_MD5_NG;
        crypto_free_shash(md5);
        VIBDRV_LOGE("_vibdrv_firmeware_check_md5(kmalloc).\n");
        goto ON_ERR;
    }

    sdescmd5->tfm = md5;
    sdescmd5->flags = 0x0;

    rc = crypto_shash_init(sdescmd5);
    if (rc) {
        ret = VIBDRV_RET_MD5_NG;
        VIBDRV_LOGE("_vibdrv_firmeware_check_md5(crypto_shash_init).\n");
        goto ON_ERR_FREE;
    }

    rc = crypto_shash_update(sdescmd5, data, HAPTICS_FIRMWARE_MD5_POS);
    if (rc) {
        ret = VIBDRV_RET_MD5_NG;
        VIBDRV_LOGE("_vibdrv_firmeware_check_md5(crypto_shash_update).\n");
        goto ON_ERR_FREE;
    }

    rc = crypto_shash_final(sdescmd5, &md5_digest_calc[0]);
    if (rc) {
        ret = VIBDRV_RET_MD5_NG;
        VIBDRV_LOGE("_vibdrv_firmeware_check_md5(crypto_shash_final).\n");
        goto ON_ERR_FREE;
    }

    if (memcmp(&md5_digest_calc[0], &md5_digest_bkup[0], HAPTICS_FIRMWARE_MD5)) {
        ret = VIBDRV_RET_MD5_NG;
    } else {
        ret = VIBDRV_RET_OK;
    }

ON_ERR_FREE:
    crypto_free_shash(md5);
    kfree(sdescmd5);
ON_ERR:
    return ret;
}

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_firmeware_write_command
===========================================================================*/
static int _vibdrv_firmeware_write_command( int blockno )
{
    int         ret = VIBDRV_RET_OK;
    uint32_t    work_sum=0;
    struct vibdrv_i2c_cmd_type i2c_cmd;
    struct _write_req write_req;
    struct _status status;

    /* setting command parameter */
    write_req.ubytes   = VIBDRV_WRITE_CMD_USIZE;
    write_req.dbytes   = VIBDRV_WRITE_CMD_DSIZE;
    write_req.command  = VIBDRV_WRITE_CMD;
    write_req.data     = blockno;
    work_sum           = (write_req.ubytes+write_req.dbytes+write_req.command+write_req.data);
    write_req.sum      = work_sum & 0xFF;
    
    i2c_cmd.pwdata     = (unsigned char*)&write_req;
    i2c_cmd.wlen       = sizeof(write_req);
    i2c_cmd.prdata     = (unsigned char*)&status;
    i2c_cmd.rlen       = sizeof(status);

    /* data transfer */
    ret = vibdrv_i2c_read(&i2c_cmd);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("vibdrv_i2c_read(read command).\n");
        goto ON_ERR;
    }
    /* check status */
    if( status.status != 0 ){
        ret = VIBDRV_RET_NG;
        VIBDRV_LOGE("vibdrv_i2c_read(status NG:%d).\n",status.status);
        goto ON_ERR;
    }
ON_ERR:
    return ret;
}

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_firmeware_data_command
===========================================================================*/
static int _vibdrv_firmeware_block_data_command( char* block_data, int size )
{
    int         ret = VIBDRV_RET_OK;
    uint32_t    work_sum=0;
    int         i;
    struct vibdrv_i2c_cmd_type i2c_cmd;
    struct _command_req command_req;
    struct _data_transfer *pdata_transfer;
    struct _status status;

    /* setting command parameter */
    command_req.ubytes   = VIBDRV_DATA_CMD_USIZE;
    command_req.dbytes   = VIBDRV_DATA_CMD_DSIZE;
    command_req.command  = VIBDRV_DATA_CMD;
    command_req.sum      = VIBDRV_DATA_CMD_SUM;
    i2c_cmd.pwdata       = (unsigned char*)&command_req;
    i2c_cmd.wlen         = sizeof(struct _command_req);

    /* data transfer */
    ret = vibdrv_i2c_write(&i2c_cmd);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("vibdrv_i2c_read(write command).\n");
        goto ON_ERR;
    }

    /* setting command parameter */
    pdata_transfer = kzalloc(sizeof(struct _data_transfer), GFP_KERNEL);
    if (!pdata_transfer) {
        VIBDRV_LOGE("kzalloc(could not allocate memory).\n");
        goto ON_ERR;
    }
    pdata_transfer->ubytes = VIBDRV_DATA_TRANS_USIZE;
    pdata_transfer->dbytes = VIBDRV_DATA_TRANS_DSIZE;
    for (i = 0; i < VIBDRV_DATA_TRANS_MAX; i++) {
        pdata_transfer->data[i] = block_data[i];
        work_sum += pdata_transfer->data[i];
    }
    work_sum          += pdata_transfer->ubytes;
    work_sum          += pdata_transfer->dbytes;
    pdata_transfer->sum= work_sum & 0xFF;
    i2c_cmd.pwdata     = (unsigned char*)pdata_transfer;
    i2c_cmd.wlen       = sizeof(struct _data_transfer);
    i2c_cmd.prdata     = (unsigned char*)&status;
    i2c_cmd.rlen       = sizeof(struct _status);

    /* data transfer */
    ret = vibdrv_i2c_read(&i2c_cmd);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("vibdrv_i2c_read(read command).\n");
        goto ON_ERR_FREE;
    }
    /* check status */
    if( status.status != 0 ){
        ret = VIBDRV_RET_NG;
        VIBDRV_LOGE("vibdrv_i2c_read(status NG:%d).\n",status.status);
        goto ON_ERR_FREE;
    }

ON_ERR_FREE:
    kfree(pdata_transfer);

ON_ERR:
    return ret;
}

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_firmeware_verify_command
===========================================================================*/
static int _vibdrv_firmeware_verify_command( int blockno )
{
    int         ret = VIBDRV_RET_OK;
    uint32_t    work_sum=0;
    struct vibdrv_i2c_cmd_type i2c_cmd;
    struct _write_req write_req;
    struct _status status;

    /* setting command parameter */
    write_req.ubytes   = VIBDRV_VERIFY_CMD_USIZE;
    write_req.dbytes   = VIBDRV_VERIFY_CMD_DSIZE;
    write_req.command  = VIBDRV_VERIFY_CMD;
    write_req.data     = blockno;
    work_sum           = (write_req.ubytes+write_req.dbytes+write_req.command+write_req.data);
    write_req.sum      = work_sum & 0xFF;

    i2c_cmd.pwdata     = (unsigned char*)&write_req;
    i2c_cmd.wlen       = sizeof(struct _write_req);
    i2c_cmd.prdata     = (unsigned char*)&status;
    i2c_cmd.rlen       = sizeof(struct _status);

    /* data transfer */
    ret = vibdrv_i2c_read(&i2c_cmd);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("vibdrv_i2c_read(read command).\n");
        goto ON_ERR;
    }
    /* check status */
    if( status.status != 0 ){
        VIBDRV_LOGE("vibdrv_i2c_read(status NG:%d).\n",status.status);
        goto ON_ERR;
    }

ON_ERR:
    return ret;
}

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_firmeware_read_command
===========================================================================*/
static int _vibdrv_firmeware_read_command( int blockno, char* block_data, int size, bool verify )
{
    int         ret = VIBDRV_RET_OK;
    uint32_t    work_sum=0;
    int         read_pageno=0;
    int         i=0;
    int         block_data_cnt=0;
    struct vibdrv_i2c_cmd_type i2c_cmd;
    struct _read_req read_req;
    struct _read_transfer *pread_transfer;
    struct _status status;

    for (read_pageno=0; read_pageno<VIBDRV_READ_PAGE_MAX; read_pageno++ ) {

        /* setting command parameter */
        read_req.ubytes   = VIBDRV_READ_CMD_USIZE;
        read_req.dbytes   = VIBDRV_READ_CMD_DSIZE;
        read_req.command  = VIBDRV_READ_CMD;
        read_req.blockno  = blockno;
        read_req.pageno   = read_pageno;
        work_sum          = (read_req.ubytes+read_req.dbytes+read_req.command+read_req.blockno+read_req.pageno);
        read_req.sum      = work_sum & 0xFF;
        i2c_cmd.pwdata    = (unsigned char*)&read_req;
        i2c_cmd.wlen      = sizeof(struct _read_req);

        pread_transfer = kzalloc(sizeof(struct _read_transfer), GFP_KERNEL);
        if (!pread_transfer) {
            VIBDRV_LOGE("kzalloc(could not allocate memory).\n");
            ret=VIBDRV_RET_NG;
            goto ON_ERR;
        }
        pread_transfer->ubytes = VIBDRV_READ_DATA_TRANS_USIZE;
        pread_transfer->dbytes = VIBDRV_READ_DATA_TRANS_DSIZE;
        i2c_cmd.prdata    = (unsigned char*)pread_transfer;
        i2c_cmd.rlen      = sizeof(struct _read_transfer);

        /* data transfer */
        ret = vibdrv_i2c_read(&i2c_cmd);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("vibdrv_i2c_read(read command).\n");
            kfree(pread_transfer);
            ret=VIBDRV_RET_NG;
            goto ON_ERR;
        }

        /* status transfer */
        i2c_cmd.prdata    = (unsigned char*)&status;
        i2c_cmd.rlen      = sizeof(struct _status);
        ret = vibdrv_i2c_one_read(&i2c_cmd);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("vibdrv_i2c_read(read command).\n");
            kfree(pread_transfer);
            ret=VIBDRV_RET_NG;
            goto ON_ERR;
        }
        
        /* check status */
        if( status.status != 0 ){
            VIBDRV_LOGE("vibdrv_i2c_read(status NG:%d).\n",status.status);
            kfree(pread_transfer);
            ret=VIBDRV_RET_RETRY;
            goto ON_ERR;
        }
        
        if( verify ) {
            /* verify block data */
            for (i=0; i<VIBDRV_READ_DATA_TRANS_MAX; i++ ) {
                if( *(block_data+block_data_cnt+i) != *(pread_transfer->data+i) ){
                    VIBDRV_LOGE("Verify NG block_data:%x read:%x.cnt:%d.\n",*(block_data+block_data_cnt+i),*(pread_transfer->data+i),i);
                    kfree(pread_transfer);
                    ret=VIBDRV_RET_RETRY;
                    goto ON_ERR;
                }
            }
        } else {
            memcpy((block_data+block_data_cnt),pread_transfer->data,VIBDRV_READ_DATA_TRANS_MAX);
        }
        block_data_cnt+=VIBDRV_READ_DATA_TRANS_MAX;
        kfree(pread_transfer);
    }

ON_ERR:
    return ret;
}

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_firmeware_boot_swap
===========================================================================*/
static int _vibdrv_firmeware_boot_swap( void )
{
    int         ret = VIBDRV_RET_OK;
    struct vibdrv_i2c_cmd_type i2c_cmd;
    struct _command_req command_req;
    struct _status status;

    /* setting command parameter */
    command_req.ubytes   = VIBDRV_BOOTSWAP_USIZE;
    command_req.dbytes   = VIBDRV_BOOTSWAP_DSIZE;
    command_req.command  = VIBDRV_BOOTSWAP_CMD;
    command_req.sum      = VIBDRV_BOOTSWAP_SUM;
    i2c_cmd.pwdata       = (unsigned char*)&command_req;
    i2c_cmd.wlen         = sizeof(struct _command_req);
    i2c_cmd.prdata       = (unsigned char*)&status;
    i2c_cmd.rlen         = sizeof(struct _status);

    /* data transfer */
    ret = vibdrv_i2c_read(&i2c_cmd);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("vibdrv_i2c_read(read command).\n");
        goto ON_ERR;
    }
    /* check status */
    if( status.status != 0 ){
        ret = VIBDRV_RET_NG;
        VIBDRV_LOGE("vibdrv_i2c_read(status NG:%d).\n",status.status);
        goto ON_ERR;
    }
ON_ERR:
    return ret;
}

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_change_firmeware_mode
===========================================================================*/
static void _vibdrv_change_firmeware_mode( void )
{
    gpio_set_value(vibdrv_get_gpio_rst(),VIBDRV_GPIO_LEVEL_LOW);
    udelay(VIBDRV_DELAY_US_TIME_10);
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_PRESS),VIBDRV_GPIO_LEVEL_LOW);
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_RELEASE),VIBDRV_GPIO_LEVEL_LOW);
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_VIB), VIBDRV_GPIO_LEVEL_LOW);

    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_PRESS),VIBDRV_GPIO_LEVEL_HIGH);
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_RELEASE),VIBDRV_GPIO_LEVEL_HIGH);
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_VIB), VIBDRV_GPIO_LEVEL_HIGH);
    udelay(VIBDRV_DELAY_US_TIME_10);
    gpio_set_value(vibdrv_get_gpio_rst(),VIBDRV_GPIO_LEVEL_HIGH);

    // 50ms delay
    schedule_timeout_interruptible(msecs_to_jiffies(VIBDRV_DELAY_MS_TIME_50));
    return ;
}

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_change_normal_mode
===========================================================================*/
static void _vibdrv_change_normal_mode( void )
{
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_PRESS),VIBDRV_GPIO_LEVEL_LOW);
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_RELEASE),VIBDRV_GPIO_LEVEL_LOW);
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_VIB), VIBDRV_GPIO_LEVEL_LOW);
    gpio_set_value(vibdrv_get_gpio_rst(),VIBDRV_GPIO_LEVEL_LOW);
    udelay(VIBDRV_DELAY_US_TIME_10);
    gpio_set_value(vibdrv_get_gpio_rst(),VIBDRV_GPIO_LEVEL_HIGH);

    // 150ms delay
    schedule_timeout_interruptible(msecs_to_jiffies(VIBDRV_DELAY_MS_TIME_150));
    return ;
}

/*===========================================================================
    LOCAL FUNCTION      _vibdrv_get_waveform_table
===========================================================================*/
static int _vibdrv_get_waveform_table( int8_t trg_type, int8_t id_value, struct _waveform_table *pwftable )
{
    int         ret = VIBDRV_RET_NG;
    int         loop = sizeof(waveformtbl)/sizeof(struct _waveform_table);
    int         i;
    
    for(i=0;i<loop;i++){
        if( trg_type == waveformtbl[i].trg_type ) {
            if( ( id_value == waveformtbl[i].id_value ) || ( trg_type == VIBDRV_WF_TRG_VER ) ){
                memcpy(pwftable, &waveformtbl[i], sizeof(struct _waveform_table));
                if( pwftable->trg_type == VIBDRV_WF_TRG_VER ) {
                    pwftable->id_value = id_value;
                }
                ret = VIBDRV_RET_OK;
                break;
            }
        }
    }
    return ret;
}


/*===========================================================================
    LOCAL FUNCTION      _vibdrv_get_access_table_offset
===========================================================================*/
static int _vibdrv_get_access_table_offset( struct _waveform_table *pwftable, int *mng_offset )
{
    int         ret = VIBDRV_RET_OK;

    switch(pwftable->trg_type){
    case VIBDRV_WF_TRG_VIB:
        *mng_offset = (pwftable->id_value*HAPTICS_ACCESS_TABLE_ID)+HAPTICS_ACCESS_VIB_ADR;
        break;
    case VIBDRV_WF_TRG_HP1:
        *mng_offset = (pwftable->id_value*HAPTICS_ACCESS_TABLE_ID)+HAPTICS_ACCESS_HP1_ADR;
        break;
    case VIBDRV_WF_TRG_HP2:
        *mng_offset = (pwftable->id_value*HAPTICS_ACCESS_TABLE_ID)+HAPTICS_ACCESS_HP2_ADR;
        break;
    case VIBDRV_WF_TRG_VER:
        *mng_offset = HAPTICS_ACCESS_VER_ADR;
        break;
    default:
        VIBDRV_LOGE("_vibdrv_get_access_table_offset faild trg:%x.\n",pwftable->trg_type);
        ret = VIBDRV_RET_NG;
    }
    VIBDRV_LOGI("_vibdrv_get_access_table_offset offset: %x id:%x trg:%x.\n",*mng_offset,pwftable->id_value, pwftable->trg_type);
    return ret;
}


/*===========================================================================
    LOCAL FUNCTION      _vibdrv_access_table_backup
===========================================================================*/
static int _vibdrv_access_table_backup( struct _waveform_table *pwftable, char *access_data )
{
    int         ret = VIBDRV_RET_OK;
    int         mng_offset;
    unsigned char   access_id[HAPTICS_ACCESS_TABLE_ID];

    VIBDRV_LOGI("_vibdrv_access_table_backup.\n");

    /* 1. read command(access table backup) */
    ret = _vibdrv_firmeware_read_command(HAPTICS_ACCESS_TABLE_POS,access_data,VIBDRV_DATA_TRANS_MAX,false);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_read_command faild.\n");
        goto ON_ERR;
    }
    
    /* 2. set access table*/
    ret = _vibdrv_get_access_table_offset(pwftable,&mng_offset);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_get_access_table_offset faild.\n");
        goto ON_ERR;
    }
    if( pwftable->trg_type == VIBDRV_WF_TRG_VER ) {
        /* no backup */
        *(access_data + mng_offset) = pwftable->id_value;
        return VIBDRV_RET_OK;
    } else {
        memcpy(&access_id[0],(access_data+mng_offset),HAPTICS_ACCESS_TABLE_ID);
        VIBDRV_LOGI("ID:%x%x%x%x%x%x%x%x.\n",
                    access_id[0],access_id[1],access_id[2],access_id[3],
                    access_id[4],access_id[5],access_id[6],access_id[7]);
        memset((access_data+mng_offset),0xFF,HAPTICS_ACCESS_TABLE_ID);
    }
    
    /* 3.write command(access table) */
    ret = _vibdrv_firmeware_write_command(HAPTICS_ACCESS_TABLE_POS);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_write_command faild.\n");
        goto ON_ERR;
    }

    /* 4.data command(access table) */
    ret = _vibdrv_firmeware_block_data_command(access_data,VIBDRV_DATA_TRANS_MAX);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_block_data_command faild.\n");
        goto ON_ERR;
    }

    /* 5.verify command(access table) */
    ret = _vibdrv_firmeware_verify_command(HAPTICS_ACCESS_TABLE_POS);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_write_command faild.\n");
        goto ON_ERR;
    }

    /* 6. read command(access table verify) */
    ret = _vibdrv_firmeware_read_command(HAPTICS_ACCESS_TABLE_POS,access_data,VIBDRV_DATA_TRANS_MAX,true);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_read_command faild.\n");
        goto ON_ERR;
    }
    
ON_ERR:
    return ret;
}


/*===========================================================================
    LOCAL FUNCTION      _vibdrv_access_table_restore
===========================================================================*/
static int _vibdrv_access_table_restore( struct _waveform_table *pwftable, char *access_data )
{
    int         ret = VIBDRV_RET_OK;
    int         mng_offset;
    unsigned char   access_id[HAPTICS_ACCESS_TABLE_ID];

    VIBDRV_LOGI("_vibdrv_access_table_restore.\n");

   /* 1.write command(access table) */
    ret = _vibdrv_firmeware_write_command(HAPTICS_ACCESS_TABLE_POS);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_write_command faild.\n");
        goto ON_ERR;
    }

    /* 2. set access table*/
    ret = _vibdrv_get_access_table_offset(pwftable,&mng_offset);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_get_access_table_offset faild.\n");
        goto ON_ERR;
    }
    if( pwftable->trg_type != VIBDRV_WF_TRG_VER ) {
        access_id[0] = HAPTICS_ACCESS_MARK_DDATA;
        access_id[1] = HAPTICS_ACCESS_MARK_UDATA;
        access_id[2] = pwftable->blockno;
        access_id[3] = 0x00;
        access_id[4] = ~pwftable->blockno;
        access_id[5] = 0xFF;
        access_id[6] = HAPTICS_ACCESS_MARK_UDATA;
        access_id[7] = HAPTICS_ACCESS_MARK_DDATA;
        VIBDRV_LOGI("ID:%x%x%x%x%x%x%x%x.\n",
                    access_id[0],access_id[1],access_id[2],access_id[3],
                    access_id[4],access_id[5],access_id[6],access_id[7]);
        memcpy((access_data+mng_offset),&access_id[0],HAPTICS_ACCESS_TABLE_ID);
    }

    /* 3.data command(access table) */
    ret = _vibdrv_firmeware_block_data_command(access_data,VIBDRV_DATA_TRANS_MAX);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_block_data_command faild.\n");
        goto ON_ERR;
    }

    /* 4.verify command(access table) */
    ret = _vibdrv_firmeware_verify_command(HAPTICS_ACCESS_TABLE_POS);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_write_command faild.\n");
        goto ON_ERR;
    }

    /* 5. read command(access table verify) */
    ret = _vibdrv_firmeware_read_command(HAPTICS_ACCESS_TABLE_POS,access_data,VIBDRV_DATA_TRANS_MAX,true);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_read_command faild.\n");
        goto ON_ERR;
    }

ON_ERR:
    return ret;
}


/*===========================================================================
    FUNCTION      _vibdrv_write_waveform_block
===========================================================================*/
int _vibdrv_write_waveform_block(int8_t trg_type, int8_t id_value, char* data, int size)
{
    int         ret = VIBDRV_RET_OK;
    char       *mng_data;
    int         i;
    char       *work_data;
    int8_t      write_blockno=0;
    int         write_size=0;
    int         copy_size=0;
    struct _waveform_table wftable;
    
    VIBDRV_LOGI("_vibdrv_write_waveform_block trg: %x id:%x size:%x.\n",trg_type,id_value,size);

    /* waveform table */
    ret = _vibdrv_get_waveform_table( trg_type, id_value, &wftable);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_get_waveform_table.\n");
        ret=VIBDRV_RET_NG;
        goto ON_ERR;
    }

    mng_data = kzalloc( VIBDRV_DATA_TRANS_MAX, GFP_KERNEL);
    if (!mng_data) {
        VIBDRV_LOGE("kzalloc(could not allocate memory).\n");
        ret=VIBDRV_RET_NG;
        goto ON_ERR;
    }

    /* 1. access table backup */
    ret = _vibdrv_access_table_backup( &wftable, mng_data);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_access_table_backup faild.\n");
        goto ON_ERR_FREE;
    }

    write_blockno = wftable.blockno;
    for( i=0; i<wftable.maxblockcnt; i++) {

        /* setting write parameter */
        if( write_size >= size ) {
            /* write complete */
            VIBDRV_LOGI("_vibdrv_write_waveform_block complete write_size:%x.\n",write_size);
            break;
        }else{
            work_data = &data[write_size];
            copy_size = size - write_size;
            if( copy_size > VIBDRV_DATA_TRANS_MAX ) {
                copy_size = VIBDRV_DATA_TRANS_MAX;
            }
            VIBDRV_LOGI("_vibdrv_write_waveform_block write_size:%x write_blockno:%x.\n",write_size,write_blockno);
        }

        /* 2-1.write command(wave data) */
        ret = _vibdrv_firmeware_write_command( write_blockno );
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_write_command faild.\n");
            goto ON_ERR_FREE;
        }

        /* 2-2.data command(wave data) */
        ret = _vibdrv_firmeware_block_data_command( work_data, copy_size);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_block_data_command faild.\n");
            goto ON_ERR_FREE;
        }

        /* 2-3.verify command(wave data) */
        ret = _vibdrv_firmeware_verify_command( write_blockno );
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_write_command faild.\n");
            goto ON_ERR_FREE;
        }

        /* 2-4. read command(wave data) */
        ret = _vibdrv_firmeware_read_command( write_blockno, work_data, copy_size, true);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_read_command faild.\n");
            goto ON_ERR_FREE;
        }
        /* write complete 1block */
        write_size += copy_size;
        write_blockno++;
    }

    /* 3. access table restore */
    ret = _vibdrv_access_table_restore( &wftable, mng_data);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_access_table_backup faild.\n");
        goto ON_ERR_FREE;
    }

ON_ERR_FREE:
    kfree(mng_data);

ON_ERR:
    VIBDRV_LOGI("_vibdrv_write_waveform_block end.\n");
    return ret;
}


/*===========================================================================
    FUNCTION      vibdrv_haptics_suspend
===========================================================================*/
int vibdrv_haptics_suspend(void)
{
    VIBDRV_LOGI("vibdrv_haptics_suspend.\n");

    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_INT), VIBDRV_GPIO_LEVEL_LOW);
    udelay(VIBDRV_DELAY_US_TIME);
    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_haptics_resume
===========================================================================*/
int vibdrv_haptics_resume(void)
{
    VIBDRV_LOGI("vibdrv_haptics_resume.\n");

    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_INT), VIBDRV_GPIO_LEVEL_HIGH);
    udelay(VIBDRV_DELAY_US_TIME);
    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_haptics_resume
===========================================================================*/
int vibdrv_haptics_enable_amp(void)
{
    VIBDRV_LOGI("vibdrv_haptics_enable_amp.\n");

    if (!g_bAmpEnabled){
        g_bAmpEnabled = true;

        VIBDRV_LOGI("amp start.\n");
        gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_VIB), VIBDRV_GPIO_LEVEL_HIGH);
        udelay(VIBDRV_DELAY_US_TIME);
    }
    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_haptics_resume
===========================================================================*/
int vibdrv_haptics_disable_amp(void)
{
   VIBDRV_LOGI("vibdrv_haptics_disable_amp.\n");

   if (g_bAmpEnabled){
        g_bAmpEnabled = false;

        VIBDRV_LOGI("amp end.\n");
        gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_VIB), VIBDRV_GPIO_LEVEL_LOW);
        udelay(VIBDRV_DELAY_US_TIME);
    }
    return VIBDRV_RET_OK;
}

/*===========================================================================
    FUNCTION      vibdrv_write_firmware
===========================================================================*/
int vibdrv_write_firmware(char* data, int size)
{
    int                 ret = VIBDRV_RET_OK;
    int                 write_size=0;
    int                 copy_size=0;
    char                *work_data;
    int                 write_blockno=0;
    int                 retry=0;
    
    VIBDRV_LOGI("vibdrv_write_firmware size:%x.\n",size);
    
    vibdrv_i2c_lock();
    _vibdrv_change_firmeware_mode();
    
    /* 0.check md5 */
    ret = _vibdrv_firmeware_check_md5(data,size);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_check_md5 faild.\n");
        goto ON_ERR;
    }
    
    size = size - HAPTICS_FIRMWARE_MD5;
    write_blockno = HAPTICS_FIRMWARE_POS;
    while(1) {
        /* setting write parameter */
        if( write_size >= size ) {
            /* write complete */
            VIBDRV_LOGI("vibdrv_write_firmware complete write_size:%x.\n",write_size);
            break;
        }else{
            VIBDRV_LOGI("vibdrv_write_firmware2 write_size:%x write_blockno:%x.\n",write_size,write_blockno);
            work_data = &data[write_size];
            copy_size = size - write_size;
            if( copy_size > VIBDRV_DATA_TRANS_MAX ) {
                copy_size = VIBDRV_DATA_TRANS_MAX;
            }
        }
ON_RETRY:
        /* 1.write command */
        ret = _vibdrv_firmeware_write_command(write_blockno);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_write_command faild.\n");
            goto ON_ERR;
        }

        /* 2.data command(write) */
        ret = _vibdrv_firmeware_block_data_command(work_data,copy_size);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_block_data_command faild.\n");
            goto ON_ERR;
        }

        /* 3.verify command */
        ret = _vibdrv_firmeware_verify_command(write_blockno);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_write_command faild.\n");
            goto ON_ERR;
        }

        /* 4.read command(verify) */
        ret = _vibdrv_firmeware_read_command(write_blockno, work_data, copy_size, true);
        if( ret == VIBDRV_RET_RETRY ){
            VIBDRV_LOGE("_vibdrv_firmeware_read_command retry.\n");
            if( retry > VIBDRV_WRITE_RETRY){
                VIBDRV_LOGE("vibdrv_write_firmware retry max faild.\n");
                goto ON_ERR;
            } else {
                retry++;
                goto ON_RETRY;
            }
        }else if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_read_command faild.\n");
            goto ON_ERR;
        }
        
        /* write complete 1block */
        write_size += copy_size;
        write_blockno++;
    }
    /* 5.bootswap command */
    ret = _vibdrv_firmeware_boot_swap();
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_boot_swap faild.\n");
        goto ON_ERR;
    }

ON_ERR:
    _vibdrv_change_normal_mode();
    vibdrv_i2c_unlock();

    VIBDRV_LOGI("vibdrv_write_firmware end.\n");
    return ret;
}


/*===========================================================================
    FUNCTION      vibdrv_haptics_resume
===========================================================================*/
int vibdrv_get_firmware_version(int8_t *major, int8_t *minor)
{
    int                 ret = VIBDRV_RET_OK;
    struct vibdrv_i2c_cmd_type i2c_cmd;
    struct _command_req get_ver_req;
    struct _firm_transfer get_ver_ans;
    
    VIBDRV_LOGI("vibdrv_get_firmware_version.\n");

    /* command transfer(set data) */
    vibdrv_i2c_lock();
    get_ver_req.ubytes  = VIBDRV_GET_VER_CMD_USIZE;
    get_ver_req.dbytes  = VIBDRV_GET_VER_CMD_DSIZE;
    get_ver_req.command = VIBDRV_GET_VER_CMD;
    get_ver_req.sum     = VIBDRV_GET_VER_CMD_SUM;
    i2c_cmd.pwdata      = (unsigned char*)&get_ver_req;
    i2c_cmd.wlen        = sizeof(struct _command_req);
    i2c_cmd.prdata      = (unsigned char*)&get_ver_ans;
    i2c_cmd.rlen        = sizeof(struct _firm_transfer);
    /* data transfer */
    ret = vibdrv_i2c_read(&i2c_cmd);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("vibdrv_i2c_read(command transfer).\n");
        ret = VIBDRV_RET_NG;
        goto ON_ERR;
    }
    
    *major = get_ver_ans.uver;
    *minor = get_ver_ans.dver;

ON_ERR:
    vibdrv_i2c_unlock();
    return ret;
}

/*===========================================================================
    FUNCTION      vibdrv_set_int_status
===========================================================================*/
int vibdrv_set_int_status(int8_t in_status, int8_t *out_status)
{
    int               ret = VIBDRV_RET_OK;
    int  int_edge;

    VIBDRV_LOGI("vibdrv_set_int_status.\n");

    switch( in_status ) {
    case VIBDRV_INT_LOW:
        vibdrv_haptics_suspend();
        break;
    case VIBDRV_INT_HIGH:
        vibdrv_haptics_resume();
        break;
    default:
        *out_status = VIBDRV_INT_ERR;
        ret = VIBDRV_RET_NG;
        VIBDRV_LOGE("vibdrv_set_int_status(in status NG:%d).\n",in_status);
        goto ON_ERR;
    }
    int_edge = gpio_get_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_INT));
    if( int_edge < 0 ) {
        *out_status = VIBDRV_INT_ERR;
        ret = VIBDRV_RET_NG;
        VIBDRV_LOGE("vibdrv_set_int_status(gpio_get_value NG:%x).\n",int_edge);
        goto ON_ERR;
    }
    *out_status = int_edge;

ON_ERR:
    return ret;
}

/*===========================================================================
    FUNCTION      vibdrv_write_waveform
===========================================================================*/
int vibdrv_write_waveform(struct _waveform_data_table *waveform, int waveform_cnt)
{
    int          ret = VIBDRV_RET_OK;
    struct _waveform_data_table *pwaveform;
    int     i;
    
    vibdrv_i2c_lock();
    _vibdrv_change_firmeware_mode();

    for(i=0;i<waveform_cnt;i++){
        pwaveform = &waveform[i];
        ret = _vibdrv_write_waveform_block(pwaveform->trg_type,pwaveform->id_value,
                                           &pwaveform->waveform_data[0],pwaveform->waveform_size);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_write_waveform_block faild.\n");
            goto ON_ERR;
        }
    }

ON_ERR:
    _vibdrv_change_normal_mode();
    vibdrv_i2c_unlock();

    return ret;
}


/*===========================================================================
    FUNCTION      vibdrv_check_waveform
===========================================================================*/
int vibdrv_check_waveform(int8_t type, int8_t table_id)
{
    int          ret = VIBDRV_RET_OK;
    char         *mng_data;
    struct _waveform_table wftable;
    unsigned char   access_id[HAPTICS_ACCESS_TABLE_ID];
    unsigned char   access_check[HAPTICS_ACCESS_TABLE_ID];
    int         mng_offset;

    VIBDRV_LOGI("vibdrv_check_waveform trg: %x id:%x.\n",type,table_id);
    
    vibdrv_i2c_lock();
    _vibdrv_change_firmeware_mode();

    /* waveform table */
    ret = _vibdrv_get_waveform_table(type,table_id,&wftable);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_get_waveform_table.\n");
        ret=VIBDRV_RET_NG;
        goto ON_ERR;
    }

    mng_data = kzalloc(VIBDRV_DATA_TRANS_MAX, GFP_KERNEL);
    if (!mng_data) {
        VIBDRV_LOGE("kzalloc(could not allocate memory).\n");
        ret=VIBDRV_RET_NG;
        goto ON_ERR;
    }

    /* 1. read command(access table) */
    ret = _vibdrv_firmeware_read_command(HAPTICS_ACCESS_TABLE_POS,mng_data,VIBDRV_DATA_TRANS_MAX,false);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_firmeware_read_command faild.\n");
        goto ON_ERR_FREE;
    }

    /* 2. set access table*/
    ret = _vibdrv_get_access_table_offset(&wftable,&mng_offset);
    if( ret != VIBDRV_RET_OK ){
        VIBDRV_LOGE("_vibdrv_get_access_table_offset faild.\n");
        goto ON_ERR_FREE;
    }
    if( wftable.trg_type == VIBDRV_WF_TRG_VER ) {
        VIBDRV_LOGE("trg_type faild trg:%x.\n",wftable.trg_type);
        ret = VIBDRV_RET_NG;
        goto ON_ERR_FREE;
    } else {
        memcpy(&access_id[0],(mng_data+mng_offset),HAPTICS_ACCESS_TABLE_ID);
        VIBDRV_LOGI("ID:%x%x%x%x%x%x%x%x.\n",
                    access_id[0],access_id[1],access_id[2],access_id[3],
                    access_id[4],access_id[5],access_id[6],access_id[7]);
    }

    /* 3. check access table*/
    access_check[0] = HAPTICS_ACCESS_MARK_DDATA;
    access_check[1] = HAPTICS_ACCESS_MARK_UDATA;
    access_check[2] = wftable.blockno;
    access_check[3] = 0x00;
    access_check[4] = ~wftable.blockno;
    access_check[5] = 0xFF;
    access_check[6] = HAPTICS_ACCESS_MARK_UDATA;
    access_check[7] = HAPTICS_ACCESS_MARK_DDATA;
    VIBDRV_LOGI("ID:%x%x%x%x%x%x%x%x.\n",
                access_check[0],access_check[1],access_check[2],access_check[3],
                access_check[4],access_check[5],access_check[6],access_check[7]);

    if (memcmp(&access_check[0], &access_id[0], HAPTICS_ACCESS_TABLE_ID)) {
        VIBDRV_LOGE("memcmp faild.\n");
        ret = VIBDRV_RET_NG;
    } else {
        ret = VIBDRV_RET_OK;
    }

ON_ERR_FREE:
    kfree(mng_data);

ON_ERR:
    _vibdrv_change_normal_mode();
    vibdrv_i2c_unlock();

    return ret;
}

/*===========================================================================
    FUNCTION      vibdrv_get_gpio_rst
===========================================================================*/
uint8_t vibdrv_get_gpio_rst(void)
{
    uint8_t rst_gpio=VIBDRV_GPIO_HAPTICS_RST_1;

    if( (system_rev & 0xF0) == VIBDRV_DEVICE_TYPE_FJDEV005 ) {
       if( (system_rev & 0x0F) < VIBDRV_DEVICE_STATUS_2_1_2 ) {
            rst_gpio = VIBDRV_GPIO_HAPTICS_RST_0;
        }
    }

    VIBDRV_LOGI("vibdrv_get_gpio_rst system_rev:%x rst_gpio:%x.\n",system_rev,rst_gpio);
    return rst_gpio;
}

/*===========================================================================
    FUNCTION      vibdrv_set_state
===========================================================================*/
void vibdrv_set_state(int8_t state)
{
    VIBDRV_LOGI("vibdrv_set_state state:%x.\n",state);
    g_Firmware_state = state;
    return;
}

/*===========================================================================
    FUNCTION      vibdrv_check_state
===========================================================================*/
bool vibdrv_check_state(void)
{
    bool ret=true;

    if(g_Firmware_state == VIBDRV_WRITING){
        ret=false;
    }
    return ret;
}


/*===========================================================================
    FUNCTION      vibdrv_get_state
===========================================================================*/
int8_t vibdrv_get_state(void)
{
    return g_Firmware_state;
}

/*===========================================================================
    FUNCTION      vibdrv_check_waveform_param
===========================================================================*/
bool vibdrv_check_waveform_param(int8_t trg_type, int8_t id_value)
{
    struct _waveform_table wftable;
    bool ret=false;
    
    /* waveform table */
    ret = _vibdrv_get_waveform_table( trg_type, id_value, &wftable);
    if( ret == VIBDRV_RET_OK ){
        ret=true;
    }
    return ret;
}


#if VIBDRV_DEBUG
/*===========================================================================
    FUNCTION      vibdrv_read_firmware
===========================================================================*/
int vibdrv_read_firmware(char* data, int size, int8_t blockno)
{
    int         ret = VIBDRV_RET_OK;
    int         read_size=0;
    int         copy_size=0;
    char        *work_data;
    int         read_blockno=0;

    VIBDRV_LOGI("vibdrv_write_firmware size:%x.\n",size);
    
    vibdrv_i2c_lock();
    _vibdrv_change_firmeware_mode();

    read_blockno = blockno;
    while(1) {
        /* setting write parameter */
        if( read_size >= size ) {
            /* write complete */
            VIBDRV_LOGI("vibdrv_read_firmware complete read_size:%x.\n",read_size);
            break;
        }else{
            work_data = &data[read_size];
            copy_size = size - read_size;
            if( copy_size > VIBDRV_DATA_TRANS_MAX ) {
                copy_size = VIBDRV_DATA_TRANS_MAX;
            }
        }
        VIBDRV_LOGI("vibdrv_write_firmware read_size:%x read_blockno:%x.\n",read_size,read_blockno);

        /* read command */
        ret = _vibdrv_firmeware_read_command(read_blockno,work_data,VIBDRV_DATA_TRANS_MAX,false);
        if( ret != VIBDRV_RET_OK ){
            VIBDRV_LOGE("_vibdrv_firmeware_read_command faild.\n");
            goto ON_ERR;
        }
        
        read_size += copy_size;
        read_blockno++;
    }

ON_ERR:
    _vibdrv_change_normal_mode();
    vibdrv_i2c_unlock();

    VIBDRV_LOGI("vibdrv_write_firmware end.\n");
    return ret;
}
#endif //VIBDRV_DEBUG
