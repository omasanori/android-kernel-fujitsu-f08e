/*
 * mkdrv_regulator.c
 *
 * Copyright(C) 2012 - 2013 FUJITSU LIMITED
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

//==============================================================================
// include file
//==============================================================================
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include "mkdrv_common.h"
#include "mkdrv_regulator.h"

//==============================================================================
// define
//==============================================================================
#define MKDRV_REGULATOR_NUM				45

//==============================================================================
// typedef
//==============================================================================
typedef struct {
	char *				regname;
	unsigned int		regno_sw;
	unsigned int		regno_trim;
	int					base_uvolt;
	int					step_uvolt;
	unsigned int		max_step;
	struct regulator *	ctrlreg;
}mkdrv_regulator_ctrlparam;
//==============================================================================
// const data
//==============================================================================
static mkdrv_regulator_ctrlparam mkdrv_regulator_ctrl_tbl[MKDRV_REGULATOR_NUM] = {
/*    regname               regno_sw    regno_trim  base_uvolt  step_uvolt  max_step    ctrlreg */
	{ "test_8921_s1",		0x01,		0x01,		375000,		25000,		0x6C,		0 },
	{ "test_8921_s2",		0x02,		0x02,		375000,		25000,		0x6C,		0 },
	{ "test_8921_s3",		0x03,		0x03,		375000,		25000,		0x6C,		0 },
	{ "test_8921_s4",		0x04,		0x04,		375000,		25000,		0x6C,		0 },
	{ "test_8921_s5",		0x05,		0x05,		375000,		25000,		0x76,		0 },
	{ "test_8921_s6",		0x06,		0x06,		375000,		25000,		0x76,		0 },
	{ "test_8921_s7",		0x07,		0x07,		375000,		25000,		0x6C,		0 },
	{ "test_8921_s8",		0x08,		0x08,		375000,		25000,		0x6C,		0 },
	{ "test_8921_l1",		0x09,		0x09,		750000,		25000,		0x20,		0 },
	{ "test_8921_l2",		0x0A,		0x0A,		750000,		25000,		0x20,		0 },
	{ "test_8921_l3",		0x0B,		0x0B,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l4",		0x0C,		0x0C,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l5",		0x0D,		0x0D,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l6",		0x0E,		0x0E,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l7",		0x0F,		0x0F,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l8",		0x10,		0x10,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l9",		0x11,		0x11,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l10",		0x12,		0x12,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l11",		0x13,		0x13,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l12",		0x14,		0x14,		750000,		25000,		0x20,		0 },
	{ "test_8921_l14",		0x15,		0x15,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l15",		0x16,		0x16,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l16",		0x17,		0x17,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l17",		0x18,		0x18,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l18",		0x19,		0x19,		750000,		25000,		0x20,		0 },
	{ "test_8921_l21",		0x1A,		0x1A,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l22",		0x1B,		0x1B,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l23",		0x1C,		0x1C,		750000,		25000,		0xA7,		0 },
	{ "test_8921_l24",		0x1D,		0x1D,		750000,		25000,		0x20,		0 },
	{ "test_8921_l25",		0x1E,		0x1E,		750000,		25000,		0x20,		0 },
	{ "test_8921_l26",		0x1F,		0x1F,		750000,		25000,		0x20,		0 },
	{ "test_8921_l27",		0x20,		0x20,		750000,		25000,		0x20,		0 },
	{ "test_8921_l28",		0x21,		0x21,		750000,		25000,		0x20,		0 },
	{ "test_8921_l29",		0x22,		0x22,		750000,		25000,		0xA7,		0 },
	{ "test_8921_lvs1",		0x23,		0x00,		0,			0,			0,			0 },
	{ "test_8921_lvs2",		0x24,		0x00,		0,			0,			0,			0 },
	{ "test_8921_lvs3",		0x25,		0x00,		0,			0,			0,			0 },
	{ "test_8921_lvs4",		0x26,		0x00,		0,			0,			0,			0 },
	{ "test_8921_lvs5",		0x27,		0x00,		0,			0,			0,			0 },
	{ "test_8921_lvs6",		0x28,		0x00,		0,			0,			0,			0 },
	{ "test_8921_lvs7",		0x29,		0x00,		0,			0,			0,			0 },
	{ "test_8921_usb_otg",	0x2A,		0x00,		0,			0,			0,			0 },
	{ "test_8921_hdmi_mvs",	0x2B,		0x00,		0,			0,			0,			0 },
	{ "8821_s0",			0x2C,		0x23,		375000,		25000,		0,			0 },
	{ "8821_s1",			0x2D,		0x24,		375000,		25000,		0,			0 },
};
//always on init enable flag
int reg_init_enable[MKDRV_REGULATOR_NUM];
//regulator get flag
static int regulator_get_flag = 0;
//==============================================================================
// static functions prototype
//==============================================================================
static void _mkdrv_regulator_get(void);
//==============================================================================
// functions
//==============================================================================
/**
 * mkdrv_regulator_switch
 * 
 * @param
 *
 * @retval success(0),error(non zero)
 */
