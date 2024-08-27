/*
 * linux/drivers/mmc/core/lock.h
 *
 * Copyright 2006 Instituto Nokia de Tecnologia (INdT), All Rights Reserved.
 * Copyright 2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MMC password key handling.
 */
/*----------------------------------------------------------------------------*/
/* COPYRIGHT(C) FUJITSU LIMITED 2012-2013                                     */
/*----------------------------------------------------------------------------*/
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>

#include "sd.h"
#include "host.h"
#include "core.h"
#include "lock.h"
#include "mmc_ops.h"
#include "sdio_ops.h"

#define dev_to_mmc_card(d) container_of(d, struct mmc_card, dev)
#define dev_to_mmc_host(d) container_of(d, struct mmc_host, class_dev)

int  lockable_sdstatus    = SDSTATUS_UNINSERTED;
char lockable_cid[40]     = {0};
int  lockable_event       = EVENT_INIT;
int  sysfs_result         = RESULT_INIT;
int  lockable_result_flag = 0;
int  lockable_retry       = 0;
int  lockable_uevent_flag = 0;
/* FUJITSU:2012-09-03 SD mod start */
struct lock_unlock_key temp_key={0,{0}};
struct lock_unlock_key lockable_key={0,{0}};
struct lock_unlock_key lockable_old_key={0,{0}};
/* FUJITSU:2012-09-03 SD mod end */
struct mutex lockable_lock;
/* FUJITSU:2013-01-23 SD detect Add start */
static char lockable_old_cid[40] = {0};
/* FUJITSU:2013-01-23 SD detect Add end */

/* FUJITSU:2013-01-23 SD detect Add start */
void mmc_set_lockable_init(struct mmc_host *mmc)
{
	SDLOG_INFO("lockable init.");
	lockable_sdstatus    = SDSTATUS_UNINSERTED;
	memset(lockable_cid, 0, sizeof(lockable_cid));
	lockable_event       = EVENT_INIT;
	sysfs_result         = RESULT_INIT;
	lockable_result_flag = 0;
	lockable_retry       = 0;
	lockable_uevent_flag = 0;
	temp_key.len         = 0;
	memset(temp_key.data, 0, sizeof(temp_key.data));
	lockable_key.len     = 0;
	memset(lockable_key.data, 0, sizeof(lockable_key.data));
	lockable_old_key.len = 0;
	memset(lockable_old_key.data, 0, sizeof(lockable_old_key.data));
	memset(lockable_old_cid, 0, sizeof(lockable_old_cid));
}
/* FUJITSU:2013-01-23 SD detect Add end */

/* password check */
static int password_check(struct mmc_host *host, int event)
{
	int err = SDLOCCK_OK;

	if(event == EVENT_ERASE)
		return SDLOCCK_OK;
/* FUJITSU:2012-09-03 SD mod start */
	if((lockable_key.len <= 0) || (MMC_KEYLEN < lockable_key.len)){
		SDLOG_ERR("[event=%d, key_len=%d, key_len(max)=%d]", event, lockable_key.len, MMC_KEYLEN);
/* FUJITSU:2012-09-03 SD mod end */
		err = SDLOCCK_ERR;
	}
	if(event == EVENT_CHANGE){
/* FUJITSU:2012-09-03 SD mod start */
		if((lockable_old_key.len <= 0) || (MMC_KEYLEN < lockable_old_key.len)){
			SDLOG_ERR("[event=%d, old_key_len=%d, old_key_len(max)=%d]", event, lockable_old_key.len, MMC_KEYLEN);
/* FUJITSU:2012-09-03 SD mod end */
			err = SDLOCCK_ERR;
		}
	}

	if (err) {
		SDLOG_ERR("[info=password error]");
		mmc_host_lockable_set_result(host, RESULT_NOPASSWORD);
	}

	return err;
}

/* status check */
static int status_check(struct mmc_host *host, int event)
{
	int err = SDLOCCK_OK;
	switch (event) {
		case EVENT_ASSIGN:
			if(lockable_sdstatus != SDSTATUS_NO_PASSWORD)
				err = SDLOCCK_ERR;
			break;
		case EVENT_REMOVE:
		case EVENT_CHANGE:
			if(lockable_sdstatus != SDSTATUS_UNLOCK)
				err = SDLOCCK_ERR;
			break;
		case EVENT_UNLOCK:
		case EVENT_ERASE:
			if(lockable_sdstatus != SDSTATUS_LOCK)
				err = SDLOCCK_ERR;
			break;
		default:
			break;
	}
	if (err) {
		SDLOG_ERR("[info=bad status, event=%d, sdstatus=%d]",event,lockable_sdstatus);
		mmc_host_lockable_set_result(host, RESULT_BADSTATUS);
	}
	return err;
}

