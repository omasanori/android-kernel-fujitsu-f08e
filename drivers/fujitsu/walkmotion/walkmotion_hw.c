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
#include <../board-8064.h>
#include "walkmotion_hw.h"
#include "walkmotion_data.h"

extern struct fj_wm_data *fj_wm_data;
extern struct input_dev *fj_wm_this_data;
extern struct workqueue_struct *fj_wm_wq;
extern struct work_struct fj_wm_work_queue_motion_irq;

/** (HW)fj_wm_hw_irq_handler
 *
 * @param irq  : Not use
 * @param data : Not use
 * @return IRQ_HANDLED
 */
irqreturn_t fj_wm_hw_irq_handler(int irq, void *data)
{
    debug_printk("%s : start\n", __func__);
    queue_work(fj_wm_wq, &fj_wm_work_queue_motion_irq);
/* FUJITSU:2013-01-11 Proximity sensor wake lock add start */
    wake_lock_timeout(&fj_wm_data->wake_lock,
        unlikely(fj_wm_data->prepare_flag) ? FJ_WM_WAKELOCK_TIME_PREPARE*HZ : FJ_WM_WAKELOCK_TIME*HZ);
/* FUJITSU:2013-01-11 Proximity sensor wake lock add  end  */

    debug_printk("%s : end\n", __func__);

    return IRQ_HANDLED;
}

/** (HW)fj_wm_hw_board_check
 *
 * @param ws : Not use
 * @return void
 */
void fj_wm_hw_motion_irq(struct work_struct *ws)
{
    static int ev_value = 1;

    debug_printk("%s : start\n", __func__);

    if (fj_wm_data->init_flag == 1) {
        debug_printk("%s : first motion_irq\n", __func__);
        fj_wm_data->init_flag = 0;
        wake_up_interruptible(&fj_wm_data->wait_queue_motion_irq);
    } else {
        if (likely(fj_wm_data->irq_flag == 1)) {
            debug_printk("%s : normal motion_irq(ev_value : %d)\n", __func__, ev_value);
            input_report_abs(fj_wm_this_data, ABS_X, ev_value);
            input_sync(fj_wm_this_data);
            ev_value =  (ev_value == 1) ? 2 : 1;
        }
    }

    debug_printk("%s : end\n", __func__);
}

/** (HW)fj_wm_hw_board_check
 *
 * @param void
 * @return 0 or 1
 */
static int fj_wm_hw_board_check(void)
{
    int ret   = 0;
    int model = 0;
    int board = 0;
    debug_printk("%s : start\n", __func__);

    ret    = gpio_get_value_cansleep(FJ_WM_GPIO_MODEL_3);
    model  = ret << 2;
    ret    = gpio_get_value_cansleep(FJ_WM_GPIO_MODEL_2);
    model |= ret << 1;
    model |= gpio_get_value_cansleep(FJ_WM_GPIO_MODEL_1);

    ret    = gpio_get_value_cansleep(FJ_WM_GPIO_BOARD_3);
    board  = ret << 2;
    ret    = gpio_get_value_cansleep(FJ_WM_GPIO_BOARD_2);
    board |= ret << 1;
    board |= gpio_get_value_cansleep(FJ_WM_GPIO_BOARD_1);

    debug_printk("%s : model=%d\n", __func__, model);
    debug_printk("%s : board=%d\n", __func__, board);

    ret = 0;
    if (model == FJ_WM_MODEL_LUPIN) {
        ret = 1;
    }

    debug_printk("%s : ret=%d end\n", __func__, ret);
    return ret;
}

/** (HW)fj_wm_hw_set_gpio
 *
 * @param pin   : gpio pin
 * @param value : gpio value
 * @return 0  Success
 */
