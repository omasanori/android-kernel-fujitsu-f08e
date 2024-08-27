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
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/mpp.h>
#include <linux/leds-pm8xxx.h>
#include "../arch/arm/mach-msm/board-8064.h"
#include <linux/export.h>
#include "../arch/arm/mach-msm/include/mach/board_fj.h"

#include <linux/mm.h>

#include <linux/earphone.h>

#include <linux/kthread.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <mach/msm_smsm.h>
#include <linux/irq.h> /* Earphone interupt enable */
#include <linux/nonvolatile_common.h> /* nv */

#include "edc.h"
#include "edc_hw.h"
#include "edc_hw_type.h"
#include "../../../sound/soc/codecs/mc_yamaha/mc_asoc_priv.h"
#include "../../../sound/soc/codecs/mc_yamaha/mcdriver.h"

#include <linux/ovp.h> /* add 2013/3/7 */

/* FUJITSU st 2013/3/7 */
extern int edc_ovp_usbearphone_path_set( unsigned int cmd );
/* FUJITSU ed 2013/3/7 */

extern int ymu_control(int scene, int param);

static int edc_chatt_cnt = 0;
static int edc_mic_pierce = EARPHONE_NOT;

static int pierce_id_1000_ohm_low  = 0;
static int pierce_id_1000_ohm_high = 0;
static int pierce_id_1500_ohm_low  = 0;
static int pierce_id_1500_ohm_high = 0;
static int pierce_id_2200_ohm_low  = 0;
static int pierce_id_2200_ohm_high = 0;

/* earphone detect skip add start */
/* extern unsigned int mdm_powoff_get(void); */

static int edc_state_num = EDC_STATE_INIT;

int edc_state_matrix[EDC_STATE_MAX][EDC_STATE_MAX]={
#if 0
{ 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},  /* INT */
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* LOOP */
{-1, 0, 0,-1,-1,-1,-1,-1,-1, 0,-1},  /* CHATT */
{-1,-1, 0,-1,-1,-1,-1,-1,-1, 0,-1},  /* DESITION */
{-1,-1,-1, 0,-1,-1, 0,-1,-1, 0,-1},  /* LOOP PIERCE */
{-1,-1,-1,-1, 0, 0,-1,-1,-1, 0,-1},  /* CHATT PIERCE */
{-1,-1,-1,-1,-1, 0,-1,-1,-1, 0,-1},  /* DESITION PIERCE */
{-1, 0,-1,-1,-1,-1,-1, 0,-1, 0,-1},  /* ISR */
{-1, 0,-1,-1, 0, 0, 0, 0,-1, 0, 0},  /* PLUG */
{-1, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0},  /* SUSPEND */
{-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}; /* RESUME */
#endif
{ 0,-1,-1,-1,-1,-1,-1,-1},  /* INT */
{ 0, 0, 0, 0, 0, 0, 0, 0},  /* LOOP */
{-1, 0, 0,-1,-1,-1, 0,-1},  /* CHATT */
{-1,-1, 0,-1,-1,-1, 0,-1},  /* DESITION */
{-1, 0,-1,-1, 0,-1, 0, 0},  /* ISR */
{-1, 0,-1,-1, 0,-1, 0, 0},  /* PLUG */
{-1, 0, 0, 0, 0, 0, 0 ,0},  /* SUSPEND */
{-1, 0, 0, 0, 0, 0, 0, 0}}; /* RESUME */

int  edc_state_check( int cu_num , int be_num );
void edc_state_NG( int cu_num , int be_num );

/**********************************************************************/
/* globals															  */
/**********************************************************************/
extern int edc_interrupt_flag;

/* bit0: Headset  bit1:USB Earphone */
extern int edc_Headset_Definition_Flag;
extern edc_handle_t edc_port[EDC_NUM_OF_COUNT];
extern struct wake_lock edc_wlock_gpio;			/* suspend control */
extern struct switch_dev edc_headset_dev;
#if OPT_PIERCE_SWITCH
extern struct switch_dev edc_pierce1_dev;
extern struct switch_dev edc_pierce2_dev;
extern struct switch_dev edc_pierce3_dev;
#endif
extern struct input_dev *edc_ipdev;

#if OPT_USB_EARPHONE
extern int edc_UsbEarphoneFlag;			/* 0:USB earphone off 1:USB earphone on */
#endif
extern int edc_FaiEarphoneFlag;			/* 0:3.5 earphone off 1:3.5 earphone off */
extern int edc_Hedset_Switch_enable_Flag;	/* 0:with no switch  1:with switch */
extern int edc_mic_bias_flag;			/* 0:OFF  1:mic bias on */
extern int edc_voice_call_flag;			/* 0:not talking  1:talking */
extern int edc_inout_timer_expire;		/* 0:no timer 1:timer 2:switch timer */
extern int edc_suspend_cansel_flg;
extern int edc_model_pierce;
extern int edc_base_portstatus;
extern int edc_headset_mic_use_flag;

extern int edc_board_type;		/*add 2013/3/14 */
extern int edc_machine_type;	/*add 2013/3/14 */
extern int edc_pierce_retry_flg;/*add 2013/5/30 */

int plug_loop_call=0;
int test_cnt=0;
int edc_switch_irq_flg = 0;

extern int fj_boot_mode;

static DECLARE_DELAYED_WORK(work_que_loop, edc_hw_gpio_plug_loop);
static DECLARE_DELAYED_WORK(work_que_chatt, edc_hw_gpio_plug_chatt);
static DECLARE_DELAYED_WORK(work_que_decision, edc_hw_gpio_plug_decision);
static DECLARE_DELAYED_WORK(work_que_plug, edc_hw_handler_plug);
static DECLARE_DELAYED_WORK(work_que_switch, edc_hw_handler_switch);

/* TEST */
static DECLARE_DELAYED_WORK(work_que_suspend, edc_hw_suspend_loop);
static DECLARE_DELAYED_WORK(work_que_resume, edc_hw_resume_loop);
static DECLARE_DELAYED_WORK(work_que_mc, edc_hw_mc_loop);

