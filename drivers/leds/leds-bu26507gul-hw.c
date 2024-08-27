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
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "../arch/arm/mach-msm/board-8064.h"
#include "leds-bu26507gul-init.h"
#include "leds-bu26507gul-hw.h"

/* GPIO device name */
#define DEV_NAME_BU26507GUL	"BU26507GUL"

/* Global parameter*/
static	int							g_bu26507gul_control;
static	int							g_bu26507gul_display;
static	int							g_bu26507gul_write;
static	unsigned char				g_bu26507gul_matrix[2][30];
static	struct i2c_client			*gp_bu26507gul_i2ccli;
static	const struct i2c_device_id	*gp_bu26507gul_i2cid;

/* function */
int bu26507gul_hw_init(void)
{
	illumination_info_printk("[%s] \n",__func__);
	return (0);
}

int bu26507gul_hw_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int	ret = 0;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_power_on();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_power_on ret=%d \n",__func__,ret);
		return (ret);
	}
	g_bu26507gul_control = BU26507GUL_CONTROL_CONT;
	g_bu26507gul_display = BU26507GUL_DISPMATRIX_A;
	g_bu26507gul_write   = BU26507GUL_WRITEMATRIX_A;
	memset(g_bu26507gul_matrix, 0, sizeof(g_bu26507gul_matrix));

	gp_bu26507gul_i2ccli = client;
	gp_bu26507gul_i2cid  = id;

	return (0);
}

int bu26507gul_hw_suspend(struct device *dev)
{
	illumination_info_printk("[%s] \n",__func__);
	return (0);
}

int bu26507gul_hw_resume(struct device *dev)
{
	illumination_info_printk("[%s] \n",__func__);
	return (0);
}

