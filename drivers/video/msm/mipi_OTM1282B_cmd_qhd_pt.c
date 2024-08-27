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
#include "mipi_OTM1282B.h"

#undef BOOTSPLASH_DISABLE
static struct msm_panel_info pinfo;
static struct dsi_buf otm1282b_tx_buf;
static struct dsi_buf otm1282b_rx_buf;

static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db = {
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
0xA3, 0x31, 0xD9,
0x00, 0x50, 0x48, 0x63,
0x30, 0x07, 0x03,
0x00, 0x14, 0x03, 0x00, 0x02,
0x00, 0x20, 0x00, 0x01 },
};

typedef enum {
    LCD_INIT = 0,
    LCD_READY,
    LCD_ON,
    LCD_OFF
} LCD_STATE;

static int lcd_state = LCD_INIT;

static char exit_sleep[]  = {0x11, 0x00};
static char display_on[]  = {0x29, 0x00};

static char display_off[] = {0x28, 0x00};
static char enter_sleep[] = {0x10, 0x00};

static char cmd_Interface_setting[] = {
    0xFF, 0x12,
          0x82,
          0x01,
};

static char cmd_AdressShiftFunction80[] = {
    0x00, 0x80,
};

static char cmd_Interface_setting2[] = {
    0xFF, 0x12,
          0x82,
};

static char cmd_AdressShiftFunction00[] = {
    0x00, 0x00,
};

static char cmd_interface_setting3[] = {
    0x1C, 0x00,
};

//cmd_AdressShiftFunction00

static char cmd_CLEVER_Setting[] = {
    0x59, 0x00,
};

//cmd_AdressShiftFunction80

static char cmd_Display_setting[] = {
    0xA5, 0x0F,
          0x04,
          0x73,
};

static char cmd_AdressShiftFunction90[] = {
    0x00, 0x90,
};

static char cmd_Display_setting2[] = {
    0xA5, 0x81,
};

//cmd_AdressShiftFunction80

static char cmd_Display_setting3[] = {
    0xA6, 0x00,
          0x26,
          0x00,
};

//cmd_AdressShiftFunction90

static char cmd_interface_setting4[] = {
    0xB3, 0x65,
          0x24,
          0x70,
};

static char cmd_AdressShiftFunctionA0[] = {
    0x00, 0xA0,
};

static char cmd_Display_setting4[] = {
    0xB3, 0x41,
          0x02,
          0x1C,
          0x03,
          0xC0,
          0x91,
          0xF0,
};

//cmd_AdressShiftFunction90

static char cmd_RAM_Setting[] = {
    0xB4, 0x09,
};

//cmd_AdressShiftFunction80

static char cmd_Source_timing_setting[] = {
    0xC0, 0x01,
          0x03,
          0x00,
          0x34,
          0x04,
          0x01,
          0x03,
          0x0C,
          0x04,
          0x01,
          0x03,
          0x00,
          0x0C,
          0x04,
};

//cmd_AdressShiftFunctionA0

static char cmd_Source_timing_setting2[] = {
    0xC0, 0x00,
          0x00,
          0x00,
          0x00,
          0x05,
          0x2E,
          0x05,
};

static char cmd_AdressShiftFunctionB3[] = {
    0x00, 0xB3,
};

static char cmd_Display_setting5[] = {
    0xC0, 0x44,
};

static char cmd_AdressShiftFunctionB4[] = {
    0x00, 0xB4,
};

static char cmd_Panel_setting2[] = {
    0xC0, 0x00,
};

static char cmd_AdressShiftFunctionD0[] = {
    0x00, 0xD0,
};

static char cmd_Source_timing_setting3[] = {
    0xC0, 0x00,
          0x00,
          0x00,
          0x00,
          0x05,
          0x2E,
          0x05,
};

//cmd_AdressShiftFunction80

static char cmd_Source_timing_setting4[] = {
    0xC1, 0x77,
          0x77,
};

//cmd_AdressShiftFunction90

static char cmd_Source_timing_setting5[] = {
    0xC1, 0x77,
          0x00,
};

//cmd_AdressShiftFunctionA0

static char cmd_Panel_setting3[] = {
    0xC1, 0x00,
          0xE0,
          0x00,
};

static char cmd_AdressShiftFunctionB0[] = {
    0x00, 0xB0,
};

static char cmd_Panel_setting4[] = {
    0xC1, 0x0C,
};

//cmd_AdressShiftFunction80

static char cmd_LTPS_timing_setting[] = {
    0xC2, 0x81,
          0x01,
          0x00,
          0x00,
          0x00,
          0x00,
          0x00,
          0x00,
};

