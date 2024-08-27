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


#ifndef _LINUX_OVP_DRIVER_H
#define _LINUX_OVP_DRIVER_H

#include "ovp_local.h"
#ifndef OVP_LOCAL_BUILD
#define INITIALIZED_MASK                (INITIALIZE_MHL | INITIALIZE_OTG | INITIALIZE_CHG | INITIALIZE_SUPPLY)
#else
#define INITIALIZED_MASK                (INITIALIZE_OTG | INITIALIZE_CHG | INITIALIZE_SUPPLY)
#endif
/* ------------------------------------------------------------------------
 * Constants
 */
#define OVP_DRIVER_NAME                    "ovp_driver"
#define READ_SIZE                          2

// i2c
#define OVP_SLAVE_ADDRESS                  (0x6A)   /* f11eif:BH1772GLC */

// APQ_GPIO
#define OVP_APQ_GPIO_53                    53
#define OVP_APQ_GPIO_54                    54
#define OVP_APQ_GPIO_59                    59
#define OVP_APQ_GPIO_82                    82
#define OVP_APQ_GPIO_83                    83

// PM_GPIO
#define OVP_PM_GPIO_7                      7
#define OVP_PM_GPIO_12                     12
#define OVP_PM_GPIO_22                     22
#define OVP_PM_GPIO_23                     23
#define OVP_PM_GPIO_27                     27
#define OVP_PM_GPIO_28                     28
#define OVP_PM_GPIO_29                     29
#define OVP_PM_GPIO_31                     31
#define OVP_PM_GPIO_42                     42

/* PM8921 GPIO configration */
#define GPIO_OVP_VC_DET                    OVP_PM_GPIO_7     /* VC detect   */
#define GPIO_OVP_INTB_DET                  OVP_PM_GPIO_22    /* INTB detect */
#define GPIO_OVP_CHG_DET                   OVP_PM_GPIO_23    /* CHG detec t */
#define GPIO_OVP_RESET                     OVP_PM_GPIO_27    /* reset       */
#define GPIO_OVP_VB_DET                    OVP_PM_GPIO_28    /* VB detect   */


// Register Address
#define OVP_DEVICE_ID                      (0x00)    // 00h
#define OVP_DEVICE_ID_RDM                  (0xff)
#define OVP_IDDET                          (0x01)    // 01h
#define OVP_IDDET_RDM                      (0x13)
#define OVP_UCDCNT                         (0x02)    // 02h
#define OVP_UCDCNT_RDM                     (0xff)
#define OVP_SW_CONTROL                     (0x03)    // 03h
#define OVP_SW_CONTROL_RDM                 (0xff)
#define OVP_OVP_CONTROL                    (0x04)    // 04h
#define OVP_OVP_CONTROL_RDM                (0x0f)
#define OVP_INT_STA                        (0x05)    // 05h
#define OVP_INT_STA_RDM                    (0xfc)
#define OVP_STATUS                         (0x06)    // 06h
#define OVP_STATUS_RDM                     (0xff)
#define OVP_ID_STA                         (0x07)    // 07h
#define OVP_ID_STA_RDM                     (0x7f)
#define OVP_S_RST                          (0x08)    // 08h
#define OVP_S_RST_RDM                      (0x01)

/* FUJITSU:2012-05-09 OVP start */
#define CONTINUE_COUNT                     40
/* FUJITSU:2012-05-09 OVP end */

// device check
#define DEVICE_USB_CLIENT                  0x0D
/* FUJITSU:2012-11-16 ACA-SDP charging start */
#define DEVICE_USB_ACA_RIDA                0x05
#define DEVICE_USB_ACA_RIDB                0x03
#define DEVICE_USB_ACA_RIDC                0x01
/* FUJITSU:2012-11-16 ACA-SDP charging end */
#define DEVICE_USB_OTG                     0x00
#define DEVICE_CABLE_NO_VBUS               0x06
#define DEVICE_CABLE_VBUS                  0x06
#define DEVICE_STEREOEARPHONE              0x08
#define DEVICE_STEREOEARPHONE2             0x09
#define DEVICE_EARPHONESWITCH              0x02
#define DEVICE_USB_MHL                     0x0E
#define DEVICE_UNKNOWN                     -1

