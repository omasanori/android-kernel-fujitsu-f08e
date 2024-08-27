/*
 * cyttsp4_i2c.c
 * Cypress TrueTouch(TM) Standard Product V4 I2C Driver module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modified by: Cypress Semiconductor for test with device
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

#include <linux/input/cyttsp4_bus.h>
#include <linux/input/cyttsp4_core.h>
#include "cyttsp4_i2c.h"

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_RETRY add start */
#include "cyttsp4_regs.h"
#include "fj_touch_noracs.h"

extern struct cyttsp4_nonvolatile_info		cy_nv_info;		/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_LOG */
#define CY_I2C_LOG_MAX			1024
/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_RETRY add end */

#define CY_I2C_DATA_SIZE  (3 * 256)

struct cyttsp4_i2c {
	struct i2c_client *client;
	u8 wr_buf[CY_I2C_DATA_SIZE];
	struct mutex lock;
/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_RETRY add start */
	u8 pr_buf[CY_MAX_PRBUF_SIZE];			/* for debug log */
	u8 pr_data_i2c[CY_I2C_LOG_MAX];			/* for debug log */
/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_RETRY add end */
};

/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_RETRY add start */
static int cyttsp4_i2c_read_block_data(struct cyttsp4_i2c *ts_i2c, u16 addr, int length, void *values, int max_xfer);
static int cyttsp4_i2c_write_block_data(struct cyttsp4_i2c *ts_i2c, u16 addr, int length, const void *values, int max_xfer);

#define CY_I2C_RETRY_MAX		4
static const int cyttsp4_retry_tim_tbl[] = {	/* total time 12ms */
	1000,
	2000,
	3000,
	6000,
};

static void _cyttsp4_i2c_pr_buf(struct cyttsp4_i2c *ts_i2c, u8 *dptr, int size, const char *data_name)
{
	int i, k;
	const char fmt[] = "%02X ";
	int max;
	u8 *pr_buf;

	
	if (cy_nv_info.i2c_log != CY_TOUCH_DBG_ON) {		/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_LOG */
		return;
	}

	if (!size)
		return;

	max = (CY_MAX_PRBUF_SIZE - 1) - sizeof(CY_PR_TRUNCATED);

	pr_buf = &ts_i2c->pr_buf[0];

	pr_buf[0] = 0;
	for (i = k = 0; i < size && k < max; i++, k += 3)
		scnprintf(pr_buf + k, CY_MAX_PRBUF_SIZE, fmt, dptr[i]);

	pr_info( "[TPD] %s[0..%d]=%s%s\n", data_name, size - 1,
			pr_buf, size <= max ? "" : CY_PR_TRUNCATED);
}

static void _cyttsp4_i2c_pr_buf_i2c(struct cyttsp4_i2c *ts_i2c, u16 command, u8 *dptr, int size, const char *data_name)
{
	u8 *pr_data_i2c = &ts_i2c->pr_data_i2c[0];
	int		len = 0;

	if (cy_nv_info.i2c_log != CY_TOUCH_DBG_ON) {		/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_LOG */
		return;
	}

	if (CY_I2C_LOG_MAX < (size + 2)) {
		return ;
	}

	pr_data_i2c[0] = ts_i2c->client->addr;		/* 0x24 */
	pr_data_i2c[1] = (u8)command;
	len += 2;

	if (size != 0) {
		memcpy(&pr_data_i2c[ len ], dptr, size);
		len += size;
	}

	_cyttsp4_i2c_pr_buf(ts_i2c, pr_data_i2c, len, data_name);
}

static int _cyttsp4_i2c_read_data(struct cyttsp4_i2c *ts_i2c, u16 addr, int length, void *values, int max_xfer)
{
	int ret = 0;
	int tries = 0;

	for (tries = 0, ret = -1; tries < CY_I2C_RETRY_MAX && (ret < 0); tries++) {

		_cyttsp4_i2c_pr_buf_i2c(ts_i2c, addr, NULL, 0, "I2C Read(Req) W ");

		ret = cyttsp4_i2c_read_block_data(ts_i2c, addr, length, values, max_xfer);
		if (ret < 0) {
			_cyttsp4_elog(NULL, "%s: i2c read block data fail (ret=%d), try=%d\n", __func__, ret, tries);
			usleep(cyttsp4_retry_tim_tbl[tries]);
			/*
			 * TODO: remove the extra sleep delay when
			 * the loader exit sequence is streamlined
			  */
		}
		_cyttsp4_i2c_pr_buf_i2c(ts_i2c, addr, values, length, "I2C Read(Ret) R ");

		if (ret == -EINVAL) {
			break;
		}
	}

	return ret;
}

