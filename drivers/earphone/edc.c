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
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/irqreturn.h>
#include <linux/init.h>
#include <linux/ihex.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/spinlock_types.h>
#include <linux/timer.h> 
#include <linux/switch.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/ovp.h>
#include <linux/kthread.h>
#include <linux/syscalls.h> /* add 2013/3/7 */

#include "../arch/arm/mach-msm/board-8064.h"
#include "../arch/arm/include/asm/system_info.h"

#include <linux/export.h>

#include <linux/mm.h>

#include <linux/earphone.h>

#include "edc.h"
#include "edc_hw_type.h"

/**********************************************************************/
/* prototypes		 												  */
/**********************************************************************/
static int edc_if_init(int);
static int edc_if_exit(int);
static int edc_sw_init(int);
static int edc_sw_exit(int);
static int edc_sw_suspend(struct platform_device *pdev, pm_message_t st);
static int edc_sw_resume(struct platform_device *pdev);
static int edc_sw_earphone_status( unsigned long arg );
#if OPT_IOCTL_TEST
static int edc_sw_earphone_status_test( int *arg );
#endif
#if OPT_AUDIO_POLLING
static int edc_sw_earphone_phi35_use( void );
static int edc_sw_earphone_phi35_unuse( void );
static int edc_sw_earphone_switch_use( void );
static int edc_sw_earphone_switch_unuse( void );
static int edc_sw_earphone_mic_push( void );
static int edc_sw_earphone_mic_release( void );
#endif
#if OPT_USB_EARPHONE
static int edc_sw_usb_earphone_use( void );
static int edc_sw_usb_earphone_unuse( void );
#endif
static int edc_sw_earphone_voice_call_start( void );
static int edc_sw_earphone_voice_call_stop( void );
static int edc_sw_earphone_wakeup( void );
int edc_sw_earphone_det_output( void );
int edc_sw_earphone_status_get( unsigned long arg );

int edc_sw_earphone_xled_rst_on( void );
int edc_sw_earphone_xled_rst_off( void );
int edc_sw_earphone_illumi_on( void );
int edc_sw_earphone_illumi_off( void );
static int edc_sw_earphone_headset_mic_use(void);
static int edc_sw_earphone_headset_mic_unuse(void);

static int edc_sw_check_wakelock( void );
static ssize_t edc_sw_msm_headset_print_name(struct switch_dev *sdev_t, char *buf);
#if OPT_PIERCE_SWITCH
static ssize_t edc_sw_msm_pierce1_print_name(struct switch_dev *sdev_t, char *buf);
static ssize_t edc_sw_msm_pierce2_print_name(struct switch_dev *sdev_t, char *buf);
static ssize_t edc_sw_msm_pierce3_print_name(struct switch_dev *sdev_t, char *buf);
#endif
static int edc_sw_earphone_status_read( unsigned long arg );
#if OPT_IOCTL_TEST
static int edc_sw_earphone_status_read_test( int *arg );
#endif

/* FUJITSU st 2013/3/7 */
int edc_ovp_usbearphone_path_set( unsigned int cmd );
/* FUJITSU ed 2013/3/7 */

/**********************************************************************/
/* globals															  */
/**********************************************************************/

/* bit0: Headset  bit1:USB Earphone */
int edc_Headset_Definition_Flag = 0;
edc_handle_t edc_port[EDC_NUM_OF_COUNT];
struct switch_dev edc_headset_dev;
#if OPT_PIERCE_SWITCH
struct switch_dev edc_pierce1_dev;
struct switch_dev edc_pierce2_dev;
struct switch_dev edc_pierce3_dev;
#endif
struct input_dev *edc_ipdev;
struct wake_lock edc_wlock_gpio;	/* suspend control */

int edc_suspend_cansel_flg = 0;
int edc_interrupt_flag=0;
#if OPT_USB_EARPHONE
int edc_UsbEarphoneFlag=0; /* 0:USB earphone off 1:USB earphone on */
#endif
int edc_FaiEarphoneFlag=0;		/* 0:3.5 earphone off 1:3.5 earphone off*/
int edc_Hedset_Switch_enable_Flag=0;/* 0:with no switch  1:with switch */
int edc_mic_bias_flag=0;		/* 0:OFF  1:mic bias on */
int edc_voice_call_flag=0;		/* 0:not talking  1:talking */
int edc_inout_timer_expire=0;	/* 0:no timer 1:timer 2:switch timer */
int edc_backup_value = 0;
int edc_model_pierce = PIERCE_MACHINE_TYPE_NOT;
int edc_base_portstatus=-1;
int edc_open_flag=0; /* add 2013/3/8 */
int edc_board_type=0; /*add 2013/3/14 */
int edc_machine_type=0; /*add 2013/3/14 */
int edc_headset_mic_use_flag=0;
int edc_pierce_retry_flg=0;/*add 2013/5/30 */

static DECLARE_WAIT_QUEUE_HEAD(edc_wq);

static int __devinit earphone_probe(struct platform_device *pdev);
static int __devexit earphone_remove(struct platform_device *pdev);
static int earphone_suspend(struct platform_device *pdev, pm_message_t st);
static int earphone_resume(struct platform_device *pdev);

static struct platform_driver platform_edc_driver = {
	.driver = {
		   .name = "earphone-driver",
		   .owner = THIS_MODULE,
		   },
	.probe 		= earphone_probe,
	.remove		= __devexit_p(earphone_remove),		/* Remove Entry */
	.suspend	= earphone_suspend,					/* Suspend Entry */
	.resume		= earphone_resume,					/* Resume Entry */
};


