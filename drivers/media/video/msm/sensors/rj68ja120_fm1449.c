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

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/mfd/pm8xxx/pm8921.h>
//#include <linux/leds-bd61800.h>
#include <media/msm_camera.h>
#include <mach/camera.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
//#include <media/v4l2-subdev.h>
#include "msm.h"
#include "msm_sensor.h"
#include "msm_ispif.h"
//#include <mach/irqs.h>
#ifndef CAMSENSOR_GPIO_FRONT_H
#include "camsensor_gpio_front.h"
#endif // CAMSENSOR_GPIO_FRONT_H
#include "../../../arch/arm/mach-msm/smd_private.h"
#include "../../../arch/arm/mach-msm/board-8064.h"

#define PLATFORM_DRIVER_NAME  "rj68ja120_fm1449"
#define rj68ja120_fm1449_obj rj68ja120_fm1449_##obj

#define LOGI(fmt, args...)      printk(KERN_INFO "rj68ja120_fm1449: " fmt, ##args)
//#define LOGI(fmt, args...)      do{}while(0)
//#define LOGD(fmt, args...)      printk(KERN_DEBUG "rj68ja120_fm1449: " fmt, ##args)
#define LOGD(fmt, args...)      do{}while(0)
#define LOGE(fmt, args...)      printk(KERN_ERR "rj68ja120_fm1449: " fmt, ##args)

static bool is_use_i2c_driver = true;

// GPIO
//#define RESET                   288
//#define MCLK                    4
#define MCLK                    5
//#define CAM_STBY                287
//#define CAM_INT                 283
#define CAM_I2C_SDA             16
#define CAM_I2C_SCL             17

static unsigned RESET = BU1852_EXP1_GPIO_TO_SYS(12); // gpio12
static unsigned CAM_STBY = BU1852_EXP1_GPIO_TO_SYS(11); // gpio11
static unsigned CAM_INT = BU1852_EXP2_GPIO_TO_SYS(17); // gpio17

// I2C ADDRESS
#if 0	//BRA	1
#define CAM_I2C_ADDRESS         0x2D
#else
#define CAM_I2C_ADDRESS         0x3C
#endif

//#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + NR_GPIO_IRQS)

/*===================================================================*
    LOCAL DECLARATIONS
 *===================================================================*/
DEFINE_MUTEX(rj68ja120_fm1449_mtx);
static struct msm_sensor_ctrl_t rj68ja120_fm1449_s_ctrl;

/*===================================================================*
    EXTERNAL DECLARATIONS
 *===================================================================*/
//extern int factory_mode(void);
extern int _I2C_WAIT_;
extern unsigned int system_rev;

////////////////////////////////
// I2C
////////////////////////////////
static void __i2c_err_dump(struct camsensor_i2c_command_isp* cmd)
{
	int i;
	LOGE(">> I2C err dump start!\n");
	if (cmd->rlen != 0) {
		// read
		for (i=0; i<cmd->rlen; i++) {
			LOGE("<m6mo read err> [cate:0x%02x param:0x%02x]\n",
				cmd->category, cmd->byte+i);
		}
	} else {
		// write
		for (i=0; i<cmd->wlen; i++) {
			LOGE("<m6mo write err> [cate:0x%02x param:0x%02x] <-0x%02x\n",
				cmd->category, cmd->byte+i, cmd->pwdata[i]);
		}
	}
	LOGE("<< I2C err dump end!\n");
}

static void __i2c_access_dump(struct camsensor_i2c_command_isp* cmd)
{
	int i;
	LOGD(">> I2C ok dump !\n");
	if (cmd->rlen != 0) {
		// read
		for (i=0; i<cmd->rlen; i++) {
			LOGD("<m6mo read ok> [cate:0x%02x param:0x%02x] ->0x%02x\n",
				cmd->category, cmd->byte+i, cmd->prdata[i]);
		}
	} else {
		// write
		for (i=0; i<cmd->wlen; i++) {
			LOGD("<m6mo write ok> [cate:0x%02x param:0x%02x] <-0x%02x\n",
				cmd->category, cmd->byte+i, cmd->pwdata[i]);
		}
	}
	LOGD("<< I2C ok dump end!\n");
}

static int rj68ja120_fm1449_i2c_write_isp(
	struct i2c_client* client, struct camsensor_i2c_command_isp* cmd)
{
	int rc = 0;
	if (is_use_i2c_driver) {
		rc = camsensor_i2c_front_write_isp(client, cmd);
	} else {
		rc = camsensor_gpioi2c_front_write_isp(cmd);
	}
	if (rc) {
		__i2c_err_dump(cmd);
	} else {
		__i2c_access_dump(cmd);
	}

	return rc;
}

