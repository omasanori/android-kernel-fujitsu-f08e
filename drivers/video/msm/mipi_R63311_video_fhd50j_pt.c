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
#include "mipi_R63311.h"

static struct msm_panel_info pinfo;
static struct dsi_buf r63311_tx_buf;
static struct dsi_buf r63311_rx_buf;

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
/* DSIPHY_REGULATOR_CTRL */
.regulator = {0x03, 0x0a, 0x04, 0x00, 0x20},
/* DSIPHY_CTRL */
.ctrl = {0x5f, 0x00, 0x00, 0x10},
/* DSIPHY_STRENGTH_CTRL */
.strength = {0xff, 0x00, 0x06, 0x00},
/* DSIPHY_TIMING_CTRL */
.timing = { 0xE0, 0x37, 0x24,
0, /* DSIPHY_TIMING_CTRL_3 = 0 */
0x66, 0x6D, 0x2A, 0x3A, 0x3F, 0x03, 0x04}, 

/* DSIPHY_PLL_CTRL */
.pll = { 0x00,
0xA1, 0x31, 0xD9,
0x00, 0x50, 0x48, 0x63,
0x77, 0x88, 0x99,
0x00, 0x14, 0x03, 0x00, 0x02,
0x00, 0x20, 0x00, 0x01 },
};

static int lcd_state = 0;

static char exit_sleep[]  = {0x11, 0x00};
static char display_on[]  = {0x29, 0x00};
static char display_off[] = {0x28, 0x00};
static char enter_sleep[] = {0x10, 0x00};

static char cmd_MCA_open[] = {
    0xB0, 0x00,
};

static char cmd_NOP[] = {
    0x00, 0x00,
};

static char cmd_IF_setting[] = {
    0xB3, 0x14,
          0x00,
          0x00,
          0x20,
          0x00,
          0x00,
};

static char cmd_IF_ID_setting[] = {
    0xB4, 0x0C,
          0x00,
};

static char cmd_DSI_ctrl[] = {
    0xB6, 0x3A,
          0xD3,
};

static char cmd_Display_setting1[] = {
    0xC1, 0x0C, /* 1 */
          0x60,
          0x40,
          0x00,
          0x00, /* 5 */
          0x41,
          0x00,
          0x00,
          0x00,
          0x00, /* 10 */
          0x00,
          0xD4,
          0x62,
          0x6C,
          0xA9, /* 15 */
          0x03,
          0x00,
          0x00,
          0x00,
          0x94, /* 20 */
          0x11,
          0x00,
          0x00,
          0x15,
          0x15, /* 25 */
          0x15,
          0x15,
          0x00,
          0x00,
          0x00, /* 30 */
          0x00,
          0x02,
          0x02,
          0x00,
};

static char cmd_Display_setting2[] = {
    0xC2, 0x70, /* 1 */
          0x07,
          0x80,
          0x08,
          0x08, /* 5 */
          0x00,
          0x00,
};

static char cmd_source_timing_setting[] = {
    0xC4, 0x70, /* 1 */
          0x00,
          0x00,
          0x00,
          0x00, /* 5 */
          0x00,
          0x00,
          0x00,
          0x00,
          0x02, /* 10 */
          0x02,
          0x00,
          0x00,
          0x00,
          0x00, /* 15 */
          0x00,
          0x00,
          0x00,
          0x00,
          0x00, /* 20 */
          0x02,
          0x02,
};

static char cmd_LTPS_timing_setting[] = {
    0xC6, 0x00, /* 1 */
          0xFF,
          0x00,
          0x66,
          0x00, /* 5 */
          0x00,
          0x00,
          0x00,
          0x00,
          0x00, /* 10 */
          0x00,
          0x00,
          0x00,
          0x00,
          0x00, /* 15 */
          0x00,
          0x05,
          0x1A,
          0x05,
          0x00, /* 20 */
          0x01,
          0x00,
          0xFF,
          0x00,
          0x66, /* 25 */
          0x00,
          0x00,
          0x00,
          0x00,
          0x00, /* 30 */
          0x00,
          0x00,
          0x00,
          0x00,
          0x00, /* 35 */
          0x00,
          0x00,
          0x05,
          0x1A,
          0x05, /* 40 */
};

static char cmd_gamma_R[] = {
    0xC7, 0x07, /* 1 */
          0x09,
          0x0E,
          0x14,
          0x26, /* 5 */
          0x3F,
          0x30,
          0x3C,
          0x43,
          0x4C, /* 10 */
          0x50,
          0x78,
          0x07,
          0x09,
          0x0E, /* 15 */
          0x14,
          0x26,
          0x3F,
          0x30,
          0x3C, /* 20 */
          0x43,
          0x4C,
          0x50,
          0x78,
};

static char cmd_gamma_G[] = {
    0xC8, 0x07, /* 1 */
          0x11,
          0x19,
          0x23,
          0x2F, /* 5 */
          0x46,
          0x33,
          0x3E,
          0x46,
          0x4F, /* 10 */
          0x53,
          0x78,
          0x07,
          0x11,
          0x19, /* 15 */
          0x23,
          0x2F,
          0x46,
          0x33,
          0x3E, /* 20 */
          0x46,
          0x4F,
          0x53,
          0x78,
};

