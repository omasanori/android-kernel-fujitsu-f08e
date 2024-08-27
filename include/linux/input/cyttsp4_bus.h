/*
 * cyttsp4_bus.h
 * Cypress TrueTouch(TM) Standard Product V4 Bus Driver.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modified by: Cypress Semiconductor to add device functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_BUS_H
#define _LINUX_CYTTSP4_BUS_H

#include <linux/device.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/limits.h>


extern struct bus_type cyttsp4_bus_type;

struct cyttsp4_driver;
struct cyttsp4_device;
struct cyttsp4_adapter;

enum cyttsp4_atten_type {
	CY_ATTEN_IRQ,
	CY_ATTEN_STARTUP,
	CY_ATTEN_EXCLUSIVE,
	CY_ATTEN_WAKE,
	CY_ATTEN_NUM_ATTEN,
};

/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL end start */
enum cyttsp4_powermode_type {
/* for cyttsp4_set_powermode */
	 CY_POWMODE_SUSPEND				= (int)0	/* Suspend(DeepSleep) */
	,CY_POWMODE_RESUME							/* Resume(WakeUp)     */
	,CY_POWMODE_HARDRESET						/* HardReset          */
	,CY_POWMODE_SOFTRESET						/* SoftReset          */
};

enum cyttsp4_information_type {
/* for cyttsp4_request_get_information */
	 CY_INFOTYPE_IS_PROBE_ERR		= (int)0	/* probe error		*/	/* (enum cyttsp4_information_state *)	*/
	,CY_INFOTYPE_IS_CORE_SLEEP					/* sleep state		*/	/* (enum cyttsp4_information_state *)	*/
	,CY_INFOTYPE_PRESS_CTRL_INFO				/* press ctrl info	*/	/* (u8*)[8]								*/
};

enum cyttsp4_information_option {
/* for cyttsp4_request_get_information */
	CY_INFOOPT_NONE					= (int)0	/* OPT None			*/
};

enum cyttsp4_information_state {
/* for cyttsp4_request_get_information */
	/* common state */
	 CY_INFOSTATE_FALSE				= (int)0	/* FALSE		*/
	,CY_INFOSTATE_TRUE				= (int)1	/* TRUE			*/
	,CY_INFOSTATE_INIT				= (int)-1	/* INIT Value	*/ /* FUJITSU:2013-05-17 HMW_TOUCH_RUNTIME_CONFIG add */
};
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL add end */

typedef int (*cyttsp4_atten_func) (struct cyttsp4_device *);

struct cyttsp4_ops {
	int (*write)(struct cyttsp4_adapter *dev, u16 addr,
		const void *buf, int size, int max_xfer);
	int (*read)(struct cyttsp4_adapter *dev, u16 addr, void *buf,
		int size, int max_xfer);
};

struct cyttsp4_adapter {
	struct list_head node;
	char id[NAME_MAX];
	struct device *dev;
	int (*write)(struct cyttsp4_adapter *dev, u16 addr,
		const void *buf, int size, int max_xfer);
	int (*read)(struct cyttsp4_adapter *dev, u16 addr, void *buf,
		int size, int max_xfer);
};
#define to_cyttsp4_adapter(d) container_of(d, struct cyttsp4_adapter, dev)

struct cyttsp4_core_info {
	char const *name;
	char const *id;
	char const *adap_id;
	void *platform_data;
};

struct cyttsp4_core {
	struct list_head node;
	char const *name;
	char const *id;
	char const *adap_id;
	struct device dev;
	struct cyttsp4_adapter *adap;
};
#define to_cyttsp4_core(d) container_of(d, struct cyttsp4_core, dev)

struct cyttsp4_device_info {
	char const *name;
	char const *core_id;
	void *platform_data;
};

struct cyttsp4_device {
	struct list_head node;
	char const *name;
	char const *core_id;
	struct device dev;
	struct cyttsp4_core *core;
};
#define to_cyttsp4_device(d) container_of(d, struct cyttsp4_device, dev)

