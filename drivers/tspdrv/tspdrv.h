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
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/

#ifndef _TSPDRV_H
#define _TSPDRV_H

#include <linux/ioctl.h>

/* Constants */
#define TSPDRV_MODULE_NAME                  "tspdrv"
#define TSPDRV                              "/dev/"TSPDRV_MODULE_NAME
#define TSPDRV_MAGIC_NUMBER                 0x494D4D53


/* define the HIGH,LOW */
#define TSPDRV_GPIO_LEVEL_HIGH              (1)
#define TSPDRV_GPIO_LEVEL_LOW               (0)

#define TSPDRV_RET_NG                       -1

/* Power Mode */
enum tspdrv_power_mode {
    LOW_MODE            = 0x00,             // Low
    NORMAL_MODE         = 0x01              // Normal
};

/* Set the return value */
enum tspdrv_return_val {
    TSPDRV_RET_OK       = 0x00,             // Normal
    TSPDRV_ERR_OVER     = 0x01,             // freq value is over 20
    TSPDRV_ERR_NOANW    = 0xFE              // no answer from application
};

/* data struct for MC mode */
typedef struct haptics_io_val {
    char    wave_id;                        // input/output wave ID
    int     cycle_val;                      // input cycle
    int     freq_val;                       // input freq
    int     return_val;                     // output retrun ID
} haptics_io_val_t;

/* IOCTL Group ID */
#define TSPDRV_IOCTL_GROUP                  0x52

/* Command ID from framework */
#define TSPDRV_HAPTICS_PRESS                _IO(TSPDRV_IOCTL_GROUP, 1)
#define TSPDRV_HAPTICS_RELEASE              _IO(TSPDRV_IOCTL_GROUP, 2)
#define TSPDRV_THEME_STANDARD               _IO(TSPDRV_IOCTL_GROUP, 3)
#define TSPDRV_THEME_STRONG                 _IO(TSPDRV_IOCTL_GROUP, 4)

/* Maker command */
#define TSPDRV_MK_CMD_HAPTICS               _IOWR(TSPDRV_IOCTL_GROUP, 5, haptics_io_val_t)

/* AMP Command */
#define TSPDRV_AMP_ENABLE                   _IO(TSPDRV_IOCTL_GROUP, 6)
#define TSPDRV_AMP_DISABLE                  _IO(TSPDRV_IOCTL_GROUP, 7)

/* Power Mode */
#define TSPDRV_POWER_MODE                   _IOR(TSPDRV_IOCTL_GROUP, 8, int)

/* Define the port */
#define TSPDRV_GPIO_HAPTICS_PRESS           (6)     //GPIO6  HAPTICS1_ON(PRESS)
#define TSPDRV_GPIO_HAPTICS_RELEASE         (12)    //GPIO12 HAPTICS2_ON(RELEASE)
#define TSPDRV_GPIO_HAPTICS_RST             (0)     //GPIO0  RST

#define TSPDRV_DELAY_US_TIME                3       // at least 3 us

#endif  /* _TSPDRV_H */