static char cmd_gamma_B[] = {
    0xC9, 0x07, /* 1 */
          0x28,
          0x2B,
          0x33,
          0x3E, /* 5 */
          0x4D,
          0x35,
          0x40,
          0x48,
          0x59, /* 10 */
          0x74,
          0x78,
          0x07,
          0x28,
          0x2B, /* 15 */
          0x33,
          0x3E,
          0x4D,
          0x35,
          0x40, /* 20 */
          0x48,
          0x59,
          0x74,
          0x78,
};

static char cmd_panel_pin_ctrl[] = {
    0xCB, 0x58, /* 1 */
          0xE0,
          0x0F,
          0x1E,
          0x00, /* 5 */
          0x00,
          0x00,
          0x00,
          0xC0,
};

static char cmd_panel_IF_ctrl[] = {
    0xCC, 0x07, /* 1 */
};

static char cmd_power_chip_setting[] = {
    0xD0, 0x00, /* 1 */
          0x00,
          0x19,
          0x18,
          0x99, /* 5 */
          0x9D,
          0x9D,
          0x39,
          0x81,
          0x00, /* 10 */
          0xBB,
          0x57,
          0xCC,
          0x01,
};

static char cmd_internal_power_setting[] = {
    0xD3, 0x1B, /* 1 */
          0x33,
          0xBB,
          0xCC,
          0xC4, /* 5 */
          0x33,
          0x33,
          0x33,
          0x00,
          0x01, /* 10 */
          0x00,
          0xA0,
          0xD8,
          0xA0,
          0x04, /* 15 */
          0x5B,
          0x33,
          0x33,
          0x22,
          0x70, /* 20 */
          0x02,
          0x5B,
          0x43,
          0x3D,
          0xBF, /* 25 */
          0x99,
};

static char cmd_internal_power_setting2[] = {
    0xD5, 0x06, /* 1 */
          0x00,
          0x00,
          0x01,
          0x2F, /* 5 */
          0x01,
          0x2F,
};

static char cmd_test[] = {
    0xD7, 0x84, /* 1 */
          0xFC,
          0x7F,
          0xA8,
          0xCE, /* 5 */
          0x38,
          0xFC,
          0xC1,
          0x83,
          0xE7, /* 10 */
          0x8F,
          0x1F,
          0x3C,
          0x10,
          0xFA, /* 15 */
          0xC3,
          0x0F,
          0x04,
          0x41,
          0x20, /* 20 */
};

static char cmd_seq_ctrl[] = {
    0xD9, 0x00, /* 1 */
          0x00,
};

/*----------------------------
    MIPI COMMAND SEQUENCES
-----------------------------*/
static struct dsi_cmd_desc init_cmds[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_MCA_open), cmd_MCA_open},
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(cmd_NOP), cmd_NOP},
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(cmd_NOP), cmd_NOP},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_IF_setting), cmd_IF_setting},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_IF_ID_setting), cmd_IF_ID_setting},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_DSI_ctrl), cmd_DSI_ctrl},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Display_setting1), cmd_Display_setting1},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Display_setting2), cmd_Display_setting2},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_source_timing_setting), cmd_source_timing_setting},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting), cmd_LTPS_timing_setting},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_gamma_R), cmd_gamma_R},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_gamma_G), cmd_gamma_G},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_gamma_B), cmd_gamma_B},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_panel_pin_ctrl), cmd_panel_pin_ctrl},
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_panel_IF_ctrl), cmd_panel_IF_ctrl},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_power_chip_setting), cmd_power_chip_setting},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_internal_power_setting), cmd_internal_power_setting},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_internal_power_setting2), cmd_internal_power_setting2},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_internal_power_setting2), cmd_internal_power_setting2},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_test), cmd_test},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_seq_ctrl), cmd_seq_ctrl},
};

static struct dsi_cmd_desc sleep_out_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
};

static struct dsi_cmd_desc display_on_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc display_off_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,  20, sizeof(display_off), display_off},
};

static struct dsi_cmd_desc sleep_in_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,  80, sizeof(enter_sleep), enter_sleep},
};

static void mipi_r63311_video_fhd50j_panel_display_on(void)
{
    int sent_cmds;

    printk("[LCD]%s enter\n",__func__);
    
    if(lcd_state==0) {
        lcd_state=1;
        return;
    }
    
    sent_cmds = mipi_dsi_cmds_tx(&r63311_tx_buf,
                                 sleep_out_cmds,
                      ARRAY_SIZE(sleep_out_cmds));
    if(sent_cmds !=   ARRAY_SIZE(sleep_out_cmds) ) {
        printk("[LCD]%s send sleep_out_cmds stopped(%d)\n",__func__,sent_cmds);
        return;
    }

    sent_cmds = mipi_dsi_cmds_tx(&r63311_tx_buf,
                                 display_on_cmds,
                      ARRAY_SIZE(display_on_cmds));
    if(sent_cmds !=   ARRAY_SIZE(display_on_cmds) ) {
        printk("[LCD]%s send display_on_cmds stopped(%d)",__func__,sent_cmds);
    }
    printk("[LCD]%s leave\n",__func__);
    return;
}