/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add start */
struct cyttsp4_touch_hardware_info {
	u32 firmware_version;
	u16 config_crc;
	u8  panel_maker_code;
	u8  lcd_maker_code;
	u8  lcd_maker_id;
	u8  model_type;
	u8  trial_type;
/* FUJITSU:2013-05-28 HMW_TOUCH_FWVER add start */
	u32 config_version;
	u8  project_id;			/* HMW:0x0B... */
/* FUJITSU:2013-05-28 HMW_TOUCH_FWVER add end */
};
/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add end */
/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add start */
#define		CY_TOUCH_DEBUG_FLAGS			16
#define		CY_TOUCH_SET_PARM_FLAGS			8
#define		CY_TOUCH_CONFIG_PARM_VALUE		64
#define		CY_TOUCH_PRESS_NOISE_PARM		8			/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add */

struct cyttsp4_nonvolatile_info {
	u8		debug_i[ CY_TOUCH_DEBUG_FLAGS ];
	u8		set_config_param[ CY_TOUCH_SET_PARM_FLAGS ];
	u8		config_param_value[ CY_TOUCH_CONFIG_PARM_VALUE ];
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add start */
	u8		press_noise_param[ CY_TOUCH_PRESS_NOISE_PARM ];
/* FUJITSU:2013-07-05 HMW_TOUCH_PRESS_FREQ_CHG add end */

	/* 0 is default value */
	/* debug_i */
	u8		touch_disable;						/* 0 : enable, 1 : disable							*/
	u8		power_on_calibration;				/* 1 : do pow on calib,					0 : off 	*/
	u8		cyttsp_debug_tools;					/* 1 : cypress debug tool (on),			0 : off		*/
												/*		OLD : set_cyttsp_debug_tools				*/
	u8		recovery_led;						/* 1 : do flash led by reset,			0 : off		*/
												/*		OLD : light_at_TPreset						*/
	u8		not_write_config_parm;				/* 1 : not write config param, 			0 : off		*/
												/*		OLD : write_config_params_onoff				*/
	u8		not_use_charger_armor;				/* 1 : not use charger armor,			0 : off		*/
												/*		OLD : ChargerArmor_disable					*/
	u8		saku2_log;							/* 1 : enable saku2 log,				0 : off		*/
												/* 		OLD : log_saku2								*/
	u8		saku2_led;							/* 1 : enable saku2 LED,				0 : off		*/
												/*		OLD : light_at_Event						*/
	u8		saku2_trace_printk;					/* 1 : enable saku2 tarece printk,		0 : off		*/
												/*		OLD : ena_trace_printk						*/
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add start */
	u8		data_io_debug;						/* 1 : execute Data I/O Debug Routine,	0 : off		*/
	u8		i2c_log;							/* 1 : i2c log enable,					0 : off		*/		/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_LOG add */

/* for press sensor */
	u8		press_normalize;					/* 1 : press normalization,				0 : off		*/
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add start */
	u8		press_resume_force_calib;			/* 1 : resume force calibration,		0 : off		*/
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add end */
};
/* FUJITSU:2013-04-22 HMW_TTDA_MODIFY_LOG add end */

