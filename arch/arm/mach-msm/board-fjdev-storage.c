/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
// COPYRIGHT(C) FUJITSU LIMITED 2012-2013
/*----------------------------------------------------------------------------*/

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/bootmem.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include "devices.h"
#include "board-8064.h"
#include "board-storage-common-a.h"


/* APQ8064 has 4 SDCC controllers */
enum sdcc_controllers {
	SDCC1,
	SDCC2,
	SDCC3,
	SDCC4,
	MAX_SDCC_CONTROLLER
};

/* All SDCC controllers require VDD/VCC voltage */
static struct msm_mmc_reg_data mmc_vdd_reg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.name = "sdc_vdd",
		.high_vol_level = 2950000,
		.low_vol_level = 2950000,
		.always_on = 1,
		.lpm_sup = 1,
		.lpm_uA = 9000,
		.hpm_uA = 200000, /* 200mA */
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.name = "sdc_vdd",
		.high_vol_level = 2950000,
		.low_vol_level = 2950000,
		.hpm_uA = 800000, /* 800mA */
	}
};

/* SDCC controllers may require voting for VDD IO voltage */
static struct msm_mmc_reg_data mmc_vdd_io_reg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.name = "sdc_vdd_io",
		.always_on = 1,
		.high_vol_level = 1800000,
		.low_vol_level = 1800000,
		.hpm_uA = 200000, /* 200mA */
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.name = "sdc_vdd_io",
		.high_vol_level = 2950000,
/* FUJITSU:2012-11-16 mod start */
#if 0
		.low_vol_level = 1850000,
#else
		.low_vol_level = 1800000,
#endif
/* FUJITSU:2012-11-16 mod end */
		.always_on = 1,
		.lpm_sup = 1,
		/* Max. Active current required is 16 mA */
		.hpm_uA = 16000,
		/*
		 * Sleep current required is ~300 uA. But min. vote can be
		 * in terms of mA (min. 1 mA). So let's vote for 2 mA
		 * during sleep.
		 */
		.lpm_uA = 2000,
	}
};

static struct msm_mmc_slot_reg_data mmc_slot_vreg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC1],
		.vdd_io_data = &mmc_vdd_io_reg_data[SDCC1],
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC3],
		.vdd_io_data = &mmc_vdd_io_reg_data[SDCC3],
	}
};

/* SDC1 pad data */
static struct msm_mmc_pad_drv sdc1_pad_drv_on_cfg[] = {
/* FUJITSU:2012-11-16 sdcc1 mod start */
#if 0
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_16MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_10MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_10MA}
#else
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_6MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_6MA}
#endif
/* FUJITSU:2012-11-16 sdcc1 mod end */
};

static struct msm_mmc_pad_drv sdc1_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
/* FUJITSU:2012-11-16 sdcc1 mod start */
#if 0
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_UP},
#else
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_NO_PULL},
#endif
/* FUJITSU:2012-11-16 sdcc1 mod end */
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
/* FUJITSU:2012-11-16 sdcc1 mod start */
#if 0
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_UP},
#else
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_NO_PULL},
#endif
/* FUJITSU:2012-11-16 sdcc1 mod end */
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_UP}
};

/* SDC3 pad data */
static struct msm_mmc_pad_drv sdc3_pad_drv_on_cfg[] = {
/* FUJITSU:2013-04-17 SD mod start */
#if 0
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_8MA}
#else
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_6MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_6MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_6MA}
#endif
/* FUJITSU:2013-04-17 SD mod end */
};

static struct msm_mmc_pad_drv sdc3_pad_drv_off_cfg[] = {
/* FUJITSU:2013-01-23 SD start */
#if 0
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_2MA}
#else
/* FUJITSU:2013-04-17 SD mod start */
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_6MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_6MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_6MA}
/* FUJITSU:2013-04-17 SD mod end */
#endif
/* FUJITSU:2013-01-23 SD end */
};

static struct msm_mmc_pad_pull sdc3_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC3_CLK, GPIO_CFG_NO_PULL},
/* FUJITSU:2012-11-16 mod start */
#if 0
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_UP}
#else
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_NO_PULL}
#endif
/* FUJITSU:2012-11-16 mod end */
};

static struct msm_mmc_pad_pull sdc3_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC3_CLK, GPIO_CFG_NO_PULL},
/* FUJITSU:2012-11-16 mod start */
#if 0
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_UP}
#else
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_DOWN},
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_DOWN}
#endif
/* FUJITSU:2012-11-16 mod end */
};

static struct msm_mmc_pad_pull_data mmc_pad_pull_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_pull_on_cfg,
		.off = sdc1_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_pull_on_cfg)
	},
	[SDCC3] = {
		.on = sdc3_pad_pull_on_cfg,
		.off = sdc3_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc3_pad_pull_on_cfg)
	},
};

