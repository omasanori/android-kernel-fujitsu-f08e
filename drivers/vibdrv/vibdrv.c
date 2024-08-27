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

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <../board-8064.h>

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <vibdrv.h>
#include <vibdrv_i2c.h>
#include <vibdrv_vib.h>

#include "../staging/android/timed_output.h"

/*============================================================================
                        EXTERNAL VARIABLES DEFINITIONS
============================================================================*/
/* write fw thread */
static struct workqueue_struct *work_queue;
static struct work_struct      write_work;
static struct list_head        write_head;
struct writeDataType {
    struct list_head list;
    int   firmware_size;
    char  firmware_data[HAPTICS_FIRMWARE_SIZE];
};

/* write wf thread */
static struct workqueue_struct *wave_work_queue;
static struct work_struct      wave_write_work;
static struct list_head        wave_write_head;
struct writeWaveType {
    struct list_head list;
    int   waveform_cnt;
    struct _waveform_data_table waveform[HAPTICS_WAVE_TABLE_MAX];
};

/* register timed_output class */
static struct timed_output_dev timed_dev;
static int vibrator_value;
static struct work_struct work_vibrator_on;
static struct delayed_work work_vibrator_off;
static struct hrtimer vibe_timer;
static void vibdrv_timed_vibrator_off(struct timed_output_dev *sdev, int delay);
static void vibdrv_timed_vibrator_on(struct timed_output_dev *sdev);

static DEFINE_SPINLOCK(work_lock);
static struct mutex vib_mutex;