static int rj68ja120_fm1449_i2c_read_isp(
	struct i2c_client* client, struct camsensor_i2c_command_isp* cmd)
{
	int rc = 0;
	if (is_use_i2c_driver) {
		rc = camsensor_i2c_front_read_isp(client, cmd);
	} else {
		rc = camsensor_gpioi2c_front_read_isp(cmd);
	}
	if (rc) {
		__i2c_err_dump(cmd);
	} else {
		__i2c_access_dump(cmd);
	}
	return rc;
}

static int rj68ja120_fm1449_i2c_write(
	struct i2c_client* client, struct camsensor_i2c_command* cmd)
{
	int rc = 0;
	if (is_use_i2c_driver) {
		rc = camsensor_i2c_front_write(client, cmd);
	} else {
		rc = camsensor_gpioi2c_front_write(cmd);
	}
	if (rc)
		LOGE("%s ERR!", __func__);

	return rc;
}

static int rj68ja120_fm1449_i2c_read(
	struct i2c_client* client, struct camsensor_i2c_command* cmd)
{
	int rc = 0;
	if (is_use_i2c_driver) {
		rc = camsensor_i2c_front_read(client, cmd);
	} else {
		rc = camsensor_gpioi2c_front_read(cmd);
	}
	if (rc)
		LOGE("%s ERR!", __func__);

	return rc;
}

static int32_t rj68ja120_fm1449_get_temp(struct msm_sensor_ctrl_t *s_ctrl,
			uint32_t* temp)
{
    int rc = 0;
	uint32_t def_temp = 0x70;  // normal temperature.
    *temp = def_temp;
    return rc;
}

static int32_t rj68ja120_fm1449_command(struct msm_sensor_ctrl_t *s_ctrl,
			struct cfg_cmd_ctrl_isp* cmd)
{
    struct camsensor_i2c_command_isp i2c_cmd;
	int rc = 0;
	int cpl = 0;
    i2c_cmd.slave_addr = CAM_I2C_ADDRESS;
	i2c_cmd.category   = cmd->category;
	i2c_cmd.byte       = cmd->byte;
    i2c_cmd.pwdata     = cmd->wvalue;
    i2c_cmd.wlen       = cmd->txlen;
    i2c_cmd.prdata     = cmd->rvalue;
    i2c_cmd.rlen       = cmd->rxlen;

	if (!cmd->rxlen) {
    	rc = rj68ja120_fm1449_i2c_write_isp(s_ctrl->sensor_i2c_client->client, &i2c_cmd);
	} else {
    	rc = rj68ja120_fm1449_i2c_read_isp(s_ctrl->sensor_i2c_client->client, &i2c_cmd);
		if (!rc) {
			for (cpl=0; cpl<cmd->rxlen; cpl++) {
				cmd->rvalue[cpl] = i2c_cmd.prdata[cpl];
			}
		}
	}
	if (rc) {
		LOGE("%s %s failed(%d)!!",
			__func__, cmd->rxlen ? "read" : "write", rc);
	}
	return rc;
}

static int32_t rj68ja120_fm1449_command_normal(struct msm_sensor_ctrl_t *s_ctrl,
			struct cfg_cmd_ctrl* cmd)
{
	struct camsensor_i2c_command i2c_cmd;
	int rc = 0;
	int cpl = 0;
    i2c_cmd.slave_addr = CAM_I2C_ADDRESS;
    i2c_cmd.pwdata     = cmd->wvalue;
    i2c_cmd.wlen       = cmd->txlen;
    i2c_cmd.prdata     = cmd->rvalue;
    i2c_cmd.rlen       = cmd->rxlen;

	if (!cmd->rxlen) {
    	rc = rj68ja120_fm1449_i2c_write(s_ctrl->sensor_i2c_client->client, &i2c_cmd);
	} else {
    	rc = rj68ja120_fm1449_i2c_read(s_ctrl->sensor_i2c_client->client, &i2c_cmd);
		if (!rc) {
			for (cpl=0; cpl<cmd->rxlen; cpl++) {
				cmd->rvalue[cpl] = i2c_cmd.prdata[cpl];
			}
		}
	}
	if (rc) {
		LOGE("%s %s failed(%d)!!",
			__func__, cmd->rxlen ? "read" : "write", rc);
	}
	return rc;
}

static int32_t rj68ja120_fm1449_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
    int32_t rc = 0;
	uint32_t p_clk = 228570000;

    LOGI("+%s(%d,%d)\n", __func__, update_type, res);
//	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		msm_sensor_enable_debugfs(s_ctrl);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &p_clk);
//		msleep(30);
	}

    LOGD("-%s done(%d).\n", __func__, rc);
    return rc;
}