MODULE_DESCRIPTION("Earphone Device Driver");
MODULE_AUTHOR("Fujitsu");
MODULE_LICENSE("GPL");

static int edc_if_open(struct inode *inode, struct file *filp);
static int edc_if_release(struct inode *inode, struct file *filp);
static long edc_if_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

/* EDC driver FOPS */
static struct file_operations edc_fops = {
	.unlocked_ioctl	= edc_if_ioctl,			/* ioctl Entry */
	.open			= edc_if_open,			/* open Entry */
	.release		= edc_if_release,		/* release Entry */
};

/* driver definition */
static struct miscdevice edc_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,		/* auto */
	.name = "edc",						/* Driver name */
	.fops = &edc_fops,					/* FOPS */
};


#if OPT_IOCTL_TEST
static struct task_struct *edc_tsk;

static int edc_kthred(void *arg)
{
	int  retval=0;
	int  ret=0;
	int  oldval=0;

	printk( "[EDC] TEST THRED START\n");
	
	ret = edc_sw_earphone_status_read_test( &oldval );
	if ( ret < 0 )
	{
		return 0;
	}
	while(1)
	{
		ret = edc_sw_earphone_status_test( &retval );
		printk( "[EDC] TEST oldval = %x retval = %x\n",oldval,retval);
		if ( ret < 0 )
		{
			break;
		}
		else
		{
			if (oldval != retval)
			{
				// headset check
				if ((retval & HEDSET_CHECK_BIT) != (oldval & HEDSET_CHECK_BIT))
				{
					if (retval & HEDSET_CHECK_BIT)
					{
						printk( "[EDC] TEST HEADSET insert \n");
					}
					else
					{
						printk( "[EDC] TEST HEADSET remove \n");
					}
				}
				// MIC switch check
				if ((retval & MIC_SWITCH_CHECK_BIT) != (oldval & MIC_SWITCH_CHECK_BIT))
				{
					if (retval & MIC_SWITCH_CHECK_BIT)
					{
						printk( "[EDC] TEST MIC SWITCH push \n");
					}
					else
					{
						printk( "[EDC] TEST MIC SWITCH release \n");
					}
				}
				// PIERCE_ID_1000_OHM_FLAG
				if ((retval & PIERCE_ID_1000_OHM_CHECK_BIT) != (oldval & PIERCE_ID_1000_OHM_CHECK_BIT))
				{
					if (retval & PIERCE_ID_1000_OHM_CHECK_BIT)
					{
						printk( "[EDC] TEST PIERCE_ID_1000 insert \n");
					}
					else
					{
						printk( "[EDC] TEST PIERCE_ID_1000 remove \n");
					}
				}
				// PIERCE_ID_1500
				if ((retval & PIERCE_ID_1500_OHM_CHECK_BIT) != (oldval & PIERCE_ID_1500_OHM_CHECK_BIT))
				{
					if (retval & PIERCE_ID_1500_OHM_CHECK_BIT)
					{
						printk( "[EDC] TEST PIERCE_ID_1500 insert \n");
					}
					else
					{
						printk( "[EDC] TEST PIERCE_ID_1500 remove \n");
					}
				}
				// PIERCE_ID_2200
				if ((retval & PIERCE_ID_2200_OHM_CHECK_BIT) != (oldval & PIERCE_ID_2200_OHM_CHECK_BIT))
				{
					if (retval & PIERCE_ID_2200_OHM_CHECK_BIT)
					{
						printk( "[EDC] TEST PIERCE_ID_2200 insert \n");
					}
					else
					{
						printk( "[EDC] TEST PIERCE_ID_2200 remove \n");
					}
				}
				oldval= retval;
			}
		}
	}

	return 0;
}
#endif

