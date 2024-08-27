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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/byteorder/generic.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/wakelock.h>

#include <asm/uaccess.h>
#include <asm/io.h>

/* #include <gpio-names.h> */
/* #include <board-tegra3.h> */

#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <mach/vreg.h>
#include "../../../arch/arm/mach-msm/board-8064.h"

#include <asm/system.h>

/* governor s */
#include <linux/poll.h>
/* governor e */


#define DEVCOUNT			2
#define FTDTV_DRV_NAME		"ftdtv"
#define DRV_VERSION			"0.1"

#define TYPE_POW_1SEG 0x01
struct FTDTVPowerParam{
	int	band;
	int	type;
	int	pid;
};

#define FTDTV_IOC_MAGIC		'f'

#define FTDTV_USE_TERMINAL_CHECK			0

#define FTDTV_IOCTL_POWER_ON				_IOW (FTDTV_IOC_MAGIC, 0, struct FTDTVPowerParam)
#define FTDTV_IOCTL_POWER_OFF				_IOW (FTDTV_IOC_MAGIC, 1, struct FTDTVPowerParam)
#define FTDTV_IOCTL_IRQ_TMCC	 			_IO  (FTDTV_IOC_MAGIC, 3)
#define FTDTV_IOCTL_IRQ_TMCC_CANCEL			_IO  (FTDTV_IOC_MAGIC, 4)
#define FTDTV_IOCTL_IRQ_SEARCH_CH			_IOW (FTDTV_IOC_MAGIC, 5, int)
#define FTDTV_IOCTL_IRQ_SEARCH_CH_CANCEL	_IO  (FTDTV_IOC_MAGIC, 6)


#define FTDTV_DEBUG			0
#if FTDTV_DEBUG
#define	LOG_DBG(...)		printk(KERN_ERR __VA_ARGS__)
#define	LOG_INFO(...)		printk(KERN_ERR __VA_ARGS__)
#define LOG_WARN(...)		printk(KERN_ERR __VA_ARGS__)
#define LOG_ERR(...)		printk(KERN_ERR __VA_ARGS__)
#else
#define	LOG_DBG(...)		printk(KERN_DEBUG __VA_ARGS__)
#define	LOG_INFO(...)		printk(KERN_INFO __VA_ARGS__)
#define LOG_WARN(...)		printk(KERN_WARNING __VA_ARGS__)
#define LOG_ERR(...)		printk(KERN_ERR __VA_ARGS__)
#endif

/*--- Static variables ---------------------------------------------------------*/

static struct cdev cdev_p;
static unsigned int ftdtv_major = 0;
static unsigned int ftdtv_minor = 0;

static struct class *ftdtv_class;
dev_t  ftdtv_dev;

/* global irq parameter */
//static int ftdtv_xirq_gpio = TEGRA_GPIO_PH1;
#define FTDTV_XIRQ_GPIO		59
//static int ftdtv_xirq_gpio = 59;
static int irq_no = 0;

/* global gpio no parameter */
/* static int ftdtv_pon_gpio = TEGRA_GPIO_PH6; */
/* static int ftdtv_nreset_gpio = TEGRA_GPIO_PA3; */
/* static int ftdtv_ant_pon_gpio = TEGRA_GPIO_PH7; */
//static int ftdtv_pon_gpio = TEGRA_GPIO_PH6;
#define PM8921_DTV_XRESET_13				13
#define FTDTV_XRESET_GPIO		PM8921_GPIO_PM_TO_SYS(PM8921_DTV_XRESET_13)
//static int ftdtv_xreset_gpio = FTDTV_XRESET_GPIO;
/* static int ftdtv_ant_pon_gpio = TEGRA_GPIO_PH7; */
/* PMIC GPIO 13 */
struct pm_gpio pmic_xreset_13 = {
 .direction      = PM_GPIO_DIR_OUT,
 .pull           = PM_GPIO_PULL_NO,
 .vin_sel        = PM_GPIO_VIN_S4,
 .function       = PM_GPIO_FUNC_NORMAL,
 .inv_int_pol    = 0,
 .out_strength   = PM_GPIO_STRENGTH_MED,
 .output_value   = 0,
};
#define VREG_TABLE  1
static struct vreg_info {
    char    *name;
    unsigned int    lvl;
    //    struct vreg *vreg;
    struct regulator *reg;
} vreg_info[VREG_TABLE] = {
    {"8921_l12",  1200000, NULL},		//L12
};
enum fjavdtv_irq_state {
	FJAVDTV_IRQ_STATE_NONE = 0,		/* irq not detect */
	FJAVDTV_IRQ_STATE_DETECT,		/* irq detect */
	FJAVDTV_IRQ_STATE_CANCEL,		/* irq cancel from libdtv_tuner */