static struct msm_mmc_pad_drv_data mmc_pad_drv_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_drv_on_cfg,
		.off = sdc1_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_drv_on_cfg)
	},
	[SDCC3] = {
		.on = sdc3_pad_drv_on_cfg,
		.off = sdc3_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc3_pad_drv_on_cfg)
	},
};

static struct msm_mmc_pad_data mmc_pad_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.pull = &mmc_pad_pull_data[SDCC1],
		.drv = &mmc_pad_drv_data[SDCC1]
	},
	[SDCC3] = {
		.pull = &mmc_pad_pull_data[SDCC3],
		.drv = &mmc_pad_drv_data[SDCC3]
	},
};

static struct msm_mmc_gpio sdc2_gpio[] = {
	{59, "sdc2_clk"},
	{57, "sdc2_cmd"},
	{62, "sdc2_dat_0"},
	{61, "sdc2_dat_1"},
	{60, "sdc2_dat_2"},
	{58, "sdc2_dat_3"},
};

static struct msm_mmc_gpio sdc4_gpio[] = {
	{68, "sdc4_clk"},
	{67, "sdc4_cmd"},
	{66, "sdc4_dat_0"},
	{65, "sdc4_dat_1"},
	{64, "sdc4_dat_2"},
	{63, "sdc4_dat_3"},
};

static struct msm_mmc_gpio_data mmc_gpio_data[MAX_SDCC_CONTROLLER] = {
	[SDCC2] = {
		.gpio = sdc2_gpio,
		.size = ARRAY_SIZE(sdc2_gpio),
	},
	[SDCC4] = {
		.gpio = sdc4_gpio,
		.size = ARRAY_SIZE(sdc4_gpio),
	}
};

static struct msm_mmc_pin_data mmc_slot_pin_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.pad_data = &mmc_pad_data[SDCC1],
	},
	[SDCC2] = {
		.is_gpio = 1,
		.gpio_data = &mmc_gpio_data[SDCC2],
	},
	[SDCC3] = {
		.pad_data = &mmc_pad_data[SDCC3],
	},
	[SDCC4] = {
		.is_gpio = 1,
		.gpio_data = &mmc_gpio_data[SDCC4],
	},
};

#define MSM_MPM_PIN_SDC1_DAT1	17
#define MSM_MPM_PIN_SDC3_DAT1	21

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static unsigned int sdc1_sup_clk_rates[] = {
	400000, 24000000, 48000000, 96000000
};

static struct mmc_platform_data sdc1_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
#ifdef CONFIG_MMC_MSM_SDC1_8_BIT_SUPPORT
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
#else
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#endif
	.sup_clk_table	= sdc1_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc1_sup_clk_rates),
	.nonremovable	= 1,
	.pin_data	= &mmc_slot_pin_data[SDCC1],
	.vreg_data	= &mmc_slot_vreg_data[SDCC1],
	.uhs_caps	= MMC_CAP_1_8V_DDR | MMC_CAP_UHS_DDR50,
/* FUJITSU:2012-11-16 sdcc1 del start */
#if 0
	.uhs_caps2	= MMC_CAP2_HS200_1_8V_SDR,
#endif
/* FUJITSU:2012-11-16 sdcc1 del end */
	.packed_write	= MMC_CAP2_PACKED_WR | MMC_CAP2_PACKED_WR_CONTROL,
	.mpm_sdiowakeup_int = MSM_MPM_PIN_SDC1_DAT1,
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
};
static struct mmc_platform_data *apq8064_sdc1_pdata = &sdc1_data;
#else
static struct mmc_platform_data *apq8064_sdc1_pdata;
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static unsigned int sdc2_sup_clk_rates[] = {
	400000, 24000000, 48000000
};

static struct mmc_platform_data sdc2_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc2_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc2_sup_clk_rates),
	.pin_data	= &mmc_slot_pin_data[SDCC2],
	.sdiowakeup_irq = MSM_GPIO_TO_INT(61),
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
};
static struct mmc_platform_data *apq8064_sdc2_pdata = &sdc2_data;
#else
static struct mmc_platform_data *apq8064_sdc2_pdata;
#endif

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static unsigned int sdc3_sup_clk_rates[] = {
/* FUJITSU:2012-12-21 sd mod start */
#if 0
	400000, 24000000, 48000000, 96000000, 192000000
#else
/* FUJITSU:2013-04-17 SD mod start */
	400000, 23530000, 47060000, 88888888
/* FUJITSU:2013-04-17 SD mod end */
#endif
/* FUJITSU:2012-12-21 sd mod end */
};

