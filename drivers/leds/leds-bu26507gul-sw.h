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
#ifndef __LEDS_BU26507GUL_SW__
#define __LEDS_BU26507GUL_SW__

/* prot type */
extern int bu26507gul_sw_init(void);
extern int bu26507gul_sw_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int bu26507gul_sw_suspend(struct device *dev);
extern int bu26507gul_sw_resume(struct device *dev);
extern int bu26507gul_sw_remove(struct i2c_client *client);
extern int bu26507gul_sw_shutdown(struct i2c_client *client);
extern int bu26507gul_sw_exit(void);
extern int bu26507gul_sw_sloop_cycle(int p_ledno, int p_scyc);
extern int bu26507gul_sw_sloop_delay(int p_ledno, int p_sdly);
extern int bu26507gul_sw_brightness(int p_ledno, int p_ledelec);
extern int bu26507gul_sw_all(int p_ledno, int p_ledelec, int p_sdly, int p_scyc);
extern int bu26507gul_sw_pwmset(int p_pwmduty);
extern int bu26507gul_sw_slp(int p_slp);
extern int bu26507gul_sw_start(int p_start, int p_disp_ab);
extern int bu26507gul_sw_iab(int p_write_ab);

#endif /* __LEDS_BU26507GUL_SW__ */
