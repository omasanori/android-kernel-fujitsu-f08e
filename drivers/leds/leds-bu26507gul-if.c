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
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/leds-bu26507gul.h>
#include "leds-bu26507gul-init.h"
#include "leds-bu26507gul-if.h"
#include "leds-bu26507gul-sw.h"

/* Global parameter*/
static struct i2c_client *this_client;
static struct rw_semaphore bu26507gul_lock;

static struct bu26507gul_led 			led[BU26507GUL_NUM_LEDS];
static struct bu26507gul_illumination	illumination_led;
static	struct i2c_client				*p_bu26507gul_i2ccli;
static	const struct i2c_device_id		*p_bu26507gul_i2cid;

ssize_t bu26507gul_if_sloop_cycle(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bu26507gul_led *led_data = container_of(led_cdev, struct bu26507gul_led, cdev);
	int ret = -EINVAL;

	int p_ledno = led_data->id;
	int p_scyc = (int)simple_strtoul(buf, NULL, 10);
	
	illumination_info2_printk("[%s] param=(%s)\n",__func__,buf);
	
	down_write(&bu26507gul_lock);
	
	ret = bu26507gul_sw_sloop_cycle(p_ledno,p_scyc);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_sloop_cycle ret=%d \n",__func__,ret);
	} else {
		ret = size;
	}

	up_write(&bu26507gul_lock);
	return ret;
}

ssize_t bu26507gul_if_sloop_delay(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bu26507gul_led *led_data = container_of(led_cdev, struct bu26507gul_led, cdev);
	int ret = -EINVAL;

	int p_ledno = led_data->id;
	int p_sdly = (int)simple_strtoul(buf, NULL, 10);
	
	illumination_info2_printk("[%s] param=(%s)\n",__func__,buf);

	down_write(&bu26507gul_lock);
	ret = bu26507gul_sw_sloop_delay(p_ledno,p_sdly);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_sloop_delay ret=%d \n",__func__,ret);
	} else {
		ret = size;
	}
	up_write(&bu26507gul_lock);

	return ret;
}

void bu26507gul_if_brightness(struct led_classdev *led_cdev, enum led_brightness value)
{
	int ret;
	struct bu26507gul_led *led_data = container_of(led_cdev, struct bu26507gul_led, cdev);
	
	int p_ledno = led_data->id;
	int p_ledelec = (value  & MASK_BU26507GUL_BRIGHTNESS_VALUE);
	p_ledelec = p_ledelec >> SHIFT_BU26507GUL_BRIGHTNESS_VALUE;

	illumination_info2_printk("[%s] param=(%x)\n",__func__,value);
	
	down_write(&bu26507gul_lock);
	ret = bu26507gul_sw_brightness(p_ledno,p_ledelec);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_brightness ret=%d \n",__func__,ret);
	}
	up_write(&bu26507gul_lock);
}

static void bu26507gul_if_brightness_dummy(struct led_classdev *led_cdev, enum led_brightness value)
{
}

char *param_strtok_r(char *str, const char *delim, char **saveptr)
{
    char *s = str ? str : *saveptr;
    char *p;

	illumination_info_printk("[%s] \n",__func__);
    while (*s && (strchr(delim, *s) != NULL)) {
        s++;
    }
    p = s;

    while (*p && (strchr(delim, *p) == NULL)) {
        p++;
    }

    if (*p) {
        *saveptr = p + 1;
        *p = '\0';
    } else {
        *saveptr = p;
    }
    return s;
}

