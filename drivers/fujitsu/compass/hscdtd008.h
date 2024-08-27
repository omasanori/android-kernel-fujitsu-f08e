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
#ifndef _HSCDTD008_H
#define _HSCDTD008_H
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/compass.h>
#include <linux/export.h>
#include <linux/module.h>


/*--------------------------------------------------------------------
Interrupt
--------------------------------------------------------------------*/
#define JIKI_DRDY_GPIO 69

#define CMPS_IRQ()  gpio_to_irq(JIKI_DRDY_GPIO)

/*--------------------------------------------------------------------
CMPS Interruption terminal (CMPS.DRDY -> JIKI_DRDY)
--------------------------------------------------------------------*/
// When the interruption demand is generated, the flag becomes one.
// It is flag automatically clearness output data is read.
// GPIO True where interruption has been generated.
#define GPIO_ISACTIVE()   (gpio_get_value(JIKI_DRDY_GPIO))

//----------------------------------------------------------------

/*--------------------------------------------------------------------
Register address
--------------------------------------------------------------------*/
#define HSCD_INFOLSB (0x0D) // R:  Information register(LSB)
#define HSCD_INFOMSB (0x0E) // R:  Information register(MSB)
#define HSCD_WIA     (0x0F) // R:  "Who I Am" register
#define HSCD_XLSB    (0x10) // R:  X Axis Output data register(LSB)
#define HSCD_XMSB    (0x11) // R:  X Axis Output data register(MSB)
#define HSCD_YLSB    (0x12) // R:  Y Axis Output data register(LSB)
#define HSCD_YMSB    (0x13) // R:  Y Axis Output data register(MSB)
#define HSCD_ZLSB    (0x14) // R:  Z Axis Output data register(LSB)
#define HSCD_ZMSB    (0x15) // R:  Z Axis Output data register(MSB)

#define HSCD_STAT    (0x18) // R:  Status register
#define HSCD_CNTL1   (0x1B) // RW: Control register1
#define HSCD_CNTL2   (0x1C) // RW: Control register2
#define HSCD_CNTL3   (0x1D) // RW: Control register3
#define HSCD_CNTL4   (0x1E) // RW: Control register4

#define HSCD_XOFFLSB (0x20) // RW: X Offset Drift value register(LSB)
#define HSCD_XOFFMSB (0x21) // RW: X Offset Drift value register(MSB)
#define HSCD_YOFFLSB (0x22) // RW: Y Offset Drift value register(LSB)
#define HSCD_YOFFMSB (0x23) // RW: Y Offset Drift value register(MSB)
#define HSCD_ZOFFLSB (0x24) // RW: Z Offset Drift value register(LSB)
#define HSCD_ZOFFMSB (0x25) // RW: Z Offset Drift value register(MSB)
#define HSCD_TEMP    (0x31) // RW: Temperature data register


// The maximum number of consecutive register numbers (XLSB-YMSB=6)
// (I2C It uses it for the size decision in the sending and receiving buffer.)
#define CMPS_MAX_CONTIGUOUS_REGS (HSCD_ZMSB - HSCD_XLSB + 1)

/*--------------------------------------------------------------------
Register Name
--------------------------------------------------------------------*/

//Address : 18h
//Status Register (STAT)
#define HSCD_DRDY	(0x40)	//Data Ready Detection
							//0 = Not Detected, 1 = Detected
#define HSCD_DOR	(0x20)	//Data Overrun Detection
							//0 = Not Detected, 1 = Detected

/* FUJITSU:2013-05-08 COMPASS add start */
#define HSCD_TRDY	(0x02)	//Must be use Default setting 0 = (Default)
							//temp measure completion
/* FUJITSU:2013-05-08 COMPASS add end */

//Address : 1Bh
//Control 1 Register (CTRL1)
#define HSCD_ODR	(0x08)	//Output Data Rate 10Hz (Default)
#define HSCD_FS		(0x02)	//State Control in Active Mode 1 = Force State (Default)
#define HSCD_PC		(0x80)	//Power Mode Control 1 = Active Mode

//Address : 1Ch
//Control 2 Register (CTRL2)
#define HSCD_DRP	(0x04)	//DRDY signal active level control
							//0 = ACTIVE LOW, 1 = ACTIVE HIGH (Default)
#define HSCD_DEN	(0x08)	//Data Ready Function Control Enable

//Address : 1Dh
//Control 3 Register (CTRL3)
#define HSCD_SRST	(0x80)	//Soft Reset Control Enable
							//0 = No Action (Default), 1 = Soft Reset
#define HSCD_FRC	(0x40)	//Start to Measure in Force State
							//0 = No Action (Default), 1 = Measurement Start
#define HSCD_STC	(0x10)	//Self Test Control Enable
							//0 = No Action (Default)
							//1 = Set parameters to Self Test Response (STB) register.
#define HSCD_TCS	(0x02)	//Start to Measure Temperature in Active Mode
							//0 = No Action (Default), 1 = Measurement Start
#define HSCD_OCL	(0x01)	//Start to Calibrate Offset in Active Mode
							//0 = No Action (Default), 1 = Action

//Address : 1Eh,
//Control 4 Register (CTRL4)

#define HSCD_MMD	(0x80)	//Must be use Default setting
#define HSCD_RS		(0x10)	//Set Dynamic range of output data.
							//0 = 14 bit signed value (-8192 to +8191) (Default)
							//1 = 15 bit signed value (-16384 to +16383)

#define HSCD_ALL_ZERO	(0x00)// ALL ZERO

