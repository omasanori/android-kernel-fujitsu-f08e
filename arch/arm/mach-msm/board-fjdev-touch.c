/*
 * arch/arm/mach-tegra/board-fjdev-touch.c
 *
 * COPYRIGHT(C) 2013 FUJITSU LIMITED
 * Copyright (c) 2011-2012, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>

#include <linux/input/cyttsp4_bus.h>
#include <linux/input/cyttsp4_core.h>
#include <linux/input/cyttsp4_mt.h>

#include <linux/input/t13xhmkj.h>
#include <linux/input/t13xhmwj.h>
/* FUJITSU:2013-06-11 HMW_TOUCH_SPD_SET_CONFIG add start */
#include <linux/input/t13xspkj.h>
#include <linux/input/t13xspwj.h>
/* FUJITSU:2013-06-11 HMW_TOUCH_SPD_SET_CONFIG add end */

#include <mach/vreg.h>
#include "board-8064.h"
#include <asm/system.h>

#define CYTTSP4_I2C_NAME      "cyttsp4_i2c_adapter"
#define CYTTSP4_I2C_TCH_ADR   0x24
#define CYTTSP4_LDR_TCH_ADR   0x24
#define CYTTSP4_I2C_IRQ_GPIO  34                         /* APQ8064: GPIO[34] */
#define CYTTSP4_I2C_RST_GPIO  PM8921_GPIO_PM_TO_SYS(14)  /* PM8921 : GPIO_14  */
#define CYTTSP4_VDDA_GPIO     PM8921_GPIO_PM_TO_SYS(30)  /* PM8921 : GPIO_30  */
#define CYTTSP4_VDO_GPIO      PM8921_GPIO_PM_TO_SYS(19)  /* PM8921 : GPIO_19  */

#define CY_MAXX             1079
#define CY_MAXY             1919
/* PANEL_QHD */
#define CY_MAXX_QHD          539
#define CY_MAXY_QHD          959
/* PANEL_HD */
#define CY_MAXX_HD           719
#define CY_MAXY_HD          1279

#define CY_MINX             0
#define CY_MINY             0

#define CY_ABS_MIN_X        CY_MINX
#define CY_ABS_MIN_Y        CY_MINY
#define CY_ABS_MIN_P        0
#define CY_ABS_MIN_W        0
#define CY_ABS_MIN_T        0
#define CY_ABS_MIN_MJ       0
#define CY_ABS_MIN_MN       0
#define CY_ABS_MIN_OR      -128

#define CY_ABS_MAX_X        CY_MAXX
#define CY_ABS_MAX_Y        CY_MAXY
/* PANEL_QHD */
#define CY_ABS_MAX_X_QHD    CY_MAXX_QHD
#define CY_ABS_MAX_Y_QHD    CY_MAXY_QHD
/* PANEL_HD */
#define CY_ABS_MAX_X_HD     CY_MAXX_HD
#define CY_ABS_MAX_Y_HD     CY_MAXY_HD

#define CY_ABS_MAX_P        255
#define CY_ABS_MAX_W        255
#define CY_ABS_MAX_T        15
#define CY_ABS_MAX_MJ       255
#define CY_ABS_MAX_MN       255
#define CY_ABS_MAX_OR       127

#define CY_IGNORE_VALUE     0xFFFF
#define CY_WAKE_DFLT        99

#define CY_TRIAL_TYP_MSK        0x0F
#define CY_TRIAL_TYP_HMW_1_2    0x02

#define CY_GPIO_LCD_TYPE         PM8921_GPIO_PM_TO_SYS(35)  /* PM8921 : GPIO_35  */
#define CY_MODEL_PTN_MSK         0xF0
#define CY_MODEL_PTN_SFT         4
#define CY_TYPE_PTN_MSK          0x0F
#define CY_TYPE_PTN_SFT          0

#define CY_MT_TOUCH_ROTATE_FLAGS (CY_MT_FLAG_INV_X | CY_MT_FLAG_INV_Y)
/***********************************/
/* Platform Data for Loader Module */
/***********************************/
static struct touch_settings cyttsp4_sett_param_regs = {
	.data = NULL,
	.size = 0,
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = NULL,
	.size = 0,
	.tag = 0,
};

