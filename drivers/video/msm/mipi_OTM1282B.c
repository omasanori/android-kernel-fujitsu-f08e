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
    INCLUDE FILES FOR MODULE
============================================================================*/
#include <linux/delay.h>
#include <linux/pwm.h>

#include <linux/gpio.h>
#include <mach/gpio.h>

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_OTM1282B.h"

#define BKL_I2C_SLAVE_ADDR 0x38

static int bkl_hwen = 0;
static int prev_lv = 0;
static struct i2c_adapter *i2c_bkl = NULL;;

static struct msm_panel_common_pdata *mipi_otm1282b_pdata;

static int bkl_i2c_write(unsigned char addr,unsigned char data)
{
    struct i2c_msg msg;
    u_int8_t buf[8];
    int ret = 0;
    int try = 0;
    
    pr_debug(" (0x%x,0x%x)\n",addr,data);
    
    msg.addr  = BKL_I2C_SLAVE_ADDR;
    msg.buf   = buf;
    msg.len   = 2;
    msg.flags = 0;
    
    buf[0] = addr;
    buf[1] = data;
    
    ret = i2c_transfer(i2c_bkl, &msg, 1);
    
    if (ret < 0) {
        pr_err("[BKL]%s I2C(addr:%x,data:%x) ERROR ret = %d\n",__func__,addr,data,ret);
        for (try = 0; try < 50; try++) {
            msleep(5);
            ret = i2c_transfer(i2c_bkl, &msg, 1);
            if (ret >= 0 ) {
                pr_err("[BKL]%s I2C(addr:%x,data:%x) ERROR ret = %d retry(%d):OK\n",__func__,addr,data,ret,try);
                break;
            }
        }
        if(ret < 0){
            pr_err("[BKL]%s I2C(addr:%x,data:%x) ERROR ret = %d retry(%d):NG\n",__func__,addr,data,ret,try);
        }
    }
    else {
        /* I2C transfer successful. return success(0) */
        ret = 0;
    }
    return ret;
}

static int rate_reset = 0; /* FUJITSU:2013-05-17 ramprate change */
static void mipi_otm1282b_backlight_on(int req_lv)
{
    int ret = 0;

    pr_info("[BKL]%s Backlight turn on(%d).\n",__func__,req_lv);
    gpio_set_value(bkl_hwen, 1);
    
    ret |= bkl_i2c_write(0x00,0x04); //MASTER ENABLE
    ret |= bkl_i2c_write(0x10,0xFF); //Diode Enable
    ret |= bkl_i2c_write(0x20,0x7D); //Configration
    /* FUJITSU:2013-05-17 ramprate change */
    ret |= bkl_i2c_write(0x30,0x00); //OPTION
    rate_reset = 1;
    /* FUJITSU:2013-05-17 ramprate change */
    ret |= bkl_i2c_write(0xA0,req_lv); //GROUP A Brightness

    if (ret) {
        pr_err("[BKL]%s I2C set failed.\n", __func__);
    }
    return;
}

static void mipi_otm1282b_backlight_change(int req_lv)
{
    int ret = 0;

    /* FUJITSU:2013-05-17 ramprate change */
    if (rate_reset == 1) {
        pr_debug("[BKL] %s rate change\n",__func__);
        ret |= bkl_i2c_write(0x30,0x36); //OPTION
        rate_reset = 0;
    }
    /* FUJITSU:2013-05-17 ramprate change */
    
    ret |= bkl_i2c_write(0xA0,req_lv); //GROUP A Brightness

    if (ret) {
        pr_err("[BKL]%s I2C set failed.\n", __func__);
    }
    return;
}

static void mipi_otm1282b_backlight_off(int req_lv)
{
    int ret = 0;

    ret |= bkl_i2c_write(0x00,0x00); //MASTER ENABLE
    gpio_set_value(bkl_hwen, 0);

    pr_info("[BKL]%s Backlight turn off.\n",__func__);

    if (ret) {
        pr_err("[BKL]%s I2C set failed.\n", __func__);
    }
    return;
}


static void mipi_otm1282b_set_backlight(struct msm_fb_data_type *mfd)
{
    int req_lv;

    if (!mfd) {
        pr_err("[BKL]%s mfd=NULL.\n", __func__);
        return;
    }

    req_lv = mfd->bl_level;
    if ( (req_lv < 0) || (mfd->panel_info.bl_max < req_lv) ) {
        pr_err("[BKL]%s bl_level Invalid. (%d)\n", __func__,req_lv);
        return;
    }
    
    pr_debug("[BKL] %s(%d)\n",__func__,req_lv);
    
    if (req_lv > 0) {
        if (prev_lv == 0){
            mipi_otm1282b_backlight_on(req_lv);
        }
        else {
            mipi_otm1282b_backlight_change(req_lv);
        }
    }
    else {
        mipi_otm1282b_backlight_off(req_lv);
    }
    
    prev_lv = req_lv;
    
    return;
}


