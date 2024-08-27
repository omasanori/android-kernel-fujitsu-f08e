/*
 * Copyright(C) 2012-2013 FUJITSU LIMITED
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

#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/walkmotion.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/delay.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/gpio.h>
#include <linux/cdev.h>
#include <linux/input.h>
#include <linux/kdev_t.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include "walkmotion_hw.h"
#include "walkmotion_data.h"

/* Driver name */
#define DRIVER_NAME "fj-walkmotion"
#define DEV_NAME "fj_wm_ctl"
/* Delay */
#define FJ_WM_DEF_MC_INIT_DELAY 20

/* Initializer */
#define FJ_WM_STS_INITIALIZER        (0x00)
/* Initialized */
#define FJ_WM_STS_MC_INITIALIZED     (0x01)

struct fj_wm_data *fj_wm_data;
struct input_dev *fj_wm_this_data;
struct workqueue_struct *fj_wm_wq;

static int fj_wm_major;
static struct cdev fj_wm_cdev;
static struct class *fj_wm_class;
static int fj_wm_open_cnt;

DECLARE_WORK(fj_wm_work_queue_motion_irq, fj_wm_hw_motion_irq);

/** (SW)fj_wm_sw_init
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_sw_init(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_hw_init();

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (SW)fj_wm_sw_exit
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_sw_exit(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_hw_exit();

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (SW)fj_wm_sw_probe
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_sw_probe(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_hw_probe();

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (SW)fj_wm_sw_remove
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_sw_remove(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_hw_remove();

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (SW)fj_wm_sw_release
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_sw_release(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_hw_release();
    if (likely(ret == 0)) {
        fj_wm_data->state = FJ_WM_STS_INITIALIZER;
    }

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (SW)fj_wm_sw_ioct_initialize
 *
 * @return 0  Success
 *         !0 Fail
 */
static long fj_wm_sw_ioct_initialize(void)
{
    long lRet = 0;
    debug_printk("%s : start\n", __func__);

    wake_lock(&fj_wm_data->wake_lock);

    lRet = fj_wm_hw_hce_reset();
    if (likely(lRet == 0)) {
        fj_wm_data->state = FJ_WM_STS_MC_INITIALIZED;
    }

    wake_unlock(&fj_wm_data->wake_lock);

    debug_printk("%s : ret(%ld) end\n", __func__, lRet);
    return lRet;
}

/** (SW)fj_wm_sw_iocs_request_motion_irq
 *
 * @param value
 * @return 0  Success
 *         !0 Fail
 */
static long fj_wm_sw_iocs_request_motion_irq(unsigned long value)
{
    long lRet = 0;
    debug_printk("%s : start\n", __func__);

    if (likely(fj_wm_data->state == FJ_WM_STS_MC_INITIALIZED)) {
        lRet = fj_wm_hw_hce_request_irq(value);
    } else {
        lRet = -EINVAL;
    }

    debug_printk("%s : ret(%ld) end\n", __func__, lRet);
    return lRet;
}

/** (SW)fj_wm_sw_ioct_cancel_motion_irq
 *
 * @return 0  Success
 *         !0 Fail
 */
static long fj_wm_sw_ioct_cancel_motion_irq(void)
{
    long lRet = 0;
    debug_printk("%s : start\n", __func__);

    if (likely(fj_wm_data->state == FJ_WM_STS_MC_INITIALIZED)) {
        lRet = fj_wm_hw_hce_cancel_irq();
    } else {
        lRet = -EINVAL;
    }

    debug_printk("%s : ret(%ld) end\n", __func__, lRet);
    return lRet;
}

/** (SW)fj_wm_sw_iocs_wakeup_control
 *
 * @param value
 * @return 0  Success
 *         !0 Fail
 */
static long fj_wm_sw_iocs_wakeup_control(unsigned long value)
{
    long lRet = 0;
    debug_printk("%s : start\n", __func__);

    fj_wm_data->wakeup_flag = value;
    lRet = fj_wm_hw_hce_wakeup_set(value);

    debug_printk("%s : ret(%ld) end\n", __func__, lRet);
    return lRet;
}

/** (IF)fj_wm_if_init
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_if_init(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_sw_init();

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (IF)fj_wm_if_exit
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_if_exit(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_sw_exit();

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (IF)fj_wm_if_probe
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_if_probe(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_sw_probe();

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (IF)fj_wm_if_remove
 *
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_if_remove(void)
{
   int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_sw_remove();

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (IF)fj_wm_if_open
 *
 * @param inode : Not use
 * @param file  : Not use
 * @return 0  Success
 *         !0 Fail
 */