/**********************************************************************/
/* earphone_init													  */
/**********************************************************************/
static __init int earphone_init(void)
{
	int	ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	ret = platform_driver_register(&platform_edc_driver);

	if( ret ){
		printk("[EDC] ERROR(%s):platform_driver_register() (%d)\n",__func__,ret);
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}
module_init(earphone_init);

/**********************************************************************/
/* earphone_exit													  */
/**********************************************************************/
static __exit void earphone_exit(void)
{
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	platform_driver_unregister(&platform_edc_driver);

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

}
module_exit(earphone_exit);

/**********************************************************************/
/* earphone_probe													  */
/**********************************************************************/
static int __devinit earphone_probe(struct platform_device *pdev)
{
	int ret = EDC_OK;
	int rc = 0;
	struct input_dev *ipdevtmp;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	memset(edc_port, 0, sizeof(edc_port));
	EDC_DBG_PRINTK("[EDC] earphone work size %d\n", sizeof(edc_port));

	/* MACHINE_TYPE get */
	/* FJDEV001(Ca) */
	/* FJDEV002(Lu) */
	/* FJDEV004(Th) */
	/* FJDEV005(Hi) */
	edc_machine_type = (system_rev & 0xF0);
	printk("[EDC] MACHINE_TYPE = %x\n",edc_machine_type );

	/* BOARD_TYPE get */
	edc_board_type =  (system_rev & 0x0F);
	printk("[EDC] BOARD_TYPE = %x\n",edc_board_type );

	/* PIERCE_TYPE jugde */
	edc_model_pierce = PIERCE_MACHINE_TYPE_NOT;
	if( edc_machine_type == EDC_MACHINE_TYPE_FJDEV002 ){
		printk("[EDC] PIERCE_MACHINE_TYPE (Y)\n");
		edc_model_pierce = PIERCE_MACHINE_TYPE;
	}
	else if( edc_machine_type == EDC_MACHINE_TYPE_FJDEV001 ){
		if( edc_board_type >=  EDC_BOARD_TYPE_FJDEV001_2_3 ){
			printk("[EDC] PIERCE_MACHINE_TYPE (N)\n");
			edc_model_pierce = PIERCE_MACHINE_TYPE_NOT;
		}
		else{
			printk("[EDC] PIERCE_MACHINE_TYPE (Y)\n");
			edc_model_pierce = PIERCE_MACHINE_TYPE;
		}
	}
	else if( edc_machine_type == EDC_MACHINE_TYPE_FJDEV004 || 
			 edc_machine_type == EDC_MACHINE_TYPE_FJDEV005){
		printk("[EDC] PIERCE_MACHINE_TYPE (N)\n");
		edc_model_pierce = PIERCE_MACHINE_TYPE_NOT;
	}
	else{
		printk("[EDC] ERROR(%s):edc_machine_type (%x)\n",__func__,edc_machine_type );
		return EDC_NG;
	}

	edc_headset_mic_use_flag=0;
	edc_pierce_retry_flg=0;

	/* ******* switch file headset Initial Setting Start */
	edc_headset_dev.name	= "h2w";
	edc_headset_dev.print_name = edc_sw_msm_headset_print_name;

	rc = switch_dev_register(&edc_headset_dev);
	if (rc)
	{
		printk("[EDC] ERROR(%s):headset switch_dev_register() (%d)\n",__func__,rc );
		goto err_headset_switch_dev_register;
	}
	/* ******* switch file headset Initial Setting End */

#if	OPT_PIERCE_SWITCH

	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){

		/* ******* switch file pierce1 Initial Setting Start */
		edc_pierce1_dev.name	= "pierce1_h2w";
		edc_pierce1_dev.print_name = edc_sw_msm_pierce1_print_name;

		rc = switch_dev_register(&edc_pierce1_dev);
		if (rc)
		{
			printk("[EDC] ERROR(%s):pierce1 switch_dev_register() (%d)\n",__func__,rc );
			goto err_pierce1_switch_dev_register;
		}
		printk("[EDC] (%s) pierce1 switch_dev_register() OK\n",__func__ );
		/* ******* switch file pierce1 Initial Setting End */

		/* ******* switch file pierce2 Initial Setting Start */
		edc_pierce2_dev.name	= "pierce2_h2w";
		edc_pierce2_dev.print_name = edc_sw_msm_pierce2_print_name;

		rc = switch_dev_register(&edc_pierce2_dev);
		if (rc)
		{
			printk("[EDC] ERROR(%s):pierce2 switch_dev_register() (%d)\n",__func__,rc );
			goto err_pierce2_switch_dev_register;
		}
		printk("[EDC] (%s) pierce2 switch_dev_register() OK\n",__func__ );
		/* ******* switch file pierce2 Initial Setting End */

		/* ******* switch file pierce3 Initial Setting Start */
		edc_pierce3_dev.name	= "pierce3_h2w";
		edc_pierce3_dev.print_name = edc_sw_msm_pierce3_print_name;

		rc = switch_dev_register(&edc_pierce3_dev);
		if (rc)
		{
			printk("[EDC] ERROR(%s):pierce3 switch_dev_register() (%d)\n",__func__,rc );
			goto err_pierce3_switch_dev_register;
		}
		printk("[EDC] (%s) pierce3 switch_dev_register() OK\n",__func__ );
		/* ******* switch file pierce3 Initial Setting End */
	}
	
#endif

	/* ******* input device Setting Start */
	ipdevtmp = input_allocate_device();
	if (!ipdevtmp) {
		printk("[EDC] ERROR(%s):input_allocate_device() \n",__func__);
		rc = -ENOMEM;
		goto err_alloc_input_dev;
	}
	edc_ipdev = ipdevtmp;
	set_bit(EV_KEY, edc_ipdev->evbit);
	set_bit(KEY_POWER, edc_ipdev->keybit);
	edc_ipdev->name= "edc";
	edc_ipdev->id.vendor	= 0x0001;
	edc_ipdev->id.product	= 1;
	edc_ipdev->id.version	= 1;

	input_set_capability(edc_ipdev, EV_KEY, KEY_MEDIA);
	input_set_capability(edc_ipdev, EV_KEY, KEY_POWER);

	rc = input_register_device(edc_ipdev);
	if (rc) {
		printk("[EDC] ERROR(%s):input_register_device() (%d)\n",__func__,rc);
		dev_err(&edc_ipdev->dev,
				"hs_probe: input_register_device rc=%d\n", rc);
		goto err_reg_input_dev;
	}

	edc_if_init(EARPHONE_PROBE_STAGE_0);

	ret = misc_register(&edc_miscdev);
	if (ret) {
		printk("[EDC] ERROR(%s):misc_register() (%d)\n",__func__,ret);
		goto err_reg_input_dev;
	}

    wake_lock_init(&edc_wlock_gpio, WAKE_LOCK_SUSPEND, "wlock_gpio");

	ret = edc_if_init(EARPHONE_PROBE_STAGE_1);
	if (ret) {
		printk("[EDC] ERROR(%s):edc_if_init() (%d)\n",__func__,ret);
		goto err_setup_conf;
	}

	edc_if_init(EARPHONE_PROBE_STAGE_2);

	edc_if_init(EARPHONE_PROBE_STAGE_3);

#if OPT_IOCTL_TEST
	edc_tsk = kthread_create(edc_kthred, NULL, "edc_kthred");
	if (IS_ERR(edc_tsk)) {
		ret = PTR_ERR(edc_tsk);
		edc_tsk = NULL;
		return EDC_NG;
	}
	wake_up_process(edc_tsk);
#endif

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	/* ovp_device_initialized(INITIALIZE_EARPHONE); */

	return ret;

err_setup_conf:
	misc_deregister(&edc_miscdev);
err_reg_input_dev:
	input_free_device(edc_ipdev);
err_alloc_input_dev:

#if	OPT_PIERCE_SWITCH
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		switch_dev_unregister(&edc_pierce3_dev);
	}
