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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_SY35516.h"

#undef BOOTSPLASH_DISABLE
static struct msm_panel_info pinfo;
static struct dsi_buf sy35516_tx_buf;
static struct dsi_buf sy35516_rx_buf;

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MUST TUNE
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
/* DSIPHY_REGULATOR_CTRL */
.regulator = {0x03, 0x0a, 0x04, 0x00, 0x20},
/* DSIPHY_CTRL */
.ctrl = {0x5f, 0x00, 0x00, 0x10},
/* DSIPHY_STRENGTH_CTRL */
.strength = {0xff, 0x00, 0x06, 0x00},
/* DSIPHY_TIMING_CTRL */
.timing = { 0x7C, 0x1C, 0x12,
0, /* DSIPHY_TIMING_CTRL_3 = 0 */
0x40, 0x48, 0x17, 0x1F, 0x1F, 0x03, 0x04}, 

/* DSIPHY_PLL_CTRL */
.pll = { 0x00,
0xA2, 0x31, 0xD9,
0x00, 0x50, 0x48, 0x63,
0x30, 0x07, 0x03,
0x00, 0x14, 0x03, 0x00, 0x02,
0x00, 0x20, 0x00, 0x01 },
};
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< MUST TUNE

static int lcd_state = 0;

static char exit_sleep[]  = {0x11, 0x00};
static char display_on[]  = {0x29, 0x00};

static char display_off[] = {0x28, 0x00};
static char enter_sleep[] = {0x10, 0x00};

static char cmd_MAUCCTR1[] = {
    0xF0, 0x55,
          0xAA,
          0x52,
          0x08,
          0x00,
};

static char cmd_DOPCTR[] = {
    0xB1, 0x7C,
          0x02,
          0x00,
};

static char cmd_DPRFCTR1[] = {
    0xBD, 0x01,
          0x2C,
          0x0C,
          0x08,
          0x01,
};

static char cmd_DPRFCTR2[] = {
    0xBE, 0x01,
          0x2C,
          0x0C,
          0x08,
          0x01,
};

static char cmd_DPRFCTR3[] = {
    0xBF, 0x01,
          0x2C,
          0x0C,
          0x08,
          0x01,
};

static char cmd_MAUCCTR2[] = {
    0xF0, 0x55,
          0xAA,
          0x52,
          0x00,
          0x00,
};

static char cmd_CASET[] = {
    0x2A, 0x00,
          0x00,
          0x02,
          0x1B,
};

static char cmd_PASET[] = {
    0x2B, 0x00,
          0x00,
          0x03,
          0xBF,
};

static char cmd_COLMOD[] = {
    0x3A, 0x77,
//          0x11,
};

static struct dsi_cmd_desc init_cmds[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_MAUCCTR1), cmd_MAUCCTR1},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_DOPCTR), cmd_DOPCTR},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_DPRFCTR1), cmd_DPRFCTR1},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_DPRFCTR2), cmd_DPRFCTR2},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_DPRFCTR3), cmd_DPRFCTR3},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_MAUCCTR2), cmd_MAUCCTR2},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_CASET), cmd_CASET},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_PASET), cmd_PASET},
    {DTYPE_DCS_WRITE1, 1, 0, 0,   0, sizeof(cmd_COLMOD), cmd_COLMOD},
};

static struct dsi_cmd_desc sleep_out_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(exit_sleep), exit_sleep},
};

static struct dsi_cmd_desc display_on_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc display_off_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,  50, sizeof(display_off), display_off},
};

static struct dsi_cmd_desc sleep_in_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0, 120, sizeof(enter_sleep), enter_sleep},
};

static void mipi_sy35516_video_fhd50j_panel_display_on(void)
{
    int sent_cmds;

    pr_info("[LCD]%s enter\n",__func__);
    return;
    
#ifndef BOOTSPLASH_DISABLE
    if(lcd_state==0) {
        lcd_state=1;
        return;
    }
#endif
    
    msleep(120);
    
    sent_cmds = mipi_dsi_cmds_tx(&sy35516_tx_buf,
                                 display_on_cmds,
                      ARRAY_SIZE(display_on_cmds));
    if(sent_cmds !=   ARRAY_SIZE(display_on_cmds) ) {
        pr_err("[LCD]%s send display_on_cmds stopped(%d)",__func__,sent_cmds);
    }
    pr_info("[LCD]%s leave\n",__func__);
    return;
}