ssize_t bu26507gul_if_all(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct bu26507gul_led *led_data = container_of(led_cdev, struct bu26507gul_led, cdev);
	int ret = -EINVAL;

	int p_ledno = led_data->id;
	int p_scyc,p_sdly,p_ledelec;

    const char *delim = ",";
    char *s;
    char *p;
	char a_param[256];
	
	illumination_info2_printk("[%s] param=(%s)\n",__func__,buf);
    strcpy(a_param,buf);
    s = param_strtok_r(a_param, delim, &p);
    
    if (unlikely( s == NULL)) {
		illumination_error_printk("[%s] param error 1 !! \n",__func__);
		return ret;
    }
    p_scyc = (int)simple_strtoul(s, NULL, 10);
    
    s = param_strtok_r(NULL, delim, &p);
    if (unlikely( s == NULL)) {
		illumination_error_printk("[%s] param error 2 !! \n",__func__);
		return ret;
    }
    p_sdly = (int)simple_strtoul(s, NULL, 10);

    s = param_strtok_r(NULL, delim, &p);
    if (unlikely( s == NULL)) {
		illumination_error_printk("[%s] param error 3 !! \n",__func__);
		return ret;
    }
    p_ledelec = (int)simple_strtoul(s, NULL, 10);
	
	down_write(&bu26507gul_lock);
	ret = bu26507gul_sw_all(p_ledno,p_ledelec,p_sdly,p_scyc);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_all ret=%d \n",__func__,ret);
	} else {
		ret = size;
	}
	up_write(&bu26507gul_lock);

	return ret;
}

ssize_t bu26507gul_if_pwmset(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size)
{
	int ret = -EINVAL;
	int p_pwmduty = (int)simple_strtoul(buf, NULL, 10);
	
	illumination_info2_printk("[%s] param=(%s)\n",__func__,buf);

	down_write(&bu26507gul_lock);
	ret = bu26507gul_sw_pwmset(p_pwmduty);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_pwmset ret=%d \n",__func__,ret);
	} else {
		ret = size;
	}
	up_write(&bu26507gul_lock);

	return ret;
}

ssize_t bu26507gul_if_slp(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size)
{
	int ret = -EINVAL;
	int p_slp = (int)simple_strtoul(buf, NULL, 10);
	
	illumination_info2_printk("[%s] param=(%s)\n",__func__,buf);

	down_write(&bu26507gul_lock);
	ret = bu26507gul_sw_slp(p_slp);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_slp ret=%d \n",__func__,ret);
	} else {
		ret = size;
	}
	up_write(&bu26507gul_lock);

	return ret;
}

ssize_t bu26507gul_if_start(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size)
{
	int ret = -EINVAL;
	int p_start,p_disp_ab;

    const char *delim = ",";
    char *s;
    char *p;
	char a_param[256];
	
	illumination_info2_printk("[%s] param=(%s)\n",__func__,buf);
    strcpy(a_param,buf);
    s = param_strtok_r(a_param, delim, &p);
    
    if (unlikely( s == NULL)) {
		illumination_error_printk("[%s] param error 1 !! \n",__func__);
		return ret;
    }
    p_start = (int)simple_strtoul(s, NULL, 10);
    
    s = param_strtok_r(NULL, delim, &p);
    if (unlikely( *s == '-')) {
    	p_disp_ab = -(int)simple_strtoul(s+1, NULL, 10);
    } else {
    	p_disp_ab = (int)simple_strtoul(s, NULL, 10);
    }

	down_write(&bu26507gul_lock);
	ret = bu26507gul_sw_start(p_start,p_disp_ab);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_start ret=%d \n",__func__,ret);
	} else {
		ret = size;
	}
	up_write(&bu26507gul_lock);

	return ret;
}

ssize_t bu26507gul_if_iab(struct device *dev, struct device_attribute *attrs, const char *buf, size_t size)
{
	int ret = -EINVAL;
	int p_write_ab = (int)simple_strtoul(buf, NULL, 10);
	
	illumination_info2_printk("[%s] param=(%s)\n",__func__,buf);

	down_write(&bu26507gul_lock);
	ret = bu26507gul_sw_iab(p_write_ab);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_iab ret=%d \n",__func__,ret);
	} else {
		ret = size;
	}
	up_write(&bu26507gul_lock);

	return ret;
}