//cmd_AdressShiftFunction90

static char cmd_LTPS_timing_setting2[] = {
    0xC2, 0x81,
          0x0C,
          0x01,
          0x00,
          0x0E,
          0x82,
          0x0C,
          0x01,
          0x00,
          0x0E,
          0x01,
          0x0E,
          0x01,
          0x00,
          0x0E,
};

//cmd_AdressShiftFunctionA0

static char cmd_LTPS_timing_setting3[] = {
    0xC2, 0x80,
          0x0C,
          0x01,
          0x00,
          0x0E,
          0x81,
          0x0C,
          0x01,
          0x00,
          0x0E,
          0x82,
          0x0C,
          0x01,
          0x00,
          0x0E,
};

//cmd_AdressShiftFunctionB0

static char cmd_LTPS_timing_setting4[] = {
    0xC2, 0x01,
          0x0E,
          0x01,
          0x00,
          0x0E,
          0x80,
          0x0C,
          0x01,
          0x00,
          0x0E,
};

static char cmd_AdressShiftFunctionE0[] = {
    0x00, 0xE0,
};

static char cmd_LTPS_timing_setting5[] = {
    0xC2, 0x81,
          0x51,
          0x00,
          0x00,
          0x15,
          0x81,
          0x51,
          0x00,
          0x00,
          0x15,
          0x00,
          0x02,
};

static char cmd_AdressShiftFunctionF8[] = {
    0x00, 0xF8,
};

static char cmd_LTPS_timing_setting6[] = {
    0xC2, 0x00,
          0x80,
          0x00,
          0x00,
          0x01,
};

//cmd_AdressShiftFunction80

static char cmd_LTPS_timing_setting7[] = {
    0xC3, 0x81,
          0x01,
          0x00,
          0x00,
          0x00,
          0x00,
          0x00,
          0x00,
};

//cmd_AdressShiftFunction90

static char cmd_LTPS_timing_setting8[] = {
    0xC3, 0x81,
          0x0C,
          0x01,
          0x00,
          0x0E,
          0x82,
          0x0C,
          0x01,
          0x00,
          0x0E,
          0x01,
          0x0E,
          0x01,
          0x00,
          0x0E,
};

//cmd_AdressShiftFunctionA0

static char cmd_LTPS_timing_setting9[] = {
    0xC3, 0x80,
          0x0C,
          0x01,
          0x00,
          0x0E,
          0x81,
          0x0C,
          0x01,
          0x00,
          0x0E,
          0x82,
          0x0C,
          0x01,
          0x00,
          0x0E,
};

//cmd_AdressShiftFunctionB0

static char cmd_LTPS_timing_setting10[] = {
    0xC3, 0x01,
          0x0E,
          0x01,
          0x00,
          0x0E,
          0x80,
          0x0C,
          0x01,
          0x00,
          0x0E,
};

//cmd_AdressShiftFunctionE0

static char cmd_LTPS_timing_setting11[] = {
    0xC3, 0x81,
          0x51,
          0x00,
          0x00,
          0x15,
          0x81,
          0x51,
          0x00,
          0x00,
          0x15,
          0x00,
          0x02,
};

static char cmd_AdressShiftFunctionB5[] = {
    0x00, 0xB5,
};

static char cmd_LTPS_timing_setting12[] = {
    0xCB, 0xC0,
          0x3C,
          0xFC,
};

static char cmd_AdressShiftFunctionE5[] = {
    0x00, 0xE5,
};

static char cmd_LTPS_timing_setting13[] = {
    0xCB, 0x1F,
          0x10,
          0x10,
};

static char cmd_AdressShiftFunction9A[] = {
    0x00, 0x9A,
};

static char cmd_LTPS_timing_setting14[] = {
    0xCD, 0x0C,
          0x0D,
          0x0E,
};

//cmd_AdressShiftFunctionA0

static char cmd_LTPS_timing_setting15[] = {
    0xCD, 0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x12,
          0x2D,
          0x06,
          0x14,
          0x2D,
          0x2D,
          0x08,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
};

//cmd_AdressShiftFunctionB0

static char cmd_LTPS_timing_setting16[] = {
    0xCD, 0x2D,
          0x2D,
          0x2D,
          0x24,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
};

//cmd_AdressShiftFunctionD0

static char cmd_LTPS_timing_setting17[] = {
    0xCD, 0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x12,
          0x2D,
          0x2D,
          0x2D,
          0x03,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
};

//cmd_AdressShiftFunctionE0