/**********************************************************************/
/* edc_hw_init														  */
/**********************************************************************/
int edc_hw_init(void)
{
	int ret=EDC_OK;
	int cu_num=EDC_STATE_INIT;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	/* state check st */
	if( edc_state_check( cu_num , edc_state_num ) == EDC_STATE_NG ){
		edc_state_NG( cu_num , edc_state_num );
		edc_state_num = cu_num;
		return EDC_NG;
	}
	edc_state_num = cu_num;
	/* state check ed */

	ret = edc_hw_setup_config();

	if (ret) {
		printk("[EDC] ERROR(%s):edc_hw_setup_config() (%d)\n",__func__,ret);
		return ret;
	}

	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		
		ret = edc_hw_nv_read();

		if (ret) {
			printk("[EDC] ERROR(%s):edc_hw_nv_read() (%d)\n",__func__,ret);
			return ret;
		}
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_exit														  */
/**********************************************************************/
int edc_hw_exit(void)
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	ret = edc_hw_delete_config();

	if (ret) {
		printk("[EDC] ERROR(%s):edc_hw_delete_config() (%d)\n",__func__,ret);
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_suspend													  */
/**********************************************************************/
int edc_hw_suspend( void )
{
	int ret=EDC_OK;
	int portstatus;
	int cu_num=EDC_STATE_SUSPEND;

	EDC_DBG_PRINTK("[EDC] %s() status=%d flg=%d \n",__func__,edc_base_portstatus
		                                                    ,edc_suspend_cansel_flg);

	/* state check st */
	if( edc_state_check( cu_num , edc_state_num ) == EDC_STATE_NG ){
		edc_state_NG( cu_num , edc_state_num );
		edc_state_num = cu_num;
		return EDC_NG;
	}
	edc_state_num = cu_num;
	/* state check ed */

	if (edc_suspend_cansel_flg) {
		printk("[EDC] edc_suspend_cansel_flg(!=LOOP) = ON\n");
		return -EAGAIN;
	}

	cancel_delayed_work_sync(&work_que_loop);

	if (edc_suspend_cansel_flg) {
		printk("[EDC] edc_suspend_cansel_flg(==LOOP) = ON\n");
		return -EAGAIN;
	}

	/*if ( edc_base_portstatus == 0 )
		edc_hw_earphone_disable_irq(edc_port[EDC_PLUG].gpio_det_irq);*/

	if ( edc_base_portstatus == 1) { /* Earphone interupt enable */
	
    	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET), &xhscall_det_in_down );
		if( ret < 0 ) {
			printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET pm8xxx_gpio_config() (%d)\n",__func__,ret );
			return ret;
		}
		if( edc_voice_call_flag ){
			ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), &xheadset_det_in_en );
			if( ret < 0 ) {
				printk("[EDC] ERROR(%s):GPIO_XHEADSET_DET pm8xxx_gpio_config() (%d)\n",__func__,ret );
				return ret;
			}
			msleep(10);
			portstatus = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));
			printk( "[EDC] status = %d\n",portstatus);
			printk( "[EDC] gpio_set_value_cansleep(GPIO_HSDET_OWNER,EDC_GPIO_HIGH)\n");
			gpio_set_value_cansleep(GPIO_HSDET_OWNER,EDC_GPIO_HIGH); /* detect owner MDM */
		} else {
			ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), &xheadset_det_out_en );
			if( ret < 0 ) {
				printk("[EDC] ERROR(%s):GPIO_XHEADSET_DET pm8xxx_gpio_config() (%d)\n",__func__,ret );
				return ret;
			}
		}
	}

	if (edc_mic_bias_flag)
	{
		/*EDC_DBG_PRINTK("[EDC] YMU_MICBIAS_OFF\n");*/

		if( edc_Headset_Definition_Flag & HEDSET_CHECK_BIT ){
			if ( edc_voice_call_flag ) {
				/* Earphone interupt enable start */
				irq_set_irq_wake(edc_port[EDC_SWITCH].gpio_det_irq, EDC_WAKE_ON);
			} else {
				/*irq_set_irq_wake(edc_port[EDC_SWITCH].gpio_det_irq, EDC_WAKE_OFF);*/
				edc_hw_earphone_disable_irq(edc_port[EDC_SWITCH].gpio_det_irq);
			}
		}

		ymu_control(YMU_MICBIAS_OFF, 0);
	}

	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		/* GPIO-EXCOL11=L */
		gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_LOW );
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_resume													  */
/**********************************************************************/
int edc_hw_resume( void )
{
	int ret=EDC_OK;
	int cu_num=EDC_STATE_RESUME;

	EDC_DBG_PRINTK("[EDC] %s() status=%d \n", __func__,edc_base_portstatus);

	/* state check st */
	if( edc_state_check( cu_num , edc_state_num ) == EDC_STATE_NG ){
		edc_state_NG( cu_num , edc_state_num );
		edc_state_num = cu_num;
		return EDC_NG;
	}
	edc_state_num = cu_num;
	/* state check ed */

	if ( edc_base_portstatus == 1 ) { /* Earphone interupt enable */
		if( edc_voice_call_flag ){
			gpio_set_value_cansleep(GPIO_HSDET_OWNER,EDC_GPIO_LOW ); /* detect owner APQ */
		}
	}

	if (edc_mic_bias_flag){
		/*EDC_DBG_PRINTK("[EDC] YMU_MICBIAS_ON\n");*/
		ymu_control(YMU_MICBIAS_ON, 0);
		
		if( edc_Headset_Definition_Flag & HEDSET_CHECK_BIT ){
			/* Earphone interupt enable start */
			if ( edc_voice_call_flag )
				irq_set_irq_wake(edc_port[EDC_SWITCH].gpio_det_irq, EDC_WAKE_OFF);
			else{
				edc_hw_earphone_enable_irq(edc_port[EDC_SWITCH].gpio_det_irq);
			}
		}
	}

	plug_loop_call = 1;
	schedule_delayed_work(&work_que_loop, msecs_to_jiffies(RESUME_PHI35_LOOP));

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_setup_config												  */
/**********************************************************************/
int edc_hw_setup_config(void)
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	/* headset switch pulldown */
	ret = gpio_request(PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET), "XHSCALL_DET");
	if (ret){
		printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET gpio_request() (%d)\n",__func__,ret);
		goto ON_ERR;
	}
	ret = gpio_direction_input(PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET));
	if (ret){
		printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET gpio_direction_input() (%d)\n",__func__,ret);
		goto ON_ERR;
	}
	edc_port[EDC_SWITCH].gpio_det_irq = PM8921_GPIO_IRQ(PM8921_IRQ_BASE, GPIO_XHSCALL_DET);
	ret = request_irq(edc_port[EDC_SWITCH].gpio_det_irq,
					&edc_hw_irq_handler_switch, /* Thread function */
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
					"XHSCALL_DET",
					NULL);
	if (ret < 0){
		printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET request_irq() (%d)\n",__func__,ret);
		goto ON_ERR;
	}
	edc_hw_earphone_disable_irq(edc_port[EDC_SWITCH].gpio_det_irq);

	ret = gpio_request(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), "XHEADSET_DET");
	if (ret) {
		printk("[EDC] ERROR(%s):GPIO_XHEADSET_DET gpio_request() (%d)\n",__func__,ret);
		goto ON_ERR;
	}

	ret = gpio_direction_input(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));
	if (ret) {
		printk("[EDC] ERROR(%s):gpio_direction_input(XHEADSET_DET) = %d\n",__func__,ret);
		goto ON_ERR;
	}
	edc_port[EDC_PLUG].gpio_det_irq = PM8921_GPIO_IRQ(PM8921_IRQ_BASE,GPIO_XHEADSET_DET);
	ret = request_irq(edc_port[EDC_PLUG].gpio_det_irq,
					 &edc_hw_irq_handler_inout,
					 IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
					 "XHEADSET_DET",
					 NULL);
	if (ret < 0) {
		printk("[EDC] ERROR(%s):request_irq(XHEADSET_DET) ret=%d\n",__func__,ret);
		goto ON_ERR;
	}
	irq_set_irq_wake(edc_port[EDC_PLUG].gpio_det_irq, EDC_WAKE_ON);

	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		ret = gpio_request(PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), "KO_JMIC_ONOFF");
		if (ret) {
			printk("[EDC] ERROR(%s):GPIO_KO_JMIC_ONOFF gpio_request() (%d)\n",__func__,ret);
			goto ON_ERR;
		}
		printk( "[EDC] GPIO_KO_JMIC_ONOFF [19] set\n");
	}
	else{
		printk( "[EDC] GPIO_KO_JMIC_ONOFF [19] not set\n");
	}

	ret = gpio_request(GPIO_HSDET_OWNER, "HSDET_OWNER");
	if (ret){
		printk("[EDC] ERROR(%s):GPIO_HSDET_OWNER gpio_request() (%d)\n",__func__,ret);
		goto ON_ERR;
	}
	
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		ret = gpio_request(GPIO_ANSW_FET_ON, "ANSW_FET_ON");
		if (ret){
			printk("[EDC] ERROR(%s):GPIO_ANSW_FET_ON gpio_request() (%d)\n",__func__,ret);
			goto ON_ERR;
		}
		gpio_direction_output(GPIO_ANSW_FET_ON,0);

		ret = gpio_request(GPIO_SEL_JEAR_LED, "SEL_JEAR_LED");
		if (ret){
			printk("[EDC] ERROR(%s):GPIO_SEL_JEAR_LED gpio_request() (%d)\n",__func__,ret);
			goto ON_ERR;
		}
		gpio_direction_output(GPIO_SEL_JEAR_LED,0);
	}

	edc_hw_pm8921_gpio_control2(GPIO_XHSCALL_DET, EDC_GPIO_LOW);

	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		/*ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_L );*/
		/* 2013/5/9 */
		ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_H );
		if( ret < 0 ) {
			printk("[EDC] ERROR(%s):GPIO_KO_JMIC_ONOFF pm8xxx_gpio_config() (%d)\n",__func__,ret );
			goto ON_ERR;
		}
		printk( "[EDC] GPIO_KO_JMIC_ONOFF [19] set\n");
	}
	else{
		printk( "[EDC] GPIO_KO_JMIC_ONOFF [19] not set\n");
	}
		
	/* add 2013/3/25 */
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		gpio_set_value_cansleep( GPIO_ANSW_FET_ON , EDC_GPIO_HIGH );
		printk( "[EDC] GPIO_ANSW_FET_ON [EX-10] set\n");
	}
	else{
		printk( "[EDC] GPIO_ANSW_FET_ON [EX-10] not set\n");
	}

	/* test */
	/*schedule_delayed_work(&work_que_suspend, msecs_to_jiffies(60*1000));*/
	/*schedule_delayed_work(&work_que_mc, msecs_to_jiffies(60*1000));*/

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;

ON_ERR:
	ret = edc_hw_delete_config();
	return ret;
}

/**********************************************************************/
/* edc_hw_delete_config												  */
/**********************************************************************/
int edc_hw_delete_config(void)
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	free_irq(edc_port[EDC_PLUG].gpio_det_irq, NULL); /* Earphone interupt enable */
	/* IRQInterrupt Release */
	free_irq(edc_port[EDC_SWITCH].gpio_det_irq, NULL);

	/* setup XHEADSET_DET */
	gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));

	/* setup KO_JMIC_ONOFF */
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF));
	}
	
	/* setup XHS_CALL */
	gpio_free(PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET));
	/* setup HSDET_OWNER */
	gpio_free(GPIO_HSDET_OWNER);

	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){

		gpio_free(GPIO_ANSW_FET_ON);

		gpio_free(GPIO_SEL_JEAR_LED);
	}

	/* headset switch pulldown */
	ret = pm8xxx_gpio_config(pm8921_gpios_disable.gpio, &pm8921_gpios_disable.config); /* GPIO_XHSCALL_DET disable */
	if ( ret < 0 ) {
		printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET pm8xxx_gpio_config() (XHS_CALL, DIS) = %d\n",__func__,ret);
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_gpio_plug_loop											  */
/**********************************************************************/
void edc_hw_gpio_plug_loop(struct work_struct *work)
{
	int ret=EDC_OK;
	int portstatus=0;
	int cu_num=EDC_STATE_LOOP;
	int base_num=edc_state_num;
	int pierce_id_flg=0;
	int pierce_kind=0;
	int adc_code=0;

#ifdef EDC_DEBUG_LOOP
	EDC_DBG_LOOP_PRINTK("[EDC] %s() st \n", __func__);
#else
	if( plug_loop_call == 1 ){
		EDC_DBG_PRINTK("[EDC] %s() st \n", __func__);
	}
#endif
	plug_loop_call = 0;

	edc_suspend_cansel_flg = 0;

	edc_pierce_retry_flg = 0;

	/* state check st */
	if( edc_state_check( cu_num , edc_state_num ) == EDC_STATE_NG ){
		edc_state_NG( cu_num , edc_state_num );
		edc_state_num = cu_num;
		return;
	}
	edc_state_num = cu_num;
	/* state check ed */

	if ( edc_base_portstatus == 0 ){
		edc_hw_earphone_disable_irq(edc_port[EDC_PLUG].gpio_det_irq);
	}

	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), &xheadset_det_in_en );
	if( ret < 0 ) {
		printk("[EDC] GPIO_XHEADSET_DET INPUT pm8xxx_gpio_config (%d)\n", ret );
		goto ON_SKIP;
	}
	msleep(10);
	portstatus = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));

	EDC_DBG_LOOP_PRINTK("[EDC] edc_base_portstatus = %d , portstatus = %d\n",edc_base_portstatus,portstatus);

	if( edc_base_portstatus == portstatus )
	{
        goto ON_SKIP;
	}
	msleep(20);
	portstatus = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));
	if( edc_base_portstatus == portstatus )
	{
        goto ON_SKIP;
	}

	/* add 2013/4/11 */
	if( portstatus == 1 ){

		/* test */
		/*if( test_cnt == 0 ){
			schedule_delayed_work(&work_que_suspend, msecs_to_jiffies(2));
			msleep( 10 );
		}*/

		edc_port[EDC_PLUG].Polling_Counter=0;
		edc_port[EDC_PLUG].timer_flag = 1;
		edc_inout_timer_expire |= 1;

		schedule_delayed_work(&work_que_plug, msecs_to_jiffies(PHI35_INSERT_INTERVAL));
		edc_suspend_cansel_flg = 1;
		return;
	}

	/* test */
	/*if( test_cnt == 0 ){
		test_cnt++;
		schedule_delayed_work(&work_que_suspend, msecs_to_jiffies(2));
		msleep( 10 );
	}*/

    edc_chatt_cnt=0;

    schedule_delayed_work(&work_que_chatt, msecs_to_jiffies(PHI35_INSERT_CHATT_AUDIO));
	edc_suspend_cansel_flg = 1;
    /* earphone detect skip add start */
	/* if (mdm_powoff_get()) {
		printk("[EDC] HEADPHONE: Headset detect skip \n");
		goto ON_SKIP;
	} else {
		schedule_delayed_work(&work_que_chatt, msecs_to_jiffies(PHI35_INSERT_CHATT_AUDIO));
	}*/
    return;