//01h IDDET Register
#define IDDET_ADCPOLEN_POSI                0
#define IDDET_ADCPOLEN_WIDTH               1
#define IDDET_ADCPOLEN_ENA                 1
#define IDDET_ADCPOLEN_DIS                 0

#define IDDET_ADCRETRY_POSI                1
#define IDDET_ADCRETRY_WIDTH               1
#define IDDET_ADCRETRY_RETRY               1
#define IDDET_ADCRETRY_DEFAULT             0

#define IDDET_IDRDETEN_POSI                4
#define IDDET_IDRDETEN_WIDTH               1
#define IDDET_IDRDETEN_ENA                 1
#define IDDET_IDRDETEN_DIS                 0

//02h UCDCNT Register
#define UCDCNT_DCDRETRY_POSI               0
#define UCDCNT_DCDRETRY_WIDTH              1
#define UCDCNT_DCDRETRY_RETRY              1
#define UCDCNT_DCDRETRY_DEFAULT            0

#define UCDCNT_ENUMRDY_POSI                1
#define UCDCNT_ENUMRDY_WIDTH               1
#define UCDCNT_ENUMRDY_RDY                 1
#define UCDCNT_ENUMRDY_DEFAULT             0

#define UCDCNT_USBDETCTL_POSI              2
#define UCDCNT_USBDETCTL_WIDTH             1
#define UCDCNT_USBDETCTL_ENA               0
#define UCDCNT_USBDETCTL_DIS               1

#define UCDCNT_RESERVED_POSI               3
#define UCDCNT_RESERVED_WIDTH              1
#define UCDCNT_RESERVED_DEFAULT            0

#define UCDCNT_PDREN_POSI                  4
#define UCDCNT_PDREN_WIDTH                 1
#define UCDCNT_PDREN_ENA                   1
#define UCDCNT_PDREN_DIS                   0

#define UCDCNT_PDLEN_POSI                  5
#define UCDCNT_PDLEN_WIDTH                 1
#define UCDCNT_PDLEN_ENA                   1
#define UCDCNT_PDLEN_DIS                   0

#define UCDCNT_INTBEN_POSI                 6
#define UCDCNT_INTBEN_WIDTH                1
#define UCDCNT_INTBEN_ENA                  1
#define UCDCNT_INTBEN_DIS                  0

#define UCDCNT_INTBPOL_POSI                7
#define UCDCNT_INTBPOL_WIDTH               1
#define UCDCNT_INTBPOL_Hi_Z                1
#define UCDCNT_INTBPOL_L                   0

//03h SW Control Register
#define SW_CONTROL_MUXSW_HDML_POSI         0
#define SW_CONTROL_MUXSW_HDML_WIDTH        3
#define SW_CONTROL_MUXSW_HDML_DSS_L        0
#define SW_CONTROL_MUXSW_HDML_DSS_H        1
#define SW_CONTROL_MUXSW_HDML_EARL         2
#define SW_CONTROL_MUXSW_HDML_Hi_Z         7//(3 or 4 or 5 or 6 or 7)Hi-Z

#define SW_CONTROL_MUXSW_HDPR_POSI         3
#define SW_CONTROL_MUXSW_HDPR_WIDTH        3
#define SW_CONTROL_MUXSW_HDPR_DSS_L        0
#define SW_CONTROL_MUXSW_HDPR_DSS_H        1
#define SW_CONTROL_MUXSW_HDPR_EARR         2
#define SW_CONTROL_MUXSW_HDPR_MICOUT       3
#define SW_CONTROL_MUXSW_HDPR_Hi_Z         7//(4 or 5 or 6 or 7)Hi-Z

#define SW_CONTROL_CBUSSWEN_POSI           6
#define SW_CONTROL_CBUSSWEN_WIDTH          1
#define SW_CONTROL_CBUSSWEN_ENA            1
#define SW_CONTROL_CBUSSWEN_DIS            0

