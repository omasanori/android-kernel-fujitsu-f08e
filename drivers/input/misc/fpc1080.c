/* FPC1080 Swipe Sensor Driver
 *
 * Copyright (c) 2011 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */
/*------------------------------------------------------------------*/
// COPYRIGHT(c) FUJITSU LIMITED 2013
/*------------------------------------------------------------------*/

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/earlysuspend.h>

#include <linux/spi/fpc1080.h>
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
#include <asm/system.h>
#include <linux/fta.h>
#include "fpc1080_local.h"
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fingerprint Cards AB <tech@fingerprints.com>");
MODULE_DESCRIPTION("FPC1080 swipe sensor driver.");

/* -------------------------------------------------------------------- */
/* fpc1080 sensor commands and registers				*/
/* -------------------------------------------------------------------- */
/* FUJITSU:2013-04-18 FingerPrintSensor_FPC1080 del-S */
#if 0
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
#endif
/* FUJITSU:2013-04-18 FingerPrintSensor_FPC1080 del-S */

#define FPC1080_STATUS_IRQ			(1 << 0u)
#define FPC1080_STATUS_FSM_IDLE			(1 << 1u)

#define FPC1080_SMRT_MOTION_EST_BIT_8		(1 << 0u)
#define FPC1080_SMRT_MOTION_EST_BIT_9		(1 << 1u)
#define FPC1080_SMRT_SHORT_CLICK		(1 << 2u)
#define FPC1080_SMRT_LONG_CLICK			(1 << 3u)
#define FPC1080_SMRT_X_SIGN			(1 << 4u)
#define FPC1080_SMRT_Y_SIGN			(1 << 5u)
#define FPC1080_SMRT_X_BYTE			4
#define FPC1080_SMRT_Y_BYTE			3
#define FPC1080_SMRT_MO_CNTR_BYTE		2
#define FPC1080_SMRT_MO_EST_BYTE		1
#define FPC1080_SMRT_BITS			0

#define FPC1080_SPI_FNGR_DRV_TST		(1 << 2u)
#define FPC1080_SPI_FNGR_DRV_EXT		(1 << 1u)
#define FPC1080_SPI_SMRT_SENS_EN		(1 << 0u)

#define	FPC1080_PATTERN1_XREG			0x78
#define FPC1080_PATTERN2_XREG			0x7C

#define FPC1080_IRQ_REBOOT			0xFF
#define FPC1080_IRQ_CMD_DONE			(1 << 7u)
#define FPC1080_IRQ_DY				(1 << 6u)
#define FPC1080_IRQ_DX				(1 << 5u)
#define FPC1080_IRQ_FING_LOST			(1 << 4u)
#define FPC1080_IRQ_SHORT_CLICK			(1 << 3u)
#define FPC1080_IRQ_LONG_CLICK			(1 << 2u)
#define FPC1080_IRQ_FING_UP			(1 << 1u)
#define FPC1080_IRQ_FING_DOWN			(1 << 0u)


/* -------------------------------------------------------------------- */
/* fpc1080 driver constants						*/
/* -------------------------------------------------------------------- */
#define FPC1080_HARDWARE_ID			0x1A

#define FPC1080_SYNCED_REG_SIZE			2
#define FPC1080_MOTION_THRESHOLD		25
#define FPC1080_MOTON_FRAMES_THERSHOLD		50

#define FPC1080_MAJOR				235

#define FPC1080_DEFAULT_IRQ_TIMEOUT		(100 * HZ / 1000)
#define FPC1080_FRAME_SIZE			(128 * 8)
#define FPC1080_MAX_FRAMES			256
#define FPC1080_IMAGE_BUFFER_SIZE		(FPC1080_MAX_FRAMES * \
						 FPC1080_FRAME_SIZE)
/* FUJITSU:2013-04-26 FingerPrintSensor_FPC1080 add-S */
#define FPC1080_SPI_BLOCK_SIZE			32
/* FUJITSU:2013-04-26 FingerPrintSensor_FPC1080 add-E */

/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 del-S */
// #define FPC1080_SPI_CLOCK_SPEED			(12 * 1000 * 1000)
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 del-S */

#define FPC1080_DEV_NAME                        "fpc1080"
#define FPC1080_CLASS_NAME                      "fpsensor"
#define FPC1080_WORKER_THREAD_NAME		"fpc1080worker"

#define FPC1080_IOCTL_MAGIC_NO			0xFC

#define FPC1080_IOCTL_START_CAPTURE	_IO(FPC1080_IOCTL_MAGIC_NO, 0)
#define FPC1080_IOCTL_ABORT_CAPTURE	_IO(FPC1080_IOCTL_MAGIC_NO, 1)
#define FPC1080_IOCTL_CAPTURE_SINGLE	_IOW(FPC1080_IOCTL_MAGIC_NO, 2, int)

#define FPC1080_KEY_FINGER_PRESENT		188

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
#define FPC1080_IOCTL_CMD(c) \
	((c) == FPC1080_IOCTL_START_CAPTURE		? "IOCTL_START_CAPTURE" : \
	((c) == FPC1080_IOCTL_ABORT_CAPTURE		? "IOCTL_ABORT_CAPTURE" : \
	((c) == FPC1080_IOCTL_CAPTURE_SINGLE		? "IOCTL_CAPTURE_SINGLE" : "undefine_ioctl")))
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */

enum {
	FPC1080_THREAD_IDLE_MODE = 0,
	FPC1080_THREAD_CAPTURE_MODE,
	FPC1080_THREAD_NAV_MODE,
	FPC1080_THREAD_EXIT
};

/* -------------------------------------------------------------------- */
/* global variables							*/
/* -------------------------------------------------------------------- */
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 del-S */
//static int fpc1080_device_count;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 del-E */
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
#ifdef DEBUG_FPC1080_PERFORM
static ktime_t kt_open;
#endif
static u8 fpc1080_state = 0;
enum {
	FPC1080_STATE_INIT = 1,
	FPC1080_STATE_DIAG,
	FPC1080_STATE_OPEN,
	FPC1080_STATE_CLOSE,
};
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */

/* -------------------------------------------------------------------- */
/* fpc1080 data types							*/
/* -------------------------------------------------------------------- */
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 del-S */
#if 0
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
};
#endif
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 del-E */

struct fpc1080_attribute {
	struct device_attribute attr;
	size_t offset;
};

/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
static int __init fpc1080_init(void);

static void __exit fpc1080_exit(void);

static int __devinit fpc1080_probe(struct spi_device *spi);

static int __devexit fpc1080_remove(struct spi_device *spi);

static int fpc1080_suspend(struct device *dev);

static int fpc1080_resume(struct device *dev);

static int fpc1080_open(struct inode *inode, struct file *file);

static ssize_t fpc1080_write(struct file *file, const char *buff,
					size_t count, loff_t *ppos);

static ssize_t fpc1080_read(struct file *file, char *buff,
				size_t count, loff_t *ppos);

static int fpc1080_release(struct inode *inode, struct file *file);

static unsigned int fpc1080_poll(struct file *file, poll_table *wait);

static long fpc1080_ioctl(struct file *filp,
			  unsigned int cmd,
			  unsigned long arg);

static int fpc1080_reset(struct fpc1080_data *fpc1080);