ON_SKIP:

	if( fj_boot_mode == FJ_MODE_KERNEL_MODE || fj_boot_mode == FJ_MODE_MAKER_MODE ){
		goto ON_SKIP2;
	}

	if( edc_voice_call_flag )
		goto ON_SKIP2;

	if( edc_headset_mic_use_flag )
		goto ON_SKIP2;
	
	/* add 2013/04/19 st */
	pierce_id_flg = 0;
	if( edc_machine_type == EDC_MACHINE_TYPE_FJDEV002 && portstatus == 0 ){
		if( edc_Headset_Definition_Flag & HEDSET_CHECK_BIT )
			pierce_id_flg = 1;
		else{
			if( base_num == EDC_STATE_RESUME ){
				pierce_id_flg = 1;
			}
		}
	}
	
	if( pierce_id_flg ){

		//ret = edc_hw_earphone_remove( &pierce_kind , 0 );
		
		//if( ret ){
		//	printk("[EDC] ERROR(%s): edc_hw_earphone_remove() (%d)\n",__func__,ret);
		//}

		ret = edc_hw_earphone_loop_pierce_get( &pierce_kind , 0 , &adc_code );

		if( ret == EDC_OK ) {

			int old_flag = ( edc_Headset_Definition_Flag & ( PIERCE_ID_ALL_CHECK_BIT | HEDSET_CHECK_BIT ));
			int new_flag = ( pierce_kind & ( PIERCE_ID_ALL_CHECK_BIT | HEDSET_CHECK_BIT ));

			if( old_flag != new_flag ){

				printk("[EDC] %s earphone<-->pierce oldflg=%d newflg=%d\n", __func__,old_flag,new_flag);

				edc_port[EDC_PLUG].Polling_Counter=0;
				edc_port[EDC_PLUG].timer_flag = 1;
				edc_inout_timer_expire |= 1;

				schedule_delayed_work(&work_que_plug, msecs_to_jiffies(PHI35_INSERT_INTERVAL));
				edc_suspend_cansel_flg = 1;
				return;
        	}
		}
		else{
			printk("[EDC] ERROR(%s): edc_hw_earphone_loop_pierce_get() (%d)\n",__func__,ret);
		}
	}
	/* add 2013/04/19 ed */

ON_SKIP2:

	EDC_DBG_LOOP_PRINTK("[EDC] xheadset_det_out_en \n");
	if( portstatus == 1 ){
		ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), &xheadset_det_out_en );
		if( ret < 0 ) {
			printk("[EDC] GPIO_XHEADSET_DET OUTPUT pm8xxx_gpio_config (%d)\n", ret );
		}
	}
	schedule_delayed_work(&work_que_loop, msecs_to_jiffies(PHI35_LOOP));

	if ( edc_base_portstatus == 0 ){
		irq_set_irq_type(edc_port[EDC_PLUG].gpio_det_irq,IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND );
		edc_hw_earphone_enable_irq(edc_port[EDC_PLUG].gpio_det_irq);
	}

	EDC_DBG_LOOP_PRINTK("[EDC] %s() ed \n", __func__);

	return;
}

/**********************************************************************/
/* edc_hw_gpio_plug_chatt(base==1,cu==0)							  */
/**********************************************************************/
void edc_hw_gpio_plug_chatt(struct work_struct *work)
{
	/*int ret=EDC_OK;*/
	int portstatus;
	int cu_num=EDC_STATE_CHATT;

#ifdef EDC_DEBUG_LOOP
	EDC_DBG_LOOP_PRINTK("[EDC] %s()\n", __func__);
#else
	if( edc_chatt_cnt == 0 )
		EDC_DBG_PRINTK("[EDC] %s()\n", __func__);
#endif

	/* test */
	/*if( test_cnt == 0 ){
		edc_state_num = 0;
		test_cnt++;
	}*/

	/* state check st */
	if( edc_state_check( cu_num , edc_state_num ) == EDC_STATE_NG ){
		edc_state_NG( cu_num , edc_state_num );
		edc_state_num = cu_num;
		return;
	}
	edc_state_num = cu_num;
	/* state check ed */

	portstatus = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));
	if( edc_base_portstatus == portstatus )
	{
        goto ON_SKIP2;
	}

    edc_chatt_cnt++;
	if( edc_chatt_cnt <= CHATT_CNT )
	{
		schedule_delayed_work(&work_que_chatt, msecs_to_jiffies(PHI35_INSERT_CHATT_AUDIO));
	}
	else
	{
		schedule_delayed_work(&work_que_decision, msecs_to_jiffies(PHI35_INSERT_CHATT_AUDIO));
	}

    return;
ON_SKIP2:

	/*if( portstatus == 1 ){
		ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), &xheadset_det_out_en );
		if( ret < 0 ) {
			printk("[EDC] GPIO_XHEADSET_DET OUTPUT pm8xxx_gpio_config (%d)\n", ret );
		}
	}*/
	plug_loop_call = 1;
	schedule_delayed_work(&work_que_loop, msecs_to_jiffies(PHI35_LOOP));

	return;
}

/**********************************************************************/
/* edc_hw_gpio_plug_decision(base==1,cu==0)							  */
/**********************************************************************/
void edc_hw_gpio_plug_decision(struct work_struct *work)
{
	int ret=EDC_OK;
	int portstatus=0;
	int pierce_bitset=0;
	struct pm8xxx_adc_chan_result result_adc;
	/*int c_portstatus=edc_base_portstatus;*/
	int cu_num=EDC_STATE_DESITION;
	int submerge;
	
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	/* test */
	/*if( test_cnt == 0 ){
		edc_state_num = 0;
		test_cnt++;
	}*/

	/* state check st */
	if( edc_state_check( cu_num , edc_state_num ) == EDC_STATE_NG ){
		edc_state_NG( cu_num , edc_state_num );
		edc_state_num = cu_num;
		return;
	}
	edc_state_num = cu_num;
	/* state check ed */

ON_SKIP3:

	portstatus = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));
	if( edc_base_portstatus == portstatus )
	{
        goto ON_SKIP2;
	}

	/* remove */
	if( portstatus ){
		/* add 2013/4/11 */
		printk("[EDC] (%s) Error portstatus=%d\n", __func__,portstatus);
		edc_base_portstatus = -1;
		plug_loop_call = 1;
		schedule_delayed_work(&work_que_loop, msecs_to_jiffies(PHI35_LOOP));
		return;
	}
	/* insert */
	else{

		/*  SUBMERGE judge */
		/*ret = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(MPP_XHEADSET_VDD_DET), &earphone_config);
		if (ret < 0) {
			printk("[EDC] ERROR(%s):pm8xxx_mpp_config() (%d)\n",__func__, ret);
			goto ON_SKIP2;
		}*/
		msleep(20);
		ret = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_11, ADC_MPP_2_AMUX6, &result_adc);
		if (ret) {
			printk("[EDC] ERROR(%s):pm8xxx_adc_mpp_config_read() (%d)\n",__func__,ret);
			goto ON_SKIP2;
		}
		printk("[EDC] HEADPHONE: ret=%d ADC MPP11 adc_code:%x physical:%lld\n",ret,result_adc.adc_code, result_adc.physical);

		/*ret = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(MPP_XHEADSET_VDD_DET), &earphone_deconfig);
		if (ret < 0) {
			printk("[EDC] ERROR(%s): pm8xxx_mpp_config ret=%d\n",__func__,ret);
			goto ON_SKIP2;
		}*/

		submerge = 0;
		if( edc_machine_type == EDC_MACHINE_TYPE_FJDEV002 ){
			printk( "[EDC] EARPHONE_IN_VOL = %d\n",EARPHONE_IN_VOL_PIERCE);
			if( EARPHONE_IN_VOL_PIERCE < (result_adc.physical / EARPHONE_ADC) )
				submerge = 1;
		}
		else{
			printk( "[EDC] EARPHONE_IN_VOL = %d\n",EARPHONE_IN_VOL);
			if( EARPHONE_IN_VOL < (result_adc.physical / EARPHONE_ADC) )
				submerge = 1;
		}

		if( submerge )
		{
			printk("[EDC]  HEADPHONE: SUBMERGE\n");
			ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET), &xhscall_det_in_down );
			if( ret < 0 ) {
				printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET pm8xxx_gpio_config() (%d)\n",__func__,ret );
			}
   			goto ON_SKIP2;
   		}

		/* headset or pierce ? */
		pierce_bitset = 0;
		if(edc_model_pierce == PIERCE_MACHINE_TYPE){
			ret = edc_hw_earphone_insert( &pierce_bitset );
			if( ret ){
				printk("[EDC] ERROR(%s):edc_hw_earphone_insert() (%d)\n",__func__,ret);
				goto ON_SKIP2;
			}
		}

		if( pierce_bitset & PIERCE_ID_ALL_CHECK_BIT ){
			if( edc_pierce_retry_flg == 0 ){
				printk("[EDC] pierce retry \n");
				edc_pierce_retry_flg = 1;
				msleep(200);
				goto ON_SKIP3;
			}
		}
		edc_pierce_retry_flg = 0;

		if( pierce_bitset & PIERCE_ID_ALL_CHECK_BIT )
			edc_mic_bias_flag = 1;

		/* pierce1 */
		if( pierce_bitset & PIERCE_ID_1000_OHM_CHECK_BIT ){
			edc_Headset_Definition_Flag |= pierce_bitset;
#if OPT_PIERCE_SWITCH
			switch_set_state(&edc_pierce1_dev, 0x0001);
			printk("[EDC] switch_set_state (pierce1) 1\n");
#endif
			edc_base_portstatus = 0;
			edc_headset_mic_use_flag=0;
		}
		/* pierce2 */
		else if( pierce_bitset & PIERCE_ID_1500_OHM_CHECK_BIT ){
			edc_Headset_Definition_Flag |= pierce_bitset;
#if OPT_PIERCE_SWITCH
			switch_set_state(&edc_pierce2_dev, 0x0001);
			printk("[EDC] switch_set_state (pierce2) 1\n");
#endif
			edc_base_portstatus = 0;
			edc_headset_mic_use_flag=0;
		}
		/* pierce3 */
		else if( pierce_bitset & PIERCE_ID_2200_OHM_CHECK_BIT ){
			edc_Headset_Definition_Flag |= pierce_bitset;
#if OPT_PIERCE_SWITCH
			switch_set_state(&edc_pierce3_dev, 0x0001);
			printk("[EDC] switch_set_state (pierce3) 1\n");
#endif
			edc_base_portstatus = 0;
			edc_headset_mic_use_flag=0;
		}
		/* headset */
		else{

			edc_Headset_Definition_Flag &= ~MIC_SWITCH_CHECK_BIT;
			edc_Headset_Definition_Flag |= HEDSET_CHECK_BIT;

			/* del 2013/3/28 */
			//switch_set_state(&edc_headset_dev, 0x0001);
			//printk("[EDC] switch_set_state (headset) 1\n");
			/* add 2013/3/7 */
			ret = edc_ovp_usbearphone_path_set( OVP_USBEARPHONE_PATH_OFF );
			if( ret ){
				printk("[EDC] ERROR(%s):edc_ovp_usbearphone_path_set() (%d)\n",__func__,ret);
				goto ON_SKIP2;
			}
			printk("[EDC] OVP_USBEARPHONE_PATH_OFF\n");
			/* add 2013/3/7 */

			/* add 2013/3/28 */
			msleep(100);
			switch_set_state(&edc_headset_dev, 0x0001);
			printk("[EDC] switch_set_state (headset) 1\n");
			edc_base_portstatus = 0;
			edc_headset_mic_use_flag=0;

#if OPT_AUDIO_POLLING
#else
			edc_switch_irq_flg = 0;

			edc_hw_earphone_phi35_use( );
			edc_hw_earphone_switch_use( );

			msleep(20);
			if( edc_switch_irq_flg == 0 ){
				int irq=0;
				int data;
				printk( "[EDC] edc_switch_irq_flg == 0 \n");
				edc_hw_irq_handler_switch(irq, (void*)&data);

			}
#endif
		}

		/*irq_set_irq_type(edc_port[EDC_PLUG].gpio_det_irq,IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND );
		edc_hw_earphone_enable_irq(edc_port[EDC_PLUG].gpio_det_irq);*/

		printk("[EDC] Insert\n");
	}

	edc_interrupt_flag++;
	edc_hw_wake_up_interruptible();
	/* wake_up_interruptible(&edc_wq); */

