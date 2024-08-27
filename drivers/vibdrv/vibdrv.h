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

#ifndef _VIBDRV_H
#define _VIBDRV_H

/* debug option */
#define VIBDRV_DEBUG 0

/* Constants */
#define VIBDRV_MODULE_NAME        "vibdrv"
#define VIBDRV                    "/dev/"VIBDRV_MODULE_NAME

/* define the HIGH,LOW */
#define VIBDRV_GPIO_LEVEL_HIGH    (1)
#define VIBDRV_GPIO_LEVEL_LOW     (0)

/* haptics firmware */
#define HAPTICS_FIRMWARE_MD5      (0x10)
#define HAPTICS_FIRMWARE_MD5_POS  (0x17000)
#define HAPTICS_FIRMWARE_SIZE     (0x17000 + HAPTICS_FIRMWARE_MD5)
#define HAPTICS_WAVEFORM_SIZE     (0x800)
#define HAPTICS_WAVE_TABLE_MAX    (0x37)

/* Set the return value */
enum vibdrv_return_val {
    VIBDRV_RET_OK                 = 0x00,   /* Normal */

    VIBDRV_RET_MD5_NG             = 0xFC,   /* MD5 NG */
    VIBDRV_RET_NG                 = 0xFD,   /* NG */
    VIBDRV_ERR_NOANW              = 0xFE,   /* no answer from application */
    VIBDRV_RET_RETRY              = 0xFF,   /* retry(static error) */
};

/* Set the fw write return value */
enum vibdrv_writing_status {
    VIBDRV_WRITE_COMPLETE         = 0x00,
    VIBDRV_WRITING                = 0x01,
    VIBDRV_WRITE_NG               = 0xFE,
};

/* waveform trg type */
enum vibdrv_waveform_type {
    VIBDRV_WF_TRG_VIB             = 0x00,
    VIBDRV_WF_TRG_HP1             = 0x01,
    VIBDRV_WF_TRG_HP2             = 0x02,
    VIBDRV_WF_TRG_VER             = 0x03,
};

/* check waveform type */
enum vibdrv_check_wf_type {
    VIBDRV_CHECK_VIB              = 0x00,
    VIBDRV_CHECK_HAPTICS1         = 0x01,
    VIBDRV_CHECK_HAPTICS2         = 0x02,
};

/* int pin status */
enum vibdrv_int_status_id {
    VIBDRV_INT_LOW                = 0x00,
    VIBDRV_INT_HIGH               = 0x01,
    VIBDRV_INT_ERR                = 0xFE,
};

/* waveform id */
enum vibdrv_waveform_id {
    VIBDRV_WF_ID_175              = 0x00,
    VIBDRV_WF_ID_172              = 0x01,
    VIBDRV_WF_ID_178              = 0x02,
    VIBDRV_WF_ID_ERR              = 0xFE,
};

/* struct write firmware */
typedef struct vibdrv_firm_data {
    int32_t  firm_size;                     /* input firmware size  */
    char     *firm_data;                    /* input firmware data  */
    int8_t   return_val;                    /* output return        */
} vibdrv_firm_data_t;

/* struct get firmware version */
typedef struct vibdrv_firm_version {
    int8_t   major_ver;                     /* output major version */
    int8_t   minor_ver;                     /* output minor version */
} vibdrv_firm_version_t;

/* struct write waveform */
typedef struct vibdrv_waveform_data {
    int8_t   trg_type;                      /* input waveform type(vibdrv_waveform_type)  */
    int8_t   id_value;                      /* input waveform id    */
    int32_t  wf_size;                       /* input waveform size  */
    char     *wf_data;                      /* input waveform data  */
} vibdrv_waveform_data_t;

/* struct write waveform table */
typedef struct vibdrv_waveform_table {
    vibdrv_waveform_data_t waveform[HAPTICS_WAVE_TABLE_MAX];  /* input struct write waveform table */
    int8_t   table_cnt;                     /* input struct write waveform count */
    int8_t   return_val;                    /* output return        */
} vibdrv_waveform_table_t;