int bu26507gul_hw_remove(struct i2c_client *client)
{
	int	ret = 0;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_power_off();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_power_off ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_exit(void)
{
	illumination_info_printk("[%s] \n",__func__);
	return (0);
}

int bu26507gul_hw_power_on(void)
{
	int ret = 0;

	illumination_info_printk("[%s] \n",__func__);
	/* gpio request */
	ret = gpio_request(BU26507GUL_ExGPIO_XLED_RST, DEV_NAME_BU26507GUL);
	if (unlikely(ret)) {
		illumination_error_printk("[%s]  gpio_request failed(%d) \n", __func__, ret);
		return ret;
	}
	/* set gpio output */
	ret = gpio_direction_output(BU26507GUL_ExGPIO_XLED_RST, 0);
	if (unlikely(ret)) {
		illumination_error_printk("[%s]  gpio_direction_output failed(%d) \n", __func__, ret);
		gpio_free(BU26507GUL_ExGPIO_XLED_RST);
		return ret;
	}
	/* RESETB LOW */
	gpio_set_value_cansleep(BU26507GUL_ExGPIO_XLED_RST, BU26507GUL_RESETB_LOW);
	/* 1ms wait */
	msleep(1);
	/* RESETB HIGH */
	gpio_set_value_cansleep(BU26507GUL_ExGPIO_XLED_RST, BU26507GUL_RESETB_HIGH);
	/* 10ms wait */
	msleep(10);
	return (0);
}

int bu26507gul_hw_power_off(void)
{
	illumination_info_printk("[%s] \n",__func__);
	/* RESETB LOW */
	gpio_set_value_cansleep(BU26507GUL_ExGPIO_XLED_RST, BU26507GUL_RESETB_LOW);
	/* gpio release */
	gpio_free(BU26507GUL_ExGPIO_XLED_RST);
	return (0);
}

int bu26507gul_hw_i2c_register_write(uint8_t p_register, uint8_t p_data)
{
	uint8_t	buffer[2];
	int		ret = 0;
	int		retry_count = 0;

	illumination_info2_printk("[%s] p_register = %02x p_data = %02x \n",__func__, p_register, p_data);
	buffer[0] = p_register;
	buffer[1] = p_data;

	for (;;) {
		ret = i2c_master_send(gp_bu26507gul_i2ccli, buffer, 2);
		if (likely(ret >= 0)) {
			ret = 0;
			break;
		}
		illumination_debug_printk("[%s]  i2c_master_send ret=%d \n",__func__,ret);
		if (unlikely(retry_count >= BU26507GUL_I2C_RETRY_MAX)) {
			illumination_error_printk("[%s]  i2c_master_send retry over \n",__func__);
			return (ret);
		}
		msleep(10);
		retry_count++;
	}
	return ret;
}

int bu26507gul_hw_matrix_clr( void )
{
	uint8_t	buffer[32];
	int		ret = 0;
	int		retry_count = 0;

	illumination_info2_printk("[%s]  \n",__func__);

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = 1;

	for (;;) {
		ret = i2c_master_send(gp_bu26507gul_i2ccli, buffer, 31);
		if (likely(ret >= 0)) {
			ret = 0;
			break;
		}
		illumination_debug_printk("[%s]  i2c_master_send ret=%d \n",__func__,ret);
		if (unlikely(retry_count >= BU26507GUL_I2C_RETRY_MAX)) {
			illumination_error_printk("[%s]  i2c_master_send retry over \n",__func__);
			return (ret);
		}
		msleep(10);
		retry_count++;
	}
	return ret;
}


int bu26507gul_hw_7fh_rmcg_ab(int p_rmcg, int p_oab, int p_iab)
{
	int		ret;
	uint8_t	rmcg;
	uint8_t	oab;
	uint8_t	iab;
	uint8_t	write = 0;

	illumination_info_printk("[%s] p_rmcg = %d p_oab = %d p_iab = %d \n",__func__, p_rmcg, p_oab, p_iab);
	rmcg = p_rmcg & MASK_BU26507GUL_REG_RMCG_OAB_IAB_RMCG;
	if (p_oab == BU26507GUL_DISPMATRIX_DONT_CARE) {
		oab = g_bu26507gul_display;
	} else {
		oab = p_oab & MASK_BU26507GUL_REG_RMCG_OAB_IAB_IAB;
	}
	if (p_iab == BU26507GUL_WRITEMATRIX_DONT_CARE) {
		iab = g_bu26507gul_write;
	} else {
		iab = p_iab & MASK_BU26507GUL_REG_RMCG_OAB_IAB_OAB;
	}
	write = (iab    << SHIFT_BU26507GUL_REG_RMCG_OAB_IAB_IAB)
		  | (oab    << SHIFT_BU26507GUL_REG_RMCG_OAB_IAB_OAB)
		  | (p_rmcg << SHIFT_BU26507GUL_REG_RMCG_OAB_IAB_RMCG);
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_RMCG_OAB_IAB, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	g_bu26507gul_control = rmcg;
	g_bu26507gul_display = oab;
	g_bu26507gul_write   = iab;
	return (0);
}

int bu26507gul_hw_00h_softreset(void)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_SFTRST, BU26507GUL_REG_SFTRST_SFTRST);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_01h_oscen(void)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_OSCEN, BU26507GUL_REG_OSCEN_OSCEN);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_11h_led_onoff(int p_led1,int p_led2,int p_led3,int p_led4,int p_led5,int p_led6)
{
	int		ret;
	uint8_t	write = 0;

	illumination_info_printk("[%s] p_led1 = %d p_led2 = %d p_led3 = %d p_led4 = %d p_led5 = %d p_led6 = %d \n",__func__, p_led1, p_led2, p_led3, p_led4, p_led5, p_led6);
	write = (((p_led1 & MASK_BU26507GUL_REG_LED1LED6ON_LED1) << SHIFT_BU26507GUL_REG_LED1LED6ON_LED1)
		  |  ((p_led2 & MASK_BU26507GUL_REG_LED1LED6ON_LED2) << SHIFT_BU26507GUL_REG_LED1LED6ON_LED2)
		  |  ((p_led3 & MASK_BU26507GUL_REG_LED1LED6ON_LED3) << SHIFT_BU26507GUL_REG_LED1LED6ON_LED3)
		  |  ((p_led4 & MASK_BU26507GUL_REG_LED1LED6ON_LED4) << SHIFT_BU26507GUL_REG_LED1LED6ON_LED4)
		  |  ((p_led5 & MASK_BU26507GUL_REG_LED1LED6ON_LED5) << SHIFT_BU26507GUL_REG_LED1LED6ON_LED5)
		  |  ((p_led6 & MASK_BU26507GUL_REG_LED1LED6ON_LED6) << SHIFT_BU26507GUL_REG_LED1LED6ON_LED6));
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_LED1LED6ON, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_20h_pwm_duty(int p_pwmduty)
{
	int		ret;
	uint8_t	write = 0;

	illumination_info_printk("[%s] p_pwmduty = %d \n",__func__, p_pwmduty);
	write = ((p_pwmduty & MASK_BU26507GUL_REG_PWMSET) << SHIFT_BU26507GUL_REG_PWMSET);
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_PWMSET, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_21h_clk_ctl(int p_clkin,int p_clkout,int p_syncon,int p_syncact, int p_clksel)
{
	int		ret;
	uint8_t	write = 0;

	illumination_info_printk("[%s] p_clkin = %d p_clkout = %d p_syncon = %d p_syncact = %d p_clksel = %d \n",__func__, p_clkin,p_clkout, p_syncon, p_syncact, p_clksel);
	write = (((p_clkin & MASK_BU26507GUL_REG_CLKSEL_CLKIN)     << SHIFT_BU26507GUL_REG_CLKSEL_CLKIN)
		  |  ((p_clkout & MASK_BU26507GUL_REG_CLKSEL_CLKOUT)   << SHIFT_BU26507GUL_REG_CLKSEL_CLKOUT)
		  |  ((p_syncon & MASK_BU26507GUL_REG_CLKSEL_SYNCON)   << SHIFT_BU26507GUL_REG_CLKSEL_SYNCON)
		  |  ((p_syncact & MASK_BU26507GUL_REG_CLKSEL_SYNCACT) << SHIFT_BU26507GUL_REG_CLKSEL_SYNCACT)
		  |  ((p_clksel & MASK_BU26507GUL_REG_CLKSEL_CLKSEL)   << SHIFT_BU26507GUL_REG_CLKSEL_CLKSEL));
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_CLKSEL, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_2dh_pwm_slp_scl(int p_scl_flg,int p_pwm_flg, int p_slp_flg, int p_slp)
{
	int		ret;
	uint8_t	write = 0;

	illumination_info_printk("[%s] p_scl_flg = %d p_pwm_flg = %d p_slp_flg = %d int p_slp = %d\n",__func__, p_scl_flg, p_pwm_flg, p_slp_flg, p_slp);
	write = (((p_scl_flg & MASK_BU26507GUL_REG_SLP_SCLEN) << SHIFT_BU26507GUL_REG_SLP_SCLEN)
		  |  ((p_slp_flg & MASK_BU26507GUL_REG_SLP_SLPEN) << SHIFT_BU26507GUL_REG_SLP_SLPEN)
		  |  ((p_pwm_flg & MASK_BU26507GUL_REG_SLP_PWMEN) << SHIFT_BU26507GUL_REG_SLP_PWMEN)
		  |  ((p_slp & MASK_BU26507GUL_REG_SLP_SLP) << SHIFT_BU26507GUL_REG_SLP_SLP));
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_SLP, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_2eh_scl_reset(void)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_SCLRST, BU26507GUL_REG_SCLRST_SCLRST);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_2fh_scl_set(int p_scl_left, int p_scl_right, int p_scl_up,int p_scl_down,int p_scl_spd,int p_scl_sup)
{
	int		ret;
	uint8_t	write = 0;

	illumination_info_printk("[%s] p_scl_left = %d p_scl_right = %d p_scl_up = %d p_scl_down = %d p_scl_spd = %d p_scl_sup = %d\n",__func__, p_scl_left, p_scl_right, p_scl_up, p_scl_down, p_scl_spd, p_scl_sup);
	write = (((p_scl_left  & MASK_BU26507GUL_REG_SCLSPEED_LEFT)       << SHIFT_BU26507GUL_REG_SCLSPEED_LEFT)
		  |  ((p_scl_right & MASK_BU26507GUL_REG_SCLSPEED_RIGHT)      << SHIFT_BU26507GUL_REG_SCLSPEED_RIGHT)
		  |  ((p_scl_up    & MASK_BU26507GUL_REG_SCLSPEED_UP)         << SHIFT_BU26507GUL_REG_SCLSPEED_UP)
		  |  ((p_scl_down  & MASK_BU26507GUL_REG_SCLSPEED_DOWN)       << SHIFT_BU26507GUL_REG_SCLSPEED_DOWN)
		  |  ((p_scl_spd   & MASK_BU26507GUL_REG_SCLSPEED_SCLSPEED)   << SHIFT_BU26507GUL_REG_SCLSPEED_SCLSPEED)
		  |  ((p_scl_sup   & MASK_BU26507GUL_REG_SCLSPEED_SCLSPEEDUP) << SHIFT_BU26507GUL_REG_SCLSPEED_SCLSPEEDUP));
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_SCLSPEED, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_30h_start(int p_onoff)
{
	int		ret;
	uint8_t	write = 0;

	illumination_info_printk("[%s] p_onoff = %d\n",__func__, p_onoff);
	write = ((p_onoff & MASK_BU26507GUL_REG_START_START) << SHIFT_BU26507GUL_REG_START_START);
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_START, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_31h_matrix_clr(int p_aclr,int p_bclr)
{
	int		ret;
	uint8_t	write = 0;

	illumination_info_printk("[%s] p_aclr = %d p_bclr = %d\n",__func__, p_aclr, p_bclr);
	write = (((p_aclr & MASK_BU26507GUL_REG_CLRAB_CLRA) << SHIFT_BU26507GUL_REG_CLRAB_CLRA)
		  |  ((p_bclr & MASK_BU26507GUL_REG_CLRAB_CLRB) << SHIFT_BU26507GUL_REG_CLRAB_CLRB));
	ret = bu26507gul_hw_i2c_register_write(BU26507GUL_REG_CLRAB, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_hw_matrix_sloop_cycle(int p_ledno, int p_scyc)
{
	int		ret;
	uint8_t	write = 0;
	uint8_t	regno = 0;

	illumination_info_printk("[%s] p_ledno = %d, p_scyc = %d\n",__func__, p_ledno, p_scyc);
	if (g_bu26507gul_control != BU26507GUL_CONTROL_MATRIX) {
		ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_MATRIX, g_bu26507gul_display, g_bu26507gul_write);
		if (unlikely(ret != 0)) {
			illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
			return (ret);
		}
	}
	regno = p_ledno;
	write = ((g_bu26507gul_matrix[g_bu26507gul_write][regno -1] & ~MASK_BU26507GUL_REG_MATRIX_SCYCAXX)
		  | ((p_scyc << SHIFT_BU26507GUL_REG_MATRIX_SCYCAXX)    &  MASK_BU26507GUL_REG_MATRIX_SCYCAXX));
	ret = bu26507gul_hw_i2c_register_write(regno, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	g_bu26507gul_matrix[g_bu26507gul_write][regno -1] = write;
	return (0);
}

int bu26507gul_hw_matrix_sloop_delay(int p_ledno, int p_sdly)
{
	int		ret;
	uint8_t	write = 0;
	uint8_t	regno = 0;

	illumination_info_printk("[%s] p_ledno = %d p_sdly = %d\n",__func__, p_ledno, p_sdly);
	if (g_bu26507gul_control != BU26507GUL_CONTROL_MATRIX) {
		ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_MATRIX, g_bu26507gul_display, g_bu26507gul_write);
		if (unlikely(ret != 0)) {
			illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
			return (ret);
		}
	}
	regno = p_ledno;
	write = ((g_bu26507gul_matrix[g_bu26507gul_write][regno -1] & ~MASK_BU26507GUL_REG_MATRIX_SDLYAXX)
		  | ((p_sdly << SHIFT_BU26507GUL_REG_MATRIX_SDLYAXX)    &  MASK_BU26507GUL_REG_MATRIX_SDLYAXX));
	ret = bu26507gul_hw_i2c_register_write(regno, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	g_bu26507gul_matrix[g_bu26507gul_write][regno -1] = write;
	return (0);
}

int bu26507gul_hw_matrix_brightness(int p_ledno, int p_ledelec)
{
	int		ret;
	uint8_t	write = 0;
	uint8_t	regno = 0;

	illumination_info_printk("[%s] p_ledno = %d p_ledelec = %d\n",__func__, p_ledno, p_ledelec);
	if (g_bu26507gul_control != BU26507GUL_CONTROL_MATRIX) {
		ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_MATRIX, g_bu26507gul_display, g_bu26507gul_write);
		if (unlikely(ret != 0)) {
			illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
			return (ret);
		}
	}
	regno = p_ledno;
	write = ((g_bu26507gul_matrix[g_bu26507gul_write][regno -1]     & ~MASK_BU26507GUL_REG_MATRIX_ILEDAXXSET)
	      | ((p_ledelec << SHIFT_BU26507GUL_REG_MATRIX_ILEDAXXSET)  &  MASK_BU26507GUL_REG_MATRIX_ILEDAXXSET));
	ret = bu26507gul_hw_i2c_register_write(regno, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	g_bu26507gul_matrix[g_bu26507gul_write][regno -1] = write;
	return (0);
}

int bu26507gul_hw_matrix_set(int p_ledno, int p_ledelec, int p_sdly, int p_scyc)
{
	int		ret;
	uint8_t	write = 0;
	uint8_t	regno = 0;

	illumination_info_printk("[%s] p_ledno = %d p_ledelec = %d p_sdly = %d p_scyc = %d\n",__func__, p_ledno, p_ledelec, p_sdly, p_scyc);
	if (g_bu26507gul_control != BU26507GUL_CONTROL_MATRIX) {
		ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_MATRIX, g_bu26507gul_display, g_bu26507gul_write);
		if (unlikely(ret != 0)) {
			illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
			return (ret);
		}
	}
	regno = p_ledno;
	write = (((p_ledelec << SHIFT_BU26507GUL_REG_MATRIX_ILEDAXXSET) & MASK_BU26507GUL_REG_MATRIX_ILEDAXXSET)
		  |  ((p_sdly    << SHIFT_BU26507GUL_REG_MATRIX_SDLYAXX)    & MASK_BU26507GUL_REG_MATRIX_SDLYAXX)
		  |  ((p_scyc    << SHIFT_BU26507GUL_REG_MATRIX_SCYCAXX)    & MASK_BU26507GUL_REG_MATRIX_SCYCAXX));
	ret = bu26507gul_hw_i2c_register_write(p_ledno, write);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_i2c_register_write ret=%d \n",__func__,ret);
		return (ret);
	}
	g_bu26507gul_matrix[g_bu26507gul_write][regno -1] = write;
	return (0);
}