ON_SKIP2:
	/*if( portstatus == 1 ){
		ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), &xheadset_det_out_en );
		if( ret < 0 ) {
			printk("[EDC] GPIO_XHEADSET_DET OUTPUT pm8xxx_gpio_config (%d)\n", ret );
		}
	}*/
	plug_loop_call = 1;
	schedule_delayed_work(&work_que_loop, msecs_to_jiffies(PHI35_LOOP));

	return;
}

/**********************************************************************/
/* edc_hw_handler_plug												  */
/**********************************************************************/
void edc_hw_handler_plug(struct work_struct *work)
{
	int ret=EDC_OK;
	int pierce_bitset=0;
	int base_flag=0;
	int cu_num=EDC_STATE_PLUG;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	/* test */
	/*if( test_cnt == 0 ){
		edc_state_num = 0;
		test_cnt++;
	}*/

	/* state check st */
	if( edc_state_check( cu_num , edc_state_num ) == EDC_STATE_NG ){
		edc_state_NG( cu_num , edc_state_num );
		edc_state_num = cu_num;
		return;
	}
	edc_state_num = cu_num;
	/* state check ed */

	edc_hw_earphone_disable_irq(edc_port[EDC_PLUG].gpio_det_irq);

	edc_port[EDC_SWITCH].Polling_Counter = 0;

	edc_inout_timer_expire &= 0xfffe;

	cancel_delayed_work_sync(&work_que_loop);

	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		ret = edc_hw_earphone_remove( &pierce_bitset , 1 );
		if( ret ){
			printk("[EDC] ERROR(%s):edc_hw_earphone_remove() (%d)\n",__func__,ret);
		}
	}
	
	if( edc_Headset_Definition_Flag & PIERCE_ID_ALL_CHECK_BIT ){
		/* MIC BIAS OFF */
		ret = ymu_control(YMU_MICBIAS_OFF,0);
		if ( ret == MCDRV_SUCCESS)
		{
			edc_mic_bias_flag = 0;
		}
		else
		{
			printk("[EDC] ERROR(%s):ymu_control() YMU_MICBIAS_OFF \n",__func__);
		}
	}
	
	/* pierce1 */
	if(edc_Headset_Definition_Flag & PIERCE_ID_1000_OHM_CHECK_BIT ){
	   edc_Headset_Definition_Flag &= ~PIERCE_ID_1000_OHM_CHECK_BIT;
#if OPT_PIERCE_SWITCH
		switch_set_state(&edc_pierce1_dev, 0x0000);
		printk("[EDC] switch_set_state (pierce1) 0\n");
#endif
		edc_base_portstatus = 1;
	}
	/* pierce2 */
	else if(edc_Headset_Definition_Flag & PIERCE_ID_1500_OHM_CHECK_BIT ){
	   		edc_Headset_Definition_Flag &= ~PIERCE_ID_1500_OHM_CHECK_BIT;
#if OPT_PIERCE_SWITCH
		switch_set_state(&edc_pierce2_dev, 0x0000);
		printk("[EDC] switch_set_state (pierce2) 0\n");
#endif
		edc_base_portstatus = 1;
	}
	/* pierce3 */
	else if(edc_Headset_Definition_Flag & PIERCE_ID_2200_OHM_CHECK_BIT ){
	   		edc_Headset_Definition_Flag &= ~PIERCE_ID_2200_OHM_CHECK_BIT;
#if OPT_PIERCE_SWITCH
		switch_set_state(&edc_pierce3_dev, 0x0000);
		printk("[EDC] switch_set_state (pierce3) 0\n");
#endif
		edc_base_portstatus = 1;
	}
	/* headset */
	else{
		base_flag = edc_Headset_Definition_Flag;
		edc_Hedset_Switch_enable_Flag = 0;
		edc_Headset_Definition_Flag &= ~MIC_SWITCH_CHECK_BIT;
		edc_Headset_Definition_Flag &= ~HEDSET_CHECK_BIT;
		switch_set_state(&edc_headset_dev, 0x0000);
		printk("[EDC] switch_set_state (headset) 0\n");
		edc_base_portstatus = 1;
		/* add 2013/3/7 */
		ret = edc_ovp_usbearphone_path_set( OVP_USBEARPHONE_PATH_ON );
		if( ret ){
			printk("[EDC] ERROR(%s):edc_ovp_usbearphone_path_set() (%d)\n",__func__,ret);
		}
		printk("[EDC] OVP_USBEARPHONE_PATH_ON\n");
		/* add 2013/3/7 */
					
#if OPT_AUDIO_POLLING
#else
		if( base_flag & HEDSET_CHECK_BIT ){
			edc_hw_earphone_phi35_unuse( );
			edc_hw_earphone_switch_unuse( );
		}
#endif
	}
	printk("[EDC] HEADPHONE: Remove_W\n");
	plug_loop_call = 1;
	schedule_delayed_work(&work_que_loop, msecs_to_jiffies(PHI35_LOOP));

	edc_interrupt_flag++;
	edc_hw_wake_up_interruptible();
	/* wake_up_interruptible(&edc_wq); */
	printk("[EDC] HEADPHONE: Headset %08x\n", edc_Headset_Definition_Flag);
	edc_port[EDC_PLUG].timer_flag = 0;

	return;
}

/**********************************************************************/
/* edc_hw_handler_switch											  */
/**********************************************************************/
void edc_hw_handler_switch(struct work_struct *work)
{
	int portstatus;

	EDC_DBG_LOOP_PRINTK("[EDC] %s()\n", __func__);

	portstatus = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET));
	EDC_DBG_LOOP_PRINTK("[EDC] timer_handler_switch sw_count=%d\n", edc_port[EDC_SWITCH].Polling_Counter);

	edc_inout_timer_expire &= 0xfffd;
	if (edc_port[EDC_SWITCH].io_data == portstatus)
	{
		edc_port[EDC_SWITCH].Polling_Counter++;
		if (!edc_Hedset_Switch_enable_Flag)
		{
			if (edc_port[EDC_SWITCH].Polling_Counter >= FIRST_SWITCH_PULL)
			{
				EDC_DBG_PRINTK("[EDC] timer_handler_switch over\n");
#if OPT_USB_EARPHONE
				EDC_DBG_PRINTK("[EDC] USB(%d) 3.5(%d)\n",edc_UsbEarphoneFlag,edc_FaiEarphoneFlag);
#else
				EDC_DBG_PRINTK("[EDC] 3.5(%d)\n",edc_FaiEarphoneFlag);
#endif

				if (!edc_port[EDC_SWITCH].io_data && (edc_Headset_Definition_Flag & (HEDSET_CHECK_BIT /*| USB_EARPHONE_CHECK_BIT*/)))
				{
					/* Headset switch enable */
					edc_Hedset_Switch_enable_Flag=1;
					printk("[EDC] HEADPHONE: Headset Switch ENABLE\n");
				}
				else
				{
					edc_hw_earphone_disable_irq(edc_port[EDC_SWITCH].gpio_det_irq);
					edc_Hedset_Switch_enable_Flag=0;
					printk("[EDC] HEADPHONE: Headset Switch DISABLE\n");
				}
				edc_interrupt_flag++;
				edc_hw_wake_up_interruptible();
				/* wake_up_interruptible(&edc_wq); */
				edc_port[EDC_SWITCH].timer_flag = 0;
				return;
			}
			else
			{
#if OPT_USB_EARPHONE
				if (edc_Headset_Definition_Flag & (HEDSET_CHECK_BIT | USB_EARPHONE_CHECK_BIT))
#else
				if (edc_Headset_Definition_Flag & HEDSET_CHECK_BIT )
#endif
				{
					schedule_delayed_work(&work_que_switch, msecs_to_jiffies(HSCALL_PUSH_INTERVAL));
                }
				else
				{
					printk("[EDC] HEADPHONE: delete timer\n");
					edc_interrupt_flag++;
					edc_hw_wake_up_interruptible();
					/* wake_up_interruptible(&edc_wq); */
					edc_port[EDC_SWITCH].timer_flag = 0;
				}
			}
			
			return;
		}
		
		if (edc_port[EDC_SWITCH].Polling_Counter >= HSCALL_PUSH_POLL_NUM)
		{
			printk("[EDC] \n");
			if (!edc_port[EDC_SWITCH].io_data)
			{
				edc_Headset_Definition_Flag &= ~MIC_SWITCH_CHECK_BIT;
#if OPT_AUDIO_POLLING
#else
				edc_hw_earphone_mic_release( );
#endif
			}
			else
			{
				edc_Headset_Definition_Flag |= MIC_SWITCH_CHECK_BIT;
#if OPT_AUDIO_POLLING
#else
				edc_hw_earphone_mic_push( );
#endif
			}
			edc_interrupt_flag++;
			edc_hw_wake_up_interruptible();
			/* wake_up_interruptible(&edc_wq); */
			printk("[EDC] HEADPHONE: switch %08x\n", edc_Headset_Definition_Flag);
			edc_port[EDC_SWITCH].timer_flag = 0;
			return ;
		}
	}
	else
	{
		/* status changes under chattering */
		edc_port[EDC_SWITCH].io_data = portstatus;
		edc_port[EDC_SWITCH].Polling_Counter = 0;
	}
	schedule_delayed_work(&work_que_switch, msecs_to_jiffies(HSCALL_PUSH_INTERVAL));
	
	return;
}

