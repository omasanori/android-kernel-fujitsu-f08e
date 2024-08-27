/*
 * fta_def.h - FTA (Fault Trace Assistant) 
 *             common definition
 *
 * Copyright(C) 2011 FUJITSU LIMITED
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

#ifndef _FTA_DEF_H
#define _FTA_DEF_H
#ifdef __cplusplus
extern "C" {
#endif


/* ==========================================================================
 *  INCLUDE HEADER
 * ========================================================================== */

#include <linux/sched.h>
#include <linux/time.h>
#include <asm/current.h>
#include <linux/spinlock.h>


/* ==========================================================================
 *  DEFINITION
 * ========================================================================== */

typedef unsigned char  uint8;
typedef unsigned char  boolean;
typedef unsigned short uint16;
typedef unsigned long  uint32;

typedef char  int8;
typedef short int16;
typedef long  int32;
typedef long  dword;

#define TRUE 1
#define FALSE 0

/* exclusive function */
extern spinlock_t fta_lock;
extern void ftadrv_flush(void);
#define FTA_ENTER_LOCK()                do { \
                                            unsigned long flags; \
                                            preempt_disable(); \
                                            spin_lock_irqsave(&fta_lock,flags);

#define FTA_LEAVE_LOCK()                    spin_unlock_irqrestore(&fta_lock,flags); \
                                            preempt_enable(); \
                                        } while(FALSE);

#define FTA_MEMORY_FLUSH()              ftadrv_flush()

/* time function */
extern void ftadrv_get_time(struct timespec *ptr);
#define FTA_LIB_GET_TIME(getptr)        ftadrv_get_time((struct timespec *)getptr)
/* set PID/TID */
struct ftadrv_pidtid {
  uint16 pid;      /* pid  */
  uint16 pinfo;    /* cpu no (0-3bit) reverved(4-15bit) */
  uint16 tid;      /* tid */
  uint16 tinfo;    /* reserved */
};
extern int ftadrv_cpu(void);
#define FTA_LIB_GET_THREADID(getptr)    {\
   struct ftadrv_pidtid *p = (struct ftadrv_pidtid *)getptr; \
   p->pid = current->tgid; \
   p->pinfo = ftadrv_cpu(); \
   p->tid = current->pid; \
   p->tinfo = 0;\
}

/* --------------------------------------------------------------------------
 *  common header
 * -------------------------------------------------------------------------- */
typedef struct _fta_common_store_header {
  uint32 tag;                           /* tag of area                        */
  uint32 size;                          /* size of area                       */
  uint8  enabled;                       /* enable/disable                     */
  uint8  statbit;                       /* status bit                         */
  uint16 offset;                        /* offset of record area              */
} fta_common_store_header;
#define FTA_COMMON_STORE_HEADER_LEN sizeof(fta_common_store_header)
/* enabled */
#define FTA_STORE_ENABLED_FALSE         FALSE
#define FTA_STORE_ENABLED_TRUE          TRUE
/* statbit */
#define FTA_STORE_STATBIT_RECORDING     0       /* record success     */
#define FTA_STORE_STATBIT_INTENTIONAL   1       /* intentional stop   */
#define FTA_STORE_STATBIT_ALLCLEAR      0       /* clear              */

/* --------------------------------
 *  definition for manage area
 * -------------------------------- */
/* tag    */
#define FTA_TAG_MANAGER                 0x6D636C7E          /* ~lcm */

/* header */
typedef struct _fta_manager_store_header {
  fta_common_store_header cmn;          /* common header                      */
  uint16 id;                            /* manage id                          */
  uint16 reclen;                        /* record length                      */
  uint16 count;                         /* record count                       */
  uint16 parity;                        /* parity for check                   */
} fta_manager_store_header;
#define FTA_MANAGER_STORE_HEADER_LEN sizeof(fta_manager_store_header)
/* manage id */
#define FTA_MANAGER_ID_DEFAULT      0

/* item for manage */
typedef struct _fta_manager_store_item {
  fta_common_store_header *addr;                    /* address for area       */
  uint32 tag;                                       /* tag of area            */
  uint32 size;                                      /* size of area           */
  const struct _fta_manager_config_store_item *def; /* configuration setting  */
  struct _fta_lock_keeper_info *lock_keeper;        /* information of exclusion*/
} fta_manager_store_item;
#define FTA_MANAGER_STORE_ITEM_LEN sizeof(fta_manager_store_item)

/* --------------------------------
 * configuration setting
 * -------------------------------- */

typedef void (*fta_common_init_func)(fta_manager_store_item *store_item);
typedef void (*fta_common_stop_func)(fta_manager_store_item *store_item);
typedef void (*fta_common_excp_func)(fta_manager_store_item *store_item);

/* structure */
typedef struct _fta_manager_config_store_item {
  uint32 tag;                               /* tag of area                    */
  uint32 size;                              /* size of area                   */
  uint16 offset;                            /* header size                    */
  fta_common_init_func init_func;           /* initial function               */
  fta_common_stop_func stop_func;           /* terminate function             */
  fta_common_excp_func excp_func;           /* call for  exception            */
  const struct _fta_manager_store_item **set_to;  /* pointer of item for manage*/
  const void *add_param;                    /* extention parameter */
} fta_manager_config_store_item;

#define FTA_MANAGER_CONFIG_STORE_ITEM_LEN sizeof(fta_manager_config_store_item)
#define FTA_CONFIG_STORE_ITEM_DEF(tag, size, offset, init_func, stop_func, excp_func, set_to, eod) \
        {tag, size, offset, init_func, stop_func, excp_func, set_to, NULL}