static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = &cyttsp4_sett_param_regs,
	.param_size = &cyttsp4_sett_param_size,
	.fw_ver = NULL,
	.fw_vsize = 0,
};

#if 0
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = (uint8_t *)&cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = (uint8_t *)&cyttsp4_ver[0],
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#endif

static struct cyttsp4_loader_platform_data _cyttsp4_loader_platform_data = {
	.fw = NULL,
	.ttconfig = &cyttsp4_ttconfig,
	.flags = 0,
};

/****************************************/
/* Platform Data for Multi-Touch Module */
/****************************************/
static const uint16_t cyttsp4_abs[] = {
	ABS_MT_POSITION_X,  CY_ABS_MIN_X,  CY_ABS_MAX_X,  0, 0,
	ABS_MT_POSITION_Y,  CY_ABS_MIN_Y,  CY_ABS_MAX_Y,  0, 0,
	ABS_MT_PRESSURE,    CY_ABS_MIN_P,  CY_ABS_MAX_P,  0, 0,
	CY_IGNORE_VALUE,    CY_ABS_MIN_W,  CY_ABS_MAX_W,  0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T,  CY_ABS_MAX_T,  0, 0,
	ABS_MT_TOUCH_MAJOR, CY_ABS_MIN_MJ, CY_ABS_MAX_MJ, 0, 0,
	ABS_MT_TOUCH_MINOR, CY_ABS_MIN_MN, CY_ABS_MAX_MN, 0, 0,
	ABS_MT_ORIENTATION, CY_ABS_MIN_OR, CY_ABS_MAX_OR, 0, 0,
};

struct touch_framework cyttsp4_framework = {
	.abs = (uint16_t *)&cyttsp4_abs[0],
	.size = ARRAY_SIZE(cyttsp4_abs),
	.enable_vkeys = 0,
};

static const uint16_t cyttsp4_abs_hd[] = {
	ABS_MT_POSITION_X,  CY_ABS_MIN_X,  CY_ABS_MAX_X_HD,  0, 0,
	ABS_MT_POSITION_Y,  CY_ABS_MIN_Y,  CY_ABS_MAX_Y_HD,  0, 0,
	ABS_MT_PRESSURE,    CY_ABS_MIN_P,  CY_ABS_MAX_P,     0, 0,
	CY_IGNORE_VALUE,    CY_ABS_MIN_W,  CY_ABS_MAX_W,     0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T,  CY_ABS_MAX_T,     0, 0,
	ABS_MT_TOUCH_MAJOR, CY_ABS_MIN_MJ, CY_ABS_MAX_MJ,    0, 0,
	ABS_MT_TOUCH_MINOR, CY_ABS_MIN_MN, CY_ABS_MAX_MN,    0, 0,
	ABS_MT_ORIENTATION, CY_ABS_MIN_OR, CY_ABS_MAX_OR,    0, 0,
};

struct touch_framework cyttsp4_framework_hd = {
	.abs = (uint16_t *)&cyttsp4_abs_hd[0],
	.size = ARRAY_SIZE(cyttsp4_abs_hd),
	.enable_vkeys = 0,
};

static const uint16_t cyttsp4_abs_qhd[] = {
	ABS_MT_POSITION_X,  CY_ABS_MIN_X,  CY_ABS_MAX_X_QHD,  0, 0,
	ABS_MT_POSITION_Y,  CY_ABS_MIN_Y,  CY_ABS_MAX_Y_QHD,  0, 0,
	ABS_MT_PRESSURE,    CY_ABS_MIN_P,  CY_ABS_MAX_P,      0, 0,
	CY_IGNORE_VALUE,    CY_ABS_MIN_W,  CY_ABS_MAX_W,      0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T,  CY_ABS_MAX_T,      0, 0,
	ABS_MT_TOUCH_MAJOR, CY_ABS_MIN_MJ, CY_ABS_MAX_MJ,     0, 0,
	ABS_MT_TOUCH_MINOR, CY_ABS_MIN_MN, CY_ABS_MAX_MN,     0, 0,
	ABS_MT_ORIENTATION, CY_ABS_MIN_OR, CY_ABS_MAX_OR,     0, 0,
};