/*--------------------------------------------------------------------
Register Content
--------------------------------------------------------------------*/
// CNTL1 Register Content
#define HSCD_MODE_STANBY			(HSCD_ODR | HSCD_FS)			// Standby Mode
#define HSCD_MODE_NORMAL			(HSCD_PC | HSCD_ODR)			// Active normality Mode
#define HSCD_MODE_FORCESTATE		(HSCD_PC | HSCD_ODR | HSCD_FS)	// Active forcestate Mode

// CNTL2 Register Content
#define HSCD_CNTL2_CLEAR			(HSCD_DRP)						// DEN OFF
#define HSCD_DATAREADY				(HSCD_DEN | HSCD_DRP)			//Contorol function for
																	// Data Ready terminal with CTRL2 register

// CNTL3 Register Content
#define HSCD_CNTL3_STANBY			(HSCD_ALL_ZERO)					// Standby Mode
#define HSCD_CNTL3_SRST				(HSCD_SRST)						// Soft reset
#define HSCD_CNTL3_FORCE			(HSCD_FRC)						// Active forcestate Mode Measurement
#define HSCD_CNTL3_TCS				(HSCD_TCS)						// Active temperature Measurement

// CNTL4 Register Content
#define HSCD_CNTL4_RS_DRANGE_14BIT	(HSCD_MMD)						// Range 14bit
#define HSCD_CNTL4_RS_DRANGE_15BIT	(HSCD_MMD | HSCD_RS)			// Range 15bit

// STAT1 Register
#define HSCD_STAT_TRDY				(HSCD_TRDY)						// temp measure completion
#define HSCD_STAT_DRDYON			(HSCD_DRDY)						// DRDY signal active level control

/*--------------------------------------------------------------------
CMPS Peculiar instance data
--------------------------------------------------------------------*/
typedef struct cmps {
	wait_queue_head_t measure_end_queue; // Present measurement completion queue
	compass_data_t measurement; // The last result of a measurement

	int use_count;              // Frequency that has been opened now
	unsigned max_retry_count;   // Maximum retrying frequency

	unsigned error_count;       // This time ioctl() error frequency
	unsigned retry_count;       // This time ioctl() retrying frequency
	u_long total_error_count;   // Total when system starts
	u_long total_retry_count;   // Total when system starts

	// State of operation
	volatile uint8_t state;		// Status
#define CMPS_STAT_IDLE		0	// State of idol
#define CMPS_STAT_MEASURING	1	// CMPS measuring
#define CMPS_STAT_MEASURED	2	// Measurement completion
#define CMPS_STAT_AVAILABLE	3	// The measuring data can be read
	//uint8_t verify;		// CMPS Collation when register is read and written

	// CMPS register
	cmps_registers_t	reg;
} cmps_t;

// Mode (MS1.MODE[1:0]) get.
#define CMPS_MODE()           (cmps->reg.cntl1 & CMPS_MODE_MASK)

// Mode (MS1.MODE[1:0]) set.
#define CMPS_SETMODE(mode)    (cmps->reg.cntl1 = (mode))

// State of operation
#define CMPS_STATE()              (cmps->state)
#define CMPS_SETSTATE(newState)   (CMPS_STATE() = (newState))

/*--------------------------------------------------------------------
CMPS Timing data
--------------------------------------------------------------------*/
// Reset waiting (us)
#define HSCD_Trnw 200
// Measurement time maximum value (us)
#define CMPS_MAX_MEASUREMENT_TIME 10000

#define CMPS_MAX_MEASUREMENT_COUNT 4

/*--------------------------------------------------------------------
--------------------------------------------------------------------*/

/*--------------------------------------------------------------------
CMPS Module STS data
--------------------------------------------------------------------*/
#define CMPS_NONE     0x00
#define CMPS_OPEN     0x01
#define CMPS_RELEASE  0x02
#define CMPS_MEASURE  0x03
#define CMPS_MSRTEMP  0x04
/*--------------------------------------------------------------------
--------------------------------------------------------------------*/

#ifdef __KERNEL__

#define cmps_update_error_count()						\
	do {												\
		cmps->total_error_count += cmps->error_count;	\
		cmps->total_retry_count += cmps->retry_count;	\
		cmps->error_count = cmps->retry_count = 0;		\
	} while (false)

extern cmps_t cmps[1];
extern spinlock_t cmps_spin_lock;
extern uint8_t cmps_sts;
extern compass_data_t pre_meas;

//// HW //////////////////////////////////////////////////////
extern int cmps_hw_read_regs(uint8_t start_reg, uint8_t reg_data[], uint16_t n_regs);
extern int cmps_hw_read_verify_regs(uint8_t start_reg, uint8_t reg_data[], uint8_t n_regs);
extern int cmps_hw_write_regs(uint8_t start_reg, const uint8_t reg_data[], uint16_t n_regs);
extern int cmps_hw_write_verify_regs(uint8_t start_reg, const uint8_t reg_data[], uint16_t n_regs, uint8_t verify_mask);
extern int cmps_hw_measure_start(void);
extern int cmps_hw_exit(void);
extern int cmps_hw_open(void);
extern int cmps_hw_release(void);
extern int cmps_hw_measure_start(void);
extern int cmps_hw_measure_end(void);
extern int cmps_hw_release(void);
extern int cmps_hw_init(bool first);
extern int cmps_hw_exit(void);

extern long cmps_hw_measure(compass_data_t __user * arg);
extern long cmps_hw_measure_temp(compass_data_t __user * arg);

/*--------------------------------------------------------------------
Log output
--------------------------------------------------------------------*/
#define compass_printk(logLevel, format, arg...) \
	if (0)	printk("%s" format, logLevel, ##arg)

#define debug(format, arg...)	\
	if (0)	compass_printk(KERN_DEBUG, format, ##arg)

#endif /* __KERNEL__ */
#endif /* _COMPASS_DEF_H */