#define SW_CONTROL_MICSWEN_POSI            7
#define SW_CONTROL_MICSWEN_WIDTH           1
#define SW_CONTROL_MICSWEN_ENA             1
#define SW_CONTROL_MICSWEN_DIS             0

//04h OVP Control Register
#define OVP_CONTROL_VBSW_DISEN_POSI        0
#define OVP_CONTROL_VBSW_DISEN_WIDTH       1
#define OVP_CONTROL_VBSW_DISEN_ENA         0
#define OVP_CONTROL_VBSW_DISEN_DIS         1

#define OVP_CONTROL_VCSW_DISEN_POSI        1
#define OVP_CONTROL_VCSW_DISEN_WIDTH       1
#define OVP_CONTROL_VCSW_DISEN_ENA         0
#define OVP_CONTROL_VCSW_DISEN_DIS         1

#define OVP_CONTROL_VBREG_DISEN_POSI       2
#define OVP_CONTROL_VBREG_DISEN_WIDTH      1
#define OVP_CONTROL_VBREG_DISEN_ENA        0
#define OVP_CONTROL_VBREG_DISEN_DIS        1

#define OVP_CONTROL_OTGSWEN_POSI           3
#define OVP_CONTROL_OTGSWEN_WIDTH          1
#define OVP_CONTROL_OTGSWEN_ENA            1
#define OVP_CONTROL_OTGSWEN_DIS            0

//05h INT_STA Register (read)
#define INT_STA_OTGDET_POSI                2
#define INT_STA_OTGDET_WIDTH               1
#define INT_STA_OTGDET_DET                 1
#define INT_STA_OTGDET_UNDET               0

#define INT_STA_VCDLDET_POSI               3
#define INT_STA_VCDLDET_WIDTH              1
#define INT_STA_VCDLDET_DET                1
#define INT_STA_VCDLDET_UNDET              0

#define INT_STA_VBUSDET_POSI               4
#define INT_STA_VBUSDET_WIDTH              1
#define INT_STA_VBUSDET_DET                1
#define INT_STA_VBUSDET_UNDET              0

#define INT_STA_COMPL_POSI                 5
#define INT_STA_COMPL_WIDTH                1
#define INT_STA_COMPL_SNDRCV_SW_PUSH       1
#define INT_STA_COMPL_SNDRCV_SW_RELEASE    0

#define INT_STA_COMPH_POSI                 6
#define INT_STA_COMPH_WIDTH                1
#define INT_STA_COMPH_REMOVE_DET           1
#define INT_STA_COMPH_REMOVE_UNDET         0

#define INT_STA_CHGDET_POSI                7
#define INT_STA_CHGDET_WIDTH               1
#define INT_STA_CHGDET_BIGCHG_DET          1
#define INT_STA_CHGDET_BIGCHG_UNDET        0

//06h STATUS Register (read)
#define STATUS_OVP_VC_POSI                 0
#define STATUS_OVP_VC_WIDTH                1
#define STATUS_OVP_VC_DET                  1
#define STATUS_OVP_VC_UNDET                0

#define STATUS_OVP_VB_POSI                 1
#define STATUS_OVP_VB_WIDTH                1
#define STATUS_OVP_VB_DET                  1
#define STATUS_OVP_VB_UNDET                0

#define STATUS_OCP_VC_POSI                 2
#define STATUS_OCP_VC_WIDTH                1
#define STATUS_OCP_VC_DET                  1
#define STATUS_OCP_VC_UNDET                0

#define STATUS_OCP_VB_POSI                 3
#define STATUS_OCP_VB_WIDTH                1
#define STATUS_OCP_VB_DET                  1
#define STATUS_OCP_VB_UNDET                0

#define STATUS_PUPDET_POSI                 4
#define STATUS_PUPDET_WIDTH                1
#define STATUS_PUPDET_DET                  1
#define STATUS_PUPDET_UNDET                0