static int _cyttsp4_i2c_write_data(struct cyttsp4_i2c *ts_i2c, u16 addr, int length, const void *values, int max_xfer)
{
	int ret = 0;
	int tries = 0;

	for (tries = 0, ret = -1; tries < CY_I2C_RETRY_MAX && (ret < 0); tries++) {

		_cyttsp4_i2c_pr_buf_i2c(ts_i2c, addr, (u8 *)values, length, "I2C Write W ");

		ret = cyttsp4_i2c_write_block_data(ts_i2c, addr, length, values, max_xfer);
		if (ret < 0) {
			_cyttsp4_elog(NULL, "%s: i2c write block data fail (ret=%d), try=%d\n", __func__, ret, tries);
			usleep(cyttsp4_retry_tim_tbl[tries]);
			/*
			 * TODO: remove the extra sleep delay when
			 * the loader exit sequence is streamlined
			  */
		}

		if (ret == -EINVAL) {
			break;
		}
	}
	return ret;
}
/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_RETRY add end */

static int cyttsp4_i2c_read_block_data(struct cyttsp4_i2c *ts_i2c, u16 addr,
		int length, void *values, int max_xfer)
{
	int rc = -EINVAL;
	int trans_len;
	u8 client_addr;
	u8 addr_lo;
	struct i2c_msg msgs[2];

	while (length > 0) {
		client_addr = ts_i2c->client->addr | ((addr >> 8) & 0x1);
		addr_lo = addr & 0xFF;
		trans_len = min(length, max_xfer);

		memset(msgs, 0, sizeof(msgs));
		msgs[0].addr = client_addr;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &addr_lo;

		msgs[1].addr = client_addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = trans_len;
		msgs[1].buf = values;

		rc = i2c_transfer(ts_i2c->client->adapter, msgs, 2);
		if (rc != 2)
			goto exit;

		length -= trans_len;
		values += trans_len;
		addr += trans_len;
	}

exit:
	return (rc < 0) ? rc : rc != ARRAY_SIZE(msgs) ? -EIO : 0;
}

static int cyttsp4_i2c_write_block_data(struct cyttsp4_i2c *ts_i2c, u16 addr,
		int length, const void *values, int max_xfer)
{
	int rc = -EINVAL;
	u8 client_addr;
	u8 addr_lo;
	int trans_len;
	struct i2c_msg msg;

	if (sizeof(ts_i2c->wr_buf) < (length + 1))
		return -ENOMEM;

	while (length > 0) {
		client_addr = ts_i2c->client->addr | ((addr >> 8) & 0x1);
		addr_lo = addr & 0xFF;
		trans_len = min(length, max_xfer);

		memset(&msg, 0, sizeof(msg));
		msg.addr = client_addr;
		msg.flags = 0;
		msg.len = trans_len + 1;
		msg.buf = ts_i2c->wr_buf;

		ts_i2c->wr_buf[0] = addr_lo;
		memcpy(&ts_i2c->wr_buf[1], values, trans_len);

		/* write data */
		rc = i2c_transfer(ts_i2c->client->adapter, &msg, 1);
		if (rc != 1)
			goto exit;

		length -= trans_len;
		values += trans_len;
		addr += trans_len;
	}

exit:
	return (rc < 0) ? rc : rc != 1 ? -EIO : 0;
}

static int cyttsp4_i2c_write(struct cyttsp4_adapter *adap, u16 addr,
	const void *buf, int size, int max_xfer)
{
	struct cyttsp4_i2c *ts = dev_get_drvdata(adap->dev);
	int rc;

	pm_runtime_get_noresume(adap->dev);
	mutex_lock(&ts->lock);
	rc = _cyttsp4_i2c_write_data(ts, addr, size, buf, max_xfer);		/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_RETRY mod */
	mutex_unlock(&ts->lock);
	pm_runtime_put_noidle(adap->dev);

	return rc;
}