/**********************************************************************/
/* edc_hw_irq_handler_inout											  */
/**********************************************************************/
irqreturn_t edc_hw_irq_handler_inout(int irq, void *data)
{
	int cu_num=EDC_STATE_ISR;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	/* test */
	/*if( test_cnt == 0 ){
		edc_state_num = 0;
		test_cnt++;
	}*/

	/* state check st */
	if( edc_state_check( cu_num , edc_state_num ) == EDC_STATE_NG ){
		edc_state_NG( cu_num , edc_state_num );
		edc_state_num = cu_num;
		return IRQ_HANDLED;
	}
	edc_state_num = cu_num;
	/* state check ed */

	/* Earphone detection start */
	if( edc_base_portstatus != 0 )
	{
		/* printk("[EDC] <Earphone: skip>"); */
		return IRQ_HANDLED;
	}
	/* Earphone detection end */
	/* Earphone refactoring start*/
	if(!edc_port[EDC_PLUG].timer_flag )
	{
		edc_suspend_cansel_flg = 1;
		/* Earphone interupt enable start */
		wake_lock_timeout(&edc_wlock_gpio, EARPHONE_WAKELOCK_TO);
		edc_port[EDC_PLUG].io_data = 1;
		edc_port[EDC_PLUG].Polling_Counter=0;
		/* Earphone interupt enable end */
		edc_port[EDC_PLUG].timer_flag = 1;
		edc_inout_timer_expire |= 1;
		schedule_delayed_work(&work_que_plug, msecs_to_jiffies(PHI35_INSERT_INTERVAL));
	}
	/* Earphone refactoring end*/

	return IRQ_HANDLED;
}

/**********************************************************************/
/* edc_hw_irq_handler_switch										  */
/**********************************************************************/
irqreturn_t edc_hw_irq_handler_switch(int irq, void *data)
{
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	edc_switch_irq_flg = 1;

#if OPT_USB_EARPHONE
	if ((edc_Headset_Definition_Flag & (HEDSET_CHECK_BIT | USB_EARPHONE_CHECK_BIT))==0)
#else
	if ((edc_Headset_Definition_Flag & HEDSET_CHECK_BIT )==0)
#endif
	{
		printk("[EDC] irq_handler_switch not detect headset flag = %x\n",edc_Headset_Definition_Flag);
		return IRQ_HANDLED;
	}
    if (!edc_port[EDC_SWITCH].timer_flag)
	{
		wake_lock_timeout(&edc_wlock_gpio, EARPHONE_WAKELOCK_TO);
		edc_port[EDC_SWITCH].io_data = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET)); /* Earphone detection */
		edc_port[EDC_SWITCH].Polling_Counter=0;
		edc_port[EDC_SWITCH].timer_flag = 1;
		edc_inout_timer_expire |= 2;
		schedule_delayed_work(&work_que_switch, msecs_to_jiffies(HSCALL_PUSH_INTERVAL));
	}
	return IRQ_HANDLED;
}


/**********************************************************************/
/* edc_hw_earphone_phi35_use										  */
/**********************************************************************/
int edc_hw_earphone_phi35_use( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	edc_FaiEarphoneFlag=1;

#if OPT_USB_EARPHONE

	if (edc_UsbEarphoneFlag==0)
	{
		wake_lock(&edc_wlock_gpio);

		ret = edc_hw_pm8921_gpio_control2(GPIO_XHSCALL_DET, EDC_GPIO_HIGH);

		if( ret ){
			printk("[EDC] ERROR(%s):edc_hw_pm8921_gpio_control2() (%d)\n",__func__,ret );
			return ret;
		}
		/* switch_set_state(&edc_sdev, 0x0001); */
		/* msleep(100); */
	}
	else
	{
		edc_hw_check_wakelock();
	}
#else
	wake_lock(&edc_wlock_gpio);

	ret = edc_hw_pm8921_gpio_control2(GPIO_XHSCALL_DET, EDC_GPIO_HIGH);

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_pm8921_gpio_control2() (%d)\n",__func__,ret );
		return ret;
	}
	/* switch_set_state(&edc_sdev, 0x0001); */
	/* msleep(100); */
#endif

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_phi35_unuse										  */
/**********************************************************************/
int edc_hw_earphone_phi35_unuse( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	ret = edc_hw_pm8921_gpio_control2(GPIO_XHSCALL_DET, EDC_GPIO_LOW);

	if( ret ){
		printk("[EDC] ERROR(%s):edc_hw_pm8921_gpio_control2() (%d)\n",__func__,ret );
		return ret;
	}

	edc_FaiEarphoneFlag=0;
	
#if OPT_USB_EARPHONE
	if (edc_UsbEarphoneFlag==0)
	{
		/* switch_set_state(&edc_sdev, 0x0000); */
		/* msleep(100); */
	}
#endif

	edc_hw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n",__func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_switch_use										  */
/**********************************************************************/
int edc_hw_earphone_switch_use( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	edc_hw_earphone_enable_irq(edc_port[EDC_SWITCH].gpio_det_irq);

	/* headset switdh pullup */
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET), &xhscall_det_in_up );
	if( ret < 0 ) {
		printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET pm8xxx_gpio_config() (%d)\n",__func__,ret );
		return ret;
	}

	/* MIC BIAS ON */
	ret = ymu_control(YMU_MICBIAS_ON,0);
	if (ret == MCDRV_SUCCESS)
	{
		edc_mic_bias_flag = 1;
	}
	else
	{
		printk("[EDC] ERROR(%s):ymu_control() YMU_MICBIAS_ON \n",__func__);
		return EDC_NG;
	}

	edc_hw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_switch_unuse										  */
/**********************************************************************/
int edc_hw_earphone_switch_unuse( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	edc_hw_earphone_disable_irq(edc_port[EDC_SWITCH].gpio_det_irq);
	edc_Hedset_Switch_enable_Flag=0;

	/* headset switch pulldown */
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET), &xhscall_det_in_down );
	if( ret < 0 ) {
		printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET pm8xxx_gpio_config() (%d)\n",__func__,ret );
		return ret;
	}

	/* MIC BIAS OFF */
	ret = ymu_control(YMU_MICBIAS_OFF,0);
	if ( ret == MCDRV_SUCCESS)
	{
		edc_mic_bias_flag = 0;
	}
	else
	{
		printk("[EDC] ERROR(%s):ymu_control() YMU_MICBIAS_OFF \n",__func__);
		return EDC_NG;
	}

	edc_hw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_mic_push											  */
