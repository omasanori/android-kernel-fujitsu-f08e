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
#ifndef _COMPASS_H
#define _COMPASS_H
#ifdef __KERNEL__
#define COMPASS_DEBUG
#else /* __KERNEL__ */
#include <stdint.h>
#include <sys/time.h>
#endif /* __KERNEL__ */
#include <linux/ioctl.h>

/* FUJITSU:2013-02-06 COMPASS add start */
#include "../../../fjfeat/FjFeatMac.h"
/* FUJITSU:2013-04-17 COMPASS mod start */
#if (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV004) \
 || (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV005)
#define FJFEAT_KERNEL_COMPASS_DRIVER  1
#endif
/* FUJITSU:2013-04-17 COMPASS mod end */
/* FUJITSU:2013-02-06 COMPASS add end */

/*--------------------------------------------------------------------
Device Number
--------------------------------------------------------------------*/
#define COMPASS_DEVNO_MAJOR 0 // Major Number (Automatic Numbering)
#define COMPASS_DEVNO_MINOR 0 // Minor Number

#define COMPASS_DEVNO MKDEV(COMPASS_DEVNO_MAJOR, COMPASS_DEVNO_MINOR)

#define COMPASS_NDEVICES    1 // Number of Devices

/*--------------------------------------------------------------------
magnetism measurement data
--------------------------------------------------------------------*/
typedef struct compass_data {
  struct timeval time;    // Measuring Time

  // Measurement data of cmps
  int8_t temperature;    // Temperature magnetic sensors (TEMP)
  int16_t magnetism[3];   // Magnetic vector (DATA[XYZ])

  // Number of errors after device opened.
  u_long total_error_count; // Number of errors
  u_long total_retry_count; // Number of retry

  int8_t dynamic_range;  // Measurement range

#define HSCD_RS_DRANGE_14    0  // 14bit
#define HSCD_RS_DRANGE_15    1  // 15bit

} compass_data_t;

/*--------------------------------------------------------------------
cmps Register
--------------------------------------------------------------------*/
// Parameters for 3-axis
typedef union cmps_xyz {
  int16_t xyz[3];
  struct { int16_t x, y, z; };
} cmps_xyz_t;

// Register all of hscd
//   Interrupt Source INS 
//   Interrupt latches INL 
//   Interrupt Control INC 
//   Interrupt Threshold Register ITHR
//   not use the interrupt line is not connected.
typedef struct cmps_registers {
  uint8_t mask;     // Select read / write registers.
#define CMPS_REGMASK_DATA     0x01 // Measurement data (TEMP,DATA[XYZ])
#define CMPS_REGMASK_OFFSET   0x02 // Sensor offset (OFF[XYZ])
#define CMPS_REGMASK_AMPGAIN  0x04 // Sensor amplifier gain (AMP)
#define CMPS_REGMASK_ADJUST   \
  (CMPS_REGMASK_OFFSET | CMPS_REGMASK_AMPGAIN)
#define CMPS_REGMASK_ALL      \
  (CMPS_REGMASK_DATA | CMPS_REGMASK_ADJUST)

  // Status and sensor measurements
  int8_t temp;     // Temperature sensor measurements (TEMP)
  cmps_xyz_t data;  //magnetism sensor measurements (DATAX, DATAY, DATAZ)

  // Mode and set value register
  uint8_t ins;      // Interrupt Source (INS Unused)
  uint8_t stat;     // Status
  uint8_t inl;      // Interrupt latches (INL Unused)
  uint8_t cntl1;    // Mode (CNTL1)
  uint8_t cntl2;    // Mode (CNTL2)
  uint8_t cntl3;    // Mode (CNTL3)
  uint8_t cntl4;    // Mode (CNTL4)
  uint8_t inc;      // Interrupt Control (INC Unused)
  uint16_t ithr;    // Interrupt Threshold (ITHR Unused)
  cmps_xyz_t off;   // magnetism sensor offset (OFFX, OFFY, OFFZ)
  struct i2c_client* st_client;  //I2C Client information

} cmps_registers_t;

/*--------------------------------------------------------------------
IOCTL
--------------------------------------------------------------------*/
#define COMPASS_IOC_MAGIC   0xA2

#define COMPASS_IOC_GETDATA     _IOWR(COMPASS_IOC_MAGIC, 0x01, compass_data_t)
#define COMPASS_IOC_TEMP_COR    _IOWR(COMPASS_IOC_MAGIC, 0x02, compass_data_t)

/*====================== Definition for driver =====================*/
#ifdef __KERNEL__
/*--------------------------------------------------------------------
General-purpose macro definition
--------------------------------------------------------------------*/
#define OK      0

/*--------------------------------------------------------------------
Function  :Structure member's address is returned.
@param    : (1) type:Structure member's type.
            (2) base:Base address of structure.
            (3) offset:Byte offset of the structure member.
@retval   :Member's address.
--------------------------------------------------------------------*/
#define STRUCT_MEMBER(type, base, offset) \
  ((type*)((char*)(base) + (offset)))

#endif /* __KERNEL__ */
#endif /* _LINUX_COMPASS_H */
