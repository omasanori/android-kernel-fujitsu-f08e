/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012-2013
/*----------------------------------------------------------------------------*/

#include <linux/regulator/pm8xxx-regulator.h>

#include "board-8064.h"

#define VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply vreg_consumers_##_id[]

/* Regulators that are present when using either PM8921 or PM8917 */
/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
VREG_CONSUMERS(L1) = {
	REGULATOR_SUPPLY("8921_l1",		NULL),
};
VREG_CONSUMERS(L2) = {
	REGULATOR_SUPPLY("8921_l2",		NULL),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.0"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.1"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.2"),
	REGULATOR_SUPPLY("lvds_pll_vdda",	"lvds.0"),
	REGULATOR_SUPPLY("dsi1_pll_vdda",	"mipi_dsi.1"),
	REGULATOR_SUPPLY("dsi_pll_vdda",	"mdp.0"),
/* FUJITSU:2013_01_31 add camera start */
	REGULATOR_SUPPLY("mipi_csi_vdd",	"3-001f"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"3-003c"),
/* FUJITSU:2013_01_31 add camera end */
};
VREG_CONSUMERS(L3) = {
	REGULATOR_SUPPLY("8921_l3",		NULL),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_otg"),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_ehci_host.1"),
};
VREG_CONSUMERS(L4) = {
	REGULATOR_SUPPLY("8921_l4",		NULL),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_otg"),
	REGULATOR_SUPPLY("iris_vddxo",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L5) = {
	REGULATOR_SUPPLY("8921_l5",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.1"),
};
VREG_CONSUMERS(L6) = {
	REGULATOR_SUPPLY("8921_l6",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L7) = {
	REGULATOR_SUPPLY("8921_l7",		NULL),
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L8) = {
	REGULATOR_SUPPLY("8921_l8",		NULL),
	REGULATOR_SUPPLY("cam_vana",		"4-001a"),
	REGULATOR_SUPPLY("cam_vana",		"4-0010"),
	REGULATOR_SUPPLY("cam_vana",		"4-0048"),
	REGULATOR_SUPPLY("cam_vana",		"4-006c"),
	REGULATOR_SUPPLY("cam_vana",		"4-0034"),
	REGULATOR_SUPPLY("cam_vana",		"4-0020"),
};
VREG_CONSUMERS(L9) = {
	REGULATOR_SUPPLY("8921_l9",		NULL),
	REGULATOR_SUPPLY("vdd",			"3-0024"),
};
VREG_CONSUMERS(L10) = {
	REGULATOR_SUPPLY("8921_l10",		NULL),
	REGULATOR_SUPPLY("iris_vddpa",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L11) = {
	REGULATOR_SUPPLY("8921_l11",		NULL),
	REGULATOR_SUPPLY("dsi1_avdd",		"mipi_dsi.1"),
};
VREG_CONSUMERS(L12) = {
	REGULATOR_SUPPLY("cam_vdig",		"4-001a"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0010"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0048"),
	REGULATOR_SUPPLY("cam_vdig",		"4-006c"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0034"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0020"),
	REGULATOR_SUPPLY("8921_l12",		NULL),
};
VREG_CONSUMERS(L13) = {
	REGULATOR_SUPPLY("8921_l13",		NULL),
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8921_l14",		NULL),
	REGULATOR_SUPPLY("vreg_xoadc",		"pm8921-charger"),
/* FUJITSU:2012-11-16 FUEL add start */
	REGULATOR_SUPPLY("pa_therm",		"pm8xxx-adc"),
/* FUJITSU:2012-11-16 FUEL add start */
};
VREG_CONSUMERS(L15) = {
	REGULATOR_SUPPLY("8921_l15",		NULL),
};
VREG_CONSUMERS(L16) = {
	REGULATOR_SUPPLY("8921_l16",		NULL),
	REGULATOR_SUPPLY("cam_vaf",		"4-001a"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0010"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0048"),
	REGULATOR_SUPPLY("cam_vaf",		"4-006c"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0034"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0020"),
};
VREG_CONSUMERS(L17) = {
	REGULATOR_SUPPLY("8921_l17",		NULL),
/* FUJITSU:2013-04-25 Felica Driver : add start */
	REGULATOR_SUPPLY("felica_l17",		"msm_serial_hsl.3"),
/* FUJITSU:2013-04-25 Felica Driver : add end   */
/* FUJITSU:2012-12-25 IrDA_Driver add start */
#ifdef CONFIG_IRDA_FJ
	REGULATOR_SUPPLY("irda_l17", "msm_serial_hsl.5"),
#endif /*CONFIG_IRDA_FJ*/
/* FUJITSU:2012-12-25 IrDA_Driver add end */
/* FUJITSU:2013-02-06 FingerPrintSensor_FPC1080 add-S */
	REGULATOR_SUPPLY("FingerPrint_l17", "msm_serial_hsl.5"),
/* FUJITSU:2013-02-06 FingerPrintSensor_FPC1080 add-E   */
};
VREG_CONSUMERS(L18) = {
	REGULATOR_SUPPLY("8921_l18",		NULL),
/* FUJITSU:2013_01_31 add camera start */
	REGULATOR_SUPPLY("cam_l18",		"3-003c"),
/* FUJITSU:2013_01_31 add camera end */
};
VREG_CONSUMERS(L21) = {
	REGULATOR_SUPPLY("8921_l21",		NULL),
};
VREG_CONSUMERS(L22) = {
	REGULATOR_SUPPLY("8921_l22",		NULL),
/* FUJITSU:2013_01_31 add camera start */
	REGULATOR_SUPPLY("cam_l22",		"3-003c"),
/* FUJITSU:2013_01_31 add camera end */
};
VREG_CONSUMERS(L23) = {
	REGULATOR_SUPPLY("8921_l23",		NULL),
	REGULATOR_SUPPLY("pll_vdd",		"pil_qdsp6v4.1"),
	REGULATOR_SUPPLY("pll_vdd",		"pil_qdsp6v4.2"),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_ehci_host.1"),
/* FUJITSU:2013_01_31 add camera start */
	REGULATOR_SUPPLY("cam_l23",		"3-003c"),
/* FUJITSU:2013_01_31 add camera end */
};
VREG_CONSUMERS(L24) = {
	REGULATOR_SUPPLY("8921_l24",		NULL),
	REGULATOR_SUPPLY("riva_vddmx",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L25) = {
	REGULATOR_SUPPLY("8921_l25",		NULL),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla2x-slim"),
};
VREG_CONSUMERS(L26) = {
	REGULATOR_SUPPLY("8921_l26",		NULL),
	REGULATOR_SUPPLY("core_vdd",		"pil_qdsp6v4.0"),
};
VREG_CONSUMERS(L27) = {
	REGULATOR_SUPPLY("8921_l27",		NULL),
	REGULATOR_SUPPLY("core_vdd",		"pil_qdsp6v4.2"),
};
VREG_CONSUMERS(L28) = {
	REGULATOR_SUPPLY("8921_l28",		NULL),
	REGULATOR_SUPPLY("core_vdd",		"pil_qdsp6v4.1"),
};
VREG_CONSUMERS(L29) = {
	REGULATOR_SUPPLY("8921_l29",		NULL),
};
VREG_CONSUMERS(S2) = {
	REGULATOR_SUPPLY("8921_s2",		NULL),
	REGULATOR_SUPPLY("iris_vddrfa",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(S3) = {
	REGULATOR_SUPPLY("8921_s3",		NULL),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_otg"),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_ehci_host.1"),
	REGULATOR_SUPPLY("HSIC_VDDCX",		"msm_hsic_host"),
	REGULATOR_SUPPLY("riva_vddcx",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("vp_pcie",             "msm_pcie"),
	REGULATOR_SUPPLY("vptx_pcie",           "msm_pcie"),
};
VREG_CONSUMERS(S4) = {
	REGULATOR_SUPPLY("8921_s4",		NULL),
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.1"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla2x-slim"),
	REGULATOR_SUPPLY("riva_vddpx",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("vcc_i2c",		"3-005b"),
	REGULATOR_SUPPLY("vcc_i2c",		"3-0024"),
	REGULATOR_SUPPLY("vddp",		"0-0048"),
	REGULATOR_SUPPLY("hdmi_lvl_tsl",	"hdmi_msm.0"),
/* FUJITSU:2013-01-15 Felica Driver add start */
	REGULATOR_SUPPLY("felica_s4",		"msm_serial_hsl.3"),
/* FUJITSU:2013-01-15 Felica Driver add end */
};
VREG_CONSUMERS(S5) = {
	REGULATOR_SUPPLY("8921_s5",		NULL),
	REGULATOR_SUPPLY("krait0",		"acpuclk-8064"),
};
VREG_CONSUMERS(S6) = {
	REGULATOR_SUPPLY("8921_s6",		NULL),
	REGULATOR_SUPPLY("krait1",		"acpuclk-8064"),
};
VREG_CONSUMERS(S7) = {
	REGULATOR_SUPPLY("8921_s7",		NULL),
};
VREG_CONSUMERS(S8) = {
	REGULATOR_SUPPLY("8921_s8",		NULL),
/* FUJITSU:2013_01_31 add camera start */
	REGULATOR_SUPPLY("cam_105v_ocam_a_ko",	"3-001f"),
/* FUJITSU:2013_01_31 add camera end */
};
VREG_CONSUMERS(LVS1) = {
	REGULATOR_SUPPLY("8921_lvs1",		NULL),
	REGULATOR_SUPPLY("iris_vddio",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(LVS3) = {
	REGULATOR_SUPPLY("8921_lvs3",		NULL),
};
VREG_CONSUMERS(LVS4) = {
	REGULATOR_SUPPLY("8921_lvs4",		NULL),
};
VREG_CONSUMERS(LVS5) = {
	REGULATOR_SUPPLY("8921_lvs5",		NULL),
	REGULATOR_SUPPLY("cam_vio",		"4-001a"),
	REGULATOR_SUPPLY("cam_vio",		"4-0010"),
	REGULATOR_SUPPLY("cam_vio",		"4-0048"),
	REGULATOR_SUPPLY("cam_vio",		"4-006c"),
	REGULATOR_SUPPLY("cam_vio",		"4-0034"),
	REGULATOR_SUPPLY("cam_vio",		"4-0020"),
};
VREG_CONSUMERS(LVS6) = {
	REGULATOR_SUPPLY("8921_lvs6",		NULL),
	REGULATOR_SUPPLY("vdd_pcie_vph",        "msm_pcie"),
};
VREG_CONSUMERS(LVS7) = {
	REGULATOR_SUPPLY("8921_lvs7",		NULL),
	REGULATOR_SUPPLY("pll_vdd",		"pil_riva"),
	REGULATOR_SUPPLY("lvds_vdda",		"lvds.0"),
	REGULATOR_SUPPLY("dsi1_vddio",		"mipi_dsi.1"),
	REGULATOR_SUPPLY("dsi_pll_vddio",	"mdp.0"),
	REGULATOR_SUPPLY("hdmi_vdda",		"hdmi_msm.0"),
};
VREG_CONSUMERS(USB_OTG) = {
	REGULATOR_SUPPLY("8921_usb_otg",	NULL),
/* FUJITSU:2012-11-16 USB mod start */
#if 0
	REGULATOR_SUPPLY("vbus_otg",		"msm_otg"),
#else
	REGULATOR_SUPPLY("vbus_otg",		NULL),
#endif
/* FUJITSU:2012-11-16 USB mod end */
};
VREG_CONSUMERS(8821_S0) = {
	REGULATOR_SUPPLY("8821_s0",		NULL),
	REGULATOR_SUPPLY("krait2",		"acpuclk-8064"),
};
VREG_CONSUMERS(8821_S1) = {
	REGULATOR_SUPPLY("8821_s1",		NULL),
	REGULATOR_SUPPLY("krait3",		"acpuclk-8064"),
};
VREG_CONSUMERS(EXT_MPP8) = {
	REGULATOR_SUPPLY("ext_mpp8",		NULL),
	REGULATOR_SUPPLY("vbus",		"msm_ehci_host.1"),
};
VREG_CONSUMERS(EXT_3P3V) = {
	REGULATOR_SUPPLY("ext_3p3v",		NULL),
	REGULATOR_SUPPLY("vdd_io",		"spi0.2"),
	REGULATOR_SUPPLY("mhl_usb_hs_switch",	"msm_otg"),
	REGULATOR_SUPPLY("lvds_vccs_3p3v",      "lvds.0"),
	REGULATOR_SUPPLY("dsi1_vccs_3p3v",      "mipi_dsi.1"),
	REGULATOR_SUPPLY("hdmi_mux_vdd",        "hdmi_msm.0"),
	REGULATOR_SUPPLY("pcie_ext_3p3v",       "msm_pcie"),
};
VREG_CONSUMERS(EXT_TS_SW) = {
	REGULATOR_SUPPLY("ext_ts_sw",		NULL),
	REGULATOR_SUPPLY("vdd_ana",		"3-005b"),
};
VREG_CONSUMERS(AVC_1P2V) = {
	REGULATOR_SUPPLY("avc_1p2v",	NULL),
};
VREG_CONSUMERS(AVC_1P8V) = {
	REGULATOR_SUPPLY("avc_1p8v",	NULL),
};
VREG_CONSUMERS(AVC_2P2V) = {
	REGULATOR_SUPPLY("avc_2p2v",	NULL),
};
VREG_CONSUMERS(AVC_5V) = {
	REGULATOR_SUPPLY("avc_5v",	NULL),
};
VREG_CONSUMERS(AVC_3P3V) = {
	REGULATOR_SUPPLY("avc_3p3v",	NULL),
};
/* FUJITSU:2013-01-23 test add start */
VREG_CONSUMERS(L1_TEST) = {
	REGULATOR_SUPPLY("test_8921_l1",		NULL),
};
VREG_CONSUMERS(L2_TEST) = {
	REGULATOR_SUPPLY("test_8921_l2",		NULL),
};
VREG_CONSUMERS(L3_TEST) = {
	REGULATOR_SUPPLY("test_8921_l3",		NULL),
};
VREG_CONSUMERS(L4_TEST) = {
	REGULATOR_SUPPLY("test_8921_l4",		NULL),
};
VREG_CONSUMERS(L5_TEST) = {
	REGULATOR_SUPPLY("test_8921_l5",		NULL),
};
VREG_CONSUMERS(L6_TEST) = {
	REGULATOR_SUPPLY("test_8921_l6",		NULL),
};
VREG_CONSUMERS(L7_TEST) = {
	REGULATOR_SUPPLY("test_8921_l7",		NULL),
};
VREG_CONSUMERS(L8_TEST) = {
	REGULATOR_SUPPLY("test_8921_l8",		NULL),
};
VREG_CONSUMERS(L9_TEST) = {
	REGULATOR_SUPPLY("test_8921_l9",		NULL),
};
VREG_CONSUMERS(L10_TEST) = {
	REGULATOR_SUPPLY("test_8921_l10",		NULL),
};
VREG_CONSUMERS(L11_TEST) = {
	REGULATOR_SUPPLY("test_8921_l11",		NULL),
};
VREG_CONSUMERS(L12_TEST) = {
	REGULATOR_SUPPLY("test_8921_l12",		NULL),
};
VREG_CONSUMERS(L14_TEST) = {
	REGULATOR_SUPPLY("test_8921_l14",		NULL),
};
VREG_CONSUMERS(L15_TEST) = {
	REGULATOR_SUPPLY("test_8921_l15",		NULL),
};
VREG_CONSUMERS(L16_TEST) = {
	REGULATOR_SUPPLY("test_8921_l16",		NULL),
};
VREG_CONSUMERS(L17_TEST) = {
	REGULATOR_SUPPLY("test_8921_l17",		NULL),
};
VREG_CONSUMERS(L18_TEST) = {
	REGULATOR_SUPPLY("test_8921_l18",		NULL),
};
VREG_CONSUMERS(L21_TEST) = {
	REGULATOR_SUPPLY("test_8921_l21",		NULL),
};
VREG_CONSUMERS(L22_TEST) = {
	REGULATOR_SUPPLY("test_8921_l22",		NULL),
};
VREG_CONSUMERS(L23_TEST) = {
	REGULATOR_SUPPLY("test_8921_l23",		NULL),
};
VREG_CONSUMERS(L24_TEST) = {
	REGULATOR_SUPPLY("test_8921_l24",		NULL),
};
VREG_CONSUMERS(L25_TEST) = {
	REGULATOR_SUPPLY("test_8921_l25",		NULL),
};
VREG_CONSUMERS(L26_TEST) = {
	REGULATOR_SUPPLY("test_8921_l26",		NULL),
};
VREG_CONSUMERS(L27_TEST) = {
	REGULATOR_SUPPLY("test_8921_l27",		NULL),
};
VREG_CONSUMERS(L28_TEST) = {
	REGULATOR_SUPPLY("test_8921_l28",		NULL),
};
VREG_CONSUMERS(L29_TEST) = {
	REGULATOR_SUPPLY("test_8921_l29",		NULL),
};
VREG_CONSUMERS(S1_TEST) = {
	REGULATOR_SUPPLY("test_8921_s1",		NULL),
};
VREG_CONSUMERS(S2_TEST) = {
	REGULATOR_SUPPLY("test_8921_s2",		NULL),
};
VREG_CONSUMERS(S3_TEST) = {
	REGULATOR_SUPPLY("test_8921_s3",		NULL),
};
VREG_CONSUMERS(S4_TEST) = {
	REGULATOR_SUPPLY("test_8921_s4",		NULL),
};
VREG_CONSUMERS(S5_TEST) = {
	REGULATOR_SUPPLY("test_8921_s5",		NULL),
};
VREG_CONSUMERS(S6_TEST) = {
	REGULATOR_SUPPLY("test_8921_s6",		NULL),
};
VREG_CONSUMERS(S7_TEST) = {
	REGULATOR_SUPPLY("test_8921_s7",		NULL),
};
VREG_CONSUMERS(S8_TEST) = {
	REGULATOR_SUPPLY("test_8921_s8",		NULL),
};
VREG_CONSUMERS(LVS1_TEST) = {
	REGULATOR_SUPPLY("test_8921_lvs1",		NULL),
};
VREG_CONSUMERS(LVS2_TEST) = {
	REGULATOR_SUPPLY("test_8921_lvs2",		NULL),
};
VREG_CONSUMERS(LVS3_TEST) = {
	REGULATOR_SUPPLY("test_8921_lvs3",		NULL),
};
VREG_CONSUMERS(LVS4_TEST) = {
	REGULATOR_SUPPLY("test_8921_lvs4",		NULL),
};
VREG_CONSUMERS(LVS5_TEST) = {
	REGULATOR_SUPPLY("test_8921_lvs5",		NULL),
};
VREG_CONSUMERS(LVS6_TEST) = {
	REGULATOR_SUPPLY("test_8921_lvs6",		NULL),
};
VREG_CONSUMERS(LVS7_TEST) = {
	REGULATOR_SUPPLY("test_8921_lvs7",		NULL),
};

VREG_CONSUMERS(USB_OTG_TEST) = {
	REGULATOR_SUPPLY("test_8921_usb_otg",	NULL),
};
VREG_CONSUMERS(HDMI_MVS_TEST) = {
	REGULATOR_SUPPLY("test_8921_hdmi_mvs",	NULL),
};
/* FUJITSU:2013-01-23 test add end */

/* Regulators that are only present when using PM8921 */
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8921_s1",		NULL),
};
VREG_CONSUMERS(LVS2) = {
	REGULATOR_SUPPLY("8921_lvs2",		NULL),
	REGULATOR_SUPPLY("iris_vdddig",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(HDMI_MVS) = {
	REGULATOR_SUPPLY("8921_hdmi_mvs",	NULL),
	REGULATOR_SUPPLY("hdmi_mvs",		"hdmi_msm.0"),
};
VREG_CONSUMERS(NCP) = {
	REGULATOR_SUPPLY("8921_ncp",		NULL),
};
/* FUJITSU:2012-11-16 USB del start */
#if 0
VREG_CONSUMERS(EXT_5V) = {
	REGULATOR_SUPPLY("ext_5v",		NULL),
	REGULATOR_SUPPLY("vbus",		"msm_ehci_host.0"),
};
#endif
/* FUJITSU:2012-11-16 USB del end */
/* Regulators that are only present when using PM8917 */
VREG_CONSUMERS(8917_S1) = {
	REGULATOR_SUPPLY("8921_s1",		NULL),
	REGULATOR_SUPPLY("iris_vdddig",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L30) = {
	REGULATOR_SUPPLY("8917_l30",		NULL),
};
VREG_CONSUMERS(L31) = {
	REGULATOR_SUPPLY("8917_l31",		NULL),
};
VREG_CONSUMERS(L32) = {
	REGULATOR_SUPPLY("8917_l32",		NULL),
};
VREG_CONSUMERS(L33) = {
	REGULATOR_SUPPLY("8917_l33",		NULL),
};
VREG_CONSUMERS(L34) = {
	REGULATOR_SUPPLY("8917_l34",		NULL),
};
VREG_CONSUMERS(L35) = {
	REGULATOR_SUPPLY("8917_l35",		NULL),
};
VREG_CONSUMERS(L36) = {
	REGULATOR_SUPPLY("8917_l36",		NULL),
};
VREG_CONSUMERS(BOOST) = {
	REGULATOR_SUPPLY("8917_boost",		NULL),
	REGULATOR_SUPPLY("vbus",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("hdmi_mvs",		"hdmi_msm.0"),
};

#define PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, _modes, _ops, \
			 _apply_uV, _pull_down, _always_on, _supply_regulator, \
			 _system_uA, _enable_time, _reg_id) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _max_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
				.name			= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= _reg_id, \
		.pull_down_enable	= _pull_down, \
		.system_uA		= _system_uA, \
		.enable_time		= _enable_time, \
	}

#define PM8XXX_LDO(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_NLDO1200(_id, _name, _always_on, _pull_down, _min_uV, \
		_max_uV, _enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_SMPS(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_FTSMPS(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS \
		| REGULATOR_CHANGE_MODE, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_VS(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

#define PM8XXX_VS300(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

#define PM8XXX_NCP(_id, _name, _always_on, _min_uV, _max_uV, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, 0, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0, 0, \
		_always_on, _supply_regulator, 0, _enable_time, _reg_id)

#define PM8XXX_BOOST(_id, _name, _always_on, _min_uV, _max_uV, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, 0, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0, 0, \
		_always_on, _supply_regulator, 0, _enable_time, _reg_id)

/* Pin control initialization */
#define PM8XXX_PC(_id, _name, _always_on, _pin_fn, _pin_ctrl, \
		  _supply_regulator, _reg_id) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
				.name		= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator  = _supply_regulator, \
		}, \
		.id		= _reg_id, \
		.pin_fn		= PM8XXX_VREG_PIN_FN_##_pin_fn, \
		.pin_ctrl	= _pin_ctrl, \
	}

#define GPIO_VREG(_id, _reg_name, _gpio_label, _gpio, _supply_regulator) \
	[GPIO_VREG_ID_##_id] = { \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.regulator_name = _reg_name, \
		.gpio_label	= _gpio_label, \
		.gpio		= _gpio, \
	}

#define SAW_VREG_INIT(_id, _name, _min_uV, _max_uV) \
	{ \
		.constraints = { \
			.name		= _name, \
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE, \
			.min_uV		= _min_uV, \
			.max_uV		= _max_uV, \
		}, \
		.num_consumer_supplies	= ARRAY_SIZE(vreg_consumers_##_id), \
		.consumer_supplies	= vreg_consumers_##_id, \
	}

#define RPM_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, _default_uV, \
		 _peak_uA, _avg_uA, _pull_down, _pin_ctrl, _freq, _pin_fn, \
		 _force_mode, _sleep_set_force_mode, _power_mode, _state, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _min_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= RPM_VREG_ID_PM8921_##_id, \
		.default_uV		= _default_uV, \
		.peak_uA		= _peak_uA, \
		.avg_uA			= _avg_uA, \
		.pull_down_enable	= _pull_down, \
		.pin_ctrl		= _pin_ctrl, \
		.freq			= RPM_VREG_FREQ_##_freq, \
		.pin_fn			= _pin_fn, \
		.force_mode		= _force_mode, \
		.sleep_set_force_mode	= _sleep_set_force_mode, \
		.power_mode		= _power_mode, \
		.state			= _state, \
		.sleep_selectable	= _sleep_selectable, \
		.system_uA		= _system_uA, \
	}

#define RPM_LDO(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator, _system_uA, _init_peak_uA) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _init_peak_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, RPM_VREG_POWER_MODE_8960_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, _system_uA)

#define RPM_SMPS(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		 _supply_regulator, _system_uA, _freq, _force_mode, \
		 _sleep_set_force_mode) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _system_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_##_force_mode, \
		 RPM_VREG_FORCE_MODE_8960_##_sleep_set_force_mode, \
		 RPM_VREG_POWER_MODE_8960_PWM, RPM_VREG_STATE_OFF, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA)

#define RPM_VS(_id, _always_on, _pd, _sleep_selectable, _supply_regulator) \
	RPM_INIT(_id, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, 0, 1000, 1000, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, RPM_VREG_POWER_MODE_8960_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

#define RPM_NCP(_id, _always_on, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator, _freq) \
	RPM_INIT(_id, _min_uV, _max_uV, 0, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS, 0, _max_uV, 1000, 1000, 0, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, \
		 RPM_VREG_FORCE_MODE_8960_NONE, RPM_VREG_POWER_MODE_8960_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

/* Pin control initialization */
#define RPM_PC_INIT(_id, _always_on, _pin_fn, _pin_ctrl, _supply_regulator) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id	  = RPM_VREG_ID_PM8921_##_id##_PC, \
		.pin_fn	  = RPM_VREG_PIN_FN_8960_##_pin_fn, \
		.pin_ctrl = _pin_ctrl, \
	}

/* GPIO regulator constraints */
struct gpio_regulator_platform_data
apq8064_gpio_regulator_pdata[] __devinitdata = {
	/*        ID      vreg_name gpio_label   gpio                  supply */
/* FUJITSU:2012-11-16 USB del start */
#if 0
	GPIO_VREG(EXT_5V, "ext_5v", "ext_5v_en", PM8921_MPP_PM_TO_SYS(7), NULL),
#endif
/* FUJITSU:2012-11-16 USB del end */
/* FUJITSU:2012-11-16 mod start */
#if 0
	GPIO_VREG(EXT_3P3V, "ext_3p3v", "ext_3p3v_en",
		  APQ8064_EXT_3P3V_REG_EN_GPIO, NULL),
#else
	GPIO_VREG(EXT_3P3V, "ext_3p3v", "ext_3p3v_en",
		  PM8921_MPP_PM_TO_SYS(12), NULL),
#endif
/* FUJITSU:2012-11-16 mod end */
	GPIO_VREG(EXT_TS_SW, "ext_ts_sw", "ext_ts_sw_en",
		  PM8921_GPIO_PM_TO_SYS(23), "ext_3p3v"),
	GPIO_VREG(EXT_MPP8, "ext_mpp8", "ext_mpp8_en",
			PM8921_MPP_PM_TO_SYS(8), NULL),
};

struct gpio_regulator_platform_data
mpq8064_gpio_regulator_pdata[] __devinitdata = {
	GPIO_VREG(AVC_1P2V, "avc_1p2v", "avc_1p2v_en", SX150X_GPIO(4, 2), NULL),
	GPIO_VREG(AVC_1P8V, "avc_1p8v", "avc_1p8v_en", SX150X_GPIO(4, 4), NULL),
	GPIO_VREG(AVC_2P2V, "avc_2p2v", "avc_2p2v_en",
						 SX150X_GPIO(4, 14), NULL),
	GPIO_VREG(AVC_5V, "avc_5v", "avc_5v_en", SX150X_GPIO(4, 3), NULL),
	GPIO_VREG(AVC_3P3V, "avc_3p3v", "avc_3p3v_en",
					SX150X_GPIO(4, 15), "avc_5v"),
};

/* SAW regulator constraints */
struct regulator_init_data msm8064_saw_regulator_pdata_8921_s5 =
	/*	      ID  vreg_name	       min_uV   max_uV */
	SAW_VREG_INIT(S5, "8921_s5",	       850000, 1300000);
struct regulator_init_data msm8064_saw_regulator_pdata_8921_s6 =
	SAW_VREG_INIT(S6, "8921_s6",	       850000, 1300000);

struct regulator_init_data msm8064_saw_regulator_pdata_8821_s0 =
	/*	      ID       vreg_name	min_uV  max_uV */
	SAW_VREG_INIT(8821_S0, "8821_s0",       850000, 1300000);
struct regulator_init_data msm8064_saw_regulator_pdata_8821_s1 =
	SAW_VREG_INIT(8821_S1, "8821_s1",       850000, 1300000);

/* PM8921 regulator constraints */
struct pm8xxx_regulator_platform_data
msm8064_pm8921_regulator_pdata[] __devinitdata = {
	/*
	 *		ID   name always_on pd min_uV   max_uV   en_t supply
	 *	system_uA reg_ID
	 */
	PM8XXX_NLDO1200(L26, "8921_l26", 0, 1, 375000, 1050000, 200, "8921_s7",
		0, 1),

	/*           ID        name     always_on pd       en_t supply reg_ID */
/* FUJITSU:2012-11-16 USB mod start */
#if 0
	PM8XXX_VS300(USB_OTG,  "8921_usb_otg",  0, 0,         0, "ext_5v", 2),
	PM8XXX_VS300(HDMI_MVS, "8921_hdmi_mvs", 0, 1,         0, "ext_5v", 3),
#else
	PM8XXX_VS300(USB_OTG,  "8921_usb_otg",  0, 1,         0, NULL, 2),
	PM8XXX_VS300(HDMI_MVS, "8921_hdmi_mvs", 0, 1,         0, NULL, 3),
#endif
/* FUJITSU:2012-11-16 USB mod end */
/* FUJITSU:2013-01-23 test add start */
	/* ID, name, always_on, pull_down, min_uV, max_uV, en_t, supply, sys_uA, reg_ID */
	PM8XXX_SMPS(S1_TEST, "test_8921_s1", 0, 1, 375000, 3050000, 200, NULL, 100000, 4),
	PM8XXX_SMPS(S2_TEST, "test_8921_s2", 0, 1, 375000, 3050000, 200, NULL, 0, 5),
	PM8XXX_SMPS(S3_TEST, "test_8921_s3", 0, 1, 375000, 3050000, 200, NULL, 100000, 6),
	PM8XXX_SMPS(S4_TEST, "test_8921_s4", 0, 1, 375000, 3050000, 200, NULL, 100000, 7),
	PM8XXX_FTSMPS(S5_TEST,"test_8921_s5", 0, 1, 375000, 3300000, 200, NULL, 0, 8),
	PM8XXX_FTSMPS(S6_TEST,"test_8921_s6", 0, 1, 375000, 3300000, 200, NULL, 0, 9),
	PM8XXX_SMPS(S7_TEST,   "test_8921_s7", 0, 0, 375000, 3050000, 200, NULL, 100000, 10),
	PM8XXX_SMPS(S8_TEST, "test_8921_s8", 0, 1,  375000, 3050000, 200, NULL, 0, 11),

	/* ID, name, always_on, pull_down, min_uV, max_uV, en_t, supply, sys_uA, reg_ID */
	PM8XXX_LDO(L1_TEST, "test_8921_l1", 0, 1, 750000, 1525000, 200, NULL, 0, 12),
	PM8XXX_LDO(L2_TEST, "test_8921_l2", 0, 1, 750000, 1525000, 200, NULL, 0, 13),
	PM8XXX_LDO(L3_TEST, "test_8921_l3", 0, 1, 750000, 4900000, 200, NULL, 0, 14),
	PM8XXX_LDO(L4_TEST, "test_8921_l4", 0, 1, 750000, 4900000, 200, NULL, 10000, 15),
	PM8XXX_LDO(L5_TEST, "test_8921_l5", 0, 1, 750000, 4900000, 200, NULL, 0, 16),
	PM8XXX_LDO(L6_TEST, "test_8921_l6", 0, 1, 750000, 4900000, 200, NULL, 0, 17),
	PM8XXX_LDO(L7_TEST, "test_8921_l7", 0, 1, 750000, 4900000, 200, NULL, 0, 18),
	PM8XXX_LDO(L8_TEST, "test_8921_l8", 0, 1, 750000, 4900000, 200, NULL, 0, 19),
	PM8XXX_LDO(L9_TEST, "test_8921_l9", 0, 1, 750000, 4900000, 200, NULL, 0, 20),
	PM8XXX_LDO(L10_TEST, "test_8921_l10", 0, 1, 750000, 4900000, 200, NULL, 0, 21),
	PM8XXX_LDO(L11_TEST, "test_8921_l11", 0, 1, 750000, 4900000, 200, NULL, 0, 22),
	PM8XXX_LDO(L12_TEST, "test_8921_l12", 0, 1, 750000, 1525000, 200, NULL, 0, 23),
	PM8XXX_LDO(L14_TEST, "test_8921_l14", 0, 1, 750000, 4900000, 200, NULL, 0, 24),
	PM8XXX_LDO(L15_TEST, "test_8921_l15", 0, 1, 750000, 4900000, 200, NULL, 0, 25),
	PM8XXX_LDO(L16_TEST, "test_8921_l16", 0, 1, 750000, 4900000, 200, NULL, 0, 26),
	PM8XXX_LDO(L17_TEST, "test_8921_l17", 0, 1, 750000, 4900000, 200, NULL, 0, 27),
	PM8XXX_LDO(L18_TEST, "test_8921_l18", 0, 1, 750000, 1525000, 200, NULL, 0, 28),
	PM8XXX_LDO(L21_TEST, "test_8921_l21", 0, 1, 750000, 4900000, 200, NULL, 0, 29),
	PM8XXX_LDO(L22_TEST, "test_8921_l22", 0, 1, 750000, 4900000, 200, NULL, 0, 30),
	PM8XXX_LDO(L23_TEST, "test_8921_l23", 0, 1, 750000, 4900000, 200, NULL, 0, 31),
	PM8XXX_NLDO1200(L24_TEST, "test_8921_l24", 0, 1, 750000, 1525000, 200,  NULL, 10000, 32),
	PM8XXX_NLDO1200(L25_TEST, "test_8921_l25", 0, 1, 750000, 1525000, 200, NULL, 10000, 33),
	PM8XXX_NLDO1200(L26_TEST, "test_8921_l26", 0, 1, 750000, 1525000, 200, NULL, 0, 34),
	PM8XXX_NLDO1200(L27_TEST, "test_8921_l27", 0, 0, 750000, 1525000, 200, NULL, 0, 35),
	PM8XXX_NLDO1200(L28_TEST, "test_8921_l28", 0, 1, 750000, 1525000, 200, NULL, 0, 36),
	PM8XXX_LDO(L29_TEST, "test_8921_l29", 0, 1, 750000, 4900000, 200, NULL, 0, 37),

	/* ID, name, always_on, pull_down, en_t, supply, reg_ID */
	PM8XXX_VS(LVS1_TEST, "test_8921_lvs1", 0, 1, 200, NULL, 38),
	PM8XXX_VS300(LVS2_TEST, "test_8921_lvs2", 0, 1, 200, NULL, 39),
	PM8XXX_VS(LVS3_TEST, "test_8921_lvs3", 0, 1, 200, NULL, 40),
	PM8XXX_VS(LVS4_TEST, "test_8921_lvs4", 0, 1, 200, NULL, 41),
	PM8XXX_VS(LVS5_TEST, "test_8921_lvs5", 0, 1, 200, NULL, 42),
	PM8XXX_VS(LVS6_TEST, "test_8921_lvs6", 0, 1, 200, NULL, 43),
	PM8XXX_VS(LVS7_TEST, "test_8921_lvs7", 0, 1, 200, NULL, 44),
	PM8XXX_VS300(USB_OTG_TEST, "test_8921_usb_otg",  0, 0, 0, NULL, 45),
	PM8XXX_VS300(HDMI_MVS_TEST, "test_8921_hdmi_mvs", 0, 1, 0, NULL, 46),
/* FUJITSU:2012-01-23 test add end */
};

/* PM8917 regulator constraints */
struct pm8xxx_regulator_platform_data
msm8064_pm8917_regulator_pdata[] __devinitdata = {
	/*
	 *		ID   name always_on pd min_uV   max_uV   en_t supply
	 *	system_uA reg_ID
	 */
	PM8XXX_NLDO1200(L26, "8921_l26", 0, 1, 375000, 1050000, 200, "8921_s7",
		0, 1),
	PM8XXX_LDO(L30,      "8917_l30", 0, 1, 1800000, 1800000, 200, NULL,
		0, 2),
	PM8XXX_LDO(L31,      "8917_l31", 0, 1, 1800000, 1800000, 200, NULL,
		0, 3),
	PM8XXX_LDO(L32,      "8917_l32", 0, 1, 2800000, 2800000, 200, NULL,
		0, 4),
	PM8XXX_LDO(L33,      "8917_l33", 0, 1, 2800000, 2800000, 200, NULL,
		0, 5),
	PM8XXX_LDO(L34,      "8917_l34", 0, 1, 1800000, 1800000, 200, NULL,
		0, 6),
	PM8XXX_LDO(L35,      "8917_l35", 0, 1, 3000000, 3000000, 200, NULL,
		0, 7),
	PM8XXX_LDO(L36,      "8917_l36", 0, 1, 1800000, 1800000, 200, NULL,
		0, 8),

	/*
	 *           ID     name   always_on  min_uV   max_uV en_t supply reg_ID
	 */
	PM8XXX_BOOST(BOOST, "8917_boost", 0,  5000000, 5000000, 500, NULL, 9),

	/*	     ID        name      always_on pd en_t supply    reg_ID */
	PM8XXX_VS300(USB_OTG,  "8921_usb_otg",  0, 1, 0,   "8917_boost", 10),
};

static struct rpm_regulator_init_data
apq8064_rpm_regulator_init_data[] __devinitdata = {
	/*	ID a_on pd ss min_uV   max_uV  supply sys_uA  freq  fm  ss_fm */
	RPM_SMPS(S1, 1, 1, 0, 1225000, 1225000, NULL, 100000, 3p20, NONE, NONE),
	RPM_SMPS(S2, 0, 1, 0, 1300000, 1300000, NULL,      0, 1p60, NONE, NONE),
	RPM_SMPS(S3, 0, 1, 1,  500000, 1150000, NULL, 100000, 4p80, NONE, NONE),
	RPM_SMPS(S4, 1, 1, 0, 1800000, 1800000, NULL, 100000, 1p60, HPM, AUTO),	/* FUJITSU:2013-03-07 mod */
	RPM_SMPS(S7, 0, 0, 0, 1100000, 1150000, NULL, 100000, 3p20, NONE, NONE),	/* FUJITSU:2013-03-05 mod */
/* FUJITSU:2013_1_10 add camera start */
#ifdef CONFIG_M9MO_IU081F2F
	RPM_SMPS(S8, 0, 1, 0, 1200000, 1200000, NULL, 500000, 1p60, NONE, NONE),
#else
	RPM_SMPS(S8, 0, 1, 0, 1050000, 1050000, NULL, 500000, 1p60, NONE, NONE),
#endif /*CONFIG_M9MO_IU081F2F*/
//	RPM_SMPS(S8, 0, 1, 0, 1050000, 2200000, NULL,      0, 1p60, NONE, NONE),	/* FUJITSU:2013-03-05 mod */
/* FUJITSU:2013_1_10 add camera end */

	/*	ID a_on pd ss min_uV   max_uV   supply    sys_uA init_ip */
	RPM_LDO(L1,  1, 1, 0, 1050000, 1100000, "8921_s4",     0,  1000),	/* FUJITSU:2013-03-05 mod */
	RPM_LDO(L2,  0, 1, 0, 1200000, 1200000, "8921_s4",     0,     0),
	RPM_LDO(L3,  0, 1, 0, 3075000, 3300000, NULL,          0,     0),	/* FUJITSU:2013-03-05 mod */
	RPM_LDO(L4,  1, 1, 0, 1800000, 1800000, NULL,          0, 10000),
	RPM_LDO(L5,  0, 1, 0, 2950000, 2950000, NULL,          0,     0),
	RPM_LDO(L6,  0, 1, 0, 2950000, 2950000, NULL,          0,     0),
	RPM_LDO(L7,  0, 1, 0, 1800000, 2950000, NULL,          0,     0),	/* FUJITSU:2012-11-16 mod */
	RPM_LDO(L8,  0, 1, 0, 1800000, 2800000, NULL,          0,     0),	/* FUJITSU:2013-03-05 mod */
	RPM_LDO(L9,  0, 1, 0, 1800000, 1800000, NULL,          0,     0),   /* FUJITSU AUDIO: 2012-12-20 mod 3000000 -> 1800000 */
	RPM_LDO(L10, 0, 1, 0, 2900000, 3300000, NULL,          0,     0),	/* FUJITSU:2012-12-07 mod */
	RPM_LDO(L11, 0, 1, 0, 2800000, 2900000, "8921_s4",     0,     0),	/* FUJITSU:2013-03-05 mod */
	RPM_LDO(L12, 0, 1, 0, 1200000, 1200000, "8921_s4",     0,     0),
	RPM_LDO(L13, 0, 0, 0, 2220000, 2220000, NULL,          0,     0),
	RPM_LDO(L14, 0, 1, 0, 1800000, 1800000, NULL,          0,     0),
	RPM_LDO(L15, 0, 1, 0, 1800000, 3000000, NULL,          0,     0),	/* FUJITSU:2013-03-05 mod */
	RPM_LDO(L16, 0, 1, 0, 2800000, 2800000, NULL,          0,     0),
/* FUJITSU:2012-12-25 IrDA_Driver mod start */
#ifdef CONFIG_IRDA_FJ
	RPM_LDO(L17, 0, 1, 0, 2850000, 2850000, NULL,          0,     0),
#else
	RPM_LDO(L17, 0, 1, 0, 2000000, 3000000, NULL,          0,     0),	/* FUJITSU:2013-03-05 mod */
#endif /*CONFIG_IRDA_FJ*/
/* FUJITSU:2012-12-25 IrDA_Driver mod end */
	RPM_LDO(L18, 0, 1, 0, 1200000, 1300000, "8921_s4",     0,     0),	/* FUJITSU:2012-12-07 mod */
	RPM_LDO(L21, 0, 1, 0, 1800000, 1900000, "8921_s4",     0,     0),	/* FUJITSU:2012-12-07 mod */
	RPM_LDO(L22, 0, 1, 0, 2600000, 2800000, NULL,          0,     0),	/* FUJITSU:2012-12-07 mod */
	RPM_LDO(L23, 0, 1, 0, 1800000, 1800000, NULL,          0,     0),
	RPM_LDO(L24, 0, 1, 1,  750000, 1150000, "8921_s1", 10000, 10000),
	RPM_LDO(L25, 1, 1, 0, 1225000, 1225000, "8921_s1", 10000, 10000),	/* FUJITSU:2012-11-16 mod */
	RPM_LDO(L27, 0, 0, 0, 1050000, 1050000, "8921_s7",     0,     0),	/* FUJITSU:2012-11-16 mod */
	RPM_LDO(L28, 0, 1, 0, 1050000, 1050000, "8921_s7",     0,     0),
	RPM_LDO(L29, 0, 1, 0, 1800000, 2050000, NULL,          0,     0),	/* FUJITSU:2013-03-07 mod */

	/*     ID  a_on pd ss                   supply */
	RPM_VS(LVS1, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS3, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS4, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS5, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS6, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS7, 0, 1, 1,                   "8921_s4"),
};

static struct rpm_regulator_init_data
apq8064_rpm_regulator_pm8921_init_data[] __devinitdata = {
	/*     ID  a_on pd ss                   supply */
	RPM_VS(LVS2, 0, 1, 0,                   "8921_s1"),

	/*	ID a_on    ss min_uV   max_uV   supply     freq */
	RPM_NCP(NCP, 0,    0, 1800000, 1800000, "8921_l6", 1p60),
};

int msm8064_pm8921_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm8064_pm8921_regulator_pdata);
int msm8064_pm8917_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm8064_pm8917_regulator_pdata);

#define RPM_REG_MAP(_id, _sleep_also, _voter, _supply, _dev_name) \
	{ \
		.vreg_id = RPM_VREG_ID_PM8921_##_id, \
		.sleep_also = _sleep_also, \
		.voter = _voter, \
		.supply = _supply, \
		.dev_name = _dev_name, \
	}
static struct rpm_regulator_consumer_mapping
	      msm_rpm_regulator_consumer_mapping[] __devinitdata = {
	RPM_REG_MAP(LVS7, 0, 1, "krait0_hfpll", "acpuclk-8064"),
	RPM_REG_MAP(LVS7, 0, 2, "krait1_hfpll", "acpuclk-8064"),
	RPM_REG_MAP(LVS7, 0, 4, "krait2_hfpll", "acpuclk-8064"),
	RPM_REG_MAP(LVS7, 0, 5, "krait3_hfpll", "acpuclk-8064"),
	RPM_REG_MAP(LVS7, 0, 6, "l2_hfpll",     "acpuclk-8064"),
	RPM_REG_MAP(L24,  0, 1, "krait0_mem",   "acpuclk-8064"),
	RPM_REG_MAP(L24,  0, 2, "krait1_mem",   "acpuclk-8064"),
	RPM_REG_MAP(L24,  0, 4, "krait2_mem",   "acpuclk-8064"),
	RPM_REG_MAP(L24,  0, 5, "krait3_mem",   "acpuclk-8064"),
	RPM_REG_MAP(S3,   0, 1, "krait0_dig",   "acpuclk-8064"),
	RPM_REG_MAP(S3,   0, 2, "krait1_dig",   "acpuclk-8064"),
	RPM_REG_MAP(S3,   0, 4, "krait2_dig",   "acpuclk-8064"),
	RPM_REG_MAP(S3,   0, 5, "krait3_dig",   "acpuclk-8064"),
};

struct rpm_regulator_platform_data apq8064_rpm_regulator_pdata __devinitdata = {
	.init_data		  = apq8064_rpm_regulator_init_data,
	.num_regulators		  = ARRAY_SIZE(apq8064_rpm_regulator_init_data),
	.version		  = RPM_VREG_VERSION_8960,
	.vreg_id_vdd_mem	  = RPM_VREG_ID_PM8921_L24,
	.vreg_id_vdd_dig	  = RPM_VREG_ID_PM8921_S3,
	.requires_tcxo_workaround = true,
	.consumer_map		  = msm_rpm_regulator_consumer_mapping,
	.consumer_map_len = ARRAY_SIZE(msm_rpm_regulator_consumer_mapping),
};

/* Regulators that are only present when using PM8921 */
struct rpm_regulator_platform_data
apq8064_rpm_regulator_pm8921_pdata __devinitdata = {
	.init_data		  = apq8064_rpm_regulator_pm8921_init_data,
	.num_regulators	= ARRAY_SIZE(apq8064_rpm_regulator_pm8921_init_data),
	.version		  = RPM_VREG_VERSION_8960,
	.vreg_id_vdd_mem	  = RPM_VREG_ID_PM8921_L24,
	.vreg_id_vdd_dig	  = RPM_VREG_ID_PM8921_S3,
	.requires_tcxo_workaround = true,
};

/*
 * Fix up regulator consumer data that moves to a different regulator when
 * PM8917 is used.
 */
void __init configure_apq8064_pm8917_power_grid(void)
{
	static struct rpm_regulator_init_data *rpm_data;
	int i;

	for (i = 0; i < ARRAY_SIZE(apq8064_rpm_regulator_init_data); i++) {
		rpm_data = &apq8064_rpm_regulator_init_data[i];
		if (rpm_data->id == RPM_VREG_ID_PM8921_S1) {
			rpm_data->init_data.consumer_supplies
				= vreg_consumers_8917_S1;
			rpm_data->init_data.num_consumer_supplies
				= ARRAY_SIZE(vreg_consumers_8917_S1);
		}
	}

	/*
	 * Switch to 8960_PM8917 rpm-regulator version so that TCXO workaround
	 * is applied to PM8917 regulators L25, L26, L27, and L28.
	 */
	apq8064_rpm_regulator_pdata.version = RPM_VREG_VERSION_8960_PM8917;
}