struct touch_framework cyttsp4_framework_qhd = {
	.abs = (uint16_t *)&cyttsp4_abs_qhd[0],
	.size = ARRAY_SIZE(cyttsp4_abs_qhd),
	.enable_vkeys = 0,
};

static struct cyttsp4_mt_platform_data _cyttsp4_mt_platform_data = {
	.frmwrk = &cyttsp4_framework,
	.flags = 0x00,
	.inp_dev_name = CYTTSP4_MT_NAME,
};

struct cyttsp4_device_info cyttsp4_mt_info __initdata = {
	.name = CYTTSP4_MT_NAME,
	.core_id = "main_ttsp_core",
	.platform_data = &_cyttsp4_mt_platform_data,
};

/*********************************/
/* Platform Data for Core Module */
/*********************************/
static int cyttsp4_get_model(void)
{
	int  ptn = 0;

	ptn = (system_rev & CY_MODEL_PTN_MSK);
	ptn = (ptn >> CY_MODEL_PTN_SFT);

	return ptn;
}

static int cyttsp4_get_trial_type(void)
{
	return	(int)(system_rev & CY_TRIAL_TYP_MSK);
}

static int cyttsp4_hw_reset(struct cyttsp4_core_platform_data *pdata, struct device *dev)
{
	int retval = 0;

	pr_info("[TPD] %s: IN XRST GPIO[%d] \n", __func__, CYTTSP4_I2C_RST_GPIO);
	gpio_set_value(CYTTSP4_I2C_RST_GPIO, 1);
	msleep(20);
	gpio_set_value(CYTTSP4_I2C_RST_GPIO, 0);
	msleep(40);
	gpio_set_value(CYTTSP4_I2C_RST_GPIO, 1);
	msleep(20);

	return retval;
}

static int cyttsp4_wakeup(struct cyttsp4_core_platform_data *pdata, struct device *dev, atomic_t *ignore_irq)
{
	int irq_gpio = pdata->irq_gpio;
	int rc = 0;

	if (ignore_irq){
		atomic_set(ignore_irq, 1);
	}
	rc = gpio_direction_output(irq_gpio, 0);
	if (rc < 0) {
		if (ignore_irq){
			atomic_set(ignore_irq, 0);
			dev_err(dev, "%s: Fail set output gpio=%d\n", __func__, irq_gpio);
		}
	} else {
		udelay(2000);
		rc = gpio_direction_input(irq_gpio);
		if (ignore_irq){
			atomic_set(ignore_irq, 0);
		}
		if (rc < 0) {
			dev_err(dev, "%s: Fail set input gpio=%d\n", __func__, irq_gpio);
		}
	}
	dev_info(dev, "%s: WAKEUP CYTTSP gpio=%d r=%d\n", __func__, irq_gpio, rc);
	return rc;
}

static int cyttsp4_sleep(struct cyttsp4_core_platform_data *pdata, struct device *dev, atomic_t *ignore_irq)
{
	return 0;
}

static int cyttsp4_power(struct cyttsp4_core_platform_data *pdata, int on, struct device *dev, atomic_t *ignore_irq)
{
	if (on){
		return cyttsp4_wakeup(pdata, dev, ignore_irq);
	}
	return cyttsp4_sleep(pdata, dev, ignore_irq);
}

static int cyttsp4_irq_stat(struct cyttsp4_core_platform_data *pdata, struct device *dev)
{
	int irq_stat = 0;
	irq_stat = gpio_get_value(CYTTSP4_I2C_IRQ_GPIO);
	return irq_stat;
}