int bu26507gul_if_init(void)
{
	int ret = 0;
	
	illumination_info_printk("[%s] \n",__func__);

	memset(&illumination_led,0,sizeof(illumination_led));
	memset(&led[0],0,sizeof(led));
	
	init_rwsem(&bu26507gul_lock);
	
	ret = bu26507gul_sw_init();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_init ret=%d \n",__func__,ret);
		return ret;
	}

	return ret;
}

static inline int __devinit bu26507gul_led_device_create_files(struct device *dev, const struct device_attribute *attrs)
{
	int ret = 0;
	int i;

	for (i = 0 ; attrs[i].attr.name != NULL ; i++) {
		ret = device_create_file(dev, &attrs[i]);
		if (unlikely(ret != 0)) {
			illumination_error_printk("[%s] failure device_create_file \"%s\"\n", __func__, attrs[i].attr.name);
			while (--i>=0) {
				device_remove_file(dev, &attrs[i]);
			}
			break;
		}
	}
	return ret;
}

static inline void bu26507gul_led_device_remove_files(struct device *dev, const struct device_attribute *attrs)
{
	for ( ; attrs->attr.name != NULL ; attrs++) {
		device_remove_file(dev, attrs);
	}
}

/*-------------------------------------------------------------------------------------------------------*/

static const struct device_attribute led_led_attrs[] = {
	__ATTR(sloopcycle, 	0644, NULL, bu26507gul_if_sloop_cycle),
	__ATTR(sloopdelay, 	0644, NULL, bu26507gul_if_sloop_delay),
	__ATTR(all, 		0644, NULL, bu26507gul_if_all),
	__ATTR_NULL,
};

/*-------------------------------------------------------------------------------------------------------*/
static const struct device_attribute illumination_led_attrs[] = {
	__ATTR(pwmset, 	0644, NULL, bu26507gul_if_pwmset),
	__ATTR(slp, 	0644, NULL, bu26507gul_if_slp),
	__ATTR(start, 	0644, NULL, bu26507gul_if_start),
	__ATTR(iab, 	0644, NULL, bu26507gul_if_iab),
	__ATTR_NULL,
};
/*-------------------------------------------------------------------------------------------------------*/

int bu26507gul_if_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bu26507gul_led_platform_data *pdata = client->dev.platform_data;
	struct bu26507gul_led 			*led_dat;
	struct led_info *cur_led;

	int ret = 0;
	int i;
	
	illumination_info_printk("[%s] \n",__func__);

	/* Check pdata, start */
	if(pdata == NULL) {
		illumination_error_printk("[%s] : missing platform data! \n",__func__);
		return -ENODEV;
	}

	if(pdata->num_leds != BU26507GUL_NUM_LEDS) {
		illumination_error_printk("[%s] : pdata leds num not match! \n",__func__);
	}
	/* Check pdata, end */

	this_client = client;

	/*----------------------------------------------------------------------------------*/
	illumination_led.cdev.name = "illumination";
	illumination_led.cdev.brightness = LED_OFF;
	illumination_led.cdev.brightness_set = bu26507gul_if_brightness_dummy;
	ret = led_classdev_register(&client->dev, &illumination_led.cdev);
	if(ret < 0) {
		illumination_error_printk("[%s] :(%d) led_classdev_register() error! (illumination_led) \n",__func__,ret);
		goto illumination_err1;
	}

	ret = bu26507gul_led_device_create_files(illumination_led.cdev.dev, illumination_led_attrs);
	if(ret <0) {
		illumination_error_printk("[%s] :(%d) bu26507gul_led_device_create_files() error! (illumination_led) \n",__func__,ret);
		goto illumination_err;
	}

	/*----------------------------------------------------------------------------------*/

	for(i=0 ; i<pdata->num_leds ; i++) {
		cur_led = &pdata->leds[i];	/* Defined leds in the board-f12-tato2.c */
		led_dat = &led[i];

		led_dat->cdev.name = cur_led->name;
		led_dat->cdev.flags = cur_led->flags;
		led_dat->cdev.brightness = LED_OFF;
		led_dat->cdev.max_brightness = LED_FULL;
		/* Set up led brightness or return led brightness */
		led_dat->cdev.brightness_set = bu26507gul_if_brightness;
		led_dat->cdev.brightness_get = NULL;

		led_dat->id = BU26507GUL_SW1LED1 + i;
		led_dat->master = client->dev.parent;
		led_dat->new_brightness = LED_OFF;

		ret = led_classdev_register(led_dat->master, &led_dat->cdev);
		if(ret < 0) {
			illumination_error_printk("[%s] :(%d) led_classdev_register() error! (LED %d, name : %s) \n",__func__,ret,led_dat->id, led_dat->cdev.name);
			goto err;
		}

		ret = bu26507gul_led_device_create_files(led_dat->cdev.dev, led_led_attrs);
		if(ret <0) {
			illumination_error_printk("[%s] :(%d) bu26507gul_led_device_create_files() error! (LED %d, name : %s) \n",__func__,ret,led_dat->id, led_dat->cdev.name);
			goto err;
		}
	}

	i2c_set_clientdata(client, led);

	p_bu26507gul_i2cid = id;
	p_bu26507gul_i2ccli = client;

	ret = bu26507gul_sw_probe(client, id);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_probe ret=%d \n",__func__,ret);
		return ret;
	}

	return ret;

