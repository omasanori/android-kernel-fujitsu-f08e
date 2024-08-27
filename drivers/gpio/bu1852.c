/*
 * bu1852.c - ROHM BU1852GXW 20-bit I/O expander driver
 *
 * Copyright (C) 2005 Ben Gardner <bgardner@wabtec.com>
 * Copyright (C) 2007 Marvell International Ltd.
 * Copyright (C) 2011 NVIDIA Corporation.
 *
 * Derived from pca953x.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */
/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012 - 2013
/*----------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/i2c/bu1852.h>
#include <linux/slab.h>
#ifdef CONFIG_OF_GPIO
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#endif
/* FUJITSU:2012-12-04 debug_sysfs_gpio add start */
#ifdef CONFIG_DEBUG_FS
#include <linux/seq_file.h>
#endif /* CONFIG_DEBUG_FS */
/* FUJITSU:2012-12-04 debug_sysfs_gpio add end */

#define BU1852_RESET        0x00

#define BU1852_INPUT_1      0x18                    /* GPIO[07-00] */
#define BU1852_INPUT_2      0x17                    /* GPIO[15-08] */
#define BU1852_INPUT_3      0x16                    /* GPIO[19-16] */

#define BU1852_OUTPUT_1     0x0E                    /* GPIO[07-00] */
#define BU1852_OUTPUT_2     0x0D                    /* GPIO[15-08] */
#define BU1852_OUTPUT_3     0x0C                    /* GPIO[19-16] */

#define BU1852_DIRECTION_1  0x08                    /* GPIO[07-00] */
#define BU1852_DIRECTION_2  0x07                    /* GPIO[15-08] */
#define BU1852_DIRECTION_3  0x06                    /* GPIO[19-16] */

#define BU1852_PULLUP_1     0x11                    /* GPIO[07-00] */
#define BU1852_PULLDOWN_1   0x10                    /* GPIO[15-08] */
#define BU1852_PULLDOWN_2   0x0F                    /* GPIO[19-16] */

#define BU1852_INTENABLE_1  0x0B                    /* GPIO[07-00] */
#define BU1852_INTENABLE_2  0x0A                    /* GPIO[15-08] */
#define BU1852_INTENABLE_3  0x09                    /* GPIO[19-16] */

#define BU1852_GPIO_NUM     20
#define BU1852_GPIO_IDX_NUM 3

#define BU1852_RETRY        3

static const struct i2c_device_id bu1852_id[] = {
	{ "bu1852_1", 0, },
	{ "bu1852_2", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bu1852_id);

struct bu1852_addr_struct {
	unsigned char input;
	unsigned char output;
	unsigned char direction;
	unsigned char pull;					/* FUJITSU:2012-12-04 debug_sysfs_gpio add */
};

/* FUJITSU:2012-12-04 debug_sysfs_gpio mod start */
static const struct bu1852_addr_struct bu1852_addr_tbl[BU1852_GPIO_IDX_NUM] = {
	{ BU1852_INPUT_1, BU1852_OUTPUT_1, BU1852_DIRECTION_1, BU1852_PULLUP_1   },     /* GPIO[07-00] */
	{ BU1852_INPUT_2, BU1852_OUTPUT_2, BU1852_DIRECTION_2, BU1852_PULLDOWN_1 },     /* GPIO[15-08] */
	{ BU1852_INPUT_3, BU1852_OUTPUT_3, BU1852_DIRECTION_3, BU1852_PULLDOWN_2 },     /* GPIO[19-16] */
};
/* FUJITSU:2012-12-04 debug_sysfs_gpio mod end */

struct bu1852_chip {
	unsigned gpio_start;
	uint16_t reg_output[BU1852_GPIO_IDX_NUM];
	uint16_t reg_direction[BU1852_GPIO_IDX_NUM];
	uint16_t reg_pull[BU1852_GPIO_IDX_NUM];				/* FUJITSU:2012-12-04 debug_sysfs_gpio add */

	struct i2c_client *client;
	struct bu1852_platform_data *dyn_pdata;
	struct gpio_chip gpio_chip;
	const char *const *names;
	struct mutex i2c_lock;
};

/* FUJITSU:2012-11-16 log add start */
#define bu1852_dev_err(dev, fmt, arg...)   dev_err(dev, fmt, ##arg)
#define bu1852_dev_warn(dev, fmt, arg...)  dev_warn(dev, fmt, ##arg)
#define bu1852_dev_dbg(dev, fmt, arg...)   dev_dbg(dev, fmt, ##arg)
#define bu1852_dev_info(dev, fmt, arg...)  _dev_info(dev, fmt, ##arg)
/* FUJITSU:2012-11-16 log add end */

static int bu1852_write_reg(struct bu1852_chip *chip, int reg, uint16_t val)
{
	int ret = 0;
	int retry_count = BU1852_RETRY;

	do {
		ret = i2c_smbus_write_byte_data(chip->client, reg, val);
		if (ret < 0) {
			retry_count--;
		} else {
			break;
		}
	} while (retry_count > -1);

	if (ret < 0) {
		bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] failed writing register ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}

	return 0;
}

