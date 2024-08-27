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


#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include "msm_serial_aux.h"
#include "msm_serial_aux_hw.h"

/* Device name definition */
#define DEV_NAME    MSM_SERIAL_AUX_DEV_NAME

/* Variable definition */
static int driver_major_no = 0;
module_param( driver_major_no, int, 0 );

static struct cdev char_dev;
static struct class* irda_class;

static atomic_t g_msm_uart_aux_available = ATOMIC_INIT(1);

/* Prototype declaration */
static int msm_uart_aux_sw_init(void);
static int msm_uart_aux_sw_exit(void);
static int msm_uart_aux_sw_irda_on(void);
static int msm_uart_aux_sw_irda_off(void);
static int msm_uart_aux_if_init(void);
static int msm_uart_aux_if_exit(void);
static int msm_uart_aux_if_open(struct inode *, struct file *);
static int msm_uart_aux_if_close(struct inode *, struct file *);
static long msm_uart_aux_if_ioctl(struct file *, unsigned int, unsigned long);
#ifdef SUPPORT_MSM_UART_AUX_MMAP
int msm_uart_aux_if_mmap(struct file *, struct vm_area_struct *);
#endif
static int msm_uart_aux_probe(struct platform_device *);
static int msm_uart_aux_remove(struct platform_device *);
static int msm_uart_aux_init(void);
static void msm_uart_aux_exit(void);

/* structure define */
static struct file_operations driver_fops = {
    .open       = msm_uart_aux_if_open,
    .unlocked_ioctl     = msm_uart_aux_if_ioctl,
#ifdef SUPPORT_MSM_UART_AUX_MMAP
    .mmap       = msm_uart_aux_if_mmap,
#endif /* SUPPORT_MSM_UART_AUX_MMAP */
    .release    = msm_uart_aux_if_close,
};

static struct platform_driver irda_driver = {
    .probe      = msm_uart_aux_probe,
    .remove     = msm_uart_aux_remove,
    .driver = {
        .name   = DEV_NAME,
        .owner  = THIS_MODULE,
    },
};

/*
 * (SW) msm_uart_aux_sw_init
 *
 *  @return     0           Success
 *  @return     !0          Error
 */
static int msm_uart_aux_sw_init(void)
{
    int ret = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    ret = msm_uart_aux_hw_init();

    pr_debug( DEV_NAME ": %s ret(%d)\n", __func__, ret );
    return ret;

}

/*
 * (SW) msm_uart_aux_sw_exit
 *
 *  @return     0           Success
 *  @return     !0          Error
 */
static int msm_uart_aux_sw_exit(void)
{
    int ret = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    ret = msm_uart_aux_hw_exit();

    pr_debug( DEV_NAME ": %s ret(%d)\n", __func__, ret );
    return ret;

}

/*
 * (SW) msm_uart_aux_sw_irda_on
 *
 *  @return     0           Success
 *  @return     !0          Error
 */
static int msm_uart_aux_sw_irda_on(void)
{
    int ret = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    ret = msm_uart_aux_hw_uart_enable();
    pr_debug( DEV_NAME ": %s msm_uart_aux_hw_uart_enable ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s uart enable error. ret=%d\n", __func__, ret);
        return ret;
    }

    ret = msm_uart_aux_hw_chg_irda();
    pr_debug( DEV_NAME ": %s msm_uart_aux_hw_chg_irda ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s chg_irda error. ret=%d\n", __func__, ret);
        return ret;
    }

    ret = msm_uart_aux_hw_irda_start();
    pr_debug( DEV_NAME ": %s msm_uart_aux_hw_irda_start ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s irda_start error. ret=%d\n", __func__, ret);
        return ret;
    }

    pr_debug( DEV_NAME ": %s ret(%d)\n", __func__, ret );
    return ret;

}

/*
 * (SW) msm_uart_aux_sw_irda_off
 *
 *  @return     0           Success
 *  @return     !0          Error
 */