err:
	if(i > 0) {
		for(i=i-1 ; i>=0 ; i--) {
			bu26507gul_led_device_remove_files(led[i].cdev.dev, led_led_attrs);
		}
	}

	bu26507gul_led_device_remove_files(illumination_led.cdev.dev, illumination_led_attrs);

illumination_err:
	led_classdev_unregister(&illumination_led.cdev);

illumination_err1:
	return ret;
}

int bu26507gul_if_suspend(struct device *dev)
{
	int ret;

	illumination_info_printk("[%s] \n",__func__);

	ret = bu26507gul_sw_suspend(dev);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_suspend ret=%d \n",__func__,ret);
		return ret;
	}
	ret = bu26507gul_sw_remove(p_bu26507gul_i2ccli);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_remove ret=%d \n",__func__,ret);
		return ret;
	}
	return ret;
}

int bu26507gul_if_resume(struct device *dev)
{
	int ret;

	illumination_info_printk("[%s] \n",__func__);

	ret = bu26507gul_sw_resume(dev);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_resume ret=%d \n",__func__,ret);
		return ret;
	}
	ret = bu26507gul_sw_probe(p_bu26507gul_i2ccli, p_bu26507gul_i2cid);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_probe ret=%d \n",__func__,ret);
		return ret;
	}
	return ret;
}

int bu26507gul_if_remove(struct i2c_client *client)
{
	int ret;
	int i;

	illumination_info_printk("[%s] \n",__func__);

	bu26507gul_led_device_remove_files(illumination_led.cdev.dev, illumination_led_attrs);
	led_classdev_unregister(&illumination_led.cdev);

	for(i=0 ; i < BU26507GUL_NUM_LEDS ; i++) {
		bu26507gul_led_device_remove_files(led[i].cdev.dev, led_led_attrs);
		led_classdev_unregister(&led[i].cdev);
	}

	ret = bu26507gul_sw_remove(client);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_remove ret=%d \n",__func__,ret);
		return ret;
	}

	return ret;
}

int bu26507gul_if_shutdown(struct i2c_client *client)
{
	int ret;

	illumination_info_printk("[%s] \n",__func__);

	ret = bu26507gul_sw_shutdown(client);
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_shutdown ret=%d \n",__func__,ret);
		return ret;
	}

	return ret;
}

int bu26507gul_if_exit(void)
{
	int ret;

	illumination_info_printk("[%s] \n",__func__);
	ret = bu26507gul_sw_exit();
	if (unlikely(ret != 0)) {
		illumination_debug_printk("[%s]  bu26507gul_sw_exit ret=%d \n",__func__,ret);
	}

	return ret;
}