static int bu1852_read_reg(struct bu1852_chip *chip, int reg, uint16_t *val)
{
	int ret = 0;
	int retry_count = BU1852_RETRY;

	do {
		ret = i2c_smbus_read_byte_data(chip->client, reg);
		if (ret < 0) {
			retry_count--;
		} else {
			break;
		}
	} while (retry_count > -1);

	if (ret < 0) {
		bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] failed reading register ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}

	*val = (uint16_t)ret;
	return 0;
}

static int bu1852_gpio_direction_input(struct gpio_chip *gc, unsigned off)
{
	struct bu1852_chip *chip;
	uint16_t reg_val = 0;
	int ret = 0;
	unsigned char idx = 0;

/* FUJITSU:2012-12-04 debug_sysfs_gpio mod start */
	if (!gc) {
		return -EINVAL;
	}
/* FUJITSU:2012-12-04 debug_sysfs_gpio mod end */

	if ((0x00 <= off) && (off <= 0x07)) {
		idx = 0;
	} else if ((0x08 <= off) && (off <= 0x0F)) {
		idx = 1;
	} else if ((0x10 <= off) && (off <= 0x13)) {
		idx = 2;
	} else {
		return -EINVAL;
	}

	chip = container_of(gc, struct bu1852_chip, gpio_chip);
/* FUJITSU:2013-01-30 debug_bu1852 mod start */
#if 0
	bu1852_dev_info(&chip->client->dev, "[%s(%d)] gpio=%d(bit%d) direction=%#x\n",
				__func__, __LINE__, chip->gpio_start + off, off % 0x08, chip->reg_direction[idx]);
#endif
/* FUJITSU:2012-01-30 debug_bu1852 mod end */
	off = off % 0x08;

	mutex_lock(&chip->i2c_lock);

	ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].direction, &reg_val);

	if (ret < 0) {
		reg_val = chip->reg_direction[idx];
	}

	reg_val = reg_val & ~(1u << off);

	ret = bu1852_write_reg(chip, bu1852_addr_tbl[idx].direction, reg_val);
	if (ret) {
		bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] failed writing register ret=%d\n", __func__, __LINE__, ret);
		mutex_unlock(&chip->i2c_lock);
		return ret;
	}

	chip->reg_direction[idx] = reg_val;
/* FUJITSU:2013-01-30 debug_bu1852 mod start */
#if 0
	bu1852_dev_info(&chip->client->dev, "[%s(%d)] gpio=%d(bit%d) direction=%#x\n",
				__func__, __LINE__, chip->gpio_start + off, off % 0x08, chip->reg_direction[idx]);
#endif
/* FUJITSU:2012-01-30 debug_bu1852 mod end */
	mutex_unlock(&chip->i2c_lock);

	return 0;
}

