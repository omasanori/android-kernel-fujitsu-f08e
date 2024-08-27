/*
 * fta_i.h - FTA (Fault Trace Assistant) 
 *           Internal header
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

#ifndef _FTA_I_H
#define _FTA_I_H
#ifdef __cplusplus
extern "C" {
#endif


/* ==========================================================================
 *  INCLUDE HEADER
 * ========================================================================== */
#include <stdarg.h>

#include <linux/module.h>
#include <linux/string.h>
#include <linux/fta.h>
#include "fta_config.h"


/* ==========================================================================
 *  DEFINITION
 * ========================================================================== */

/* --------------------------------
 *  option
 * -------------------------------- */
#define FTA_LOG_CATEGORY_FTA_ADMIN      FTA_LOG_CATEGORY_FTA_DEFAULT
#define FTA_LOG_CATEGORY_SW_FTA_ADMIN   FTA_LOG_CATEGORY_ENABLED

/* --------------------------------
 *  for manage
 * -------------------------------- */
#define FTA_CALC_MANAGER_SIZE(COUNT)    (FTA_MANAGER_STORE_HEADER_LEN + (FTA_MANAGER_STORE_ITEM_LEN * COUNT))

/* state of initialize */
#define FTA_MANAGER_STATUS_UNINIT         0       /* not initialize           */
#define FTA_MANAGER_STATUS_ENABLED        1       /* enabled                  */
#define FTA_MANAGER_STATUS_STOP           2       /* stopped                  */

typedef struct _fta_lock_keeper_info {
  uint8  manager_status;
  fta_manager_store_header *fta_manager;
} fta_lock_keeper_info;

typedef struct _fta_manager_initializer_info {
  fta_manager_store_header *fta_manager;
  const fta_manager_config_store_item * const *p_config_items;
  uint32 config_count;
  void *memory_addr;
  uint32 memory_size;
  fta_lock_keeper_info *lock_keeper;
} fta_manager_initializer_info;
#define FTA_MANAGER_INITIALIZER_INFO_DEF(  \
        fta_manager, config_items, config_count, memory_addr, \
        memory_size, lock_keeper, eod) \
        {fta_manager, config_items, config_count, memory_addr, \
        memory_size, lock_keeper}

/* --------------------------------
 *  common definition
 * -------------------------------- */

#define FTA_LIB_BIT(n)                  (1<<n)
#define FTA_LIB_CHECKBIT(b,n)           (((b)&FTA_LIB_BIT(n))!=0)
#define FTA_LIB_CHANGEBIT_SET(b,n)      ((b)|=FTA_LIB_BIT(n))
#define FTA_LIB_CHANGEBIT_CLR(b,n)      ((b)&=~FTA_LIB_BIT(n))
#define FTA_LIB_SETBITVALUE(b, n)       ((b)=(n))
#define FTA_LIB_OFFSET_ADDR(a,o)        (&((char *)(a))[o])
#define FTA_LIB_ALIGN4_32(a)            (((uint32)(a) + 3) & ~0x00000003L)
#define FTA_LIB_ALIGN4_16(a)            (((uint16)(a) + 3) & ~0x0003)

/* --------------------------------
 *  data structure
 * -------------------------------- */
/* LIST */
typedef int (*fta_list_key_compare_func)(const void *, const void *);
typedef uint32 fta_list_compare_index_id_type;
#define FTA_LIST_COMPARE_INDEX_LEN(key_size) (sizeof(fta_list_compare_index_id_type) + key_size)
#define FTA_LIST_GET_MAX_RECORD(fixed_len, areasize) ((areasize) / FTA_LIB_ALIGN4_32(fixed_len))
typedef struct fta_list_record_access_info {
  uint32 fixed_len;                 /* [I] length of the record               */
  uint32 key_size;                  /* [I] size of key                        */
  void *index_array;                /* [IO] array for search ((key_size+4)*n) */
  fta_list_key_compare_func compare_func; /* [I] function for binary search   */
  boolean lock_access;              /* [I] TRUE  = use exclusive              */
                                    /*     FALSE = do not use exclusive       */
} fta_list_record_access_info;
#define FTA_LIST_RECORD_ACCESS_INFO_DEF(fixed_len, key_size, index_array, compare_func, lock_access, eod) \
                                       {fixed_len, key_size, index_array, compare_func, lock_access}
/* LIST HASH */
typedef uint16 fta_listex_hash_id_type;
#define FTA_LISTEX_HASH_ID_EMPTY    0xFFFF
#define FTA_LISTEX_HASH_WORK_MAX(entry_num, hash_num) (entry_num + hash_num + 2)
typedef struct fta_listex_hash_access_info {
  uint32 fixed_len;                 /* [I] length of the record               */
  uint32 key_size;                  /* [I] size of key                        */
  uint32 entry_max;                 /* [I] the max number of entry (<65535)   */
  uint32 hash_size;                 /* [I] size of hash table                 */
  fta_listex_hash_id_type *work_buffer; /* [IO] work area (not NULL)          */
  boolean lock_access;              /* [I] TRUE  = use exclusive              */
                                    /*     FALSE = do not use exclusive       */
} fta_listex_hash_access_info;
#define FTA_LISTEX_HASH_ACCESS_INFO_DEF(fixed_len, key_size, entry_max, hash_size, work_buffer, lock_access, eod) \
                                     {fixed_len, key_size, entry_max, hash_size, work_buffer, lock_access}
