/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012 - 2013
/*----------------------------------------------------------------------------*/

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/msm_ion.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>

#include <asm/system.h>

#include "devices.h"
#include "board-8064.h"

/* FUJITSU:2013-02-12 MHL Start */
extern int makercmd_mode;
/* FUJITSU:2013-02-12 MHL End */

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
/* prim = 1366 x 768 x 3(bpp) x 3(pages) */
#define MSM_FB_PRIM_BUF_SIZE roundup(1920 * 1088 * 4 * 3, 0x10000)
/* FUJITSU:2013-02-12 MHL Start */
/* FB1(HDMI) = 720 x 480 x 4(bsp) x 3(pages) */
#define MSM_FB_PRIM_BUF_SIZE_MC roundup((1920 * 1088 * 4 * 3) + (720 * 480 * 4 * 3), 0x10000)
/* FUJITSU:2013-02-12 MHL End */
#else
/* prim = 1366 x 768 x 3(bpp) x 2(pages) */
#define MSM_FB_PRIM_BUF_SIZE roundup(1920 * 1088 * 4 * 2, 0x10000)
/* FUJITSU:2013-02-12 MHL Start */
/* FB1(HDMI) = 720 x 480 x 4(bsp) x 3(pages) */
#define MSM_FB_PRIM_BUF_SIZE_MC roundup((1920 * 1088 * 4 * 2) + (720 * 480 * 4 * 3), 0x10000)
/* FUJITSU:2013-02-12 MHL End */
#endif