static int bu1852_gpio_direction_output(struct gpio_chip *gc,
		unsigned off, int val)
{
	struct bu1852_chip *chip;
	uint16_t reg_val = 0;
	int ret = 0;
	unsigned char idx = 0;

/* FUJITSU:2012-12-04 debug_sysfs_gpio mod start */
	if (!gc) {
		return -EINVAL;
	}
/* FUJITSU:2012-12-04 debug_sysfs_gpio mod end */

	if ((0x00 <= off) && (off <= 0x07)) {
		idx = 0;
	} else if ((0x08 <= off) && (off <= 0x0F)) {
		idx = 1;
	} else if ((0x10 <= off) && (off <= 0x13)) {
		idx = 2;
	} else {
		return -EINVAL;
	}

	chip = container_of(gc, struct bu1852_chip, gpio_chip);
/* FUJITSU:2012-01-30 debug_bu1852 mod start */
#if 0
	bu1852_dev_info(&chip->client->dev, "[%s(%d)] gpio=%d(bit%d) output=%#x direction=%#x val=%d\n",
				__func__, __LINE__, chip->gpio_start + off, off % 0x08, chip->reg_output[idx], chip->reg_direction[idx], val);
#endif
/* FUJITSU:2012-01-30 debug_bu1852 mod end */
	off = off % 0x08;

	mutex_lock(&chip->i2c_lock);

	ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].output, &reg_val);

	if (ret < 0) {
		reg_val = chip->reg_output[idx];
	}

	if (val) {
		reg_val = reg_val | (1u << off);
	} else {
		reg_val = reg_val & ~(1u << off);
	}

	ret = bu1852_write_reg(chip, bu1852_addr_tbl[idx].output, reg_val);
	if (ret) {
		bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] failed BU1852_OUTPUT writing register ret=%d\n", __func__, __LINE__, ret);
		mutex_unlock(&chip->i2c_lock);
		return ret;
	}

	chip->reg_output[idx] = reg_val;

	/* then direction */
	ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].direction, &reg_val);

	if (ret < 0) {
		reg_val = chip->reg_direction[idx];
	}

	reg_val = reg_val | (1u << off);
	ret = bu1852_write_reg(chip, bu1852_addr_tbl[idx].direction, reg_val);
	if (ret) {
		bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] failed BU1852_DIRECTION writing register ret=%d\n", __func__, __LINE__, ret);
		mutex_unlock(&chip->i2c_lock);
		return ret;
	}

	chip->reg_direction[idx] = reg_val;
/* FUJITSU:2012-01-30 debug_bu1852 mod start */
#if 0
	bu1852_dev_info(&chip->client->dev, "[%s(%d)] gpio=%d(bit%d) output=%#x direction=%#x val=%d\n",
				__func__, __LINE__, chip->gpio_start + off, off % 0x08, chip->reg_output[idx], chip->reg_direction[idx], val);
#endif
/* FUJITSU:2012-01-30 debug_bu1852 mod end */
	mutex_unlock(&chip->i2c_lock);

	return 0;
}

static int bu1852_gpio_get_value(struct gpio_chip *gc, unsigned off)
{
	struct bu1852_chip *chip = NULL;
	uint16_t reg_val = 0;
	int ret = 0;
	unsigned char idx = 0;
	unsigned gpio_num = 0;		/* FUJITSU:2012-12-04 debug_sysfs_gpio add */

	if (!gc) {
		return -EINVAL;
	}

	if ((0x00 <= off) && (off <= 0x07)) {
		idx = 0;
	} else if ((0x08 <= off) && (off <= 0x0F)) {
		idx = 1;
	} else if ((0x10 <= off) && (off <= 0x13)) {
		idx = 2;
	} else {
		return -EINVAL;
	}

	chip = container_of(gc, struct bu1852_chip, gpio_chip);

/* FUJITSU:2012-12-04 debug_sysfs_gpio mod start */
	gpio_num = chip->gpio_start + off;
/* FUJITSU:2012-12-04 debug_sysfs_gpio mod end */
	off = off % 0x08;

	mutex_lock(&chip->i2c_lock);

/* FUJITSU:2012-11-16 get_value mod start */
	if (chip->reg_direction[idx] & (1u << off)) {
		ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].output, &reg_val);
/* FUJITSU:2012-01-30 debug_bu1852 mod start */
#if 0
		bu1852_dev_info(&chip->client->dev, "[%s(%d)] gpio=%d(bit%d) output=%#x val=%d\n",
					__func__, __LINE__, gpio_num, off, reg_val, ((reg_val & (1u << off)) ? 1 : 0));
#endif
	} else {
		ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].input, &reg_val);
#if 0
		bu1852_dev_info(&chip->client->dev, "[%s(%d)] gpio=%d(bit%d) input=%#x val=%d\n",
					__func__, __LINE__, gpio_num, off, reg_val, ((reg_val & (1u << off)) ? 1 : 0));
#endif
/* FUJITSU:2012-01-30 debug_bu1852 mod end */
	}
/* FUJITSU:2012-11-16 get_value mod end */

	if (ret < 0) {
/* FUJITSU:2012-12-04 debug_sysfs_gpio mod start */
		bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] gpio=%d err ret=%d\n",
				__func__, __LINE__, gpio_num, ret);