static void cyttsp4_power_on_sequence(bool on)
{
	struct regulator *reg_cap_v28;
	struct regulator *reg_cap_v18;
	int retval = 0;
	int trial = cyttsp4_get_trial_type();

	if (trial >= CY_TRIAL_TYP_HMW_1_2) {
		if(on){
			/* GPIO VDDA */
			retval = gpio_request(CYTTSP4_VDDA_GPIO, "cyttsp_vdda");
			if (retval < 0) {
				pr_err("cyttsp_dev_init:cyttsp CYTTSP4_VDDA_GPIO request(%d) fail ret[%d]\n",CYTTSP4_VDDA_GPIO,retval);
			}
			gpio_direction_output(CYTTSP4_VDDA_GPIO,1);
			msleep( 2 );

			/* GPIO VDO */
			retval = gpio_request(CYTTSP4_VDO_GPIO, "cyttsp_vdo");
			if (retval < 0) {
				pr_err("cyttsp_dev_init:cyttsp CY_VDO_GPIO request(%d) fail ret[%d]\n",CYTTSP4_VDO_GPIO,retval);
			}
			gpio_direction_output(CYTTSP4_VDO_GPIO,1);
			msleep( 10 );
		} else {
			/* GPIO VDDA */
			gpio_direction_output(CYTTSP4_VDDA_GPIO,0);
			/* GPIO VDO */
			gpio_direction_output(CYTTSP4_VDO_GPIO,0);
		}
		return;
	} else {
		reg_cap_v28 = regulator_get(NULL, "8921_l16");
		reg_cap_v18 = regulator_get(NULL, "8921_lvs4");
		printk( KERN_INFO "%s :[TP_pow] regulator V2.8 & regulator V1.8\n", __func__ );
		if (on) {
			retval = regulator_set_voltage(reg_cap_v28, 2800000, 2800000);
			if (retval) {
				pr_err("%s: unable to set regulator voltage to 2.8 V\n" ,__func__);
			}
			retval = regulator_enable(reg_cap_v28);
			if (retval) {
				pr_err("%s: V28_reg_enable failed %d\n", __func__, retval);
			}
			if (reg_cap_v18 != NULL) {
				printk( KERN_INFO "%s :[TP_pow] ON V1.8_reg not NULL\n", __func__ );
				retval = regulator_set_voltage(reg_cap_v18, 1800000, 1800000);
				if (retval) {
					pr_err("%s: unable to set V1.8_reg voltage to 1.8 V\n" ,__func__);
				}
				retval = regulator_enable(reg_cap_v18);
				if (retval) {
					pr_err("%s: V18_reg_enable failed %d\n", __func__, retval);
				}
			}
			msleep(40);
		} else {
			gpio_direction_output(CYTTSP4_I2C_RST_GPIO, 0);
			retval = regulator_disable(reg_cap_v28);
			if (retval) {
				pr_err("%s: v28_reg_disable failed %d\n", __func__, retval);
			}
			regulator_put(reg_cap_v28);
			if (reg_cap_v18 != NULL) {
				printk( KERN_INFO "%s :[TP_pow] OFF V18_reg not NULL\n", __func__ );
				retval = regulator_disable(reg_cap_v18);
				if (retval) {
					pr_err("%s: V18_reg_disable failed %d\n", __func__, retval);
				}
				regulator_put(reg_cap_v18);
			}
		}
	}
}

static void cyttsp4_power_on(bool on)
{
	printk( KERN_INFO "%s :[IN]\n", __func__ );

	cyttsp4_power_on_sequence(on);

	printk( KERN_INFO "%s :[OUT]\n", __func__);
}