static int cyttsp4_i2c_read(struct cyttsp4_adapter *adap, u16 addr,
	void *buf, int size, int max_xfer)
{
	struct cyttsp4_i2c *ts = dev_get_drvdata(adap->dev);
	int rc;

	pm_runtime_get_noresume(adap->dev);
	mutex_lock(&ts->lock);
	rc = _cyttsp4_i2c_read_data(ts, addr, size, buf, max_xfer);		/* FUJITSU:2013-05-28 HMW_TOUCH_I2C_RETRY mod */
	mutex_unlock(&ts->lock);
	pm_runtime_put_noidle(adap->dev);

	return rc;
}

static struct cyttsp4_ops ops = {
	.write = cyttsp4_i2c_write,
	.read = cyttsp4_i2c_read,
};

static int __devinit cyttsp4_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id)
{
	struct cyttsp4_i2c *ts_i2c;
	struct device *dev = &client->dev;
	char const *adap_id = dev_get_platdata(dev);
	char const *id;
	int rc;

	dev_info(dev, "%s: Starting %s probe...\n", __func__, CYTTSP4_I2C_NAME);

	dev_dbg(dev, "%s: debug on\n", __func__);
	dev_vdbg(dev, "%s: verbose debug on\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "%s: fail check I2C functionality\n", __func__);
		rc = -EIO;
		goto error_alloc_data_failed;
	}

	ts_i2c = kzalloc(sizeof(struct cyttsp4_i2c), GFP_KERNEL);
	if (ts_i2c == NULL) {
		dev_err(dev, "%s: Error, kzalloc.\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

	mutex_init(&ts_i2c->lock);
	ts_i2c->client = client;
	client->dev.bus = &i2c_bus_type;
	i2c_set_clientdata(client, ts_i2c);
	dev_set_drvdata(&client->dev, ts_i2c);

	if (adap_id)
		id = adap_id;
	else
		id = CYTTSP4_I2C_NAME;

	dev_dbg(dev, "%s: add adap='%s' (CYTTSP4_I2C_NAME=%s)\n", __func__, id,
		CYTTSP4_I2C_NAME);

	pm_runtime_enable(&client->dev);

	rc = cyttsp4_add_adapter(id, &ops, dev);
	if (rc) {
		dev_err(dev, "%s: Error on probe %s\n", __func__,
			CYTTSP4_I2C_NAME);
		goto add_adapter_err;
	}

	dev_info(dev, "%s: Successful probe %s\n", __func__, CYTTSP4_I2C_NAME);

	return 0;

add_adapter_err:
	pm_runtime_disable(&client->dev);
	dev_set_drvdata(&client->dev, NULL);
	i2c_set_clientdata(client, NULL);
	kfree(ts_i2c);
error_alloc_data_failed:
	return rc;
}

/* registered in driver struct */
static int __devexit cyttsp4_i2c_remove(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct cyttsp4_i2c *ts_i2c = dev_get_drvdata(dev);
	char const *adap_id = dev_get_platdata(dev);
	char const *id;

	if (adap_id)
		id = adap_id;
	else
		id = CYTTSP4_I2C_NAME;

	dev_info(dev, "%s\n", __func__);
	cyttsp4_del_adapter(id);
	pm_runtime_disable(&client->dev);
	dev_set_drvdata(&client->dev, NULL);
	i2c_set_clientdata(client, NULL);
	kfree(ts_i2c);
	return 0;
}

static const struct i2c_device_id cyttsp4_i2c_id[] = {
	{ CYTTSP4_I2C_NAME, 0 },  { }
};

static struct i2c_driver cyttsp4_i2c_driver = {
	.driver = {
		.name = CYTTSP4_I2C_NAME,
		.owner = THIS_MODULE,
	},
	.probe = cyttsp4_i2c_probe,
	.remove = __devexit_p(cyttsp4_i2c_remove),
	.id_table = cyttsp4_i2c_id,
};

static int __init cyttsp4_i2c_init(void)
{
	int rc = i2c_add_driver(&cyttsp4_i2c_driver);

	pr_info("%s: Cypress TTSP I2C Touchscreen Driver (Built %s) rc=%d\n",
		 __func__, CY_DRIVER_DATE, rc);
	return rc;
}
module_init(cyttsp4_i2c_init);

static void __exit cyttsp4_i2c_exit(void)
{
	i2c_del_driver(&cyttsp4_i2c_driver);
	pr_info("%s: module exit\n", __func__);
}
module_exit(cyttsp4_i2c_exit);

MODULE_ALIAS(CYTTSP4_I2C_NAME);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product (TTSP) I2C driver");
MODULE_AUTHOR("Cypress");
MODULE_DEVICE_TABLE(i2c, cyttsp4_i2c_id);
