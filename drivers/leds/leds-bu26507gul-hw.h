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
#ifndef __LEDS_BU26507GUL_HW__
#define __LEDS_BU26507GUL_HW__

/* BU26507GUL GPIO */
#define BU26507GUL_ExGPIO_XLED_RST	BU1852_EXP1_GPIO_BASE + 15
#define BU26507GUL_RESETB_LOW		0
#define BU26507GUL_RESETB_HIGH		1

/* BU26507GUL Data */
/* #define BU26507GUL_REG_SFTRST		0x00 */
#define BU26507GUL_REG_SFTRST_SFTRST				0x01

/* #define BU26507GUL_REG_OSCEN		0x01 */
#define BU26507GUL_REG_OSCEN_OSCEN					0x08

/* #define BU26507GUL_REG_LED1LED6ON	0x11 */
#define SHIFT_BU26507GUL_REG_LED1LED6ON_LED1		0
#define MASK_BU26507GUL_REG_LED1LED6ON_LED1			0x01
#define SHIFT_BU26507GUL_REG_LED1LED6ON_LED2		1
#define MASK_BU26507GUL_REG_LED1LED6ON_LED2			0x01
#define SHIFT_BU26507GUL_REG_LED1LED6ON_LED3		2
#define MASK_BU26507GUL_REG_LED1LED6ON_LED3			0x01
#define SHIFT_BU26507GUL_REG_LED1LED6ON_LED4		3
#define MASK_BU26507GUL_REG_LED1LED6ON_LED4			0x01
#define SHIFT_BU26507GUL_REG_LED1LED6ON_LED5		4
#define MASK_BU26507GUL_REG_LED1LED6ON_LED5			0x01
#define SHIFT_BU26507GUL_REG_LED1LED6ON_LED6		5
#define MASK_BU26507GUL_REG_LED1LED6ON_LED6			0x01

/* #define BU26507GUL_REG_PWMSET		0x20 */
#define SHIFT_BU26507GUL_REG_PWMSET					0
#define MASK_BU26507GUL_REG_PWMSET					0x3F

/* #define BU26507GUL_REG_CLKSEL		0x21 */
#define SHIFT_BU26507GUL_REG_CLKSEL_CLKIN			0
#define MASK_BU26507GUL_REG_CLKSEL_CLKIN			0x01
#define SHIFT_BU26507GUL_REG_CLKSEL_CLKOUT			1
#define MASK_BU26507GUL_REG_CLKSEL_CLKOUT			0x01
#define SHIFT_BU26507GUL_REG_CLKSEL_SYNCON			2
#define MASK_BU26507GUL_REG_CLKSEL_SYNCON			0x01
#define SHIFT_BU26507GUL_REG_CLKSEL_SYNCACT			3
#define MASK_BU26507GUL_REG_CLKSEL_SYNCACT			0x01
#define SHIFT_BU26507GUL_REG_CLKSEL_CLKSEL			6
#define MASK_BU26507GUL_REG_CLKSEL_CLKSEL			0x03

/* #define BU26507GUL_REG_SLP			0x2D */
#define SHIFT_BU26507GUL_REG_SLP_SCLEN				0
#define MASK_BU26507GUL_REG_SLP_SCLEN				0x01
#define SHIFT_BU26507GUL_REG_SLP_SLPEN				1
#define MASK_BU26507GUL_REG_SLP_SLPEN				0x01
#define SHIFT_BU26507GUL_REG_SLP_PWMEN				2
#define MASK_BU26507GUL_REG_SLP_PWMEN				0x01
#define SHIFT_BU26507GUL_REG_SLP_SLP				3
#define MASK_BU26507GUL_REG_SLP_SLP					0x03

/* #define BU26507GUL_REG_SCLRST		0x2E */
#define BU26507GUL_REG_SCLRST_SCLRST				0x01

/* #define BU26507GUL_REG_SCLSPEED		0x2F */
#define SHIFT_BU26507GUL_REG_SCLSPEED_LEFT			0
#define MASK_BU26507GUL_REG_SCLSPEED_LEFT			0x01
#define SHIFT_BU26507GUL_REG_SCLSPEED_RIGHT			1
#define MASK_BU26507GUL_REG_SCLSPEED_RIGHT			0x01
#define SHIFT_BU26507GUL_REG_SCLSPEED_DOWN			2
#define MASK_BU26507GUL_REG_SCLSPEED_DOWN			0x01
#define SHIFT_BU26507GUL_REG_SCLSPEED_UP			3
#define MASK_BU26507GUL_REG_SCLSPEED_UP				0x01
#define SHIFT_BU26507GUL_REG_SCLSPEED_SCLSPEED		4
#define MASK_BU26507GUL_REG_SCLSPEED_SCLSPEED		0x07
#define SHIFT_BU26507GUL_REG_SCLSPEED_SCLSPEEDUP	7
#define MASK_BU26507GUL_REG_SCLSPEED_SCLSPEEDUP		0x01

/* #define BU26507GUL_REG_START		0x30 */
#define SHIFT_BU26507GUL_REG_START_START			0
#define MASK_BU26507GUL_REG_START_START				0x01

/* #define BU26507GUL_REG_CLRAB		0x31 */
#define SHIFT_BU26507GUL_REG_CLRAB_CLRA				0
#define MASK_BU26507GUL_REG_CLRAB_CLRA				0x01
#define SHIFT_BU26507GUL_REG_CLRAB_CLRB				1
#define MASK_BU26507GUL_REG_CLRAB_CLRB				0x01

