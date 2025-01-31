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
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/

#ifndef _WCNSS_WLAN_H_
#define _WCNSS_WLAN_H_

#include <linux/device.h>

enum wcnss_opcode {
	WCNSS_WLAN_SWITCH_OFF = 0,
	WCNSS_WLAN_SWITCH_ON,
};

struct wcnss_wlan_config {
	int		use_48mhz_xo;
};

#define WCNSS_WLAN_IRQ_INVALID -1
#define HAVE_WCNSS_RESET_INTR 1

struct device *wcnss_wlan_get_device(void);
struct resource *wcnss_wlan_get_memory_map(struct device *dev);
int wcnss_wlan_get_dxe_tx_irq(struct device *dev);
int wcnss_wlan_get_dxe_rx_irq(struct device *dev);
void wcnss_wlan_register_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops);
void wcnss_wlan_unregister_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops);
void wcnss_register_thermal_mitigation(struct device *dev,
				void (*tm_notify)(struct device *dev, int));
void wcnss_unregister_thermal_mitigation(
				void (*tm_notify)(struct device *dev, int));
struct platform_device *wcnss_get_platform_device(void);
struct wcnss_wlan_config *wcnss_get_wlan_config(void);
int wcnss_wlan_power(struct device *dev,
				struct wcnss_wlan_config *cfg,
				enum wcnss_opcode opcode);
int req_riva_power_on_lock(char *driver_name);
int free_riva_power_on_lock(char *driver_name);
unsigned int wcnss_get_serial_number(void);
void wcnss_flush_delayed_boot_votes(void);
void wcnss_allow_suspend(void);
void wcnss_prevent_suspend(void);
void wcnss_ssr_boot_notify(void);
void wcnss_reset_intr(void);
int wcnss_cold_boot_done(void);

#define wcnss_wlan_get_drvdata(dev) dev_get_drvdata(dev)
#define wcnss_wlan_set_drvdata(dev, data) dev_set_drvdata((dev), (data))
/* FUJITSU:2013-02-21 add start */
int fj_wlan_fem_power_on(void);
int fj_wlan_fem_power_off(void);
/* FUJITSU:2013-02-21 add end */

#endif /* _WCNSS_WLAN_H_ */
