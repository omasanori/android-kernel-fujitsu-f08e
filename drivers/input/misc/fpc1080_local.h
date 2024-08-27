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

#ifndef LINUX_SPI_FPC1080_LOCAL_H
#define LINUX_SPI_FPC1080_LOCAL_H

/* -------------------------------------------------------------------- */
/* fpc1080 sensor commands and registers				*/
/* -------------------------------------------------------------------- */
#define FPC1080_ACTIVATE_SLEEP			0x28
#define FPC1080_ACTIVATE_DEEP_SLEEP		0x2C
#define FPC1080_ACTIVATE_NAVIGATION		0x30
#define FPC1080_ACTIVATE_IDLE			0x34
#define FPC1080_RD_INTERRUPT_WITH_CLEAR		0x64
#define FPC1080_RD_INTERRUPT_WITH_NO_CLEAR	0x18
#define FPC1080_WAIT_FOR_FINGER_PRESENT		0x24
#define FPC1080_READ_AND_CAPTURE_IMAGE		0xCC
#define FPC1080_READ_IMAGE_DATA			0xC4
#define FPC1080_CAPTURE_IMAGE			0xC0
#define FPC1080_SET_SMART_REF			0x48

#define FPC1080_ADC_OFFSET			0xA0
#define FPC1080_ADC_GAIN			0xA4
#define FPC1080_PXL_SETUP			0xA8
#define FPC1080_FNGR_DRIVE_CONF			0x1C
#define FPC1080_SMRT_DATA			0x10
#define FPC1080_STATUS				0x14
#define FPC1080_REG_HW_ID			0x9C

#define FPC1080_CMD(c) \
	((c) == FPC1080_REG_HW_ID			? "REG_HW_ID" : \
	((c) == FPC1080_ACTIVATE_DEEP_SLEEP		? "ACTIVATE_DEEP_SLEEP" : \
	((c) == FPC1080_RD_INTERRUPT_WITH_CLEAR		? "RD_INTERRUPT_WITH_CLEAR" : \
	((c) == FPC1080_WAIT_FOR_FINGER_PRESENT		? "WAIT_FOR_FINGER_PRESENT" : \
	((c) == FPC1080_FNGR_DRIVE_CONF			? "FNGR_DRIVE_CONF" : \
	((c) == FPC1080_CAPTURE_IMAGE			? "CAPTURE_IMAGE" : \
	((c) == FPC1080_READ_AND_CAPTURE_IMAGE		? "READ_AND_CAPTURE_IMAGE" : \
	((c) == FPC1080_READ_IMAGE_DATA			? "READ_IMAGE_DATA" : \
	((c) == FPC1080_SET_SMART_REF			? "SET_SMART_SENSOR_REFERENCE" : \
	((c) == FPC1080_ADC_OFFSET			? "ADC_OFFSET" : \
	((c) == FPC1080_ADC_GAIN			? "ADC_GAIN" : \
	((c) == FPC1080_PXL_SETUP			? "PXL_SETUP" : \
	((c) == FPC1080_SMRT_DATA			? "SMRT_DATA" : "?????")))))))))))))

/* -------------------------------------------------------------------- */
/* fpc1080 driver constants						*/
/* -------------------------------------------------------------------- */
#define DEFAULT_ADC_GAIN		20
#define DEFAULT_ADC_OFFSET		21
#define DEFAULT_PIXEL_SETUP		5

#define FPC1080_SPI_CLOCK_SPEED		(fpc1080->spi_clock_hz)

#define GET_SPI_CLOCK(_x)		((_x) == 1 ? 10800000 : \
					((_x) == 2 ? 12000000 : \
					((_x) == 3 ? 15060000 : 12000000)))

#define GET_SPI_MODE(_x)		((_x) == 1 ? SPI_MODE_0 : \
					((_x) == 2 ? SPI_MODE_1 : \
					(((system_rev & 0x0f) <= 2) ? SPI_MODE_1 : SPI_MODE_0)))

/* -------------------------------------------------------------------- */
/* fpc1080 data types							*/
/* -------------------------------------------------------------------- */
struct fpc1080_count {
	unsigned int open;
	unsigned int read;
	unsigned int write;
	unsigned int poll;
	unsigned int ioctl;
	unsigned int interrupt;
};

struct fpc1080_thread_task {
	int mode;
	int should_stop;
	struct semaphore sem_idle;
	wait_queue_head_t wait_job;
	struct task_struct *thread;
};

struct fpc1080_diag {
	u8 selftest;
	u32 capture_time;
	u32 frames_captured;
	u32 frames_stored;
};

