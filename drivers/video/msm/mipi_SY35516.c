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
#include "mipi_SY35516.h"

#define BKL_I2C_SLAVE_ADDR 0x38

static unsigned char conv_tbl[] = {
  0, 37, 64, 80, 92,100,108,114,119,124,128,131,135,138,141,144,
146,149,151,153,155,157,159,161,162,164,165,167,168,170,171,172,
174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,
190,190,191,192,193,193,194,195,196,196,197,198,198,199,200,200,
201,201,202,203,203,204,204,205,205,206,207,207,208,208,209,209,
210,210,211,211,212,212,212,213,213,214,214,215,215,216,216,216,
217,217,218,218,218,219,219,220,220,220,221,221,221,222,222,223,
223,223,224,224,224,225,225,225,226,226,226,227,227,227,228,228,
228,228,229,229,229,230,230,230,231,231,231,231,232,232,232,233,
233,233,233,234,234,234,234,235,235,235,235,236,236,236,236,237,
237,237,237,238,238,238,238,239,239,239,239,240,240,240,240,240,
241,241,241,241,242,242,242,242,242,243,243,243,243,243,244,244,
244,244,245,245,245,245,245,246,246,246,246,246,246,247,247,247,
247,247,248,248,248,248,248,249,249,249,249,249,249,250,250,250,
250,250,251,251,251,251,251,251,252,252,252,252,252,252,253,253,
253,253,253,253,254,254,254,254,254,254,255,255,255,255,255,255,
};

static int pmic8921_gpio9 = 0;
static int prev_lv = 0;
static struct i2c_adapter *i2c_bkl = NULL;;

static struct msm_panel_common_pdata *mipi_sy35516_pdata;

static int bkl_i2c_write(unsigned char addr,unsigned char data)
{
    struct i2c_msg msg;
    u_int8_t buf[8];
    int ret = 0;
    int try = 0;
    
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

static void mipi_sy35516_set_backlight(struct msm_fb_data_type *mfd)
{
    int req_lv;
    int ret = 0;

    if (!mfd) {
        pr_err("[BKL]%s mfd=NULL.\n", __func__);
        return;
    }

    req_lv = mfd->bl_level;
    if ( (req_lv < 0) || (255 < req_lv) ) {
        pr_err("[BKL]%s bl_level Invalid. (%d)\n", __func__,req_lv);
        return;
    }
    
    if (req_lv > 0) {
        if (prev_lv == 0){
            pr_info("[BKL]%s Backlight turn on( [%d]:%d).\n",__func__,req_lv,conv_tbl[req_lv]);
            gpio_set_value(pmic8921_gpio9, 1);
            
            ret |= bkl_i2c_write(0x10,0xC0);
            ret |= bkl_i2c_write(0x12,0xED); /* Run Time Ramp Rate */
            ret |= bkl_i2c_write(0x16,0xF1); /* Control A Brightness Configuration */
            ret |= bkl_i2c_write(0x74,conv_tbl[req_lv]); /* Control A Zone Target 4 */
            ret |= bkl_i2c_write(0x1D,0x01);
            if (ret) {
                pr_err("[BKL]%s I2C set failed.\n", __func__);
                return;
            }
        }
        else {
            ret = bkl_i2c_write(0x74,conv_tbl[req_lv]); /* Control A Zone Target 4 */
        }
    }
    else {
        gpio_set_value(pmic8921_gpio9, 0);
        
        pr_info("[BKL]%s Backlight turn off.\n",__func__);
    }
    
    prev_lv = req_lv;
    
    return;
}


static int __devinit mipi_sy35516_lcd_probe(struct platform_device *pdev)
{
    int rc;

    pr_info("[LCD]%s.\n", __func__);

    if (!pdev) {
        pr_err("[LCD]%s pdev=NULL.\n", __func__);
        return -ENODEV;
    }

    if (pdev->id == 0) {
        mipi_sy35516_pdata = pdev->dev.platform_data;
        return 0;
    }
    
    pmic8921_gpio9 = mipi_sy35516_pdata->gpio_num[0];
    rc = gpio_request(pmic8921_gpio9, "bkl_hwen");
    if (rc) {
        pr_err("[BKL]%s gpio_request(%d) failed, rc=%d\n",__func__,pmic8921_gpio9, rc);
    }
    else {
        pr_info("[BKL]%s gpio_request(%d) ok.\n", __func__,pmic8921_gpio9);
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

static int __devexit mipi_sy35516_lcd_remove(struct platform_device *pdev)
{
    /* Note: There are no APIs to remove fb device and free DSI buf. */
    pr_info("%s.\n", __func__);

    return 0;
}


static struct platform_driver this_driver = {
    .probe  = mipi_sy35516_lcd_probe,
    .remove  = __devexit_p(mipi_sy35516_lcd_remove),
    .driver = {
        .name   = "mipi_SY35516",
    },
};

static struct msm_fb_panel_data sy35516_panel_data = {
    .on            = NULL,
    .off           = NULL,
    .set_backlight = mipi_sy35516_set_backlight,
};

static int mipi_sy35516_lcd_init(void)
{
    pr_info("[LCD]%s\n",__func__);

    return platform_driver_register(&this_driver);
}

extern void (*delayed_display_on)(void);
/* FUJITSU:2013-06-21 LCD shutdown start*/
extern void (*lcd_power_off)(void);
/* FUJITSU:2013-06-21 LCD shutdown end*/

int mipi_sy35516_set_powerfuncs( struct sy35516_ctrl_funcs *ctrl_funcs )
{
    pr_info("[LCD]%s\n",__func__);
    
    if (ctrl_funcs == NULL) {
        return -ENODEV;
    }
    
    if (ctrl_funcs->on_fnc != NULL) {
        sy35516_panel_data.on = ctrl_funcs->on_fnc;
    } else {
        pr_err("[LCD] %s: on_fnc is null.\n",__func__);
    }
    
    if (ctrl_funcs->off_fnc != NULL) {
        sy35516_panel_data.off = ctrl_funcs->off_fnc;
    } else {
        pr_err("[LCD] %s: off_fnc is null.\n",__func__);
    }
    
    if (ctrl_funcs->after_video_on_fnc != NULL) {
        delayed_display_on = ctrl_funcs->after_video_on_fnc;
    }
/* FUJITSU:2013-06-21 LCD shutdown start*/
    if (ctrl_funcs->shutdown != NULL) {
        lcd_power_off = ctrl_funcs->shutdown;
    } else {
        pr_err("[LCD] %s: shutdown is null.\n",__func__);
    }
/* FUJITSU:2013-06-21 LCD shutdown end*/
    return 0;
}

int mipi_sy35516_device_register(struct msm_panel_info *pinfo,
                    u32 channel, u32 panel)
{
    struct platform_device *pdev = NULL;
    int ret;

    pr_info("[LCD]%s enter\n",__func__);

    ret = mipi_sy35516_lcd_init();
    if (ret) {
        pr_err("mipi_sy35516_lcd_init() failed with ret %u\n", ret);
        return ret;
    }

    pdev = platform_device_alloc("mipi_SY35516", (panel << 8)|channel);
    if (!pdev)
        return -ENOMEM;

    sy35516_panel_data.panel_info = *pinfo;

    ret = platform_device_add_data(pdev, &sy35516_panel_data,
                sizeof(sy35516_panel_data));
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