static int __devinit mipi_otm1282b_lcd_probe(struct platform_device *pdev)
{
    int rc;

    pr_info("[LCD]%s.\n", __func__);

    if (!pdev) {
        pr_err("[LCD]%s pdev=NULL.\n", __func__);
        return -ENODEV;
    }

    if (pdev->id == 0) {
        mipi_otm1282b_pdata = pdev->dev.platform_data;
        return 0;
    }
    
    bkl_hwen = mipi_otm1282b_pdata->gpio_num[0];
    rc = gpio_request(bkl_hwen, "bkl_hwen");
    if (rc) {
        pr_err("[BKL]%s gpio_request(%d) failed, rc=%d\n",__func__,bkl_hwen, rc);
    }
    else {
        pr_info("[BKL]%s gpio_request(%d) ok.\n", __func__,bkl_hwen);
    }
    
    pr_debug("[BKL]%s: get i2c_adapter\n",__func__);
    i2c_bkl = i2c_get_adapter(7);//gsbi7
    if (!i2c_bkl) {
        pr_err("[BKL]%s i2c_get_adapter() failure.\n",__func__);
    }
    else {
        pr_info("[BKL]%s get success name:[%s]\n",__func__,i2c_bkl->name);
    }

    msm_fb_add_device(pdev);
    
    return 0;
}

static int __devexit mipi_otm1282b_lcd_remove(struct platform_device *pdev)
{
    /* Note: There are no APIs to remove fb device and free DSI buf. */
    pr_info("%s.\n", __func__);

    return 0;
}


static struct platform_driver this_driver = {
    .probe  = mipi_otm1282b_lcd_probe,
    .remove  = __devexit_p(mipi_otm1282b_lcd_remove),
    .driver = {
        .name   = "mipi_OTM1282B",
    },
};

static struct msm_fb_panel_data otm1282b_panel_data = {
    .on            = NULL,
    .off           = NULL,
    .set_backlight = mipi_otm1282b_set_backlight,
};

static int mipi_otm1282b_lcd_init(void)
{
    pr_info("[LCD]%s\n",__func__);

    return platform_driver_register(&this_driver);
}

extern void (*delayed_display_on)(void);
#if 1   /* FUJITSU:2013-05-17 mode_change add */
extern ssize_t (*panel_get_wcabc)(char *buf);
extern ssize_t (*panel_set_wcabc)(const char *buf,size_t count);
#endif  /* FUJITSU:2013-05-17 mode_change add */
/* FUJITSU:2013-06-21 LCD shutdown start*/
extern void (*lcd_power_off)(void);
/* FUJITSU:2013-06-21 LCD shutdown end*/

int mipi_otm1282b_set_powerfuncs( struct otm1282b_ctrl_funcs *ctrl_funcs )
{
    pr_info("[LCD]%s\n",__func__);
    
    if (ctrl_funcs == NULL) {
        return -ENODEV;
    }
    
    if (ctrl_funcs->on_fnc != NULL) {
        otm1282b_panel_data.on = ctrl_funcs->on_fnc;
    } else {
        pr_info("[LCD] %s: on_fnc is null.\n",__func__);
    }
    
    if (ctrl_funcs->off_fnc != NULL) {
        otm1282b_panel_data.off = ctrl_funcs->off_fnc;
    } else {
        pr_info("[LCD] %s: off_fnc is null.\n",__func__);
    }
    
    if (ctrl_funcs->display_on_fnc != NULL) {
        delayed_display_on = ctrl_funcs->display_on_fnc;
    }
    
#if 1  /* FUJITSU:2013-05-17 mode_change add */
    if (ctrl_funcs->get_wcabc != NULL) {
        panel_get_wcabc = ctrl_funcs->get_wcabc;
    }
    if (ctrl_funcs->set_wcabc != NULL) {
        panel_set_wcabc = ctrl_funcs->set_wcabc;
    }
#endif /* FUJITSU:2013-05-17 mode_change add */
    
/* FUJITSU:2013-06-21 LCD shutdown start*/
    if (ctrl_funcs->shutdown != NULL) {
        lcd_power_off = ctrl_funcs->shutdown;
    } else {
        pr_info("[LCD] %s: shutdown is null.\n",__func__);
    }
/* FUJITSU:2013-06-21 LCD shutdown end*/
    return 0;
}

int mipi_otm1282b_device_register(struct msm_panel_info *pinfo,
                    u32 channel, u32 panel)
{
    struct platform_device *pdev = NULL;
    int ret;

    pr_info("[LCD]%s enter\n",__func__);

    ret = mipi_otm1282b_lcd_init();
    if (ret) {
        pr_err("mipi_otm1282b_lcd_init() failed with ret %u\n", ret);
        return ret;
    }

    pdev = platform_device_alloc("mipi_OTM1282B", (panel << 8)|channel);
    if (!pdev)
        return -ENOMEM;

    otm1282b_panel_data.panel_info = *pinfo;

    ret = platform_device_add_data(pdev, &otm1282b_panel_data,
                sizeof(otm1282b_panel_data));
    if (ret) {
        pr_debug("%s: platform_device_add_data failed!\n", __func__);
        goto err_device_put;
    }
    
    ret = platform_device_add(pdev);
    if (ret) {
        pr_debug("%s: platform_device_register failed!\n", __func__);
        goto err_device_put;
    }

    pr_info("[LCD]%s leave\n",__func__);
    return 0;

err_device_put:
    platform_device_put(pdev);
    return ret;
}