/* FUJITSU:2012-12-04 debug_sysfs_gpio mod end */
		mutex_unlock(&chip->i2c_lock);
		return ret;
	}

	mutex_unlock(&chip->i2c_lock);
	return (reg_val & (1u << off)) ? 1 : 0;
}

static void bu1852_gpio_set_value(struct gpio_chip *gc, unsigned off, int val)
{
	struct bu1852_chip *chip = NULL;
	uint16_t reg_val = 0;
	int ret = 0;
	unsigned char idx = 0;

	if (!gc) {
		return;
	}

	if ((0x00 <= off) && (off <= 0x07)) {
		idx = 0;
	} else if ((0x08 <= off) && (off <= 0x0F)) {
		idx = 1;
	} else if ((0x10 <= off) && (off <= 0x13)) {
		idx = 2;
	} else {
		return;
	}

	chip = container_of(gc, struct bu1852_chip, gpio_chip);
/* FUJITSU:2012-01-30 debug_bu1852 mod start */
#if 0
	bu1852_dev_info(&chip->client->dev, "[%s(%d)] gpio=%d(bit%d) output=%#x val=%d\n",
				__func__, __LINE__, chip->gpio_start + off, off % 0x08, chip->reg_output[idx], val);
#endif
/* FUJITSU:2012-01-30 debug_bu1852 mod end */
	off = off % 0x08;

	mutex_lock(&chip->i2c_lock);

	ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].output, &reg_val);

	if (ret < 0) {
		reg_val = chip->reg_output[idx];
	}

	if (val) {
		reg_val = reg_val | (1u << off);
	} else {
		reg_val = reg_val & ~(1u << off);
	}

	ret = bu1852_write_reg(chip, bu1852_addr_tbl[idx].output, reg_val);

	if (ret) {
		bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] failed writing register ret=%d\n", __func__, __LINE__, ret);
		mutex_unlock(&chip->i2c_lock);
		return;
	}

	chip->reg_output[idx] = reg_val;
/* FUJITSU:2012-01-30 debug_bu1852 mod start */
#if 0
	bu1852_dev_info(&chip->client->dev, "[%s(%d)] (bit%d) output=%#x val=%d\n",
				__func__, __LINE__, off, chip->reg_output[idx], val);
#endif
/* FUJITSU:2012-01-30 debug_bu1852 mod end */
	mutex_unlock(&chip->i2c_lock);

}

/* FUJITSU:2012-12-04 debug_sysfs_gpio add start */
#ifdef CONFIG_DEBUG_FS
static void bu1852_dbg_show(struct seq_file *s, struct gpio_chip *gc)
{
	struct bu1852_chip *chip = NULL;
	unsigned i;
	const char *label;
	int is_out,is_pull;
	unsigned char idx = 0;

	if (!gc) {
		return;
	}

	chip = container_of(gc, struct bu1852_chip, gpio_chip);

	for (i = 0; i < gc->ngpio; i++) {
		idx = i >> 3;

		is_out  = chip->reg_direction[idx] & (1u << (i % 0x08));
		is_pull = chip->reg_pull[idx]      & (1u << (i % 0x08));

		label = gpiochip_is_requested(gc, i);
		if (!label)
			label = "--";

		seq_printf(s, " gpio-%-3d (%-20.20s) %s %s %s",
			i + gc->base,
			label,
			is_out ? "out" : "in ",
			(gc->get(gc, i) ? "hi" : "lo"),
			is_pull ? "no_pull" : (idx ? "pull_down" : "pull_up"));
		seq_printf(s, "\n");
	}

	return;
}
#endif /* CONFIG_DEBUG_FS */
/* FUJITSU:2012-12-04 debug_sysfs_gpio add end */

/* FUJITSU:2013-1-25 gpiodump add start */
static void bu1852_dbg_show_local(struct gpio_chip *gc)
{
        struct bu1852_chip *chip = NULL;
        unsigned i;
        const char *label;
        int is_out,is_pull;
        unsigned char idx = 0;

        if (!gc) {
                return;
        }

        chip = container_of(gc, struct bu1852_chip, gpio_chip);

		 printk("GPIOEXP,gpio,base+gpio,label,direction,value,pull\n");
        for (i = 0; i < gc->ngpio; i++) {
                idx = i >> 3;

                is_out  = chip->reg_direction[idx] & (1u << (i % 0x08));
                is_pull = chip->reg_pull[idx]      & (1u << (i % 0x08));

                label = gpiochip_is_requested(gc, i);
                if (!label)
                        label = "\0";

                printk("GPIOEXP,%d,%d,%s,%s,%s,%s\n",
                        i, i + gc->base,
                        label,
                        is_out ? "O" : "I",
                        (gc->get(gc, i) ? "H" : "L"),
                        is_pull ? "NO_PULL" : (idx ? "PULL_DOWN" : "PULL_UP"));
        }

        return;
}