err_pierce3_switch_dev_register:
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		switch_dev_unregister(&edc_pierce2_dev);
	}
err_pierce2_switch_dev_register:
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		switch_dev_unregister(&edc_pierce1_dev);
	}
err_pierce1_switch_dev_register:
#endif

	switch_dev_unregister(&edc_headset_dev);
err_headset_switch_dev_register:
	return rc;
}

/**********************************************************************/
/* earphone_remove													  */
/**********************************************************************/
static int __devexit earphone_remove(struct platform_device *pdev)
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

#if OPT_IOCTL_TEST
	kthread_stop(edc_tsk);
#endif
	
	edc_if_exit( EARPHONE_REMOVE_STAGE_0 );

	misc_deregister(&edc_miscdev);
	input_unregister_device(edc_ipdev);
	switch_dev_unregister(&edc_headset_dev);
	
#if	OPT_PIERCE_SWITCH
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		switch_dev_unregister(&edc_pierce1_dev);
		switch_dev_unregister(&edc_pierce2_dev);
		switch_dev_unregister(&edc_pierce3_dev);
	}
#endif

	edc_if_exit( EARPHONE_REMOVE_STAGE_1 );

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* earphone_suspend													  */
/**********************************************************************/
static int earphone_suspend(struct platform_device *pdev, pm_message_t st)
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/

	ret = edc_sw_suspend( pdev , st );

	if( ret ){
		/*printk("[EDC] ERROR(%s):edc_sw_suspend() (%d)\n",__func__,ret);*/
		return ret;
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);*/

	return ret;
}

/**********************************************************************/
/* earphone_resume													  */
/**********************************************************************/
static int earphone_resume(struct platform_device *pdev)
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/

	ret = edc_sw_resume( pdev );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_sw_resume() (%d)\n",__func__,ret);
		return ret;
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);*/

	return ret;
}


/**********************************************************************/
/* edc_if_init													  	  */
/**********************************************************************/
static int edc_if_init(int edc_stage)
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()[%d]\n", __func__,edc_stage);

	ret = edc_sw_init( edc_stage );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_sw_init() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}


/**********************************************************************/
/* edc_if_exit													  	  */
/**********************************************************************/
static int edc_if_exit(int edc_stage)
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()[%d]\n", __func__,edc_stage);

	ret = edc_sw_exit( edc_stage );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_sw_exit() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_if_open													  	  */