static struct mmc_platform_data sdc3_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc3_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc3_sup_clk_rates),
	.pin_data	= &mmc_slot_pin_data[SDCC3],
	.vreg_data	= &mmc_slot_vreg_data[SDCC3],
	.wpswitch_gpio	= PM8921_GPIO_PM_TO_SYS(17),
	.is_wpswitch_active_low = true,
/* FUJITSU:2012-11-16 sd mod start */
#if 0
	.status_gpio	= 26,
	.status_irq	= MSM_GPIO_TO_INT(26),
#else
	.status_gpio	= PM8921_GPIO_PM_TO_SYS(26),
	.status_irq	= PM8921_GPIO_IRQ(PM8921_IRQ_BASE, 26),
#endif
/* FUJITSU:2012-11-16 sd mod end */
	.irq_flags	= IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.is_status_gpio_active_low = 1,
	.xpc_cap	= 1,
/* FUJITSU:2012-11-16 SD mod start */
#if 0
	.uhs_caps	= (MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
			MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_DDR50 |
			MMC_CAP_UHS_SDR104 | MMC_CAP_MAX_CURRENT_800),
#else
/* FUJITSU:2013-04-17 SD mod start */
	.uhs_caps	= (MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 | MMC_CAP_UHS_DDR50 |
			MMC_CAP_MAX_CURRENT_400),
/* FUJITSU:2013-04-17 SD mod end */
#endif
/* FUJITSU:2012-11-16 SD mod end */
/* FUJITSU:2012-12-21 sdcc3 del start */
#if 0
	.mpm_sdiowakeup_int = MSM_MPM_PIN_SDC3_DAT1,
#endif
/* FUJITSU:2012-12-21 sdcc3 del end */
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
	.nonremovable   = 0,	/* FUJITSU:2013-02-04 sd add */
};
static struct mmc_platform_data *apq8064_sdc3_pdata = &sdc3_data;
#else
static struct mmc_platform_data *apq8064_sdc3_pdata;
#endif


#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static unsigned int sdc4_sup_clk_rates[] = {
	400000, 24000000, 48000000
};

static struct mmc_platform_data sdc4_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc4_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc4_sup_clk_rates),
	.pin_data	= &mmc_slot_pin_data[SDCC4],
	.sdiowakeup_irq = MSM_GPIO_TO_INT(65),
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
};
static struct mmc_platform_data *apq8064_sdc4_pdata = &sdc4_data;
#else
static struct mmc_platform_data *apq8064_sdc4_pdata;
#endif

void __init apq8064_init_mmc(void)
{
	if (apq8064_sdc1_pdata)
		apq8064_add_sdcc(1, apq8064_sdc1_pdata);

	if (apq8064_sdc2_pdata)
		apq8064_add_sdcc(2, apq8064_sdc2_pdata);

	if (apq8064_sdc3_pdata) {
		if (!machine_is_apq8064_cdp()) {
			apq8064_sdc3_pdata->wpswitch_gpio = 0;
			apq8064_sdc3_pdata->is_wpswitch_active_low = false;
		}
		if (machine_is_mpq8064_cdp() || machine_is_mpq8064_hrd() ||
			machine_is_mpq8064_dtv()) {
			int rc;
			struct pm_gpio sd_card_det_init_cfg = {
				.direction      = PM_GPIO_DIR_IN,
				.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
				.pull           = PM_GPIO_PULL_UP_30,
				.vin_sel        = PM_GPIO_VIN_S4,
				.out_strength   = PM_GPIO_STRENGTH_NO,
				.function       = PM_GPIO_FUNC_NORMAL,
			};

			apq8064_sdc3_pdata->status_gpio =
				PM8921_GPIO_PM_TO_SYS(31);
			apq8064_sdc3_pdata->status_irq =
				PM8921_GPIO_IRQ(PM8921_IRQ_BASE, 31);
			rc = pm8xxx_gpio_config(apq8064_sdc3_pdata->status_gpio,
					&sd_card_det_init_cfg);
			if (rc) {
				pr_info("%s: SD_CARD_DET GPIO%d config "
					"failed(%d)\n", __func__,
					apq8064_sdc3_pdata->status_gpio, rc);
				apq8064_sdc3_pdata->status_gpio = 0;
				apq8064_sdc3_pdata->status_irq = 0;
			}
		}
		if (machine_is_apq8064_cdp()) {
			int i;

			for (i = 0;
			     i < apq8064_sdc3_pdata->pin_data->pad_data->\
				 drv->size;
			     i++)
				apq8064_sdc3_pdata->pin_data->pad_data->\
					drv->on[i].val = GPIO_CFG_10MA;
		}
		apq8064_add_sdcc(3, apq8064_sdc3_pdata);
	}

	if (apq8064_sdc4_pdata)
		apq8064_add_sdcc(4, apq8064_sdc4_pdata);
}