#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE, 4096)
/* FUJITSU:2013-02-12 MHL Start */
#define MSM_FB_SIZE_MC roundup(MSM_FB_PRIM_BUF_SIZE_MC, 4096)
/* FUJITSU:2013-02-12 MHL End */

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((1376 * 768 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */


static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};

#define MIPI_VIDEO_R63311_FHD50S_PANEL_NAME "mipi_r63311_video_fhd50s"
#define MIPI_VIDEO_R63311_FHD50J_PANEL_NAME "mipi_r63311_video_fhd50j"
#define MIPI_VIDEO_R63311_FHD52J_PANEL_NAME "mipi_r63311_video_fhd52j"
#define MIPI_VIDEO_R63306_HD47J_PANEL_NAME "mipi_r63306_video_hd47j"
#define MIPI_VIDEO_R63306_HD47S_PANEL_NAME "mipi_r63306_video_hd47s"
/* FUJITSU:2013-03-11 Power supply control change S */
#define MIPI_VIDEO_R63306_HD47J_PROTO1_PANEL_NAME "mipi_r63306_video_proto1_hd47j"
#define MIPI_VIDEO_R63306_HD47S_PROTO1_PANEL_NAME "mipi_r63306_video_proto1_hd47s"
/* FUJITSU:2013-03-11 Power supply control change E */
/* FUJITSU:2013-03-27 add LCD config */
#define MIPI_CMD_SY35516_QHD_PANEL_NAME "mipi_sy35516_cmd_qhd"
#define MIPI_VIDEO_SY35516_QHD_PANEL_NAME "mipi_sy35516_video_qhd"
/* FUJITSU:2013-03-27 add LCD config */
/* FUJITSU:2013-04-19 add LCD config */
#define MIPI_CMD_OTM1282B_QHD_PANEL_NAME "mipi_otm1282b_cmd_qhd"
/* FUJITSU:2013-04-19 add LCD config */
/* FUJITSU:2013-05-30 add LCD config */
#define MIPI_CMD_OTM1282B2_QHD_PANEL_NAME "mipi_otm1282b2_cmd_qhd"
/* FUJITSU:2013-05-30 add LCD config */

#if 0 /* FUJITSU:2012-11-16 DISP Initial Revision del */
#define LVDS_CHIMEI_PANEL_NAME "lvds_chimei_wxga"
#define LVDS_FRC_PANEL_NAME "lvds_frc_fhd"
#define MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME "mipi_video_toshiba_wsvga"
#define MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME "mipi_video_chimei_wxga"
#endif
#define HDMI_PANEL_NAME "hdmi_msm"
#define MHL_PANEL_NAME "hdmi_msm,mhl_8334"
#if 0 /* FUJITSU:2012-11-16 DISP Initial Revision del */
#define TVOUT_PANEL_NAME "tvout_msm"
#endif

#if 0 /* FUJITSU:2012-11-16 DISP Initial Revision del */
#define LVDS_PIXEL_MAP_PATTERN_1	1
#define LVDS_PIXEL_MAP_PATTERN_2	2

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
static unsigned char hdmi_is_primary = 1;
#else
static unsigned char hdmi_is_primary;
#endif

#endif
static unsigned char mhl_display_enabled;

#if 0 /* FUJITSU:2012-11-16 DISP Initial Revision del */
unsigned char apq8064_hdmi_as_primary_selected(void)
{
	return hdmi_is_primary;
}
#endif

unsigned char apq8064_mhl_display_enabled(void)
{
	return mhl_display_enabled;
}

//static void set_mdp_clocks_for_wuxga(void); /* FUJITSU:2012-12-03 DISP avoid underflow chg */
static void set_mdp_clocks_for_fullhd(void);
/* FUJITSU:2013-02-13 DISP avoid underflow chg add S */
static void set_mdp_clocks_for_hd(void);
/* FUJITSU:2013-02-13 DISP avoid underflow chg add E */

#if 1 /* FUJITSU:2012-12-03 DISP panel discrimination add */
#define PANEL_DSCRM_GPIO	(PM8921_GPIO_PM_TO_SYS(35))
static int get_panel_discrim(void)
{
    static int panel_discrim = -1;
    int rc=0;
    
    struct pm_gpio lcd_maker_id_gpio_cfg = {
        .direction      = PM_GPIO_DIR_IN,
        .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
        .pull           = PM_GPIO_PULL_UP_1P5,
        .vin_sel        = PM_GPIO_VIN_S4,
        .out_strength   = PM_GPIO_STRENGTH_NO,
        .function       = PM_GPIO_FUNC_NORMAL,
    };
    
    pr_info("[LCD_board]%s [%d]\n",__func__,panel_discrim);
    if (panel_discrim == -1) {
        rc = pm8xxx_gpio_config(PANEL_DSCRM_GPIO, &lcd_maker_id_gpio_cfg);
        if (rc) {
            pr_err("[LCD_board]%s pm8xxx_gpio_config failed.rc=%d gpio=%d\n",__func__,rc,PANEL_DSCRM_GPIO);
        }
        msleep(5);
        
        panel_discrim = gpio_get_value_cansleep(PANEL_DSCRM_GPIO);
        pr_info("[LCD_board]%s gpio_get_value_cansleep[%d]\n",__func__,panel_discrim);
        if (panel_discrim == 1){
            lcd_maker_id_gpio_cfg.pull = PM_GPIO_PULL_DN;
        } else {
            lcd_maker_id_gpio_cfg.pull = PM_GPIO_PULL_NO;
        }
        
        rc = pm8xxx_gpio_config(PANEL_DSCRM_GPIO, &lcd_maker_id_gpio_cfg);
        if (rc) {
            pr_err("[LCD_board]%s pm8xxx_gpio_config failed.rc=%d gpio=%d\n",__func__,rc,PANEL_DSCRM_GPIO);
        }
    }
    
    return panel_discrim;
}
#endif /* FUJITSU:2012-12-03 DISP panel discrimination add */

static int msm_fb_detect_panel(const char *name)
{
    switch ( system_rev & 0xF0 ) {
        case 0x70:
        case 0x30:
        case 0x60:
            if (get_panel_discrim() == 1) {
                if (!strncmp(name, MIPI_VIDEO_R63311_FHD50S_PANEL_NAME,
                        strnlen(MIPI_VIDEO_R63311_FHD50S_PANEL_NAME,
                        PANEL_NAME_MAX_LEN))) {
                    pr_info("%s : %s detected.\n",__func__,name);
                    set_mdp_clocks_for_fullhd();
                    pr_info("%s : Bandwidth TUNED...\n",__func__);
                    return 0;
                }
            } else {
                if (!strncmp(name, MIPI_VIDEO_R63311_FHD50J_PANEL_NAME,
                        strnlen(MIPI_VIDEO_R63311_FHD50J_PANEL_NAME,
                        PANEL_NAME_MAX_LEN))) {
                    pr_info("%s : %s detected.\n",__func__,name);
                    set_mdp_clocks_for_fullhd();
                    pr_info("%s : Bandwidth TUNED...\n",__func__);
                    return 0;
                }
            }
            break;
        case 0x50:
            if ( (system_rev & 0x0F) < 0x02 ) {
                if (get_panel_discrim() == 1) {
                    if (!strncmp(name, MIPI_VIDEO_R63311_FHD50S_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63311_FHD50S_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        set_mdp_clocks_for_fullhd();
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
                } else {
                    if (!strncmp(name, MIPI_VIDEO_R63311_FHD52J_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63311_FHD52J_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        set_mdp_clocks_for_fullhd();
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
                }
            } else {
//                if (get_panel_discrim() == 1) {
//                    }
//                } else {
                    if (!strncmp(name, MIPI_VIDEO_R63311_FHD52J_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63311_FHD52J_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        set_mdp_clocks_for_fullhd();
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
//                }
            }
            break;
        case 0x40:
            /* FUJITSU:2013-02-22 Power supply control change S */
            if ( (system_rev & 0x0F) < 0x02 ) {
                if (get_panel_discrim() == 0) {
                    pr_info("%s : %d panel_discrim.\n",__func__,get_panel_discrim());
                    if (!strncmp(name, MIPI_VIDEO_R63306_HD47J_PROTO1_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63306_HD47J_PROTO1_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        /* FUJITSU:2013-02-13 DISP avoid underflow chg add S */
                        set_mdp_clocks_for_hd();
                        /* FUJITSU:2013-02-13 DISP avoid underflow chg add E */
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
                } else {
                    if (!strncmp(name, MIPI_VIDEO_R63306_HD47S_PROTO1_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63306_HD47S_PROTO1_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        /* FUJITSU:2013-02-13 DISP avoid underflow chg add S */
                        set_mdp_clocks_for_hd();
                        /* FUJITSU:2013-02-13 DISP avoid underflow chg add E */
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
                }
            }else{
                if (get_panel_discrim() == 0) {
                    pr_info("%s : %d panel_discrim.\n",__func__,get_panel_discrim());
                    if (!strncmp(name, MIPI_VIDEO_R63306_HD47J_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63306_HD47J_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        /* FUJITSU:2013-02-13 DISP avoid underflow chg add S */
                        set_mdp_clocks_for_hd();
                        /* FUJITSU:2013-02-13 DISP avoid underflow chg add E */
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
                } else {
                    if (!strncmp(name, MIPI_VIDEO_R63306_HD47S_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63306_HD47S_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        /* FUJITSU:2013-02-13 DISP avoid underflow chg add S */
                        set_mdp_clocks_for_hd();
                        /* FUJITSU:2013-02-13 DISP avoid underflow chg add E */
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
                }
            }
            /* FUJITSU:2013-02-22 Power supply control change E */
            break;
/* FUJITSU:2013-03-05 DISP add start */
        case 0x10:
        case 0x20:          /*FUJITSU:2013-05-31 mod panel detect */
            if ( (system_rev & 0x0F) < 0x02 ) { /* FUJITSU:2013-04-04 DISP add start */
                if (get_panel_discrim() == 1) {
                    if (!strncmp(name, MIPI_VIDEO_R63311_FHD50S_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63311_FHD50S_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        set_mdp_clocks_for_fullhd();
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
                } else {
                    if (!strncmp(name, MIPI_VIDEO_R63311_FHD50J_PANEL_NAME,
                            strnlen(MIPI_VIDEO_R63311_FHD50J_PANEL_NAME,
                            PANEL_NAME_MAX_LEN))) {
                        pr_info("%s : %s detected.\n",__func__,name);
                        set_mdp_clocks_for_fullhd();
                        pr_info("%s : Bandwidth TUNED...\n",__func__);
                        return 0;
                    }
                }
            } else if ( (system_rev & 0x0F) == 0x02 ) { /* FUJITSU:2013-04-04 DISP add start */
                if (!strncmp(name, MIPI_CMD_SY35516_QHD_PANEL_NAME,
                     strnlen(MIPI_CMD_SY35516_QHD_PANEL_NAME,
                     PANEL_NAME_MAX_LEN))) {
                    pr_info("%s : %s detected.\n",__func__,name);
                    set_mdp_clocks_for_hd();
                    pr_info("%s : Bandwidth TUNED...\n",__func__);
                    return 0;
                }
            } else if ( (system_rev & 0x0F) >= 0x05 ) { /* FUJITSU:2013-05-30 DISP add start */
                if (!strncmp(name, MIPI_CMD_OTM1282B2_QHD_PANEL_NAME,
                     strnlen(MIPI_CMD_OTM1282B2_QHD_PANEL_NAME,
                     PANEL_NAME_MAX_LEN))) {
                    pr_info("%s : %s detected.\n",__func__,name);
                    set_mdp_clocks_for_hd();
                    pr_info("%s : Bandwidth TUNED...\n",__func__);
                    return 0;
                }                                       /* FUJITSU:2013-05-30 DISP add start */
            } else {
/* FUJITSU:2013-04-19 DISP add start */
                if (!strncmp(name, MIPI_CMD_OTM1282B_QHD_PANEL_NAME,
                     strnlen(MIPI_CMD_OTM1282B_QHD_PANEL_NAME,
                     PANEL_NAME_MAX_LEN))) {
                    pr_info("%s : %s detected.\n",__func__,name);
                    set_mdp_clocks_for_hd();
                    pr_info("%s : Bandwidth TUNED...\n",__func__);
                    return 0;
                }
/* FUJITSU:2013-04-19 DISP add end */
            }                                           /* FUJITSU:2013-04-04 DISP add start */
            break;
/* FUJITSU:2013-03-05 DISP add end */
        default:
            pr_err("%s :[%s].\n",__func__,name);
            break;
    }

	if (!strncmp(name, HDMI_PANEL_NAME,
			strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
/* FUJITSU:2012-08-02 MHL start */
//		if (apq8064_hdmi_as_primary_selected())
//			set_mdp_clocks_for_wuxga();
/* FUJITSU:2012-08-02 MHL end */
		return 0;
	}


	return -ENODEV;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name              = "msm_fb",
	.id                = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
};

/* FUJITSU:2013-03-28 DISP enable LSM function start */
#ifdef CONFIG_SECURITY_FJSEC
extern void fjsec_make_page_acl(char *id, unsigned long addr, unsigned long size);
#endif
/* FUJITSU:2013-03-28 DISP enable LSM function end */

void __init apq8064_allocate_fb_region(void)
{
	void *addr;
	unsigned long size;

/* FUJITSU:2013-02-12 MHL Start */
	if (makercmd_mode)
		size = MSM_FB_SIZE_MC;
	else
/* FUJITSU:2013-02-12 MHL End */
		size = MSM_FB_SIZE;

	addr = alloc_bootmem_align(size, 0x1000);

/* FUJITSU:2013-03-28 DISP enable LSM function start */
#ifdef CONFIG_SECURITY_FJSEC
 	fjsec_make_page_acl("apq8064", __pa(addr), size);
	fjsec_make_page_acl("makercmd", __pa(addr), size);
	fjsec_make_page_acl("recovery", __pa(addr), size);
	fjsec_make_page_acl("fota", __pa(addr), size);
#endif
/* FUJITSU:2013-03-28 DISP enable LSM function end */

	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
			size, addr, __pa(addr));
}

#define MDP_VSYNC_GPIO 0

static struct msm_bus_vectors mdp_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors mdp_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 230400000 * 2,
		.ib = 288000000 * 2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 334080000 * 2,
		.ib = 417600000 * 2,
	},
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_vga_vectors),
		mdp_vga_vectors,
	},
	{
		ARRAY_SIZE(mdp_720p_vectors),
		mdp_720p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};

static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_max_clk = 266667000,
	.mdp_max_bw = 2000000000,
	.mdp_bw_ab_factor = 115,
	.mdp_bw_ib_factor = 125,
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
	.mdp_rev = MDP_REV_44,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_MM_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
#endif
	.mdp_iommu_split_domain = 1,
};

void __init apq8064_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;
#endif
}

static struct resource hdmi_msm_resources[] = {
	{
		.name  = "hdmi_msm_qfprom_addr",
		.start = 0x00700000,
		.end   = 0x007060FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_hdmi_addr",
		.start = 0x04A00000,
		.end   = 0x04A00FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_irq",
		.start = HDMI_IRQ,
		.end   = HDMI_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

/* FUJITSU:2013-01-31 MHL Start */
static int hdmi_gpio_state;
static int fj_mhl_gpio_state;
static int hdmi_request_gpio(int on);
static int fj_mhl_request_gpio(int on);
/* FUJITSU:2013-03-06 MHL Start */
static void fj_mhl_active_i2c_gpio(int on);
/* FUJITSU:2013-03-06 MHL End */
DEFINE_MUTEX(hdmi_gpio_state_mutex);
/* FUJITSU:2013-01-31 MHL End */
static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);
static int hdmi_gpio_config(int on);
static int hdmi_panel_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.panel_power = hdmi_panel_power,
/* FUJITSU:2013-01-31 MHL Start */
//	.gpio_config = hdmi_gpio_config,
	.gpio_config = hdmi_request_gpio,
/* FUJITSU:2013-01-31 MHL End */
};

static struct platform_device hdmi_msm_device = {
	.name = "hdmi_msm",
	.id = 0,
	.num_resources = ARRAY_SIZE(hdmi_msm_resources),
	.resource = hdmi_msm_resources,
	.dev.platform_data = &hdmi_msm_data,
};

/* FUJITSU:2013-01-31 MHL Start */
static struct fj_mhl_platform_data fj_mhl_data = {
	.mhl_hdmi_gpio = fj_mhl_request_gpio,
/* FUJITSU:2013-03-06 MHL Start */
	.active_i2c_gpio = fj_mhl_active_i2c_gpio,
/* FUJITSU:2013-03-06 MHL End */
};

static struct platform_device fj_mhl_device = {
	.name = "fj_mhl",
	.id = -1,
	.dev.platform_data = &fj_mhl_data,
};
/* FUJITSU:2013-01-31 MHL End */

/* FUJITSU:2013-03-06 MHL Start */
static struct gpiomux_setting fj_mhl_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting fj_mhl_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fj_mhl_suspend_configs[] = {
	/* GSBI 5 */
	{
		.gpio      = 54,			/* GSBI5 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fj_mhl_suspended_cfg,
			[GPIOMUX_ACTIVE] = &fj_mhl_suspended_cfg,
		},
	},
	{
		.gpio      = 53,			/* GSBI5 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fj_mhl_suspended_cfg,
			[GPIOMUX_ACTIVE] = &fj_mhl_suspended_cfg,
		},
	},
};

static struct msm_gpiomux_config fj_mhl_active_configs[] = {
	/* GSBI 5 */
	{
		.gpio      = 54,			/* GSBI5 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fj_mhl_active_cfg,
			[GPIOMUX_ACTIVE] = &fj_mhl_active_cfg,
		},
	},
	{
		.gpio      = 53,			/* GSBI5 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &fj_mhl_active_cfg,
			[GPIOMUX_ACTIVE] = &fj_mhl_active_cfg,
		},
	},
};
/* FUJITSU:2013-03-06 MHL End */


static char wfd_check_mdp_iommu_split_domain(void)
{
	return mdp_pdata.mdp_iommu_split_domain;
}

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
static struct msm_wfd_platform_data wfd_pdata = {
	.wfd_check_mdp_iommu_split = wfd_check_mdp_iommu_split_domain,
};

static struct platform_device wfd_panel_device = {
	.name = "wfd_panel",
	.id = 0,
	.dev.platform_data = NULL,
};

static struct platform_device wfd_device = {
	.name          = "msm_wfd",
	.id            = -1,
	.dev.platform_data = &wfd_pdata,
};
#endif

/* HDMI related GPIOs */
/* FUJITSU:2012-08-02 MHL start */
//#define HDMI_CEC_VAR_GPIO	69
/* FUJITSU:2012-08-02 MHL end */
#define HDMI_DDC_CLK_GPIO	70
#define HDMI_DDC_DATA_GPIO	71
#define HDMI_HPD_GPIO		72

/* FUJITSU:2013-03-19 GPIO_44 control change S */
		struct pm_gpio	gpio_config = {
			.direction		= PM_GPIO_DIR_OUT,
			.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
			.output_value	= 1,
			.pull			= PM_GPIO_PULL_NO,
			.vin_sel		= PM_GPIO_VIN_S4,
			.out_strength	= PM_GPIO_STRENGTH_MED,
			.function		= PM_GPIO_FUNC_NORMAL,
			.inv_int_pol	= 0,
			.disable_pin	= 0,
		};
/* FUJITSU:2013-03-19 GPIO_44 control change E */

/* FUJITSU:2013-02-22 Power supply control change S */
static int mipi_dsi_r63306_jdi_panel_power(int on)
{
    static bool dsi_power_init = false;
    static int resx = PM8921_GPIO_PM_TO_SYS(32);
    static int vspn = PM8921_GPIO_PM_TO_SYS(44);
    static struct regulator *reg_l2;
    static struct regulator *reg_l8;
    static struct regulator *reg_lvs5;
    
    int rc = 0;
    
    printk("[LCD_board]%s(%d) enter\n", __func__, on);
    
    if (!dsi_power_init) {
        printk("[LCD_board]%s : regulator initialize.\n",__func__);
        
        /* VDD_MIPI */
        reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi1_pll_vdda");
        if (IS_ERR_OR_NULL(reg_l2)) {
            pr_err("[LCD_board]%s : could not get 8921_l2, rc = %ld\n",__func__,PTR_ERR(reg_l2));
            return -ENODEV;
        }
        rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
        if (rc) {
            pr_err("[LCD_board]%s : set_voltage 8921_l2 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable reg_l2 failed, rc=%d\n",__func__, rc);
			return -ENODEV;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
        
        /* 2.8V_ALCD_A */
        reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_l8");
        if (IS_ERR_OR_NULL(reg_l8)) {
            pr_err("[LCD_board]%s : could not get 8921_l8, rc = %ld\n",__func__,PTR_ERR(reg_l8));
            return -ENODEV;
        }
        rc = regulator_set_voltage(reg_l8, 2800000, 2800000); // 2.8V
        if (rc) {
            pr_err("[LCD_board]%s : set_voltage 8921_l8 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
        /* FUJITSU:2013-03-11 regulator_enable control change S */
        /* 2.8V_ALCD_A */
        rc = regulator_enable(reg_l8);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
		/* FUJITSU:2013-03-11 regulator_enable control change E */
		/* FUJITSU:2013-03-19 GPIO_44 control change S */
		rc = pm8xxx_gpio_config(vspn,&gpio_config);

		if (rc){
			pr_err("[LCD_board]%s: pm8xxx_gpio_config(%u) failed: rc=%d\n", __func__, vspn, rc);
    	}
		/* FUJITSU:2013-03-19 GPIO_44 control change E */
        rc = gpio_request(vspn, "mlcd_vspn");
        printk("[LCD_board]%s : vspn gpio_request rc=%d\n",__func__, rc);

        /* 1.8V_VDD */
        reg_lvs5 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_lvs5");
        if (IS_ERR_OR_NULL(reg_lvs5)) {
            pr_err("[LCD_board]%s : could not get 8921_lvs5, rc = %ld\n",__func__,PTR_ERR(reg_lvs5));
            return -ENODEV;
        }
		/* FUJITSU:2013-03-11 regulator_enable control change S */
        /* 1.8V_VDD */
        rc = regulator_enable(reg_lvs5);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_enable 8921_lvs5 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
		/* FUJITSU:2013-03-11 regulator_enable control change E */
        rc = gpio_request(resx, "mlcd_xrst");
        printk("[LCD_board]%s : gpio_request rc=%d\n",__func__, rc);
    	dsi_power_init = true;

/* FUJITSU:2013-04-30 LCD regulator control change S */
    }else{

        if(on) {
            rc = regulator_set_optimum_mode(reg_l2, 100000);
            if (rc < 0) {
                pr_err("[LCD_board]set_optimum_mode l2 failed, rc=%d\n", rc);
                return -EINVAL;
            }
            rc = regulator_enable(reg_l2);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_enable reg_l2 failed, rc=%d\n",__func__, rc);
                return -ENODEV;
            }
            /* 2.8V_ALCD_A */
            rc = regulator_enable(reg_l8);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }

            /* 1.8V_VDD */
            rc = regulator_enable(reg_lvs5);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_enable 8921_lvs5 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            msleep(10);
            gpio_set_value(resx, 1);
            msleep(10);
            /* for 5.4V */
            gpio_set_value(vspn,1);
            printk("### [LCD_board]%s: VSPN on\n",__func__);
            msleep(10);
        }
        else {
            /* for 5.4V */
            gpio_set_value(vspn, 0);
            printk("### [LCD_board]%s: VSPN off\n",__func__);
            msleep(5);
            /* MLCD_XRST */
            gpio_set_value(resx, 0);
            /* 2.8V_ALCD_A */
            rc = regulator_disable(reg_l8);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_disable 8921_l8 failed, rc=%d\n",__func__, rc);
                return -ENODEV;
            }
            /* 1.8V_VDD */
            rc = regulator_disable(reg_lvs5);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_disable 8921_lvs5 failed, rc=%d\n",__func__, rc);
                return -ENODEV;
            }
            rc = regulator_disable(reg_l2);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_disable 8921_l2 failed, rc=%d\n",__func__, rc);
                return -ENODEV;
            }
            rc = regulator_set_optimum_mode(reg_l2, 100);
            if (rc < 0) {
                pr_err("[LCD_board]set_optimum_mode l2 failed, rc=%d\n", rc);
                return -EINVAL;
            }
        }
    }
/* FUJITSU:2013-04-30 LCD regulator control change E */
    
    printk("[LCD_board]%s(%d) leave rc(%d)\n", __func__, on, rc);
    
    return rc;
}

static int mipi_dsi_r63306_sh_panel_power(int on)
{
	static bool dsi_power_init = false;
    static int resx = PM8921_GPIO_PM_TO_SYS(32);
    static int vspn = PM8921_GPIO_PM_TO_SYS(44);
	static struct regulator *reg_l2;
	static struct regulator *reg_l8;
	static struct regulator *reg_lvs5;
	int rc = 0;
	
	printk("[LCD_board]%s(%d) enter\n", __func__, on);
	
	if (!dsi_power_init) {
		printk("[LCD_board]%s : regulator initialize.\n",__func__);
		
		/* VDD_MIPI */
		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi1_pll_vdda");
		if (IS_ERR_OR_NULL(reg_l2)) {
			pr_err("[LCD_board]%s : could not get 8921_l2, rc = %ld\n",__func__,PTR_ERR(reg_l2));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("[LCD_board]%s : set_voltage 8921_l2 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		/* VDD_MIPI */
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable reg_l2 failed, rc=%d\n",__func__, rc);
			return -ENODEV;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
		
		/* 2.8V_ALCD_A */
		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_l8");
		if (IS_ERR_OR_NULL(reg_l8)) {
			pr_err("[LCD_board]%s : could not get 8921_l8, rc = %ld\n",__func__,PTR_ERR(reg_l8));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l8, 2800000, 2800000); // 2.8V
		if (rc) {
			pr_err("[LCD_board]%s : set_voltage 8921_l8 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		rc = regulator_enable(reg_l8);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
		
		/* 1.8V_VDD */
		reg_lvs5 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_lvs5");
		if (IS_ERR_OR_NULL(reg_lvs5)) {
			pr_err("[LCD_board]%s : could not get 8921_lvs5, rc = %ld\n",__func__,PTR_ERR(reg_lvs5));
			return -ENODEV;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		/* 1.8V_VDD */
		rc = regulator_enable(reg_lvs5);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable 8921_lvs5 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
		
		rc = gpio_request(resx, "mlcd_xrst");
		printk("[LCD_board]%s : gpio_request rc=%d\n",__func__, rc);
		dsi_power_init = true;

		/* FUJITSU:2013-03-19 GPIO_44 control change S */
		rc = pm8xxx_gpio_config(vspn,&gpio_config);

		if (rc){
			pr_err("[LCD_board]%s: pm8xxx_gpio_config(%u) failed: rc=%d\n", __func__, vspn, rc);
    	}
		/* FUJITSU:2013-03-19 GPIO_44 control change E */

	}else{
		if (on) {
			gpio_set_value(resx, 0);
		    /* VDD_MIPI */
			rc = regulator_enable(reg_l2);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_enable reg_l2 failed, rc=%d\n",__func__, rc);
				return -ENODEV;
			}
		
			/* 1.8V_VDD */
			rc = regulator_enable(reg_lvs5);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_enable 8921_lvs5 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
			msleep(5);

			/* 2.8V_ALCD_A */
			rc = regulator_enable(reg_l8);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
			msleep(5);

			/* MLCD_XRST */
			gpio_set_value(resx, 0);
			msleep(10);
			gpio_set_value(resx, 1);
			msleep(10);
		} else {
        /* for 5.4V */
			gpio_set_value(vspn, 0);
			printk("### [LCD_board]%s: VSPN off\n",__func__);
			msleep(10);
			/* MLCD_XRST */
			gpio_set_value(resx, 0);
			msleep(10);
			
			/* 2.8V_ALCD_A */
			rc = regulator_disable(reg_l8);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_disable 8921_l8 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
			msleep(10);

			/* 1.8V_VDD */
			rc = regulator_disable(reg_lvs5);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_disable 8921_lvs5 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
		
			/* VDD_MIPI */
			rc = regulator_disable(reg_l2);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_disable 8921_l2 failed, rc=%d\n",__func__, rc);
				return -ENODEV;
			}
		}
	}

	printk("[LCD_board]%s(%d) leave rc(%d)\n", __func__, on, rc);
	
	return rc;
}
/* FUJITSU:2013-02-22 Power supply control change E */

/* FUJITSU:2013-02-22 Power supply control change S */
//static int mipi_dsi_r63306_jdi_panel_power(int on)
static int mipi_dsi_r63306_jdi_panel_power_proto1(int on)
/* FUJITSU:2013-02-22 Power supply control change E */
{
    static bool dsi_power_init = false;
    static int resx = PM8921_GPIO_PM_TO_SYS(32);
    static struct regulator *reg_l2;
    static struct regulator *reg_l8;
    static struct regulator *reg_l15;
    static struct regulator *reg_lvs5;
    
/* FUJITSU:2013-02-13 DISP regulator setting del S */
//  static int pmic_gpio41;
/* FUJITSU:2013-02-13 DISP regulator setting del E */
    int rc = 0;
    
    printk("[LCD_board]%s(%d) enter\n", __func__, on);
    
    if (!dsi_power_init) {
        printk("[LCD_board]%s : regulator initialize.\n",__func__);
        
        /* VDD_MIPI */
        reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi1_pll_vdda");
        if (IS_ERR_OR_NULL(reg_l2)) {
            pr_err("[LCD_board]%s : could not get 8921_l2, rc = %ld\n",__func__,PTR_ERR(reg_l2));
            return -ENODEV;
        }
        rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
        if (rc) {
            pr_err("[LCD_board]%s : set_voltage 8921_l2 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable reg_l2 failed, rc=%d\n",__func__, rc);
			return -ENODEV;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
        
/* FUJITSU:2013-02-13 DISP regulator setting chg del S */
        /* 2.8V_SENS_KO */
//      pmic_gpio41 = PM8921_GPIO_PM_TO_SYS(41);
/* FUJITSU:2013-02-13 DISP regulator setting chg del E */
        
        /* 2.8V_ALCD_A */
        reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_l8");
        if (IS_ERR_OR_NULL(reg_l8)) {
            pr_err("[LCD_board]%s : could not get 8921_l8, rc = %ld\n",__func__,PTR_ERR(reg_l8));
            return -ENODEV;
        }
        rc = regulator_set_voltage(reg_l8, 2800000, 2800000); // 2.8V
        if (rc) {
            pr_err("[LCD_board]%s : set_voltage 8921_l8 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		/* 2.8V_ALCD_A */
        rc = regulator_enable(reg_l8);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
		/* FUJITSU:2013-03-11 regulator_enable control change E */
        
        /* 2.8V_LCD_A */
        reg_l15 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_l15");
        if (IS_ERR_OR_NULL(reg_l15)) {
            pr_err("[LCD_board]%s : could not get 8921_l15, rc = %ld\n",__func__,PTR_ERR(reg_l15));
            return -ENODEV;
        }

        rc = regulator_set_voltage(reg_l15, 2800000, 2800000); // 2.8V

        if (rc) {
            pr_err("[LCD_board]%s : set_voltage 8921_l15 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
    	/* FUJITSU:2013-03-11 regulator_enable control change S */
        /* 2.8V_LCD_A */
        rc = regulator_enable(reg_l15);
        if (rc) {
           pr_err("[LCD_board]%s : regulator_enable reg_l15 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
    	/* FUJITSU:2013-03-11 regulator_enable control change E */
        
        /* 1.8V_VDD */
        reg_lvs5 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_lvs5");
        if (IS_ERR_OR_NULL(reg_lvs5)) {
            pr_err("[LCD_board]%s : could not get 8921_lvs5, rc = %ld\n",__func__,PTR_ERR(reg_lvs5));
            return -ENODEV;
        }
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		/* 1.8V_VDD */
        rc = regulator_enable(reg_lvs5);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_enable 8921_lvs5 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
    	/* FUJITSU:2013-03-11 regulator_enable control change E */
        rc = gpio_request(resx, "mlcd_xrst");
        printk("[LCD_board]%s : gpio_request rc=%d\n",__func__, rc);
/* FUJITSU:2013-02-13 DISP regulator setting chg del S */
//        gpio_tlmm_config(GPIO_CFG(resx, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//        gpio_set_value(resx, 0);
/* FUJITSU:2013-02-13 DISP regulator setting chg del E */
    	dsi_power_init = true;
    }
    if(on) {
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("[LCD_board]set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable reg_l2 failed, rc=%d\n",__func__, rc);
			return -ENODEV;
		}
        /* 2.8V_ALCD_A */
        rc = regulator_enable(reg_l8);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
        
        /* 1.8V_VDD */
        rc = regulator_enable(reg_lvs5);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_enable 8921_lvs5 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
        msleep(10);
        gpio_set_value(resx, 1);
        msleep(10);
        /* 2.8V_LCD_A */
        rc = regulator_enable(reg_l15);
        if (rc) {
           pr_err("[LCD_board]%s : regulator_enable reg_l15 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
         msleep(10);
    }
    else {
        /* 2.8V_LCD_A */
        rc = regulator_disable(reg_l15);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_disable 8921_l15 failed, rc=%d\n",__func__, rc);
            return -ENODEV;
        }
		msleep(5);
        /* MLCD_XRST */
        gpio_set_value(resx, 0);
        /* 2.8V_ALCD_A */
        rc = regulator_disable(reg_l8);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_disable 8921_l8 failed, rc=%d\n",__func__, rc);
            return -ENODEV;
        }
        /* 1.8V_VDD */
        rc = regulator_disable(reg_lvs5);
        if (rc) {
            pr_err("[LCD_board]%s : regulator_disable 8921_lvs5 failed, rc=%d\n",__func__, rc);
            return -ENODEV;
        }
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_disable 8921_l2 failed, rc=%d\n",__func__, rc);
			return -ENODEV;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("[LCD_board]set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
    }
    
    printk("[LCD_board]%s(%d) leave rc(%d)\n", __func__, on, rc);
    
    return rc;
}

/* FUJITSU:2013-02-22 Power supply control change S */
//static int mipi_dsi_r63306_sh_panel_power(int on)
static int mipi_dsi_r63306_sh_panel_power_proto1(int on)
/* FUJITSU:2013-02-22 Power supply control change E */
{
	static bool dsi_power_init = false;
    static int resx = PM8921_GPIO_PM_TO_SYS(32);
	static struct regulator *reg_l2;
	static struct regulator *reg_l8;
	static struct regulator *reg_l15;
	static struct regulator *reg_lvs5;
	int rc = 0;
	
	printk("[LCD_board]%s(%d) enter\n", __func__, on);
	
	if (!dsi_power_init) {
		printk("[LCD_board]%s : regulator initialize.\n",__func__);
		
		/* VDD_MIPI */
		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi1_pll_vdda");
		if (IS_ERR_OR_NULL(reg_l2)) {
			pr_err("[LCD_board]%s : could not get 8921_l2, rc = %ld\n",__func__,PTR_ERR(reg_l2));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("[LCD_board]%s : set_voltage 8921_l2 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		/* VDD_MIPI */
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable reg_l2 failed, rc=%d\n",__func__, rc);
			return -ENODEV;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
		
		
		/* 2.8V_ALCD_A */
		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_l8");
		if (IS_ERR_OR_NULL(reg_l8)) {
			pr_err("[LCD_board]%s : could not get 8921_l8, rc = %ld\n",__func__,PTR_ERR(reg_l8));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l8, 2800000, 2800000); // 2.8V
		if (rc) {
			pr_err("[LCD_board]%s : set_voltage 8921_l8 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		/* 2.8V_ALCD_A */
		rc = regulator_enable(reg_l8);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
		
		/* 2.8V_BLCD_A*/
		reg_l15 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_l15");
		if (IS_ERR_OR_NULL(reg_l15)) {
			pr_err("[LCD_board]%s : could not get 8921_l15, rc = %ld\n",__func__,PTR_ERR(reg_l15));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l15, 2800000, 2800000); // 2.8V
		if (rc) {
			pr_err("[LCD_board]%s : set_voltage 8921_l15 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		/* 2.8V_BLCD_A */
		rc = regulator_enable(reg_l15);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable reg_l15 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
		
		/* 1.8V_VDD */
		reg_lvs5 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_lvs5");
		if (IS_ERR_OR_NULL(reg_lvs5)) {
			pr_err("[LCD_board]%s : could not get 8921_lvs5, rc = %ld\n",__func__,PTR_ERR(reg_lvs5));
			return -ENODEV;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change S */
		/* 1.8V_VDD */
		rc = regulator_enable(reg_lvs5);
		if (rc) {
			pr_err("[LCD_board]%s : regulator_enable 8921_lvs5 failed, rc=%d\n",__func__, rc);
			return -EINVAL;
		}
		/* FUJITSU:2013-03-11 regulator_enable control change E */
		
		rc = gpio_request(resx, "mlcd_xrst");
		printk("[LCD_board]%s : gpio_request rc=%d\n",__func__, rc);
/* FUJITSU:2013-02-13 DISP regulator setting chg del S */
//		gpio_tlmm_config(GPIO_CFG(resx, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		
//		gpio_set_value(resx, 0);
/* FUJITSU:2013-02-13 DISP regulator setting chg del E */
		dsi_power_init = true;

/* FUJITSU:2013-02-13 DISP regulator setting chg S */
	}else{
		if (on) {
			gpio_set_value(resx, 0);
		    /* VDD_MIPI */
			rc = regulator_enable(reg_l2);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_enable reg_l2 failed, rc=%d\n",__func__, rc);
				return -ENODEV;
			}
		
			/* 1.8V_VDD */
			rc = regulator_enable(reg_lvs5);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_enable 8921_lvs5 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
			msleep(5);

			/* 2.8V_ALCD_A */
			rc = regulator_enable(reg_l8);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
			msleep(5);

			/* 2.8V_BLCD_A */
			rc = regulator_enable(reg_l15);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_enable reg_l15 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
			msleep(5);
		
			/* MLCD_XRST */
			gpio_set_value(resx, 0);
			msleep(10);
			gpio_set_value(resx, 1);
			msleep(10);
		} else {
			/* MLCD_XRST */
			gpio_set_value(resx, 0);
			msleep(10);
			
			/* 2.8V_BLCD_A */
			rc = regulator_disable(reg_l15);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_disable 8921_l15 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
			msleep(10);

			/* 2.8V_ALCD_A */
			rc = regulator_disable(reg_l8);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_disable 8921_l8 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
			msleep(10);

			/* 1.8V_VDD */
			rc = regulator_disable(reg_lvs5);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_disable 8921_lvs5 failed, rc=%d\n",__func__, rc);
				return -EINVAL;
			}
		
			/* VDD_MIPI */
			rc = regulator_disable(reg_l2);
			if (rc) {
				pr_err("[LCD_board]%s : regulator_disable 8921_l2 failed, rc=%d\n",__func__, rc);
				return -ENODEV;
			}
		}
	}
/* FUJITSU:2013-02-13 DISP regulator setting chg E */

	printk("[LCD_board]%s(%d) leave rc(%d)\n", __func__, on, rc);
	
	return rc;
}

static bool dsi_power_init = false;
static int r63311_power_src = 0x01; /* FUJITSU:2013-03-11 DISP add power_src flag change */
static int mipi_dsi_r63311_panel_power(int on)
{
    static struct regulator *reg_l2;
    static struct regulator *reg_lvs5 = NULL; /* FUJITSU:2013-03-11 DISP add power_src flag change */
    static struct regulator *reg_l8 = NULL; /* FUJITSU:2013-03-11 DISP add power_src flag change */
    static int resx = PM8921_GPIO_PM_TO_SYS(32);
    static int vspn = PM8921_GPIO_PM_TO_SYS(44);
    int rc;

    pr_info("[LCD_board]%s(%d) enter\n", __func__, on);

    if (!dsi_power_init) {
        pr_info("[LCD_board]%s : regulator initialize.\n",__func__);

        /* VDD_MIPI */
        reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi1_pll_vdda");
        if (IS_ERR_OR_NULL(reg_l2)) {
            pr_err("could not get 8921_l2, rc = %ld\n",
            PTR_ERR(reg_l2));
            return -ENODEV;
        }
        rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
        if (rc) {
            pr_err("set_voltage l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
        
        /* 1.8V_VDD */
        /* FUJITSU:2013-02-22 DISP add power_src flag start */
        if ( (r63311_power_src & 0x0F) == 0x01 ) { /* FUJITSU:2013-03-11 DISP add power_src flag change */
            reg_lvs5 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_lvs5");
            if (IS_ERR_OR_NULL(reg_lvs5)) {
                pr_err("[LCD_board]%s : could not get 8921_lvs5, rc = %ld\n",__func__,PTR_ERR(reg_lvs5));
                return -ENODEV;
            }
        }
        if ( (r63311_power_src & 0xF0) == 0x10 ) { /* FUJITSU:2013-03-11 DISP add power_src flag change */
            reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev, "8921_l8");
            if (IS_ERR_OR_NULL(reg_l8)) {
                pr_err("could not get 8921_l8, rc = %ld\n",
                PTR_ERR(reg_l8));
                return -ENODEV;
            }
            
            rc = regulator_set_voltage(reg_l8, 1800000, 1800000);
            if (rc) {
                pr_err("set_voltage l8 failed, rc=%d\n", rc);
                return -EINVAL;
            }
        }
        /* FUJITSU:2013-02-22 DISP add power_src flag end */
        
        rc = gpio_request(resx, "mlcd_xrst");
        pr_debug("[LCD_board]%s : resx gpio_request rc=%d\n",__func__, rc);
        
        rc = gpio_request(vspn, "mlcd_vspn");
        pr_debug("[LCD_board]%s : vspn gpio_request rc=%d\n",__func__, rc);
        
        /* FUJITSU:2013-03-21 GPIO_44 control change S */
        rc = pm8xxx_gpio_config(vspn,&gpio_config);

        if (rc){
            pr_err("[LCD_board]%s: pm8xxx_gpio_config(%u) failed: rc=%d\n", __func__, vspn, rc);
        }
        /* FUJITSU:2013-0321 GPIO_44 control change E */

        dsi_power_init = true;
    }

    if(on) {
        
        rc = regulator_set_optimum_mode(reg_l2, 100000);
        if (rc < 0) {
            pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
        rc = regulator_enable(reg_l2);
        if (rc) {
            pr_err("enable l2 failed, rc=%d\n", rc);
            return -ENODEV;
        }
        msleep(10);
        /* 1.8V_VDD */
        
#if 1   /* FUJITSU:2013-03-11 DISP add power_src flag change */
        if ( !IS_ERR_OR_NULL(reg_lvs5) ) {
            rc = regulator_enable(reg_lvs5);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_enable reg_lvs5 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_lvs5 on\n",__func__);
        }
        
        if ( !IS_ERR_OR_NULL(reg_l8) ) {
            rc = regulator_enable(reg_l8);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_l8 on\n",__func__);
        }
#else
        rc = regulator_enable(reg_vdd3); /* FUJITSU:2013-02-22 DISP add power_src flag */
        if (rc) {
            pr_err("[LCD_board]%s : regulator_enable vdd3 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
#endif  /* FUJITSU:2013-03-11 DISP add power_src flag change */
        msleep(10);
        
        /* for 5.6V */
        gpio_set_value(vspn,1);
        pr_debug("[LCD_board]%s: VSPN on\n",__func__);
        
        msleep(20);
        
        gpio_set_value(resx, 1);
        pr_debug("[LCD_board]%s: RESET Hi.\n",__func__);
        msleep(10);
    }
    else {
        gpio_set_value(resx, 0);
        pr_debug("[LCD_board]%s: RESET Low\n",__func__);
        
        msleep(10);
        
        /* for 5.6V */
        gpio_set_value(vspn, 0);
        pr_debug("[LCD_board]%s: VSPN off\n",__func__);
        msleep(20);
        
        /* 1.8V_VDD */
#if 1   /* FUJITSU:2013-03-11 DISP add power_src flag change */
        if ( !IS_ERR_OR_NULL(reg_lvs5) ) {
            rc = regulator_disable(reg_lvs5);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_disable reg_lvs5 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_lvs5 off\n",__func__);
        }
        
        if ( !IS_ERR_OR_NULL(reg_l8) ) {
            rc = regulator_disable(reg_l8);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_disable reg_l8 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_l8 off\n",__func__);
        }
#else
        pr_info("[LCD_board]%s: VDD3 off\n",__func__);
        rc = regulator_disable(reg_vdd3); /* FUJITSU:2013-02-22 DISP add power_src flag */
        if (rc) {
            pr_err("[LCD_board]%s : regulator_enable vdd3 failed, rc=%d\n",__func__, rc);
            return -EINVAL;
        }
#endif
        msleep(10);
        rc = regulator_disable(reg_l2);
        if (rc) {
            pr_err("disable reg_l2 failed, rc=%d\n", rc);
            return -ENODEV;
        }
        rc = regulator_set_optimum_mode(reg_l2, 100);
        if (rc < 0) {
            pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
    
    }
    pr_info("[LCD_board]%s(%d) leave rc(%d)\n", __func__, on, rc);
    
    return rc;
}

/* FUJITSU:2013-03-27 add LCD config */
static int sy35516_power_src = 0x01;
static int mipi_dsi_sy35516_panel_power(int on)
{
    static bool dsi_power_init = false;
    static bool lcd_reset_init = false;
    static struct regulator *reg_l2;
    static struct regulator *reg_lvs5 = NULL;
    static struct regulator *reg_l8 = NULL;
    static int resx = PM8921_GPIO_PM_TO_SYS(32);
    static int vspn = PM8921_GPIO_PM_TO_SYS(44);
    int rc;

    pr_info("[LCD_board]%s(%d) enter\n", __func__, on);

    if (!dsi_power_init) {
        pr_info("[LCD_board]%s : regulator initialize.\n",__func__);

        /* VDD_MIPI */
        reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi1_pll_vdda");
        if (IS_ERR_OR_NULL(reg_l2)) {
            pr_err("could not get 8921_l2, rc = %ld\n",
            PTR_ERR(reg_l2));
            return -ENODEV;
        }
        rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
        if (rc) {
            pr_err("set_voltage l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
        
        /* 1.8V_VDD */
        if ( (sy35516_power_src & 0x0F) == 0x01 ) {
            reg_lvs5 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_lvs5");
            if (IS_ERR_OR_NULL(reg_lvs5)) {
                pr_err("[LCD_board]%s : could not get 8921_lvs5, rc = %ld\n",__func__,PTR_ERR(reg_lvs5));
                return -ENODEV;
            }
        }
        if ( (sy35516_power_src & 0xF0) == 0x10 ) {
            reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev, "8921_l8");
            if (IS_ERR_OR_NULL(reg_l8)) {
                pr_err("could not get 8921_l8, rc = %ld\n",
                PTR_ERR(reg_l8));
                return -ENODEV;
            }
            
            rc = regulator_set_voltage(reg_l8, 1800000, 1800000);
            if (rc) {
                pr_err("set_voltage l8 failed, rc=%d\n", rc);
                return -EINVAL;
            }
        }
        
        rc = gpio_request(resx, "mlcd_xrst");
        pr_debug("[LCD_board]%s : resx gpio_request rc=%d\n",__func__, rc);
        
        rc = gpio_request(vspn, "mlcd_vspn");
        pr_debug("[LCD_board]%s : vspn gpio_request rc=%d\n",__func__, rc);
        
        rc = pm8xxx_gpio_config(vspn,&gpio_config);

        if (rc){
            pr_err("[LCD_board]%s: pm8xxx_gpio_config(%u) failed: rc=%d\n", __func__, vspn, rc);
        }

        dsi_power_init = true;
    }

    if(on) {
        
        rc = regulator_set_optimum_mode(reg_l2, 100000);
        if (rc < 0) {
            pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
        rc = regulator_enable(reg_l2);
        if (rc) {
            pr_err("enable l2 failed, rc=%d\n", rc);
            return -ENODEV;
        }
        msleep(10);
        /* 1.8V_VDD */
        
        if ( !IS_ERR_OR_NULL(reg_lvs5) ) {
            rc = regulator_enable(reg_lvs5);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_enable reg_lvs5 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_lvs5 on\n",__func__);
        }
        
        if ( !IS_ERR_OR_NULL(reg_l8) ) {
            rc = regulator_enable(reg_l8);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_enable reg_l8 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_l8 on\n",__func__);
        }
        
        msleep(5);
        
        /* for 5.6V */
        gpio_set_value(vspn,1);
        pr_debug("[LCD_board]%s: VSPN on\n",__func__);
        
        msleep(10);
        
        if ( lcd_reset_init == false) {
            pr_info("[LCD_board]%s: reset skip\n",__func__);
            lcd_reset_init = true;
            return rc;
        }
        
        gpio_set_value(resx, 1);
        pr_debug("[LCD_board]%s: RESET Hi.\n",__func__);
        msleep(10);
        
        gpio_set_value(resx, 0);
        pr_debug("[LCD_board]%s: RESET Low.\n",__func__);
        msleep(10);
        
        gpio_set_value(resx, 1);
        pr_debug("[LCD_board]%s: RESET Hi.\n",__func__);
        msleep(10);
        
    }
    else {
        lcd_reset_init = true;
        
        gpio_set_value(resx, 0);
        pr_debug("[LCD_board]%s: RESET Low\n",__func__);
        
        msleep(120);
        
        /* for 5.6V */
        gpio_set_value(vspn, 0);
        pr_debug("[LCD_board]%s: VSPN off\n",__func__);
        msleep(20);
        
        /* 1.8V_VDD */
        if ( !IS_ERR_OR_NULL(reg_lvs5) ) {
            rc = regulator_disable(reg_lvs5);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_disable reg_lvs5 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_lvs5 off\n",__func__);
        }
        
        if ( !IS_ERR_OR_NULL(reg_l8) ) {
            rc = regulator_disable(reg_l8);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_disable reg_l8 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_l8 off\n",__func__);
        }
        msleep(10);
        rc = regulator_disable(reg_l2);
        if (rc) {
            pr_err("disable reg_l2 failed, rc=%d\n", rc);
            return -ENODEV;
        }
        rc = regulator_set_optimum_mode(reg_l2, 100);
        if (rc < 0) {
            pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
    
    }
    pr_info("[LCD_board]%s(%d) leave rc(%d)\n", __func__, on, rc);
    
    return rc;
}
/* FUJITSU:2013-03-27 add LCD config */

/* FUJITSU:2013-04-22 add LCD config */
//DCDC init at aboot
static int mipi_dsi_otm1282b_panel_power(int on)
{
    static bool dsi_power_init = false;
    static bool lcd_reset_init = false;
    static struct regulator *reg_l2;
    static struct regulator *reg_lvs5 = NULL;
    static int resx = PM8921_GPIO_PM_TO_SYS(32);
    static int vspn = PM8921_GPIO_PM_TO_SYS(44);
    int rc;

    pr_info("[LCD_board]%s(%d) enter\n", __func__, on);

    if (!dsi_power_init) {
        pr_info("[LCD_board]%s : regulator initialize.\n",__func__);

        /* VDD_MIPI */
        reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi1_pll_vdda");
        if (IS_ERR_OR_NULL(reg_l2)) {
            pr_err("could not get 8921_l2, rc = %ld\n",
            PTR_ERR(reg_l2));
            return -ENODEV;
        }
        rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
        if (rc) {
            pr_err("set_voltage l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
        
        /* 1.8V_VDD */
        reg_lvs5 = regulator_get(&msm_mipi_dsi1_device.dev,"8921_lvs5");
        if (IS_ERR_OR_NULL(reg_lvs5)) {
            pr_err("[LCD_board]%s : could not get 8921_lvs5, rc = %ld\n",__func__,PTR_ERR(reg_lvs5));
            return -ENODEV;
        }
        
        rc = gpio_request(resx, "mlcd_xrst");
        pr_debug("[LCD_board]%s : resx gpio_request rc=%d\n",__func__, rc);
        
        rc = gpio_request(vspn, "mlcd_vspn");
        pr_debug("[LCD_board]%s : vspn gpio_request rc=%d\n",__func__, rc);
        
        rc = pm8xxx_gpio_config(vspn,&gpio_config);

        if (rc){
            pr_err("[LCD_board]%s: pm8xxx_gpio_config(%u) failed: rc=%d\n", __func__, vspn, rc);
        }

        dsi_power_init = true;
    }

    if(on) {
        
        rc = regulator_set_optimum_mode(reg_l2, 100000);
        if (rc < 0) {
            pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
        rc = regulator_enable(reg_l2);
        if (rc) {
            pr_err("enable l2 failed, rc=%d\n", rc);
            return -ENODEV;
        }
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(10);
        usleep_range(10*1000,10*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        /* 1.8V_VDD */
        if ( !IS_ERR_OR_NULL(reg_lvs5) ) {
            rc = regulator_enable(reg_lvs5);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_enable reg_lvs5 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_lvs5 on\n",__func__);
        }
        
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(5);
        usleep_range(5*1000,5*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        gpio_set_value(resx, 1);
        pr_debug("[LCD_board]%s: RESET Hi.\n",__func__);
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(16);
        usleep_range(16*1000,16*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        /* for 5.6V */
        gpio_set_value(vspn,1);
        pr_debug("[LCD_board]%s: VSPN on\n",__func__);
        
        if ( lcd_reset_init == false) {
            pr_info("[LCD_board]%s: reset skip\n",__func__);
            lcd_reset_init = true;
            return rc;
        }
        
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(5); //2ms->5ms
        usleep_range(5*1000,5*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        gpio_set_value(resx, 0);
        pr_debug("[LCD_board]%s: RESET Low.\n",__func__);
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(4);
        usleep_range(4*1000,4*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        gpio_set_value(resx, 1);
        pr_debug("[LCD_board]%s: RESET Hi.\n",__func__);
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(11);
        usleep_range(11*1000,11*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        gpio_set_value(resx, 0);
        pr_debug("[LCD_board]%s: RESET Low.\n",__func__);
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(4);
        usleep_range(4*1000,4*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        gpio_set_value(resx, 1);
        pr_debug("[LCD_board]%s: RESET Hi.\n",__func__);
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(11);
        usleep_range(11*1000,11*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
    }
    else {
        lcd_reset_init = true;
        
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(11);
        usleep_range(11*1000,11*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        gpio_set_value(resx, 0);
        pr_debug("[LCD_board]%s: RESET Low\n",__func__);
        
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(6);
        usleep_range(6*1000,6*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        /* for 5.6V */
        gpio_set_value(vspn,0);
        pr_debug("[LCD_board]%s: VSPN off\n",__func__);
        
/* FUJITSU:2013-06-28 DISP add waiting time cutback start */
        //msleep(5);
        usleep_range(5*1000,5*1000);
/* FUJITSU:2013-06-28 DISP add waiting time cutback end */
        
        /* 1.8V_VDD */
        if ( !IS_ERR_OR_NULL(reg_lvs5) ) {
            rc = regulator_disable(reg_lvs5);
            if (rc) {
                pr_err("[LCD_board]%s : regulator_disable reg_lvs5 failed, rc=%d\n",__func__, rc);
                return -EINVAL;
            }
            pr_debug("[LCD_board]%s: reg_lvs5 off\n",__func__);
        }
        
        rc = regulator_disable(reg_l2);
        if (rc) {
            pr_err("disable reg_l2 failed, rc=%d\n", rc);
            return -ENODEV;
        }
        rc = regulator_set_optimum_mode(reg_l2, 100);
        if (rc < 0) {
            pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
            return -EINVAL;
        }
    
    }
    pr_info("[LCD_board]%s(%d) leave rc(%d)\n", __func__, on, rc);
    
    return rc;
}
/* FUJITSU:2013-04-22 add LCD config */

static bool dsi_power_on;
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_lvs7, *reg_l2, *reg_l11, *reg_ext_3p3v;
	static int gpio36, gpio25, gpio26, mpp3;
	int rc;

	pr_debug("%s: on=%d\n", __func__, on);

	if (!dsi_power_on) {
		reg_lvs7 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi1_vddio");
		if (IS_ERR_OR_NULL(reg_lvs7)) {
			pr_err("could not get 8921_lvs7, rc = %ld\n",
				PTR_ERR(reg_lvs7));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi1_pll_vdda");
		if (IS_ERR_OR_NULL(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		reg_l11 = regulator_get(&msm_mipi_dsi1_device.dev,
						"dsi1_avdd");
		if (IS_ERR(reg_l11)) {
				pr_err("could not get 8921_l11, rc = %ld\n",
						PTR_ERR(reg_l11));
				return -ENODEV;
		}
		rc = regulator_set_voltage(reg_l11, 3000000, 3000000);
		if (rc) {
				pr_err("set_voltage l11 failed, rc=%d\n", rc);
				return -EINVAL;
		}

		if (machine_is_apq8064_liquid()) {
			reg_ext_3p3v = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi1_vccs_3p3v");
			if (IS_ERR_OR_NULL(reg_ext_3p3v)) {
				pr_err("could not get reg_ext_3p3v, rc = %ld\n",
					PTR_ERR(reg_ext_3p3v));
				reg_ext_3p3v = NULL;
				return -ENODEV;
			}
			mpp3 = PM8921_MPP_PM_TO_SYS(3);
			rc = gpio_request(mpp3, "backlight_en");
			if (rc) {
				pr_err("request mpp3 failed, rc=%d\n", rc);
				return -ENODEV;
			}
		}

		gpio25 = PM8921_GPIO_PM_TO_SYS(25);
		rc = gpio_request(gpio25, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 25 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio26 = PM8921_GPIO_PM_TO_SYS(26);
		rc = gpio_request(gpio26, "pwm_backlight_ctrl");
		if (rc) {
			pr_err("request gpio 26 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio36 = PM8921_GPIO_PM_TO_SYS(36); /* lcd1_pwr_en_n */
		rc = gpio_request(gpio36, "lcd1_pwr_en_n");
		if (rc) {
			pr_err("request gpio 36 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		dsi_power_on = true;
	}

	if (on) {
		rc = regulator_enable(reg_lvs7);
		if (rc) {
			pr_err("enable lvs7 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_set_optimum_mode(reg_l11, 110000);
		if (rc < 0) {
			pr_err("set_optimum_mode l11 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l11);
		if (rc) {
			pr_err("enable l11 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		if (machine_is_apq8064_liquid()) {
			rc = regulator_enable(reg_ext_3p3v);
			if (rc) {
				pr_err("enable reg_ext_3p3v failed, rc=%d\n",
					rc);
				return -ENODEV;
			}
			gpio_set_value_cansleep(mpp3, 1);
		}

		gpio_set_value_cansleep(gpio36, 0);
		gpio_set_value_cansleep(gpio25, 1);
		if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917)
			gpio_set_value_cansleep(gpio26, 1);
	} else {
		if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917)
			gpio_set_value_cansleep(gpio26, 0);
		gpio_set_value_cansleep(gpio25, 0);
		gpio_set_value_cansleep(gpio36, 1);

		if (machine_is_apq8064_liquid()) {
			gpio_set_value_cansleep(mpp3, 0);

			rc = regulator_disable(reg_ext_3p3v);
			if (rc) {
				pr_err("disable reg_ext_3p3v failed, rc=%d\n",
					rc);
				return -ENODEV;
			}
		}

		rc = regulator_disable(reg_l11);
		if (rc) {
			pr_err("disable reg_l1 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_disable(reg_lvs7);
		if (rc) {
			pr_err("disable reg_lvs7 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
	}

	return 0;
}

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.dsi_power_save = mipi_dsi_panel_power,
};

/* FUJITSU:2013-06-21 LCD shutdown start*/
void (*lcd_power_off)(void) = NULL;
void mipi_dsi_panel_shutdown(void)
{
    if (lcd_power_off != NULL){
		pr_info("[LCD]%s enter\n",__func__);
		lcd_power_off();
		mipi_dsi_pdata.dsi_power_save(0);
		pr_info("[LCD]%s leave\n",__func__);
	}
	return;
}
/* FUJITSU:2013-2013-06-21 LCD shutdown end*/

#if 0 /* FUJITSU:2012-11-16 DISP Initial Revision del */
static bool lvds_power_on;
static int lvds_panel_power(int on)
{
	static struct regulator *reg_lvs7, *reg_l2, *reg_ext_3p3v;
	static int gpio36, gpio26, mpp3;
	int rc;

	pr_debug("%s: on=%d\n", __func__, on);

	if (!lvds_power_on) {
		reg_lvs7 = regulator_get(&msm_lvds_device.dev,
				"lvds_vdda");
		if (IS_ERR_OR_NULL(reg_lvs7)) {
			pr_err("could not get 8921_lvs7, rc = %ld\n",
				PTR_ERR(reg_lvs7));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_lvds_device.dev,
				"lvds_pll_vdda");
		if (IS_ERR_OR_NULL(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		reg_ext_3p3v = regulator_get(&msm_lvds_device.dev,
			"lvds_vccs_3p3v");
		if (IS_ERR_OR_NULL(reg_ext_3p3v)) {
			pr_err("could not get reg_ext_3p3v, rc = %ld\n",
			       PTR_ERR(reg_ext_3p3v));
		    return -ENODEV;
		}

		gpio26 = PM8921_GPIO_PM_TO_SYS(26);
		rc = gpio_request(gpio26, "pwm_backlight_ctrl");
		if (rc) {
			pr_err("request gpio 26 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio36 = PM8921_GPIO_PM_TO_SYS(36); /* lcd1_pwr_en_n */
		rc = gpio_request(gpio36, "lcd1_pwr_en_n");
		if (rc) {
			pr_err("request gpio 36 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		mpp3 = PM8921_MPP_PM_TO_SYS(3);
		rc = gpio_request(mpp3, "backlight_en");
		if (rc) {
			pr_err("request mpp3 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		lvds_power_on = true;
	}

	if (on) {
		rc = regulator_enable(reg_lvs7);
		if (rc) {
			pr_err("enable lvs7 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_enable(reg_ext_3p3v);
		if (rc) {
			pr_err("enable reg_ext_3p3v failed, rc=%d\n", rc);
			return -ENODEV;
		}

		gpio_set_value_cansleep(gpio36, 0);
		gpio_set_value_cansleep(mpp3, 1);
		if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917)
			gpio_set_value_cansleep(gpio26, 1);
	} else {
		if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917)
			gpio_set_value_cansleep(gpio26, 0);
		gpio_set_value_cansleep(mpp3, 0);
		gpio_set_value_cansleep(gpio36, 1);

		rc = regulator_disable(reg_lvs7);
		if (rc) {
			pr_err("disable reg_lvs7 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_ext_3p3v);
		if (rc) {
			pr_err("disable reg_ext_3p3v failed, rc=%d\n", rc);
			return -ENODEV;
		}
	}

	return 0;
}

static int lvds_pixel_remap(void)
{
	u32 ver = socinfo_get_version();

	if (machine_is_apq8064_cdp() ||
	    machine_is_apq8064_liquid()) {
		if ((SOCINFO_VERSION_MAJOR(ver) == 1) &&
		    (SOCINFO_VERSION_MINOR(ver) == 0))
			return LVDS_PIXEL_MAP_PATTERN_1;
	} else if (machine_is_mpq8064_dtv()) {
		if ((SOCINFO_VERSION_MAJOR(ver) == 1) &&
		    (SOCINFO_VERSION_MINOR(ver) == 0))
			return LVDS_PIXEL_MAP_PATTERN_2;
	}
	return 0;
}

static struct lcdc_platform_data lvds_pdata = {
	.lcdc_power_save = lvds_panel_power,
	.lvds_pixel_remap = lvds_pixel_remap
};
#endif

#if 0 /* FUJITSU:2012-11-16 DISP Initial Revision del */
#define LPM_CHANNEL 2
static int lvds_chimei_gpio[] = {LPM_CHANNEL};

static struct lvds_panel_platform_data lvds_chimei_pdata = {
	.gpio = lvds_chimei_gpio,
};

static struct platform_device lvds_chimei_panel_device = {
	.name = "lvds_chimei_wxga",
	.id = 0,
	.dev = {
		.platform_data = &lvds_chimei_pdata,
	}
};
#endif

#if 0 /* FUJITSU:2012-11-16 DISP Initial Revision del */
#define FRC_GPIO_UPDATE	(SX150X_EXP4_GPIO_BASE + 8)
#define FRC_GPIO_RESET	(SX150X_EXP4_GPIO_BASE + 9)
#define FRC_GPIO_PWR	(SX150X_EXP4_GPIO_BASE + 10)

static int lvds_frc_gpio[] = {FRC_GPIO_UPDATE, FRC_GPIO_RESET, FRC_GPIO_PWR};
static struct lvds_panel_platform_data lvds_frc_pdata = {
	.gpio = lvds_frc_gpio,
};

static struct platform_device lvds_frc_panel_device = {
	.name = "lvds_frc_fhd",
	.id = 0,
	.dev = {
		.platform_data = &lvds_frc_pdata,
	}
};

static int dsi2lvds_gpio[2] = {
	LPM_CHANNEL,/* Backlight PWM-ID=0 for PMIC-GPIO#24 */
	0x1F08 /* DSI2LVDS Bridge GPIO Output, mask=0x1f, out=0x08 */
};
static struct msm_panel_common_pdata mipi_dsi2lvds_pdata = {
	.gpio_num = dsi2lvds_gpio,
};

static struct platform_device mipi_dsi2lvds_bridge_device = {
	.name = "mipi_tc358764",
	.id = 0,
	.dev.platform_data = &mipi_dsi2lvds_pdata,
};
#endif

#if 0 /* FUJITSU:2012-11-16 DISP Initial Revision del */
static int toshiba_gpio[] = {LPM_CHANNEL};
static struct mipi_dsi_panel_platform_data toshiba_pdata = {
	.gpio = toshiba_gpio,
};

static struct platform_device mipi_dsi_toshiba_panel_device = {
	.name = "mipi_toshiba",
	.id = 0,
	.dev = {
			.platform_data = &toshiba_pdata,
	}
};
#endif

static struct msm_bus_vectors dtv_bus_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 566092800 * 2,
		.ib = 707616000 * 2,
	},
};

static struct msm_bus_paths dtv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_def_vectors),
		dtv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata dtv_bus_scale_pdata = {
	dtv_bus_scale_usecases,
	ARRAY_SIZE(dtv_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_pdata = {
	.bus_scale_table = &dtv_bus_scale_pdata,
	.lcdc_power_save = hdmi_panel_power,
};

static int hdmi_panel_power(int on)
{
	int rc;

	pr_debug("%s: HDMI Core: %s\n", __func__, (on ? "ON" : "OFF"));
	rc = hdmi_core_power(on, 1);
	if (rc)
		rc = hdmi_cec_power(on);

	pr_debug("%s: HDMI Core: %s Success\n", __func__, (on ? "ON" : "OFF"));
	return rc;
}

static int hdmi_enable_5v(int on)
{
/* FUJITSU:2012-08-02 MHL start */
#if 0
	/* TBD: PM8921 regulator instead of 8901 */
	static struct regulator *reg_8921_hdmi_mvs;	/* HDMI_5V */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8921_hdmi_mvs) {
		reg_8921_hdmi_mvs = regulator_get(&hdmi_msm_device.dev,
			"hdmi_mvs");
		if (IS_ERR(reg_8921_hdmi_mvs)) {
			pr_err("could not get reg_8921_hdmi_mvs, rc = %ld\n",
				PTR_ERR(reg_8921_hdmi_mvs));
			reg_8921_hdmi_mvs = NULL;
			return -ENODEV;
		}
	}

	if (on) {
		rc = regulator_enable(reg_8921_hdmi_mvs);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"8921_hdmi_mvs", rc);
			return rc;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_8921_hdmi_mvs);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"8921_hdmi_mvs", rc);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;
#endif
/* FUJITSU:2012-08-02 MHL end */
	return 0;
}

static int hdmi_core_power(int on, int show)
{
//	static struct regulator *reg_8921_lvs7, *reg_8921_s4, *reg_ext_3p3v;
	static struct regulator *reg_8921_lvs7, *reg_8921_s4;

	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

#if 0
	/* TBD: PM8921 regulator instead of 8901 */
	if (!reg_ext_3p3v) {
		reg_ext_3p3v = regulator_get(&hdmi_msm_device.dev,
					     "hdmi_mux_vdd");
		if (IS_ERR_OR_NULL(reg_ext_3p3v)) {
			pr_err("could not get reg_ext_3p3v, rc = %ld\n",
			       PTR_ERR(reg_ext_3p3v));
			reg_ext_3p3v = NULL;
			return -ENODEV;
		}
	}
#endif

	if (!reg_8921_lvs7) {
		reg_8921_lvs7 = regulator_get(&hdmi_msm_device.dev,
					      "hdmi_vdda");
		if (IS_ERR(reg_8921_lvs7)) {
			pr_err("could not get reg_8921_lvs7, rc = %ld\n",
				PTR_ERR(reg_8921_lvs7));
			reg_8921_lvs7 = NULL;
			return -ENODEV;
		}
	}
	if (!reg_8921_s4) {
		reg_8921_s4 = regulator_get(&hdmi_msm_device.dev,
					    "hdmi_lvl_tsl");
		if (IS_ERR(reg_8921_s4)) {
			pr_err("could not get reg_8921_s4, rc = %ld\n",
				PTR_ERR(reg_8921_s4));
			reg_8921_s4 = NULL;
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_8921_s4, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage failed for 8921_s4, rc=%d\n", rc);
			return -EINVAL;
		}
	}

	if (on) {
#if 0
		/*
		 * Configure 3P3V_BOOST_EN as GPIO, 8mA drive strength,
		 * pull none, out-high
		 */
		rc = regulator_set_optimum_mode(reg_ext_3p3v, 290000);
		if (rc < 0) {
			pr_err("set_optimum_mode ext_3p3v failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_enable(reg_ext_3p3v);
		if (rc) {
			pr_err("enable reg_ext_3p3v failed, rc=%d\n", rc);
			return rc;
		}
#endif
		rc = regulator_enable(reg_8921_lvs7);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_vdda", rc);
			goto error1;
		}
		rc = regulator_enable(reg_8921_s4);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_lvl_tsl", rc);
			goto error2;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
#if 0
		rc = regulator_disable(reg_ext_3p3v);
		if (rc) {
			pr_err("disable reg_ext_3p3v failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif
		rc = regulator_disable(reg_8921_lvs7);
		if (rc) {
			pr_err("disable reg_8921_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_8921_s4);
		if (rc) {
			pr_err("disable reg_8921_s4 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

error2:
	regulator_disable(reg_8921_lvs7);
error1:
//	regulator_disable(reg_ext_3p3v);
	return rc;
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;
	int pmic_gpio14 = PM8921_GPIO_PM_TO_SYS(14);

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(HDMI_DDC_CLK_GPIO, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", HDMI_DDC_CLK_GPIO, rc);
			goto error1;
		}
		rc = gpio_request(HDMI_DDC_DATA_GPIO, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", HDMI_DDC_DATA_GPIO, rc);
			goto error2;
		}
		rc = gpio_request(HDMI_HPD_GPIO, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", HDMI_HPD_GPIO, rc);
			goto error3;
		}
		if (machine_is_apq8064_liquid()) {
			rc = gpio_request(pmic_gpio14, "PMIC_HDMI_MUX_SEL");
			if (rc) {
				pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
					"PMIC_HDMI_MUX_SEL", 14, rc);
				goto error4;
			}
			gpio_set_value_cansleep(pmic_gpio14, 0);
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(HDMI_DDC_CLK_GPIO);
		gpio_free(HDMI_DDC_DATA_GPIO);
		gpio_free(HDMI_HPD_GPIO);

		if (machine_is_apq8064_liquid()) {
			gpio_set_value_cansleep(pmic_gpio14, 1);
			gpio_free(pmic_gpio14);
		}
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;
	return 0;

error4:
	gpio_free(HDMI_HPD_GPIO);
error3:
	gpio_free(HDMI_DDC_DATA_GPIO);
error2:
	gpio_free(HDMI_DDC_CLK_GPIO);
error1:
	return rc;
}

static int hdmi_cec_power(int on)
{
/* FUJITSU:2012-08-02 MHL start */
#if 0
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(HDMI_CEC_VAR_GPIO, "HDMI_CEC_VAR");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_CEC_VAR", HDMI_CEC_VAR_GPIO, rc);
			goto error;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(HDMI_CEC_VAR_GPIO);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
error:
	return rc;
#endif
/* FUJITSU:2012-08-02 MHL end */
	return 0;
}

/* FUJITSU:2013-01-31 MHL Start */
static int hdmi_request_gpio(int on)
{
	int ret = 0;

	pr_debug("%s:%d\n", __func__, on);
	mutex_lock(&hdmi_gpio_state_mutex);
	hdmi_gpio_state = on;
	if ((hdmi_gpio_state == 1) && (fj_mhl_gpio_state == 1)) {
		pr_info("hdmi_gpio_config:ON\n");
		ret = hdmi_gpio_config(1);
	} else {
		pr_info("hdmi_gpio_config:OFF\n");
		hdmi_gpio_config(0);
	}
	mutex_unlock(&hdmi_gpio_state_mutex);	

	return ret;
}

static int fj_mhl_request_gpio(int on)
{
	int ret = 0;

	pr_debug("%s:%d\n", __func__, on);
	mutex_lock(&hdmi_gpio_state_mutex);
	fj_mhl_gpio_state = on;
	if ((hdmi_gpio_state == 1) && (fj_mhl_gpio_state == 1)) {
		pr_info("hdmi_gpio_config:ON\n");
		ret = hdmi_gpio_config(1);
	} else {
		pr_info("hdmi_gpio_config:OFF\n");
		hdmi_gpio_config(0);
	}
	mutex_unlock(&hdmi_gpio_state_mutex);	

	return ret;
}
/* FUJITSU:2013-01-31 MHL End */

/* FUJITSU:2013-03-06 MHL Start */
static void fj_mhl_active_i2c_gpio(int on)
{
	pr_debug("%s:%d\n", __func__, on);
	if (on) {
		msm_gpiomux_install(fj_mhl_active_configs, ARRAY_SIZE(fj_mhl_active_configs));
	} else {
		msm_gpiomux_install(fj_mhl_suspend_configs, ARRAY_SIZE(fj_mhl_suspend_configs));
	}
}
/* FUJITSU:2013-03-06 MHL End */


/* FUJITSU:2012-11-16 DISP Initial Revision add-s */
static int mipi_r63311_gpio[] = {
    PM8921_GPIO_PM_TO_SYS(35),/* LCD MAKER-ID */
    1,                        /* PWM-ID=1 for PMIC-GPIO#25 */
    PM8921_GPIO_PM_TO_SYS(9), /* Backlight HWEN for PMIC-GPIO#09 */
};
static struct msm_panel_common_pdata mipi_r63311_pdata = {
    .gpio_num = mipi_r63311_gpio,
};

static struct platform_device mipi_r63311_device = {
    .name = "mipi_R63311",
    .id = 0,
    .dev.platform_data = &mipi_r63311_pdata,
};
/* FUJITSU:2012-11-16 DISP Initial Revision add-e */

/* FUJITSU:2013-03-27 add LCD config */
static int mipi_sy35516_gpio[] = {
    PM8921_GPIO_PM_TO_SYS(9), /* Backlight HWEN for PMIC-GPIO#09 */
};

static struct msm_panel_common_pdata mipi_sy35516_pdata = {
    .gpio_num = mipi_sy35516_gpio,
};

static struct platform_device mipi_sy35516_device = {
    .name = "mipi_SY35516",
    .id = 0,
    .dev.platform_data = &mipi_sy35516_pdata,
};
/* FUJITSU:2013-03-27 add LCD config */

/* FUJITSU:2013-04-19 add LCD config */
static int mipi_otm1282b_gpio[] = {
    PM8921_GPIO_PM_TO_SYS(9), /* Backlight HWEN for PMIC-GPIO#09 */
};

static struct msm_panel_common_pdata mipi_otm1282b_pdata = {
    .gpio_num = mipi_otm1282b_gpio,
};

static struct platform_device mipi_otm1282b_device = {
    .name = "mipi_OTM1282B",
    .id = 0,
    .dev.platform_data = &mipi_otm1282b_pdata,
};
/* FUJITSU:2013-04-19 add LCD config */

void __init apq8064_init_fb(void)
{
	platform_device_register(&msm_fb_device);

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif

/* FUJITSU:2013/01/23 DISP change device registration start */
    switch ( system_rev & 0xF0 ) {
        case 0x70:
        case 0x30:
        case 0x50:
            /* FUJITSU:2013-02-22 DISP add power_src flag start */
            platform_device_register(&mipi_r63311_device);
            mipi_dsi_pdata.dsi_power_save = mipi_dsi_r63311_panel_power;
/* FUJITSU:2013-03-21 DISP mod Power supply control for 2-4 start */
//            if ( (system_rev & 0x0F) >= 0x05 ) r63311_power_src = 0x10;
            if ( (system_rev & 0x0F) == 0x05 ) r63311_power_src = 0x10;
/* FUJITSU:2013-03-21 DISP mod Power supply control for 2-4 end */
            mdp_pdata.cont_splash_enabled = 0x1;
            pr_info("%s: mipi_r63311_device regist(%x,%d).\n",__func__,system_rev,get_panel_discrim());
            break;
            /* FUJITSU:2013-02-22 DISP add power_src flag end */
        case 0x60:
            platform_device_register(&mipi_r63311_device);
            mipi_dsi_pdata.dsi_power_save = mipi_dsi_r63311_panel_power;
            mdp_pdata.cont_splash_enabled = 0x1;
            pr_info("%s: mipi_r63311_device regist(%x,%d).\n",__func__,system_rev,get_panel_discrim());
            break;
        case 0x40:
            /* FUJITSU:2013-02-22 Power supply control change S */
            if ( (system_rev & 0x0F) < 0x02 ) {
                if (get_panel_discrim() == 0) {
                        platform_device_register(&mipi_r63311_device);
                        pr_info("%s: mipi_r63306_device_proto1 regist(%x,%d). system_rev=0x%x\n",__func__,system_rev,get_panel_discrim(),system_rev);
                        mipi_dsi_pdata.dsi_power_save = mipi_dsi_r63306_jdi_panel_power_proto1;
                        /* FUJITSU:2013-02-13 BootLogo add S */
                        mdp_pdata.cont_splash_enabled = 0x1;
                        /* FUJITSU:2013-02-13 BootLogo add E */
                } else {
                        platform_device_register(&mipi_r63311_device);
                        pr_info("%s: mipi_r63306_device_proto1 regist(%x,%d). system_rev=0x%x\n",__func__,system_rev,get_panel_discrim(),system_rev);
                        mipi_dsi_pdata.dsi_power_save = mipi_dsi_r63306_sh_panel_power_proto1;
                        /* FUJITSU:2013-02-13 BootLogo add S */
                        mdp_pdata.cont_splash_enabled = 0x1;
                        /* FUJITSU:2013-02-13 BootLogo add E */
                       }
            }else{
                if (get_panel_discrim() == 0) {
                        platform_device_register(&mipi_r63311_device);
                        pr_info("%s: mipi_r63306_device regist(%x,%d). system_rev=0x%x\n",__func__,system_rev,get_panel_discrim(),system_rev);
                        mipi_dsi_pdata.dsi_power_save = mipi_dsi_r63306_jdi_panel_power;
                        /* FUJITSU:2013-02-13 BootLogo add S */
                        mdp_pdata.cont_splash_enabled = 0x1;
                        /* FUJITSU:2013-02-13 BootLogo add E */
                } else {
                        platform_device_register(&mipi_r63311_device);
                        pr_info("%s: mipi_r63306_device regist(%x,%d). system_rev=0x%x\n",__func__,system_rev,get_panel_discrim(),system_rev);
                        mipi_dsi_pdata.dsi_power_save = mipi_dsi_r63306_sh_panel_power;
                        /* FUJITSU:2013-02-13 BootLogo add S */
                        mdp_pdata.cont_splash_enabled = 0x1;
                        /* FUJITSU:2013-02-13 BootLogo add E */
                       }
            }
            /* FUJITSU:2013-02-22 Power supply control change E */
            break;
/* FUJITSU:2013-03-05 DISP add start */
        case 0x10:
        case 0x20:          /*FUJITSU:2013-05-31 mod panel detect */
            if ( (system_rev & 0x0F) < 0x02 ) { /* FUJITSU:2013-04-04 DISP add start */
                platform_device_register(&mipi_r63311_device);
                mipi_dsi_pdata.dsi_power_save = mipi_dsi_r63311_panel_power;
                r63311_power_src = 0x10; /* FUJITSU:2013-03-25 DISP add power_src flag change */
                mdp_pdata.cont_splash_enabled = 0x1;
                pr_info("%s: mipi_r63311_device regist(%x,%d).\n",__func__,system_rev,get_panel_discrim());
            } else if ( (system_rev & 0x0F) == 0x02 ) { /* FUJITSU:2013-04-04 DISP add start */
                platform_device_register(&mipi_sy35516_device);
                mipi_dsi_pdata.dsi_power_save = mipi_dsi_sy35516_panel_power;
                sy35516_power_src = 0x10;
                mdp_pdata.cont_splash_enabled = 0x1;
                pr_info("%s: mipi_sy35516_device regist(%x,%d).\n",__func__,system_rev,get_panel_discrim());
            } else {
/* FUJITSU:2013-04-19 DISP add start */
                platform_device_register(&mipi_otm1282b_device);
                mipi_dsi_pdata.dsi_power_save = mipi_dsi_otm1282b_panel_power;
                mdp_pdata.cont_splash_enabled = 0x1;
                pr_info("%s: mipi_otm1282b_device regist(%x,%d).\n",__func__,system_rev,get_panel_discrim());
/* FUJITSU:2013-04-19 DISP add start */
            }                                           /* FUJITSU:2013-04-04 DISP add start */
            break;
/* FUJITSU:2013-03-05 DISP add end */
        default:
            break;
    }
/* FUJITSU:2013/01/23 DISP change device registration end */

	msm_fb_register_device("mdp", &mdp_pdata);

	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
	platform_device_register(&hdmi_msm_device);
	msm_fb_register_device("dtv", &dtv_pdata);
/* FUJITSU:2013-01-31 MHL Start */
	platform_device_register(&fj_mhl_device);
/* FUJITSU:2013-01-31 MHL End */
}

#if 0 /* FUJITSU:2012-12-03 DISP avoid underflow chg */
/**
 * Set MDP clocks to high frequency to avoid DSI underflow
 * when using high resolution 1200x1920 WUXGA panels
 */
static void set_mdp_clocks_for_wuxga(void)
{
	mdp_ui_vectors[0].ab = 2000000000;
	mdp_ui_vectors[0].ib = 2000000000;
	mdp_vga_vectors[0].ab = 2000000000;
	mdp_vga_vectors[0].ib = 2000000000;
	mdp_720p_vectors[0].ab = 2000000000;
	mdp_720p_vectors[0].ib = 2000000000;
	mdp_1080p_vectors[0].ab = 2000000000;
	mdp_1080p_vectors[0].ib = 2000000000;

	if (apq8064_hdmi_as_primary_selected()) {
		dtv_bus_def_vectors[0].ab = 2000000000;
		dtv_bus_def_vectors[0].ib = 2000000000;
	}
}
#else
/**
 * Set MDP clocks to high frequency to avoid DSI underflow
 * when using high resolution 1920x1080 FullHD panels
 * ( 1920 * 1080 * 4 * 60 ) + ( 1280 * 720 * 1.5 * 60) , ab * 1.25
 */
static void set_mdp_clocks_for_fullhd(void)
{
	mdp_ui_vectors[0].ab    = 684288000 * 2;
	mdp_ui_vectors[0].ib    = 855360000 * 2;
	mdp_vga_vectors[0].ab   = 684288000 * 2;
	mdp_vga_vectors[0].ib   = 855360000 * 2;
	mdp_720p_vectors[0].ab  = 684288000 * 2;
	mdp_720p_vectors[0].ib  = 855360000 * 2;
	mdp_1080p_vectors[0].ab = 684288000 * 2;
	mdp_1080p_vectors[0].ib = 855360000 * 2;
}
/* FUJITSU:2013-02-13 DISP avoid underflow chg add S */
static void set_mdp_clocks_for_hd(void)
{
	mdp_ui_vectors[0].ab    = 216000000 * 2;
	mdp_ui_vectors[0].ib    = 270000000 * 2;
	mdp_vga_vectors[0].ab   = 216000000 * 2;
	mdp_vga_vectors[0].ib   = 270000000 * 2;
	mdp_720p_vectors[0].ab  = 304128000 * 2;
	mdp_720p_vectors[0].ib  = 380160000 * 2;
	mdp_1080p_vectors[0].ab = 407808000 * 2;
	mdp_1080p_vectors[0].ib = 509760000 * 2;
}
/* FUJITSU:2013-02-13 DISP avoid underflow chg add E*/
#endif /* FUJITSU:2012-12-03 DISP avoid underflow add */

void __init apq8064_set_display_params(char *prim_panel, char *ext_panel,
		unsigned char resolution)
{
#if 1 /* FUJITSU:2012-05-02 DISP Initial Revision del */
    pr_info("%s prim_panel_name detect.\n",__func__);
#else
	/*
	 * For certain MPQ boards, HDMI should be set as primary display
	 * by default, with the flexibility to specify any other panel
	 * as a primary panel through boot parameters.
	 */
	if (machine_is_mpq8064_hrd() || machine_is_mpq8064_cdp()) {
		pr_debug("HDMI is the primary display by default for MPQ\n");
		if (!strnlen(prim_panel, PANEL_NAME_MAX_LEN))
			strlcpy(msm_fb_pdata.prim_panel_name, HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN);
	}

	if (strnlen(prim_panel, PANEL_NAME_MAX_LEN)) {
		strlcpy(msm_fb_pdata.prim_panel_name, prim_panel,
			PANEL_NAME_MAX_LEN);
		pr_debug("msm_fb_pdata.prim_panel_name %s\n",
			msm_fb_pdata.prim_panel_name);

		if (!strncmp((char *)msm_fb_pdata.prim_panel_name,
			HDMI_PANEL_NAME, strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
			pr_debug("HDMI is the primary display by"
				" boot parameter\n");
			hdmi_is_primary = 1;
			set_mdp_clocks_for_wuxga();
		}
	}
	if (strnlen(ext_panel, PANEL_NAME_MAX_LEN)) {
		strlcpy(msm_fb_pdata.ext_panel_name, ext_panel,
			PANEL_NAME_MAX_LEN);
		pr_debug("msm_fb_pdata.ext_panel_name %s\n",
			msm_fb_pdata.ext_panel_name);

		if (!strncmp((char *)msm_fb_pdata.ext_panel_name,
			MHL_PANEL_NAME, strnlen(MHL_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
			pr_debug("MHL is external display by boot parameter\n");
			mhl_display_enabled = 1;
		}
	}

	msm_fb_pdata.ext_resolution = resolution;
	hdmi_msm_data.is_mhl_enabled = mhl_display_enabled;
#endif
}