static int mipi_sy35516_video_fhd50j_lcd_on(struct platform_device *pdev)
{
    struct msm_fb_data_type *mfd;
    struct mipi_panel_info *mipi;
    int sent_cmds;

    pr_info("[LCD]%s enter\n",__func__);

    mfd = platform_get_drvdata(pdev);
    if (!mfd)
        return -ENODEV;

    if (mfd->key != MFD_KEY)
        return -EINVAL;

#ifndef BOOTSPLASH_DISABLE
    if(lcd_state==0) {
        pr_info("[LCD]%s leave on 1st_sec\n",__func__);
        return 0;
    }
#endif

    mipi  = &mfd->panel_info.mipi;
    
    sent_cmds = mipi_dsi_cmds_tx(&sy35516_tx_buf,
                                 init_cmds,
                      ARRAY_SIZE(init_cmds));
    if(sent_cmds !=   ARRAY_SIZE(init_cmds) ) {
        pr_err("[LCD]%s send init_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
    
    sent_cmds = mipi_dsi_cmds_tx(&sy35516_tx_buf,
                                 display_on_cmds,
                      ARRAY_SIZE(display_on_cmds));
    if(sent_cmds !=   ARRAY_SIZE(display_on_cmds) ) {
        pr_err("[LCD]%s send display_on_cmds stopped(%d)",__func__,sent_cmds);
    }
    
    msleep(20);
    
    sent_cmds = mipi_dsi_cmds_tx(&sy35516_tx_buf,
                                 sleep_out_cmds,
                      ARRAY_SIZE(sleep_out_cmds));
    if(sent_cmds !=   ARRAY_SIZE(sleep_out_cmds) ) {
        pr_err("[LCD]%s send sleep_out_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }

    msleep(120);
    

    pr_info("[LCD]%s leave\n",__func__);
    return 0;
}

static int mipi_sy35516_video_fhd50j_lcd_off(struct platform_device *pdev)
{
    int sent_cmds;
    
    pr_info("[LCD]%s enter\n",__func__);
    
    lcd_state=1;
    
    sent_cmds = mipi_dsi_cmds_tx(&sy35516_tx_buf,
                                 display_off_cmds,
                      ARRAY_SIZE(display_off_cmds));
    if(sent_cmds !=   ARRAY_SIZE(display_off_cmds) ) {
        pr_err("[LCD]%s send display_off_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
    
    sent_cmds = mipi_dsi_cmds_tx(&sy35516_tx_buf,
                                 sleep_in_cmds,
                      ARRAY_SIZE(sleep_in_cmds));
    if(sent_cmds !=   ARRAY_SIZE(sleep_in_cmds) ) {
        pr_err("[LCD]%s send sleep_in_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
    
    pr_info("[LCD]%s leave\n",__func__);
    return 0;
}

static struct sy35516_ctrl_funcs pfuncs = {
    .on_fnc               = mipi_sy35516_video_fhd50j_lcd_on,
    .off_fnc              = mipi_sy35516_video_fhd50j_lcd_off,
    .after_video_on_fnc   = mipi_sy35516_video_fhd50j_panel_display_on,
    .before_video_off_fnc = NULL,
};

static int mipi_sy35516_video_fhd50j_pt_init(void)
{
    int ret;

    pr_info("[LCD]%s enter\n",__func__);

	if (msm_fb_detect_client("mipi_sy35516_video_qhd"))
		return 0;

	pinfo.xres = 540;
	pinfo.yres = 960;
	pinfo.lcdc.xres_pad = 0;
	pinfo.lcdc.yres_pad = 0;

	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;

	pinfo.lcdc.h_pulse_width = 4; /* set this value to multiple of 4 and greater than 4 */
	pinfo.lcdc.h_back_porch  = 52; /* set this value to multiple of 4 and greater than 4 */
	pinfo.lcdc.h_front_porch = 20;
	
	pinfo.lcdc.v_back_porch  = 10;
	pinfo.lcdc.v_front_porch = 10;
	pinfo.lcdc.v_pulse_width = 2;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

//	pinfo.clk_rate = 500000000;

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = FALSE;
	pinfo.mipi.hfp_power_stop = FALSE;
	pinfo.mipi.hbp_power_stop = FALSE;
	pinfo.mipi.hsa_power_stop = FALSE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
//	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_BGR;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = FALSE;
	pinfo.mipi.data_lane3 = FALSE;
	
	pinfo.mipi.t_clk_post = 0x04; //CHECK
	pinfo.mipi.t_clk_pre  = 0x1B; //CHECK
	
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = 0;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;

	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo.mipi.tx_eot_append = TRUE;
	pinfo.mipi.esc_byte_ratio = 2; //CHECK

    pinfo.actual_height = 110; //CHECK
    pinfo.actual_width = 62;   //CHECK

    mipi_sy35516_set_powerfuncs(&pfuncs);

    mipi_dsi_buf_alloc(&sy35516_tx_buf, DSI_BUF_SIZE);
    mipi_dsi_buf_alloc(&sy35516_rx_buf, DSI_BUF_SIZE);

    ret = mipi_sy35516_device_register(&pinfo, MIPI_DSI_PRIM, MIPI_DSI_PANEL_QHD_PT );
    if (ret)
        pr_err("%s: failed to register device!\n", __func__);

    pr_info("[LCD]%s leave(%d)\n",__func__,ret);

    return ret;
}

module_init(mipi_sy35516_video_fhd50j_pt_init);