struct fpc1080_data {
	struct spi_device *spi;
	struct class *class;
	struct device *device;
	struct cdev cdev;
	dev_t devno;
	u32 reset_gpio;
	u32 irq_gpio;
	u32 irq;
	u32 data_offset;
	u32 avail_data;
	wait_queue_head_t waiting_data_avail;
	int interrupt_done;
	wait_queue_head_t waiting_interrupt_return;
	struct semaphore mutex;
	u8 *huge_buffer;
	u32 current_frame;
	int capture_done;
	struct fpc1080_thread_task thread_task;
	struct fpc1080_adc_setup adc_setup;
	struct fpc1080_diag diag;
	struct early_suspend		early_suspend; 
	// add FJ
	struct fpc1080_count count;
	u32 spi_clock_hz;
	u8 opened;
};


int  fpc1080_spi_wr_reg(struct fpc1080_data *, u8, u8, unsigned int);

int  fpc1080_spi_sync_fj(struct spi_device *, struct spi_message *);
void fpc1080_finger_present(struct fpc1080_data *);
void fpc1080_finger_up(struct fpc1080_data *);
int  fpc1080_open_fj(struct fpc1080_data *);
void fpc1080_release_fj(struct fpc1080_data *);
int  fpc1080_deep_sleep(struct fpc1080_data *);
void fpc1080_selftest_fj(struct fpc1080_data *, u8);
void fpc1080_cleanup_fj(struct fpc1080_data *);
void fpc1080_probe_fj(struct fpc1080_data *);
int  fpc1080_spi_assign(void);
void fpc1080_fj_dump(const char *, u8 *, unsigned int);

extern u8 fpc1080_log_flag;
extern struct device_attribute fpc1080_attr_dbg;


#define DEBUG_FPC1080_FULL
#define DEBUG_FPC1080_API
//#define DEBUG_FPC1080_CAPT
//#define DEBUG_FPC1080_API2
//#define DEBUG_FPC1080_FUNC
//#define DEBUG_FPC1080_DATA
//#define DEBUG_FPC1080_INFO
//#define DEBUG_FPC1080_DUMP
//#define DEBUG_FPC1080_PERFORM
//#define DEBUG_FPC1080_LED

#define FPC1080_KMSG	KERN_INFO

#ifdef DEBUG_FPC1080_FULL
#define DBG_PRINT_FULL(fmt,...) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#else
#define DBG_PRINT_FULL(fmt,...) if (unlikely(fpc1080_log_flag & 0x01)) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#endif

#ifdef DEBUG_FPC1080_API
#define DBG_PRINT_API(fmt,...) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#else
#define DBG_PRINT_API(fmt,...) if (unlikely(fpc1080_log_flag & 0x02)) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#endif

#ifdef DEBUG_FPC1080_CAPT
#define DBG_PRINT_CAPT(fmt,...) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#else
#define DBG_PRINT_CAPT(fmt,...) if (unlikely(fpc1080_log_flag & 0x04)) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#endif

#ifdef DEBUG_FPC1080_API2
#define DBG_PRINT_API2(fmt,...) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#else
#define DBG_PRINT_API2(fmt,...) if (unlikely(fpc1080_log_flag & 0x08)) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#endif

#ifdef DEBUG_FPC1080_FUNC
#define DBG_PRINT_FUNC(fmt,...) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#else
#define DBG_PRINT_FUNC(fmt,...) if (unlikely(fpc1080_log_flag & 0x10)) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#endif

#ifdef DEBUG_FPC1080_DATA
#define DBG_PRINT_DATA
#else
#define DBG_PRINT_DATA if (unlikely(fpc1080_log_flag & 0x20))
#endif

#ifdef DEBUG_FPC1080_INFO
#define DBG_PRINT_INFO(fmt,...) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#else
#define DBG_PRINT_INFO(fmt,...) if (unlikely(fpc1080_log_flag & 0x40)) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#endif

#ifdef DEBUG_FPC1080_DUMP
#define DBG_PRINT_DUMP(...) fpc1080_fj_dump(__VA_ARGS__)
#else
#define DBG_PRINT_DUMP(...) if (unlikely(fpc1080_log_flag & 0x80)) fpc1080_fj_dump(__VA_ARGS__)
#endif

#ifdef DEBUG_FPC1080_PERFORM
#define DBG_PRINT_PERFORM(fmt,...) printk(FPC1080_KMSG fmt,__VA_ARGS__)
#else
#define DBG_PRINT_PERFORM(fmt,...)
#endif

#ifdef DEBUG_FPC1080_LED
#define DBG_LED_ON(_color)		gpio_set_value(PM8921_GPIO_PM_TO_SYS(_color), 1)
#define DBG_LED_OFF(_color)		gpio_set_value(PM8921_GPIO_PM_TO_SYS(_color), 0)
#else
#define DBG_LED_ON(_color)		if (unlikely(fpc1080_debug_flag & 0x01)) gpio_set_value(PM8921_GPIO_PM_TO_SYS(_color), 1)
#define DBG_LED_OFF(_color)		if (unlikely(fpc1080_debug_flag & 0x01)) gpio_set_value(PM8921_GPIO_PM_TO_SYS(_color), 0)
#endif

#define spi_sync fpc1080_spi_sync_fj

#endif