static int cyttsp4_hw_init(struct cyttsp4_core_platform_data *pdata, int on, struct device *dev)
{
	int retval = 0;

	/* GPIO XRST */
	retval = gpio_request(CYTTSP4_I2C_RST_GPIO, "cyttsp_xrst");
	if (retval < 0) {
		pr_err("cyttsp_dev_init:cyttsp CYTTSP4_I2C_RST_GPIO request(%d) fail ret[%d]\n",CYTTSP4_I2C_RST_GPIO,retval);
	}
    gpio_direction_output(CYTTSP4_I2C_RST_GPIO,0);

	/* GPIO INT */
	retval = gpio_request(CYTTSP4_I2C_IRQ_GPIO, "cyttsp_int");
	if (retval < 0) {
		pr_err("cyttsp_dev_init:cyttsp CY_I2C_IRQ_GPIO request(%d) fail ret[%d]\n",CYTTSP4_I2C_IRQ_GPIO,retval);
	}
	gpio_tlmm_config( GPIO_CFG(CYTTSP4_I2C_IRQ_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_direction_input(CYTTSP4_I2C_IRQ_GPIO);

	cyttsp4_power_on(true);

	/* 12ms wait*/
    mdelay(12);

	return 0;
}

static int cyttsp4_get_lcdid(void)
{
	int model = 0;
	int rc, ret = 0;
	struct pm_gpio lcd_maker_id_gpio_cfg = {
		.direction      = PM_GPIO_DIR_IN,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.pull           = PM_GPIO_PULL_UP_1P5,
		.vin_sel        = PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_NO,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	rc = pm8xxx_gpio_config(CY_GPIO_LCD_TYPE, &lcd_maker_id_gpio_cfg);
	if (rc) {
		printk("[TPD]%s:pm8xxx_gpio_config failed.rc=%d gpio=%d\n\n", __func__, rc, CY_GPIO_LCD_TYPE);
	}
	msleep(5);

	ret = gpio_get_value_cansleep(CY_GPIO_LCD_TYPE);

	if (ret == 1){
		lcd_maker_id_gpio_cfg.pull = PM_GPIO_PULL_DN;
	} else {
		lcd_maker_id_gpio_cfg.pull = PM_GPIO_PULL_NO;
	}

	/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL mod */
	model = cyttsp4_get_model();
	switch ( model ) {
	case CY_MODEL_PTN1:
	case CY_MODEL_PTN2:
	case CY_MODEL_PTN3:
		if (cyttsp4_get_trial_type() <= CY_TRIAL_TYP_HMW_1_2) {
			ret = -1;
		} else {
			ret = CY_LCDID_TYP0;
		}
		break;
	default:
		break;
	}
	/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL mod */

	rc = pm8xxx_gpio_config(CY_GPIO_LCD_TYPE, &lcd_maker_id_gpio_cfg);
	if (rc) {
		printk("[TPD]%s:pm8xxx_gpio_config failed.rc=%d gpio=%d\n\n", __func__, rc, CY_GPIO_LCD_TYPE);
	}

	return	ret;
}

static void cyttsp4_change_params(struct cyttsp4_touch_firmware_info *fi)
{
	int lcdid = cyttsp4_get_lcdid();
/* FUJITSU:2013-06-11 HMW_TOUCH_SPD_SET_CONFIG add start */
	int model = cyttsp4_get_model();
/* FUJITSU:2013-06-11 HMW_TOUCH_SPD_SET_CONFIG add end */

/* FUJITSU:2013-06-11 HMW_TOUCH_SPD_SET_CONFIG mod start */
	printk("[TPD]%s:Model Pattern[%X]\n", __func__, model);

	switch(model){
	case CY_MODEL_PTN1:
		switch(lcdid){
		case CY_LCDID_TYP0:
			switch(fi->panel_maker_code){
			case CY_VENDOR_ID2:
				printk("[TPD]%s:change the configuration data:Panel[%X]-Lcd[%X]\n", __func__, fi->panel_maker_code, lcdid);
				cyttsp4_sett_param_regs.data = (uint8_t *)&t13xhmwj_param_regs[0];
				cyttsp4_sett_param_regs.size = sizeof(t13xhmwj_param_regs);
				cyttsp4_sett_param_size.data = (uint8_t *)&t13xhmwj_param_size[0];
				cyttsp4_sett_param_size.size = sizeof(t13xhmwj_param_size);
				cyttsp4_ttconfig.fw_ver = t13xhmwj_ttconfig_fw_ver;
				cyttsp4_ttconfig.fw_vsize = sizeof(t13xhmwj_ttconfig_fw_ver);
				break;
			case CY_VENDOR_ID3:
				printk("[TPD]%s:change the configuration data:Panel[%X]-Lcd[%X]\n", __func__, fi->panel_maker_code, lcdid);
				cyttsp4_sett_param_regs.data = (uint8_t *)&t13xhmkj_param_regs[0];
				cyttsp4_sett_param_regs.size = sizeof(t13xhmkj_param_regs);
				cyttsp4_sett_param_size.data = (uint8_t *)&t13xhmkj_param_size[0];
				cyttsp4_sett_param_size.size = sizeof(t13xhmkj_param_size);
				cyttsp4_ttconfig.fw_ver = t13xhmkj_ttconfig_fw_ver;
				cyttsp4_ttconfig.fw_vsize = sizeof(t13xhmkj_ttconfig_fw_ver);
				break;
			default :
				printk("[TPD]%s:Do not change the configuration data:Panel[%X]-Lcd[%X]\n", __func__, fi->panel_maker_code, lcdid);
				break;
			}
			break;
		default :
			printk("[TPD]%s:Do not change the configuration data:-Panel[%X]-Lcd[%X]\n", __func__, fi->panel_maker_code, lcdid);
			break;
		}
		break;
	case CY_MODEL_PTN2:
		switch(lcdid){
		case CY_LCDID_TYP0:
			switch(fi->panel_maker_code){
			case CY_VENDOR_ID2:
				printk("[TPD]%s:change the configuration data:Panel[%X]-Lcd[%X]\n", __func__, fi->panel_maker_code, lcdid);
				cyttsp4_sett_param_regs.data = (uint8_t *)&t13xspwj_param_regs[0];
				cyttsp4_sett_param_regs.size = sizeof(t13xspwj_param_regs);
				cyttsp4_sett_param_size.data = (uint8_t *)&t13xspwj_param_size[0];
				cyttsp4_sett_param_size.size = sizeof(t13xspwj_param_size);
				cyttsp4_ttconfig.fw_ver = t13xspwj_ttconfig_fw_ver;
				cyttsp4_ttconfig.fw_vsize = sizeof(t13xspwj_ttconfig_fw_ver);
				break;
			case CY_VENDOR_ID3:
				printk("[TPD]%s:change the configuration data:Panel[%X]-Lcd[%X]\n", __func__, fi->panel_maker_code, lcdid);
				cyttsp4_sett_param_regs.data = (uint8_t *)&t13xspkj_param_regs[0];
				cyttsp4_sett_param_regs.size = sizeof(t13xspkj_param_regs);
				cyttsp4_sett_param_size.data = (uint8_t *)&t13xspkj_param_size[0];
				cyttsp4_sett_param_size.size = sizeof(t13xspkj_param_size);
				cyttsp4_ttconfig.fw_ver = t13xspkj_ttconfig_fw_ver;
				cyttsp4_ttconfig.fw_vsize = sizeof(t13xspkj_ttconfig_fw_ver);
				break;
			default :
				printk("[TPD]%s:Do not change the configuration data:Panel[%X]-Lcd[%X]\n", __func__, fi->panel_maker_code, lcdid);
				break;
			}
			break;
		default :
			printk("[TPD]%s:Do not change the configuration data:-Panel[%X]-Lcd[%X]\n", __func__, fi->panel_maker_code, lcdid);
			break;
		}
		break;
	case CY_MODEL_PTN3:
	default :
		printk("[TPD]%s:Do not change the configuration data\n", __func__);
		break;
	}
/* FUJITSU:2013-06-11 HMW_TOUCH_SPD_SET_CONFIG mod end */

}

static void cyttsp4_init_flags(struct cyttsp4_touch_firmware_info *fi)
{
	int trial = cyttsp4_get_trial_type();

	printk("%s:trial = %x\n", __func__, trial);
	_cyttsp4_mt_platform_data.flags |= CY_MT_FLAG_NO_TOUCH_ON_LO;
	switch(fi->firmware_version){
	case CY_FIRMWARE_PRESS_V0_PRT:
		pr_info("[TPD_Bootup] coordinate rotation 180 (%x)\n", fi->firmware_version);
		_cyttsp4_mt_platform_data.flags |= CY_MT_TOUCH_ROTATE_FLAGS;
		break;
	default :
		pr_info("[TPD_Bootup] not rotation (%x)\n", fi->firmware_version);
		break;
	}
}

static int cyttsp4_pdata_init(struct cyttsp4_core_platform_data* pdata, struct cyttsp4_touch_firmware_info *fi, int flags)
{
	int retval = 0;

	if(flags & CY_INIT_FLAG_SET_CONFIG_PARAMS){
		cyttsp4_change_params(fi);
	}
	if(flags & CY_INIT_FLAG_SET_PDATA_FLAGS){
		cyttsp4_init_flags(fi);
	}

	return retval;
}

static int cyttsp4_get_platform_info(struct cyttsp4_core_platform_data *pdata, struct cyttsp4_touch_platform_info *pi)
{
	int ret = -EPERM;

	pr_info("[TPD]:%s IN\n",__func__);

	if(pi != NULL){
		pi->model_type = cyttsp4_get_model();
		pi->trial_type = cyttsp4_get_trial_type();
		pi->lcd_maker_id = cyttsp4_get_lcdid();
	}

	return ret;
}

static int cyttsp4_mt_setting(struct cyttsp4_core_platform_data *pdata, u8 setting , unsigned short flags, struct device *dev)
{

	if(setting){
		_cyttsp4_mt_platform_data.flags |= flags;
	} else {
		_cyttsp4_mt_platform_data.flags &= ~flags;
	}

	return 0;
}

/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add start */
static int cyttsp4_board_is_support(struct cyttsp4_core_platform_data *pdata, int check_type, struct device *dev)
{
	int model = 0;

	int ret = 0;		/* 0 : not support, 1 : support */

	model = cyttsp4_get_model();

	switch ( check_type ) {
	case CY_CORE_SUPPORT_PRESS:
		switch ( model ) {
		case CY_MODEL_PTN1:
		case CY_MODEL_PTN2:		/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
			ret = 1;
			break;
		default:
			break;
		}
		break;
	case CY_CORE_SUPPORT_HOVER:
		break;
	default:
		break;
	}

	return ret;
}
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add end */

static struct cyttsp4_core_platform_data _cyttsp4_core_platform_data = {
	.irq_gpio      = CYTTSP4_I2C_IRQ_GPIO,
	.rst_gpio      = CYTTSP4_I2C_RST_GPIO,
	.xres          = cyttsp4_hw_reset,
	.init          = cyttsp4_hw_init,
	.power         = cyttsp4_power,
	.irq_stat      = cyttsp4_irq_stat,
	.mt_setting    = cyttsp4_mt_setting,
	.board_is_support = cyttsp4_board_is_support,				/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add */
	.pdata_init    = cyttsp4_pdata_init,
	.get_platform_info = cyttsp4_get_platform_info,
	.sett = {
		NULL, /* Reserved */
		NULL, /* Command Registers */
		NULL, /* Touch Report */
		NULL, /* Cypress Data Record */
		NULL, /* Test Record */
		NULL, /* Panel Configuration Record */
		NULL, /* &cyttsp4_sett_param_regs, */
		NULL, /* &cyttsp4_sett_param_size, */
		NULL, /* Reserved */
		NULL, /* Reserved */
		NULL, /* Operational Configuration Record */
		NULL, /* &cyttsp4_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp4_sett_mdata, *//* Manufacturing Data Record */
		NULL, /* Config and Test Registers */
		NULL, /* button-to-keycode table */
	},
	.loader_pdata = &_cyttsp4_loader_platform_data,
	.max_xfer_len = 250,
};

static struct cyttsp4_core_info cyttsp4_core_info __initdata = {
	.name = CYTTSP4_CORE_NAME,
	.id = "main_ttsp_core",
	.adap_id = CYTTSP4_I2C_NAME,
	.platform_data = &_cyttsp4_core_platform_data,
};

static struct i2c_board_info __initdata cyttsp4_i2c_info[] = {
	{
		I2C_BOARD_INFO(CYTTSP4_I2C_NAME, CYTTSP4_I2C_TCH_ADR),
		.irq = MSM_GPIO_TO_INT(CYTTSP4_I2C_IRQ_GPIO),
		.platform_data = CYTTSP4_I2C_NAME,
	},
};

int __init f12nab_touch_init(void)
{
	int model = 0;
	int ret   = 0;

	model = cyttsp4_get_model();

	switch(model){
	case CY_MODEL_PTN1:
	case CY_MODEL_PTN2:		/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
	case CY_MODEL_PTN3:		/* FUJITSU:2013-05-28 HMW_TOUCH_SPD_MODEL add */
		if(cyttsp4_get_trial_type() >= CY_TRIAL_TYP_HMW_1_2){
			/* PANEL_QHD */
			_cyttsp4_mt_platform_data.frmwrk = &cyttsp4_framework_qhd;
		}
		break;
	default :
		break;
	}

	ret = i2c_register_board_info(2, cyttsp4_i2c_info, ARRAY_SIZE(cyttsp4_i2c_info));
	cyttsp4_register_core_device(&cyttsp4_core_info);
	cyttsp4_register_device(&cyttsp4_mt_info);

 	return ret;
}
