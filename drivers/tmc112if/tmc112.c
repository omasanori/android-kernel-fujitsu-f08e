/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2011
/*----------------------------------------------------------------------------*/

#define DEBUG

#ifdef	DEBUG
#define	DBG_PRINTK(x, ...)	printk("tmc112(%d):" x, __LINE__, ##__VA_ARGS__)
#endif

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/irqreturn.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/firmware.h>
#include <linux/ihex.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
/* 2012-01-16 CAMERA POWER ON [add] start */
#include <linux/mfd/pm8xxx/gpio.h>
/* 2012-01-16 CAMERA POWER ON [add] end */
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/spinlock_types.h>

#include <linux/clk.h>
#include <linux/ioctl.h>

#include <mach/msm_iomap.h>

#include <asm/system.h> /* FUJITSU: 2012-09-25 MC Camera M6MO -> M7MO add */

#ifndef _TMC112_H_
#include "tmc112.h"
#endif

/* 2012-01-16 CAMERA POWER ON [add] start */
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
/* 2012-01-16 CAMERA POWER ON [add] end */

/**********************************************************************/
/* macros															  */
/**********************************************************************/
#define	DEBUG_SLOW_WRITE				1

#define	SPI_CLK_MAX_2ND	(25600000)

#ifdef	DEBUG_SLOW_WRITE
#define	SPI_CLK_MAX_RW	5400000				/* clk_tbl_gsbi_qup */
#else
#define	SPI_CLK_MAX_RW	SPI_CLK_MAX_2ND
#endif


/**********************************************************************/
/* globals															  */
/**********************************************************************/
static struct spi_device *tmc112 = NULL;	/* spi_device* created in probe */

/**
 *	tmc112_read
 *
 *	read the status from ISP
 *
 *	@param	*f		[in]	file descriptor
 *	@param	*data	[in]	buffer
 *	@param	len		[in]	max size
 *	@param	*off	[in]	read offset in buffer
 *	@return	>0	bytes read<br>
 *			==0	canceled by Close() is invoked<br>
 *			<0	error
 */
static ssize_t tmc112_read(struct file *f, char __user *data, size_t len, loff_t *off)
{
	return len;
}


/**
 *	tmc112_write
 *
 *	write() sends command/parameter to ISP.
 *
 *	@param	*f		[in]	file descriptor
 *	@param	*data	[in]	buffer
 *	@param	len		[in]	data size
 *	@param	*off	[in]	offset in buffer
 *	@return	>0	written size<br>
 *			<=0	error
 */
static ssize_t tmc112_write(struct file *f, const char __user *data, size_t len, loff_t *off)
{
	u8 *buf = kzalloc(len, GFP_KERNEL);
	struct spi_message m;
	struct spi_transfer t;
	int ret;

	DBG_PRINTK("%s\n", __func__);
	DBG_PRINTK("*data=%p, len=%x, off=%p\n", data, len, off);


	if (!buf) {
		return -ENOMEM;
	}

	if (copy_from_user(buf, data, len)) {
		ret = -EFAULT;
		goto err;
	}

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));
	t.tx_buf = buf;
	t.len = len;
	t.bits_per_word = 8;
#ifdef	DEBUG_SLOW_WRITE
	t.speed_hz = SPI_CLK_MAX_RW;
#endif
	spi_message_add_tail(&t, &m);

	ret = spi_sync(tmc112, &m);
	if (ret != 0)
		goto err;

	kfree(buf);
	DBG_PRINTK("%s OK\n", __func__);
	return len;

err:
	DBG_PRINTK("%s ERR=%d\n", __func__, ret);
	kfree(buf);
	return ret;
}
/* FUJITSU:2012-09-19 add start */
extern void spi_env_ctrl(int runtype/* 0:off, 1:on, 2:shutdown */, int csno);
/* FUJITSU: 2013-02-07 CAMFWLIB chng start */
/* extern void spi_softcs_mutex(int enable);	*/
extern void fjdev_spi_bus4_softcs_mutex(int enable /* 0:unlock, 1:lock */);
/* FUJITSU: 2013-02-07 CAMFWLIB chng end */
/* FUJITSU:2012-09-19 add end */
/**
 *	tmc112_open
 *
 *	tmc112 initialize
 *
 *	@param	inode	[in]	inode
 *	@param	filp	[in]	filp
 *	@return	status<br>
 *			==0	OK<br>
 *			<0	error
 */
