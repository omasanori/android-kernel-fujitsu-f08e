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
#include <asm/uaccess.h>    /* copy_from_user */
#include <linux/proc_fs.h>
#include <linux/stat.h>     /* S_IFREG, ... */
#include <linux/gpio.h>
#include <../arch/arm/mach-msm/board-8064.h>

#define LCD_ADB_DEBUG
#define LCD_DEBUG_LOG
#ifdef LCD_DEBUG_LOG
#define PRINT_LOG(a) printk a
#else   // LCD_DEBUG_LOG
#define PRINT_LOG(a) do {} while(0)
#endif  // LCD_DEBUG_LOG

static struct msm_panel_info pinfo;
static struct dsi_buf r63306_tx_buf;
static struct dsi_buf r63306_rx_buf;

static int lcd_state = 0;

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
    /* regulator */
	{0x03, 0x0a, 0x04, 0x00, 0x20},
	/* timing */
	{0x89, 0x1F, 0x14, 
	 0x00, 
	 0x44, 0x4A, 0x19, 0x22, 0x23, 0x03, 0x04, 0xa0},
    /* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
    /* strength */
	{0xff, 0x00, 0x06, 0x00},
	/* pll control */
	{0x0, 
	0xD4, 0x01, 0x19, 
	0x00, 0x50, 0x48, 0x63,
	0x77, 0x88, 0x99,
	0x00, 0x14, 0x03, 0x00, 0x02,
	0x00, 0x20, 0x00, 0x01 },
};

static char sh_exit_sleep[]  = {0x11, 0x00};
static char sh_cmd_B0_open[] = {0xB0, 0x00};

static char sh_cmd_E4[] = {
	0xE4, 0x00,
		  0x00,
		  0x00,
		  0x00,
		  0x7F,
};

static char sh_cmd_C1[] = {
    0xC1, 0x00,
          0xAF,
          0x00,
          0x00,
          0xA5,
          0x00,
          0x00,
          0xA0,
          0x09,
          0x21,
          0x09,
          0x00,
          0x00,
          0x00,
          0x01,
};

static char sh_cmd_C2[] = {
    0xC2, 0x00,
          0x09,
          0x09,
          0x00,
          0x00,
};

static char sh_cmd_C8[] = {
    0xC8, 0x00,
          0x00,
          0x00,
          0x00,
};

static char sh_cmd_C9[] = {
    0xC9, 0x1E,
          0x16,
          0x18,
          0x28,
          0x27,
          0x1F,
          0x23,
          0x28,
          0x1C,
          0x14,
          0x19,
          0x07,
          0x1E,
};

static char sh_cmd_CA[] = {
    0xCA, 0x21,
          0x29,
          0x47,
          0x37,
          0x38,
          0x40,
          0x3C,
          0x37,
          0x43,
          0x4B,
          0x46,
          0x38,
          0x21,
};

static char sh_cmd_CB[] = {
    0xCB, 0x3A,
          0x34,
          0x34,
          0x36,
          0x2F,
          0x23,
          0x27,
          0x2A,
          0x1C,
          0x14,
          0x19,
          0x08,
          0x1E,
};

static char sh_cmd_CC[] = {
    0xCC, 0x05,
          0x0B,
          0x2B,
          0x29,
          0x30,
          0x3C,
          0x38,
          0x35,
          0x43,
          0x4B,
          0x46,
          0x37,
          0x21,
};

static char sh_cmd_CD[] = {
    0xCD, 0x32,
          0x32,
          0x36,
          0x3F,
          0x38,
          0x2B,
          0x2D,
          0x2F,
          0x20,
          0x18,
          0x1E,
          0x0D,
          0x28,
};

static char sh_cmd_CE[] = {
    0xCE, 0x0D,
          0x0D,
          0x29,
          0x20,
          0x27,
          0x34,
          0x32,
          0x30,
          0x3F,
          0x47,
          0x41,
          0x32,
          0x17,
};

static char sh_cmd_C6[] = {
    0xC6, 0x04,
          0x22,
          0x22,
          0x7C,
          0x00,
          0x00,
          0x00,
          0x00,
          0x00,
          0x00,
};

static char sh_cmd_D0[] = {
    0xD0, 0x6D,
          0x65,
          0x09,
          0x18,
          0xA3,
          0x00,
          0x13,
};

static char sh_cmd_D1[] = {
    0xD1, 0x77,
          0xD4,
};

static char sh_cmd_B4[] = {
    0xB4, 0x02,
};