	FJAVDTV_IRQ_STATE_END			/* terminater */
};

wait_queue_head_t		tmcc_irq;
enum fjavdtv_irq_state	interrupt_flg = FJAVDTV_IRQ_STATE_NONE;
wait_queue_head_t		search_ch_irq;
enum fjavdtv_irq_state	interrupt_flg_search_ch = FJAVDTV_IRQ_STATE_NONE;
int						flgpower = 0;	/* 0:PowerOff, 1:PowerOn */
static int				cntdev = 0;

struct wake_lock ftdtv_wake_lock;

static irqreturn_t ftdtv_irq_tmcc_handler(int irq, void *data)
{
	int retval = IRQ_NONE;
	struct cdev* pdev = (struct cdev*)data;
	
	LOG_DBG("[ftdtv]ftdtv_irq_tmcc_handler -S\n");
	
	if(pdev != &cdev_p){
		return retval;
	}
	
	if(flgpower){
		interrupt_flg = FJAVDTV_IRQ_STATE_DETECT;
		wake_up_interruptible(&tmcc_irq);
		retval = IRQ_HANDLED;
		disable_irq_nosync(irq_no);
	}
	
	LOG_DBG("[ftdtv]ftdtv_irq_tmcc_handler -E\n");
	return retval;
}

static irqreturn_t ftdtv_irq_search_ch_handler(int irq, void *data)
{
	int retval = IRQ_NONE;
	struct cdev* pdev = (struct cdev*)data;
	
	LOG_DBG("[ftdtv]ftdtv_irq_search_ch_handler -S\n");
	
	if(pdev != &cdev_p){
		return retval;
	}
	
	if(flgpower){
		interrupt_flg_search_ch = FJAVDTV_IRQ_STATE_DETECT;
		wake_up_interruptible(&search_ch_irq);
		retval = IRQ_HANDLED;
		disable_irq_nosync(irq_no);
	}
	
	LOG_DBG("[ftdtv]ftdtv_irq_search_ch_handler -E\n");
	return retval;
}

static void ftdtv_check_terminal(void)
{
# if 0
	int board_id = 0;

	LOG_INFO("[ftdtv]%s -S\n", __func__);

	LOG_INFO("[ftdtv]%s -E board_id:[%d] pon_gpio:[%d]"
			 "nreset_gpio:[%d] xirq_gpio_no[%d]\n",
			 __func__, board_id, ftdtv_pon_gpio, ftdtv_nreset_gpio,
			 ftdtv_xirq_gpio);
#endif
}