static int msm_uart_aux_sw_irda_off(void)
{
    int ret = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    ret = msm_uart_aux_hw_irda_stop();
    pr_debug( DEV_NAME ": %s msm_uart_aux_hw_irda_stop ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s irda_stop error. ret=%d\n", __func__, ret);
        return ret;
    }

    ret = msm_uart_aux_hw_uart_disable();
    pr_debug( DEV_NAME ": %s msm_uart_aux_hw_uart_disable ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s uart disable error. ret=%d\n", __func__, ret);
        return ret;
    }

    ret = msm_uart_aux_hw_chg_uart();
    pr_debug( DEV_NAME ": %s msm_uart_aux_hw_chg_uart ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s irda_chg_uart error. ret=%d\n", __func__, ret);
        return ret;
    }

    pr_debug( DEV_NAME ": %s ret(%d)\n", __func__, ret );
    return ret;

}

/*
 * (IF) msm_uart_aux_if_init
 *
 *  @return     0           Success
 *  @return     !0          Error
 */
static int msm_uart_aux_if_init(void)
{
    int ret = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    ret = msm_uart_aux_sw_init();

    pr_debug( DEV_NAME ": %s ret(%d)\n", __func__, ret );
    return ret;

}

/*
 * (IF) msm_uart_aux_if_exit
 *
 *  @return     0           Success
 *  @return     !0          Error
 */
static int msm_uart_aux_if_exit(void)
{
    int ret = 0;

    pr_debug( DEV_NAME ": %s start\n", __func__ );

    ret = msm_uart_aux_sw_exit();

    pr_debug( DEV_NAME ": %s ret(%d)\n", __func__, ret );
    return ret;

}

/* 
 * (IF) msm_uart_aux_if_open
 *
 *  @param      inode       : Not use.
 *  @param      filp        : Check access mode.
 *  @return     0           Success
 *  @return     -EACCES     The requested access to the file is not 
 *                          allowd.
 *  @return     -EMFILE     The system limit of the total number of 
 *                          open files has been reached.
 */
static int msm_uart_aux_if_open(struct inode *inode, struct file *filp)
{
    pr_debug( DEV_NAME ": %s start g_msm_uart_aux_available(%d)\n", __func__, g_msm_uart_aux_available.counter );

    /* check open count */
    if (unlikely(!atomic_dec_and_test(&g_msm_uart_aux_available))) {
        atomic_inc(&g_msm_uart_aux_available);
        pr_err( DEV_NAME ": %s open failed because others have opened.\n", __func__ );
        pr_debug( DEV_NAME ": %s ret(%d)\n", __func__, -EMFILE );
        return -EMFILE;
    }
    
    /* check access mode */
    if (unlikely((filp->f_flags & O_ACCMODE) != O_RDWR)) {
        atomic_inc(&g_msm_uart_aux_available);
        pr_err( DEV_NAME ": %s open failed because invalid mode.\n", __func__ );
        pr_debug( DEV_NAME ": %s ret(%d)\n", __func__, -EACCES );
        return -EACCES;
    }

    pr_debug( DEV_NAME ": %s end g_msm_uart_aux_available(%d)\n", __func__, g_msm_uart_aux_available.counter );
    return 0;
}

/* 
 * (IF) msm_uart_aux_if_close
 *
 *  @param      inode       : Not use.
 *  @param      filp        : Not use.
 *  @return     0           Success
 */
static int msm_uart_aux_if_close(struct inode *inode, struct file *filp)
{
    pr_debug( DEV_NAME ": %s start g_msm_uart_aux_available(%d)\n", __func__, g_msm_uart_aux_available.counter );
    
    /* increment for an atomic variable */
    atomic_inc(&g_msm_uart_aux_available);

    pr_debug( DEV_NAME ": %s end g_msm_uart_aux_available(%d)\n", __func__, g_msm_uart_aux_available.counter );
    return 0;
}