/**********************************************************************/
static int edc_if_open(struct inode *inode, struct file *filp)
{
	int ret=EDC_OK;

	if( edc_open_flag ){
		printk( "[EDC] %s err\n",__func__);
		return -1;
	}

	edc_open_flag = 1;

	EDC_DBG_LOOP_PRINTK("[EDC] %s()\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_if_release													  */
/**********************************************************************/
static int edc_if_release(struct inode *inode, struct file *filp)
{
	int ret=EDC_OK;

	if( edc_open_flag == 0 ){
		printk( "[EDC] %s err\n",__func__);
		return -1;
	}

	edc_open_flag = 0;

	EDC_DBG_LOOP_PRINTK("[EDC] %s()\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_if_ioctl													  	  */
/**********************************************************************/
static long edc_if_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret=EDC_OK;

	if( cmd == EARPHONE_ILLUMI_ON || cmd == EARPHONE_ILLUMI_OFF ){
		EDC_DBG_LOOP_PRINTK("[EDC] %s()[cmd=%d]\n",__func__,cmd);
	}
	else{
		EDC_DBG_PRINTK("[EDC] %s()[cmd=%d]\n",__func__,cmd);
	}

	switch( cmd ) 
	{
		case EARPHONE_STATUS :
			ret = edc_sw_earphone_status( arg );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_status() (%d)\n",__func__,ret );
			break;
#if OPT_AUDIO_POLLING
		case EARPHONE_PHI35_USE :
			ret = edc_sw_earphone_phi35_use( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_phi35_use() (%d)\n",__func__,ret );
			break;

		case EARPHONE_PHI35_UNUSE :
			ret = edc_sw_earphone_phi35_unuse( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_phi35_unuse() (%d)\n",__func__,ret );
			break;

		case EARPHONE_SWITCH_USE:
			ret = edc_sw_earphone_switch_use( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_switch_use() (%d)\n",__func__,ret );
			break;

		case EARPHONE_SWITCH_UNUSE:
			ret = edc_sw_earphone_switch_unuse( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_switch_unuse() (%d)\n",__func__,ret );
			break;

		case EARPHONE_MIC_PUSH:
			ret = edc_sw_earphone_mic_push( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_mic_push() (%d)\n",__func__,ret );
			break;

		case EARPHONE_MIC_RELESE:
			ret = edc_sw_earphone_mic_release( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_mic_release() (%d)\n",__func__,ret );
			break;
#endif
#if OPT_USB_EARPHONE
		case USB_EARPHONE_USE :
			ret = edc_sw_usb_earphone_use( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_usb_earphone_use() (%d)\n",__func__,ret );
			break;
#endif
#if OPT_USB_EARPHONE
		case USB_EARPHONE_UNUSE :
			ret = edc_sw_usb_earphone_unuse( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_usb_earphone_unuse() (%d)\n",__func__,ret );
			break;
#endif
		case EARPHONE_VOICE_CALL_START:
			ret = edc_sw_earphone_voice_call_start( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_voice_call_start() (%d)\n",__func__,ret );
			break;

		case EARPHONE_VOICE_CALL_STOP:
			ret = edc_sw_earphone_voice_call_stop( );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_voice_call_stop() (%d)\n",__func__,ret );
			break;

		case EARPHONE_STATUS_READ :
			ret = edc_sw_earphone_status_read( arg );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_status_read() (%d)\n",__func__,ret );
			break;

		case EARPHONE_WAKE_UP:
			ret = edc_sw_earphone_wakeup();
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_wakeup() (%d)\n",__func__,ret );
			break;

		case EARPHONE_DET_OUTPUT :
			ret = edc_sw_earphone_det_output();
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_det_output() (%d)\n",__func__,ret );
			break;

		case EARPHONE_STATUS_GET :
			ret = edc_sw_earphone_status_get( arg );
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_status_get() (%d)\n",__func__,ret );
			break;

		/* TEST */
		/*
		case EARPHONE_PIERCE_USE :
			printk("[EDC]:(%s) EARPHONE_PIERCE_USE \n",__func__ );
			break;

		case EARPHONE_PIERCE_UNUSE :
			printk("[EDC] (%s) EARPHONE_PIERCE_UNUSE\n",__func__ );
			break;
		*/

		case EARPHONE_KO_XLED_RST_ON :
			ret = edc_sw_earphone_xled_rst_on();
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_xled_rst_on() (%d)\n",__func__,ret );
			break;

		case EARPHONE_KO_XLED_RST_OFF :
			ret = edc_sw_earphone_xled_rst_off();
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_xled_rst_off() (%d)\n",__func__,ret );
			break;

		case EARPHONE_ILLUMI_ON :
			ret = edc_sw_earphone_illumi_on();
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_illumi_on() (%d)\n",__func__,ret );
			break;

		case EARPHONE_ILLUMI_OFF :
			ret = edc_sw_earphone_illumi_off();
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_illumi_off() (%d)\n",__func__,ret );
			break;

		case EARPHONE_HEADSET_MIC_USE :
			ret = edc_sw_earphone_headset_mic_use();
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_headset_mic_use() (%d)\n",__func__,ret );
			break;

		case EARPHONE_HEADSET_MIC_UNUSE :
			ret = edc_sw_earphone_headset_mic_unuse();
			if( ret != EDC_OK )
				printk("[EDC] ERROR(%s):edc_sw_earphone_headset_mic_unuse() (%d)\n",__func__,ret );
			break;
		
		default:
			printk("[EDC] ERROR(%s):(cmd=%d)\n",__func__,cmd );
			return -EINVAL;
	}

	if( ret == EDC_OK ){
		if( cmd == EARPHONE_ILLUMI_ON || cmd == EARPHONE_ILLUMI_OFF ){
			EDC_DBG_LOOP_PRINTK("[EDC] %s() OK\n", __func__);
		}
		else{
			EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
		}
	}
	
	return ret;
}

/**********************************************************************/
/* edc_usb_insert_detect											  */
/**********************************************************************/
void edc_usb_insert_detect(void)
{
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

#if OPT_USB_EARPHONE
	edc_Headset_Definition_Flag |= USB_EARPHONE_CHECK_BIT;
	edc_interrupt_flag++;
	edc_Hedset_Switch_enable_Flag=0;
	wake_lock(&edc_wlock_gpio);

	wake_up(&edc_wq);
	EDC_DBG_PRINTK("[EDC] HEADPHONE: edc_Headset_Definition_Flag %08x\n", edc_Headset_Definition_Flag);
#endif

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
}

EXPORT_SYMBOL_GPL(edc_usb_insert_detect);

/**********************************************************************/
/* edc_usb_remove_detect											  */
/**********************************************************************/
void edc_usb_remove_detect(void)
{
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

#if OPT_USB_EARPHONE
	edc_Headset_Definition_Flag &= ~USB_EARPHONE_CHECK_BIT;
	edc_Hedset_Switch_enable_Flag=0;

	edc_interrupt_flag++;
	wake_lock(&edc_wlock_gpio);
	wake_up(&edc_wq);
	EDC_DBG_PRINTK("[EDC] HEADPHONE: edc_Headset_Definition_Flag %08x\n", edc_Headset_Definition_Flag);
#endif

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);
}

EXPORT_SYMBOL_GPL(edc_usb_remove_detect);

/**********************************************************************/
/* edc_sw_init													  	  */
/**********************************************************************/
static int edc_sw_init( int edc_stage )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()[%d]\n", __func__,edc_stage);

	if( edc_stage == EARPHONE_PROBE_STAGE_0 ){
		init_waitqueue_head(&edc_wq);
	}
	else if( edc_stage == EARPHONE_PROBE_STAGE_1 ){
		ret = edc_hw_init( );
		if( ret ){
			printk("[EDC] ERROR(%s):edc_hw_init() (%d)\n",__func__,ret);
			return ret;
		}
	}
	else if( edc_stage == EARPHONE_PROBE_STAGE_2 ){
		/* check the current status */
		edc_port[EDC_PLUG].Polling_Counter=0;
		edc_port[EDC_SWITCH].Polling_Counter=0;
		edc_port[EDC_PLUG].timer_flag = 0;
		edc_port[EDC_SWITCH].timer_flag = 0;
	}
	else if( edc_stage == EARPHONE_PROBE_STAGE_3 ){
		edc_hw_chedule_delayed_work_que_loop( );
	}
	else{
		printk("[EDC] ERROR(%s):edc_stage (%d)\n",__func__,edc_stage );
 		return EDC_NG;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_exit													  	  */
/**********************************************************************/
static int edc_sw_exit( int edc_stage )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	if( edc_stage == EARPHONE_REMOVE_STAGE_0 ){
		ret = edc_hw_exit( );
		if( ret ){
			printk("[EDC] ERROR(%s):edc_hw_exit() (%d)\n",__func__,ret );
			return ret;
		}
	}
	else if( edc_stage == EARPHONE_REMOVE_STAGE_1 ){
		wake_lock_destroy(&edc_wlock_gpio);
		del_timer( &edc_port[EDC_PLUG].timer );
		del_timer( &edc_port[EDC_SWITCH].timer );
	}
	else{
		printk("[EDC] ERROR(%s):edc_stage (%d)\n",__func__,edc_stage );
		return EDC_NG;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
	
	return ret;
}

/**********************************************************************/
/* edc_sw_suspend													  */
/**********************************************************************/
static int edc_sw_suspend(struct platform_device *pdev, pm_message_t st)
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/

	ret = edc_hw_suspend( );

	if( ret ){
		/*printk("[EDC] ERROR(%s):edc_hw_suspend() (%d)\n",__func__,ret );*/
		return ret;
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);*/

	return ret;
}

/**********************************************************************/
/* edc_sw_resume													  */
/**********************************************************************/
static int edc_sw_resume(struct platform_device *pdev)
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/

	ret = edc_hw_resume( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_resume() (%d)\n",__func__,ret);
		return ret;
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);*/

	return ret;
}
/**********************************************************************/
/* edc_sw_earphone_status											  */
/**********************************************************************/
static int edc_sw_earphone_status( unsigned long arg )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	wait_event_interruptible(edc_wq,  edc_interrupt_flag || (edc_backup_value != edc_Headset_Definition_Flag));

	EDC_DBG_PRINTK("[EDC] EARPHONE_STATUS Start %d\n", edc_port[EDC_PLUG].Polling_Counter);

	edc_backup_value = edc_Headset_Definition_Flag;

	ret = copy_to_user((char __user *)arg, &edc_backup_value, sizeof(int));

	edc_interrupt_flag = 0;
	edc_sw_check_wakelock();

	if( ret ){
		printk("[EDC] ERROR(%s):copy_to_user() (%d)\n",__func__,ret);
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}
#if OPT_IOCTL_TEST
/**********************************************************************/
/* edc_sw_earphone_status_test										  */
/**********************************************************************/
static int edc_sw_earphone_status_test( int *arg )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	wait_event_interruptible(edc_wq,  edc_interrupt_flag || (edc_backup_value != edc_Headset_Definition_Flag));

	EDC_DBG_PRINTK("[EDC] EARPHONE_STATUS Start %d\n", edc_port[EDC_PLUG].Polling_Counter);

	edc_backup_value = edc_Headset_Definition_Flag;

	/* ret = copy_to_user((char __user *)arg, &edc_backup_value, sizeof(int)); */
	*arg = edc_backup_value;
	
	edc_interrupt_flag = 0;
	edc_sw_check_wakelock();

	/*if( ret ){
		printk("[EDC] ERROR(%s):copy_to_user() (%d)\n",__func__,ret);
		return ret;
	}*/

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}
#endif
#if OPT_AUDIO_POLLING
/**********************************************************************/
/* edc_sw_earphone_phi35_use										  */
/**********************************************************************/
static int edc_sw_earphone_phi35_use( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	ret = edc_hw_earphone_phi35_use( );
	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_phi35_use() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_phi35_unuse										  */
/**********************************************************************/
static int edc_sw_earphone_phi35_unuse( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	ret = edc_hw_earphone_phi35_unuse( );
	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_phi35_unuse() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_switch_use										  */
/**********************************************************************/
static int edc_sw_earphone_switch_use( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	ret = edc_hw_earphone_switch_use( );
	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_switch_use() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_switch_unuse										  */
/**********************************************************************/
static int edc_sw_earphone_switch_unuse( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	ret = edc_hw_earphone_switch_unuse( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_switch_unuse() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_mic_push											  */
/**********************************************************************/
static int edc_sw_earphone_mic_push( void )
{
	int ret=EDC_OK;
	
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	ret = edc_hw_earphone_mic_push( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_mic_push() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_mic_release										  */
/**********************************************************************/
static int edc_sw_earphone_mic_release( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	ret = edc_hw_earphone_mic_release( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_mic_release() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}
#endif
#if OPT_USB_EARPHONE
/**********************************************************************/
/* edc_sw_usb_earphone_use	  										  */
/**********************************************************************/
static int edc_sw_usb_earphone_use( void )
{
	int ret=EDC_OK;
	int timer_active = 0;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	ret = edc_hw_usb_earphone_use( EARPHONE_USB_STAGE_0 );
	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_usb_earphone_use() (%d)\n",__func__,ret );
		return ret;
	}

	edc_UsbEarphoneFlag=1;
	if (edc_FaiEarphoneFlag)
	{
		/* No Device Notification */
		switch_set_state(&edc_headset_dev, 0x0000);
		printk("[EDC] switch_set_state 0\n");
		msleep(100);
	}
	/* Headhone Notification */
	switch_set_state(&edc_headset_dev, 0x0001);
	printk("[EDC] switch_set_state 1\n");
	msleep(100);
	/* USB earphone switch re-detection */
	edc_Hedset_Switch_enable_Flag=0;
	edc_port[EDC_SWITCH].Polling_Counter=0;
	edc_port[EDC_SWITCH].timer_flag = 0;
	/* Fix timer interrupt control start */
	timer_active = timer_pending(&edc_port[EDC_SWITCH].timer);
	if(timer_active == 1)
	{
		printk("[EDC] USB_EARPHONE_USE timer");
		del_timer_sync( &edc_port[EDC_SWITCH].timer );
	}
	/* Fix timer interrupt control end */
	edc_hw_usb_earphone_use( EARPHONE_USB_STAGE_1 );
	edc_sw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_usb_earphone_unuse										  */
/**********************************************************************/
static int edc_sw_usb_earphone_unuse( void )
{
	int ret=EDC_OK;
	int timer_active = 0;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	/* No Device Notification */
	switch_set_state(&edc_headset_dev, 0x0000);
	printk("[EDC] switch_set_state 0\n");
	if (edc_FaiEarphoneFlag)
	{
		msleep(200);
		ret = edc_hw_usb_earphone_unuse( EARPHONE_USB_STAGE_0 );
		if( ret ){
			printk("[EDC] ERROR(%s):edc_hw_usb_earphone_unuse() (%d)\n",__func__,ret );
			return ret;
		}
		/* Headhone Notification */
		switch_set_state(&edc_headset_dev, 0x0001);
		printk("[EDC] switch_set_state 1\n");
		/* USB earphone switch re-detection */
		edc_Hedset_Switch_enable_Flag=0;
		edc_port[EDC_SWITCH].Polling_Counter=0;
		edc_port[EDC_SWITCH].timer_flag = 0;

		timer_active = timer_pending(&edc_port[EDC_SWITCH].timer);
		if(timer_active == 1)
		{
			printk("[EDC] USB_EARPHONE_UNUSE timer");
			del_timer_sync( &edc_port[EDC_SWITCH].timer );
		}
		/* timer interrupt control end */
		edc_hw_usb_earphone_unuse( EARPHONE_USB_STAGE_1 );
	}
	edc_UsbEarphoneFlag=0;
	msleep(200);
	edc_sw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}
#endif

/**********************************************************************/
/* edc_sw_earphone_voice_call_start									  */
/**********************************************************************/
static int edc_sw_earphone_voice_call_start( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	edc_voice_call_flag = 1;

	edc_sw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_voice_call_stop			  						  */
/**********************************************************************/
static int edc_sw_earphone_voice_call_stop( void )
{
	int ret=EDC_OK ;
	
	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	edc_voice_call_flag = 0;

	ret = edc_hw_earphone_voice_call_stop( );
	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_voice_call_stop() (%d)\n",__func__,ret );
		return ret;
	}

	edc_sw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_wakeup											  */
/**********************************************************************/
static int edc_sw_earphone_wakeup( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	input_report_key(edc_ipdev, KEY_POWER, 1);
	input_sync(edc_ipdev);
	msleep(200);
	input_report_key(edc_ipdev, KEY_POWER, 0);
	input_sync(edc_ipdev);

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
	
	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_det_output										  */
/**********************************************************************/
int edc_sw_earphone_det_output( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	ret = edc_hw_earphone_det_output( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_det_output() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);
	
	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_status_get										  */
/**********************************************************************/
int edc_sw_earphone_status_get( unsigned long arg )
{
	int ret=EDC_OK;
	struct earphone_status earphone_read_status;
	int error=0;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	memset(&earphone_read_status, 0, sizeof(earphone_read_status));

	ret = edc_hw_earphone_status_get( &earphone_read_status );
	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_status_get() (%d)\n",__func__,ret );
		return ret;
	}

	printk("[EDC] [MC HEAD SET]\n");
	printk("[EDC] plug_status = %d\n", earphone_read_status.plug_status);
	printk("[EDC] sw_status   = %d\n", earphone_read_status.sw_status);
	printk("[EDC] plug_vol[0] = %0x\n", earphone_read_status.plug_vol[0]);
	printk("[EDC] plug_vol[1] = %0x\n", earphone_read_status.plug_vol[1]);
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		printk("[EDC] pierce_status = %d\n", earphone_read_status.pierce_status);
		printk("[EDC] pierce_vol[0] = %0x\n", earphone_read_status.pierce_vol[0]);
		printk("[EDC] pierce_vol[1] = %0x\n", earphone_read_status.pierce_vol[1]);
	}
	
	error = copy_to_user((char __user *)arg, &earphone_read_status, sizeof(earphone_read_status));
	edc_hw_chedule_delayed_work_que_loop( );

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_check_wakwlock											  */
/**********************************************************************/
static int edc_sw_check_wakelock( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	ret = edc_hw_check_wakelock( );
	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_check_wakelock() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_msm_headset_print_name									  */
/**********************************************************************/
static ssize_t edc_sw_msm_headset_print_name(struct switch_dev *sdev_t, char *buf)
{
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	switch (switch_get_state(&edc_headset_dev)) 
	{
	case EDC_NO_DEVICE:
		EDC_DBG_PRINTK("[EDC] No Device \n");
		return sprintf(buf, "No Device\n");
	case EDC_DEVICE:
#if OPT_USB_EARPHONE
		if (edc_Headset_Definition_Flag & USB_EARPHONE_CHECK_BIT) {
			EDC_DBG_PRINTK("[EDC] UsbHeadset\n");
			return sprintf(buf, "UsbHeadset\n");
		}
		else {
			EDC_DBG_PRINTK("[EDC] Headset\n");
			return sprintf(buf, "Headset\n");
		}
#else
		EDC_DBG_PRINTK("[EDC] Headset\n");
		return sprintf(buf, "Headset\n");
#endif
	}
	return -EINVAL;
}

#if	OPT_PIERCE_SWITCH 
/**********************************************************************/
/* edc_sw_msm_pierce1_print_name										  */
/**********************************************************************/
static ssize_t edc_sw_msm_pierce1_print_name(struct switch_dev *sdev_t, char *buf)
{
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	switch (switch_get_state(&edc_pierce1_dev)) 
	{
	case EDC_NO_DEVICE:
		EDC_DBG_PRINTK("[EDC] No Device \n");
		return sprintf(buf, "No Device\n");
	case EDC_DEVICE:
		EDC_DBG_PRINTK("[EDC] Pierce1\n");
		return sprintf(buf, "Pierce1\n");
	}
	return -EINVAL;
}

/**********************************************************************/
/* edc_sw_msm_pierce2_print_name										  */
/**********************************************************************/
static ssize_t edc_sw_msm_pierce2_print_name(struct switch_dev *sdev_t, char *buf)
{
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	switch (switch_get_state(&edc_pierce2_dev)) 
	{
	case EDC_NO_DEVICE:
		EDC_DBG_PRINTK("[EDC] No Device \n");
		return sprintf(buf, "No Device\n");
	case EDC_DEVICE:
		EDC_DBG_PRINTK("[EDC] Pierce2\n");
		return sprintf(buf, "Pierce2\n");
	}
	return -EINVAL;
}

/**********************************************************************/
/* edc_sw_msm_pierce3_print_name										  */
/**********************************************************************/
static ssize_t edc_sw_msm_pierce3_print_name(struct switch_dev *sdev_t, char *buf)
{
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	switch (switch_get_state(&edc_pierce3_dev)) 
	{
	case EDC_NO_DEVICE:
		EDC_DBG_PRINTK("[EDC] No Device \n");
		return sprintf(buf, "No Device\n");
	case EDC_DEVICE:
		EDC_DBG_PRINTK("[EDC] Pierce3\n");
		return sprintf(buf, "Pierce3\n");
	}
	return -EINVAL;
}

#endif

/**********************************************************************/
/* edc_sw_earphone_status_read										  */
/**********************************************************************/
static int edc_sw_earphone_status_read( unsigned long arg )
{
	int error = 0;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	error = copy_to_user((char __user *)arg, &edc_backup_value, sizeof(int));

	return error;
}

#if OPT_IOCTL_TEST
/**********************************************************************/
/* edc_sw_earphone_status_read_test									  */
/**********************************************************************/
static int edc_sw_earphone_status_read_test( int *arg )
{
	int error = 0;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	/* error = copy_to_user((char __user *)arg, &edc_backup_value, sizeof(int)); */
	*arg = edc_backup_value;
	
	return error;
}
#endif

void edc_hw_wake_up_interruptible(void)
{
	wake_up_interruptible(&edc_wq);
}

/* FUJITSU st 2013/3/7 */
/**********************************************************************/
/* edc_ovp_usbearphone_path_set									      */
/**********************************************************************/
int	edc_ovp_usbearphone_path_set( unsigned int cmd )
{
	int ret=EDC_OK;
	int fd;
	unsigned long val=0;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	if ((fd = sys_open("/dev/ovp", O_RDONLY, 0)) < 0) {
		printk( "[EDC] %s /dev/ovp sys_open err (%d)\n",__func__,fd);
		return EDC_NG;
	}
	ret = sys_ioctl( fd , cmd , (unsigned long)&val);
	if( ret < 0 ){
		printk( "[EDC] %s /dev/ovp sys_ioctl err (%d)\n",__func__,ret);
		return EDC_NG;
	}
	sys_close(fd);

	return EDC_OK;
}
/* FUJITSU ed 2013/3/7 */

/**********************************************************************/
/* edc_sw_earphone_xled_rst_on										  */
/**********************************************************************/
int edc_sw_earphone_xled_rst_on( void )
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n",__func__);*/

	ret = edc_hw_earphone_xled_rst_on( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_xled_rst_on() (%d)\n",__func__,ret );
		return ret;
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);*/

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_xled_rst_off										  */
/**********************************************************************/
int edc_sw_earphone_xled_rst_off( void )
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n",__func__);*/

	ret = edc_hw_earphone_xled_rst_off( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_xled_rst_off() (%d)\n",__func__,ret );
		return ret;
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);*/

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_illumi_on										  */
/**********************************************************************/
int edc_sw_earphone_illumi_on( void )
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n",__func__);*/

	ret = edc_hw_earphone_illumi_on( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_illumi_on() (%d)\n",__func__,ret );
		return ret;
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);*/

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_illumi_off										  */
/**********************************************************************/
int edc_sw_earphone_illumi_off( void )
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n",__func__);*/

	ret = edc_hw_earphone_illumi_off( );

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_earphone_illumi_off() (%d)\n",__func__,ret );
		return ret;
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);*/

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_headset_mic_use									  */
/**********************************************************************/
static int edc_sw_earphone_headset_mic_use( void )
{
	int ret=EDC_OK;

	edc_headset_mic_use_flag = 1;

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_sw_earphone_headset_mic_unuse			  					  */
/**********************************************************************/
static int edc_sw_earphone_headset_mic_unuse( void )
{
	int ret=EDC_OK ;

	edc_headset_mic_use_flag = 0;

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}