static void mipi_r63311_video_fhd50j_panel_display_off(void)
{
    int sent_cmds;

    printk("[LCD]%s enter\n",__func__);

    sent_cmds = mipi_dsi_cmds_tx(&r63311_tx_buf, 
                                 display_off_cmds,
                      ARRAY_SIZE(display_off_cmds));

    sent_cmds = mipi_dsi_cmds_tx(&r63311_tx_buf,
                                 sleep_in_cmds,
                      ARRAY_SIZE(sleep_in_cmds));

    if(sent_cmds !=   ARRAY_SIZE(sleep_in_cmds) ) {
        printk("[LCD]%s send sleep_in_cmds stopped(%d)",__func__,sent_cmds);
        return;
    }

    mipi_set_tx_power_mode(1);

    printk("[LCD]%s leave\n",__func__);
    return;
}

static int mipi_r63311_video_fhd50j_lcd_on(struct platform_device *pdev)
{
    struct msm_fb_data_type *mfd;
    struct mipi_panel_info *mipi;
    int sent_cmds;

    printk("[LCD]%s enter\n",__func__);

    mfd = platform_get_drvdata(pdev);
    if (!mfd)
        return -ENODEV;

    if (mfd->key != MFD_KEY)
        return -EINVAL;

    if(lcd_state==0) {
        mipi_set_tx_power_mode(0);
        printk("[LCD]%s leave on 1st_sec\n",__func__);
        return 0;
    }

    mipi  = &mfd->panel_info.mipi;
    
    sent_cmds = mipi_dsi_cmds_tx(&r63311_tx_buf,
                                 init_cmds,
                      ARRAY_SIZE(init_cmds));
    if(sent_cmds !=   ARRAY_SIZE(init_cmds) ) {
        printk("[LCD]%s send init_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }

    mipi_set_tx_power_mode(0);

    printk("[LCD]%s leave\n",__func__);
    return 0;
}

static int mipi_r63311_video_fhd50j_lcd_off(struct platform_device *pdev)
{
    printk("[LCD]%s enter\n",__func__);
    
    lcd_state=1;
    
    printk("[LCD]%s leave\n",__func__);
    return 0;
}

/* FUJITSU:2013-04-02 DISP panel name start */
static char *mipi_r63311_video_fhd50j_panel_name(void)
{
	return "";
}
/* FUJITSU:2013-04-02 DISP panel name end */

static struct r63311_ctrl_funcs pfuncs = {
    .on_fnc               = mipi_r63311_video_fhd50j_lcd_on,
    .off_fnc              = mipi_r63311_video_fhd50j_lcd_off,
    .after_video_on_fnc   = mipi_r63311_video_fhd50j_panel_display_on,
    .before_video_off_fnc = mipi_r63311_video_fhd50j_panel_display_off,
/* FUJITSU:2013-04-02 DISP panel name start */
    .name_fnc             = mipi_r63311_video_fhd50j_panel_name,
/* FUJITSU:2013-04-02 DISP panel name end */
};

static int mipi_r63311_video_fhd50j_pt_init(void)
{
    int ret;

    printk("[LCD]%s enter\n",__func__);

	if (msm_fb_detect_client("mipi_r63311_video_fhd50j"))
		return 0;

	pinfo.xres = 1080;
	pinfo.yres = 1920;
	pinfo.lcdc.xres_pad = 0;
	pinfo.lcdc.yres_pad = 0;

	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;

	pinfo.lcdc.h_pulse_width = 12; /* set this value to multiple of 4 and greater than 4 */
	pinfo.lcdc.h_back_porch  = 52; /* set this value to multiple of 4 and greater than 4 */
	pinfo.lcdc.h_front_porch = 104;
	
	pinfo.lcdc.v_back_porch  = 4;
	pinfo.lcdc.v_front_porch = 8;
	pinfo.lcdc.v_pulse_width = 4;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	pinfo.clk_rate = 878000000;

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
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = TRUE;
	pinfo.mipi.data_lane3 = TRUE;
	
	pinfo.mipi.t_clk_post = 0x03;
	pinfo.mipi.t_clk_pre  = 0x2D;
	
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = 0;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;

	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo.mipi.tx_eot_append = TRUE;
	pinfo.mipi.esc_byte_ratio = 8;

    pinfo.actual_height = 110;
    pinfo.actual_width = 62;

    mipi_r63311_set_powerfuncs(&pfuncs);

    mipi_dsi_buf_alloc(&r63311_tx_buf, DSI_BUF_SIZE);
    mipi_dsi_buf_alloc(&r63311_rx_buf, DSI_BUF_SIZE);

    ret = mipi_r63311_device_register(&pinfo, MIPI_DSI_PRIM, 8 );
    if (ret)
        pr_err("%s: failed to register device!\n", __func__);

    printk("[LCD]%s leave(%d)\n",__func__,ret);

    return ret;
}

module_init(mipi_r63311_video_fhd50j_pt_init);