/* 
 * (IF) msm_uart_aux_if_ioctl
 *  
 *  @param      filp        : Not use.
 *  @param      cmd         : Request Code.
 *  @param      arg         : Not use.
 *  @return     0           Success
 *  @return     -ENOTTY     The request type is non-support.
 *  @return     -EINVAL     An invalid request code.
 *  @return     -EIO        An I/O Error.
 *
 */
static long msm_uart_aux_if_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    pr_debug( DEV_NAME ": %s(cmd=%x) start\n", __func__, cmd );

    if (unlikely(_IOC_TYPE(cmd) != MSM_AUX_IOC_TYPE)) {
        pr_err( DEV_NAME ": %s ioctl() TYPE error", __func__ );
        pr_err( DEV_NAME ": %s         IOCTL_CHG_IRDA=0x%x\n", __func__, MSM_AUX_IOCTL_CHG_IRDA );
        pr_err( DEV_NAME ": %s         IOCTL_CHG_UART=0x%x\n", __func__, MSM_AUX_IOCTL_CHG_UART );
        return -ENOTTY;
    }


    switch ( cmd ) {
    case MSM_AUX_IOCTL_CHG_IRDA:
        printk( KERN_INFO DEV_NAME ": %s ioctl(MSM_AUX_IOCTL_CHG_IRDA) set IRDA mode\n", __func__ );
        ret = msm_uart_aux_sw_irda_on();
        if (unlikely(ret < 0)) {
            pr_err( DEV_NAME ": %s ioctl() msm_uart_aux_sw_irda_on failed. ret=%d\n", __func__, ret );
            return ret;
        }
        break;

    case MSM_AUX_IOCTL_CHG_UART:
        printk( KERN_INFO DEV_NAME ": %s ioctl(MSM_AUX_IOCTL_CHG_UART) set UART mode\n", __func__ );
        ret = msm_uart_aux_sw_irda_off();
        if (unlikely(ret < 0)) {
            pr_err( DEV_NAME ": %s ioctl() msm_uart_aux_sw_irda_off failed. ret=%d\n", __func__, ret );
            return ret;
        }
        break;

   default:  /* invalid cmd */
        pr_err( DEV_NAME ": %s ioctl() cmd No. error %d\n", __func__, cmd );
        ret = -EINVAL;
        break;
    } /* end of switch */

    printk( KERN_INFO DEV_NAME ": %s ret = %d\n",__func__, ret );
    return ret;
}

#ifdef SUPPORT_MSM_UART_AUX_MMAP
/* 
 * (IF) msm_uart_aux_if_mmap
 *  
 *  @param      filp        : Not use.
 *  @param      vma         : Not use.
 *  @return     0           Success
 */
int msm_uart_aux_if_mmap(struct file *filp, struct vm_area_struct *vma )
{
    pr_debug( DEV_NAME ": %s called\n", __func__ );
    return 0;
}
#endif /* SUPPORT_MSM_UART_AUX_MMAP */


/* 
 * (INIT) msm_uart_aux_probe
 *  
 *  @param      pdev        : Not use.
 *  @return     0           Success
 */
static int msm_uart_aux_probe(struct platform_device *pdev)
{
    pr_debug( DEV_NAME ": %s called\n", __func__ );
    return 0;
}

/* 
 * (INIT) msm_uart_aux_remove
 *  
 *  @param      pdev        : Not use.
 *  @return     0           Success
 */
static int msm_uart_aux_remove(struct platform_device *pdev)
{
    pr_debug( DEV_NAME ": %s called\n", __func__ );
    return 0;
}


/* 
 * (INIT) msm_uart_aux_init
 *  
 *  @return     0           Success
 *  @return     !0          Error
 */