int mkdrv_regulator_switch(mkdrv_func_data_t * data)
{
	int							ret = 0,currentval=0;
	unsigned int				switch_regno,onoff;
	mkdrv_regulator_ctrlparam	*regulator_ctrl_tbl_ptr = NULL;
	int							*reg_init_enable_ptr = NULL;
	int							cnt = 0;

	switch_regno = data->iInputdata[0];
	onoff = data->iInputdata[1];

	if(switch_regno < 1){
		printk(KERN_ERR "%s switch reg no range err no:%d\n",__func__,switch_regno);
		return -1;
	}

	if(onoff > 1){
		printk(KERN_ERR "%s switch param err\n",__func__);
		return -1;
	}

	if(regulator_get_flag == 0){
		_mkdrv_regulator_get();
	}

	for (cnt = 0; cnt < MKDRV_REGULATOR_NUM; cnt++) {
		if (mkdrv_regulator_ctrl_tbl[cnt].regno_sw == switch_regno) {
			regulator_ctrl_tbl_ptr = &mkdrv_regulator_ctrl_tbl[cnt];
			reg_init_enable_ptr = &reg_init_enable[cnt];
			break;
		}
	}

	if (regulator_ctrl_tbl_ptr == NULL) {
		printk(KERN_ERR "%s switch reg no err no:%d\n",__func__,switch_regno);
		return -1;
	}

	if (IS_ERR(regulator_ctrl_tbl_ptr->ctrlreg)) {
		printk(KERN_ERR "%s reg get err no:%d\n",__func__,switch_regno);
		return -1;
	}

	if(onoff == 1){
		if(regulator_ctrl_tbl_ptr->max_step != 0x00){
			currentval = regulator_get_voltage(regulator_ctrl_tbl_ptr->ctrlreg);
			if(currentval < 0){
				printk(KERN_ERR "%s get volt err no:%d\n",__func__,switch_regno);
				return -1;
			}
			ret = regulator_set_voltage(regulator_ctrl_tbl_ptr->ctrlreg,currentval,currentval);
			if (ret < 0) {
				printk(KERN_ERR "%s set volt err no:%d\n",__func__,switch_regno);
				return -1;
			}
		}
		ret = regulator_enable(regulator_ctrl_tbl_ptr->ctrlreg);
		if(ret < 0){
			printk(KERN_ERR "%s reg enable err no:%d\n",__func__,switch_regno);
			return -1;
		}else{
			printk(KERN_INFO "regulator enable success no:%d cur:%d\n",switch_regno,currentval);
			if(*reg_init_enable_ptr == 0){
				*reg_init_enable_ptr = 1;
			}
		}
	}else{
		if(regulator_is_enabled(regulator_ctrl_tbl_ptr->ctrlreg) == false){
			if(regulator_ctrl_tbl_ptr->max_step != 0x00){
				currentval = regulator_get_voltage(regulator_ctrl_tbl_ptr->ctrlreg);
				if(currentval < 0){
					printk(KERN_ERR "%s get volt err no:%d\n",__func__,switch_regno);
				}else{
					ret = regulator_set_voltage(regulator_ctrl_tbl_ptr->ctrlreg,currentval,currentval);
					if (ret < 0) {
						printk(KERN_ERR "%s set volt err no:%d\n",__func__,switch_regno);
					}
				}
			}
			ret = regulator_enable(regulator_ctrl_tbl_ptr->ctrlreg);
			if(ret < 0){
				printk(KERN_ERR "%s reg enable err no:%d\n",__func__,switch_regno);
			}
		}
		ret = regulator_force_disable(regulator_ctrl_tbl_ptr->ctrlreg);
		if(ret < 0){
			printk(KERN_ERR "%s reg disable err no:%d\n",__func__,switch_regno);
			return -1;
		}else{
			printk(KERN_INFO "regulator disable success no:%d\n",switch_regno);
		}
	}

	return 0;
}
/**
 * mkdrv_regulator_trim
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
int mkdrv_regulator_trim(mkdrv_func_data_t * data)
{
	int							ret = 0,currentval,settingval;
	unsigned int				trim_regno,trimval,trim_sel;
	int							before_reg_enable = 0;
	mkdrv_regulator_ctrlparam	*regulator_ctrl_tbl_ptr = NULL;
	int							*reg_init_enable_ptr = NULL;
	int							cnt = 0;

	trim_regno = data->iInputdata[0];
	trim_sel = data->iInputdata[1];
	trimval = data->iInputdata[2];

	if(trim_regno < 1){
		printk(KERN_ERR "%s trim reg no range err no:%d\n",__func__,trim_regno);
		return -1;
	}

	if(regulator_get_flag == 0){
		_mkdrv_regulator_get();
	}

	for (cnt = 0; cnt < MKDRV_REGULATOR_NUM; cnt++) {
		if (mkdrv_regulator_ctrl_tbl[cnt].regno_trim == trim_regno) {
			regulator_ctrl_tbl_ptr = &mkdrv_regulator_ctrl_tbl[cnt];
			reg_init_enable_ptr = &reg_init_enable[cnt];
			break;
		}
	}

	if (regulator_ctrl_tbl_ptr == NULL) {
		printk(KERN_ERR "%s trim reg no err no:%d\n",__func__,trim_regno);
		return -1;
	}

	if (IS_ERR(regulator_ctrl_tbl_ptr->ctrlreg)) {
		printk(KERN_ERR "%s regget err no:%d\n",__func__,trim_regno);
		return -1;
	}

	if(trim_sel > 1){
		printk(KERN_ERR "%s trim sel err sel:%d\n",__func__,trim_sel);
		return -1;
	}
	if(trim_sel == 1){
		if(regulator_ctrl_tbl_ptr->max_step == 0){
			printk(KERN_ERR "%s trim set err no:%d\n",__func__,trim_regno);
			return -1;
		}
		if(trimval < 1 || trimval > regulator_ctrl_tbl_ptr->max_step){
			printk(KERN_ERR "%s trim range err trim:%d\n",__func__,trimval);
			return -1;
		}
	}

	currentval = regulator_get_voltage(regulator_ctrl_tbl_ptr->ctrlreg);
	if(currentval < 0){
		printk(KERN_ERR "%s get volt err no:%d\n",__func__,trim_regno);
		return -1;
	}
	data->iOutputdata[1] = (currentval - regulator_ctrl_tbl_ptr->base_uvolt) /
	               regulator_ctrl_tbl_ptr->step_uvolt;
	data->iOutputdata[1] += 1;
	if(trim_sel == 0){
		data->iOutputdata[0] = data->iOutputdata[1];
		printk(KERN_INFO "regulator trim no:%d curr:%d\n",trim_regno,data->iOutputdata[1]);
		return 0;
	}
	if(*reg_init_enable_ptr == 0){
		before_reg_enable = 1;
	}
	if(regulator_is_enabled(regulator_ctrl_tbl_ptr->ctrlreg) == false){
		before_reg_enable = 1;
	}

	if(before_reg_enable == 1){
		ret = regulator_set_voltage(regulator_ctrl_tbl_ptr->ctrlreg,currentval,currentval);
		if (ret < 0) {
			printk(KERN_ERR "%s set enable volt err trim:%d\n",__func__,trimval);
		}
		ret = regulator_enable(regulator_ctrl_tbl_ptr->ctrlreg);
		if(ret < 0){
			printk(KERN_ERR "%s reg enable err no:%d\n",__func__,trim_regno);
		}else{
			if(*reg_init_enable_ptr == 0){
				*reg_init_enable_ptr = 1;
			}
		}
	}

	settingval = regulator_ctrl_tbl_ptr->base_uvolt +
	             (regulator_ctrl_tbl_ptr->step_uvolt * (trimval - 1));
	ret = regulator_set_voltage(regulator_ctrl_tbl_ptr->ctrlreg,settingval,settingval);
	if (ret < 0) {
		printk(KERN_ERR "%s set volt err trim:%d\n",__func__,trimval);
		return -1;
	}
	data->iOutputdata[0] = trimval;
	printk(KERN_INFO "regulator trim success no:%d set:%d curr:%d\n",trim_regno,data->iOutputdata[0],data->iOutputdata[1]);
	return 0;
}
/**
 * mkdrv_regulator_init
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
int mkdrv_regulator_init(void)
{
	memset(reg_init_enable,0,sizeof(reg_init_enable));

	return 0;
}
/**
 * mkdrv_regulator_init
 * 
 * @param  
 * @retval success(0),error(non zero)
 */
static void _mkdrv_regulator_get(void)
{
	int		i;
	int		error = 0;

	printk(KERN_INFO "%s start\n",__func__);
	/* All Regulator get */
	for(i=0;i<MKDRV_REGULATOR_NUM;i++){
		if (mkdrv_regulator_ctrl_tbl[i].ctrlreg == 0) {
			mkdrv_regulator_ctrl_tbl[i].ctrlreg = regulator_get(NULL,mkdrv_regulator_ctrl_tbl[i].regname);
			if (IS_ERR(mkdrv_regulator_ctrl_tbl[i].ctrlreg)) {
				printk(KERN_ERR "%s regget err no:%d\n",__func__,i);
				error = 1;
			}
		}
	}

	if(error == 0){
		regulator_get_flag = 1;
	}

	return;
}
MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("mkdrv device");
MODULE_LICENSE("GPL");