/**********************************************************************/
int edc_hw_earphone_mic_push( void )
{
	int ret=EDC_OK;
	
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	input_report_key(edc_ipdev, KEY_MEDIA, 1);
	input_sync(edc_ipdev);

	edc_hw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_mic_release										  */
/**********************************************************************/
int edc_hw_earphone_mic_release( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n",__func__);

	input_report_key(edc_ipdev, KEY_MEDIA, 0);
	input_sync(edc_ipdev);

	edc_hw_check_wakelock();

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

#if OPT_USB_EARPHONE
/**********************************************************************/
/* edc_hw_usb_earphone_use	 	 	 								  */
/**********************************************************************/
int edc_hw_usb_earphone_use( int edc_stage )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	if( edc_stage == EARPHONE_USB_STAGE_0 ){
		ret = edc_hw_pm8921_gpio_control2(GPIO_XHSCALL_DET, EDC_GPIO_LOW);

		if( ret ){
			printk("[EDC] ERROR(%s):edc_hw_pm8921_gpio_control2() (%d)\n",__func__,ret );
			return ret;
		}
	}
	else if( edc_stage == EARPHONE_USB_STAGE_1 ){
		schedule_delayed_work(&work_que_switch, msecs_to_jiffies(HSCALL_PUSH_INTERVAL));
	}
	else{
		printk("[EDC] ERROR(%s):stage No (%d)\n",__func__,edc_stage );
		ret = EDC_NG;
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_usb_earphone_unuse										  */
/**********************************************************************/
int edc_hw_usb_earphone_unuse( int edc_stage )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	if( edc_stage == EARPHONE_USB_STAGE_0 ){
		ret = edc_hw_pm8921_gpio_control2(GPIO_XHSCALL_DET, EDC_GPIO_HIGH);

		if( ret ){
			printk("[EDC] ERROR(%s):edc_hw_pm8921_gpio_control2() (%d)\n",__func__,ret );
			return ret;
		}
	}
	else if( edc_stage == EARPHONE_USB_STAGE_1 ){
		schedule_delayed_work(&work_que_switch, msecs_to_jiffies(HSCALL_PUSH_INTERVAL));
	}
	else{
		printk("[EDC] ERROR(%s):stage No (%d)\n",__func__,edc_stage );
		ret = EDC_NG;
		return ret;
	}
	
	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
	
	return ret;
}
#endif
/**********************************************************************/
/* edc_hw_earphone_det_output										  */
/**********************************************************************/
int edc_hw_earphone_det_output( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	/* change 2013/4/11 */
	/*cancel_delayed_work(&work_que_loop);*/
	cancel_delayed_work_sync(&work_que_loop);

	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), &xheadset_det_in_en );

	if( ret < 0 ) {
		printk("[EDC] ERROR(%s):GPIO_XHEADSET_DET pm8xxx_gpio_config() (%d)\n",__func__,ret );
		return ret;
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}
/**********************************************************************/
/* edc_hw_earphone_status_get							  			  */
/**********************************************************************/
int edc_hw_earphone_status_get( struct earphone_status *earphone_read_status )
{
	int ret=EDC_OK;
	int	plug_status=0;
	struct pm8xxx_adc_chan_result result_adc;
	int pierce_kind=0;
	int adc_code=0;


	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET), &xheadset_det_in_en );
	if( ret < 0 ) {
		printk("[EDC] ERROR(%s):GPIO_XHEADSET_DET pm8xxx_gpio_config() (%d)\n",__func__,ret );
		return ret;
	}
	msleep(10);
	/* status get */
	plug_status = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));
	printk("[EDC] %s 1st EARPHONE_DET_GET=%d\n",__func__, plug_status);
	msleep(20);
	/* status get */
	plug_status = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHEADSET_DET));
	printk("[EDC] %s 2nd EARPHONE_DET_GET=%d\n",__func__, plug_status);

	if ( plug_status == 0 ) {

		if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
			ret = edc_hw_earphone_remove( &pierce_kind , 1 );
			if( ret ){
				printk("[EDC] ERROR(%s):edc_hw_earphone_remove() (%d)\n",__func__,ret);
				return ret;
			}
			ret = edc_hw_earphone_loop_pierce_get( &pierce_kind , 1 , &adc_code );
			if( ret ){
				printk("[EDC] ERROR(%s):edc_hw_earphone_loop_pierce_get() (%d)\n",__func__,ret);
				return ret;
			}
		}
		else{
			pierce_kind |= HEDSET_CHECK_BIT;
		}

		if( pierce_kind & HEDSET_CHECK_BIT ){
			ymu_control(YMU_MICBIAS_OFF,0);
			earphone_read_status->plug_status = 1;
			/* earphone detect */
			ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET), &xhscall_det_in_up );
			if( ret < 0 ) {
				printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET INPUT_PUP pm8xxx_gpio_config (%d)\n",__func__,ret );
				return ret;
			}
			ymu_control(YMU_MICBIAS_ON,0);
			earphone_read_status->sw_status = gpio_get_value(PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET));
			printk("[EDC] %s  EARPHONE_SWITCH_GET=%d\n",__func__, earphone_read_status->sw_status);
			ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_XHSCALL_DET), &xhscall_det_in_down );
			if( ret < 0 ) {
				printk("[EDC] ERROR(%s):GPIO_XHSCALL_DET INPUT_PDN pm8xxx_gpio_config (%d)\n",__func__,ret );
				return ret;
			}
			/*ymu_control(YMU_MICBIAS_OFF,0);*/
			earphone_read_status->pierce_status = 0;
		}
		else{
			earphone_read_status->plug_status = 0;
			earphone_read_status->sw_status = 0;
			earphone_read_status->pierce_status = 1;
		}
	}
	else {
		earphone_read_status->sw_status = 0;
		earphone_read_status->plug_status = 0;
		earphone_read_status->pierce_status = 0;
		if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
			ret = edc_hw_earphone_loop_pierce_get( &pierce_kind , 1 , &adc_code );
			if( ret ){
				printk("[EDC] ERROR(%s):edc_hw_earphone_loop_pierce_get() (%d)\n",__func__,ret);
				return ret;
			}
		}
	}
	/* adc vol get */

	result_adc.adc_code = 0;

	/*ret = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(MPP_XHEADSET_VDD_DET), &earphone_config);
	if (ret < 0) {
		printk("[EDC] ERROR(%s):MPP11 pm8xxx_mpp_config() config (%d)\n",__func__,ret);
		return ret;
	}*/
	msleep(40);
	ret = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_11, ADC_MPP_2_AMUX6, &result_adc);
	if (ret) {
		printk("[EDC] ERROR(%s):MPP11 pm8xxx_adc_mpp_config_read() (%d)\n",__func__,ret);
		return ret;
	}
	printk("[EDC] HEADPHONE: ADC MPP11 value raw:%x physical:%llx\n",result_adc.adc_code, result_adc.physical);
	/*ret = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(MPP_XHEADSET_VDD_DET), &earphone_deconfig);
	if (ret < 0) {
		printk("[EDC] ERROR(%s):MPP11 pm8xxx_mpp_config() deconfig (%d)\n",__func__,ret);
		return ret;
	}*/
	earphone_read_status->plug_vol[0] = (result_adc.adc_code);
	earphone_read_status->plug_vol[1] = (result_adc.adc_code >> 8);

	/* pierce adc vol get */

	result_adc.adc_code = adc_code;

	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		earphone_read_status->pierce_vol[0] = (result_adc.adc_code);
		earphone_read_status->pierce_vol[1] = (result_adc.adc_code >> 8);
	}

#if 0
	printk("[EDC] [MC HEAD SET]\n");
	printk("[EDC] plug_status = %d\n", earphone_read_status->plug_status);
	printk("[EDC] sw_status   = %d\n", earphone_read_status->sw_status);
	printk("[EDC] plug_vol[0] = %0x\n", earphone_read_status->plug_vol[0]);
	printk("[EDC] plug_vol[1] = %0x\n", earphone_read_status->plug_vol[1]);
	if( edc_model_pierce == PIERCE_MACHINE_TYPE ){
		printk("[EDC] pierce_status = %d\n", earphone_read_status->pierce_status);
		printk("[EDC] pierce_vol[0] = %0x\n", earphone_read_status->pierce_vol[0]);
		printk("[EDC] pierce_vol[1] = %0x\n", earphone_read_status->pierce_vol[1]);
	}
#endif

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_pm8921_gpio_control2									  	  */
/**********************************************************************/
int edc_hw_pm8921_gpio_control2(int gpio, int highlow)
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/

/*	struct pm_gpio param = {
		.direction	= PM_GPIO_DIR_OUT,
		.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
		.output_value	= highlow,
		.pull		= PM_GPIO_PULL_NO,
		.vin_sel	= PM_GPIO_VIN_L17,
		.out_strength	= PM_GPIO_STRENGTH_MED,
		.function	= PM_GPIO_FUNC_NORMAL,
	};

	EDC_DBG_PRINTK("[EDC] %s(%d, %d)\n", __func__, gpio, highlow);

	ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(gpio), &param);
	if (ret) {
		EDC_DBG_PRINTK("[EDC] ERROR:pm8xxx_gpio_config(gpio=%d, value=%d) ret=%d\n", PM8921_GPIO_PM_TO_SYS(gpio), highlow, ret);
	}
	EDC_DBG_PRINTK("[EDC] edc_port (%d) = %d\n", gpio,gpio_get_value_cansleep(PM8921_GPIO_PM_TO_SYS(gpio)));
*/
	/*EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);*/

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_voice_call_stop								  	  */
/**********************************************************************/
int edc_hw_earphone_voice_call_stop( void )
{
	int ret=EDC_OK;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	gpio_set_value_cansleep(GPIO_HSDET_OWNER,EDC_GPIO_LOW); /* detect owner MDM */

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_insert							  	 		  	  */
/**********************************************************************/
int edc_hw_earphone_insert( int *pierce_bitset )
{
	int ret=EDC_OK;
	struct pm8xxx_adc_chan_result result_adc;

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	/* del 2013/3/14 */
	/* analog SW ON (GPIO-EX COL10=H) */
	/* gpio_set_value_cansleep( GPIO_ANSW_FET_ON , EDC_GPIO_HIGH ); */

	*pierce_bitset = 0;

	/* add 2013/3/15 */
	if( edc_machine_type == EDC_MACHINE_TYPE_FJDEV001 ){
		*pierce_bitset = 0;
		
		printk("[EDC] insert normal earphone\n");

		edc_mic_pierce = EARPHONE_MIC ;

		/* GPIO-EXCOL11=L*/
		gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_LOW );

		ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_L );
		if( ret < 0 ) {
			printk("[EDC] ERROR(%s):GPIO_KO_JMIC_ONOFF pm8xxx_gpio_config() (%d)\n",__func__,ret );
			return ret;
		}

		EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

		return ret;

	}

	if( edc_mic_bias_flag == 0 ){
		/* MIC BIAS ON */
		ret = ymu_control(YMU_MICBIAS_ON,0);
		if (ret == MCDRV_SUCCESS)
		{
		/*edc_mic_bias_flag = 1;*/
		}
		else
		{
			printk("[EDC] ERROR(%s):ymu_control() YMU_MICBIAS_ON \n",__func__);
			return EDC_NG;
		}
	}

	/* 2013/5/9 */
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_L );
	if( ret < 0 ) {
		printk("[EDC] ERROR(%s):GPIO_KO_JMIC_ONOFF pm8xxx_gpio_config() (%d)\n",__func__,ret );
		return ret;
	}

	/* PM8921-MPP_12 */
	/* adc vol get */
	/*ret = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(MPP_KO_PIERCE_DET), &earphone_pierce_config);
	if (ret < 0) {
		printk("[EDC] ERROR(%s):pm8xxx_mpp_config() config (%d)\n",__func__, ret);
		return ret;
	}*/
	msleep(40);
	ret = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_12, ADC_MPP_1_AMUX6, &result_adc);
	if (ret) {
		printk("[EDC] ERROR(%s):pm8xxx_adc_mpp_config_read() (%d)\n",__func__,ret);
		return ret;
	}
	printk("[EDC] HEADPHONE: ADC MPP12 adc_code:%x\n",result_adc.adc_code);
	/*ret = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(MPP_KO_PIERCE_DET), &earphone_pierce_deconfig);
	if (ret < 0) {
		printk("[EDC] ERROR(%s):pm8xxx_mpp_config() deconfig (%d)\n",__func__, ret);
		return ret;
	}*/

	if( ( (result_adc.adc_code >= pierce_id_1000_ohm_low ) && ( result_adc.adc_code <= pierce_id_1000_ohm_high ) ) || 
	    ( (result_adc.adc_code >= pierce_id_1500_ohm_low ) && ( result_adc.adc_code <= pierce_id_1500_ohm_high ) ) || 
	    ( (result_adc.adc_code >= pierce_id_2200_ohm_low ) && ( result_adc.adc_code <= pierce_id_2200_ohm_high ) ) ){

		if( (result_adc.adc_code >= pierce_id_1000_ohm_low ) && ( result_adc.adc_code <= pierce_id_1000_ohm_high ) ){
			*pierce_bitset |= PIERCE_ID_1000_OHM_CHECK_BIT;
			printk("[EDC] insert earphone pierce 1 \n");
		}
		else if((result_adc.adc_code >= pierce_id_1500_ohm_low ) && ( result_adc.adc_code <= pierce_id_1500_ohm_high ) ){
			*pierce_bitset |= PIERCE_ID_1500_OHM_CHECK_BIT;
			printk("[EDC] insert earphone pierce 2 \n");
		}
		else if((result_adc.adc_code >= pierce_id_2200_ohm_low ) && ( result_adc.adc_code <= pierce_id_2200_ohm_high ) ){
			*pierce_bitset |= PIERCE_ID_2200_OHM_CHECK_BIT;
			printk("[EDC] insert earphone pierce 3 \n");
		}

		edc_mic_pierce = EARPHONE_PIERCE ;

		/* GPIO-EXCOL11=H */
		/*gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_HIGH );*/

#if 0
		/* YUM829-MIC3 OFF */
		ret = ymu_control(YMU_MICBIAS_OFF,0);
		if (ret == MCDRV_SUCCESS)
		{
			/*edc_mic_bias_flag = 0;*/
		}
		else
		{
			printk("[EDC] ERROR(%s):ymu_control() YMU_MICBIAS_OFF \n",__func__);
			return EDC_NG;
		}
#endif

		ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_H );
		if( ret < 0 ) {
			printk("[EDC] ERROR(%s):GPIO_KO_JMIC_ONOFF pm8xxx_gpio_config() (%d)\n",__func__,ret );
			return ret;
		}
	}
	else{

		*pierce_bitset |= HEDSET_CHECK_BIT;

		printk("[EDC] insert normal earphone\n");

		edc_mic_pierce = EARPHONE_MIC ;

		/* GPIO-EXCOL11=L*/
		gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_LOW );

		if( edc_mic_bias_flag == 0 ){
			/* YUM829-MIC3 OFF */
			ret = ymu_control(YMU_MICBIAS_OFF,0);
			if (ret == MCDRV_SUCCESS)
			{
				/*edc_mic_bias_flag = 0;*/
			}
			else
			{
				printk("[EDC] ERROR(%s):ymu_control() YMU_MICBIAS_OFF \n",__func__);
				return EDC_NG;
			}
		}
	}

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}
/**********************************************************************/
/* edc_hw_earphone_remove							  	 			  */
/**********************************************************************/
int edc_hw_earphone_remove( int *pierce_bitset , int debug_mode )
{
	int	ret=EDC_OK;

	if( debug_mode )
		EDC_DBG_PRINTK("[EDC] %s()\n", __func__);
	else
		EDC_DBG_LOOP_PRINTK("[EDC] %s()\n", __func__);

	edc_mic_pierce = EARPHONE_NOT ;

	*pierce_bitset  &= ~(PIERCE_ID_1000_OHM_CHECK_BIT|
						 PIERCE_ID_1500_OHM_CHECK_BIT|
						 PIERCE_ID_2200_OHM_CHECK_BIT);

	/* (GPIO-EX COL11=L) */
	gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_LOW );

	/* del 2013/3/4 */
    /* (GPIO-EX COL10=L) */
    /* gpio_set_value_cansleep( GPIO_ANSW_FET_ON , EDC_GPIO_LOW ); */

    /* PM8921 GPIO_19=H */
	/*ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_L );*/
	/* 2013/5/9 */
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_H );
	if( ret < 0 ) {
		printk("[EDC] ERROR(%s):GPIO_KO_JMIC_ONOFF pm8xxx_gpio_config() (%d)\n",__func__,ret );
		return ret;
	}

	if( debug_mode )
		EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
	else
		EDC_DBG_LOOP_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_loop_pierce_get					  	 		  	  */