static int ftdtv_poweron(void)
{
	int ret = 0;
	LOG_INFO("[ftdtv]%s -S\n", __func__);

	wake_lock(&ftdtv_wake_lock);
#if 1
	ret = regulator_set_optimum_mode(vreg_info[0].reg, 100000);
	if(ret < 0){
		printk(KERN_ERR "set_optimum_mode l21 failed, ret=%d\n", ret);
		ret =  -EINVAL;
	}
	/* L17 1.8V */
	if (regulator_enable(vreg_info[0].reg)) {
		printk("%s: * vreg %s enable failed !\n", __func__, vreg_info[0].name);
		ret = -EINVAL;
	}
	mdelay(40);
	/* GPIO_DTV_RST:HIGH */
	gpio_set_value_cansleep(FTDTV_XRESET_GPIO, 1);
	mdelay(1);
#else
	mdelay(1);
	ret = gpio_direction_output(ftdtv_pon_gpio, 1);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s ftdtv_pon_gpio error\n", __func__);
	}
	ret = gpio_direction_output(ftdtv_ant_pon_gpio, 1);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s ftdtv_ant_pon_gpio error\n", __func__);
	}

	mdelay(50);

	ret = gpio_direction_output(ftdtv_nreset_gpio, 1);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s ftdtv_nreset_gpio error\n", __func__);
	}

	mdelay(10);

	gpio_direction_input(ftdtv_xirq_gpio);
#endif

	LOG_INFO("[ftdtv]%s -E\n", __func__);
	return 0;
}

static int ftdtv_poweroff(void)
{
	int ret = 0;
	LOG_INFO("[ftdtv]ftdtv_poweroff -S\n");

#if 1
	/* DTV XRESET */
	gpio_set_value_cansleep(FTDTV_XRESET_GPIO, 0);

	/* L17 1.8V */
	if (regulator_disable(vreg_info[0].reg)) {
		printk("%s: * vreg %s disable failed !\n", __func__, vreg_info[0].name);
		ret = -EINVAL;
	}
	ret = regulator_set_optimum_mode(vreg_info[0].reg, 0);
	if(ret < 0){
		printk(KERN_ERR "set_optimum_mode l21 failed, ret=%d\n", ret);
		ret =  -EINVAL;
	}
#else
	ret = gpio_direction_output(ftdtv_nreset_gpio, 0);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s ftdtv_nreset_gpio error\n", __func__);
	}
	mdelay(1);

	ret = gpio_direction_output(ftdtv_pon_gpio, 0);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s ftdtv_pon_gpio error\n", __func__);
	}
	ret = gpio_direction_output(ftdtv_ant_pon_gpio, 0);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s ftdtv_ant_pon_gpio error\n", __func__);
	}
#endif
	wake_unlock(&ftdtv_wake_lock);

	LOG_INFO("[ftdtv]ftdtv_poweroff -E\n");
	return 0;
}

static int ftdtv_irq_tmcc(void)
{
	int ret;
	LOG_DBG("[ftdtv]ftdtv_irq_tmcc -S  irq[%d]\n", irq_no);

	interrupt_flg = FJAVDTV_IRQ_STATE_NONE;

	enable_irq(irq_no);
	ret = request_irq(irq_no, ftdtv_irq_tmcc_handler,
		(IRQF_ONESHOT | IRQF_TRIGGER_HIGH),
		FTDTV_DRV_NAME, &cdev_p);
	if(ret){
		LOG_ERR("[ftdtv]Interrupt handler registration failure ret:[%d]\n", ret);
		interrupt_flg = FJAVDTV_IRQ_STATE_NONE;
		return ret;
	}

	/* Interrupt pending */
	ret = wait_event_interruptible(tmcc_irq,
		(interrupt_flg != FJAVDTV_IRQ_STATE_NONE));
	if(ret == -ERESTARTSYS){
		LOG_ERR("[ftdtv]wait_event_interruptible failure ret:[%d]\n", ret);
		free_irq(irq_no, &cdev_p);
		interrupt_flg = FJAVDTV_IRQ_STATE_NONE;
		return ret;
	}

	free_irq(irq_no, &cdev_p);
	interrupt_flg = FJAVDTV_IRQ_STATE_NONE;
	
	LOG_DBG("[ftdtv]ftdtv_irq_tmcc -E\n");
	return 0;
}

static int ftdtv_irq_tmcc_cancel(void)
{
	LOG_DBG("[ftdtv]ftdtv_irq_tmcc_cancel -S\n");

	interrupt_flg = FJAVDTV_IRQ_STATE_CANCEL;
	wake_up_interruptible(&tmcc_irq);
	disable_irq_nosync(irq_no);

	LOG_DBG("[ftdtv]ftdtv_irq_tmcc_cancel -E\n");
	return 0;
}