#define FTA_LISTEX_HASH_ACCESS_GET    0
#define FTA_LISTEX_HASH_ACCESS_ADD    1
#define FTA_LISTEX_HASH_ACCESS_DEL    2
#define FTA_LISTEX_HASH_ACCESS_MAX    3

/* VAR  */
typedef boolean (*fta_var_before_alloc_func)(fta_var_store_header *var_header, const void *key_info, uint32 alloc_len);
typedef struct _fta_var_record_access_info {
  uint32 key_size;                  /* [I] size of key                        */
  uint32 datalen_size;              /* [I] size of data length                */
  fta_var_before_alloc_func check_func; /* [I] check for allocate area        */
  boolean lock_access;              /* [I] TRUE  = use exclusive              */
                                    /*     FALSE = do not use exclusive       */
} fta_var_record_access_info;
#define FTA_VAR_RECORD_ACCESS_INFO_DEF(key_size, datalen_size, overload_type, lock_access, eod) \
                                      {key_size, datalen_size, overload_type, lock_access}


/* --------------------------------
 *  API
 * -------------------------------- */

/* admin */
boolean _fta_manage_check_exportable(fta_manager_store_header *fta_manager, uint32 limit_size);
boolean _fta_manage_initialize(const fta_manager_initializer_info *manager_initializer);
void _fta_manage_initialize_store_items(fta_manager_store_header *fta_manager);
void _fta_common_init_common(fta_manager_store_item *store_item);
void _fta_manage_terminate_store_items(fta_manager_store_header *fta_manager);
fta_manager_store_item *_fta_manage_get_store_item(fta_manager_store_header *fta_manager, uint32 tag);
/* lib */
uint16 fta_lib_check_parity16(uint16 *check_addr, uint32 count, uint16 start);
int32 fta_lib_snprintf(uint8 *buf, uint32 len, const uint8 *msgformat, uint32 argc, ...);
int32 fta_lib_scprintf(const uint8 *msgformat, uint32 argc, ...);
int32 fta_lib_vsnprintf(uint8 *buf, uint32 len, const uint8 *msgformat, uint32 argc, va_list argv);
int32 fta_lib_strlen(const uint8 *string);
int32 fta_lib_strnlen(const uint8 *string, uint32 maxlen);

/* log */
void _fta_log_init_common(fta_manager_store_item *store_item);
void _fta_log_stop_common(fta_manager_store_item *store_item);
void _fta_log_excp_common(fta_manager_store_item *store_item);
int32 _fta_log_pause(const fta_manager_store_item * const *p_store_item);
int32 _fta_log_resume(const fta_manager_store_item * const *p_store_item);
void *_fta_log_alloc(const fta_manager_store_item * const *p_store_item, uint16 alloc_len,
                     uint16 set_level, const uint32 *set_time, uint32 *set_thread_id);
boolean _fta_log_message(const fta_manager_store_item * const *p_store_item,
                         const uint8 *srcname, uint32 info, const uint8 *msgformat,
                         uint32 arg1, uint32 arg2, uint32 arg3);
boolean _fta_log_message_filter_common(const uint8 *srcname, uint32 info);

/* list */
void _fta_list_init_common(fta_manager_store_item *store_item);
void _fta_list_stop_common(fta_manager_store_item *store_item);
void _fta_list_excp_common(fta_manager_store_item *store_item);
void *_fta_list_get_record(const fta_manager_store_item * const *p_store_item,
                           const fta_list_record_access_info *access_info, 
                           const void *key_info, boolean arrival);
/* list hash */
void _fta_listex_hash_init_common(fta_manager_store_item *store_item);
void _fta_listex_hash_stop_common(fta_manager_store_item *store_item);
void _fta_listex_hash_excp_common(fta_manager_store_item *store_item);
void *_fta_listex_hash_record(const fta_manager_store_item * const *p_store_item,
                              const fta_listex_hash_access_info *access_info,
                              const void *key_info, uint32 access_type);
/* var */
void _fta_var_init_common(fta_manager_store_item *store_item);
void _fta_var_init_common2(fta_manager_store_item *store_item);
void _fta_var_stop_common(fta_manager_store_item *store_item);
void _fta_var_excp_common(fta_manager_store_item *store_item);
void *_fta_var_alloc(const fta_manager_store_item * const *p_store_item,
                     const fta_var_record_access_info *access_info, const void *key_info, uint32 alloc_len);
boolean _fta_var_before_alloc_reload(fta_var_store_header *var_header, const void *key_info, uint32 alloc_len);
boolean _fta_var_before_alloc_single(fta_var_store_header *var_header, const void *key_info, uint32 alloc_len);
void *_fta_var_find(const fta_manager_store_item * const *p_store_item,
                    const fta_var_record_access_info *access_info, const void *key_info);

#ifdef __cplusplus
}
#endif
#endif  /* _FTA_I_H */