static char sh_cmd_36[] = {
    0x36, 0x10,
};

static char sh_cmd_B0_protect[] = {
    0xB0, 0x03,
};

static char sh_display_on[]  = {0x29, 0x00};
static char sh_display_off[] = {0x28, 0x00};
static char sh_enter_sleep[] = {0x10, 0x00};

static struct dsi_cmd_desc r63306_sh_sleep_out_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(sh_exit_sleep), sh_exit_sleep},
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(sh_cmd_B0_open), sh_cmd_B0_open},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_C1), sh_cmd_C1},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_C2), sh_cmd_C2},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_C8), sh_cmd_C8},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_C9), sh_cmd_C9},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_CA), sh_cmd_CA},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_CB), sh_cmd_CB},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_CC), sh_cmd_CC},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_CD), sh_cmd_CD},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_CE), sh_cmd_CE},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_C6), sh_cmd_C6},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_D0), sh_cmd_D0},
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(sh_cmd_B4), sh_cmd_B4},
    {DTYPE_DCS_WRITE1, 1, 0, 0,   0, sizeof(sh_cmd_36), sh_cmd_36},
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(sh_cmd_B0_protect), sh_cmd_B0_protect},
};

static struct dsi_cmd_desc r63306_sh_regulator_cmds[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(sh_cmd_B0_open), sh_cmd_B0_open},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_D1), sh_cmd_D1},
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(sh_cmd_E4), sh_cmd_E4},
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(sh_cmd_B0_protect), sh_cmd_B0_protect},
};

static struct dsi_cmd_desc r63306_sh_display_off_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,  20, sizeof(sh_display_off), sh_display_off},
    {DTYPE_DCS_WRITE,  1, 0, 0,  60, sizeof(sh_enter_sleep), sh_enter_sleep}
};

static struct dsi_cmd_desc r63306_sh_display_on_cmds[] = {
	{DTYPE_DCS_WRITE,  1, 0, 0,  20, sizeof(sh_display_on), sh_display_on},
};

#ifdef LCD_ADB_DEBUG
static struct semaphore proc_sem;
static struct msm_panel_info* s_pAdbDebug_pinfo = NULL;
static struct msm_fb_data_type* s_mfd = NULL;
#endif  // LCD_ADB_DEBUG

extern int SaveMsmFbData(struct msm_fb_data_type* mfd);
extern int InitLcdAdbDebug(struct msm_panel_info* pInfo);

static void mipi_r63306_video_hd47s_panel_display_on(void)
{
    int sent_cmds;

    printk(KERN_INFO "[LCD]%s enter\n",__func__);

	 if(lcd_state==0) {
        lcd_state=1;
        return;
       }

    msleep(80); //TODO: check wait time
    
    sent_cmds = mipi_dsi_cmds_tx(&r63306_tx_buf,
                                 r63306_sh_display_on_cmds,
                      ARRAY_SIZE(r63306_sh_display_on_cmds));
    if(sent_cmds !=   ARRAY_SIZE(r63306_sh_display_on_cmds) ) {
        printk("[LCD]%s send r63306_sh_display_on_cmds stopped(%d)\n",__func__,sent_cmds);
    }
    printk(KERN_INFO "[LCD]%s leave\n",__func__);
    return;
}

static void mipi_r63306_video_hd47s_panel_display_off(void)
{
    int sent_cmds;

    printk(KERN_INFO "[LCD]%s enter\n",__func__);

    sent_cmds = mipi_dsi_cmds_tx(&r63306_tx_buf, r63306_sh_display_off_cmds,
                                               ARRAY_SIZE(r63306_sh_display_off_cmds));

    if(sent_cmds !=   ARRAY_SIZE(r63306_sh_display_off_cmds) ) {
        printk("[LCD]%s send r63306_sh_display_off_cmds stopped(%d)\n",__func__,sent_cmds);
        return;
    }

    printk(KERN_INFO "[LCD]%s leave\n",__func__);
    return;
}

