/* Qualcomm Secure Execution Environment Communicator (QSEECOM) driver
 *
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*----------------------------------------------------------------------------*/
/* COPYRIGHT(C) FUJITSU LIMITED 2013                                          */
/*----------------------------------------------------------------------------*/

#ifndef __QSEECOM_LEGACY_H_
#define __QSEECOM_LEGACY_H_

#include <linux/types.h>

#define TZ_SCHED_CMD_ID_REGISTER_LISTENER    0x04

enum tz_sched_cmd_type {
	TZ_SCHED_CMD_INVALID = 0,
	TZ_SCHED_CMD_NEW,      /* New TZ Scheduler Command */
	TZ_SCHED_CMD_PENDING,  /* Pending cmd...sched will restore stack */
	TZ_SCHED_CMD_COMPLETE, /* TZ sched command is complete */
	TZ_SCHED_CMD_MAX     = 0x7FFFFFFF
};

enum tz_sched_cmd_status {
	TZ_SCHED_STATUS_INCOMPLETE = 0,
	TZ_SCHED_STATUS_COMPLETE,
	TZ_SCHED_STATUS_MAX  = 0x7FFFFFFF
};

/* FUJITSU:2013-01-18 sddl_encrypt add start */
enum tz_oem_cmd_type {
	TZ_OEM_CMD_INVALID = 0,
	TZ_OEM_CMD_NEW,      /** New TZ OEM Command */
	TZ_OEM_CMD_PENDING,  /** Pending cmd...oem will restore stack */
	TZ_OEM_CMD_COMPLETE, /** TZ oem command is complete */
	TZ_OEM_CMD_MAX     = 0x7FFFFFFF
};

enum tz_oem_cmd_status {
	TZ_OEM_STATUS_INCOMPLETE = 0,
	TZ_OEM_STATUS_COMPLETE,
	TZ_OEM_STATUS_MAX  = 0x7FFFFFFF
};

#define TZ_OEM_IV_SIZE      16
/* FUJITSU:2013-01-18 sddl_encrypt add end */

/* Command structure for initializing shared buffers */
__packed struct qse_pr_init_sb_req_s {
	/* First 4 bytes should always be command id */
	uint32_t                  pr_cmd;
	/* Pointer to the physical location of sb buffer */
	uint32_t                  sb_ptr;
	/* length of shared buffer */
	uint32_t                  sb_len;
	uint32_t                  listener_id;
};

__packed struct qse_pr_init_sb_rsp_s {
	/* First 4 bytes should always be command id */
	uint32_t                  pr_cmd;
	/* Return code, 0 for success, Approp error code otherwise */
	int32_t                   ret;
};

/*
 * struct QSEECom_command - QSECom command buffer
 * @cmd_type: value from enum tz_sched_cmd_type
 * @sb_in_cmd_addr: points to physical location of command
 *                buffer
 * @sb_in_cmd_len: length of command buffer
 */
__packed struct qseecom_command {
	uint32_t               cmd_type;
	uint8_t                *sb_in_cmd_addr;
	uint32_t               sb_in_cmd_len;
};

/*
 * struct QSEECom_response - QSECom response buffer
 * @cmd_status: value from enum tz_sched_cmd_status
 * @sb_in_rsp_addr: points to physical location of response
 *                buffer
 * @sb_in_rsp_len: length of command response
 */
__packed struct qseecom_response {
	uint32_t                 cmd_status;
	uint8_t                  *sb_in_rsp_addr;
	uint32_t                 sb_in_rsp_len;
};

#endif /* __QSEECOM_LEGACY_H_ */