static char cmd_LTPS_timing_setting18[] = {
    0xCD, 0x2D,
          0x2D,
          0x05,
          0x2D,
          0x0D,
          0x0C,
          0x0B,
          0x13,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
          0x2D,
};

//cmd_AdressShiftFunction80

static char cmd_Power_Setting[] = {
    0xC4, 0x38,
          0x88,
          0x00,
          0x00,
};

//cmd_AdressShiftFunctionA0

static char cmd_Power_Setting2[] = {
    0xC4, 0x30,
          0x26,
          0x04,
          0x3A,
          0x30,
          0x26,
          0x84,
          0x3A,
          0x01,
};

//cmd_AdressShiftFunction80

static char cmd_Power_Setting3[] = {
    0xC5, 0x04,
          0x00,
          0x10,
};

//cmd_AdressShiftFunction90

static char cmd_Power_Setting4[] = {
    0xC5, 0x00,
          0xD6,
          0x91,
          0xAC,
          0xB1,
          0xAC,
          0x44,
          0x44,
          0x40,
          0x88,
};

//cmd_AdressShiftFunctionA0

static char cmd_Power_Setting5[] = {
    0xC5, 0x00,
          0xD6,
          0x91,
          0xAC,
          0xB1,
          0xAC,
          0x44,
          0x44,
          0x40,
          0x88,
};

//cmd_AdressShiftFunctionB0

static char cmd_Power_Setting6[] = {
    0xC5, 0x00,
          0xD6,
          0x91,
          0xAC,
          0xB1,
          0xAC,
          0x44,
          0x44,
          0x40,
          0x88,
};

static char cmd_AdressShiftFunctionC0[] = {
    0x00, 0xC0,
};

static char cmd_Power_Setting7[] = {
    0xC5, 0x12,
          0xAA,
          0x00,
};

//cmd_AdressShiftFunctionB0

static char cmd_PWM_Setting[] = {
    0xCA, 0x03,
          0x03,
          0x5F,
          0x40,
};

//cmd_AdressShiftFunction00

static char cmd_Power_Setting8[] = {
    0xD8, 0x34,
          0x34,
};

//cmd_AdressShiftFunction00