/* mmc_lockable_start */
static int mmc_lockable_start(struct mmc_host *host, int event)
{
	int err = SDLOCCK_OK;
	
	if(password_check(host, event))
		return SDLOCCK_ERR;

	if(status_check(host, event))
		return SDLOCCK_ERR;

	switch (event) {
		case EVENT_ASSIGN:
		case EVENT_REMOVE:
		case EVENT_CHANGE:
			mmc_stop_host(host);
			msleep(50);/* 50ms */
			if(event != EVENT_ASSIGN)
				lockable_sdstatus = SDSTATUS_LOCK;
			SDLOG_DEBUG("mmc_stop_host complete.");
			break;
		case EVENT_UNLOCK:
		case EVENT_ERASE:
		default:
			break;
	}
	lockable_event = event;
	err = mmc_rescan_lock(host);
	if (err) {
		SDLOG_ERR("[mmc_rescan_lock error, event=%d]",event);
		mmc_host_lockable_set_result(host, RESULT_ERR);
	}
	return err;
}

/* lockable */
static ssize_t
mmc_lockable_store(struct device *dev, struct device_attribute *attr,
		const char *data, size_t len)
{
	struct mmc_host *host = dev_to_mmc_host(dev);

	mutex_lock(&lockable_lock);
	
	lockable_result_flag = 1;

	if (!strncmp(data, "unlock", 6)) {
		/* unlock */
		SDLOG_DEBUG("SDlock_start[Parameter=unlock(event=%d)]",EVENT_UNLOCK);
		if(!mmc_lockable_start(host, EVENT_UNLOCK)){
			SDLOG_DEBUG("SDlock_OK[Parameter=unlock]");
			lockable_sdstatus = SDSTATUS_UNLOCK;
			mmc_host_lockable_set_result(host, RESULT_SUCCESS);
		}
	} else if (!strncmp(data, "erase", 5)) {
		/* forced erase only works while card is locked */
		SDLOG_DEBUG("SDlock_start[Parameter=erase(event=%d)]",EVENT_ERASE);
		if(!mmc_lockable_start(host, EVENT_ERASE)){
			SDLOG_DEBUG("SDlock_OK[Parameter=erase]");
			lockable_sdstatus = SDSTATUS_NO_PASSWORD;
			mmc_host_lockable_set_result(host, RESULT_SUCCESS);
		}
	} else if (!strncmp(data, "remove", 6)) {
		/* remove password only works while card is unlocked */
		SDLOG_DEBUG("SDlock_start[Parameter=remove(event=%d)]",EVENT_REMOVE);
		if(!mmc_lockable_start(host, EVENT_REMOVE)){
			SDLOG_DEBUG("SDlock_OK[Parameter=remove]");
			lockable_sdstatus = SDSTATUS_NO_PASSWORD;
			mmc_host_lockable_set_result(host, RESULT_SUCCESS);
		}
	} else if (!strncmp(data, "assign", 6)) {
		/* assign */
		SDLOG_DEBUG("SDlock_start[Parameter=assign(event=%d)]",EVENT_ASSIGN);
		if(!mmc_lockable_start(host, EVENT_ASSIGN)){
			SDLOG_DEBUG("SDlock_OK[Parameter=assign]");
			lockable_sdstatus = SDSTATUS_UNLOCK;
			mmc_host_lockable_set_result(host, RESULT_SUCCESS);
		}
	} else if (!strncmp(data, "change", 6)) {
		/* change */
		SDLOG_DEBUG("SDlock_start[Parameter=change(event=%d)]",EVENT_CHANGE);
		if(!mmc_lockable_start(host, EVENT_CHANGE)){
			SDLOG_DEBUG("SDlock_OK[Parameter=change]");
			lockable_sdstatus = SDSTATUS_UNLOCK;
			mmc_host_lockable_set_result(host, RESULT_SUCCESS);
		}
	} else {
		/* unknow */
		SDLOG_ERR("[info=illegal parameter]");
		mmc_host_lockable_set_result(host,RESULT_ILLEGALPARAM);
	}
	lockable_event = EVENT_INIT;

	mutex_unlock(&lockable_lock);

	return len;
}


/* lockable_result */
static ssize_t
mmc_lockable_result_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int result;

	mutex_lock(&lockable_lock);
	result = sysfs_result;
	sysfs_result = RESULT_INIT;
	mutex_unlock(&lockable_lock);
	SDLOG_DEBUG("[sysfs_result=%d]",result);
	return sprintf(buf, "%d\n", result);
}