static ssize_t fpc1080_show_attr_adc_setup(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t fpc1080_store_attr_adc_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static ssize_t fpc1080_show_attr_diag(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t fpc1080_store_attr_diag(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static int fpc1080_wreg(struct fpc1080_data *fpc1080, u8 addr, u8 value);
static int fpc1080_write_adc_setup(struct fpc1080_data *fpc1080);
static int fpc1080_cmd_wait_irq(struct fpc1080_data *fpc1080, u8 cmd, u8 *irq);
static int fpc1080_spi_rd_image(struct fpc1080_data *fpc1080, bool capture);
static int fpc1080_selftest_short(struct fpc1080_data *fpc1080);

static void fpc1080_early_suspend(struct early_suspend *h);
static void fpc1080_late_resume(struct early_suspend *h);


/* -------------------------------------------------------------------- */
/* External interface							*/
/* -------------------------------------------------------------------- */
module_init(fpc1080_init);
module_exit(fpc1080_exit);

static const struct dev_pm_ops fpc1080_pm = {
	.suspend = fpc1080_suspend,
	.resume = fpc1080_resume
};

static struct spi_driver fpc1080_driver = {
	.driver = {
		.name	= FPC1080_DEV_NAME,
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
		.pm = &fpc1080_pm,
	},
	.probe	= fpc1080_probe,
	.remove	= __devexit_p(fpc1080_remove)
};

static const struct file_operations fpc1080_fops = {
	.owner = THIS_MODULE,
	.open  = fpc1080_open,
	.write = fpc1080_write,
	.read  = fpc1080_read,
	.release = fpc1080_release,
	.poll = fpc1080_poll,
	.unlocked_ioctl = fpc1080_ioctl
};

/* -------------------------------------------------------------------- */
/* devfs								*/
/* -------------------------------------------------------------------- */
#define FPC1080_ATTR(__grp, __field, __mode)				\
{									\
	.attr = __ATTR(__field, (__mode),				\
	fpc1080_show_attr_##__grp,					\
	fpc1080_store_attr_##__grp),					\
	.offset = offsetof(struct fpc1080_##__grp, __field)		\
}

#define FPC1080_DEV_ATTR(_grp, _field, _mode)				\
struct fpc1080_attribute fpc1080_attr_##_field =			\
					FPC1080_ATTR(_grp, _field, (_mode))

/* FUJITSU:2013-05-08 FingerPrintSensor_FPC1080 mod-S */
/* #define ADC_SETUP_MODE (S_IWUSR | S_IWGRP | S_IWOTH) */
#define ADC_SETUP_MODE (S_IWUSR)
/* FUJITSU:2013-05-88 FingerPrintSensor_FPC1080 mod-S */

static FPC1080_DEV_ATTR(adc_setup, gain,	ADC_SETUP_MODE);
static FPC1080_DEV_ATTR(adc_setup, offset,	ADC_SETUP_MODE);
static FPC1080_DEV_ATTR(adc_setup, pxl_setup,	ADC_SETUP_MODE);

static struct attribute *fpc1080_adc_attrs[] = {
	&fpc1080_attr_gain.attr.attr,
	&fpc1080_attr_offset.attr.attr,
	&fpc1080_attr_pxl_setup.attr.attr,
	NULL
};

static const struct attribute_group fpc1080_adc_attr_group = {
	.attrs = fpc1080_adc_attrs,
	.name = "adc_setup"
};

#define DIAG_MODE (S_IRUSR | S_IRGRP | S_IROTH)

static FPC1080_DEV_ATTR(diag, selftest,		DIAG_MODE);
static FPC1080_DEV_ATTR(diag, capture_time,	DIAG_MODE);
static FPC1080_DEV_ATTR(diag, frames_captured,	DIAG_MODE);
static FPC1080_DEV_ATTR(diag, frames_stored,	DIAG_MODE);

static struct attribute *fpc1080_diag_attrs[] = {
	&fpc1080_attr_selftest.attr.attr,
	&fpc1080_attr_capture_time.attr.attr,
	&fpc1080_attr_frames_captured.attr.attr,
	&fpc1080_attr_frames_stored.attr.attr,
	NULL
};

static const struct attribute_group fpc1080_diag_attr_group = {
	.attrs = fpc1080_diag_attrs,
	.name = "diag"
};


/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */
static ssize_t fpc1080_show_attr_adc_setup(struct device *dev,
				struct device_attribute *attr, char *buf)
{
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: \n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return -ENOTTY;
	/*u8 *target;
	struct fpc1080_data *fpc1080;
	struct fpc1080_attribute *fpc_attr;
	fpc1080 = dev_get_drvdata(dev);
	fpc_attr = container_of(attr, struct fpc1080_attribute, attr);

	target = ((u8 *)&fpc1080->nav_settings) + fpc_attr->offset;
	return scnprintf(buf, PAGE_SIZE, "%s: %i\n", attr->attr.name,
				*target);*/

}

/* -------------------------------------------------------------------- */
static ssize_t fpc1080_store_attr_adc_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	u8 *target;
	u8 tmp;
	struct fpc1080_data *fpc1080;
	struct fpc1080_attribute *fpc_attr;
	fpc1080 = dev_get_drvdata(dev);
	fpc_attr = container_of(attr, struct fpc1080_attribute, attr);

	if ((sscanf(buf, "%hhu", &tmp)) <= 0)
		return -EINVAL;

	target = ((u8 *)&fpc1080->adc_setup) + fpc_attr->offset;
	*target = tmp;

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_API("[FPC1080]%s: offset=%d attr=%s val=%d[0x%02X]\n", __func__, fpc_attr->offset, attr->attr.name, tmp, tmp);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return strnlen(buf, count);
}

/* -------------------------------------------------------------------- */
static ssize_t fpc1080_show_attr_diag(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fpc1080_data *fpc1080;
	struct fpc1080_attribute *fpc_attr;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
#ifdef DEBUG_FPC1080_PERFORM
	ktime_t kt_selftest_s, kt_selftest_e;
	kt_selftest_s = ktime_get();
#endif
	fpc1080_state = FPC1080_STATE_DIAG;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */

	fpc1080 = dev_get_drvdata(dev);
	fpc_attr = container_of(attr, struct fpc1080_attribute, attr);

	if(fpc_attr->offset == offsetof(struct fpc1080_diag, selftest)) {
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		DBG_PRINT_API("[FPC1080]%s: selftest start  ###########-->>\n", __func__);
		fpc1080_selftest_fj(fpc1080, 1);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */

		fpc1080_selftest_short(fpc1080);

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		fpc1080_selftest_fj(fpc1080, 0);
		DBG_PRINT_API("[FPC1080]%s: selftest=%u(%s)  <<--###########\n", __func__,
				fpc1080->diag.selftest, fpc1080->diag.selftest ? "OK" : "NG");
#ifdef DEBUG_FPC1080_PERFORM
		kt_selftest_e = ktime_get();
		DBG_PRINT_PERFORM("[FPC1080]%s: FPC1080_PERFORM %lld\n", __func__,
			ktime_to_us(ktime_sub(kt_selftest_e, kt_selftest_s)));
#endif
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
		return scnprintf(buf, PAGE_SIZE, "%i\n",
				 fpc1080->diag.selftest);
	}

	if(fpc_attr->offset == offsetof(struct fpc1080_diag, capture_time)) {
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		DBG_PRINT_API("[FPC1080]%s: capture_time=%u\n", __func__, fpc1080->diag.capture_time);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
		return scnprintf(buf, PAGE_SIZE, "%i\n",
				 fpc1080->diag.capture_time);
	}

	if(fpc_attr->offset == offsetof(struct fpc1080_diag, frames_captured)) {
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		DBG_PRINT_API("[FPC1080]%s: frames_captured=%u\n", __func__, fpc1080->diag.frames_captured);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
		return scnprintf(buf, PAGE_SIZE, "%i\n",
				 fpc1080->diag.frames_captured);
	}

	if(fpc_attr->offset == offsetof(struct fpc1080_diag, frames_stored)) {
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		DBG_PRINT_API("[FPC1080]%s: frames_stored=%u\n", __func__, fpc1080->diag.frames_stored);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
		return scnprintf(buf, PAGE_SIZE, "%i\n",
				 fpc1080->diag.frames_stored);
	}

	return -ENOENT;
}

/* -------------------------------------------------------------------- */
static ssize_t fpc1080_store_attr_diag(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: \n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return -EPERM;
}

/* -------------------------------------------------------------------- */
static int fpc1080_wait_for_irq(struct fpc1080_data *fpc1080,
							int timeout)
{
	int result;

	if (!timeout) {
		result = wait_event_interruptible(
				fpc1080->waiting_interrupt_return,
				fpc1080->interrupt_done);
	} else {
		result = wait_event_interruptible_timeout(
				fpc1080->waiting_interrupt_return,
				fpc1080->interrupt_done, timeout);
	}

	if (result < 0) {
		dev_err(&fpc1080->spi->dev, "wait_event_interruptible "
				"interrupted by signal.\n");
		return result;
	}

	if (result || !timeout) {
		fpc1080->interrupt_done = 0;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		DBG_PRINT_FUNC("[FPC1080]%s: detected wake-up\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
		return 0;
	}

	return -ETIMEDOUT;
}

/* -------------------------------------------------------------------- */
static int fpc1080_spi_wr_rd(struct fpc1080_data *fpc1080, u8 *tx,
					u8 *rx, unsigned int length)
{
	int error;
	struct spi_message m;
	struct spi_transfer t = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1080_SPI_CLOCK_SPEED,
		.tx_buf = tx,
		.rx_buf = rx,
		.len = length,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN cmd:%s length=%d\n", __func__, length ? FPC1080_CMD(tx[0]) : "None", length);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	error = spi_sync(fpc1080->spi, &m);

	if (error) {
		dev_err(&fpc1080->spi->dev, "spi_sync failed.\n");
		return error;
	}

	fpc1080->avail_data = length;
	fpc1080->data_offset = 0;
	return 0;
}

/* -------------------------------------------------------------------- */
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-S */
int fpc1080_spi_wr_reg(struct fpc1080_data *fpc1080, u8 addr,
						u8 value, unsigned int size)
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-E */
{
	int error;
	fpc1080->huge_buffer[0] = addr;
	fpc1080->huge_buffer[1] = value;
	error = fpc1080_spi_wr_rd(fpc1080, fpc1080->huge_buffer,
				fpc1080->huge_buffer + size, size);
	if (error)
		return error;

	fpc1080->data_offset = size > FPC1080_SYNCED_REG_SIZE ?
				size + FPC1080_SYNCED_REG_SIZE : size + 1;
	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_reset(struct fpc1080_data *fpc1080)
{
	int error = 0;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN INT=%d\n", __func__, gpio_get_value(fpc1080->irq_gpio));
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	gpio_set_value(fpc1080->reset_gpio, 0);
		udelay(1000);
	gpio_set_value(fpc1080->reset_gpio, 1);
		udelay(1250);
	error = gpio_get_value(fpc1080->irq_gpio) ? 0 : -EIO;
		if (error) {
			printk(KERN_INFO "reset timed out, waiting again..\n");
			udelay(1250);
			error = gpio_get_value(fpc1080->irq_gpio) ? 0 : -EIO;
		}

	disable_irq(fpc1080->irq);
	fpc1080->interrupt_done = 0;
	enable_irq(fpc1080->irq);

	if (error) {
/* FUJITSU:2013-03-13 FingerPrintSensor_FPC1080 add-S */
		if (fpc1080_state != FPC1080_STATE_CLOSE) {
			char ftabuf[100];
			memset(ftabuf, 0, sizeof(ftabuf));
			snprintf(ftabuf, sizeof(ftabuf), "fpc1080 reset timed out.[%d]\n", fpc1080_state);
			ftadrv_send_str((const unsigned char*)ftabuf);
		}
/* FUJITSU:2013-03-13 FingerPrintSensor_FPC1080 add-E */
		dev_err(&fpc1080->spi->dev, "irq after reset timed out\n");
		return -EIO;
	}

	error = fpc1080_spi_wr_reg(fpc1080, FPC1080_RD_INTERRUPT_WITH_CLEAR,
									0, 2);


	if (error)
		return error;

	if (fpc1080->huge_buffer[fpc1080->data_offset] != FPC1080_IRQ_REBOOT) {
/* FUJITSU:2013-03-13 FingerPrintSensor_FPC1080 add-S */
		if (fpc1080_state != FPC1080_STATE_CLOSE) {
			char ftabuf[100];
			memset(ftabuf, 0, sizeof(ftabuf));
			snprintf(ftabuf, sizeof(ftabuf), "fpc1080 unexpected response at reset.[%d]\n", fpc1080_state);
			ftadrv_send_str((const unsigned char*)ftabuf);
		}
/* FUJITSU:2013-03-13 FingerPrintSensor_FPC1080 add-E */
		dev_err(&fpc1080->spi->dev, "unexpected response at reset.\n");
		return -EIO;
	}
	fpc1080->data_offset = 0;
	fpc1080->avail_data = 0;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT INT=%d\n", __func__, gpio_get_value(fpc1080->irq_gpio));
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_capture_single(struct fpc1080_data *fpc1080, int mode)
{
	int error;
	u8 pat1, pat2, irq;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_CAPT("[FPC1080]%s: IN mode=%d\n", __func__, mode);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	pat1 = 0x55;
	pat2 = 0xAA;
	switch (mode) {
	case 0:
		break;
	case 1:
		pat1 = 0xAA;
		pat2 = 0x55;

	case 2:
		error = fpc1080_wreg(fpc1080, FPC1080_FNGR_DRIVE_CONF,
				     FPC1080_SPI_FNGR_DRV_TST);
		if (error)
			return error;
		error = fpc1080_wreg(fpc1080, FPC1080_PATTERN1_XREG, pat1);
		if (error)
			return error;
		error = fpc1080_wreg(fpc1080, FPC1080_PATTERN2_XREG, pat2);
		if (error)
			return error;
		break;
	default:
		return -EINVAL;

	}

	error = fpc1080_write_adc_setup(fpc1080);
	if (error)
		return error;
	error = fpc1080_cmd_wait_irq(fpc1080, FPC1080_CAPTURE_IMAGE, &irq);
	if (error)
		return error;
	return fpc1080_spi_rd_image(fpc1080, 0);
}

/* -------------------------------------------------------------------- */
static int fpc1080_spi_rd_image(struct fpc1080_data *fpc1080, bool capture)
{
	int error;
	u8* tx_ptr;
	struct spi_message spi_mess;
	struct spi_transfer trans_rd_cap1;

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN frame=%d\n", __func__, fpc1080->current_frame);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	memset(&trans_rd_cap1, 0 , sizeof(struct spi_transfer));

	if (fpc1080->current_frame >= FPC1080_MAX_FRAMES)
	  fpc1080->current_frame = 0;

	tx_ptr = fpc1080->huge_buffer +
	  fpc1080->current_frame * FPC1080_FRAME_SIZE;

/* FUJITSU:2013-04-22 FingerPrintSensor_FPC1080 mod-S */
	//*tx_ptr = FPC1080_READ_AND_CAPTURE_IMAGE;
	*tx_ptr = capture ? FPC1080_READ_AND_CAPTURE_IMAGE : FPC1080_READ_IMAGE_DATA;
/* FUJITSU:2013-04-22 FingerPrintSensor_FPC1080 mod-E */

	trans_rd_cap1.cs_change = 0;
	trans_rd_cap1.delay_usecs = 0;
	trans_rd_cap1.speed_hz = FPC1080_SPI_CLOCK_SPEED;
	trans_rd_cap1.tx_buf = tx_ptr;
	trans_rd_cap1.rx_buf = tx_ptr;
/* FUJITSU:2013-04-26 FingerPrintSensor_FPC1080 mod-S */
	/*trans_rd_cap1.len = 1026;*/
	trans_rd_cap1.len = FPC1080_FRAME_SIZE + FPC1080_SPI_BLOCK_SIZE;
/* FUJITSU:2013-04-26 FingerPrintSensor_FPC1080 mod-E */
	trans_rd_cap1.bits_per_word = 8;

	spi_message_init(&spi_mess);
	spi_message_add_tail(&trans_rd_cap1, &spi_mess);

	error = spi_sync(fpc1080->spi, &spi_mess);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-S */
	if (error) {
		dev_err(&fpc1080->spi->dev, "spi_sync failed.\n");
		return error;
	}
	if (!capture)
		DBG_PRINT_DUMP("CAPTURE_SINGLE", trans_rd_cap1.rx_buf, trans_rd_cap1.len );
	memmove(tx_ptr, tx_ptr + 2, FPC1080_FRAME_SIZE);	/* FUJITSU:2013-04-22 FingerPrintSensor_FPC1080 mod */
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-E */

	fpc1080->current_frame++;
	fpc1080->avail_data += FPC1080_FRAME_SIZE;
	wake_up_interruptible(&fpc1080->waiting_data_avail);

	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_spi_read_wait_irq(struct fpc1080_data *fpc1080, u8 *irq)
{
	int error;
	u8 buf[4];
	struct spi_message m;

	struct spi_transfer t = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1080_SPI_CLOCK_SPEED,
		.tx_buf = buf,
		.rx_buf = buf + 2,
		.len = 2,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	buf[0] = FPC1080_RD_INTERRUPT_WITH_CLEAR;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	while (1) {
		error = fpc1080_wait_for_irq(fpc1080,
					     FPC1080_DEFAULT_IRQ_TIMEOUT);
		if (error == 0)
			break;
		if (fpc1080->thread_task.should_stop)
			return -EINTR;
		if (error != -ETIMEDOUT)
			return error;
	}
	if (error)
		return error;

	error = spi_sync(fpc1080->spi, &m);

	if (error) {
		dev_err(&fpc1080->spi->dev, "spi_sync failed.\n");
		return error;
	}

	*irq = buf[3];
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT *irq=0x%02X\n", __func__, *irq);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_cmd_wait_irq(struct fpc1080_data *fpc1080, u8 cmd, u8 *irq)
{
	int error;
	struct spi_message m;
	struct spi_transfer t = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1080_SPI_CLOCK_SPEED,
		.tx_buf = &cmd,
		.rx_buf = NULL,
		.len = 1,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_CAPT("[FPC1080]%s: IN cmd:%s\n", __func__, FPC1080_CMD(cmd));
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	error = spi_sync(fpc1080->spi, &m);

	if (error) {
		dev_err(&fpc1080->spi->dev, "spi_sync failed.\n");
		return error;
	}

	return fpc1080_spi_read_wait_irq(fpc1080, irq);
}

/* -------------------------------------------------------------------- */
static int fpc1080_spi_is_motion(struct fpc1080_data *fpc1080)
{
	int error;
	u8 tx[7];
	u8 rx[7];

	struct spi_message m;

	struct spi_transfer t = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1080_SPI_CLOCK_SPEED,
		.tx_buf = tx,
		.rx_buf = rx,
		.len = 7,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	tx[0] = FPC1080_SMRT_DATA;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);


	error = spi_sync(fpc1080->spi, &m);

	if (error) {
		dev_err(&fpc1080->spi->dev, "spi_sync failed.\n");
		return error;
	}
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_CAPT("[FPC1080]%s: OUT  [%02X:%02X]\n", __func__, rx[2 + FPC1080_SMRT_X_BYTE], rx[2 + FPC1080_SMRT_Y_BYTE]);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return ((rx[2 + FPC1080_SMRT_X_BYTE] > 0) ||
		(rx[2 + FPC1080_SMRT_Y_BYTE] >= 3));
}

/* -------------------------------------------------------------------- */
static int fpc1080_wreg(struct fpc1080_data *fpc1080, u8 addr, u8 value)
{
	int error;
	u8 tx[2];

	struct spi_message m;

	struct spi_transfer t = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1080_SPI_CLOCK_SPEED,
		.tx_buf = tx,
		.rx_buf = NULL,
		.len = 2,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN addr=0x%02X val=0x%02X\n", __func__, addr, value);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	tx[0] = addr;
	tx[1] = value;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	error = spi_sync(fpc1080->spi, &m);

	if (error)
		dev_err(&fpc1080->spi->dev, "spi_sync failed.\n");

	dev_dbg(&fpc1080->spi->dev, "wrote %X to register %X\n", value, addr);
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1080_thread_goto_idle(struct fpc1080_data *fpc1080)
{
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	fpc1080->thread_task.should_stop = 1;
	fpc1080->thread_task.mode = FPC1080_THREAD_IDLE_MODE;
	if (down_interruptible(&fpc1080->thread_task.sem_idle))
		return -ERESTARTSYS;

	up(&fpc1080->thread_task.sem_idle);

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_start_thread(struct fpc1080_data *fpc1080, int mode)
{
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	 fpc1080->thread_task.should_stop = 0;
	 fpc1080->thread_task.mode = mode;
	 wake_up_interruptible(&fpc1080->thread_task.wait_job);

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	 return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_write_adc_setup(struct fpc1080_data *fpc1080)
{
	int error;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: gain:%d offset:%d pxl_setup:%d\n", __func__,
		fpc1080->adc_setup.gain, fpc1080->adc_setup.offset, fpc1080->adc_setup.pxl_setup);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	error = fpc1080_wreg(fpc1080, FPC1080_ADC_GAIN,
			     fpc1080->adc_setup.gain);
	if (error)
		return error;

	error = fpc1080_wreg(fpc1080, FPC1080_ADC_OFFSET,
			     fpc1080->adc_setup.offset);
	if (error)
		return error;

	return fpc1080_wreg(fpc1080, FPC1080_PXL_SETUP,
			    fpc1080->adc_setup.pxl_setup);
}

/* -------------------------------------------------------------------- */
static int fpc1080_capture_task(struct fpc1080_data *fpc1080)
{
	int error;
	int keep_image;
	bool finger_present = false;

	u8 irq;
	u32 total_captures;

	struct timespec ts_start, ts_end, ts_delta;

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_CAPT("[FPC1080]%s: IN  ###########-->>\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	error = fpc1080_write_adc_setup(fpc1080);
	if (error)
		goto out;

	error = fpc1080_wreg(fpc1080, FPC1080_FNGR_DRIVE_CONF,
			     (FPC1080_SPI_FNGR_DRV_EXT |
			      FPC1080_SPI_SMRT_SENS_EN));
	if (error)
		goto out;

	error = fpc1080_cmd_wait_irq(fpc1080,
				     FPC1080_WAIT_FOR_FINGER_PRESENT,
				     &irq);
	if (error)
		goto out;

	finger_present = true;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	fpc1080_finger_present(fpc1080);
	DBG_PRINT_CAPT("[FPC1080]%s: Detected FINGER_PRESENT IntReg:0x%02X  ###########\n", __func__, irq);
	if (irq == FPC1080_IRQ_REBOOT) {
		error = -EIO;
		goto out;
	}
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */

	getnstimeofday(&ts_start);

	error = fpc1080_cmd_wait_irq(fpc1080, FPC1080_CAPTURE_IMAGE, &irq);
	if (error)
		goto out;

	keep_image = 1;
	total_captures = 0;

	while (finger_present) {
		if (fpc1080->current_frame >= FPC1080_MAX_FRAMES)
			keep_image = false;

		if (fpc1080->thread_task.should_stop) {
			error = -EINTR;
			break;
		}

		if (finger_present)
			total_captures++;

		if (keep_image) {
			error = fpc1080_cmd_wait_irq(fpc1080,
						     FPC1080_SET_SMART_REF,
						     &irq);
			if (error)
				goto out;

			error = fpc1080_spi_rd_image(fpc1080, 1);
			if (error)
				break;

			fpc1080_spi_read_wait_irq(fpc1080, &irq);
		} else {
			error = fpc1080_cmd_wait_irq(fpc1080,
						     FPC1080_CAPTURE_IMAGE,
						     &irq);
			if (error)
				break;
		}

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-S */
		if (irq & FPC1080_IRQ_FING_UP) {
			DBG_PRINT_CAPT("[FPC1080]%s: FPC1080_IRQ_FINGER_UP IntReg:0x%02X  ###########\n", __func__, irq);
			finger_present = false;
		}
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-E */

		error = fpc1080_spi_is_motion(fpc1080);
		if (error < 0)
			break;

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		if (keep_image != error) {
			DBG_PRINT_CAPT("[FPC1080]%s: keep_image = %d -> %d    captured %u  frame %u   avail_data %u -----\n",
					__func__, keep_image, error, total_captures, fpc1080->current_frame, fpc1080->avail_data);
		}
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
		keep_image = (finger_present)? error : false;
	}

	getnstimeofday(&ts_end);
	ts_delta = timespec_sub(ts_end, ts_start);

	fpc1080->diag.capture_time = ts_delta.tv_nsec / NSEC_PER_MSEC;
	fpc1080->diag.capture_time += (ts_delta.tv_sec * MSEC_PER_SEC);

	fpc1080->diag.frames_stored = fpc1080->current_frame;
	fpc1080->diag.frames_captured = total_captures;

	if (fpc1080->diag.capture_time > 0) {
/* FUJITSU:2013-03-13 FingerPrintSensor_FPC1080 add-S */
		char ftabuf[100];
		memset(ftabuf, 0, sizeof(ftabuf));
		snprintf(ftabuf, sizeof(ftabuf),
			"fpc1080 error=%d captured %lu frames (%lu kept) in %lu ms (%lu fps)\n", error,
			(long unsigned int)fpc1080->diag.frames_captured,
			(long unsigned int)fpc1080->diag.frames_stored,
			(long unsigned int)fpc1080->diag.capture_time,
			(long unsigned int)(total_captures * MSEC_PER_SEC /
					    fpc1080->diag.capture_time));
		ftadrv_send_str((const unsigned char*)ftabuf);
/* FUJITSU:2013-03-13 FingerPrintSensor_FPC1080 add-E */
		dev_dbg(&fpc1080->spi->dev,
			"captured %lu frames (%lu kept) in %lu  ms (%lu fps)\n",
			(long unsigned int)fpc1080->diag.frames_captured,
			(long unsigned int)fpc1080->diag.frames_stored,
			(long unsigned int)fpc1080->diag.capture_time,
			(long unsigned int)(total_captures * MSEC_PER_SEC /
					    fpc1080->diag.capture_time));
	}

out:
	if (error) {
		fpc1080->avail_data = 0;
		dev_err(&fpc1080->spi->dev,
			"capture_task failed with error %i\n", error);
	}
	fpc1080->capture_done = 1;
	wake_up_interruptible(&fpc1080->waiting_data_avail);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	fpc1080_finger_up(fpc1080);
	DBG_PRINT_CAPT("[FPC1080]%s: OUT ret=%d  <<--###########\n", __func__, error);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return error;
}

/* -------------------------------------------------------------------- */
static int threadfn(void *_fpc1080)
{
	struct fpc1080_data *fpc1080 = _fpc1080;

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	while (!kthread_should_stop()) {

		up(&fpc1080->thread_task.sem_idle);
		wait_event_interruptible(fpc1080->thread_task.wait_job,
			fpc1080->thread_task.mode != FPC1080_THREAD_IDLE_MODE);

		down(&fpc1080->thread_task.sem_idle);

		switch (fpc1080->thread_task.mode) {
		case FPC1080_THREAD_CAPTURE_MODE:
			fpc1080_capture_task(fpc1080);
			break;
		default:
			break;
		}

		if(fpc1080->thread_task.mode != FPC1080_THREAD_EXIT) {
			fpc1080->thread_task.mode = FPC1080_THREAD_IDLE_MODE;
		}
	}
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_abort_capture(struct fpc1080_data *fpc1080)
{

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	fpc1080_thread_goto_idle(fpc1080);
	fpc1080->avail_data = 0;
	fpc1080->current_frame = 0;
	fpc1080->data_offset = 0;
	return fpc1080_reset(fpc1080);
}

/* -------------------------------------------------------------------- */
static int fpc1080_start_capture(struct fpc1080_data *fpc1080)
{
	int error = 0;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	error = fpc1080_abort_capture(fpc1080);

	if (error)
		return error;

	fpc1080_start_thread(fpc1080, FPC1080_THREAD_CAPTURE_MODE);
	fpc1080->capture_done = 0;

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return error;
}

/* -------------------------------------------------------------------- */
irqreturn_t fpc1080_interrupt(int irq, void *_fpc1080)
{
	struct fpc1080_data *fpc1080 = _fpc1080;
	if (gpio_get_value(fpc1080->irq_gpio)) {
		fpc1080->interrupt_done = 1;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		fpc1080->count.interrupt++;
		DBG_PRINT_FUNC("[FPC1080]%s: OUT WakeupInterruptible\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
		wake_up_interruptible(&fpc1080->waiting_interrupt_return);
		return IRQ_HANDLED;
	}

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	pr_err("[FPC1080]%s: OUT invalid interrupt\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return IRQ_NONE;
}

/* -------------------------------------------------------------------- */
static int fpc1080_selftest_short(struct fpc1080_data *fpc1080)
{
	int error;

	error = fpc1080_abort_capture(fpc1080);
	if (error) {
		dev_err(&fpc1080->spi->dev, "fpc1080 selftest, "
						"reset fail on entry.\n");
		goto err;
	}

	error = fpc1080_spi_wr_reg(fpc1080, FPC1080_REG_HW_ID, 0, 2);
	if (error) {
		dev_err(&fpc1080->spi->dev, "fpc1080 selftest, "
						"read HW-ID fail.\n");
		goto err;
	}

	if (fpc1080->huge_buffer[fpc1080->data_offset] != FPC1080_HARDWARE_ID) {
		dev_err(&fpc1080->spi->dev, "fpc1080 selftest, "
						"HW-ID mismatch.\n");
		error = -EIO;
		goto err;
	}

	error= fpc1080_wreg(fpc1080, FPC1080_CAPTURE_IMAGE, 0);
	if (error) {
		dev_err(&fpc1080->spi->dev, "fpc1080 selftest, "
						"capture cmd fail.\n");
		goto err;
	}

	error = fpc1080_wait_for_irq(fpc1080, FPC1080_DEFAULT_IRQ_TIMEOUT);
	if (error) {
		dev_err(&fpc1080->spi->dev, "fpc1080 selftest, "
						"irq timeout.\n");
		goto err;
	}

	error = fpc1080_abort_capture(fpc1080);
	if (error) {
		dev_err(&fpc1080->spi->dev, "fpc1080 selftest, "
						"reset fail on exit.\n");
		goto err;
	}

err:
	fpc1080->diag.selftest = (error == 0)? 1 : 0;
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1080_open(struct inode *inode, struct file *file)
{
	struct fpc1080_data *fpc1080;
	int error;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-S */
#ifdef DEBUG_FPC1080_PERFORM
	ktime_t kt_open_e;
	kt_open = ktime_get();
#endif
	DBG_PRINT_FULL("[FPC1080]%s: IN ==============================>\n", __func__);
	fpc1080 = container_of(inode->i_cdev, struct fpc1080_data, cdev);
	fpc1080->count.read = fpc1080->count.write = fpc1080->count.poll = fpc1080->count.ioctl = fpc1080->count.interrupt = 0;
	fpc1080->count.open++;
	fpc1080_state = FPC1080_STATE_OPEN;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-E */

	if (down_interruptible(&fpc1080->mutex))
		return -ERESTARTSYS;

	file->private_data = fpc1080;
/* FUJITSU:2013-03-06 FingerPrintSensor_FPC1080 mod-S */
	error = fpc1080_open_fj(fpc1080);
	if (error) {
		goto exit;
	}
	error = fpc1080_reset(fpc1080);
	if (error) {
		fpc1080_deep_sleep(fpc1080);
	}
exit:
	up(&fpc1080->mutex);
	DBG_PRINT_API("[FPC1080]%s: OUT ret=%d\n", __func__, error);
#ifdef DEBUG_FPC1080_PERFORM
	kt_open_e = ktime_get();
	DBG_PRINT_PERFORM("[FPC1080]%s: FPC1080_PERFORM %lld\n", __func__,
		ktime_to_us(ktime_sub(kt_open_e, kt_open)));
#endif
/* FUJITSU:2013-03-06 FingerPrintSensor_FPC1080 mod-E */
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1080_release(struct inode *inode, struct file *file)
{
	int status;
	struct fpc1080_data *fpc1080 = file->private_data;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
#ifdef DEBUG_FPC1080_PERFORM
	ktime_t kt_release_s, kt_release_e;
	kt_release_s = ktime_get();
#endif
	DBG_PRINT_FULL("[FPC1080]%s: IN open=%u read=%u write=%u poll=%u ioctl=%u interrupt=%u <==============================\n",
			__func__, fpc1080->count.open, fpc1080->count.read, fpc1080->count.write,
			fpc1080->count.poll, fpc1080->count.ioctl, fpc1080->count.interrupt);
	fpc1080_state = FPC1080_STATE_CLOSE;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	status = 0;
	if (down_interruptible(&fpc1080->mutex))
		return -ERESTARTSYS;

	fpc1080_abort_capture(fpc1080);

	if (!atomic_read(&file->f_count)) {
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-S */
		status = fpc1080_deep_sleep(fpc1080);
		fpc1080_release_fj(fpc1080);
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-E */
	}
	up(&fpc1080->mutex);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_API("[FPC1080]%s: OUT ret=%d <==============================\n", __func__, status);
#ifdef DEBUG_FPC1080_PERFORM
	kt_release_e = ktime_get();
	DBG_PRINT_PERFORM("[FPC1080]%s: FPC1080_PERFORM %lld:%lld\n", __func__,
		ktime_to_us(ktime_sub(kt_release_e, kt_release_s)),
		ktime_to_us(ktime_sub(kt_release_e, kt_open)));
#endif
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return status;
}

/* -------------------------------------------------------------------- */
static ssize_t fpc1080_read(struct file *file, char *buff,
				size_t count, loff_t *ppos)
{
	int error;
	unsigned int max_dat;

	struct fpc1080_data *fpc1080 = file->private_data;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_API2("[FPC1080]%s: IN count=%d avail_data=%d\n", __func__, count, fpc1080->avail_data);
	fpc1080->count.read++;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	error = 0;

	if (down_interruptible(&fpc1080->mutex))
		return -ERESTARTSYS;

	if (!fpc1080->capture_done) {
		error = wait_event_interruptible(
				fpc1080->waiting_data_avail,
				(fpc1080->capture_done || fpc1080->avail_data));
	}

	max_dat = (count > fpc1080->avail_data) ? fpc1080->avail_data : count;
	if (max_dat) {
		error = copy_to_user(buff,
			&fpc1080->huge_buffer[fpc1080->data_offset], max_dat);
		if (error)
			goto out;

		fpc1080->data_offset += max_dat;
		fpc1080->avail_data -= max_dat;
		error = max_dat;
	}
out:
	up(&fpc1080->mutex);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_API2("[FPC1080]%s: OUT ret=%d\n", __func__, error);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return error;
}

/* -------------------------------------------------------------------- */
static ssize_t fpc1080_write(struct file *file, const char *buff,
					size_t count, loff_t *ppos)
{
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	struct fpc1080_data *fpc1080 = file->private_data;
	fpc1080->count.write++;
	DBG_PRINT_API("[FPC1080]%s: OUT write system-call unavailable\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return -ENOTTY;
}

/* -------------------------------------------------------------------- */
static unsigned int fpc1080_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;
	struct fpc1080_data *fpc1080 = file->private_data;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_API2("[FPC1080]%s: IN\n", __func__);
	fpc1080->count.poll++;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */

	if (down_interruptible(&fpc1080->mutex))
		return -ERESTARTSYS;

	if (fpc1080->avail_data == 0 && !fpc1080->capture_done)
		poll_wait(file, &fpc1080->waiting_data_avail, wait);

	if (fpc1080->avail_data > 0)
		ret |= 	(POLLIN | POLLRDNORM);
	else if (fpc1080->capture_done)
		ret |= POLLHUP;

	up(&fpc1080->mutex);

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	if (ret)
		DBG_PRINT_API2("[FPC1080]%s: OUT ret=0x%04X\n",__func__, ret);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return ret;
}

/* -------------------------------------------------------------------- */
static long fpc1080_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg) {
	int error;
	struct fpc1080_data *fpc1080 = filp->private_data;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
#ifdef DEBUG_FPC1080_PERFORM
	ktime_t kt_ioctl_s, kt_ioctl_e;
	kt_ioctl_s = ktime_get();
#endif
	DBG_PRINT_FULL("[FPC1080]%s: IN cmd=0x%X [%s]\n", __func__, cmd, FPC1080_IOCTL_CMD(cmd));
	fpc1080->count.ioctl++;
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	error = 0;
	if (down_interruptible(&fpc1080->mutex))
		return -ERESTARTSYS;

	switch (cmd) {
	case FPC1080_IOCTL_START_CAPTURE:
		error = fpc1080_start_capture(fpc1080);
		break;
	case FPC1080_IOCTL_ABORT_CAPTURE:
		error = fpc1080_abort_capture(fpc1080);
		break;
	case FPC1080_IOCTL_CAPTURE_SINGLE:
		error = fpc1080_abort_capture(fpc1080);
		if (error)
			break;

		error = fpc1080_capture_single(fpc1080, arg);
		break;
	default:
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
		dev_err(&fpc1080->spi->dev, "undefined FPC1080_IOCTL_xxx cmd:0x%X\n", cmd);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
		error = -ENOTTY;
		break;
	}
	up(&fpc1080->mutex);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_API("[FPC1080]%s: OUT ret=%d\n",__func__, error);
#ifdef DEBUG_FPC1080_PERFORM
	kt_ioctl_e = ktime_get();
	DBG_PRINT_PERFORM("[FPC1080]%s: FPC1080_PERFORM %lld\n", __func__,
		ktime_to_us(ktime_sub(kt_ioctl_e, kt_ioctl_s)));
#endif
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1080_cleanup(struct fpc1080_data *fpc1080)
{
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	printk(KERN_INFO "[FPC1080]%s: IN\n", __func__);
	fpc1080_cleanup_fj(fpc1080);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	if (fpc1080->thread_task.thread) {
		fpc1080->thread_task.should_stop = 1;
		fpc1080->thread_task.mode = FPC1080_THREAD_EXIT;
		wake_up_interruptible(&fpc1080->thread_task.wait_job);
		kthread_stop(fpc1080->thread_task.thread);
	}

	if (!IS_ERR_OR_NULL(fpc1080->device))
		device_destroy(fpc1080->class, fpc1080->devno);

	class_destroy(fpc1080->class);

	if (fpc1080->irq >= 0)
		free_irq(fpc1080->irq, fpc1080);

	if (gpio_is_valid(fpc1080->irq_gpio))
		gpio_free(fpc1080->irq_gpio);

	if (gpio_is_valid(fpc1080->reset_gpio))
		gpio_free(fpc1080->reset_gpio);

	if (fpc1080->huge_buffer) {
		free_pages((unsigned long)fpc1080->huge_buffer,
			   get_order(FPC1080_IMAGE_BUFFER_SIZE));
	}

	kfree(fpc1080);

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return 0;
}

/* -------------------------------------------------------------------- */
static int __devinit fpc1080_probe(struct spi_device *spi)
{
	struct fpc1080_platform_data *fpc1080_pdata;
	int error;
	struct fpc1080_data *fpc1080 = NULL;

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	error = 0;

	fpc1080 = kzalloc(sizeof(*fpc1080), GFP_KERNEL);
	if (!fpc1080) {
		dev_err(&spi->dev,
		"failed to allocate memory for struct fpc1080_data\n");

		return -ENOMEM;
	}

	fpc1080->huge_buffer =
		(u8 *)__get_free_pages(GFP_KERNEL,
/* FUJITSU:2013-04-26 FingerPrintSensor_FPC1080 mod-S */
				       /*get_order(FPC1080_IMAGE_BUFFER_SIZE + 2));*/
				       get_order(FPC1080_IMAGE_BUFFER_SIZE + FPC1080_SPI_BLOCK_SIZE));
/* FUJITSU:2013-04-26 FingerPrintSensor_FPC1080 mod-E */

	if (!fpc1080->huge_buffer) {
		dev_err(&fpc1080->spi->dev, "failed to get free pages\n");
		return -ENOMEM;
	}

	spi_set_drvdata(spi, fpc1080);
	fpc1080->spi = spi;
	fpc1080->reset_gpio = -EINVAL;
	fpc1080->irq_gpio = -EINVAL;
	fpc1080->irq = -EINVAL;

	init_waitqueue_head(&fpc1080->waiting_interrupt_return);
	init_waitqueue_head(&fpc1080->waiting_data_avail);
	fpc1080_pdata = spi->dev.platform_data;

	memset(&(fpc1080->diag), 0, sizeof(fpc1080->diag));

	if (!fpc1080_pdata) {
		dev_err(&fpc1080->spi->dev,
				"spi->dev.platform_data is NULL.\n");
		error = -EINVAL;
		goto err;
	}

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-S */
#if 0
	error = gpio_request(fpc1080_pdata->reset_gpio, "fpc1080_reset");

	if (error) {
		dev_err(&fpc1080->spi->dev,
			"fpc1080_probe - gpio_request (reset) failed.\n");
		goto err;
	}

	fpc1080->reset_gpio = fpc1080_pdata->reset_gpio;

	error = gpio_direction_output(fpc1080->reset_gpio, 1);

	if (error) {
		dev_err(&fpc1080->spi->dev,
		"fpc1080_probe - gpio_direction_output(reset) failed.\n");

		goto err;
	}

	error = gpio_request(fpc1080_pdata->irq_gpio, "fpc1080 irq");

	if (error) {
		dev_err(&fpc1080->spi->dev, "gpio_request (irq) failed.\n");
		goto err;
	}

	fpc1080->irq_gpio = fpc1080_pdata->irq_gpio;

	error = gpio_direction_input(fpc1080->irq_gpio);

	if (error) {
		dev_err(&fpc1080->spi->dev,
			"gpio_direction_input (irq) failed.\n");
		goto err;
	}
#else
	fpc1080_probe_fj(fpc1080);
	fpc1080->reset_gpio = fpc1080_pdata->reset_gpio;
	fpc1080->irq_gpio = fpc1080_pdata->irq_gpio;
#endif
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-E */

	fpc1080->irq = gpio_to_irq(fpc1080->irq_gpio);

	if (fpc1080->irq < 0) {
		dev_err(&fpc1080->spi->dev, "gpio_to_irq failed.\n");
		error = fpc1080->irq;
		goto err;
	}

	error = request_irq(fpc1080->irq, fpc1080_interrupt,
			IRQF_TRIGGER_RISING, "fpc1080", fpc1080);

	if (error) {
		dev_err(&fpc1080->spi->dev, "request_irq %i failed.\n",
			fpc1080->irq);

		fpc1080->irq = -EINVAL;
		goto err;
	}

	fpc1080->spi->mode = SPI_MODE_0;
	fpc1080->spi->bits_per_word = 8;

	error = spi_setup(fpc1080->spi);

	if (error) {
		dev_err(&fpc1080->spi->dev, "spi_setup failed\n");
		goto err;
	}

	error = fpc1080_reset(fpc1080);

	if (error)
		goto err;

	error = fpc1080_spi_wr_reg(fpc1080, FPC1080_REG_HW_ID, 0, 2);
	if (error)
		goto err;

	if (fpc1080->huge_buffer[fpc1080->data_offset] != FPC1080_HARDWARE_ID) {
/* FUJITSU:2013-03-13 FingerPrintSensor_FPC1080 add-S */
		char ftabuf[50];
		memset(ftabuf, 0, sizeof(ftabuf));
		snprintf(ftabuf, sizeof(ftabuf), "fpc1080 HW-ID mismatch: %02X\n",
			fpc1080->huge_buffer[fpc1080->data_offset]);
		ftadrv_send_str((const unsigned char*)ftabuf);
/* FUJITSU:2013-03-13 FingerPrintSensor_FPC1080 add-E */
		dev_err(&fpc1080->spi->dev,
			"hardware id mismatch: %x expected %x\n",
			fpc1080->huge_buffer[fpc1080->data_offset],
			FPC1080_HARDWARE_ID);

		error = -EIO;
		goto err;
	}

	dev_info(&fpc1080->spi->dev, "hardware id: %x\n",
		 fpc1080->huge_buffer[fpc1080->data_offset]);

	fpc1080->class = class_create(THIS_MODULE, FPC1080_CLASS_NAME);

	if (IS_ERR(fpc1080->class)) {
		dev_err(&fpc1080->spi->dev, "failed to create class.\n");
		error = PTR_ERR(fpc1080->class);
		goto err;
	}

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-S */
#if 0
	fpc1080->devno = MKDEV(FPC1080_MAJOR, fpc1080_device_count++);
#endif
	error = alloc_chrdev_region(&fpc1080->devno, 0, 1, FPC1080_DEV_NAME);
	if (error < 0) {
		dev_err(&fpc1080->spi->dev, "alloc_chrdev_region failed\n");
		goto err;
	}
	DBG_PRINT_INFO("[FPC1080]%s: DeviceMajor=%d\n", __func__, MAJOR(fpc1080->devno));
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-E */

	fpc1080->device = device_create(fpc1080->class, NULL, fpc1080->devno,
						NULL, "%s", FPC1080_DEV_NAME);

	if (IS_ERR(fpc1080->device)) {
		dev_err(&fpc1080->spi->dev, "device_create failed.\n");
		error = PTR_ERR(fpc1080->device);
		goto err;
	}

	fpc1080->adc_setup.gain = fpc1080_pdata->adc_setup.gain;
	fpc1080->adc_setup.offset = fpc1080_pdata->adc_setup.offset;
	fpc1080->adc_setup.pxl_setup = fpc1080_pdata->adc_setup.pxl_setup;

	sema_init(&fpc1080->mutex, 0);
	error = sysfs_create_group(&spi->dev.kobj, &fpc1080_adc_attr_group);
	if (error) {
		dev_err(&fpc1080->spi->dev, "sysf_create_group failed.\n");
		goto err;
	}

	error = sysfs_create_group(&spi->dev.kobj, &fpc1080_diag_attr_group);
	if (error) {
		dev_err(&fpc1080->spi->dev, "sysf_create_group failed.\n");
		goto err_sysf_1;
	}

/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 del-S */
#if 0
	error = register_chrdev_region(fpc1080->devno, 1, FPC1080_DEV_NAME);

	if (error) {
		dev_err(&fpc1080->spi->dev,
		  "fpc1080_probe - register_chrdev_region failed.\n");

		goto err_sysf_2;
	}
#endif
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 del-E */

	cdev_init(&fpc1080->cdev, &fpc1080_fops);
	fpc1080->cdev.owner = THIS_MODULE;

	error = cdev_add(&fpc1080->cdev, fpc1080->devno, 1);

	if (error) {
		dev_err(&fpc1080->spi->dev, "cdev_add failed.\n");
		goto err_chrdev;
	}

	init_waitqueue_head(&fpc1080->thread_task.wait_job);
	sema_init(&fpc1080->thread_task.sem_idle, 0);
	fpc1080->thread_task.mode = FPC1080_THREAD_IDLE_MODE;
	fpc1080->thread_task.thread = kthread_run(threadfn, fpc1080, "%s",
						 FPC1080_WORKER_THREAD_NAME);
/* FUJITSU:2013-04-26 FingerPrintSensor_FPC1080 add-S */
	{
		struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
		sched_setscheduler_nocheck(fpc1080->thread_task.thread, SCHED_FIFO, &param);
	}
/* FUJITSU:2013-04-26 FingerPrintSensor_FPC1080 add-E */

#ifdef CONFIG_HAS_EARLYSUSPEND
	fpc1080->early_suspend.suspend = fpc1080_early_suspend,
	fpc1080->early_suspend.resume = fpc1080_late_resume,
	fpc1080->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	register_early_suspend(&fpc1080->early_suspend);		
#endif

	if (IS_ERR(fpc1080->thread_task.thread)) {
		dev_err(&fpc1080->spi->dev, "kthread_run failed.\n");
		goto err_chrdev;
	}

/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-S */
	error = fpc1080_deep_sleep(fpc1080);
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-E */
	if (error)
		goto err_cdev;

	up(&fpc1080->mutex);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FULL("[FPC1080]%s: Normal OUT\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return 0;
err_cdev:
	cdev_del(&fpc1080->cdev);

err_chrdev:
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 del-S */
//	unregister_chrdev_region(fpc1080->devno, 1);
//
//err_sysf_2:
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 del-E */
	sysfs_remove_group(&spi->dev.kobj, &fpc1080_diag_attr_group);

err_sysf_1:
	sysfs_remove_group(&spi->dev.kobj, &fpc1080_adc_attr_group);

err:
	fpc1080_cleanup(fpc1080);
	spi_set_drvdata(spi, NULL);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT ret=%d\n", __func__, error);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return error;
}

/* -------------------------------------------------------------------- */
static int __devexit fpc1080_remove(struct spi_device *spi)
{
	struct fpc1080_data *fpc1080 = spi_get_drvdata(spi);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	printk(KERN_INFO "[FPC1080]%s: IN\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	sysfs_remove_group(&fpc1080->spi->dev.kobj, &fpc1080_adc_attr_group);
	sysfs_remove_group(&fpc1080->spi->dev.kobj, &fpc1080_diag_attr_group);

/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-S */
	fpc1080_deep_sleep(fpc1080);
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-E */

	cdev_del(&fpc1080->cdev);
	unregister_chrdev_region(fpc1080->devno, 1);
	fpc1080_cleanup(fpc1080);
	spi_set_drvdata(spi, NULL);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_FUNC("[FPC1080]%s: OUT\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_suspend(struct device *dev)
{
	struct fpc1080_data *fpc1080 = dev_get_drvdata(dev);
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-S */
	DBG_PRINT_API("[FPC1080]%s:\n", __func__);
	fpc1080_deep_sleep(fpc1080);
/* FUJITSU:2013-02-28 FingerPrintSensor_FPC1080 mod-E */
	return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1080_resume(struct device *dev)
{
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-S */
	DBG_PRINT_API("[FPC1080]%s:\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 add-E */
	return 0;
}
/* -------------------------------------------------------------------- */

static void fpc1080_early_suspend(struct early_suspend *h)
{
	struct fpc1080_data * fpc1080;
	fpc1080	= container_of(h, struct fpc1080_data, early_suspend);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-S */
//	fpc1080_spi_wr_reg(fpc1080, FPC1080_ACTIVATE_DEEP_SLEEP, 0, 1);
	DBG_PRINT_API("[FPC1080]%s:\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-E */
}

static void fpc1080_late_resume(struct early_suspend *h)
{
	struct fpc1080_data * fpc1080;
	fpc1080	= container_of(h, struct fpc1080_data, early_suspend);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-S */
//	fpc1080_spi_wr_reg(fpc1080, FPC1080_WAIT_FOR_FINGER_PRESENT, 0, 1);
	DBG_PRINT_API("[FPC1080]%s:\n", __func__);
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-E */
}


/* -------------------------------------------------------------------- */
static int __init fpc1080_init(void)
{
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-S */
	int error;

#ifdef DEBUG_FPC1080_PERFORM
	ktime_t kt_init_s, kt_init_e;
	kt_init_s = ktime_get();
#endif
	printk(KERN_INFO "[FPC1080]%s: IN system_rev=0x%02X\n", __func__, system_rev);
	fpc1080_state = FPC1080_STATE_INIT;
	error = fpc1080_spi_assign();
	if (error)
		return error;
	if (spi_register_driver(&fpc1080_driver)) {
		pr_err("[FPC1080]%s: ERROR-OUT\n", __func__);
		return -EINVAL;
	}

	DBG_PRINT_FUNC("[FPC1080]%s: OUT\n", __func__);
#ifdef DEBUG_FPC1080_PERFORM
	kt_init_e = ktime_get();
	DBG_PRINT_PERFORM("[FPC1080]%s: FPC1080_PERFORM %lld\n", __func__,
		ktime_to_us(ktime_sub(kt_init_e, kt_init_s)));
#endif
/* FUJITSU:2013-01-28 FingerPrintSensor_FPC1080 mod-E */
	return 0;
}

/* -------------------------------------------------------------------- */
static void __exit fpc1080_exit(void)
{
	spi_unregister_driver(&fpc1080_driver);
}

/* -------------------------------------------------------------------- */