static char cmd_Power_Setting9[] = {
    0xD9, 0x78,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_setting[] = {
    0xE1, 0x00,
          0x09,
          0x10,
          0x1C,
          0x27,
          0x30,
          0x3F,
          0x53,
          0x5F,
          0x6F,
          0x77,
          0x7C,
          0x81,
          0x81,
          0x83,
          0x7E,
          0x77,
          0x68,
          0x5D,
          0x5A,
          0x55,
          0x34,
          0x20,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_setting2[] = {
    0xE2, 0x00,
          0x09,
          0x10,
          0x1C,
          0x27,
          0x30,
          0x3F,
          0x53,
          0x5F,
          0x6F,
          0x77,
          0x7C,
          0x81,
          0x81,
          0x83,
          0x7E,
          0x77,
          0x68,
          0x5D,
          0x5A,
          0x55,
          0x34,
          0x20,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_setting3[] = {
    0xE3, 0x00,
          0x09,
          0x10,
          0x1C,
          0x27,
          0x30,
          0x3F,
          0x53,
          0x5F,
          0x6F,
          0x77,
          0x7C,
          0x81,
          0x81,
          0x83,
          0x7E,
          0x77,
          0x68,
          0x5D,
          0x5A,
          0x55,
          0x34,
          0x20,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_setting4[] = {
    0xE4, 0x00,
          0x09,
          0x10,
          0x1C,
          0x27,
          0x30,
          0x3F,
          0x53,
          0x5F,
          0x6F,
          0x77,
          0x7C,
          0x81,
          0x81,
          0x83,
          0x7E,
          0x77,
          0x68,
          0x5D,
          0x5A,
          0x55,
          0x34,
          0x20,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_setting5[] = {
    0xE5, 0x00,
          0x09,
          0x10,
          0x1C,
          0x27,
          0x30,
          0x3F,
          0x53,
          0x5F,
          0x6F,
          0x77,
          0x7C,
          0x81,
          0x81,
          0x83,
          0x7E,
          0x77,
          0x68,
          0x5D,
          0x5A,
          0x55,
          0x34,
          0x20,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_setting6[] = {
    0xE6, 0x00,
          0x09,
          0x10,
          0x1C,
          0x27,
          0x30,
          0x3F,
          0x53,
          0x5F,
          0x6F,
          0x77,
          0x7C,
          0x81,
          0x81,
          0x83,
          0x7E,
          0x77,
          0x68,
          0x5D,
          0x5A,
          0x55,
          0x34,
          0x20,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_setting7[] = {
    0xE7, 0x00,
          0x09,
          0x10,
          0x1C,
          0x27,
          0x30,
          0x3F,
          0x53,
          0x5F,
          0x6F,
          0x77,
          0x7C,
          0x81,
          0x81,
          0x83,
          0x7E,
          0x77,
          0x68,
          0x5D,
          0x5A,
          0x55,
          0x34,
          0x20,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_setting8[] = {
    0xE8, 0x00,
          0x09,
          0x10,
          0x1C,
          0x27,
          0x30,
          0x3F,
          0x53,
          0x5F,
          0x6F,
          0x77,
          0x7C,
          0x81,
          0x81,
          0x83,
          0x7E,
          0x77,
          0x68,
          0x5D,
          0x5A,
          0x55,
          0x34,
          0x20,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_settingC[] = {
    0xEC, 0x50,
          0x44,
          0x54,
          0x33,
          0x33,
          0x33,
          0x43,
          0x44,
          0x43,
          0x44,
          0x44,
          0x44,
          0x44,
          0x54,
          0x44,
          0x44,
          0x44,
          0x45,
          0x45,
          0x44,
          0x44,
          0x44,
          0x44,
          0x44,
          0x43,
          0x43,
          0x33,
          0x43,
          0x44,
          0x54,
          0x45,
          0x65,
          0x08,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_settingD[] = {
    0xED, 0x40,
          0x43,
          0x44,
          0x33,
          0x33,
          0x23,
          0x33,
          0x33,
          0x44,
          0x44,
          0x34,
          0x33,
          0x44,
          0x34,
          0x53,
          0x44,
          0x44,
          0x33,
          0x43,
          0x55,
          0x44,
          0x44,
          0x44,
          0x54,
          0x34,
          0x53,
          0x55,
          0x45,
          0x55,
          0x65,
          0x45,
          0x66,
          0x09,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_settingE[] = {
    0xEE, 0x30,
          0x32,
          0x34,
          0x23,
          0x22,
          0x21,
          0x22,
          0x22,
          0x23,
          0x33,
          0x32,
          0x32,
          0x22,
          0x23,
          0x32,
          0x32,
          0x32,
          0x23,
          0x33,
          0x23,
          0x33,
          0x32,
          0x23,
          0x33,
          0x22,
          0x23,
          0x23,
          0x32,
          0x32,
          0x12,
          0x22,
          0x22,
          0x02,
};

//cmd_AdressShiftFunction00

static char cmd_Gamma_settingF[] = {
    0xEF, 0x50,
          0x55,
          0x34,
          0x43,
          0x33,
          0x34,
          0x33,
          0x43,
          0x44,
          0x44,
          0x35,
          0x44,
          0x34,
          0x23,
          0x52,
          0x44,
          0x34,
          0x33,
          0x43,
          0x34,
          0x33,
          0x43,
          0x33,
          0x34,
          0x34,
          0x43,
          0x44,
          0x44,
          0x44,
          0x34,
          0x43,
          0x55,
          0x06,
};

//cmd_AdressShiftFunction00

static char cmd_WhiteMagic_settingA[] = {
    0x50, 0xFF,
};

static char cmd_AdressShiftFunction10[] = {
    0x00, 0x10,
};

static char cmd_WhiteMagic_settingB[] = {
    0xA3, 0xD3,
          0xD3,
          0xD3,
          0xD3,
          0xD3,
          0x33,
};

static char cmd_AdressShiftFunction20[] = {
    0x00, 0x20,
};

static char cmd_WhiteMagic_settingC[] = {
    0xA3, 0x32,
          0x00,
          0xE5,
          0x55,
          0x13,
          0x20,
          0x80,
          0x80,
          0x80,
          0x20,
          0x40,
          0x95,
          0x0E,
          0x10,
          0x00,
};

//cmd_AdressShiftFunction00

static char cmd_WhiteMagic_settingD[] = {
    0x50, 0x00,
};

static char cmd_WhiteMagic_setting[] = {
    0x51, 0xFF,
};

static char cmd_WhiteMagic_setting2[] = {
    0x53, 0x2C,
};

static char cmd_WhiteMagic_setting3[] = {
    0x55, 0x03,
};

static char cmd_WhiteMagic_setting4[] = {
    0x5E, 0x80,
};

//cmd_AdressShiftFunction80

static char cmd_Interface_setting3[] = {
    0xFF, 0x00,
          0x00,
};

static char cmd_Interface_setting4[] = {
    0xFF, 0x00,
          0x00,
          0x00,
};

static char cmd_Column_Address_Set[] = {
    0x2A, 0x00,
          0x00,
          0x02,
          0x1B,
};

static char cmd_Page_Address_Set[] = {
    0x2B, 0x00,
          0x00,
          0x03,
          0xBF,
};

static char cmd_TE_Settings[] = {
    0x35, 0x00,
};


static struct dsi_cmd_desc init_cmds[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Interface_setting),cmd_Interface_setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Interface_setting2),cmd_Interface_setting2 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_interface_setting3),cmd_interface_setting3 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_CLEVER_Setting),cmd_CLEVER_Setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Display_setting),cmd_Display_setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction90),cmd_AdressShiftFunction90 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Display_setting2),cmd_Display_setting2 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Display_setting3),cmd_Display_setting3 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction90),cmd_AdressShiftFunction90 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_interface_setting4),cmd_interface_setting4 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionA0),cmd_AdressShiftFunctionA0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Display_setting4),cmd_Display_setting4 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction90),cmd_AdressShiftFunction90 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_RAM_Setting),cmd_RAM_Setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Source_timing_setting),cmd_Source_timing_setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionA0),cmd_AdressShiftFunctionA0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Source_timing_setting2),cmd_Source_timing_setting2 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB3),cmd_AdressShiftFunctionB3 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Display_setting5),cmd_Display_setting5 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB4),cmd_AdressShiftFunctionB4 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Panel_setting2),cmd_Panel_setting2 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionD0),cmd_AdressShiftFunctionD0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Source_timing_setting3),cmd_Source_timing_setting3 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Source_timing_setting4),cmd_Source_timing_setting4 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction90),cmd_AdressShiftFunction90 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Source_timing_setting5),cmd_Source_timing_setting5 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionA0),cmd_AdressShiftFunctionA0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Panel_setting3),cmd_Panel_setting3 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB0),cmd_AdressShiftFunctionB0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Panel_setting4),cmd_Panel_setting4 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting),cmd_LTPS_timing_setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction90),cmd_AdressShiftFunction90 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting2),cmd_LTPS_timing_setting2 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionA0),cmd_AdressShiftFunctionA0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting3),cmd_LTPS_timing_setting3 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB0),cmd_AdressShiftFunctionB0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting4),cmd_LTPS_timing_setting4 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionE0),cmd_AdressShiftFunctionE0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting5),cmd_LTPS_timing_setting5 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionF8),cmd_AdressShiftFunctionF8 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting6),cmd_LTPS_timing_setting6 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting7),cmd_LTPS_timing_setting7 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction90),cmd_AdressShiftFunction90 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting8),cmd_LTPS_timing_setting8 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionA0),cmd_AdressShiftFunctionA0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting9),cmd_LTPS_timing_setting9 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB0),cmd_AdressShiftFunctionB0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting10),cmd_LTPS_timing_setting10 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionE0),cmd_AdressShiftFunctionE0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting11),cmd_LTPS_timing_setting11 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB5),cmd_AdressShiftFunctionB5 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting12),cmd_LTPS_timing_setting12 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionE5),cmd_AdressShiftFunctionE5 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting13),cmd_LTPS_timing_setting13 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction9A),cmd_AdressShiftFunction9A },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting14),cmd_LTPS_timing_setting14 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionA0),cmd_AdressShiftFunctionA0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting15),cmd_LTPS_timing_setting15 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB0),cmd_AdressShiftFunctionB0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting16),cmd_LTPS_timing_setting16 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionD0),cmd_AdressShiftFunctionD0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting17),cmd_LTPS_timing_setting17 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionE0),cmd_AdressShiftFunctionE0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_LTPS_timing_setting18),cmd_LTPS_timing_setting18 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting),cmd_Power_Setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionA0),cmd_AdressShiftFunctionA0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting2),cmd_Power_Setting2 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting3),cmd_Power_Setting3 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction90),cmd_AdressShiftFunction90 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting4),cmd_Power_Setting4 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionA0),cmd_AdressShiftFunctionA0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting5),cmd_Power_Setting5 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB0),cmd_AdressShiftFunctionB0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting6),cmd_Power_Setting6 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionC0),cmd_AdressShiftFunctionC0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting7),cmd_Power_Setting7 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunctionB0),cmd_AdressShiftFunctionB0 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_PWM_Setting),cmd_PWM_Setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting8),cmd_Power_Setting8 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Power_Setting9),cmd_Power_Setting9 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_setting),cmd_Gamma_setting },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_setting2),cmd_Gamma_setting2 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_setting3),cmd_Gamma_setting3 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_setting4),cmd_Gamma_setting4 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_setting5),cmd_Gamma_setting5 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_setting6),cmd_Gamma_setting6 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_setting7),cmd_Gamma_setting7 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_setting8),cmd_Gamma_setting8 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_settingC),cmd_Gamma_settingC },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_settingD),cmd_Gamma_settingD },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_settingE),cmd_Gamma_settingE },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Gamma_settingF),cmd_Gamma_settingF },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_WhiteMagic_settingA),cmd_WhiteMagic_settingA },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction10),cmd_AdressShiftFunction10 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_WhiteMagic_settingB),cmd_WhiteMagic_settingB },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction20),cmd_AdressShiftFunction20 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_WhiteMagic_settingC),cmd_WhiteMagic_settingC },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction00),cmd_AdressShiftFunction00 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_WhiteMagic_settingD),cmd_WhiteMagic_settingD },
    {DTYPE_DCS_WRITE1, 1, 0, 0,   0, sizeof(cmd_WhiteMagic_setting),cmd_WhiteMagic_setting },
    {DTYPE_DCS_WRITE1, 1, 0, 0,   0, sizeof(cmd_WhiteMagic_setting2),cmd_WhiteMagic_setting2 },
    {DTYPE_DCS_WRITE1, 1, 0, 0,   0, sizeof(cmd_WhiteMagic_setting3),cmd_WhiteMagic_setting3 },
    {DTYPE_DCS_WRITE1, 1, 0, 0,   0, sizeof(cmd_WhiteMagic_setting4),cmd_WhiteMagic_setting4 },
    {DTYPE_GEN_WRITE2, 1, 0, 0,   0, sizeof(cmd_AdressShiftFunction80),cmd_AdressShiftFunction80 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Interface_setting3),cmd_Interface_setting3 },
    {DTYPE_GEN_LWRITE, 1, 0, 0,   0, sizeof(cmd_Interface_setting4),cmd_Interface_setting4 },
    {DTYPE_DCS_LWRITE, 1, 0, 0,   0, sizeof(cmd_Column_Address_Set),cmd_Column_Address_Set },
    {DTYPE_DCS_LWRITE, 1, 0, 0,   0, sizeof(cmd_Page_Address_Set),cmd_Page_Address_Set },
    {DTYPE_DCS_WRITE1, 1, 0, 0,   0, sizeof(cmd_TE_Settings),cmd_TE_Settings },
};