/**********************************************************************/
int edc_hw_earphone_loop_pierce_get( int *pierce_bitset , int debug_mode , int *adc_code )
{
	int ret=EDC_OK;
	struct pm8xxx_adc_chan_result result_adc;

	if( debug_mode )
		EDC_DBG_PRINTK("[EDC] %s()\n", __func__);
	else
		EDC_DBG_LOOP_PRINTK("[EDC] %s()\n", __func__);

	*pierce_bitset = 0;

	if( edc_mic_bias_flag == 0 ){
		/* MIC BIAS ON */
		ret = ymu_control(YMU_MICBIAS_ON,0);
		if (ret != MCDRV_SUCCESS)
		{
			printk("[EDC] ERROR(%s):ymu_control() YMU_MICBIAS_ON \n",__func__);
			return EDC_NG;
		}
	}

	/* 2013/5/9 */
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_L );
	if( ret < 0 ) {
		printk("[EDC] ERROR(%s):GPIO_KO_JMIC_ONOFF pm8xxx_gpio_config() (%d)\n",__func__,ret );
		return ret;
	}

	/* PM8921-MPP_12 */
	/* adc vol get */
	/*ret = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(MPP_KO_PIERCE_DET), &earphone_pierce_config);
	if (ret < 0) {
		printk("[EDC] ERROR(%s):pm8xxx_mpp_config() config (%d)\n",__func__, ret);
		return ret;
	}*/
	/*msleep(40);*/
	ret = pm8xxx_adc_mpp_config_read(PM8XXX_AMUX_MPP_12, ADC_MPP_1_AMUX6, &result_adc);
	if (ret) {
		printk("[EDC] ERROR(%s):pm8xxx_adc_mpp_config_read() (%d)\n",__func__,ret);
		return ret;
	}

	if( debug_mode )
		EDC_DBG_PRINTK("[EDC] HEADPHONE: ADC MPP12 adc_code:%x\n",result_adc.adc_code);
	else
		EDC_DBG_LOOP_PRINTK("[EDC] HEADPHONE: ADC MPP12 adc_code:%x\n",result_adc.adc_code);

	/*ret = pm8xxx_mpp_config(PM8921_MPP_PM_TO_SYS(MPP_KO_PIERCE_DET), &earphone_pierce_deconfig);
	if (ret < 0) {
		printk("[EDC] ERROR(%s):pm8xxx_mpp_config() deconfig (%d)\n",__func__, ret);
		return ret;
	}*/

	*adc_code = result_adc.adc_code;

	if( ( (result_adc.adc_code >= pierce_id_1000_ohm_low ) && ( result_adc.adc_code <= pierce_id_1000_ohm_high ) ) || 
	    ( (result_adc.adc_code >= pierce_id_1500_ohm_low ) && ( result_adc.adc_code <= pierce_id_1500_ohm_high ) ) || 
	    ( (result_adc.adc_code >= pierce_id_2200_ohm_low ) && ( result_adc.adc_code <= pierce_id_2200_ohm_high ) ) ){

		if( (result_adc.adc_code >= pierce_id_1000_ohm_low ) && ( result_adc.adc_code <= pierce_id_1000_ohm_high ) ){
			*pierce_bitset |= PIERCE_ID_1000_OHM_CHECK_BIT;
			if( debug_mode )
				EDC_DBG_PRINTK("[EDC] insert earphone pierce 1 \n");
			else
				EDC_DBG_LOOP_PRINTK("[EDC] insert earphone pierce 1 \n");
		}
		else if((result_adc.adc_code >= pierce_id_1500_ohm_low ) && ( result_adc.adc_code <= pierce_id_1500_ohm_high ) ){
			*pierce_bitset |= PIERCE_ID_1500_OHM_CHECK_BIT;
			if( debug_mode )
				EDC_DBG_PRINTK("[EDC] insert earphone pierce 2 \n");
			else
				EDC_DBG_LOOP_PRINTK("[EDC] insert earphone pierce 2 \n");
		}
		else if((result_adc.adc_code >= pierce_id_2200_ohm_low ) && ( result_adc.adc_code <= pierce_id_2200_ohm_high ) ){
			*pierce_bitset |= PIERCE_ID_2200_OHM_CHECK_BIT;
			if( debug_mode )
				EDC_DBG_PRINTK("[EDC] insert earphone pierce 3 \n");
			else
				EDC_DBG_LOOP_PRINTK("[EDC] insert earphone pierce 3 \n");
		}

	    /* GPIO-EXCOL11=H*/
		/*gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_HIGH );*/

	    ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(GPIO_KO_JMIC_ONOFF), &ko_jmic_onoff_out_H );
		if( ret < 0 ) {
			printk("[EDC] ERROR(%s):GPIO_KO_JMIC_ONOFF pm8xxx_gpio_config() (%d)\n",__func__,ret );
			return ret;
		}
	}
	else{

		*pierce_bitset |= HEDSET_CHECK_BIT;

		/* GPIO-EXCOL11=L*/
		gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_LOW );
		
		if( debug_mode )
			EDC_DBG_PRINTK("[EDC] insert normal earphone\n");
		else
			EDC_DBG_LOOP_PRINTK("[EDC] insert normal earphone\n");
	}

	if( edc_mic_bias_flag == 0 ){
		/* YUM829-MIC3 OFF */
		ret = ymu_control(YMU_MICBIAS_OFF,0);
		if (ret != MCDRV_SUCCESS)
		{
			printk("[EDC] ERROR(%s):ymu_control() YMU_MICBIAS_OFF \n",__func__);
			return EDC_NG;
		}
	}

	if( debug_mode )
		EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
	else
		EDC_DBG_LOOP_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_nv_read													  */