static int ftdtv_irq_search_ch(unsigned int timeout)
{
	int ret;
	LOG_DBG("[ftdtv]ftdtv_irq_search_ch -S  irq[%d]\n", irq_no);

	/* waitqueue initialization */
	init_waitqueue_head(&search_ch_irq);
	interrupt_flg_search_ch = FJAVDTV_IRQ_STATE_NONE;

	enable_irq(irq_no);
	ret = request_irq(irq_no, ftdtv_irq_search_ch_handler,
		(IRQF_ONESHOT | IRQF_TRIGGER_HIGH),
		FTDTV_DRV_NAME, &cdev_p);
	if(ret){
		LOG_ERR("[ftdtv]Interrupt handler registration failure ret:[%d]\n", ret);
		interrupt_flg_search_ch = FJAVDTV_IRQ_STATE_NONE;
		return ret;
	}

	/* Interrupt pending */
	ret = wait_event_interruptible_timeout(search_ch_irq,
		(interrupt_flg_search_ch != FJAVDTV_IRQ_STATE_NONE), msecs_to_jiffies(timeout));
	if(ret == -ERESTARTSYS){
		LOG_ERR("[ftdtv]wait_event_interruptible_timeout failure ret:[%d]\n", ret);
		free_irq(irq_no, &cdev_p);
		interrupt_flg_search_ch = FJAVDTV_IRQ_STATE_NONE;
		return ret;
	}
	else if(ret == 0){
		LOG_INFO("[ftdtv]wait_event_interruptible_timeout timeout ret:[%d]\n", ret);
		free_irq(irq_no, &cdev_p);
		interrupt_flg_search_ch = FJAVDTV_IRQ_STATE_NONE;
		ret = -ETIMEDOUT;
		return ret;
	}
	
	free_irq(irq_no, &cdev_p);
	interrupt_flg_search_ch = FJAVDTV_IRQ_STATE_NONE;
	
	LOG_DBG("[ftdtv]ftdtv_irq_search_ch -E\n");
	return 0;
}

static int ftdtv_irq_search_ch_cancel(void)
{
	LOG_DBG("[ftdtv]ftdtv_irq_search_ch_cancel -S\n");

	interrupt_flg_search_ch = FJAVDTV_IRQ_STATE_CANCEL;
	wake_up_interruptible(&search_ch_irq);
	disable_irq_nosync(irq_no);

	LOG_DBG("[ftdtv]ftdtv_irq_search_ch_cancel -E\n");
	return 0;
}

static int ftdtv_open(struct inode* inode, struct file* file_p)
{
	int ret = 0;
	LOG_DBG("[ftdtv]ftdtv_open -S\n");

	if ((file_p->f_flags & 0x0000000f) != O_RDONLY) {
		cntdev++;
	}

	LOG_DBG("[ftdtv]ftdtv_open -E cnt=%d\n", cntdev);
	return ret;
}

static int ftdtv_release(struct inode* inode, struct file* file_p)
{
	int ret = 0;
	LOG_DBG("[ftdtv]ftdtv_release -S\n");

	if ((file_p->f_flags & 0x0000000f) != O_RDONLY) {
		cntdev--;
	}
	if (cntdev == 0) {
		if (flgpower) {
			flgpower = 0;
			(void)ftdtv_poweroff();
		}
	}

	LOG_DBG("[ftdtv]ftdtv_release -E cnt=%d\n", cntdev);
	return ret;
}

