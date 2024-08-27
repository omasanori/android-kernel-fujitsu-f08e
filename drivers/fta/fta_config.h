/*
 * fta_config.h - FTA (Fault Trace Assistant)
 *                configuration header
 *
 * Copyright(C) 2011-2012 FUJITSU LIMITED
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

#ifndef _FTA_CONFIG_H
#define _FTA_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif


/* ==========================================================================
 *  INCLUDE HEADER
 * ========================================================================== */
#include "fta_def.h"

/* ==========================================================================
 *  DEFINITION
 * ========================================================================== */

/* size of memory */
/* FUJITSU:2013-03-05_[P]-13-1ST-GEN_131-_BUGFIX-GEN_MOD-S */
//#define FTA_CONFIG_STORE_MEMORY_SIZE    0x280000
#define FTA_CONFIG_STORE_MEMORY_SIZE    0x300000
/* FUJITSU:2013-03-05_[P]-13-1ST-GEN_131-_BUGFIX-GEN_MOD-E */

/* count of area(excluded manage area) */
//#define FTA_CONFIG_STORE_INITIAL_COUNT  10
#define FTA_CONFIG_STORE_INITIAL_COUNT  9



/* tag infomation */
#define FTA_TAG_KMSG                        0x47534D4B          /* KMSG */
#define FTA_TAG_KMSG_MEMORY_SIZE            (0)
#define FTA_TAG_DIAG                        0x47414944          /* DIAG */
#define FTA_TAG_DIAG_MEMORY_SIZE            (128*1024)
#define FTA_TAG_PROC0                       0x30435250          /* PRC0 */
#define FTA_TAG_PROC0_MEMORY_SIZE           (8*1024)
#define FTA_TAG_PROC1                       0x31435250          /* PRC1 */
#define FTA_TAG_PROC1_MEMORY_SIZE           (12*1024)
#define FTA_TAG_PROC2                       0x32435250          /* PRC2 */
#define FTA_TAG_PROC2_MEMORY_SIZE           (16*1024)
#define FTA_TAG_PROC3                       0x33435250          /* PRC3 */
#define FTA_TAG_PROC3_MEMORY_SIZE           (12*1024)
#define FTA_TAG_TASKTABLE                   0x4B534154          /* TASK */
#define FTA_TAG_TASKTABLE_MEMORY_SIZE       (64*1024)
//#define FTA_TAG_DISPATCH                    0x50534944          /* DISP */
//#define FTA_TAG_DISPATCH_MEMORY_SIZE        (8*1024)
#define FTA_TAG_VAR_LOG                     0x00474F4C          /* LOG */
#define FTA_LOG_MAIN_SIZE                   (256*1024)          /* log/main */
#define FTA_LOG_EVENT_SIZE                  (256*1024)          /* log/event */
#define FTA_LOG_RADIO_SIZE                  (256*1024)           /* log/radio */
#define FTA_LOG_SYSTEM_SIZE                 (256*1024)           /* log/system */
#define FTA_LOG_USER0_SIZE                  (16*1024)           /* log/user0 */
#define FTA_LOG_USER1_SIZE                  (16*1024)           /* log/user1 */
#define FTA_LOG_HEADER_SIZE                 (16*1024)           /* log header */
#define FTA_TAG_VAR_LOG_MEMORY_SIZE         (FTA_LOG_MAIN_SIZE   + \
                                             FTA_LOG_EVENT_SIZE  + \
                                             FTA_LOG_RADIO_SIZE  + \
                                             FTA_LOG_SYSTEM_SIZE + \
                                             FTA_LOG_USER0_SIZE  + \
                                             FTA_LOG_USER1_SIZE  + \
                                             FTA_LOG_HEADER_SIZE)
#define FTA_TAG_TERMINFO                    0x4d524554          /* TERM */
#define FTA_TAG_TERMINFO_MEMORY_SIZE        (1024)

/* --------------------------------
 *  extern
 * -------------------------------- */
typedef struct {
  const fta_manager_store_item *fta_store_item_kmsg;
  const fta_manager_store_item *fta_store_item_diag;
  const fta_manager_store_item *fta_store_item_proc0;
  const fta_manager_store_item *fta_store_item_proc1;
  const fta_manager_store_item *fta_store_item_proc2;
  const fta_manager_store_item *fta_store_item_proc3;
  const fta_manager_store_item *fta_store_item_tasktable;
//  const fta_manager_store_item *fta_store_item_dispatch;
  const fta_manager_store_item *fta_store_item_log;
  const fta_manager_store_item *fta_store_item_terminfo;
} fta_reference_pointers;
extern fta_reference_pointers fta_reference;

extern char *fta_log_area;

struct procDataType {
    struct list_head list;
    int ret;
    int type;
    int size;
    char *ptr;
    struct completion comp;
    unsigned short tid;
    unsigned short state;
};

enum {
  FTA_RA = 1,
  FTA_CA,
  FTA_RC,
  FTA_CC,
  FTA_CD,
/* 2012-08-20 add start */
  FTA_WC,
  FTA_SC,
  FTA_RN,
  FTA_CN,
  FTA_WN,
  FTA_SN,
/* 2012-08-20 add end */
};

#ifdef __cplusplus
}
#endif
#endif  /* _FTA_CONFIG_H */
