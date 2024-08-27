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
#ifndef __BU26507GUL_IF__
#define __BU26507GUL_IF__

/* define */
#define SHIFT_BU26507GUL_BRIGHTNESS_VALUE		4
#define MASK_BU26507GUL_BRIGHTNESS_VALUE		0x00ff

/* prot type */
extern ssize_t bu26507gul_if_sloop_cycle(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size);
extern ssize_t bu26507gul_if_sloop_delay(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size);
extern void bu26507gul_if_brightness(struct led_classdev *led_cdev, enum led_brightness value);
extern char *param_strtok_r(char *str, const char *delim, char **saveptr);
extern ssize_t bu26507gul_if_all(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size);
extern ssize_t bu26507gul_if_pwmset(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size);
extern ssize_t bu26507gul_if_slp(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size);
extern ssize_t bu26507gul_if_start(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size);
extern ssize_t bu26507gul_if_iab(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size);

extern int bu26507gul_if_init(void);
extern int bu26507gul_if_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int bu26507gul_if_suspend(struct device *dev);
extern int bu26507gul_if_resume(struct device *dev);
extern int bu26507gul_if_remove(struct i2c_client *client);
extern int bu26507gul_if_shutdown(struct i2c_client *client);
extern int bu26507gul_if_exit(void);

#endif /* __BU26507GUL_IF__ */