static int msm_uart_aux_init(void)
{
    int ret = 0;
    dev_t dev = MKDEV( driver_major_no, 0 );

    pr_debug( DEV_NAME ": %s called\n", __func__ );

    /* register class "MSM_UART_AUX" */
    irda_class = class_create( THIS_MODULE, DEV_NAME );
    pr_debug( DEV_NAME ": %s class_create ret(%ld)\n", __func__, IS_ERR( irda_class ) );
    if (unlikely(IS_ERR( irda_class ))) {
        pr_err( DEV_NAME ": %s can't class_create\n", __func__ );
        return PTR_ERR( irda_class );
    }

    /* Register char_dev "MSM_UART_AUX" */
    ret = alloc_chrdev_region( &dev, 0, 2, DEV_NAME );
    pr_debug( DEV_NAME ": %s alloc_chrdev_region ret(%d)\n", __func__, ret );
    driver_major_no = MAJOR(dev);
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s can't set major %d\n", __func__, driver_major_no );
        goto init_error1;
    }
    if (driver_major_no == 0) {
        driver_major_no = ret;
        pr_debug( DEV_NAME ": %s Major no. is assigned to %d.\n", __func__, ret );
    }
    dev = MKDEV( driver_major_no, 0 );
    printk( KERN_INFO  DEV_NAME ": %s UART Major No = %d\n",__func__, driver_major_no );
    cdev_init( &char_dev, &driver_fops );
    char_dev.owner = THIS_MODULE;
    ret = cdev_add( &char_dev, dev, 1 );
    pr_debug( DEV_NAME ": %s cdev_add ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s cdev_add failed\n", __func__ );
        goto init_error2;
    }
    if (unlikely(IS_ERR( device_create( irda_class, NULL, dev, NULL, DEV_NAME ) )))
    {
        pr_err( DEV_NAME ": %s device_create failed\n", __func__ );
        goto init_error3;
    }
    pr_debug( DEV_NAME ": %s device_create completed\n", __func__ );

    ret = msm_uart_aux_if_init();
    pr_debug( DEV_NAME ": %s msm_uart_aux_if_init ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s Load failed. ret = %d\n", __func__, ret );
        goto init_error3;
    }

    /* register driver "MSM_UART_AUX" */
    ret = platform_driver_register(&irda_driver);
    pr_debug( DEV_NAME ": %s platform_driver_register ret(%d)\n", __func__, ret );
    if (unlikely(ret < 0)) {
        pr_err( DEV_NAME ": %s platform_driver_register failed ret = %d\n", __func__, ret );
        goto init_error3;
    }
    
    printk( KERN_INFO  DEV_NAME ": %s ret = %d\n",__func__, ret );
    return ret;

init_error3:
    cdev_del( &char_dev );
init_error2:
    unregister_chrdev_region( dev, 2 );
init_error1:
    class_destroy( irda_class );
    return ret;

}

/* 
 * (INIT) msm_uart_aux_exit
 */
static void msm_uart_aux_exit(void)
{
    int ret = 0;
    dev_t dev = MKDEV( driver_major_no, 0 );

    pr_debug( DEV_NAME ": %s called\n", __func__ );

    platform_driver_unregister( &irda_driver );
    pr_debug( DEV_NAME ": %s platform_driver_unregister call\n", __func__ );

    ret = msm_uart_aux_if_exit();
    pr_debug( DEV_NAME ": %s msm_uart_aux_if_exit ret(%d)\n", __func__, ret );
    if (unlikely(ret)) {
        pr_err( DEV_NAME ": %s if_exit failed. ret = %d\n", __func__, ret );
    }

    cdev_del( &char_dev );
    unregister_chrdev_region( dev, 1 );
    class_destroy( irda_class );

    printk( KERN_INFO  DEV_NAME ": %s Unload normally\n",__func__ );
}

/* <Exported Functionsn>-----------------------------------------------------*/
module_init(msm_uart_aux_init);
module_exit(msm_uart_aux_exit);

MODULE_AUTHOR("FUJITSU");
MODULE_LICENSE( "GPL" );
MODULE_VERSION("1.0.0");