struct cyttsp4_core_driver {
	struct device_driver driver;
	int (*probe)(struct cyttsp4_core *core);
	int (*remove)(struct cyttsp4_core *core);
	int (*subscribe_attention)(struct cyttsp4_device *ttsp,
				enum cyttsp4_atten_type type,
				cyttsp4_atten_func func,
				int flags);
	int (*unsubscribe_attention)(struct cyttsp4_device *ttsp,
				enum cyttsp4_atten_type type,
				cyttsp4_atten_func func,
				int flags);
	int (*request_exclusive)(struct cyttsp4_device *ttsp, int timeout_ms);
	int (*release_exclusive)(struct cyttsp4_device *ttsp);
	int (*request_reset)(struct cyttsp4_device *ttsp);
	int (*request_restart)(struct cyttsp4_device *ttsp, bool wait);
	int (*request_set_mode)(struct cyttsp4_device *ttsp, int mode);
	struct cyttsp4_sysinfo *(*request_sysinfo)(struct cyttsp4_device *ttsp);
	struct cyttsp4_loader_platform_data
		*(*request_loader_pdata)(struct cyttsp4_device *ttsp);
	int (*request_handshake)(struct cyttsp4_device *ttsp, u8 mode);
	int (*request_exec_cmd)(struct cyttsp4_device *ttsp, u8 mode,
			u8 *cmd_buf, size_t cmd_size, u8 *return_buf,
			size_t return_buf_size, int timeout_ms);
	int (*request_stop_wd)(struct cyttsp4_device *ttsp);
	int (*request_toggle_lowpower)(struct cyttsp4_device *ttsp, u8 mode);
	int (*request_config_row_size)(struct cyttsp4_device *ttsp,
			u16 *config_row_size);
	int (*request_write_config)(struct cyttsp4_device *ttsp, u8 ebid,
			u16 offset, u8 *data, u16 length);
	int (*request_enable_scan_type)(struct cyttsp4_device *ttsp,
			u8 scan_type);
	int (*request_disable_scan_type)(struct cyttsp4_device *ttsp,
			u8 scan_type);
	const u8 *(*get_security_key)(struct cyttsp4_device *ttsp, int *size);
	void (*get_touch_record)(struct cyttsp4_device *ttsp, int rec_no,
			int *rec_abs);
/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add start */
	int (*request_hardware_info)(struct cyttsp4_device *ttsp, struct cyttsp4_touch_hardware_info *hi);
/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL add start */
	int (*request_direct_write)(struct cyttsp4_device *ttsp, u16 addr, const void *buf, int size);
	int (*request_direct_read)(struct cyttsp4_device *ttsp, u16 addr, void *buf, int size);
	int (*request_set_powermode)(struct cyttsp4_device *ttsp, enum cyttsp4_powermode_type type);
	int (*request_get_information)(struct cyttsp4_device *ttsp, enum cyttsp4_information_type type, enum cyttsp4_information_option option, void *info);
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL add end */
/* FUJITSU:2013-04-23 HMW_TTDA_PRESS add start */
	int (*request_set_pressure_ctrl)(struct cyttsp4_device *ttsp, bool IsSet, enum cyttsp4_information_state *info);
/* FUJITSU:2013-04-23 HMW_TTDA_PRESS add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add start */
	int (*request_set_normalize_mode)(struct cyttsp4_device *ttsp, bool IsSet, enum cyttsp4_information_state *info);
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add end */
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add start */
	int (*request_set_ioctl_handle)(struct cyttsp4_device *ttsp, void *ioctl);
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_RUNTIME_CONFIG add start */
	int (*request_set_runtime_parm)(struct cyttsp4_device *ttsp, u8 param_id, u8 param_size, u32 param_value);
	int (*request_get_runtime_parm)(struct cyttsp4_device *ttsp, u8 param_id, u32 *param_value);
/* FUJITSU:2013-05-17 HMW_TOUCH_RUNTIME_CONFIG add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add start */
	int (*request_do_force_calib)(struct cyttsp4_device *ttsp, enum cyttsp4_information_option option);
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add end */
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add start */
	int (*request_is_support)(struct cyttsp4_device *ttsp, int check_type);
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add end */
	int (*write)(struct cyttsp4_device *ttsp, int mode,
		u16 addr, const void *buf, int size);
	int (*read)(struct cyttsp4_device *ttsp, int mode,
		u16 addr, void *buf, int size);
};
#define to_cyttsp4_core_driver(d) \
	container_of(d, struct cyttsp4_core_driver, driver)

struct cyttsp4_driver {
	struct device_driver driver;
	int (*probe)(struct cyttsp4_device *dev);
	int (*remove)(struct cyttsp4_device *fev);
};
#define to_cyttsp4_driver(d) container_of(d, struct cyttsp4_driver, driver)

extern int cyttsp4_register_driver(struct cyttsp4_driver *drv);
extern void cyttsp4_unregister_driver(struct cyttsp4_driver *drv);