static struct dsi_cmd_desc sleep_out_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(exit_sleep), exit_sleep},
};

static struct dsi_cmd_desc display_on_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc display_off_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(display_off), display_off},
};

static struct dsi_cmd_desc sleep_in_cmds[] = {
    {DTYPE_DCS_WRITE,  1, 0, 0,   0, sizeof(enter_sleep), enter_sleep},
};

/* FUJITSU:2013-06-21 LCD shutdown start*/
DEFINE_MUTEX(mipi_otm1282b_cmd_shutdown_mutex);
/* FUJITSU:2013-06-21 LCD shutdown end*/

static int mipi_otm1282b_cmd_qhd_lcd_on(struct platform_device *pdev)
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
    if(lcd_state==LCD_INIT) {
        pr_info("[LCD]%s leave on 1st_sec\n",__func__);
        lcd_state  = LCD_ON;
        return 0;
    }
#endif

    mipi  = &mfd->panel_info.mipi;
    
    
    sent_cmds = mipi_dsi_cmds_tx(&otm1282b_tx_buf,
                                 init_cmds,
                      ARRAY_SIZE(init_cmds));
    if(sent_cmds !=   ARRAY_SIZE(init_cmds) ) {
        pr_err("[LCD]%s send init_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
    
    msleep(20);
    
    sent_cmds = mipi_dsi_cmds_tx(&otm1282b_tx_buf,
                                 sleep_out_cmds,
                      ARRAY_SIZE(sleep_out_cmds));
    if(sent_cmds !=   ARRAY_SIZE(sleep_out_cmds) ) {
        pr_err("[LCD]%s send sleep_out_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
    
    lcd_state  = LCD_READY;
    pr_info("[LCD]%s leave\n",__func__);
    return 0;
}

static int mipi_otm1282b_cmd_qhd_lcd_off(struct platform_device *pdev)
{
    int sent_cmds;
    
    pr_info("[LCD]%s enter\n",__func__);
 /* FUJITSU:2013-06-21 LCD shutdown start*/
    if( lcd_state==LCD_OFF ) {
        pr_info("[LCD]%s LCD is already the off. \n",__func__);
        return 0;
    }
 /* FUJITSU:2013-06-21 LCD shutdown end*/
  
    lcd_state=LCD_OFF;
    
    sent_cmds = mipi_dsi_cmds_tx(&otm1282b_tx_buf,
                                 display_off_cmds,
                      ARRAY_SIZE(display_off_cmds));
    if(sent_cmds !=   ARRAY_SIZE(display_off_cmds) ) {
        pr_err("[LCD]%s send display_off_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
    
    sent_cmds = mipi_dsi_cmds_tx(&otm1282b_tx_buf,
                                 sleep_in_cmds,
                      ARRAY_SIZE(sleep_in_cmds));
    if(sent_cmds !=   ARRAY_SIZE(sleep_in_cmds) ) {
        pr_err("[LCD]%s send sleep_in_cmds stopped(%d)\n",__func__,sent_cmds);
        return -ENODEV;
    }
    
    msleep(70);

    pr_info("[LCD]%s leave\n",__func__);
    return 0;
}

static void mipi_otm1282b_cmd_qhd_display_on(void)
{
    int sent_cmds;

/* FUJITSU:2013-06-21 LCD shutdown start*/
    mutex_lock(&mipi_otm1282b_cmd_shutdown_mutex);
/* FUJITSU:2013-06-21 LCD shutdown end*/

    if(lcd_state != LCD_READY) {
/* FUJITSU:2013-06-21 LCD shutdown start*/
        mutex_unlock(&mipi_otm1282b_cmd_shutdown_mutex);
/* FUJITSU:2013-06-21 LCD shutdown end*/
        return;
    }
    
    pr_info("[LCD]%s enter\n",__func__);
    
    msleep(120);
    
    sent_cmds = mipi_dsi_cmds_tx(&otm1282b_tx_buf,
                                 display_on_cmds,
                      ARRAY_SIZE(display_on_cmds));
    if(sent_cmds !=   ARRAY_SIZE(display_on_cmds) ) {
        pr_err("[LCD]%s send display_on_cmds stopped(%d)",__func__,sent_cmds);
    }
    
    msleep(20);
    
    lcd_state = LCD_ON;

/* FUJITSU:2013-06-21 LCD shutdown start*/
    mutex_unlock(&mipi_otm1282b_cmd_shutdown_mutex);
/* FUJITSU:2013-06-21 LCD shutdown end*/

    pr_info("[LCD]%s leave\n",__func__);
    return;
}

/* FUJITSU:2013-06-21 LCD shutdown start*/
void mipi_otm1282b_cmd_qhd_power_off(void)
{
    pr_info("[LCD]%s enter\n",__func__);

    mutex_lock(&mipi_otm1282b_cmd_shutdown_mutex);

    mipi_otm1282b_cmd_qhd_lcd_off(NULL);

    mutex_unlock(&mipi_otm1282b_cmd_shutdown_mutex);

    pr_info("[LCD]%s leave\n",__func__);
    return;
}
/* FUJITSU:2013-06-21 LCD shutdown end*/

#if 1  /* FUJITSU:2013-05-17 mode_change add */
static int mipi_otm1282b_cmd_qhd_atoh(const char c)
{
    int val = -1;
    
    if (c >= '0' && c <= '9' ) {
        val = c - '0';
    } else if (c >= 'A' && c <= 'F') {
        val = c + 0xa - 'A';
    }
    return val;
}

static char cmd_wcabc[] = {
    0x55, 0x03,
};

static struct dsi_cmd_desc wcabc_set = {
    DTYPE_GEN_WRITE1, 1, 0, 0,   0, sizeof(cmd_wcabc),cmd_wcabc };


static ssize_t mipi_otm1282b_cmd_qhd_get_wcabc(char *buf)
{
    ssize_t ret;
    ret = sprintf(buf,"%02x\n",cmd_WhiteMagic_setting3[1]);
    pr_info("%s return(%02x)\n",__func__,cmd_WhiteMagic_setting3[1]);
    return ret;
}

static ssize_t mipi_otm1282b_cmd_qhd_set_wcabc(const char *buf,size_t count)
{
    int val = -1;
    char tmp[2];
    
    pr_info("%s.\n",__func__);
    
    tmp[0] = *buf++;
    tmp[1] = *buf;
    
    val = mipi_otm1282b_cmd_qhd_atoh(tmp[0]);
    val *= 0x10;
    val += mipi_otm1282b_cmd_qhd_atoh(tmp[1]);
    
    if (cmd_WhiteMagic_setting3[1] == val) { 
        pr_debug("%s already set(%d,%d)\n",__func__,val,cmd_WhiteMagic_setting3[1]);
        return count;
    }
    cmd_wcabc[1]=val;
    pr_debug("%s param(0x%02x)\n",__func__,cmd_wcabc[1]);
    
    if ( lcd_state == LCD_ON) {
        struct dcs_cmd_req cmdreq;

        pr_info("%s put param(%d)\n",__func__,cmd_wcabc[1]);

        cmdreq.cmds = &wcabc_set;
        cmdreq.cmds_cnt = 1;
        cmdreq.flags = CMD_REQ_COMMIT;
        cmdreq.rlen = 0;
        cmdreq.cb = NULL;
        mipi_dsi_cmdlist_put(&cmdreq);
    }
    
    cmd_WhiteMagic_setting3[1]=cmd_wcabc[1];
    
    return count;
}
#endif /* FUJITSU:2013-05-17 mode_change add */

static struct otm1282b_ctrl_funcs pfuncs = {
    .on_fnc               = mipi_otm1282b_cmd_qhd_lcd_on,
    .off_fnc              = mipi_otm1282b_cmd_qhd_lcd_off,
    .display_on_fnc       = mipi_otm1282b_cmd_qhd_display_on,
    .get_wcabc            = mipi_otm1282b_cmd_qhd_get_wcabc,/* FUJITSU:2013-05-17 mode_change add */
    .set_wcabc            = mipi_otm1282b_cmd_qhd_set_wcabc,/* FUJITSU:2013-05-17 mode_change add */
/* FUJITSU:2013-06-21 LCD shutdown start*/
    .shutdown             = mipi_otm1282b_cmd_qhd_power_off,
/* FUJITSU:2013-06-21 LCD shutdown end*/
};

static int mipi_otm1282b_cmd_qhd_pt_init(void)
{
    int ret;

    pr_info("[LCD]%s enter\n",__func__);

#if 1  /* FUJITSU:2013-05-30 DISP add start */
    if (msm_fb_detect_client("mipi_otm1282b2_cmd_qhd") == 0) {
        pinfo.lcd.hw_vsync_mode = TRUE;
        pinfo.lcd.refx100 = 6400; /* adjust refx100 to prevent tearing */
        pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW_TE;
    
    } else if (msm_fb_detect_client("mipi_otm1282b_cmd_qhd") == 0) {
        pinfo.lcd.hw_vsync_mode = FALSE;
        pinfo.lcd.refx100 = 6000; /* adjust refx100 to prevent tearing */
        pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
    } else {
        return 0;
    }
#else
	if (msm_fb_detect_client("mipi_otm1282b_cmd_qhd"))
		return 0;
#endif /* FUJITSU:2013-05-30 DISP add start */

	pinfo.xres = 540;
	pinfo.yres = 960;
	pinfo.lcdc.xres_pad = 0;
	pinfo.lcdc.yres_pad = 0;

	pinfo.type = MIPI_CMD_PANEL;
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
	pinfo.bl_max = 101;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	pinfo.clk_rate = 436360000;
	pinfo.mipi.esc_byte_ratio = 4;

	pinfo.lcd.vsync_enable = TRUE;

	pinfo.mipi.mode = DSI_CMD_MODE;
	pinfo.mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = FALSE;
	pinfo.mipi.data_lane3 = FALSE;
	pinfo.mipi.t_clk_post = 0x04; //CHECK
	pinfo.mipi.t_clk_pre  = 0x1B; //CHECK
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.interleave_max = 1;
	pinfo.mipi.insert_dcs_cmd = TRUE;
	pinfo.mipi.wr_mem_continue = 0x3c;
	pinfo.mipi.wr_mem_start = 0x2c;
	pinfo.mipi.dsi_phy_db = &dsi_cmd_mode_phy_db;
	pinfo.mipi.tx_eot_append = TRUE;
	
	pinfo.mipi.frame_rate = 60;
		
    pinfo.actual_height = 94;
    pinfo.actual_width = 53;

    mipi_otm1282b_set_powerfuncs(&pfuncs);

    mipi_dsi_buf_alloc(&otm1282b_tx_buf, DSI_BUF_SIZE);
    mipi_dsi_buf_alloc(&otm1282b_rx_buf, DSI_BUF_SIZE);

    ret = mipi_otm1282b_device_register(&pinfo, MIPI_DSI_PRIM, MIPI_DSI_PANEL_QHD_PT );
    if (ret)
        pr_err("%s: failed to register device!\n", __func__);

    pr_info("[LCD]%s leave(%d)\n",__func__,ret);

    return ret;
}

module_init(mipi_otm1282b_cmd_qhd_pt_init);