static int tmc112_open(struct inode *inode, struct file *filp)
{
	int rc = 0;

	DBG_PRINTK("%s\n", __func__);

	if (filp == NULL) {
		return -ENOMEM;
	}
	if (!try_module_get(THIS_MODULE)) {
		return -ENODEV;
	}

	/* GPIO value dump start */
#if 1
#if 0 /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del start */
	/* M6MO */
	DBG_PRINTK("%s ***********************************\n", __func__);
	rc = gpio_get_value(38);
	DBG_PRINTK("%s GPIO38 value = %d\n", __func__,rc);
	rc = gpio_get_value(40);
	DBG_PRINTK("%s GPIO40 value = %d\n", __func__,rc);
	rc = gpio_get_value(41);
	DBG_PRINTK("%s GPIO41 value = %d\n", __func__,rc);
	DBG_PRINTK("%s ***********************************\n", __func__);
#endif /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del end */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add start */
	/* M7MO */
	/* Proto Type other */
	DBG_PRINTK("%s ***********************************\n", __func__);
	rc = gpio_get_value(10);
	DBG_PRINTK("%s DATA GPIO10 value = %d\n", __func__,rc);
	rc = gpio_get_value(2);
	DBG_PRINTK("%s CS GPIO2 value = %d\n", __func__,rc);
	rc = gpio_get_value(13);
	DBG_PRINTK("%s CLK GPIO13 value = %d\n", __func__,rc);
	DBG_PRINTK("%s ***********************************\n", __func__);
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add end */
#endif
	/* GPIO value dump end */

#if 0 /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del start */
	/* M6MO */
	/* GPIO40(CS1) Low Setting */
	gpio_tlmm_config(GPIO_CFG(40, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(40);
	DBG_PRINTK("%s GPIO40 [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(40, 0);
	DBG_PRINTK("%s GPIO40 Set Low\n", __func__);
	rc = gpio_get_value(40);
	DBG_PRINTK("%s GPIO40 [after] gpio_get_value = %d\n", __func__,rc);
#endif /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del end */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add start */
	/* M7MO */
/* FUJITSU:2012-09-19 add start */
	spi_env_ctrl(1, 1);
/* FUJITSU:2012-09-19 add end */
	/* GPIO2(CS) Low Setting */
	gpio_tlmm_config(GPIO_CFG(2, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(2);
	DBG_PRINTK("%s CS GPIO2 [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(2, 0);
	DBG_PRINTK("%s CS GPIO2 Set Low\n", __func__);
	rc = gpio_get_value(2);
	DBG_PRINTK("%s CS GPIO2 [after] gpio_get_value = %d\n", __func__,rc);
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add end */

	/* wait 1ms */
	msleep(1);
	DBG_PRINTK("%s 1ms wait\n", __func__);

/* FUJITSU: 2012-09-25 MC Camera M6MO -> M7MO add start */
	/* GPIO13(SPI_CLK) GSBI4_0 FUNC_SEL 1 change */
	gpio_tlmm_config(GPIO_CFG(13, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	DBG_PRINTK("%s GPIO13(SPI_CLK) GSBI4_0 FUNC_SEL 1 change\n", __func__);

	/* GPIO10(SPI_DIN) GSBI4_3 FUNC_SEL 1 change */
	gpio_tlmm_config(GPIO_CFG(10, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	DBG_PRINTK("%s GPIO10(SPI_DIN) GSBI4_3 FUNC_SEL 1 change\n", __func__);
/* FUJITSU: 2012-09-25 MC Camera M6MO -> M7MO add end */

	/* GPIO41(SPI_CLK) High Setting */
#if 0
	gpio_tlmm_config(GPIO_CFG(41, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(41);
	DBG_PRINTK("%s GPIO41 [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(41, 1);
	DBG_PRINTK("%s GPIO41 Set High\n", __func__);
	rc = gpio_get_value(41);
	DBG_PRINTK("%s GPIO41 [after] gpio_get_value = %d\n", __func__,rc);
	gpio_tlmm_config(GPIO_CFG(41, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
#else
/* FUJITSU: 2012-01-23 add start */
/* FUJITSU: 2013-02-07 CAMFWLIB chng start */
#if 0
| /* FUJITSU:2012-09-19 add start */
| 	spi_softcs_mutex(1);
| /* FUJITSU:2012-09-19 add end */
#endif /* 0 */
	fjdev_spi_bus4_softcs_mutex( 1 );
/* FUJITSU: 2013-02-07 CAMFWLIB chng end */
	tmc112->bits_per_word = 8;
	tmc112->mode = SPI_MODE_3;
	rc = spi_setup(tmc112);
/* FUJITSU:2012-09-19 mod start */
	if (rc < 0){
/* FUJITSU: 2013-02-07 CAMFWLIB chng start */
/* 		spi_softcs_mutex(0);	*/
		fjdev_spi_bus4_softcs_mutex( 0 );
/* FUJITSU: 2013-02-07 CAMFWLIB chng end */
		return rc;
	}
/* FUJITSU:2012-09-19 mod end */
	DBG_PRINTK("%s spi_setup() ret[%d]\n", __func__,rc);
/* FUJITSU: 2012-01-23 add end */
#endif

	/* wait 1ms */
	msleep(1);
	DBG_PRINTK("%s 1ms wait\n", __func__);

	/* GPIO38(SPI_MOSI_DATA) Setting */
/* FUJITSU: 2012-01-23 del start */
//	gpio_tlmm_config(GPIO_CFG(38, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,	GPIO_CFG_16MA), GPIO_CFG_ENABLE);
/* FUJITSU: 2012-01-23 del end */

#if 0 /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del start */
	/* M6MO */
	/* GPIO40(CS1) High Setting */
	gpio_tlmm_config(GPIO_CFG(40, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(40);
	DBG_PRINTK("%s GPIO40 [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(40, 1);
	DBG_PRINTK("%s GPIO40 Set High\n", __func__);
	rc = gpio_get_value(40);
	DBG_PRINTK("%s GPIO40 [after] gpio_get_value = %d\n", __func__,rc);
#endif /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del end */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add start */
	/* M7MO */
	/* GPIO2(CS) High Setting */
	gpio_tlmm_config(GPIO_CFG(2, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(2);
	DBG_PRINTK("%s CS GPIO2 [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(2, 1);
	DBG_PRINTK("%s CS GPIO2 Set High\n", __func__);
	rc = gpio_get_value(25);
	DBG_PRINTK("%s CS GPIO2 [after] gpio_get_value = %d\n", __func__,rc);
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add end */

	msleep(5);
	DBG_PRINTK("%s 5ms wait\n", __func__);

DBG_PRINTK("%s() SUCCESS\n", __func__);
	return 0;

}

/**
 *	tmc112_release
 *
 *	disable ISP<br>
 *
 *	@param	inode	[in]	inode
 *	@param	filp	[in]	filp
 *	@return	error code<br>
 *			0	success<br>
 *			<0	fail
 */
static int tmc112_release(struct inode *inode, struct file *filp)
{
	int rc = 0;

	DBG_PRINTK("%s\n", __func__);

	/* parameter check */
	if (filp == NULL) {
		return -EBADF;
	}

	/* wait 5ms */
	msleep(5);
	DBG_PRINTK("%s 5ms wait\n", __func__);

#if 0 /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del start */
	/* M6MO */
	/* GPIO40(CS1) Low Setting */
	gpio_tlmm_config(GPIO_CFG(40, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(40);
	DBG_PRINTK("%s GPIO40 [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(40, 0);
	DBG_PRINTK("%s GPIO40 Set Low\n", __func__);
	rc = gpio_get_value(40);
	DBG_PRINTK("%s GPIO40 [after] gpio_get_value = %d\n", __func__,rc);
#endif /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del end */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add start */
	/* M7MO */
	/* GPIO2(CS) Low Setting */
	gpio_tlmm_config(GPIO_CFG(2, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(2);
	DBG_PRINTK("%s CS GPIO2 [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(2, 0);
	DBG_PRINTK("%s CS GPIO2 Set Low\n", __func__);
	rc = gpio_get_value(2);
	DBG_PRINTK("%s CS GPIO2 [after] gpio_get_value = %d\n", __func__,rc);
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add end */
/* FUJITSU:2012-09-19 add start */
	spi_env_ctrl(0, 1);
/* FUJITSU:2012-09-19 add end */
	/* GPIO41(SPI_CLK) Low Setting */
#if 0
	gpio_tlmm_config(GPIO_CFG(41, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(41);
	DBG_PRINTK("%s GPIO41 [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(41, 0);
	DBG_PRINTK("%s GPIO41 Set Low\n", __func__);
	rc = gpio_get_value(41);
	DBG_PRINTK("%s GPIO41 [after] gpio_get_value = %d\n", __func__,rc);
#else
/* FUJITSU: 2012-01-23 add start */
/* FUJITSU: 2013-02-07 CAMFWLIB chng start */
#if 0
| /* FUJITSU:2012-09-19 add start */
| 	spi_softcs_mutex(0);
| /* FUJITSU:2012-09-19 add end */
#endif /* 0 */
	fjdev_spi_bus4_softcs_mutex( 0 );
/* FUJITSU: 2013-02-07 CAMFWLIB chng end */
	tmc112->bits_per_word = 8;
	tmc112->mode = SPI_MODE_1;
	rc = spi_setup(tmc112);
	if (rc < 0)
		return rc;
	DBG_PRINTK("%s spi_setup() ret[%d]\n", __func__,rc);
/* FUJITSU: 2012-01-23 add end */
#endif

	/* GPIO38(SPI_MOSI_DATA) Setting */
/* FUJITSU: 2012-01-23 del start */
//	gpio_tlmm_config(GPIO_CFG(38, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//	gpio_set_value(38, 0);
//	DBG_PRINTK("%s GPIO38 Set Low\n", __func__);
//	rc = gpio_get_value(38);
//	DBG_PRINTK("%s GPIO38 Restore default value = %d\n", __func__,rc);
/* FUJITSU: 2012-01-23 del end */

/* FUJITSU: 2012-09-25 MC Camera M6MO -> M7MO add start */
	/* GPIO13(SPI_CLK) gpio change */
	DBG_PRINTK("%s GPIO13(SPI_CLK) FUNC_SEL 0 change\n", __func__);
	gpio_tlmm_config(GPIO_CFG(13, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(13);
	DBG_PRINTK("%s GPIO13(SPI_CLK) [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(13, 0);
	DBG_PRINTK("%s GPIO13(SPI_CLK) Set Low\n", __func__);
	rc = gpio_get_value(13);
	DBG_PRINTK("%s GPIO13(SPI_CLK) [after] gpio_get_value = %d\n", __func__,rc);

	/* GPIO10(SPI_DIN) alternate change */
	DBG_PRINTK("%s GPIO10(SPI_DIN) FUNC_SEL 0 change\n", __func__);
	gpio_tlmm_config(GPIO_CFG(10, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_get_value(10);
	DBG_PRINTK("%s GPIO10(SPI_DIN) [before] gpio_get_value = %d\n", __func__,rc);
	gpio_set_value(10, 0);
	DBG_PRINTK("%s GPIO10(SPI_DIN) Set Low\n", __func__);
	rc = gpio_get_value(10);
	DBG_PRINTK("%s GPIO10(SPI_DIN) [after] gpio_get_value = %d\n", __func__,rc);
/* FUJITSU: 2012-09-25 MC Camera M6MO -> M7MO add end */

	module_put(THIS_MODULE);

	return 0;
}

static long tmc112_ioctl(
	struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	int ret = 0;

	DBG_PRINTK("%s\n", __func__);

	return  ret;
}

/* tmc112 driver FOPS */
static struct file_operations tmc112_fops = {
	.read			= tmc112_read,				/* read Entry    */
	.write			= tmc112_write,				/* write Entry   */
	.unlocked_ioctl	= tmc112_ioctl,				/* ioctl Entry   */
	.open			= tmc112_open,				/* open Entry    */
	.release		= tmc112_release,			/* release Entry */
};

/* driver definition */
static struct miscdevice tmc112_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,			/* auto        */
	.name = "tmc112",						/* Driver name */
	.fops = &tmc112_fops,					/* FOPS        */
};


#if 0 /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del start */
/* 2012-01-16 CAMERA POWER ON [add] start */
static int tmc112_pm8921_gpio_control_(int gpio, int onoff)
{
	int ret;
	struct pm_gpio param = {
		.direction	= PM_GPIO_DIR_OUT,
		.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
		.output_value	= onoff,
		.pull		= PM_GPIO_PULL_NO,
		.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength	= PM_GPIO_STRENGTH_MED,
		.function	= PM_GPIO_FUNC_NORMAL,
	};

	DBG_PRINTK("tmc112_pm8921_gpio_control_(%d, %d)\n", gpio, onoff);
	ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(gpio), &param);
	if (ret) {
		DBG_PRINTK("failed pm8xxx_gpio_config(gpio=%d, value=%d) ret=%d\n", PM8921_GPIO_PM_TO_SYS(gpio), onoff, ret);
	}
	return ret;
}
/* 2012-01-16 CAMERA POWER ON [add] end */
#endif /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del end */

#if 0 /* FUJITSU: 2012-11-19 MC Camera M6MO -> M7MO del start */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add start */
static void tmc112_spi_power_control(int onoff)
{
	DBG_PRINTK("%s\n", __func__);

	if (onoff)
	{
		/* M7MO Power On */
		/* +1.2V_M7MO_IST */
		gpio_set_value_cansleep(278, 1);
		msleep(1);
		/* +1.8V_M7MO_EST */
		gpio_set_value_cansleep(292, 1);
		msleep(1);
	}
	else
	{
		/* M7MO Power Off */
		/* +1.8V_M7MO_EST */
		gpio_set_value_cansleep(292, 0);
		msleep(1);
		/* +1.2V_M7MO_IST */
		gpio_set_value_cansleep(278, 0);
		msleep(1);
	}

	return;
}
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add end */
#endif /* FUJITSU: 2012-11-19 MC Camera M6MO -> M7MO del end */


/**
 *	tmc112_halt
 *
 *	halt the driver
 *
 *	@return	0	success
 */
int tmc112_halt(void)
{

	DBG_PRINTK("%s\n", __func__);

	return 0;
}


/*
 *	tmc112_resume
 *
 *	resume from suspend<br>
 *
 *	@param	spi	[in]	spi_device*
 *	@return	0	success
 */
static int tmc112_resume(struct spi_device *spi)
{
	DBG_PRINTK("%s\n", __func__);

	dev_dbg(&spi->dev, "%s()\n", __func__);

#if 0 /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del start */
/* 2012-01-16 CAMERA POWER ON [add] start */
	{
		/* +1.2V_M6MO_IST */
		tmc112_pm8921_gpio_control_(10, 1);
		msleep(20);
		/* +1.8V_M6M0_EST */
		tmc112_pm8921_gpio_control_(14, 1);
		msleep(20);
	}
/* 2012-01-16 CAMERA POWER ON [add] end */
#endif /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del end */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add start */
	/* M7MO Power on */
//	tmc112_spi_power_control(1); /* FUJITSU: 2012-11-19 MC Camera M6MO -> M7MO del */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add end */

	return 0;
}

/*
 *	tmc112_suspend
 *
 *	goto suspend<br>
 *
 *	@param	spi	[in]		spi_device*
 *	@param	mesg	[in]	pm_message_t
 *	@return	0	success
 */
static int tmc112_suspend(struct spi_device *spi, pm_message_t mesg)
{
	int ret = 0;
	DBG_PRINTK("%s\n", __func__);

	dev_dbg(&spi->dev, "%s(), msg=%d\n", __func__, mesg.event);

#if 0 /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del start */
/* 2012-01-16 CAMERA POWER OFF [add] start */
	{
		/* +1.8V_M6M0_EST */
		tmc112_pm8921_gpio_control_(14, 0);
		msleep(20);
		/* +1.2V_M6MO_IST */
		tmc112_pm8921_gpio_control_(10, 0);
		msleep(20);
	}
/* 2012-01-16 CAMERA POWER OFF [add] end */
#endif /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del end */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add start */
	/* M7MO Power off */
//	tmc112_spi_power_control(0); /* FUJITSU: 2012-11-19 MC Camera M6MO -> M7MO del */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add end */

	return ret;
}


/**
 *	tmc112_spi_probe
 *
 *	Probe<br>
 *
 *	@param	spi	[in]	spi_device
 *	@return	0	success<br>
 *			<0	fail
 */
static int __devinit tmc112_spi_probe(struct spi_device *spi)
{
	int ret;
	struct tmc112_pdata *pdata = NULL;

	DBG_PRINTK("tmc112_spi_probe\n");

	pdata = (struct tmc112_pdata *)kzalloc(sizeof(struct tmc112_pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&spi->dev, "Please provide valid platform data\n");
		return -EINVAL;
	}

	pdata->pin_reset = 0;
	pdata->pin_request = 0;
	pdata->pin_ready = 0;
	pdata->pin_xirq = 0;
	pdata->gpio_request_irq = 0;
	pdata->vreg_core = NULL;
	pdata->vreg_io = NULL;
	pdata->clk_b = NULL;

	pdata->request_raised = 0;
	pdata->ready_raised = 0;
	
	pdata->suspend = 0;
	pdata->suspend_withrun = 0;
	pdata->resume = 0;

	pdata->running = 0;
	pdata->cancel_read = 0;


	tmc112_miscdev.parent = &spi->dev;
	ret = misc_register(&tmc112_miscdev);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to register miscdev: %d\n", ret);
		kfree(pdata);
		return ret;
	}


	tmc112 = spi;
	spi->dev.platform_data = pdata;
	spi->chip_select = 1;
	return 0;

}

/**
 *	tmc112_spi_remove
 *
 *	remove<br>
 *	deregister driver it was registered in Probe
 *
 *	@param	spi	[in]	spi_device*
 *	@return	0	success<br>
 */
static int __devexit tmc112_spi_remove(struct spi_device *spi)
{
	struct tmc112_pdata *pdata = dev_get_platdata(&spi->dev);

	dev_err(&spi->dev, "%s\n", __func__);

	misc_deregister(&tmc112_miscdev);
	tmc112 = NULL;
	if (pdata) {
		kfree(pdata);
	}

	return 0;
}


/* tmc112 driver as spi child driver */
static struct spi_driver tmc112_spi_driver = {
	.driver = {
		.name	= "tmc112_firm_transfer",				///< 
		.bus 	= &spi_bus_type,						///< 
		.owner	= THIS_MODULE,							///< owner
	},
	.probe		= tmc112_spi_probe,						///< Probe Entry
	.remove		= __devexit_p(tmc112_spi_remove),		///< Remove Entry
	.suspend	= tmc112_suspend,						///< Suspend Entry
	.resume		= tmc112_resume,						///< Resume Entry
};


/**
 *	tmc112_init
 *
 *	Initialize driver<br>
 *	register with spi_register_driver
 *
 *	@return	0	success<br>
 *			<0	fail
 */
static int __init tmc112_init(void)
{
	int ret;
	DBG_PRINTK("tmc112_init\n");

	ret = spi_register_driver(&tmc112_spi_driver);
	if (ret) {
		DBG_PRINTK("tmc112_init ret=%d\n", ret);
	}
#if 0 /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del start */
/* 2012-01-16 CAMERA POWER ON [add] start */
	{
		/* +1.2V_M6MO_IST */
		tmc112_pm8921_gpio_control_(10, 1);
		msleep(20);
		/* +1.8V_M6M0_EST */
		tmc112_pm8921_gpio_control_(14, 1);
		msleep(20);
	}
/* 2012-01-16 CAMERA POWER ON [add] end */
#endif /* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO del end */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add start */
	/* M7MO Power on */
//	tmc112_spi_power_control(1); /* FUJITSU: 2012-11-19 MC Camera M6MO -> M7MO del */
/* FUJITSU: 2012-07-18 MC Camera M6MO -> M7MO add end */

	return ret;
}
module_init(tmc112_init);

/**
 *	tmc112_exit
 *
 *	finalize driver<br>
 *	deregister spi_register_driver
 */
static void __exit tmc112_exit(void)
{
	DBG_PRINTK("tmc112_exit\n");
	spi_unregister_driver(&tmc112_spi_driver);
}
module_exit(tmc112_exit);

MODULE_DESCRIPTION("ASoC tmc112 driver");
MODULE_AUTHOR("Fujitsu");
MODULE_LICENSE("GPL");