extern int cyttsp4_register_core_driver(struct cyttsp4_core_driver *drv);
extern void cyttsp4_unregister_core_driver(struct cyttsp4_core_driver *drv);

extern int cyttsp4_register_device(struct cyttsp4_device_info const *dev_info);
extern int cyttsp4_unregister_device(char const *name, char const *core_id);

extern int cyttsp4_register_core_device(
		struct cyttsp4_core_info const *core_info);

extern int cyttsp4_add_adapter(char const *id, struct cyttsp4_ops const *ops,
		struct device *parent);

extern int cyttsp4_del_adapter(char const *id);

static inline int cyttsp4_read(struct cyttsp4_device *ttsp, int mode, u16 addr,
		void *buf, int size)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->read(ttsp, mode, addr, buf, size);
}

static inline int cyttsp4_write(struct cyttsp4_device *ttsp, int mode, u16 addr,
		const void *buf, int size)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->write(ttsp, mode, addr, buf, size);
}

static inline int cyttsp4_subscribe_attention(struct cyttsp4_device *ttsp,
		enum cyttsp4_atten_type type, cyttsp4_atten_func func,
		int flags)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->subscribe_attention(ttsp, type, func, flags);
}

static inline int cyttsp4_unsubscribe_attention(struct cyttsp4_device *ttsp,
		enum cyttsp4_atten_type type, cyttsp4_atten_func func,
		int flags)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->unsubscribe_attention(ttsp, type, func, flags);
}

static inline int cyttsp4_request_exclusive(struct cyttsp4_device *ttsp,
		int timeout_ms)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_exclusive(ttsp, timeout_ms);
}

static inline int cyttsp4_release_exclusive(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->release_exclusive(ttsp);
}

static inline int cyttsp4_request_reset(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_reset(ttsp);
}

static inline int cyttsp4_request_restart(struct cyttsp4_device *ttsp,
		bool wait)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_restart(ttsp, wait);
}

static inline int cyttsp4_request_set_mode(struct cyttsp4_device *ttsp,
		int mode)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_set_mode(ttsp, mode);
}

static inline struct cyttsp4_sysinfo *cyttsp4_request_sysinfo(
		struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_sysinfo(ttsp);
}

static inline struct cyttsp4_loader_platform_data *cyttsp4_request_loader_pdata(
		struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_loader_pdata(ttsp);
}

static inline int cyttsp4_request_handshake(struct cyttsp4_device *ttsp,
		u8 mode)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_handshake(ttsp, mode);
}

static inline int cyttsp4_request_exec_cmd(struct cyttsp4_device *ttsp,
		u8 mode, u8 *cmd_buf, size_t cmd_size, u8 *return_buf,
		size_t return_buf_size, int timeout_ms)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_exec_cmd(ttsp, mode, cmd_buf, cmd_size, return_buf,
			return_buf_size, timeout_ms);
}

static inline int cyttsp4_request_stop_wd(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_stop_wd(ttsp);
}

static inline int cyttsp4_request_toggle_lowpower(struct cyttsp4_device *ttsp,
		u8 mode)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_toggle_lowpower(ttsp, mode);
}

static inline int cyttsp4_request_config_row_size(struct cyttsp4_device *ttsp,
		u16 *config_row_size)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_config_row_size(ttsp, config_row_size);
}

static inline int cyttsp4_request_write_config(struct cyttsp4_device *ttsp,
		u8 ebid, u16 offset, u8 *data, u16 length)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_write_config(ttsp, ebid, offset, data, length);
}

static inline int cyttsp4_request_enable_scan_type(struct cyttsp4_device *ttsp,
		u8 scan_type)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_enable_scan_type(ttsp, scan_type);
}

static inline int cyttsp4_request_disable_scan_type(struct cyttsp4_device *ttsp,
		u8 scan_type)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_disable_scan_type(ttsp, scan_type);
}

static inline const u8 *cyttsp4_get_security_key(struct cyttsp4_device *ttsp,
		int *size)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->get_security_key(ttsp, size);
}