static int mipi_r63306_video_hd47s_lcd_on(struct platform_device *pdev)
{
    struct msm_fb_data_type *mfd;
    static int vspn = PM8921_GPIO_PM_TO_SYS(44);
	int rc;
    int sent_cmds;

    printk(KERN_INFO "[LCD]%s enter\n",__func__);
    mfd = platform_get_drvdata(pdev);
    if (!mfd)
        return -ENODEV;

    SaveMsmFbData(mfd);

    if (mfd->key != MFD_KEY)
        return -EINVAL;

	if(lcd_state==0){
        rc = gpio_request(vspn, "mlcd_vspn");
        printk(KERN_INFO "[LCD_board]%s : vspn gpio_request rc=%d\n",__func__, rc);
	}

	if(lcd_state==0) {
        printk(KERN_INFO "[LCD]%s leave on 1st_sec\n",__func__);
        return 0;
    }

    sent_cmds = mipi_dsi_cmds_tx(&r63306_tx_buf,
                                 r63306_sh_regulator_cmds,
                      ARRAY_SIZE(r63306_sh_regulator_cmds));

    if(sent_cmds != ARRAY_SIZE(r63306_sh_regulator_cmds) ) {
        printk("[LCD]%s send r63306_sh_regulator_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
        /* for 5.4V */
    gpio_set_value(vspn,1);
    printk(KERN_INFO "### [LCD_board]%s: VSPN on\n",__func__);
	msleep(10);
	
    sent_cmds = mipi_dsi_cmds_tx(&r63306_tx_buf,
                                 r63306_sh_sleep_out_cmds,
                      ARRAY_SIZE(r63306_sh_sleep_out_cmds));
    
    if(sent_cmds != ARRAY_SIZE(r63306_sh_sleep_out_cmds) ) {
        printk("[LCD]%s send r63306_sh_sleep_out_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
    
    printk(KERN_INFO "[LCD]%s leave\n",__func__);
    return 0;
}

static int mipi_r63306_video_hd47s_lcd_off(struct platform_device *pdev)
{

    printk(KERN_INFO "[LCD]%s enter\n",__func__);

	lcd_state=1;

	printk(KERN_INFO "[LCD]%s leave\n",__func__);
    return 0;
}

/* FUJITSU:2013-04-08 DISP panel name start */
static char *mipi_r63306_video_hd47s_panel_name(void)
{
	return "_SHARP";
}
/* FUJITSU:2013-04-08 DISP panel name end */

static struct r63311_ctrl_funcs pfuncs = {
    .on_fnc               = mipi_r63306_video_hd47s_lcd_on,
    .off_fnc              = mipi_r63306_video_hd47s_lcd_off,
    .after_video_on_fnc   = mipi_r63306_video_hd47s_panel_display_on,
    .before_video_off_fnc = mipi_r63306_video_hd47s_panel_display_off,
/* FUJITSU:2013-04-08 DISP panel name start */
    .name_fnc             = mipi_r63306_video_hd47s_panel_name,
/* FUJITSU:2013-04-08 DISP panel name end */
};

static int mipi_video_r63306_hd47s_pt_init(void)
{
    int ret;

    printk(KERN_INFO "[LCD]%s enter\n",__func__);

	if (msm_fb_detect_client("mipi_r63306_video_hd47s"))
		return 0;

	pinfo.xres = 720;
	pinfo.yres = 1280;
	pinfo.lcdc.xres_pad = 0;
	pinfo.lcdc.yres_pad = 0;

	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.lcdc.h_back_porch = 156;
	pinfo.lcdc.h_front_porch = 160;
	pinfo.lcdc.h_pulse_width = 8;
	pinfo.lcdc.v_back_porch = 3;
	pinfo.lcdc.v_front_porch = 9;
	pinfo.lcdc.v_pulse_width = 4;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
    pinfo.actual_height = 103;
    pinfo.actual_width = 57;

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = TRUE;
	pinfo.mipi.hfp_power_stop = TRUE;
	pinfo.mipi.hbp_power_stop = TRUE;
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
	pinfo.mipi.t_clk_post = 0x04;
	pinfo.mipi.t_clk_pre = 0x1c;
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = 0;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo.mipi.tx_eot_append = TRUE;
	pinfo.mipi.esc_byte_ratio = 4;

    mipi_r63311_set_powerfuncs(&pfuncs);

    mipi_dsi_buf_alloc(&r63306_tx_buf, DSI_BUF_SIZE);
    mipi_dsi_buf_alloc(&r63306_rx_buf, DSI_BUF_SIZE);

    ret = mipi_r63311_device_register(&pinfo, MIPI_DSI_PRIM, MIPI_DSI_PANEL_720P_PT);
    if (ret)
        pr_err("%s: failed to register device!\n", __func__);

    printk(KERN_INFO "[LCD]%s leave(%d)\n",__func__,ret);

    InitLcdAdbDebug(&pinfo);

    return ret;
}

module_init(mipi_video_r63306_hd47s_pt_init);

#ifdef LCD_ADB_DEBUG
static int proc_hex2int (unsigned char *hex)
{
    int num = 0;
    
//    PRINT_LOG(("[LCD]proc_hex2int: IN hex_addr = 0x%x\n", hex));
//    PRINT_LOG(("[LCD]proc_hex2int: IN data = [%01X][%01X]\n", hex[0], hex[1]));
    if (('0' <= hex[0]) && ('9' >= hex[0])) {
        num = hex[0] - '0';
    } else if (('A' <= hex[0]) && ('F' >= hex[0])) {
        num = hex[0] - 'A' + 10;
    } else if (('a' <= hex[0]) && ('f' >= hex[0])) {
        num = hex[0] - 'a' + 10;
    } else {
        num = 0;
    }
    num <<= 4;
    if (('0' <= hex[1]) && ('9' >= hex[1])) {
        num += hex[1] - '0';
    } else if (('A' <= hex[1]) && ('F' >= hex[1])) {
        num += hex[1] - 'A' + 10;
    } else if (('a' <= hex[1]) && ('f' >= hex[1])) {
        num += hex[1] - 'a' + 10;
    } else {
        num += 0;
    }
    
    return num;
}

static int ProcChars2Ints(unsigned int *pBuf, unsigned char *pData, int nLen)
{
    int i = 0;
    unsigned char *bp = pData;
    int seq_len = 0;

    PRINT_LOG(("[LCD]ProcChars2Ints: IN seq_addr = 0x%X, buf_addr = 0x%X\n", (unsigned int)pBuf, (unsigned int)pData));

    for (seq_len = 0; ((bp < (pData + nLen)) && (seq_len < 50)); seq_len++) {
        PRINT_LOG(("[LCD]ProcChars2Ints:Loop Start Count=%d\n",seq_len));
        while (0x20 == *bp) {
            bp++;
            if (bp >= (pData + nLen)) {
                break;
            }
            if (0 == *bp) {
                break;
            }
        }
        pBuf[seq_len] = proc_hex2int(bp);
        bp += 2;
        if (0 == *bp) {
            break;
        }
    }

    for (i = 0; i < seq_len; i++)
    {
        PRINT_LOG(("[LCD]ProcChars2Ints:[0x%08X]\n", pBuf[i]));
    }

    return seq_len;
}

static int IntToHexStr(char* buf, unsigned int* punData, int nDatalen)
{
    int len = 0;
    int i;
    
    if((NULL == buf) || (NULL == punData))
    {
        return -1;
    }

    buf[0] = 0;
    for(i = 0; i < nDatalen; i++)
    {
        len += sprintf((buf+len), "0x%02X, ", punData[i]);
    }
    if(0 < nDatalen)
    {
        *(buf+len-2) = 0;
    }

    return 0;
}
#define LCD_TP_MAXBUF 512
static int
proc_read( char* page, char** start, off_t offset, int count, int* eof, void* data )
{
    int len = 0;
    char WorkBuff[LCD_TP_MAXBUF];

    PRINT_LOG(("[LCD]proc_read: called\n"));

    if ( down_interruptible( &proc_sem ) ) {
        PRINT_LOG(("[LCD]proc_read: read failed\n"));
        return -ERESTARTSYS;
    }

    len += sprintf((page+len), "[LCD]proc_read: h_back_porch  = [0x%02X(0x%02X)]\r\n", s_pAdbDebug_pinfo->lcdc.h_back_porch, s_mfd->fbi->var.left_margin);
    len += sprintf((page+len), "[LCD]proc_read: h_front_porch = [0x%02X(0x%02X)]\r\n", s_pAdbDebug_pinfo->lcdc.h_front_porch, s_mfd->fbi->var.right_margin);
    len += sprintf((page+len), "[LCD]proc_read: h_pulse_width = [0x%02X(0x%02X)]\r\n", s_pAdbDebug_pinfo->lcdc.h_pulse_width, s_mfd->fbi->var.hsync_len);
    len += sprintf((page+len), "[LCD]proc_read: v_back_porch  = [0x%02X(0x%02X)]\r\n", s_pAdbDebug_pinfo->lcdc.v_back_porch, s_mfd->fbi->var.upper_margin);
    len += sprintf((page+len), "[LCD]proc_read: v_front_porch = [0x%02X(0x%02X)]\r\n", s_pAdbDebug_pinfo->lcdc.v_front_porch, s_mfd->fbi->var.lower_margin);
    len += sprintf((page+len), "[LCD]proc_read: v_pulse_width = [0x%02X(0x%02X)]\r\n", s_pAdbDebug_pinfo->lcdc.v_pulse_width, s_mfd->fbi->var.vsync_len);
    len += sprintf((page+len), "[LCD]proc_read: --- dsi_video_mode_phy_db ---\r\n");
    IntToHexStr(WorkBuff, s_pAdbDebug_pinfo->mipi.dsi_phy_db->regulator, sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->regulator)/sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->regulator[0]));
    len += sprintf((page+len), "[LCD]proc_read: regulator = [%s]\r\n", WorkBuff);
    IntToHexStr(WorkBuff, s_pAdbDebug_pinfo->mipi.dsi_phy_db->timing, sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->timing)/sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->timing[0]));
    len += sprintf((page+len), "[LCD]proc_read: timing    = [%s]\r\n", WorkBuff);
    IntToHexStr(WorkBuff, s_pAdbDebug_pinfo->mipi.dsi_phy_db->ctrl, sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->ctrl)/sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->ctrl[0]));
    len += sprintf((page+len), "[LCD]proc_read: ctrl      = [%s]\r\n", WorkBuff);
    IntToHexStr(WorkBuff, s_pAdbDebug_pinfo->mipi.dsi_phy_db->strength, sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->strength)/sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->strength[0]));
    len += sprintf((page+len), "[LCD]proc_read: strength  = [%s]\r\n", WorkBuff);
    IntToHexStr(WorkBuff, s_pAdbDebug_pinfo->mipi.dsi_phy_db->pll, sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->pll)/sizeof(s_pAdbDebug_pinfo->mipi.dsi_phy_db->pll[0]));
    len += sprintf((page+len), "[LCD]proc_read: pll       = [%s]\r\n", WorkBuff);
    len += sprintf((page+len), "[LCD]proc_read: clk_rate = %d\r\n", s_mfd->panel_info.clk_rate);

    up( &proc_sem );

    *eof = 1;
    return len;
}