static long ftdtv_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int rc = 0;
	void __user *argp = (void __user *)arg;
	int timeout;
	LOG_DBG("[ftdtv]ftdtv_ioctl -S\n");
	
	LOG_DBG("[ftdtv][cmd:0x%x]\n", cmd);
	switch(cmd){
	case FTDTV_IOCTL_POWER_ON:
		do {
			struct FTDTVPowerParam powparam;
			rc = copy_from_user(&powparam, (int __user *)arg, sizeof(struct FTDTVPowerParam));
			if (rc) {
				LOG_ERR("[ftdtv]copy_from_user failed. [ret:%d]\n", rc);
				ret = -EFAULT;
				break;
			}
			if (powparam.type != TYPE_POW_1SEG) {
				LOG_ERR("[ftdtv]Invalid parameter, type=0x%x\n", powparam.type);
				ret = -EINVAL;
				break;
			}
			if (flgpower & TYPE_POW_1SEG) {
				ret = -EBUSY;
			} else {
				ret = ftdtv_poweron();
				if (ret == 0) {
					flgpower |= powparam.type;
				} else {
					LOG_ERR("[ftdtv]ftdtv_poweron failed. [ret:%d]\n", rc);
					(void)ftdtv_poweroff();
				}
			}
		} while (0);
		break;
	case FTDTV_IOCTL_POWER_OFF:
		do {
			struct FTDTVPowerParam powparam;
			int tmppower = flgpower;

			rc = copy_from_user(&powparam, (int __user *)arg, sizeof(struct FTDTVPowerParam));
			if (rc != 0) {
				LOG_ERR("[ftdtv]copy_from_user failed. [ret:%d]\n", rc);
				ret = -EFAULT;
				break;
			}
			if (powparam.type != TYPE_POW_1SEG) {
				LOG_ERR("[ftdtv]Invalid parameter, type=0x%x\n", powparam.type);
				ret = -EINVAL;
				break;
			}
			flgpower &= (~TYPE_POW_1SEG);
			if (tmppower != flgpower) {
				(void)ftdtv_poweroff();
			}
		} while (0);
		break;
	case FTDTV_IOCTL_IRQ_TMCC:
		ftdtv_irq_tmcc();
		break;
	case FTDTV_IOCTL_IRQ_TMCC_CANCEL:
		ftdtv_irq_tmcc_cancel();
		break;
	case FTDTV_IOCTL_IRQ_SEARCH_CH:
		ret = copy_from_user(&timeout, argp, sizeof(int));
		if(ret){
			LOG_ERR("[ftdtv]copy_from_user failed. [ret:%d]\n", ret);
			return -EFAULT;
		}
		ret = ftdtv_irq_search_ch(timeout);
		break;
	case FTDTV_IOCTL_IRQ_SEARCH_CH_CANCEL:
		ftdtv_irq_search_ch_cancel();
		break;
	default:
		LOG_ERR("[ftdtv]cmd code default connect.\n");
		ret = -ENOTTY;
		break;
	}
	LOG_DBG("[ftdtv]ftdtv_ioctl -E [ret:%d]\n", ret);
	return ret;
}

static struct file_operations ftdtv_fops = {
  .owner = THIS_MODULE,
  .open = ftdtv_open,
  .release = ftdtv_release,
  .unlocked_ioctl = ftdtv_ioctl,
};

static void __exit ftdtv_exit(void)
{
	LOG_DBG("[ftdtv]ftdtv_exit -S\n");

	wake_lock_destroy(&ftdtv_wake_lock);
#if 1
	gpio_free(FTDTV_XIRQ_GPIO);
#else
	gpio_free(ftdtv_pon_gpio);
	gpio_free(ftdtv_nreset_gpio);
	gpio_free(ftdtv_xirq_gpio);
	gpio_free(ftdtv_ant_pon_gpio);
#endif
	interrupt_flg = 1;
	wake_up_interruptible(&tmcc_irq);

	/* System call registration cancellation */
	device_destroy(ftdtv_class, 0);
	class_destroy(ftdtv_class);
	cdev_del(&cdev_p);

	/* Device number release */
	unregister_chrdev_region(ftdtv_dev, 1);

	LOG_DBG("[ftdtv]ftdtv_exit -E\n");
}