/* struct check waveform */
typedef struct vibdrv_check_waveform {
    int8_t   type;                          /* input wave type(vibdrv_check_wf_type) */
    int8_t   table_id;                      /* input table id       */
    int8_t   return_val;                    /* output return        */
} vibdrv_check_waveform_t;

/* struct set int pin */
typedef struct vibdrv_int_status {
    int8_t   int_status;                    /* input  intpin status(vibdrv_int_status) */
    int8_t   ret_status;                    /* output intpin status(vibdrv_int_status) */
} vibdrv_int_status_t;

/* IOCTL Group ID */
#define VIBDRV_IOCTL_GROUP               0x53

/* Command ID from framework */
#define VIBDRV_ENABLE_AMP                _IO(VIBDRV_IOCTL_GROUP, 1)
#define VIBDRV_DISABLE_AMP               _IO(VIBDRV_IOCTL_GROUP, 2)
#define VIBDRV_WRITE_FW                  _IOWR(VIBDRV_IOCTL_GROUP, 3, vibdrv_firm_data_t)
#define VIBDRV_CHK_FW_WRITE              _IOWR(VIBDRV_IOCTL_GROUP, 4, int8_t)
#define VIBDRV_GET_FWVER                 _IOR( VIBDRV_IOCTL_GROUP, 5, vibdrv_firm_version_t)
#define VIBDRV_WRITE_WF                  _IOWR(VIBDRV_IOCTL_GROUP, 6, vibdrv_waveform_table_t)
#define VIBDRV_CEHCK_WF                  _IOWR(VIBDRV_IOCTL_GROUP, 7, vibdrv_check_waveform_t)
#define VIBDRV_SET_INT                   _IOWR(VIBDRV_IOCTL_GROUP, 8, vibdrv_int_status_t)

#if VIBDRV_DEBUG
/* struct write firmware */
typedef struct vibdrv_read_firm_data {
    int8_t   blockno;                       /* input read block no  */
    int32_t  firm_size;                     /* input firmware size  */
    char     *firm_data;                    /* input firmware data  */
    int8_t   return_val;                    /* output return        */
} vibdrv_read_firm_data_t;
#define VIBDRV_READ_WF                   _IOWR(VIBDRV_IOCTL_GROUP,20, vibdrv_read_firm_data_t)
#endif

/* Define the port */
#define VIBDRV_GPIO_HAPTICS_PRESS        (6)   //GPIO6  HAPTICS1_ON(PRESS)
#define VIBDRV_GPIO_HAPTICS_RELEASE      (12)  //GPIO12 HAPTICS2_ON(RELEASE)
#define VIBDRV_GPIO_HAPTICS_VIB          (15)  //GPIO15 VIB
#define VIBDRV_GPIO_HAPTICS_INT          (17)  //GPIO17 INT
#define VIBDRV_GPIO_HAPTICS_RST_0        (0)
#define VIBDRV_GPIO_HAPTICS_RST_1        (1)

/* Device Type */
#define VIBDRV_DEVICE_TYPE_FJDEV005      0x10
/* Device status */
#define VIBDRV_DEVICE_STATUS_2_1_2       0x05

#define VIBDRV_DELAY_US_TIME             2
#define VIBDRV_DELAY_US_TIME_10          10
#define VIBDRV_DELAY_MS_TIME_50          50
#define VIBDRV_DELAY_MS_TIME_150         150

/* debug log */
#define VIBDRV_LOGE(fmt, args...)        printk(KERN_ERR "[vibdrv]: " fmt, ##args)
#if VIBDRV_DEBUG
#define VIBDRV_LOGD(fmt, args...)        printk(KERN_DEBUG "[vibdrv]: " fmt, ##args)
#define VIBDRV_LOGI(fmt, args...)        printk(KERN_INFO "[vibdrv]: " fmt, ##args)
#else
#define VIBDRV_LOGD(fmt, args...)        do {} while (0)
#define VIBDRV_LOGI(fmt, args...)        do {} while (0)
#endif

#endif  /* _VIBDRV_H */