static int fj_wm_if_open(struct inode *inode, struct file *file)
{
    debug_printk("%s : start\n", __func__);

    if (unlikely(fj_wm_open_cnt > 0)) {
        printk(KERN_ERR "%s : open multiple error\n", __func__);
        return -EMFILE;
    } else {
        fj_wm_open_cnt++;
    }

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (IF)fj_wm_if_release
 * @param inode : Not use
 * @param file  : Not use
 * @return 0  Success
 */
static int fj_wm_if_release(struct inode *inode, struct file *file)
{
    debug_printk("%s : start\n", __func__);

    if (likely(fj_wm_open_cnt > 0)) {
        fj_wm_open_cnt--;
        fj_wm_sw_release();
    }

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (IF)fj_wm_if_unlocked_ioctl
 *
 * @param file  : File descriptor
 * @param cmd   : Control command
 * @param arg   : Argument
 * @return 0  Success
 *         !0 Fail
 */
static long
fj_wm_if_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long lRet = 0;
    debug_printk("%s : start\n", __func__);

    mutex_lock(&fj_wm_data->io_lock);

    switch (cmd) {
    /** Initialize MC */
    case FJ_WM_IOCT_INITIALIZE:
        lRet = fj_wm_sw_ioct_initialize();
        break;
    /** Request motion IRQ */
    case FJ_WM_IOCS_REQUESTMOTIONIRQ:
        if (likely((arg == FJ_WM_EDGE_HIGH) || (arg == FJ_WM_EDGE_LOW))) {
            lRet = fj_wm_sw_iocs_request_motion_irq(arg);
        } else {
            printk(KERN_ERR "%s : invalid argument (arg : %ld)\n", __func__, arg);
            lRet = -EINVAL;
        }
        break;
    /** Cancel request motion IRQ */
    case FJ_WM_IOCT_CANCELMOTIONIRQ:
        lRet = fj_wm_sw_ioct_cancel_motion_irq();
        break;
    /* Wakeup */
    case FJ_WM_IOCS_WAKEUPCONTROL:
        if (likely((arg == FJ_WM_WAKEUP_HIGH) || (arg == FJ_WM_WAKEUP_LOW))) {
            if (fj_wm_data->prepare_flag) {
                printk(KERN_WARNING "%s : cmd=FJ_WM_IOCS_WAKEUPCONTROL arg=%ld  but kernel going suspend now. \n", __func__, arg);
            }
            lRet = fj_wm_sw_iocs_wakeup_control(arg);
        } else {
            printk(KERN_ERR "%s : invalid argument (arg : %ld)\n", __func__, arg);
            lRet = -EINVAL;
        }
        break;
    default:
        printk(KERN_ERR "%s : invalid command (cmd : %d)\n", __func__, cmd);
        lRet = -EINVAL;
        break;
    }

    mutex_unlock(&fj_wm_data->io_lock);

    debug_printk("%s : ret(%ld) end\n", __func__, lRet);
    return lRet;
}

/** (INIT)fj_wm_setup_cdev
 *
 * @param dev   : cdev
 * @param minor : minor
 * @param fops  : file operations
 * @return void
 */
static void
fj_wm_setup_cdev(struct cdev *dev, int minor,
        struct file_operations *fops)
{
    int err = 0;
    int devno = MKDEV(fj_wm_major, minor);
    debug_printk("%s : start\n", __func__);

    cdev_init(dev, fops);
    dev->owner = THIS_MODULE;
    dev->ops = fops;
    err = cdev_add(dev, devno, 1);
    /* Fail gracefully if need be */
    if (unlikely(err != 0))
        printk(KERN_ERR "%s : cdev_add failed(err : %d)\n", __func__, err);
    if (unlikely(IS_ERR(device_create(fj_wm_class, NULL, devno, NULL, DEV_NAME))))
        printk(KERN_ERR "%s : device_create failed\n", __func__);

    debug_printk("%s : end\n", __func__);
}

/** Initialize file operations */
static struct file_operations fj_wm_fileops = {
    .owner              = THIS_MODULE,
    .open               = fj_wm_if_open,           /** open */
    .release            = fj_wm_if_release,        /** release */
    .unlocked_ioctl     = fj_wm_if_unlocked_ioctl  /** ioctl */
};

/** (INIT)fj_walkmotion_probe
 *
 * @param pdev : Not use
 * @return 0  Success
 *         !0 Fail
 */
static int __devinit
fj_walkmotion_probe(struct platform_device *pdev)
{
    struct fj_wm_platform_data *pdata;
    int ret = 0;
    static struct input_dev *input_data = NULL;

    debug_printk("%s : start\n", __func__);
    fj_wm_data = kzalloc(sizeof(*fj_wm_data), GFP_KERNEL);
    if (unlikely(fj_wm_data == 0)) {
        printk(KERN_ERR "%s : could not allocate memory\n", __func__);
        return -ENOMEM;
    }

    input_data = input_allocate_device();
    if (unlikely(input_data == 0)) {
        printk(KERN_ERR "%s : input_allocate_device failed\n", __func__);
        kfree(fj_wm_data);
        return -ENOMEM;
    }
    set_bit(EV_ABS, input_data->evbit);
    input_set_capability(input_data, EV_ABS, ABS_X);
    input_data->name = DRIVER_NAME;

    ret = input_register_device(input_data);
    if (unlikely(ret != 0)) {
        printk(KERN_ERR "%s : input_register_device failed\n", __func__);
        kfree(fj_wm_data);
        return ret;
    }

    fj_wm_this_data = input_data;

    ret = fj_wm_if_probe();
    if (unlikely(ret != 0)) {
        printk(KERN_ERR "%s : fj_wm_if_probe failed\n", __func__);
        input_unregister_device(input_data);
        kfree(fj_wm_data);
        return ret;
    }

    fj_wm_wq = create_workqueue("fj_wm_workq");
    if(unlikely(fj_wm_wq == NULL)) {
        printk(KERN_ERR "%s : couldn't create workqueue\n", __func__);
        fj_wm_if_remove();
        input_unregister_device(input_data);
        kfree(fj_wm_data);
        return -ENOMEM;
    }

    /* Initialize work queue */
    INIT_WORK(&fj_wm_work_queue_motion_irq, fj_wm_hw_motion_irq);
    /* Initialize wait queue */
    init_waitqueue_head(&fj_wm_data->wait_queue_motion_irq);

    /* Initialize */
    mutex_init(&fj_wm_data->io_lock);
    wake_lock_init(&fj_wm_data->wake_lock, WAKE_LOCK_SUSPEND, DRIVER_NAME);

    pdata = pdev->dev.platform_data;
    if (pdata->mc_init_delay < 0) {
        fj_wm_data->mc_init_delay = FJ_WM_DEF_MC_INIT_DELAY;
    } else {
        fj_wm_data->mc_init_delay = pdata->mc_init_delay;
    }
    fj_wm_data->motion_irq = pdata->motion_irq;

    fj_wm_data->state = FJ_WM_STS_INITIALIZER;
    fj_wm_data->dbg_dev = &pdev->dev;
    fj_wm_data->irq_flag = 0;

/* FUJITSU:2013-01-11 Proximity sensor wake lock add start */
	fj_wm_data->prepare_flag = 0;
	spin_lock_init( &fj_wm_data->spinlock );
/* FUJITSU:2013-01-11 Proximity sensor wake lock add  end  */

    fj_wm_data->init_flag = 0;
    fj_wm_data->wakeup_flag = 0;

    fj_wm_open_cnt = 0;

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (INIT)fj_walkmotion_remove
 *
 * @param pdev : not use
 * @return 0  success
 */
static int __devexit fj_walkmotion_remove(struct platform_device *pdev)
{
    debug_printk("%s : start\n", __func__);

    fj_wm_if_remove();

    cancel_work_sync(&fj_wm_work_queue_motion_irq);
    destroy_workqueue(fj_wm_wq);

    if (likely(fj_wm_this_data != NULL)) {
        input_unregister_device(fj_wm_this_data);
    }
    kfree(fj_wm_data);

    debug_printk("%s : end\n", __func__);
    return 0;
}

/* FUJITSU:2013-01-11 Proximity sensor wake lock add start */
/** Prepare module
 *
 * @param pdev  : not use
 * @param state : not use
 * @return 0 success
 */
static int fj_walkmotion_prepare(struct device *pdev)
{
	unsigned long	irq_flags = 0;

	debug_printk("%s : start\n", __func__);

    printk(KERN_INFO "%s : HCE's wakeup setting(%d)---0:WAKEUP, 1:SLEEP. \n", __func__, fj_wm_data->wakeup_flag);

	/* flg on */
	spin_lock_irqsave( &fj_wm_data->spinlock, irq_flags );
	fj_wm_data->prepare_flag = 1;
	spin_unlock_irqrestore( &fj_wm_data->spinlock, irq_flags );

	debug_printk("%s : end\n", __func__);
	return 0;
}

/** Complete module
 *
 * @param pdev  : not use
 * @return void
 */
static void fj_walkmotion_complete(struct device *pdev)
{
	unsigned long	irq_flags = 0;

	debug_printk("%s : start\n", __func__);

	/* flg off */
	spin_lock_irqsave( &fj_wm_data->spinlock, irq_flags );
	fj_wm_data->prepare_flag = 0;
	spin_unlock_irqrestore( &fj_wm_data->spinlock, irq_flags );

	debug_printk("%s : end\n", __func__);
}
/* FUJITSU:2013-01-11 Proximity sensor wake lock add  end  */

/** Walk motion driver */
/* FUJITSU:2013-01-11 Proximity sensor wake lock add start */
static const struct dev_pm_ops fj_walkmotion_pm_ops = {
	.prepare		= fj_walkmotion_prepare,
	.complete		= fj_walkmotion_complete,
};
/* FUJITSU:2013-01-11 Proximity sensor wake lock add  end  */

static struct platform_driver fj_walkmotion_driver = {
    .driver.name    = DRIVER_NAME,
    .driver.owner   = THIS_MODULE,
/* FUJITSU:2013-01-11 Proximity sensor wake lock add start */
	.driver.pm		= &fj_walkmotion_pm_ops,
/* FUJITSU:2013-01-11 Proximity sensor wake lock add  end  */
    .probe      = fj_walkmotion_probe,
    .remove     = __devexit_p(fj_walkmotion_remove),
};

/** (INIT)fj_walkmotion_init
 *
 * @return 0  Success
 *         !0 Fail
 */
static int __init fj_walkmotion_init(void)
{
    int ret = 0;
    dev_t dev = 0;

    debug_printk("%s : start\n", __func__);

    ret = fj_wm_if_init();
    if (unlikely(ret != 0)) {
        printk(KERN_ERR "%s : fj_wm_if_init error(err : %d )\n", __func__, ret);

        return ret;
    }

    fj_wm_class = class_create(THIS_MODULE, DEV_NAME);
    if (unlikely(IS_ERR(fj_wm_class))) {
        ret = PTR_ERR(fj_wm_class);
        printk(KERN_ERR "%s : class_create error(err : %d )\n", __func__, ret);

        return ret;
    }

    ret = alloc_chrdev_region(&dev, 0, 2, DEV_NAME);
    if (unlikely(ret < 0)) {
        printk(KERN_ERR "%s : Can't allocate chrdev region(ret : %d)\n",
                                                                __func__, ret);
        return ret;
    }

    fj_wm_major = MAJOR(dev);
    if (fj_wm_major == 0)
        fj_wm_major = ret;

    fj_wm_setup_cdev(&fj_wm_cdev, 0, &fj_wm_fileops);

    ret = platform_driver_register(&fj_walkmotion_driver);
    if (unlikely(ret != 0)) {
        printk(KERN_ERR "%s : platform_driver_register(err : %d )\n", __func__, ret);
        class_destroy(fj_wm_class);
        fj_wm_if_exit();
    }

    debug_printk("%s : end\n", __func__);
    return ret;
}
module_init(fj_walkmotion_init);

/** (INIT)fj_walkmotion_exit
 *
 * @return void
 */
static void __exit fj_walkmotion_exit(void)
{
    debug_printk("%s : start\n", __func__);
    class_destroy(fj_wm_class);
    platform_driver_unregister(&fj_walkmotion_driver);

    fj_wm_if_exit();

    debug_printk("%s : end\n", __func__);
}
module_exit(fj_walkmotion_exit);

MODULE_ALIAS("platform:fj-walkmotion");
MODULE_AUTHOR("FUJITSU LIMITED");
MODULE_DESCRIPTION("Fujitsu Walk Motion MC Driver");
MODULE_LICENSE("GPL");