static int32_t rj68ja120_fm1449_set_sensor_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int mode, int res)
{
    int32_t rc = 0;
	LOGI("+%s()\n", __func__);
	if (s_ctrl->curr_res != res) {
		if (s_ctrl->func_tbl->sensor_setting
			(s_ctrl, MSM_SENSOR_UPDATE_PERIODIC, res) < 0)
			return rc;
		s_ctrl->curr_res = res;
		s_ctrl->cam_mode = mode;
	}
    LOGD("-%s end(%d).\n", __func__, rc);
    return rc;
}

static int32_t rj68ja120_fm1449_sensor_mode_init(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info)
{
	int32_t rc = 0;
	s_ctrl->fps_divider = Q10;
	s_ctrl->cam_mode = MSM_SENSOR_MODE_INVALID;

	LOGI("+%s()\n", __func__);

	if (mode != s_ctrl->cam_mode) {
		s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
		s_ctrl->cam_mode = mode;

		rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
			MSM_SENSOR_REG_INIT, 0);
	}
	return rc;
}

static void rj68ja120_fm1449_delay_frames(struct msm_sensor_ctrl_t *s_ctrl)
{
	long fps = 0;
	uint32_t delay = 0;

	if (s_ctrl->curr_res < MSM_SENSOR_INVALID_RES &&
		s_ctrl->wait_num_frames > 0) {
//		fps = s_ctrl->msm_sensor_reg->
//			output_settings[s_ctrl->curr_res].vt_pixel_clk /
//			s_ctrl->curr_frame_length_lines /
//			s_ctrl->curr_line_length_pclk;
		fps = 15;
		delay = (1000 * s_ctrl->wait_num_frames) / fps / Q10;
	}
	LOGI("%s fps = %ld, delay = %d, min_delay %d\n", __func__, fps,
		delay, s_ctrl->min_delay);
	if (delay > s_ctrl->min_delay)
		msleep(delay);
	else if (s_ctrl->min_delay)
		msleep(s_ctrl->min_delay);
	return;
}