#define FTA_CONFIG_STORE_ITEM_DEF2(tag, size, offset, init_func, stop_func, excp_func, set_to, add_param, eod) \
        {tag, size, offset, init_func, stop_func, excp_func, set_to, add_param}
#define FTA_CONFIG_UNDEFINED            0

/* --------------------------------
 *  log area
 * -------------------------------- */

/* header */
typedef struct _fta_log_store_header {
  fta_common_store_header cmn;          /* common header                      */
  uint32 bottom;                        /* bottom of record                   */
  uint32 firstpos;                      /* latest record                      */
  uint32 lastpos;                       /* newest record                      */
  uint32 nextpos;                       /* next record                        */
  uint8  next_log_number;               /* next log serial number             */
  uint8  rfu1;                          /* reserved                           */
  uint16 pause_count;                   /* count of pause                     */
} fta_log_store_header;
#define FTA_LOG_STORE_HEADER_LEN sizeof(fta_log_store_header)

/* header for log record */
typedef struct _fta_log_common_record {
  uint16 log_size;                      /* recorod size                       */
  uint16 prev_size;                     /* previous record size               */
  uint8  log_number;                    /* log serial number                  */
  uint8  log_kind;                      /* kind                               */
  uint16 log_level;                     /* level                              */
  uint32 time[2];                       /* time string                        */
                                        /* [0]=sec, [1]=nsec                  */
  uint32 thread_id[2];                  /* process/thread id                  */
                                        /* [0]=pid, [1]=tid                   */
} fta_log_common_record;
#define FTA_LOG_COMMON_RECORD_LEN sizeof(fta_log_common_record)
/* log_size max */
#define FTA_LOG_COMMON_RECORD_MAX_LOG_SIZE  0xFFFC
/* log_kind */
#define FTA_LOG_KIND_EXCLUDER           0       /* exclusive record           */
#define FTA_LOG_KIND_MESSAGE            1       /* message record             */
#define FTA_LOG_KIND_EVENT              2       /* event record               */
#define FTA_LOG_KIND_CONSTBLK           3       /* const record               */
#define FTA_LOG_KIND_STRUCT             4       /* data record                */

/* message record */
typedef struct _fta_log_message_record {
  fta_log_common_record cmn;            /* common header                      */
  uint16 output_line;                   /* line no                            */
  /*
   *  data of theNULL character pause 
   *  ex) file
   *      message
   */
} fta_log_message_record;
#define FTA_LOG_MESSAGE_RECORD_LEN (FTA_LOG_COMMON_RECORD_LEN + 2)

/* header for log struct */
typedef struct _fta_log_struct_record {
  fta_log_common_record cmn;            /* common header                      */
  uint16 output_line;                   /* line no                            */
  uint16 size;                          /* data size                          */
  uint32 addr;                          /* data address                       */
  /*
   * data(variable length)
   */
} fta_log_struct_record;
#define FTA_LOG_STRUCT_RECORD_LEN (FTA_LOG_COMMON_RECORD_LEN + 8)

/* function definition */
typedef boolean (*fta_log_message_filter_func)(const uint8 *srcname, uint32 info);

/* --------------------------------
 *  list area
 * -------------------------------- */

/* header */
typedef struct _fta_list_store_header {
  fta_common_store_header cmn;          /* common header                      */
  uint16 version;                       /* version                            */
  uint16 count;                         /* count of record                    */
  uint32 quota;                         /* record size                        */
  uint32 bottom;                        /* bottom of record                   */
  /* data... */
} fta_list_store_header;
#define FTA_LIST_STORE_HEADER_LEN sizeof(fta_list_store_header)
/* version */
#define FTA_LIST_VERSION_DEFAULT        1
#define FTA_LIST_VERSION_HASH           2
/* quota */
#define FTA_LIST_QUOTA_DEFAULT          0

/* data record(key:16bit) */
typedef struct _fta_list_record_16 {
  uint16 key;                           /* key                                */
  /* data... */
} fta_list_record_16;
#define FTA_LIST_RECORD_16_LEN          2

/* data record(key:32bit) */
typedef struct _fta_list_record_32 {
  uint32 key;                           /* key                               */
  /* data... */
} fta_list_record_32;
#define FTA_LIST_RECORD_32_LEN          4

/* --------------------------------
 *  var area
 * -------------------------------- */

/* header */
typedef struct _fta_var_store_header {
  fta_common_store_header cmn;          /* common header                      */
  uint16 version;                       /* version                            */
  uint16 count;                         /* count of record                    */
  uint32 quota;                         /* record size                        */
  uint32 bottom;                        /* bottom of record                   */
  /* data... */
} fta_var_store_header;
#define FTA_VAR_STORE_HEADER_LEN sizeof(fta_var_store_header)
/* version */
#define FTA_VAR_VERSION_DEFAULT         1
/* quota */
#define FTA_VAR_QUOTA_DEFAULT           0

/* data record(key:16bit) */
typedef struct _fta_var_record_16 {
  uint16 key;                           /* key                                */
  uint16 datalen;                       /* size of the record                 */
  /* data... */
} fta_var_record_16;
#define FTA_VAR_RECORD_16_LEN           4

/* data record(key:32bit) */
typedef struct _fta_var_record_32 {
  uint32 key;                           /* key                                */
  uint32 datalen;                       /* size of the record                 */
  /* data... */
} fta_var_record_32;
#define FTA_VAR_RECORD_32_LEN           8

#ifdef __cplusplus
}
#endif
#endif  /* _FTA_DEF_H */