/**********************************************************************/
int edc_hw_nv_read(void)
{
	int ret=EDC_OK;
	
#if OPT_NV_READ
	uint8_t val[EDC_APNV_PIERCE_JUDGE_I_SIZE];
	int retry_count=0;
	int apnv_id=0;
#endif

	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

	memset( val , 0 , EDC_APNV_PIERCE_JUDGE_I_SIZE );
	
	pierce_id_1000_ohm_low  = ADC_1000_LOW;
	pierce_id_1000_ohm_high = ADC_1000_HIGH;
	pierce_id_1500_ohm_low  = ADC_1500_LOW;
	pierce_id_1500_ohm_high = ADC_1500_HIGH;
	pierce_id_2200_ohm_low  = ADC_2200_LOW;
	pierce_id_2200_ohm_high = ADC_2200_HIGH;

#if OPT_NV_READ

	apnv_id = EDC_APNV_PIERCE_JUDGE_I;
	if( edc_machine_type == EDC_MACHINE_TYPE_FJDEV002 &&
		edc_board_type >= EDC_BOARD_TYPE_FJDEV002_RYO_2 ){
		apnv_id = EDC_APNV_PIERCE_JUDGE_II;
	}

	while (retry_count < EDC_RETRY_COUNT_MAX) {
		ret = get_nonvolatile(val, apnv_id , EDC_APNV_PIERCE_JUDGE_I_SIZE);
		if (likely(ret < 0)) {
			retry_count++;
			msleep(20);
		}
		else{
			break;
		}
	}
	if (unlikely(retry_count >= EDC_RETRY_COUNT_MAX)) {
		printk("[EDC] ERROR(%s):get_nonvolatile() (%d)\n",__func__,ret);
		return ret;
	}

	if( ret != EDC_APNV_PIERCE_JUDGE_I_SIZE ){
		printk("[EDC] ERROR(%s):get_nonvolatile() read size err(%d)\n",__func__,ret);
		return EDC_NG;
	}
	else{
		ret = EDC_OK;
	}
	
	pierce_id_1000_ohm_low = val[0];
	pierce_id_1000_ohm_low = ((pierce_id_1000_ohm_low << 8 ) | val[1]);
	
	pierce_id_1000_ohm_high = val[2];
	pierce_id_1000_ohm_high = ((pierce_id_1000_ohm_high << 8 ) | val[3]);
	
	pierce_id_1500_ohm_low = val[4];
	pierce_id_1500_ohm_low = ((pierce_id_1500_ohm_low << 8 ) | val[5]);
	
	pierce_id_1500_ohm_high = val[6];
	pierce_id_1500_ohm_high = ((pierce_id_1500_ohm_high << 8 ) | val[7]);
	
	pierce_id_2200_ohm_low = val[8];
	pierce_id_2200_ohm_low = ((pierce_id_2200_ohm_low << 8 ) | val[9]);
	
	pierce_id_2200_ohm_high = val[10];
	pierce_id_2200_ohm_high = ((pierce_id_2200_ohm_high << 8 ) | val[11]);

#endif
	
	printk("[EDC] pierce_id_1000_ohm_low  = 0x%x\n",pierce_id_1000_ohm_low);
	printk("[EDC] pierce_id_1000_ohm_high = 0x%x\n",pierce_id_1000_ohm_high);
	printk("[EDC] pierce_id_1500_ohm_low  = 0x%x\n",pierce_id_1500_ohm_low);
	printk("[EDC] pierce_id_1500_ohm_high = 0x%x\n",pierce_id_1500_ohm_high);
	printk("[EDC] pierce_id_2200_ohm_low  = 0x%x\n",pierce_id_2200_ohm_low);
	printk("[EDC] pierce_id_2200_ohm_high = 0x%x\n",pierce_id_2200_ohm_high);

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_check_wakwlock											  */
/**********************************************************************/
int edc_hw_check_wakelock( void )
{
	int ret=EDC_OK;
	
	EDC_DBG_PRINTK("[EDC] %s()\n", __func__);

#if OPT_USB_EARPHONE
	if ( edc_UsbEarphoneFlag==0 && edc_FaiEarphoneFlag==0 && edc_voice_call_flag == 0)
#else
	if ( edc_FaiEarphoneFlag==0 && edc_voice_call_flag == 0)
#endif
	{
		EDC_DBG_PRINTK("[EDC] wake_unlock 1\n");
		wake_unlock(&edc_wlock_gpio);
		return ret;
	}
	if (edc_inout_timer_expire)
	{
		EDC_DBG_PRINTK("[EDC] timer expire %04x\n", edc_inout_timer_expire);
		return ret;
	}

	if (edc_voice_call_flag)
	{
		if (edc_Hedset_Switch_enable_Flag)
		{
			EDC_DBG_PRINTK("[EDC] wake_lock [voice & switch]\n");
		/*	wake_lock(&edc_wlock_gpio); */
		}
		else
		{
			EDC_DBG_PRINTK("[EDC] wake_lock [voice & non switch]\n");
			wake_unlock(&edc_wlock_gpio);
		}
	}
	else
	{
		EDC_DBG_PRINTK("[EDC] wake_unlock [non voice call]\n");
		wake_unlock(&edc_wlock_gpio);
	}

	return ret;
}

/**********************************************************************/
/* edc_hw_chedule_delayed_work_que_loop								  */
/**********************************************************************/
void edc_hw_chedule_delayed_work_que_loop( void )
{
	plug_loop_call = 1;
	schedule_delayed_work(&work_que_loop, msecs_to_jiffies(PHI35_LOOP));
}

/**********************************************************************/
/* edc_hw_earphone_enable_irq										  */
/**********************************************************************/
void edc_hw_earphone_enable_irq( int irq )
{
	struct irq_data *d;

	if( irq == edc_port[EDC_PLUG].gpio_det_irq ){
		EDC_DBG_IRQ_PRINTK( "[EDC] PLUG ");
	}
	else if( irq == edc_port[EDC_SWITCH].gpio_det_irq ){
		EDC_DBG_IRQ_PRINTK( "[EDC] SWITCH ");
	}
	else{
		printk( "[EDC] %s Err irq NG %d\n",__func__,irq);
	}

	d = irq_get_irq_data(irq);
	if(d->state_use_accessors & IRQD_IRQ_DISABLED){
		enable_irq(irq);
		EDC_DBG_IRQ_PRINTK( "enable_irq() set\n");
	}
	else{
		EDC_DBG_IRQ_PRINTK( "enable_irq() skip\n");
	}
}

/**********************************************************************/
/* edc_hw_earphone_disable_irq										  */
/**********************************************************************/
void edc_hw_earphone_disable_irq( int irq )
{
	struct irq_data *d;

	if( irq == edc_port[EDC_PLUG].gpio_det_irq ){
		EDC_DBG_IRQ_PRINTK( "[EDC] PLUG ");
	}
	else if( irq == edc_port[EDC_SWITCH].gpio_det_irq ){
		EDC_DBG_IRQ_PRINTK( "[EDC] SWITCH ");
	}
	else{
		printk( "[EDC] %s Err irq NG %d\n",__func__,irq);
	}

	d = irq_get_irq_data(irq);
	if((d->state_use_accessors & IRQD_IRQ_DISABLED)==0){
		disable_irq(irq);
		EDC_DBG_IRQ_PRINTK( " disable_irq() set\n");
	}
	else{
		EDC_DBG_IRQ_PRINTK( " disable_irq() skip\n");
	}
}

/**********************************************************************/
/*  edc_state_check													  */
/**********************************************************************/
int edc_state_check( int cu_num , int be_num )
{
	int ret=EDC_STATE_OK;

	if( cu_num >= EDC_STATE_MAX ){
		printk( "[EDC] %s Err cu_num NG %d\n",__func__,cu_num);
		return (EDC_STATE_NG);
	}
	if( be_num >= EDC_STATE_MAX ){
		printk( "[EDC] %s Err be_num NG %d\n",__func__,be_num);
		return (EDC_STATE_NG);
	}
	ret = edc_state_matrix[cu_num][be_num];
	EDC_DBG_STATE_PRINTK( "[EDC] %s cu_num=%d be_num=%d ret=%d\n",__func__,cu_num,be_num,ret);
	return ret;
}

/**********************************************************************/
/*  edc_state_NG													  */
/**********************************************************************/
void edc_state_NG( int cu_num , int be_num )
{
	printk( "[EDC] %s state Error cu_num=%d be_num=%d\n",__func__,cu_num,be_num);
	edc_base_portstatus = -1;
	plug_loop_call = 1;
	schedule_delayed_work(&work_que_loop, msecs_to_jiffies(PHI35_LOOP)); /* LOOP(S1) move */
}

/* TEST */
/**********************************************************************/
/* edc_hw_suspend_loop												  */
/**********************************************************************/
void edc_hw_suspend_loop(struct work_struct *work)
{
	edc_hw_suspend();

	schedule_delayed_work(&work_que_resume, msecs_to_jiffies(60*1000));

}
/**********************************************************************/
/* edc_hw_resume_loop												  */
/**********************************************************************/
void edc_hw_resume_loop(struct work_struct *work)
{
	edc_hw_resume();

	schedule_delayed_work(&work_que_suspend, msecs_to_jiffies(60*1000));

}
extern int edc_sw_earphone_det_output( void );
extern int edc_sw_earphone_status_get( unsigned long arg );
/**********************************************************************/
/* edc_hw_mc_loop												 	  */
/**********************************************************************/
void edc_hw_mc_loop(struct work_struct *work)
{
	unsigned long arg=0;

	edc_sw_earphone_det_output();

	edc_sw_earphone_status_get( arg );

	schedule_delayed_work(&work_que_mc, msecs_to_jiffies(10500));

}

/**********************************************************************/
/* edc_hw_earphone_xled_rst_on										  */
/**********************************************************************/
int edc_hw_earphone_xled_rst_on( void )
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/

	/* GPIO-EXCOL7=H */
	gpio_set_value_cansleep( GPIO_KO_XLED_RST , EDC_GPIO_HIGH );

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_xled_rst_off										  */
/**********************************************************************/
int edc_hw_earphone_xled_rst_off( void )
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/

	/* GPIO-EXCOL7=L */
	gpio_set_value_cansleep( GPIO_KO_XLED_RST , EDC_GPIO_LOW );

	EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);

	return ret;
}

int edc_hw_earphone_illumi_log_flg=0;
/**********************************************************************/
/* edc_hw_earphone_illumi_on										  */
/**********************************************************************/
int edc_hw_earphone_illumi_on( void )
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/

	if( edc_Headset_Definition_Flag & PIERCE_ID_ALL_CHECK_BIT ){
		/* GPIO-EXCOL11=H */
		gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_HIGH );
		if(  edc_hw_earphone_illumi_log_flg == 0 ){
			EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
			edc_hw_earphone_illumi_log_flg = 1;
		}
	}

	/*EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);*/

	return ret;
}

/**********************************************************************/
/* edc_hw_earphone_illumi_off										  */
/**********************************************************************/
int edc_hw_earphone_illumi_off( void )
{
	int ret=EDC_OK;

	/*EDC_DBG_PRINTK("[EDC] %s()\n", __func__);*/
	
	/* GPIO-EXCOL11=L */
	gpio_set_value_cansleep( GPIO_SEL_JEAR_LED , EDC_GPIO_LOW );

	if( edc_Headset_Definition_Flag & PIERCE_ID_ALL_CHECK_BIT ){
		if( edc_hw_earphone_illumi_log_flg == 1 ){
			EDC_DBG_PRINTK("[EDC] %s() OK\n", __func__);
			edc_hw_earphone_illumi_log_flg = 0;
		}
	}

	return ret;
}