static int32_t rj68ja120_fm1449_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
    struct sensor_cfg_data cfg;
    int rc = 0;

    if (copy_from_user(&cfg, (void *)argp, sizeof(struct sensor_cfg_data)))
        return -EFAULT;

	mutex_lock(s_ctrl->msm_sensor_mutex);
    switch (cfg.cfgtype) {

	case CFG_SENSOR_INIT:
        LOGD("-%s (CFG_SENSOR_INIT)!\n", __func__);
		if (!s_ctrl->func_tbl->sensor_mode_init) {
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->func_tbl->sensor_mode_init(
			s_ctrl,
			cfg.mode,
			&(cfg.cfg.init_info));
		break;

    case CFG_GET_TEMP:
        LOGD("-%s (CFG_SENSOR_INIT)!\n", __func__);
    	rc = rj68ja120_fm1449_get_temp(s_ctrl, &cfg.cfg.temp);
        if (rc < 0) {
    		break;
    	}
        LOGD("+%s (CFG_GET_TEMP:%d)\n", __func__, cfg.cfg.temp);
        if (copy_to_user((void *)argp, &cfg, sizeof(struct sensor_cfg_data)))
            rc = -EFAULT;
        break;

    case CFG_PWR_UP:
        LOGD("-%s (CFG_PWR_UP)\n", __func__);
    	if ((!s_ctrl->func_tbl->sensor_power_down) ||
    		(!s_ctrl->func_tbl->sensor_power_up)){
			rc = -EFAULT;
			break;
		}
        rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
        if (rc < 0) {
            LOGE("-%s sensor_power_down Failed!\n", __func__);
        }
        mdelay(10);
        rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
        if (rc < 0) {
            LOGE("-%s sensor_power_up Failed!\n", __func__);
        }
        break;

    case CFG_COMMAND:
        LOGD("-%s (CFG_COMMAND)\n", __func__);
    	rc = rj68ja120_fm1449_command(s_ctrl, &cfg.cfg.cmd_isp);
    	if (!rc) {
	       	if (cfg.cfg.cmd_isp.rxlen) {
	               rc = copy_to_user((void *)argp, &cfg, sizeof(struct sensor_cfg_data));
	               if(rc != 0){
						LOGE("-%s copy_to_user() Failed!(%d)\n", __func__, rc);
						rc = -EFAULT;
	   			}
	       	}
    	}
        break;

    case CFG_COMMAND_NORMAL:
        LOGD("-%s (CFG_COMMAND_NORMAL)\n", __func__);
    	rc = rj68ja120_fm1449_command_normal(s_ctrl, &cfg.cfg.cmd);
    	if (!rc) {
    		if (copy_to_user((void *)argp, &cfg, sizeof(struct sensor_cfg_data))) {
                rc = -EFAULT;
    		}
    	}
        break;

    case CFG_SET_MODE:
        LOGD("-%s (CFG_SET_MODE)\n", __func__);
		if (!s_ctrl->func_tbl->sensor_set_sensor_mode) {
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->func_tbl->sensor_set_sensor_mode(
 				s_ctrl, cfg.mode, cfg.rs);
        break;

    case CFG_SET_IOPORT:
        LOGD("-%s (CFG_SET_IOPORT) gpio(%d), val(%d)\n", __func__, cfg.cfg.ioport.pin, cfg.cfg.ioport.val);
        //gpio_set_value(cfg.cfg.ioport.pin, cfg.cfg.ioport.val);
    	gpio_set_value_cansleep(cfg.cfg.ioport.pin, cfg.cfg.ioport.val);
        break;

    case CFG_CAM_INT:
   		cfg.cfg.cam_int = gpio_get_value_cansleep(CAM_INT);
//		LOGI("-%s (CFG_CAM_INT:%d) system_rev(0x%x)\n", __func__, cfg.cfg.cam_int, system_rev);
		LOGI("-%s (CFG_CAM_INT:%d)\n", __func__, cfg.cfg.cam_int);
		if (copy_to_user((void *)argp, &cfg, sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_CSI_PARAMS:
		if (s_ctrl->func_tbl->sensor_get_csi_params == NULL) {
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->func_tbl->sensor_get_csi_params(
			s_ctrl,
			&cfg.cfg.csi_lane_params);

		if (copy_to_user((void *)argp,
			&cfg,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_START_STREAM:
        LOGI("-%s (CFG_START_STREAM) !\n", __func__);
    	rj68ja120_fm1449_delay_frames(s_ctrl);
		break;

    // not supported command
	case CFG_STOP_STREAM:
        LOGI("-%s (CFG_STOP_STREAM) nop!\n", __func__);
		break;
    case CFG_START:
        LOGI("-%s (CFG_START) nop!\n", __func__);
		break;
	case CFG_GET_EEPROM_DATA:
        LOGI("-%s (CFG_GET_EEPROM_DATA) nop!\n", __func__);
    	break;
	case CFG_GET_OUTPUT_INFO:
        LOGI("-%s (CFG_GET_OUTPUT_INFO) nop!\n", __func__);
    	break;
	case CFG_SEND_WB_INFO:
        LOGI("-%s (CFG_SEND_WB_INFO) nop!\n", __func__);
    	break;
	case CFG_SET_EFFECT:
        LOGI("-%s (CFG_SET_EFFECT) nop!\n", __func__);
    	break;
	case CFG_GET_AF_MAX_STEPS:
        LOGI("-%s (CFG_GET_AF_MAX_STEPS) nop!\n", __func__);
    	break;
	case CFG_SET_DEFAULT_FOCUS:
        LOGI("-%s (CFG_SET_DEFAULT_FOCUS) nop!\n", __func__);
    	break;
	case CFG_MOVE_FOCUS:
        LOGI("-%s (CFG_MOVE_FOCUS) nop!\n", __func__);
    	break;
	case CFG_PWR_DOWN:
        LOGI("-%s (CFG_PWR_DOWN) nop!\n", __func__);
    	break;
	case CFG_SET_PICT_EXP_GAIN:
        LOGI("-%s (CFG_SET_PICT_EXP_GAIN) nop!\n", __func__);
    	break;
	case CFG_SET_EXP_GAIN:
        LOGI("-%s (CFG_SET_EXP_GAIN) nop!\n", __func__);
    	break;
	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
        LOGI("-%s (CFG_SET_FPS/CFG_SET_PICT_FPS) nop!\n", __func__);
    	break;
	case CFG_GET_PICT_MAX_EXP_LC:
        LOGI("-%s (CFG_GET_PICT_MAX_EXP_LC) nop!\n", __func__);
    	break;
	case CFG_GET_PICT_P_PL:
        LOGI("-%s (CFG_GET_PICT_P_PL) nop!\n", __func__);
    	break;
	case CFG_GET_PICT_L_PF:
        LOGI("-%s (CFG_GET_PICT_L_PF) nop!\n", __func__);
    	break;
	case CFG_GET_PREV_P_PL:
        LOGI("-%s (CFG_GET_PREV_P_PL) nop!\n", __func__);
    	break;
	case CFG_GET_PREV_L_PF:
        LOGI("-%s (CFG_GET_PREV_L_PF) nop!\n", __func__);
    	break;
	case CFG_GET_PICT_FPS:
        LOGI("-%s (CFG_GET_PICT_FPS) nop!\n", __func__);
    	break;
    default:
        LOGE("-%s ERR root:%d\n", __func__, cfg.cfgtype);
        rc = -EINVAL;
        break;
    }
	mutex_unlock(s_ctrl->msm_sensor_mutex);

    if (rc) LOGD("-%s done.(%d)\n", __func__, rc);
    return rc;
}

#if 0
struct pm8xxx_gpio_init {
    char *name;
	unsigned gpio;
	bool poff;
	struct pm_gpio config;
};
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + NR_GPIO_IRQS)
#define PM8XXX_GPIO_INIT(_name, _gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable, _poff) \
{ \
	.name	= _name, \
	.gpio	= PM8921_GPIO_PM_TO_SYS(_gpio), \
	.poff	= _poff, \
	.config	= { \
		.direction	= _dir, \
		.output_buffer	= _buf, \
		.output_value	= _val, \
		.pull		= _pull, \
		.vin_sel	= _vin, \
		.out_strength	= _out_strength, \
		.function	= _func, \
		.inv_int_pol	= _inv, \
		.disable_pin	= _disable, \
	} \
}
#define PM8XXX_GPIO_OUTPUT_FUNC(_name, _gpio, _val, _func, _poff) \
	PM8XXX_GPIO_INIT(_name, _gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_MED, \
			_func, 0, 0, _poff)

static struct pm8xxx_gpio_init pm8921_cam_gpios[] = {
// ICAM_PWON28(GPIO_17:+2.8V_ICAM_KO)
//	PM8XXX_GPIO_OUTPUT_FUNC("2.8V_ICAM_KO", 17, 0, PM_GPIO_FUNC_NORMAL, true),	//BRA
// ICAM_PWON15(GPIO_25:+1.5V_ICAM_KO)
	PM8XXX_GPIO_OUTPUT_FUNC("1.2V_ICAM_KO", 22, 0, PM_GPIO_FUNC_NORMAL, true),	//BRA VDDA
// ICAM_PWON18(GPIO_19:+1.8V_ICAM_KO)
//	PM8XXX_GPIO_OUTPUT_FUNC("1.8V_ICAM_KO", 19, 0, PM_GPIO_FUNC_NORMAL, true),	//BRA
};
#endif

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", 25600000},
};

//struct bu1852gxw_int {
//	char		*name;
//	unsigned	gpio;
//	bool		booton;
//	int			on_wait;
//	int			off_wait;
//};

//#define BU1852GXW_CAM_GPIOS_VAL 3
//static struct bu1852gxw_int bu1852gxw_cam_gpios[] = {
//	{"+2.8V_ICAM", 289, false, 1, 1},
//	{"+1.2V_ICAM", 291, false, 1, 1},
//	{"+1.8V_ICAM", 290, false, 1, 1},
//};

static int32_t rj68ja120_fm1449_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	LOGI("+%s()\n", __func__);

	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->vreg_seq,
			s_ctrl->num_vreg_seq,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->vreg_seq,
			s_ctrl->num_vreg_seq,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}

//	for (i = 0; i < BU1852GXW_CAM_GPIOS_VAL; i++) {
//		if(!(bu1852gxw_cam_gpios[i].booton)){
//			LOGD("%s: * gpiochek!(gpio_ex:%d val:%d, name:%s) !\n", 
//				__func__,
//				bu1852gxw_cam_gpios[i].gpio,
//				gpio_get_value_cansleep(bu1852gxw_cam_gpios[i].gpio),
//				bu1852gxw_cam_gpios[i].name);
//
//			gpio_set_value_cansleep(bu1852gxw_cam_gpios[i].gpio, 1);
//			mdelay(bu1852gxw_cam_gpios[i].on_wait);
//
//			LOGD("%s: * poweron(gpio_ex:%d val:%d, name:%s) !\n", 
//				__func__,
//				bu1852gxw_cam_gpios[i].gpio,
//				gpio_get_value_cansleep(bu1852gxw_cam_gpios[i].gpio),
//				bu1852gxw_cam_gpios[i].name);
//		}
//	}

	// STBY
	gpio_set_value_cansleep(CAM_STBY, 1);
	LOGD("%s: * ICAM_XSTBY(gpio_ex:%d) val:%d)\n", __func__, CAM_STBY, gpio_get_value_cansleep(CAM_STBY));

	//MCLK
	gpio_tlmm_config(GPIO_CFG(MCLK, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}
	msleep(1);

	// Reset
	gpio_set_value_cansleep(RESET, 1);
	LOGD("%s: * ICAM_XRST(gpio_ex:%d) val:%d)\n", __func__, RESET, gpio_get_value_cansleep(RESET));
	msleep(1);
	
	if( !is_use_i2c_driver ) {
		LOGI("+%s (i2c gpio cfg)\n", __func__);
		gpio_tlmm_config(GPIO_CFG(CAM_I2C_SDA, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(CAM_I2C_SCL, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}

	s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;

	LOGD("-%s Done.\n", __func__);
	return 0;

enable_clk_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->vreg_seq,
			s_ctrl->num_vreg_seq,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 0);

config_vreg_failed:
//_sensor_poweron_fail:
//	for (i = (BU1852GXW_CAM_GPIOS_VAL-1); i >= 0; --i) {
//		if(!(bu1852gxw_cam_gpios[i].booton)){
//			LOGD("%s: * gpiochek!(gpio_ex:%d val:%d, name:%s) !\n", 
//				__func__,
//				bu1852gxw_cam_gpios[i].gpio,
//				gpio_get_value_cansleep(bu1852gxw_cam_gpios[i].gpio),
//				bu1852gxw_cam_gpios[i].name);
//
//			gpio_set_value_cansleep(bu1852gxw_cam_gpios[i].gpio, 0);
//			mdelay(bu1852gxw_cam_gpios[i].on_wait);
//
//			LOGD("%s: * poweroff(gpio_ex:%d val:%d, name:%s) !\n", 
//				__func__,
//				bu1852gxw_cam_gpios[i].gpio,
//				gpio_get_value_cansleep(bu1852gxw_cam_gpios[i].gpio),
//				bu1852gxw_cam_gpios[i].name);
//		}
//	}

	kfree(s_ctrl->reg_ptr);

	LOGE("-%s Failed...\n", __func__);
	return -1;
}

static int32_t rj68ja120_fm1449_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
//	int i;
	LOGI("+%s()\n", __func__);

	if( !is_use_i2c_driver ) {
		LOGD("+%s(After 2-1) GPIO CFG\n", __func__);
		gpio_tlmm_config(GPIO_CFG(CAM_I2C_SDA, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(CAM_I2C_SCL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		//gpio_set_value(CAM_I2C_SDA, 1);	// KO_I2C3_SDA
		gpio_set_value_cansleep(CAM_I2C_SDA, 1);	// KO_I2C3_SDA
		//gpio_set_value(CAM_I2C_SCL, 1);	// KO_I2C3_SCL
		gpio_set_value_cansleep(CAM_I2C_SCL, 1);	// KO_I2C3_SCL
	}

	// STBY
	gpio_set_value_cansleep(CAM_STBY, 0);
	LOGD("%s: * ICAM_XSTBY(gpio_ex:286) val:%d)\n", __func__, gpio_get_value_cansleep(CAM_STBY));
	msleep(40);	//1 Frame Time + 100,000 system clocks->33ms+3.9ms
	
	// Reset
	gpio_set_value_cansleep(RESET, 0);
	LOGD("%s: * ICAM_XRST(gpio_ex:286) val:%d)\n", __func__, gpio_get_value_cansleep(RESET));
	msleep(1);

	// Clock
	gpio_tlmm_config( GPIO_CFG(MCLK, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE );
	msleep(10);

//	for (i = (BU1852GXW_CAM_GPIOS_VAL-1); i >= 0; --i) {
//		if(!(bu1852gxw_cam_gpios[i].booton)){
//			LOGD("%s: * gpiochek!(gpio_ex:%d val:%d, name:%s) !\n", 
//				__func__,
//				bu1852gxw_cam_gpios[i].gpio,
//				gpio_get_value_cansleep(bu1852gxw_cam_gpios[i].gpio),
//				bu1852gxw_cam_gpios[i].name);
//
//			gpio_set_value_cansleep(bu1852gxw_cam_gpios[i].gpio, 0);
//			mdelay(bu1852gxw_cam_gpios[i].on_wait);
//
//			LOGD("%s: * poweroff(gpio_ex:%d val:%d, name:%s) !\n", 
//				__func__,
//				bu1852gxw_cam_gpios[i].gpio,
//				gpio_get_value_cansleep(bu1852gxw_cam_gpios[i].gpio),
//				bu1852gxw_cam_gpios[i].name);
//		}
//	}

	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 0);

	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 0);

	kfree(s_ctrl->reg_ptr);

	s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;

    LOGD("-%s Done.\n", __func__);
    return 0;
}

static const struct i2c_device_id rj68ja120_fm1449_i2c_id[] = {
	{PLATFORM_DRIVER_NAME, (kernel_ulong_t)&rj68ja120_fm1449_s_ctrl},
	{ },
};

int32_t rj68ja120_fm1449_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;
	LOGI("+%s\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LOGE("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	s_ctrl->sensor_device_type = MSM_SENSOR_I2C_DEVICE;
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		goto probe_failure;
	}

	s_ctrl->sensordata = client->dev.platform_data;

	if (!s_ctrl->wait_num_frames)
		s_ctrl->wait_num_frames = 1 * Q10;

	pr_err("%s %s probe succeeded\n", __func__, client->name);

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);
	s_ctrl->sensor_v4l2_subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->sensor_v4l2_subdev.entity, 0, NULL, 0);
	s_ctrl->sensor_v4l2_subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->sensor_v4l2_subdev.entity.group_id = SENSOR_DEV;
	s_ctrl->sensor_v4l2_subdev.entity.name =
		s_ctrl->sensor_v4l2_subdev.name;
	
	rc = msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	if (rc < 0) {
		LOGE("%s: msm_sensor_register(%d) failed\n", __func__, rc);
		return rc;
	}
	s_ctrl->sensor_v4l2_subdev.entity.revision =
		s_ctrl->sensor_v4l2_subdev.devnode->num;

	LOGD("-%s done.\n", __func__);
	return rc;

probe_failure:
	rc = -EFAULT;
	LOGE("-%s failed(%d)\n", __func__, rc);
	return rc;
}

static struct i2c_driver rj68ja120_fm1449_i2c_driver = {
	.id_table = rj68ja120_fm1449_i2c_id,
	.probe  = rj68ja120_fm1449_i2c_probe,
	.remove = __exit_p(rj68ja120_fm1449_i2c_remove),
	.driver = {
		.name = PLATFORM_DRIVER_NAME,
	},
};

static struct msm_camera_i2c_client rj68ja120_fm1449_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static const struct of_device_id rj68ja120_fm1449_dt_match[] = {
	{.compatible = "qcom,rj68ja120_fm1449", .data = &rj68ja120_fm1449_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, rj68ja120_fm1449_dt_match);

static struct platform_driver rj68ja120_fm1449_platform_driver = {
	.driver = {
		.name = "qcom,rj68ja120_fm1449",
		.owner = THIS_MODULE,
		.of_match_table = rj68ja120_fm1449_dt_match,
	},
};

static int32_t rj68ja120_fm1449_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(rj68ja120_fm1449_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init rj68ja120_fm1449_init(void)
{
	int rc = 0;
//	int i = 0;
	LOGI("+%s\n", __func__);

	rc = platform_driver_probe(&rj68ja120_fm1449_platform_driver,
		rj68ja120_fm1449_platform_probe);
	if (!rc){
		LOGE("%s: platform_driver_probe failed rc(%d)\n", __func__, rc);
//		return rc;
	}

	if (is_use_i2c_driver) {
		rc = i2c_add_driver(&rj68ja120_fm1449_i2c_driver);
		if (rc < 0) {
			rc = -ENOTSUPP;
			LOGE("%s: I2C add driver failed\n", __func__);
			i2c_del_driver(&rj68ja120_fm1449_i2c_driver);
//			return rc;
		}
	}

//	for (i = 0; i < BU1852GXW_CAM_GPIOS_VAL; i++) {
//		LOGD("-%s: GPIO_Exp gpio_requestn and direction_output(%d:%s) ok !\n", __func__, bu1852gxw_cam_gpios[i].gpio, bu1852gxw_cam_gpios[i].name);
//		if (gpio_request(bu1852gxw_cam_gpios[i].gpio, bu1852gxw_cam_gpios[i].name) < 0) {
//			LOGE("%s: * gpio_request(%s) failed !\n", __func__, bu1852gxw_cam_gpios[i].name);
//			goto init_failure;
//		}
//		
//		//GPIO_Exp OUTPUT PULL_Down
//		gpio_direction_output(bu1852gxw_cam_gpios[i].gpio, 0);
//	}
	
	// ICAM_XRST/ICAM_XSTBY GPIO_Exp requestn and direction_output
	if (gpio_request(RESET, "ICAM_XRST") < 0) {
		LOGE("%s: * gpio_request(ICAM_XRST) failed !\n", __func__);
	}
	gpio_direction_output(RESET, 0);
	
	if (gpio_request(CAM_STBY, "ICAM_XSTBY") < 0) {
		LOGE("%s: * gpio_request(ICAM_XSTBY) failed !\n", __func__);
	}
	gpio_direction_output(CAM_STBY, 0);

	return 0;

//init_failure:
//	rc = -EFAULT;
//	LOGE("-%s failed(%d)\n", __func__, rc);
//	return rc;
}

static void __exit rj68ja120_fm1449_exit(void)
{
	if (rj68ja120_fm1449_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&rj68ja120_fm1449_s_ctrl);
		platform_driver_unregister(&rj68ja120_fm1449_platform_driver);
	} else
		i2c_del_driver(&rj68ja120_fm1449_i2c_driver);
	return;
}

int32_t rj68ja120_fm1449_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	return 0; // nop
}

static struct msm_sensor_fn_t rj68ja120_fm1449_func_tbl = {
//	.sensor_start_stream = msm_sensor_start_stream,
//	.sensor_stop_stream = msm_sensor_stop_stream,
//	.sensor_group_hold_on = msm_sensor_group_hold_on,
//	.sensor_group_hold_off = msm_sensor_group_hold_off,
//	.sensor_get_prev_lines_pf = msm_sensor_get_prev_lines_pf,
//	.sensor_get_prev_pixels_pl = msm_sensor_get_prev_pixels_pl,
//	.sensor_get_pict_lines_pf = msm_sensor_get_pict_lines_pf,
//	.sensor_get_pict_pixels_pl = msm_sensor_get_pict_pixels_pl,
//	.sensor_get_pict_max_exp_lc = msm_sensor_get_pict_max_exp_lc,
//	.sensor_get_pict_fps = msm_sensor_get_pict_fps,
//	.sensor_set_fps = msm_sensor_set_fps,
//	.sensor_write_exp_gain = msm_sensor_write_exp_gain1,
//	.sensor_write_snapshot_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_setting = rj68ja120_fm1449_sensor_setting,
	.sensor_set_sensor_mode = rj68ja120_fm1449_set_sensor_mode,
	.sensor_mode_init = rj68ja120_fm1449_sensor_mode_init,
	.sensor_config = rj68ja120_fm1449_sensor_config,
	.sensor_power_up = rj68ja120_fm1449_sensor_power_up,
	.sensor_power_down = rj68ja120_fm1449_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_match_id = rj68ja120_fm1449_sensor_match_id,
};

static struct msm_sensor_reg_t rj68ja120_fm1449_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
//	.start_stream_conf = rj68ja120_fm1449_start_settings,
//	.start_stream_conf_size = ARRAY_SIZE(rj68ja120_fm1449_start_settings),
//	.stop_stream_conf = rj68ja120_fm1449_stop_settings,
//	.stop_stream_conf_size = ARRAY_SIZE(rj68ja120_fm1449_stop_settings),
//	.group_hold_on_conf = rj68ja120_fm1449_groupon_settings,
//	.group_hold_on_conf_size = ARRAY_SIZE(rj68ja120_fm1449_groupon_settings),
//	.group_hold_off_conf = rj68ja120_fm1449_groupoff_settings,
//	.group_hold_off_conf_size =
//		ARRAY_SIZE(rj68ja120_fm1449_groupoff_settings),
//	.init_settings = &rj68ja120_fm1449_init_conf[0],
//	.init_size = ARRAY_SIZE(rj68ja120_fm1449_init_conf),
//	.mode_settings = &rj68ja120_fm1449_confs[0],
//	.output_settings = &rj68ja120_fm1449_dimensions[0],
//	.num_conf = ARRAY_SIZE(rj68ja120_fm1449_confs),
};

static long rj68ja120_fm1449_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	void __user *argp = (void __user *)arg;
	if (s_ctrl->sensor_state == MSM_SENSOR_POWER_DOWN)
		return -EINVAL;
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return s_ctrl->func_tbl->sensor_config(s_ctrl, argp);
	case VIDIOC_MSM_SENSOR_RELEASE:
		return 0;
	case VIDIOC_MSM_SENSOR_CSID_INFO: {
		struct msm_sensor_csi_info *csi_info =
			(struct msm_sensor_csi_info *)arg;
		s_ctrl->is_csic = csi_info->is_csic;
		return 0;
	}
	default:
		return -ENOIOCTLCMD;
	}
}