static int fj_wm_hw_set_gpio(unsigned long pin, unsigned long value)
{
    debug_printk("%s : start\n", __func__);

    debug_printk("gpio_set_value_cansleep(%ld, %ld)\n", pin, value);
    gpio_set_value_cansleep(pin, value);

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (HW)fj_wm_hw_init
 *
 * @return 0  Success
 */
int fj_wm_hw_init(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_hw_board_check();
    if (ret != 0) {
        usleep(FJ_WM_LUPIN_SLEEP_TIME);

        fj_wm_hw_set_gpio(PM8921_GPIO_PM_TO_SYS(FJ_WM_PM8921_SENS_HCE_LDO_ON), FJ_WM_GPIO_HIGH);
    }

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (HW)fj_wm_hw_exit
 *
 * @return 0  Success
 */
int fj_wm_hw_exit(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = fj_wm_hw_board_check();
    if (ret != 0) {
        fj_wm_hw_set_gpio(PM8921_GPIO_PM_TO_SYS(FJ_WM_PM8921_SENS_HCE_LDO_ON), FJ_WM_GPIO_LOW);
    }

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (HW)fj_wm_hw_probe
 *
 * @return 0  Success
 *         !0 Fail
 */
int fj_wm_hw_probe(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    ret = gpio_tlmm_config(GPIO_CFG(FJ_WM_GPIO_MOTION_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
    if (unlikely(ret != 0)) {
        printk(KERN_ERR "%s: gpio_tlmm_config(MOTION_IRQ)=%d\n", __func__, ret);
    }

    debug_printk("%s : ret(%d) end\n", __func__, ret);
    return ret;
}

/** (HW)fj_wm_hw_remove
 *
 * @return 0  Success
 */
int fj_wm_hw_remove(void)
{
    debug_printk("%s : start\n", __func__);
    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (HW)fj_wm_hw_release
 *
 * @return 0  Success
 */
int fj_wm_hw_release(void)
{
    debug_printk("%s : start\n", __func__);

    if (likely(fj_wm_data->irq_flag == 1)) {
        free_irq(fj_wm_data->motion_irq, fj_wm_data);
        fj_wm_data->irq_flag = 0;
        debug_printk("%s : irq_flag = %d\n", __func__, fj_wm_data->irq_flag);
    }

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (HW)fj_wm_hw_hce_reset
 *
 * @return 0  Success
 *         !0 Fail
 */
long fj_wm_hw_hce_reset(void)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    fj_wm_hw_set_gpio(FJ_WM_GPIO_RESET, FJ_WM_GPIO_LOW);

    /* Wait for 20ms */
    msleep(fj_wm_data->mc_init_delay);

    if (likely(fj_wm_data->irq_flag == 0)) {
        ret = request_irq(fj_wm_data->motion_irq, fj_wm_hw_irq_handler,
                                    IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, "fj_wm", fj_wm_data);
        if (unlikely(ret < 0)) {
            printk(KERN_ERR "FJ_WM_IOCT_INITIALIZE : request_irq failed (ret : %d)\n", ret);
            return ret;
        } else {
            fj_wm_data->irq_flag = 1;
            debug_printk("FJ_WM_IOCT_INITIALIZE : irq_flag = %d\n", fj_wm_data->irq_flag);
        }
    }

    fj_wm_hw_set_gpio(FJ_WM_GPIO_RESET, FJ_WM_GPIO_HIGH);

    /* Wait IRQ */
    fj_wm_data->init_flag = 1;
    ret = wait_event_interruptible_timeout(fj_wm_data->wait_queue_motion_irq,
                                                    fj_wm_data->init_flag == 0,
                                                    msecs_to_jiffies(FJ_WM_HCE_RESET_IRQ_WAIT_TIME));
    if (unlikely(ret <= 0)) {
        /* If canceled */
        if (likely(fj_wm_data->irq_flag == 1)) {
            free_irq(fj_wm_data->motion_irq, fj_wm_data);
            fj_wm_data->irq_flag = 0;
            debug_printk("FJ_WM_IOCT_INITIALIZE : irq_flag = %d\n", fj_wm_data->irq_flag);
        }
        printk(KERN_ERR "FJ_WM_IOCT_INITIALIZE : wait_event_interruptible_timeout canceled (ret : %d)\n", ret);
        return -ECANCELED;
    } else {
        if (likely(fj_wm_data->irq_flag == 1)) {
            free_irq(fj_wm_data->motion_irq, fj_wm_data);
            fj_wm_data->irq_flag = 0;
            debug_printk("FJ_WM_IOCT_INITIALIZE : irq_flag = %d\n", fj_wm_data->irq_flag);
        }
    }

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (HW)fj_wm_hw_hce_request_irq
 *
 * @param value
 * @return 0  Success
 *         !0 Fail
 */
long fj_wm_hw_hce_request_irq(unsigned long value)
{
    int ret = 0;
    debug_printk("%s : start\n", __func__);

    if (value == FJ_WM_EDGE_HIGH) {
        /* High-edge detection */
        debug_printk("FJ_WM_IOCS_REQUESTMOTIONIRQ : FJ_WM_EDGE_HIGH\n");
        if (likely(fj_wm_data->irq_flag == 0)) {
            ret = request_irq(fj_wm_data->motion_irq, fj_wm_hw_irq_handler,
                                    IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, "fj_wm", fj_wm_data);
            if (unlikely(ret < 0)) {
                printk(KERN_ERR "FJ_WM_IOCS_REQUESTMOTIONIRQ"
                     "(FJ_WM_EDGE_HIGH) : request_irq failed (ret : %d)\n",
                                                                      ret);
                return ret;
            }
            fj_wm_data->irq_flag = 1;
            debug_printk("FJ_WM_IOCS_REQUESTMOTIONIRQ(FJ_WM_EDGE_HIGH) : "
                                     "irq_flag = %d\n", fj_wm_data->irq_flag);
        }
    } else {
        /* Low-edge detection */
        debug_printk("FJ_WM_IOCS_REQUESTMOTIONIRQ : FJ_WM_EDGE_LOW\n");
        if (likely(fj_wm_data->irq_flag == 0)) {
            ret = request_irq(fj_wm_data->motion_irq, fj_wm_hw_irq_handler,
                                   IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND, "fj_wm", fj_wm_data);
            if (unlikely(ret < 0)) {
                printk(KERN_ERR "FJ_WM_IOCS_REQUESTMOTIONIRQ"
                                    "(FJ_WM_EDGE_LOW) : request_irq failed"
                                                    " (ret : %d)\n" , ret);
                return ret;
            }
            fj_wm_data->irq_flag = 1;
            debug_printk("FJ_WM_IOCS_REQUESTMOTIONIRQ(FJ_WM_EDGE_LOW) : "
                                     "irq_flag = %d\n", fj_wm_data->irq_flag);
        }
    }

    irq_set_irq_wake(fj_wm_data->motion_irq, 1);

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (HW)fj_wm_hw_hce_cancel_irq
 *
 * @return 0  Success
 */
long fj_wm_hw_hce_cancel_irq(void)
{
    debug_printk("%s : start\n", __func__);

    if (likely(fj_wm_data->irq_flag == 1)) {
        free_irq(fj_wm_data->motion_irq, fj_wm_data);
        fj_wm_data->irq_flag = 0;
        debug_printk("FJ_WM_IOCT_CANCELMOTIONIRQ : irq_flag = %d\n",
                                                        fj_wm_data->irq_flag);
    }
    input_report_abs(fj_wm_this_data, ABS_X, 0);
    input_sync(fj_wm_this_data);

    debug_printk("%s : end\n", __func__);
    return 0;
}

/** (HW)fj_wm_hw_hce_wakeup_set
 *
 * @param value
 * @return 0  Success
 */
long fj_wm_hw_hce_wakeup_set(unsigned long value)
{
    debug_printk("%s : start\n", __func__);

    fj_wm_hw_set_gpio(FJ_WM_GPIO_HOSU_WUP, value);

    debug_printk("%s : end\n", __func__);
    return 0;
}