static int __init ftdtv_init(void)
{
	int ret = 0;
	int i;
	dev_t dev = MKDEV(ftdtv_major, 0);

	LOG_DBG("[ftdtv]ftdtv_init -S\n");

	if(ftdtv_major){
		ret = register_chrdev_region(dev, DEVCOUNT, FTDTV_DRV_NAME);
	}
	else {
		/* Device number acquisition */
		ret = alloc_chrdev_region(&dev, 0 ,DEVCOUNT, FTDTV_DRV_NAME);
		LOG_DBG("[ftdtv]module %d %d\n", MAJOR(dev), MINOR(dev));
		ftdtv_major = MAJOR(dev);
	}

	if(ret < 0){
		printk(KERN_ERR "ftdtv error fail to get major %d\n", ftdtv_major);
		printk(KERN_ERR "ftdtv_drv:%s -E\n", __func__);
		return ret;
	}

	/* System call registration */
	cdev_init(&cdev_p, &ftdtv_fops);
	cdev_p.owner = THIS_MODULE;
	cdev_p.ops = &ftdtv_fops;

	ret = cdev_add(&cdev_p, MKDEV(ftdtv_major, ftdtv_minor), 1);
	if(ret){
		LOG_ERR("[ftdtv]System call not registration.\n");
		cdev_del(&cdev_p);
		return -ENOMEM;
	}

	ftdtv_class = class_create(THIS_MODULE, FTDTV_DRV_NAME);
	if(IS_ERR(ftdtv_class)){
		LOG_ERR("[ftdtv]class_create error.\n");
		cdev_del(&cdev_p);
		return -ENOMEM;
	}
	
	ftdtv_dev = MKDEV(ftdtv_major, ftdtv_minor);
	device_create(ftdtv_class, NULL, ftdtv_dev, NULL, FTDTV_DRV_NAME);

	/* waitqueue initialization */
	init_waitqueue_head(&tmcc_irq);

	/* reset tuner power state */
	flgpower = 0;

	/* check terminal version */
	ftdtv_check_terminal();

	wake_lock_init(&ftdtv_wake_lock, WAKE_LOCK_SUSPEND, "ftdtv");
#if 1
	/* XIRQ */
	ret = gpio_request(FTDTV_XIRQ_GPIO, "ftdtv_xirq");
	if (ret) {
		printk(KERN_ERR "%s: unable to request ftdtv_xirq gpio [59]\n", __func__);
		printk(KERN_ERR "ftdtv_drv:%s -E\n", __func__);
		return ret;
	}
#if 0
	ret = gpio_tlmm_config(GPIO_CFG(FTDTV_XIRQ_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA));
	
#endif
	ret = gpio_direction_input(FTDTV_XIRQ_GPIO);
	if (ret) {
		printk(KERN_ERR "%s: unable to set_direction for ftdtv_xirq gpio [59]\n", __func__);
		printk(KERN_ERR "ftdtv_drv:%s -E\n", __func__);
	}
	irq_no = gpio_to_irq(FTDTV_XIRQ_GPIO);

	/* XRESET */
	ret = pm8xxx_gpio_config(FTDTV_XRESET_GPIO, &pmic_xreset_13);
	if (ret) {
		printk(KERN_ERR "%s: unable to config for pmic_xreset_13\n", __func__);
		printk(KERN_INFO "ftdtv_drv:%s -E\n", __func__);
		return ret;
	}
	/* PMIC GPIO13 is changed into Low. */
	gpio_set_value_cansleep(FTDTV_XRESET_GPIO, 0);

	for (i=0;i<VREG_TABLE;++i) {
		vreg_info[i].reg = regulator_get(NULL, vreg_info[i].name);
		printk("name = %s Level = %d\n",vreg_info[i].name,vreg_info[i].lvl);
		if (IS_ERR(vreg_info[i].reg)) {
			printk("%s: * vreg_get(%s) failed (%ld) !\n",
			       __func__, vreg_info[i].name, PTR_ERR(vreg_info[i].reg));
			return -1;
			//break;
		}
	}
	for (i=0;i<VREG_TABLE;++i) {
		if (regulator_set_voltage(vreg_info[i].reg, vreg_info[i].lvl, vreg_info[i].lvl)) {
			printk("%s: * %s set level failed !\n", __func__, vreg_info[i].name);
			return -1;
		}
	}
#else
	tegra_gpio_enable(ftdtv_xirq_gpio);
	tegra_gpio_enable(ftdtv_pon_gpio);
	tegra_gpio_enable(ftdtv_nreset_gpio);
	tegra_gpio_enable(ftdtv_ant_pon_gpio);

	/* use gpio request */
	ret = gpio_request(ftdtv_pon_gpio, "ftdtv_pon");
	if(ret){
		LOG_ERR("%s: unable to request ftdtv_pon gpio [%d]\n", __func__, ftdtv_pon_gpio);
		goto error;
	}
	gpio_request(ftdtv_nreset_gpio, "ftdtv_nreset");
	if(ret){
		LOG_ERR("%s: unable to request ftdtv_nreset gpio [%d]\n", __func__, ftdtv_nreset_gpio);
		goto error;
	}
	gpio_request(ftdtv_xirq_gpio, "ftdtv_nirq");
	if(ret){
		LOG_ERR("%s: unable to request ftdtv_nirq gpio [%d]\n", __func__, ftdtv_xirq_gpio);
		goto error;
	}
	gpio_request(ftdtv_ant_pon_gpio, "ftdtv_ant_pon");
	if(ret){
		LOG_ERR("%s: unable to request ftdtv_ant_pon gpio [%d]\n", __func__, ftdtv_ant_pon_gpio);
		goto error;
	}

	ret = gpio_direction_output(ftdtv_pon_gpio, 0);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s unable to set_direction for ftdtv_pon_gpio[%d]\n", __func__, ftdtv_pon_gpio);
		goto error;
	}
	ret = gpio_direction_output(ftdtv_nreset_gpio, 0);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s unable to set_direction for ftdtv_nreset_gpio[%d]\n", __func__, ftdtv_nreset_gpio);
		goto error;
	}
	ret = gpio_direction_output(ftdtv_ant_pon_gpio, 0);
	if(ret != 0){
		LOG_ERR("[ftdtv]%s unable to set_direction for ftdtv_ant_pon_gpio[%d]\n", __func__, ftdtv_ant_pon_gpio);
		goto error;
	}

	/* Interrupt handler registration */
	irq_no = gpio_to_irq(ftdtv_xirq_gpio);