#define STATUS_CHGPORT_POSI                5
#define STATUS_CHGPORT_WIDTH               2
#define STATUS_CHGPORT_UNDET               0
#define STATUS_CHGPORT_STNDRD_DWNSTRM      1
#define STATUS_CHGPORT_CHRGNG_DWNSTRM      2
#define STATUS_CHGPORT_DDCTD_CHRGNG        3

#define STATUS_DCDFAIL_POSI                7
#define STATUS_DCDFAIL_WIDTH               1
#define STATUS_DCDFAIL_DCDDET              0
#define STATUS_DCDFAIL_DCDFAIL             1

//07h ID_STA Register (read)
#define ID_STA_INDO_POSI                   0
#define ID_STA_INDO_WIDTH                  4
#define ID_STA_INDO_USB_CLIENT             0x0D
#define ID_STA_INDO_USB_ACA_RIDA           0x05
#define ID_STA_INDO_USB_ACA_RIDB           0x03
#define ID_STA_INDO_USB_ACA_RIDC           0x01
#define ID_STA_INDO_USB_OTG                0x00
#define ID_STA_INDO_CABLE_NO_VBUS          0x06
#define ID_STA_INDO_CABLE_VBUS             0x06
#define ID_STA_INDO_STEREOEARPHONE         0x08
#define ID_STA_INDO_EARPHONESWITCH         0x02
#define ID_STA_INDO_USB_MHL                0x0E

#define ID_STA_IDRDET_POSI                 4
#define ID_STA_IDRDET_WIDTH                1
#define ID_STA_IDRDET_DET                  1
#define ID_STA_IDRDET_UNDET                0

#define ID_STA_EXTID_POSI                  5
#define ID_STA_EXTID_WIDTH                 1
#define ID_STA_EXTID_NEWASSIGN             1
#define ID_STA_EXTID_OLDASSIGN             0

#define ID_STA_VBINOP_POSI                 6
#define ID_STA_VBINOP_WIDTH                1
#define ID_STA_VBINOP_DET                  1
#define ID_STA_VBINOP_UNDET                0

//08h S_RST Register
#define S_RST_SOFT_RESET_POSI              0
#define S_RST_SOFT_RESET_WIDTH             1
#define S_RST_SOFT_RESET_RESET             1
#define S_RST_SOFT_RESET_NOTRESET          0


#define RETRY_COUNT                        2
#define NO_RETRY                           0

// I2C
#define I2C_TRANS_WRITE                    1
#define I2C_TARNS_READ                     2
#define I2C_RTRY_WAIT                      20
#define I2C_RTRY_CNT                       10
#define I2C_WRITE_BUFSIZE                  64
#define I2C_READ_SIZE                      2

#define WAKE_ON                            1

// timer
#define DEVICE_CHECK_COUNT                 60 /* FUJITSU:2012-12-17 vbus chattering add  */
#define WAIT_TIME                          25000
#define WAIT_TIME_FOR_07H                  5
#define OVP_INITIALIZE_WAIT                60000  /* FUJITSU:2012-09-25 add */

/* FUJITSU:2012-11-01 MHL_detect_retry add start */
#define CLIENT_CHECK_COUNT                 750 /* FUJITSU:2012-12-17 vbus chattering add  */
/* FUJITSU:2012-11-01 MHL_detect_retry add end */

/* FUJITSU:2012-12-17 vbus chattering add start */
#define DEVICE_CHATTERING_COUNT            10
/* FUJITSU:2012-12-17 vbus chattering add start */
/* FUJITSU:2013-01-08 OVP MHL an examination add_start */
#define MHL_CHATTERING_COUNT                7
/* FUJITSU:2013-01-08 OVP MHL an examination add_end */

#define OVP_FJDEV_MASK                      0xf0
#define OVP_FJDEV001                        0x50
#define OVP_FJDEV002                        0x40
#define OVP_FJDEV004                        0x60
#define OVP_FJDEV005                        0x10

#endif /* _LINUX_OVP_DRIVER_H */