static struct gpio_chip *gc_bu1852_1;
static struct gpio_chip *gc_bu1852_2;

void bu1852_dbg_dump(void)
{
    bu1852_dbg_show_local(gc_bu1852_1);
    bu1852_dbg_show_local(gc_bu1852_2);
}
/* FUJITSU:2013-1-25 gpiodump add end */

static void bu1852_setup_gpio(struct bu1852_chip *chip)
{
	struct gpio_chip *gc;

	gc = &chip->gpio_chip;

	gc->direction_input  = bu1852_gpio_direction_input;
	gc->direction_output = bu1852_gpio_direction_output;
	gc->get = bu1852_gpio_get_value;
	gc->set = bu1852_gpio_set_value;
	gc->can_sleep = 1;

	gc->base = chip->gpio_start;
	gc->ngpio = BU1852_GPIO_NUM;
	gc->label = chip->client->name;
	gc->dev = &chip->client->dev;
	gc->owner = THIS_MODULE;
	gc->names = chip->names;

/* FUJITSU:2012-12-04 debug_sysfs_gpio add start */
#ifdef CONFIG_DEBUG_FS
	gc->dbg_show = bu1852_dbg_show;
#endif /* CONFIG_DEBUG_FS */
/* FUJITSU:2012-12-04 debug_sysfs_gpio add end */

/* FUJITSU:2013-1-20 gpio dump add start */
	if (strcmp(gc->label, "bu1852_1") == 0){
		gc_bu1852_1 = gc;
	}else if (strcmp(gc->label, "bu1852_2") == 0){
		gc_bu1852_2 = gc;
	}
/* FUJITSU:2013-1-20 gpio dump add start */
}

/*
 * Handlers for alternative sources of platform_data
 */
#ifdef CONFIG_OF_GPIO
/*
 * Translate OpenFirmware node properties into platform_data
 */
static struct bu1852_platform_data *
bu1852_get_alt_pdata(struct i2c_client *client)
{
	struct bu1852_platform_data *pdata;
	struct device_node *node;
	const uint16_t *val;

	node = client->dev.of_node;
	if (node == NULL)
		return NULL;

	pdata = kzalloc(sizeof(struct bu1852_platform_data), GFP_KERNEL);
	if (pdata == NULL) {
		bu1852_dev_err(&client->dev, "Unable to allocate platform_data\n");
		return NULL;
	}

	pdata->gpio_base = -1;
	val = of_get_property(node, "linux,gpio-base", NULL);
	if (val) {
		if (*val < 0)
			bu1852_dev_warn(&client->dev,
				 "invalid gpio-base in device tree\n");
		else
			pdata->gpio_base = *val;
	}

	val = of_get_property(node, "pulldown", NULL);
	if (val)
		pdata->invert = *val;

	return pdata;
}
#else
static struct bu1852_platform_data *
bu1852_get_alt_pdata(struct i2c_client *client)
{
	return NULL;
}
#endif