//static struct v4l2_subdev_core_ops rj68ja120_fm1449_subdev_core_ops;
static struct v4l2_subdev_core_ops rj68ja120_fm1449_subdev_core_ops = {
	.ioctl = rj68ja120_fm1449_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};
static struct v4l2_subdev_video_ops rj68ja120_fm1449_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops rj68ja120_fm1449_subdev_ops = {
	.core = &rj68ja120_fm1449_subdev_core_ops,
	.video  = &rj68ja120_fm1449_subdev_video_ops,
};

static struct v4l2_subdev_info rj68ja120_fm1449_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_ctrl_t rj68ja120_fm1449_s_ctrl = {
	.msm_sensor_reg = &rj68ja120_fm1449_regs,
	.sensor_i2c_client = &rj68ja120_fm1449_sensor_i2c_client,
	.sensor_i2c_addr = CAM_I2C_ADDRESS,
	.vreg_seq = NULL,
	.num_vreg_seq = 0,
//	.sensor_output_reg_addr = &rj68ja120_fm1449_reg_addr,
//	.sensor_id_info = &rj68ja120_fm1449_id_info,
//	.sensor_exp_gain_info = &rj68ja120_fm1449_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.wait_num_frames = 0,
	.min_delay = 0,
	.msm_sensor_mutex = &rj68ja120_fm1449_mtx,
	.sensor_i2c_driver = &rj68ja120_fm1449_i2c_driver,
	.sensor_v4l2_subdev_info = rj68ja120_fm1449_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(rj68ja120_fm1449_subdev_info),
	.sensor_v4l2_subdev_ops = &rj68ja120_fm1449_subdev_ops,
	.func_tbl = &rj68ja120_fm1449_func_tbl,
};

module_init(rj68ja120_fm1449_init);
module_exit(rj68ja120_fm1449_exit);
MODULE_DESCRIPTION("Sony 13M sensor driver");
MODULE_LICENSE("GPL v2");