static inline void cyttsp4_get_touch_record(struct cyttsp4_device *ttsp,
		int rec_no, int *rec_abs)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	d->get_touch_record(ttsp, rec_no, rec_abs);
}
/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add start */
static inline int cyttsp4_request_hardware_info(struct cyttsp4_device *ttsp, struct cyttsp4_touch_hardware_info *hi)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_hardware_info(ttsp, hi);
}
/* FUJITSU:2013-04-02 HMW_MULTI_VENDER add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL add start */
static inline int cyttsp4_request_direct_read(struct cyttsp4_device *ttsp, u16 addr, void *buf, int size)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_direct_read(ttsp, addr, buf, size);
}
static inline int cyttsp4_request_direct_write(struct cyttsp4_device *ttsp, u16 addr, void *buf, int size)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_direct_write(ttsp, addr, buf, size);
}
static inline int cyttsp4_request_set_powermode(struct cyttsp4_device *ttsp, enum cyttsp4_powermode_type type)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_set_powermode(ttsp, type);
}
static inline int cyttsp4_request_get_information(struct cyttsp4_device *ttsp, enum cyttsp4_information_type type, enum cyttsp4_information_option option, void *info)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_get_information(ttsp, type, option, info);
}
/* FUJITSU:2013-05-17 HMW_TOUCH_IOCTL add end */
/* FUJITSU:2013-05-01 HMW_TTDA_MODIFY_LOG mod start */
extern struct cyttsp4_nonvolatile_info		cy_nv_info;
static inline struct cyttsp4_nonvolatile_info *cyttsp4_request_ref_nonvolatile_info(struct cyttsp4_device *ttsp)
{
	/* see fj_touch_noracs.h */
	return &cy_nv_info;
}
/* FUJITSU:2013-05-01 HMW_TTDA_MODIFY_LOG mod end */
/* FUJITSU:2013-04-23 HMW_TTDA_PRESS add start */
static inline int cyttsp4_request_set_pressure_ctrl(struct cyttsp4_device *ttsp, bool IsSet, enum cyttsp4_information_state *info)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_set_pressure_ctrl(ttsp, IsSet, info);
}
/* FUJITSU:2013-04-23 HMW_TTDA_PRESS add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add start */
static inline int cyttsp4_request_set_normalize(struct cyttsp4_device *ttsp, bool IsSet, enum cyttsp4_information_state *info)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);

	return d->request_set_normalize_mode(ttsp, IsSet, info);
}
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_NORMALIZE_CORE add end */
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add start */
static inline int cyttsp4_request_set_ioctl_handle(struct cyttsp4_device *ttsp, void *ioctl)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_set_ioctl_handle(ttsp, ioctl);
}
/* FUJITSU:2013-05-07 HMW_TTDA_RUNTIME_CONFIG add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_RUNTIME_CONFIG add start */
static inline int cyttsp4_request_set_runtime_parm(struct cyttsp4_device *ttsp, u8 param_id, u8 param_size, u32 param_value)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_set_runtime_parm(ttsp, param_id, param_size, param_value);
}

static inline int cyttsp4_request_get_runtime_parm(struct cyttsp4_device *ttsp, u8 param_id, u32 *param_value)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);
	return d->request_get_runtime_parm(ttsp, param_id, param_value);
}
/* FUJITSU:2013-05-17 HMW_TOUCH_RUNTIME_CONFIG add end */
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add start */
static inline int cyttsp4_request_do_force_calib(struct cyttsp4_device *ttsp, enum cyttsp4_information_option option)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);

	return d->request_do_force_calib(ttsp, option);
}
/* FUJITSU:2013-05-17 HMW_TOUCH_PRESS_FORCE_CALIB add end */
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add start */
static inline int cyttsp4_request_is_support(struct cyttsp4_device *ttsp, int check_type)
{
	struct cyttsp4_core *cd = ttsp->core;
	struct cyttsp4_core_driver *d = to_cyttsp4_core_driver(cd->dev.driver);

	return d->request_is_support(ttsp, check_type);
}
/* FUJITSU:2013-05-28 HMW_TOUCH_MULTI_MODEL add end */

#endif /* _LINUX_CYTTSP4_BUS_H */