/* cid_info */
static ssize_t
mmc_cid_info_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	SDLOG_DEBUG("lockable_cid[%s]",lockable_cid);
	return sprintf(buf, "%s\n", lockable_cid);
}


static ssize_t
mmc_cid_info_store(struct device *dev, struct device_attribute *attr,
		const char *data, size_t len)
{
	struct mmc_host *host = dev_to_mmc_host(dev);
	struct mmc_card *card = dev_to_mmc_card(dev);
	
	SDLOG_DEBUG("lockable_sdstatus[%d]",lockable_sdstatus);
	if(lockable_sdstatus == SDSTATUS_LOCK){
		lockable_uevent_flag = 2;
		mmc_host_lockable_uevent(host, card);
	}
	
	return len;
}


/* sdstatus */
static ssize_t
mmc_sdstatus_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	SDLOG_DEBUG("lockable_sdstatus[%d]",lockable_sdstatus);
	return sprintf(buf, "%d\n", lockable_sdstatus);
}


/* lockable_key */
static ssize_t
mmc_lockable_key_store(struct device *dev, struct device_attribute *attr,
		const char *data, size_t len)
{

/* FUJITSU:2013-01-23 SD Add start */
	struct mmc_host *host;
/* FUJITSU:2013-01-23 SD Add end */

	mutex_lock(&lockable_lock);

/* FUJITSU:2013-01-23 SD Add start */
	host = dev_to_mmc_host(dev);
/* FUJITSU:2013-01-23 SD Add end */

/* FUJITSU:2012-09-03 SD mod start */
	if((len <= 1) || (MMC_KEYLEN+1 < len)){
/* FUJITSU:2012-09-03 SD mod end */
		sysfs_result = RESULT_ILLEGALPARAM;
		SDLOG_ERR("illegal parameter");
		mutex_unlock(&lockable_lock);
		return len;
	}
/* FUJITSU:2012-09-03 SD mod start */
	memset(lockable_key.data, 0, sizeof(lockable_key.data));
	memcpy(lockable_key.data, data, len-1);
	
	lockable_key.len=len-1;
	
    SDLOG_DEBUG("lockable_key.data[%s]",lockable_key.data);
/* FUJITSU:2012-09-03 SD mod end */

	sysfs_result = RESULT_SUCCESS;

/* FUJITSU:2013-02-04 SD Add start */
	if (host->ops->get_cd && (host->ops->get_cd(host) == 0)){
		mmc_set_lockable_init(host);
		/* After this, sysfs_result is initialized with mmc_lockable_result_show. */
		sysfs_result = RESULT_ERR; 
		SDLOG_ERR("SD card has been removed.");
	}
/* FUJITSU:2013-02-04 SD Add end */
	mutex_unlock(&lockable_lock);
	
	return len;
}

/* lockable_old_key */
static ssize_t
mmc_lockable_old_key_store(struct device *dev, struct device_attribute *attr,
		const char *data, size_t len)
{
	
/* FUJITSU:2013-01-23 SD Add start */
	struct mmc_host *host;
/* FUJITSU:2013-01-23 SD Add end */

	mutex_lock(&lockable_lock);

/* FUJITSU:2013-01-23 SD Add start */
	host = dev_to_mmc_host(dev);
/* FUJITSU:2013-01-23 SD Add end */
/* FUJITSU:2012-09-03 SD mod start */
	if((len <= 1) || (MMC_KEYLEN+1 < len)){
/* FUJITSU:2012-09-03 SD mod end */
		sysfs_result = RESULT_ILLEGALPARAM;
		SDLOG_ERR("illegal parameter");
		mutex_unlock(&lockable_lock);
		return len;
	}
/* FUJITSU:2012-09-03 SD mod start */
	memset(lockable_old_key.data, 0, sizeof(lockable_old_key.data));
	memcpy(lockable_old_key.data, data, len-1);
	
	lockable_old_key.len = len-1;
	
    SDLOG_DEBUG("lockable_old_key.data[%s]",lockable_old_key.data);
/* FUJITSU:2012-09-03 SD mod end */

	sysfs_result = RESULT_SUCCESS;

/* FUJITSU:2013-02-04 SD Add start */
	if (host->ops->get_cd && (host->ops->get_cd(host) == 0)){
		mmc_set_lockable_init(host);
		/* After this, sysfs_result is initialized with mmc_lockable_result_show. */
		sysfs_result = RESULT_ERR; 
		SDLOG_ERR("SD card has been removed.");
	}
/* FUJITSU:2013-02-04 SD Add end */

	mutex_unlock(&lockable_lock);

	return len;
}