static int
proc_write( struct file* filp, const char* buffer, unsigned long count, void* data )
{
    unsigned char procbuf[LCD_TP_MAXBUF];
    int copy_len;
    int seq_len = 0;
    int nChgClkRate = 0;
    unsigned char *p = procbuf;
    uint32 h_period, v_period;

    PRINT_LOG(("[LCD]proc_write: called\n"));

    if ( down_interruptible( &proc_sem ) ) {
        PRINT_LOG(("[LCD]proc_write: write failed\n"));
        return -ERESTARTSYS;
    }

    if(NULL == s_mfd) {
        PRINT_LOG(("[LCD]proc_write: mfd init failed\n"));
        return -ERESTARTSYS;
    }

    copy_len = count;

    if ( copy_from_user( procbuf, buffer, copy_len ) ) {
        up( &proc_sem );
        PRINT_LOG(("[LCD]proc_write: copy_from_user failet\n"));
        return -EFAULT;
    }
    procbuf[copy_len-1] = 0;
    PRINT_LOG(("[LCD]proc_write: Input Data = [%s]\n", procbuf));
    while (*p) {
        PRINT_LOG(("[LCD]proc_write: Input Data Dump = [%02X]\n", *p++));
    }

    if(('H' == procbuf[0]) && ('B' == procbuf[1]) && ('P' == procbuf[2]))
    {
        seq_len = ProcChars2Ints((unsigned int*)&s_pAdbDebug_pinfo->lcdc.h_back_porch, &procbuf[3], copy_len);
        s_mfd->fbi->var.left_margin = s_mfd->panel_info.lcdc.h_back_porch = s_pAdbDebug_pinfo->lcdc.h_back_porch;
        nChgClkRate = 1;
    }
    else if(('H' == procbuf[0]) && ('F' == procbuf[1]) && ('P' == procbuf[2]))
    {
        seq_len = ProcChars2Ints((unsigned int*)&s_pAdbDebug_pinfo->lcdc.h_front_porch, &procbuf[3], copy_len);
        s_mfd->fbi->var.right_margin = s_mfd->panel_info.lcdc.h_front_porch = s_pAdbDebug_pinfo->lcdc.h_front_porch;
        nChgClkRate = 1;
    }
    else if(('H' == procbuf[0]) && ('P' == procbuf[1]) && ('W' == procbuf[2]))
    {
        seq_len = ProcChars2Ints((unsigned int*)&s_pAdbDebug_pinfo->lcdc.h_pulse_width, &procbuf[3], copy_len);
        s_mfd->fbi->var.hsync_len = s_mfd->panel_info.lcdc.h_pulse_width = s_pAdbDebug_pinfo->lcdc.h_pulse_width;
        nChgClkRate = 1;
    }
    else if(('V' == procbuf[0]) && ('B' == procbuf[1]) && ('P' == procbuf[2]))
    {
        seq_len = ProcChars2Ints((unsigned int*)&s_pAdbDebug_pinfo->lcdc.v_back_porch, &procbuf[3], copy_len);
        s_mfd->fbi->var.upper_margin = s_mfd->panel_info.lcdc.v_back_porch = s_pAdbDebug_pinfo->lcdc.v_back_porch;
        nChgClkRate = 1;
    }
    else if(('V' == procbuf[0]) && ('F' == procbuf[1]) && ('P' == procbuf[2]))
    {
        seq_len = ProcChars2Ints((unsigned int*)&s_pAdbDebug_pinfo->lcdc.v_front_porch, &procbuf[3], copy_len);
        s_mfd->fbi->var.lower_margin = s_mfd->panel_info.lcdc.v_front_porch = s_pAdbDebug_pinfo->lcdc.v_front_porch;
        nChgClkRate = 1;
    }
    else if(('V' == procbuf[0]) && ('P' == procbuf[1]) && ('W' == procbuf[2]))
    {
        seq_len = ProcChars2Ints((unsigned int*)&s_pAdbDebug_pinfo->lcdc.v_pulse_width, &procbuf[3], copy_len);
        s_mfd->fbi->var.vsync_len = s_mfd->panel_info.lcdc.v_pulse_width = s_pAdbDebug_pinfo->lcdc.v_pulse_width;
        nChgClkRate = 1;
    }
    else if(('P' == procbuf[0]) && ('H' == procbuf[1]) && ('Y' == procbuf[2]))
    {
        seq_len = ProcChars2Ints((unsigned int*)(s_pAdbDebug_pinfo->mipi.dsi_phy_db), &procbuf[3], copy_len);
    }

    if(0 != nChgClkRate)
    {
        h_period = ((s_mfd->panel_info.lcdc.h_pulse_width)
                    + (s_mfd->panel_info.lcdc.h_back_porch)
                    + (s_mfd->panel_info.xres)
                    + (s_mfd->panel_info.lcdc.h_front_porch));

        v_period = ((s_mfd->panel_info.lcdc.v_pulse_width)
                    + (s_mfd->panel_info.lcdc.v_back_porch)
                    + (s_mfd->panel_info.yres)
                    + (s_mfd->panel_info.lcdc.v_front_porch));

        h_period += s_mfd->panel_info.lcdc.xres_pad;
        v_period += s_mfd->panel_info.lcdc.yres_pad;

        s_mfd->panel_info.clk_rate = ((h_period * v_period * (s_mfd->panel_info.mipi.frame_rate) * 3/*bpp*/ * 8) / 4/*lanes*/);
        PRINT_LOG(("[LCD]proc_write: clk_rate = %d\n", s_mfd->panel_info.clk_rate));
    }

    up( &proc_sem );

    return copy_len;
}
#endif  // LCD_ADB_DEBUG

int SaveMsmFbData(struct msm_fb_data_type* mfd)
{
#ifdef LCD_ADB_DEBUG
    if(NULL == s_mfd)
    {
        s_mfd = mfd;
    }
#endif  // LCD_ADB_DEBUG
    return 0;
}

int InitLcdAdbDebug(struct msm_panel_info* pInfo)
{
#ifdef LCD_ADB_DEBUG
    struct proc_dir_entry* entry;
    entry = create_proc_entry( "driver/lcddbg", 
                S_IFREG | S_IRUGO | S_IWUGO, NULL );

    if ( entry != NULL ) {
        entry->read_proc  = proc_read;
        entry->write_proc = proc_write;
        s_pAdbDebug_pinfo = pInfo;
    }
    else {
        PRINT_LOG(("[LCD]InitLcdAdbDebug:  create_proc_entry failed\n"));
        return -EBUSY;
    }

    sema_init( &proc_sem, 1 );
    PRINT_LOG(("[LCD]InitLcdAdbDebug: OK\n"));

#endif  // LCD_ADB_DEBUG
    return 0;
}
