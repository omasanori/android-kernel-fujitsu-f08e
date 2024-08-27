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
#include "leds-bu26507gul-init.h"
#include "leds-bu26507gul-sw.h"
#include "leds-bu26507gul-hw.h"

/* Global parameter*/

/* function */
int bu26507gul_sw_init(void)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_init();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_init ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int		ret;
	int		iab;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_probe(client,id);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_probe ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_CONT,
									BU26507GUL_DISPMATRIX_DONT_CARE,
									BU26507GUL_WRITEMATRIX_DONT_CARE);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_00h_softreset();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_00h_softreset ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_01h_oscen();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_01h_oscen ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_11h_led_onoff(BU26507GUL_HW_11H__LED_ON,			/* LED1 */
									  BU26507GUL_HW_11H__LED_ON,			/* LED2 */
									  BU26507GUL_HW_11H__LED_ON,			/* LED3 */
									  BU26507GUL_HW_11H__LED_ON,			/* LED4 */
									  BU26507GUL_HW_11H__LED_ON,			/* LED5 */
									  BU26507GUL_HW_11H__LED_ON);			/* LED6 */
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_11h_led_onoff ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_20h_pwm_duty(BU26507GUL_HW_20H_PWM_DUTY_MAX);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_20h_pwm_duty ret=%d \n",__func__,ret);
		return (ret);
	}
	for (iab = BU26507GUL_WRITEMATRIX_B ; iab >= BU26507GUL_WRITEMATRIX_A ; iab --) {
		ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_MATRIX,
										BU26507GUL_DISPMATRIX_DONT_CARE,
										iab);
		if (unlikely(ret != 0)) {
			illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d (iab=%d)\n",__func__,ret,iab);
			return (ret);
		}
		ret = bu26507gul_hw_matrix_clr( );
		if (unlikely(ret != 0)) {
			illumination_debug_printk("[%s]  bu26507gul_hw_matrix_clr ret=%d (iab=%d)\n",__func__,ret,iab);
			return (ret);
		}
	}
	ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_CONT,
									BU26507GUL_DISPMATRIX_A,
									BU26507GUL_WRITEMATRIX_A);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
		return (ret);
	}

	return (0);
}

int bu26507gul_sw_suspend(struct device *dev)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_suspend(dev);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_suspend ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_resume(struct device *dev)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_resume(dev);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_resume ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_remove(struct i2c_client *client)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_CONT,
									BU26507GUL_DISPMATRIX_DONT_CARE,
									BU26507GUL_WRITEMATRIX_DONT_CARE);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_00h_softreset();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_00h_softreset ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_remove(client);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_remove ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_shutdown(struct i2c_client *client)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_power_off();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_power_off ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_exit(void)
{
	int		ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_hw_exit();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_exit ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_sloop_cycle(int p_ledno, int p_scyc)
{
	int		ret;

	illumination_info_printk("[%s] p_ledno = %d p_scyc = %d \n",__func__, p_ledno, p_scyc);
	ret = bu26507gul_hw_matrix_sloop_cycle(p_ledno, p_scyc);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_matrix_sloop_cycle ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_sloop_delay(int p_ledno, int p_sdly)
{
	int		ret;

	illumination_info_printk("[%s] p_ledno = %d p_sdly = %d\n",__func__, p_ledno, p_sdly);
	ret = bu26507gul_hw_matrix_sloop_delay(p_ledno, p_sdly);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_matrix_sloop_delay ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_brightness(int p_ledno, int p_ledelec)
{
	int		ret;

	illumination_info_printk("[%s] p_ledno = %d p_ledelec = %d\n",__func__, p_ledno, p_ledelec);
	ret = bu26507gul_hw_matrix_brightness(p_ledno, p_ledelec);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_matrix_brightness ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_all(int p_ledno, int p_ledelec, int p_sdly, int p_scyc)
{
	int		ret;

	illumination_info_printk("[%s] p_ledno = %d p_ledelec = %d p_sdly = %d p_scyc = %d\n",__func__, p_ledno, p_ledelec, p_sdly, p_scyc);
	ret = bu26507gul_hw_matrix_set(p_ledno, p_ledelec, p_sdly, p_scyc);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_matrix_set ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_pwmset(int p_pwmduty)
{
	int		ret;

	illumination_info_printk("[%s] p_pwmduty = %d\n",__func__, p_pwmduty);
	ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_CONT,
									BU26507GUL_DISPMATRIX_DONT_CARE,
									BU26507GUL_WRITEMATRIX_DONT_CARE);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_20h_pwm_duty(p_pwmduty);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_20h_pwm_duty ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_slp(int p_slp)
{
	int		ret;

	illumination_info_printk("[%s] p_slp = %d\n",__func__, p_slp);
	ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_CONT,
									BU26507GUL_DISPMATRIX_DONT_CARE,
									BU26507GUL_WRITEMATRIX_DONT_CARE);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_30h_start(BU26507GUL_HW_30H_STOP);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_30h_start ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_2dh_pwm_slp_scl(BU26507GUL_HW_2DH_SCROLL_OFF,
										BU26507GUL_HW_2DH_PWM_ON,
										BU26507GUL_HW_2DH_SLOOP_ON,
										p_slp);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_2dh_pwm_slp_scl ret=%d \n",__func__,ret);
		return (ret);
	}

	return (0);
}

int bu26507gul_sw_start(int p_start, int p_disp_ab)
{
	int		ret;

	illumination_info_printk("[%s] p_start = %d p_disp_ab = %d\n",__func__, p_start, p_disp_ab);
	ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_CONT,
									p_disp_ab,
									BU26507GUL_WRITEMATRIX_DONT_CARE);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
		return (ret);
	}
	ret = bu26507gul_hw_30h_start(p_start);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_30h_start ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}

int bu26507gul_sw_iab(int p_write_ab)
{
	int		ret;

	illumination_info_printk("[%s] p_write_ab = %d\n",__func__, p_write_ab);
	ret = bu26507gul_hw_7fh_rmcg_ab(BU26507GUL_CONTROL_CONT,
									BU26507GUL_DISPMATRIX_DONT_CARE,
									p_write_ab);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_hw_7fh_rmcg_ab ret=%d \n",__func__,ret);
		return (ret);
	}
	return (0);
}
