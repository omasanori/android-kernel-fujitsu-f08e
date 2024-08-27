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

/**********************************************************************/
/* prototypes		 												  */
/**********************************************************************/
int edc_hw_init( void );
int edc_hw_exit( void );
int edc_hw_suspend( void );
int edc_hw_resume( void );

int edc_hw_setup_config(void);
int edc_hw_delete_config(void);
int edc_hw_nv_read(void);

void edc_hw_gpio_plug_loop(struct work_struct *work);
void edc_hw_gpio_plug_chatt(struct work_struct *work);
void edc_hw_gpio_plug_decision(struct work_struct *work);

void edc_hw_handler_plug(struct work_struct *work);
void edc_hw_handler_switch(struct work_struct *work);
irqreturn_t edc_hw_irq_handler_switch(int irq, void *data);
irqreturn_t edc_hw_irq_handler_inout(int irq, void *data);
/* TEST */
void edc_hw_suspend_loop( struct work_struct *work);
void edc_hw_resume_loop( struct work_struct *work);
void edc_hw_mc_loop( struct work_struct *work);

int edc_hw_earphone_phi35_use( void );
int edc_hw_earphone_phi35_unuse( void );
int edc_hw_earphone_switch_use( void );
int edc_hw_earphone_switch_unuse( void );
int edc_hw_earphone_mic_push( void );
int edc_hw_earphone_mic_release( void );
int edc_hw_check_wakelock( void );

#if OPT_USB_EARPHONE
int edc_hw_usb_earphone_use( int );
int edc_hw_usb_earphone_unuse( int );
#endif
int edc_hw_earphone_det_output( void );
int edc_hw_earphone_status_get( struct earphone_status *earphone_read_status );

int edc_hw_earphone_xled_rst_on( void );
int edc_hw_earphone_xled_rst_off( void );
int edc_hw_earphone_illumi_on( void );
int edc_hw_earphone_illumi_off( void );

int edc_hw_pm8921_gpio_control2(int gpio, int highlow);
int edc_hw_earphone_voice_call_stop( void );

int edc_hw_earphone_mic_push( void );
int edc_hw_earphone_mic_release( void );

int edc_hw_earphone_insert(int *pierce_bitset );
int edc_hw_earphone_remove(int *pierce_bitset , int debug_mode );

/*static DECLARE_DELAYED_WORK(work_que_loop, edc_hw_gpio_plug_loop);
static DECLARE_DELAYED_WORK(work_que_chatt, edc_hw_gpio_plug_chatt);
static DECLARE_DELAYED_WORK(work_que_decision, edc_hw_gpio_plug_decision);
static DECLARE_DELAYED_WORK(work_que_plug, edc_hw_handler_plug);
static DECLARE_DELAYED_WORK(work_que_switch, edc_hw_handler_switch);*/

/*static DECLARE_WAIT_QUEUE_HEAD(edc_wq);*/

void edc_hw_wake_up_interruptible(void);
void edc_hw_chedule_delayed_work_que_loop( void );

void edc_hw_earphone_enable_irq( int irq );
void edc_hw_earphone_disable_irq( int irq );

int edc_hw_earphone_loop_pierce_get( int *pierce_bitset , int debug_mode , int *adc_code);