/* DEVICE_ATTR_list */
static DEVICE_ATTR(lockable,                   S_IWUSR, NULL,                     mmc_lockable_store);
static DEVICE_ATTR(lockable_result,  S_IRUGO          , mmc_lockable_result_show, NULL);
static DEVICE_ATTR(cid_info,         S_IRUGO | S_IWUSR, mmc_cid_info_show,        mmc_cid_info_store);
static DEVICE_ATTR(sdstatus,         S_IRUGO          , mmc_sdstatus_show,        NULL);
static DEVICE_ATTR(lockable_key,               S_IWUSR, NULL,                     mmc_lockable_key_store);
static DEVICE_ATTR(lockable_old_key,           S_IWUSR, NULL,                     mmc_lockable_old_key_store);


/* sysfs_list */
static struct device_attribute *sdlock_attributes[] = {
	&dev_attr_lockable,
	&dev_attr_lockable_result,
	&dev_attr_cid_info,
	&dev_attr_sdstatus,
	&dev_attr_lockable_key,
	&dev_attr_lockable_old_key,
	NULL
};


/* set_cid */
void mmc_host_lockable_setcid(struct mmc_host *host, struct mmc_card *card)
{
/* FUJITSU:2012-08-28 SD change start */
	if ((host->index == HOST_INDEX_SD)){
/* FUJITSU:2012-08-28 SD change end */
		snprintf(lockable_cid, sizeof(lockable_cid), "%08x%08x%08x%08x", card->raw_cid[0], card->raw_cid[1], card->raw_cid[2], card->raw_cid[3]);
/* FUJITSU:2013-01-23 SD detect Add start */
		if (memcmp(lockable_cid, lockable_old_cid,
			sizeof(lockable_cid))) {
			SDLOG_INFO("insert sd card. [before cid=%s, after cid=%s]",
				(lockable_old_cid[0] != 0) ? lockable_old_cid : "None",
				(lockable_cid[0] != 0)     ? lockable_cid     : "None");
			mmc_set_lockable_init(host);
			snprintf(lockable_cid, sizeof(lockable_cid), "%08x%08x%08x%08x", card->raw_cid[0], card->raw_cid[1], card->raw_cid[2], card->raw_cid[3]);
		}
		memcpy(lockable_old_cid, lockable_cid, sizeof(lockable_cid));
/* FUJITSU:2013-01-23 SD detect Add end */
	}

	return;
}


/* snd_uevent */
void mmc_host_lockable_uevent(struct mmc_host *host, struct mmc_card *card)
{
	int err = 0;
	char event_string[40] = {0};
	char *uevent_word[] = { event_string, NULL };

	SDLOG_DEBUG("host->index[%d] lockable_uevent_flag[%d]",host->index,lockable_uevent_flag);
/* FUJITSU:2012-08-28 SD change start */
	if ((host->index == HOST_INDEX_SD) && (lockable_uevent_flag != 1)){
/* FUJITSU:2012-08-28 SD change end */
		snprintf(event_string, sizeof(event_string), "CID=%s",lockable_cid);
		err = kobject_uevent_env(&host->parent->kobj, KOBJ_CHANGE, uevent_word);
		lockable_uevent_flag = 1;
		if (err){
			SDLOG_ERR("kobject_uevent_env() error. [err=%d, event_string=\"%s\"]", err, event_string);
			return;
		}
		SDLOG_INFO("send_uevent [lockable_uevent_flag=%d, event_string=\"%s\"]", lockable_uevent_flag, event_string);
	}

	return;

}


/* sysfs_ret_set */
void mmc_host_lockable_set_result(struct mmc_host *host,int ret)
{
/* FUJITSU:2012-08-28 SD changestart */
	if (((host->index == HOST_INDEX_SD) && (lockable_result_flag)) || (ret == RESULT_SUCCESS)){
/* FUJITSU:2012-08-28 SD change end */
		sysfs_result = ret;
		lockable_result_flag = 0;
		SDLOG_INFO("sysfs_result=%d",sysfs_result);
	}

	return;
}


/* sysfs_init */
void mmc_host_sdlock_sysfs_init(struct mmc_host *host)
{
	struct device_attribute **attrs = sdlock_attributes;
	struct device_attribute *attr = NULL;
	int err = 0;

/* FUJITSU:2012-08-28 SD change start */
	if(host->index != HOST_INDEX_SD)
/* FUJITSU:2012-08-28 SD change end */
		return;

	while ((attr = *attrs++)) {
		err = device_create_file(&host->class_dev, attr);
		if (err) {
			SDLOG_ERR("device_create_file() error. [err=%d]", err);
			return;
		}
	}

	mutex_init(&lockable_lock);

	return;
}