#endif

	LOG_DBG("[ftdtv]ftdtv_init -E\n");
	return 0;
#if 0
 error:
	LOG_DBG("[ftdtv]ftdtv_init -E\n");
	ftdtv_exit();
	return ret;
#endif
}


/*---------------------------------------------------------------------------
    dtv_ant_usb_insert_detect
---------------------------------------------------------------------------*/
void dtv_ant_usb_insert_detect(void)
{
	printk(KERN_INFO "ftdtv_drv:%s -S\n", __func__);
	
//	g_antenna = 1;
	
	printk(KERN_INFO "ftdtv_drv:%s -E\n", __func__);
}
EXPORT_SYMBOL_GPL(dtv_ant_usb_insert_detect);

/*---------------------------------------------------------------------------
    dtv_ant_usb_remove_detect
---------------------------------------------------------------------------*/
void dtv_ant_usb_remove_detect(void)
{
	printk(KERN_INFO "ftdtv_drv:%s -S\n", __func__);
	
//	g_antenna = 0;
	
	printk(KERN_INFO "ftdtv_drv:%s -E\n", __func__);
}
EXPORT_SYMBOL_GPL(dtv_ant_usb_remove_detect);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fujitsu DTV Tuner Driver");
MODULE_AUTHOR("Fujitsu Limited");
MODULE_VERSION(DRV_VERSION);

module_init(ftdtv_init);
module_exit(ftdtv_exit);