static int __devinit bu1852_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct bu1852_platform_data *pdata = NULL;
	struct bu1852_chip *chip = NULL;
	int ret = 0;
	unsigned char idx = 0;

	if (!client) {
		ret = -EINVAL;
		return ret;
	}
	if (!id) {
		ret = -EINVAL;
		return ret;
	}

	chip = kzalloc(sizeof(struct bu1852_chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		pdata = bu1852_get_alt_pdata(client);
		/*
		 * Unlike normal platform_data, this is allocated
		 * dynamically and must be freed in the driver
		 */
		chip->dyn_pdata = pdata;
	}

	if (pdata == NULL) {
		bu1852_dev_dbg(&client->dev, "no platform data\n");
		ret = -EINVAL;
		goto out_failed;
	}

	chip->client = client;

	chip->gpio_start = pdata->gpio_base;

	chip->names = pdata->names;
	mutex_init(&chip->i2c_lock);

	/* initialize cached registers from their original values.
	 * we can't share this chip with another i2c master.
	 */
	bu1852_setup_gpio(chip);

	bu1852_dev_dbg(&chip->client->dev, "[%s(%d)] chip->gpio_start=%d\n", __func__, __LINE__, chip->gpio_start);

	mutex_lock(&chip->i2c_lock);

	for (idx = 0; idx < BU1852_GPIO_IDX_NUM; idx++) {
		ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].output, &chip->reg_output[idx]);
		bu1852_dev_dbg(&chip->client->dev, "[%s(%d)] chip->reg_output[%d]=%#x\n", __func__, __LINE__, idx, chip->reg_output[idx]);

		if (ret) {
			bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] read BU1852_OUTPUT:%d ret=%d\n", __func__, __LINE__, idx, ret);
			mutex_unlock(&chip->i2c_lock);
			goto out_failed;
		}
	}

	for (idx = 0; idx < BU1852_GPIO_IDX_NUM; idx++) {
		ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].direction, &chip->reg_direction[idx]);
		bu1852_dev_dbg(&chip->client->dev, "[%s(%d)] chip->reg_direction[%d]=%#x\n", __func__, __LINE__, idx, chip->reg_direction[idx]);

		if (ret) {
			bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] read BU1852_DIRECTION:%d ret=%d\n", __func__, __LINE__, idx, ret);
			mutex_unlock(&chip->i2c_lock);
			goto out_failed;
		}
	}

/* FUJITSU:2012-12-04 debug_sysfs_gpio add start */
	for (idx = 0; idx < BU1852_GPIO_IDX_NUM; idx++) {
		ret = bu1852_read_reg(chip, bu1852_addr_tbl[idx].pull, &chip->reg_pull[idx]);
		bu1852_dev_dbg(&chip->client->dev, "[%s(%d)] chip->reg_pull[%d]=%#x\n", __func__, __LINE__, idx, chip->reg_pull[idx]);

		if (ret) {
			bu1852_dev_err(&chip->client->dev, "[%s(%d):ERROR] read BU1852_PULL:%d ret=%d\n", __func__, __LINE__, idx, ret);
			mutex_unlock(&chip->i2c_lock);
			goto out_failed;
		}
	}
/* FUJITSU:2012-12-04 debug_sysfs_gpio add end */

	mutex_unlock(&chip->i2c_lock);

	ret = gpiochip_add(&chip->gpio_chip);
	if (ret)
		goto out_failed;

	if (pdata->setup) {
		ret = pdata->setup(client, chip->gpio_chip.base,
				chip->gpio_chip.ngpio, pdata->context);
		if (ret < 0)
			bu1852_dev_warn(&client->dev, "setup failed, %d\n", ret);
	}

	i2c_set_clientdata(client, chip);
	return 0;

out_failed:
	bu1852_dev_dbg(&client->dev, "bu1852_probe failed %d\n", ret);
	kfree(chip->dyn_pdata);
	kfree(chip);
	return ret;
}

static int bu1852_remove(struct i2c_client *client)
{
	struct bu1852_platform_data *pdata = client->dev.platform_data;
	struct bu1852_chip *chip = i2c_get_clientdata(client);
	int ret = 0;

	if (pdata->teardown) {
		ret = pdata->teardown(client, chip->gpio_chip.base,
				chip->gpio_chip.ngpio, pdata->context);
		if (ret < 0) {
			bu1852_dev_err(&client->dev, "%s failed, %d\n",
					"teardown", ret);
			return ret;
		}
	}

	ret = gpiochip_remove(&chip->gpio_chip);
	if (ret) {
		bu1852_dev_err(&client->dev, "%s failed, %d\n",
				"gpiochip_remove()", ret);
		return ret;
	}

	kfree(chip->dyn_pdata);
	kfree(chip);
	return 0;
}

static struct i2c_driver bu1852_driver = {
	.driver = {
		.name	= "bu1852",
	},
	.probe		= bu1852_probe,
	.remove		= bu1852_remove,
	.id_table	= bu1852_id,
};

static int __init bu1852_init(void)
{
	return i2c_add_driver(&bu1852_driver);
}
/* register after i2c postcore initcall and before
 * subsys initcalls that may rely on these GPIOs
 */
subsys_initcall(bu1852_init);

static void __exit bu1852_exit(void)
{
	i2c_del_driver(&bu1852_driver);
}
module_exit(bu1852_exit);

MODULE_AUTHOR("David Schalig <dschalig@nvidia.com>");
MODULE_DESCRIPTION("GPIO expander driver for BU1852");
MODULE_LICENSE("GPL");