/* #define BU26507GUL_REG_RMCG_OAB_IAB	0x7F */
#define SHIFT_BU26507GUL_REG_RMCG_OAB_IAB_RMCG		0
#define MASK_BU26507GUL_REG_RMCG_OAB_IAB_RMCG		0x01
#define SHIFT_BU26507GUL_REG_RMCG_OAB_IAB_OAB		1
#define MASK_BU26507GUL_REG_RMCG_OAB_IAB_OAB		0x01
#define SHIFT_BU26507GUL_REG_RMCG_OAB_IAB_IAB		2
#define MASK_BU26507GUL_REG_RMCG_OAB_IAB_IAB		0x01

/* #define BU26507GUL_REG_SW1LED1		0x01 */
#define BU26507GUL_REG_MATRIX						BU26507GUL_REG_SW1LED1
#define SHIFT_BU26507GUL_REG_MATRIX_ILEDAXXSET		0
#define MASK_BU26507GUL_REG_MATRIX_ILEDAXXSET		0x0F
#define SHIFT_BU26507GUL_REG_MATRIX_SDLYAXX			4
#define MASK_BU26507GUL_REG_MATRIX_SDLYAXX			0x30
#define SHIFT_BU26507GUL_REG_MATRIX_SCYCAXX			6
#define MASK_BU26507GUL_REG_MATRIX_SCYCAXX			0xC0

/* Retry Counter */
#define BU26507GUL_I2C_RETRY_MAX			2

/* bu26507gul_hw_7fh_rmcg_ab Parameter */
#define BU26507GUL_CONTROL_CONT				0
#define BU26507GUL_CONTROL_MATRIX			1

#define BU26507GUL_DISPMATRIX_A				0
#define BU26507GUL_DISPMATRIX_B				1
#define BU26507GUL_DISPMATRIX_DONT_CARE		-1

#define BU26507GUL_WRITEMATRIX_A			0
#define BU26507GUL_WRITEMATRIX_B			1
#define BU26507GUL_WRITEMATRIX_DONT_CARE	-1

/* bu26507gul_hw_11h_led_onoff Parameter */
#define BU26507GUL_HW_11H__LED_ON			1
#define BU26507GUL_HW_11H__LED_OFF			0

/* bu26507gul_hw_20h_pwm_duty Parameter */
#define BU26507GUL_HW_20H_PWM_DUTY_MAX		63

/* bu26507gul_hw_2dh_pwm_slp_scl Parameter */
#define BU26507GUL_HW_2DH_SCROLL_ON			1
#define BU26507GUL_HW_2DH_SCROLL_OFF		0
#define BU26507GUL_HW_2DH_PWM_ON			1
#define BU26507GUL_HW_2DH_PWM_OFF			0
#define BU26507GUL_HW_2DH_SLOOP_ON			1
#define BU26507GUL_HW_2DH_SLOOP_OFF			0

/* bu26507gul_hw_30h_start Parameter */
#define BU26507GUL_HW_30H_START				1
#define BU26507GUL_HW_30H_STOP				0

/* bu26507gul_hw_matrix_set Parameter */
#define BU26507GUL_LEDELEC_MIN				0
#define BU26507GUL_SDLY_MIN					0
#define BU26507GUL_SCYC_MIN					0
#define BU26507GUL_PIERCE_LED_NO			0x1A

/* prot type */
extern int bu26507gul_hw_init(void);
extern int bu26507gul_hw_exit(void);
extern int bu26507gul_hw_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int bu26507gul_hw_remove(struct i2c_client *client);
extern int bu26507gul_hw_suspend(struct device *dev);
extern int bu26507gul_hw_resume(struct device *dev);
extern int bu26507gul_hw_power_on(void);
extern int bu26507gul_hw_power_off(void);
extern int bu26507gul_hw_7fh_rmcg_ab(int p_rmcg, int p_oab, int p_iab);
extern int bu26507gul_hw_00h_softreset(void);
extern int bu26507gul_hw_01h_oscen(void);
extern int bu26507gul_hw_11h_led_onoff(int p_led1, int p_led2, int p_led3, int p_led4, int p_led5, int p_led6);
extern int bu26507gul_hw_20h_pwm_duty(int p_pwmduty);
extern int bu26507gul_hw_21h_clk_ctl(int p_clkin, int p_clkout, int p_syncon, int p_syncact, int p_clksel);
extern int bu26507gul_hw_2dh_pwm_slp_scl(int p_scl_flg, int p_pwm_flg, int p_slp_flg, int p_slp);
extern int bu26507gul_hw_2eh_scl_reset(void);
extern int bu26507gul_hw_2fh_scl_set(int p_scl_left, int p_scl_right, int p_scl_up,int p_scl_down,int p_scl_spd,int p_scl_sup);
extern int bu26507gul_hw_30h_start(int p_onoff);
extern int bu26507gul_hw_31h_matrix_clr(int p_aclr,int p_bclr);
extern int bu26507gul_hw_matrix_sloop_cycle(int p_ledno, int p_scyc);
extern int bu26507gul_hw_matrix_sloop_delay(int p_ledno, int p_sdly);
extern int bu26507gul_hw_matrix_brightness(int p_ledno, int p_ledelec);
extern int bu26507gul_hw_matrix_set(int p_ledno, int p_ledelec, int p_sdly, int p_scyc);
extern int bu26507gul_hw_matrix_clr( void );

#endif /* __LEDS_BU26507GUL_HW__ */