/*===========================================================================
    STATIC FUNCTION      vibdrv_open
===========================================================================*/
static int vibdrv_open(struct inode *inode, struct file *file) 
{
    VIBDRV_LOGI("open.\n");
    if (!try_module_get(THIS_MODULE)) return -ENODEV;
    return VIBDRV_RET_OK; 
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_release
===========================================================================*/
static int vibdrv_release(struct inode *inode, struct file *file) 
{
    VIBDRV_LOGI("release.\n");
    module_put(THIS_MODULE);
    return VIBDRV_RET_OK; 
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_read
===========================================================================*/
static ssize_t vibdrv_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    VIBDRV_LOGI("read.\n");
    return VIBDRV_RET_OK;
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_write
===========================================================================*/
static ssize_t vibdrv_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    VIBDRV_LOGI("write.\n");
    return VIBDRV_RET_OK;
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_unlocked_ioctl
===========================================================================*/
static long vibdrv_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int rc = 0;
    long return_val=0;
    unsigned long           irqflags;
    static struct writeDataType *pwriteDataType=0;
    static struct writeWaveType *pwriteWaveType=0;
    vibdrv_firm_data_t      firm_data;
    vibdrv_firm_version_t   firm_version;
    vibdrv_waveform_table_t waveform_table;
    vibdrv_check_waveform_t check_waveform;
    vibdrv_int_status_t     int_status;
    int8_t  state;
    int  i;
#if VIBDRV_DEBUG
    vibdrv_read_firm_data_t read_firm_data;
#endif //VIBDRV_DEBUG
    
    switch (cmd) {
        case VIBDRV_ENABLE_AMP:
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_ENABLE_AMP.\n");
            rc = vibdrv_haptics_enable_amp();
            break;

        case VIBDRV_DISABLE_AMP:
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_DISABLE_AMP.\n");
            rc = vibdrv_haptics_disable_amp();
            break;

        case VIBDRV_WRITE_FW:
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_WRITE_FW.\n");
            if( !vibdrv_check_state() ){
                VIBDRV_LOGE("VIBDRV_WRITE_FW busy 0x%x.\n",vibdrv_check_state());
                return -EBUSY;
            }

            rc = copy_from_user(&firm_data, (int __user *) arg, sizeof(vibdrv_firm_data_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                return -EINVAL;
            }
            
            if( !firm_data.firm_data ) {
                VIBDRV_LOGE("copy_from_user faild firm_data.\n");
                return -EINVAL;
            }
            if( firm_data.firm_size > HAPTICS_FIRMWARE_SIZE ) {
                VIBDRV_LOGE("copy_from_user faild firm_size.\n");
                return -EINVAL;
            }

            pwriteDataType = kzalloc(sizeof(struct writeDataType), GFP_KERNEL);
            if (!pwriteDataType) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                return -ENOMEM;
            }

            rc = copy_from_user(&pwriteDataType->firmware_data[0], (int __user *)firm_data.firm_data, firm_data.firm_size);
            if (rc < 0) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                kfree(pwriteDataType);
                return -EINVAL;
            }
            pwriteDataType->firmware_size = firm_data.firm_size;

            spin_lock_irqsave(&work_lock, irqflags);
            INIT_LIST_HEAD(&pwriteDataType->list);
            list_add_tail(&pwriteDataType->list,&write_head);
            queue_work(work_queue, &write_work);
            vibdrv_set_state(VIBDRV_WRITING);
            spin_unlock_irqrestore(&work_lock, irqflags);

            firm_data.return_val = VIBDRV_RET_OK;
            rc = copy_to_user((int __user *) arg, &firm_data, sizeof(vibdrv_firm_data_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_to_user faild 0x%x.\n",rc);
                return -EFAULT;
            }
            break;

        case VIBDRV_CHK_FW_WRITE:
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_CHK_FW_WRITE.\n");
            state = vibdrv_get_state();
            rc = copy_to_user((int __user *) arg, &state, sizeof(int8_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_to_user faild 0x%x.\n",rc);
                return -EFAULT;
            }
            break;

        case VIBDRV_GET_FWVER:
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_GET_FWVER.\n");
            if( !vibdrv_check_state() ){
                VIBDRV_LOGE("VIBDRV_GET_FWVER busy 0x%x.\n",vibdrv_check_state());
                return -EBUSY;
            }

            rc = vibdrv_get_firmware_version(&firm_version.major_ver, &firm_version.minor_ver);
            if(rc != VIBDRV_RET_OK){
                memset(&firm_version,0,sizeof(vibdrv_firm_version_t));
            }
            rc = copy_to_user((int __user *) arg, &firm_version, sizeof(vibdrv_firm_version_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_to_user faild 0x%x.\n",rc);
                return -EFAULT;
            }
            break;

        case VIBDRV_WRITE_WF: 
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_WRITE_WF.\n");
            if( !vibdrv_check_state() ){
                VIBDRV_LOGE("VIBDRV_WRITE_WF busy 0x%x.\n",vibdrv_check_state());
                return -EBUSY;
            }

            rc = copy_from_user(&waveform_table, (int __user *) arg, sizeof(vibdrv_waveform_table_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                return -EINVAL;
            }
            
            pwriteWaveType = kzalloc(sizeof(struct writeWaveType), GFP_KERNEL);
            if (!pwriteWaveType) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                return -ENOMEM;
            }

            pwriteWaveType->waveform_cnt=0;
            for(i=0;i<waveform_table.table_cnt;i++){
                VIBDRV_LOGI("VIBDRV_WRITE_WF: trg:%x,id:%x,size:%x.\n",
                             waveform_table.waveform[i].trg_type,waveform_table.waveform[i].id_value,waveform_table.waveform[i].wf_size);
                if(!vibdrv_check_waveform_param(waveform_table.waveform[i].trg_type,waveform_table.waveform[i].id_value)){
                    VIBDRV_LOGE("parameter faild %d file skip\n",i);
                    continue;
                }
                if( waveform_table.waveform[i].wf_size > HAPTICS_WAVEFORM_SIZE ) {
                    VIBDRV_LOGE("copy_from_user faild firm_size %d file skip.\n",i);
                    continue;
                }
                pwriteWaveType->waveform[pwriteWaveType->waveform_cnt].trg_type      = waveform_table.waveform[i].trg_type;
                pwriteWaveType->waveform[pwriteWaveType->waveform_cnt].id_value      = waveform_table.waveform[i].id_value;
                if( pwriteWaveType->waveform[pwriteWaveType->waveform_cnt].trg_type != VIBDRV_WF_TRG_VER ) {
                    pwriteWaveType->waveform[pwriteWaveType->waveform_cnt].waveform_size = waveform_table.waveform[i].wf_size;
                    rc = copy_from_user(&pwriteWaveType->waveform[pwriteWaveType->waveform_cnt].waveform_data[0],
                                        (int __user *)waveform_table.waveform[i].wf_data, waveform_table.waveform[i].wf_size);
                    if (rc < 0) {
                        VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                        kfree(pwriteWaveType);
                        return -EINVAL;
                    }
                }
                pwriteWaveType->waveform_cnt++;
            }
            spin_lock_irqsave(&work_lock, irqflags);
            INIT_LIST_HEAD(&pwriteWaveType->list);
            list_add_tail(&pwriteWaveType->list,&wave_write_head);
            queue_work(wave_work_queue, &wave_write_work);
            vibdrv_set_state(VIBDRV_WRITING);
            spin_unlock_irqrestore(&work_lock, irqflags);

            waveform_table.return_val = VIBDRV_RET_OK;
            rc = copy_to_user((int __user *) arg, &waveform_table, sizeof(vibdrv_waveform_table_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_to_user faild 0x%x.\n",rc);
                return -EFAULT;
            }
            break;

        case VIBDRV_CEHCK_WF: 
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_CEHCK_WF.\n");
            if( !vibdrv_check_state() ){
                VIBDRV_LOGE("VIBDRV_CEHCK_WF busy 0x%x.\n",vibdrv_check_state());
                return -EBUSY;
            }

            rc = copy_from_user(&check_waveform, (int __user *) arg, sizeof(vibdrv_check_waveform_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                return -EINVAL;
            }
            check_waveform.return_val = vibdrv_check_waveform(check_waveform.type, check_waveform.table_id);

            rc = copy_to_user((int __user *) arg, &check_waveform, sizeof(vibdrv_check_waveform_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_to_user faild 0x%x.\n",rc);
                return -EFAULT;
            }
            break;

        case VIBDRV_SET_INT: 
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_SET_INT.\n");
            rc = copy_from_user(&int_status, (int __user *) arg, sizeof(vibdrv_int_status_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                return -EINVAL;
            }
            rc = vibdrv_set_int_status(int_status.int_status, &int_status.ret_status );
            if(rc != VIBDRV_RET_OK){
                int_status.ret_status = VIBDRV_INT_ERR;
            }
            rc = copy_to_user((int __user *) arg, &int_status, sizeof(vibdrv_int_status_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_to_user faild 0x%x.\n",rc);
                return -EFAULT;
            }
            break;

#if VIBDRV_DEBUG
        case VIBDRV_READ_WF:
            VIBDRV_LOGI("unlocked_ioctl cmd:VIBDRV_READ_WF.\n");
            if( !vibdrv_check_state() ){
                VIBDRV_LOGE("VIBDRV_READ_WF busy 0x%x.\n",vibdrv_check_state());
                return -EBUSY;
            }

            rc = copy_from_user(&read_firm_data, (int __user *) arg, sizeof(vibdrv_read_firm_data_t));
            if (rc < 0) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                return -EINVAL;
            }
            
            if( !read_firm_data.firm_data ) {
                VIBDRV_LOGE("could not memory.\n");
                return -EFAULT;
            }

            pwriteDataType = kzalloc(sizeof(struct writeDataType), GFP_KERNEL);
            if (!pwriteDataType) {
                VIBDRV_LOGE("copy_from_user faild 0x%x.\n",rc);
                printk(KERN_ERR "vibdrv: could not allocate memory\n");
                return -ENOMEM;
            }
            pwriteDataType->firmware_size = read_firm_data.firm_size;

            rc = vibdrv_read_firmware(pwriteDataType->firmware_data,pwriteDataType->firmware_size,read_firm_data.blockno);
            if(rc != 0){
                VIBDRV_LOGE("vibdrv_write_firmware faild 0x%x.\n",rc);
                return -EFAULT;
            }

            rc = copy_to_user((int __user *) read_firm_data.firm_data, pwriteDataType->firmware_data, read_firm_data.firm_size);
            if (rc < 0) {
                VIBDRV_LOGE("copy_to_user faild 0x%x.\n",rc);
                return -EFAULT;
            }
            break;
#endif //VIBDRV_DEBUG
    default:
        VIBDRV_LOGI("unlocked_ioctl cmd:default.\n");
        return_val = -EFAULT;
    }

    VIBDRV_LOGI("unlocked_ioctl end.\n");
    return return_val;
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_timer_active
===========================================================================*/
static int vibdrv_timer_active(void)
{
    int rc = 0;
    mutex_lock(&vib_mutex);
    if (hrtimer_active(&vibe_timer)) {
        rc = 1;
    }
    mutex_unlock(&vib_mutex);
    return rc;
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_vibrator_on
===========================================================================*/
static void vibdrv_vibrator_on(struct work_struct *work)
{
    VIBDRV_LOGI("vibdrv_vibrator_on.\n");
    hrtimer_start(&vibe_timer,ktime_set(vibrator_value / 1000, (vibrator_value % 1000) * 1000000),HRTIMER_MODE_REL);
    if (vibdrv_timer_active()) {
        vibdrv_haptics_enable_amp();
    }
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_vibrator_off
===========================================================================*/
static void vibdrv_vibrator_off(struct work_struct *work)
{
    VIBDRV_LOGI("vibdrv_vibrator_off.\n");
    if (!vibdrv_timer_active()) {
        vibdrv_haptics_disable_amp();
    }
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_timed_vibrator_on
===========================================================================*/
static void vibdrv_timed_vibrator_on(struct timed_output_dev *sdev)
{
    schedule_work(&work_vibrator_on);
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_timed_vibrator_off
===========================================================================*/
static void vibdrv_timed_vibrator_off(struct timed_output_dev *sdev, int delay)
{
    schedule_delayed_work(&work_vibrator_off, msecs_to_jiffies(delay));
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_get_time
===========================================================================*/
static int vibdrv_get_time(struct timed_output_dev *dev)
{
    VIBDRV_LOGI("vibdrv_get_time.\n");
    if (hrtimer_active(&vibe_timer)) {
        ktime_t r = hrtimer_get_remaining(&vibe_timer);
        return (int)ktime_to_ms(r);
    } else {
        return 0;
    }
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_enable
===========================================================================*/
static void vibdrv_enable(struct timed_output_dev *dev, int value)
{
    int vibrator_time = 0;
    if (value == 0) {
        if (vibdrv_timer_active()) {
            vibrator_time = vibdrv_get_time(dev);
            VIBDRV_LOGI("vibdrv_enable vibrator Off(time[%d])\n", vibrator_time);
            /* Put a delay of 10 ms, in case the vibrator is off immediately */
            if (vibrator_time < 10 || vibrator_time > 50){ //
                hrtimer_cancel(&vibe_timer);
                vibrator_time = 0;
            }
            vibrator_value = value;
            vibdrv_timed_vibrator_off(dev, vibrator_time + 10);
        }
    }
    else {
        VIBDRV_LOGI("vibdrv_enable vibrator On(time[%d])\n", value);
        vibrator_value = (value > 15000 ? 15000 : value); //
        hrtimer_cancel(&vibe_timer);
        vibdrv_timed_vibrator_on(dev);
    }
    return;
}

/*===========================================================================
    STATIC FUNCTION      vibrator_timer_func
===========================================================================*/
static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
    VIBDRV_LOGI("vibrator_timer_func start.\n");
    vibdrv_timed_vibrator_off(NULL,0);
    return HRTIMER_NORESTART;
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_write_work
===========================================================================*/
static void vibdrv_write_work(struct work_struct *work)
{
    int ret=VIBDRV_RET_OK;
    struct writeDataType *pwriteData,*tmp;
    unsigned long irqflags;

    VIBDRV_LOGI("vibdrv_write_work start.\n");

    mutex_lock(&vib_mutex);
    list_for_each_entry_safe(pwriteData,tmp,&(write_head),list) {
        ret = vibdrv_write_firmware(pwriteData->firmware_data,pwriteData->firmware_size);
        spin_lock_irqsave(&work_lock, irqflags);
        vibdrv_set_state( (ret == VIBDRV_RET_OK ? VIBDRV_WRITE_COMPLETE : VIBDRV_WRITE_NG ) );
        spin_unlock_irqrestore(&work_lock, irqflags);
        list_del(&pwriteData->list);
        kfree(pwriteData);
    }
    mutex_unlock(&vib_mutex);

    VIBDRV_LOGI("vibdrv_write_work end.\n");
}


/*===========================================================================
    STATIC FUNCTION      vibdrv_write_wave_work
===========================================================================*/
static void vibdrv_write_wave_work(struct work_struct *work)
{
    int ret=VIBDRV_RET_OK;
    struct writeWaveType *pwriteData,*tmp;
    unsigned long irqflags;

    VIBDRV_LOGI("vibdrv_write_wave_work start.\n");

    mutex_lock(&vib_mutex);
    list_for_each_entry_safe(pwriteData,tmp,&(wave_write_head),list) {
        ret = vibdrv_write_waveform(&pwriteData->waveform[0],pwriteData->waveform_cnt );
        spin_lock_irqsave(&work_lock, irqflags);
        vibdrv_set_state( (ret == VIBDRV_RET_OK ? VIBDRV_WRITE_COMPLETE : VIBDRV_WRITE_NG ) );
        spin_unlock_irqrestore(&work_lock, irqflags);
        list_del(&pwriteData->list);
        kfree(pwriteData);
    }
    mutex_unlock(&vib_mutex);

    VIBDRV_LOGI("vibdrv_write_wave_work end.\n");
}


/*===========================================================================
    STATIC FUNCTION      vibdrv_probe
===========================================================================*/
static int vibdrv_probe(struct platform_device *pdev)
{
    int rc = 0;

    VIBDRV_LOGI("probe.\n");

    /* keventd_wq */
    INIT_WORK(&work_vibrator_on, vibdrv_vibrator_on);
    INIT_DELAYED_WORK(&work_vibrator_off, vibdrv_vibrator_off);

    /* make a write fw thread */
    work_queue = create_workqueue("vibdrv_queue");
    INIT_WORK(&write_work, vibdrv_write_work);
    INIT_LIST_HEAD(&write_head);

    /* make a write wf thread */
    wave_work_queue = create_workqueue("vibdrv_wavequeue");
    INIT_WORK(&wave_write_work, vibdrv_write_wave_work);
    INIT_LIST_HEAD(&wave_write_head);
    vibdrv_set_state(VIBDRV_WRITE_COMPLETE);

    /* i2c initialize */
    rc = vibdrv_i2c_initialize();
    if (rc) {
        VIBDRV_LOGE("vibdrv_i2c_initialize faild 0x%x.\n",rc);
    }

    /* setup HAPTICS_INT */
    rc = gpio_request(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_INT), "haptics_int");
    if (rc) {
        VIBDRV_LOGE("gpio_request faild 0x%x.\n",rc);
    }
    rc = gpio_direction_output(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_INT), 1);
    if (rc) {
        VIBDRV_LOGE("gpio_direction_output faild 0x%x.\n",rc);
    }
    gpio_set_value(PM8921_GPIO_PM_TO_SYS(VIBDRV_GPIO_HAPTICS_INT),VIBDRV_GPIO_LEVEL_HIGH);

    /* register timed_output class */
    timed_dev.name = "vibrator";
    timed_dev.get_time = vibdrv_get_time;
    timed_dev.enable = vibdrv_enable;

    hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    vibe_timer.function = vibrator_timer_func;

    rc = timed_output_dev_register(&timed_dev);
    if (rc) {
        VIBDRV_LOGE("timed_output_dev_register faild 0x%x.\n",rc);
    }

    mutex_init(&vib_mutex);

    return rc;
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_remove
===========================================================================*/
static int vibdrv_remove(struct platform_device *pdev)
{
    VIBDRV_LOGI("remove.\n");

    vibdrv_haptics_disable_amp();

    timed_output_dev_unregister(&timed_dev);

    vibdrv_i2c_terminate();

    if (work_queue) destroy_workqueue(work_queue);
    if (wave_work_queue) destroy_workqueue(wave_work_queue);

    mutex_destroy(&vib_mutex);

    return 0;
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_suspend
===========================================================================*/
static int vibdrv_suspend(struct platform_device *pdev, pm_message_t state)
{
    VIBDRV_LOGI("suspend.\n");
    return vibdrv_haptics_suspend();
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_resume
===========================================================================*/
static int vibdrv_resume(struct platform_device *pdev)
{   
    VIBDRV_LOGI("resume.\n");
    return vibdrv_haptics_resume();
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_platform_release
===========================================================================*/
static void vibdrv_platform_release(struct device *dev) 
{
    VIBDRV_LOGI("platform_release.\n");
}

/*============================================================================
                        STRUCTURE
============================================================================*/
/* VIB driver FOPS */
static struct file_operations fops = 
{
    .owner   =          THIS_MODULE,
    .open    =          vibdrv_open,
    .release =          vibdrv_release,
    .read    =          vibdrv_read,
    .write   =          vibdrv_write,
    .unlocked_ioctl =   vibdrv_unlocked_ioctl,
};

/* driver definition */
static struct miscdevice miscdev = 
{
    .minor   =   MISC_DYNAMIC_MINOR,
    .name    =   VIBDRV_MODULE_NAME,
    .fops    =   &fops
};

static struct platform_driver platdrv = 
{
    .probe   =  vibdrv_probe,
    .remove  =  vibdrv_remove,
    .suspend =  vibdrv_suspend,
    .resume  =  vibdrv_resume,
    .driver  =
    {
        .name = VIBDRV_MODULE_NAME,
        .owner= THIS_MODULE,
    },
};

static struct platform_device platdev =
{   
    .name =     VIBDRV_MODULE_NAME,
    .id   =     -1,                     /* means that there is only one device */
    .dev  = {
        .platform_data = NULL,
        .release = vibdrv_platform_release,
    },
};

/*===========================================================================
    STATIC FUNCTION      vibdrv_init
===========================================================================*/
/* Module entrance */
static int __init vibdrv_init(void)
{
    int rc=0;

    VIBDRV_LOGI("vibdrv_init start.\n");

    rc = misc_register(&miscdev);
    if (rc) {
        VIBDRV_LOGE("misc_register faild 0x%x.\n",rc);
        return rc;
    }

    rc = platform_device_register(&platdev);
    if (rc) {
        VIBDRV_LOGE("platform_device_register faild 0x%x.\n",rc);
    }

    rc = platform_driver_register(&platdrv);
    if (rc) {
        VIBDRV_LOGE("platform_driver_register faild 0x%x.\n",rc);
    }

    VIBDRV_LOGI("vibdrv_init end.\n");

    return rc;
}

/*===========================================================================
    STATIC FUNCTION      vibdrv_exit
===========================================================================*/
static void __exit vibdrv_exit(void)
{
    VIBDRV_LOGI("vibdrv_exit start.\n");

    platform_driver_unregister(&platdrv);
    platform_device_unregister(&platdev);

    misc_deregister(&miscdev);

    VIBDRV_LOGI("vibdrv_exit end.\n");
}

module_init(vibdrv_init);
module_exit(vibdrv_exit);

MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("vibdrv device");
MODULE_LICENSE("GPL");
