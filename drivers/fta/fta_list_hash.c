/*
 * fta_list_hash.c - FTA (Fault Trace Assistant) 
 *              list hash area internals
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

/* ==========================================================================
 *  INCLUDE HEADER
 * ========================================================================== */

#include "fta_i.h"

/* ==========================================================================
 *  DEFINITION
 * ========================================================================== */
/* --------------------------------
 *  option
 * -------------------------------- */
#define FTA_LOG_CATEGORY_FTA_LIST       FTA_LOG_CATEGORY_FTA_DEFAULT
#define FTA_LOG_CATEGORY_SW_FTA_LIST    FTA_LOG_CATEGORY_ENABLED

/* --------------------------------
 *  internal use
 * -------------------------------- */
#define FTA_LISTEX_HASH_ID_EMPTY_RESET    0xFF
#define FTA_LISTEX_HASH_BUF_EMPTY_BEGIN   (access_info->entry_max)
#define FTA_LISTEX_HASH_BUF_EMPTY_END     (access_info->entry_max + 1)
#define FTA_LISTEX_HASH_BUF_HASHEDINDEX   (access_info->entry_max + 2)

/* --------------------------------
 *  static API
 * -------------------------------- */
static void *_fta_listex_hash_record_i(fta_list_store_header *list_header,
                                       const fta_listex_hash_access_info *access_info,
                                       const void *key_info, uint32 access_type);


/* ==========================================================================
 *  INTERNAL FUNCTIONS
 * ========================================================================== */

/*!
 @brief _fta_listex_hash_init_common

 initialize for list hash area

 @param [in] store_item   item for manage

 @retval    none

 @note      it is necessary to use initialize macro FTA_CONFIG_STORE_ITEM_DEF2
 @note      instead of FTA_CONFIG_STORE_ITEN_DEF in configuration.
*/
void _fta_listex_hash_init_common(fta_manager_store_item *store_item)
{
  fta_list_store_header *list_header = (fta_list_store_header *)(store_item->addr);
  const fta_listex_hash_access_info *access_info;
  uint32 fixed_len;

  /* initialize */
  list_header->count = 0;
  list_header->version = FTA_LIST_VERSION_HASH;
  list_header->quota = FTA_LIST_QUOTA_DEFAULT;
  list_header->bottom = 0;

  /* set to record manager */
  access_info = store_item->def->add_param;
  if(access_info == NULL) {
    goto _fta_listex_hash_init_common_exit;
  }

  /* check record manager */
  fixed_len = FTA_LIB_ALIGN4_32(access_info->fixed_len);
  if((fixed_len < access_info->key_size) ||
     (fixed_len > list_header->cmn.size - list_header->cmn.offset) ||
     (access_info->entry_max == 0) ||
     (access_info->entry_max >= FTA_LISTEX_HASH_ID_EMPTY) ||
     (access_info->entry_max * fixed_len >= list_header->cmn.size - list_header->cmn.offset) ||
     (access_info->hash_size == 0) ||
     (access_info->work_buffer == 0)) {
    goto _fta_listex_hash_init_common_exit;
  }

  /* initialize wake area */
  memset(access_info->work_buffer, FTA_LISTEX_HASH_ID_EMPTY_RESET,
         FTA_LISTEX_HASH_WORK_MAX(access_info->entry_max, access_info->hash_size) *
         sizeof(fta_listex_hash_id_type));

  list_header->cmn.enabled = TRUE;

  FTA_LIB_CHANGEBIT_SET(list_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING);

  return;

_fta_listex_hash_init_common_exit: ;
  FTA_LOG_MESSAGE(FTA_LIST, FTA_LOG_LEVEL_CRITICAL, "*** FTA LIST CONFIG ERROR", 0, 0, 0);

  return;
}

/*!
 @brief _fta_listex_hash_stop_common

 stop for list hash area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_listex_hash_stop_common(fta_manager_store_item *store_item)
{
  _fta_list_stop_common(store_item);
  return;
}

/*!
 @brief _fta_listex_hash_excp_common

 exceoption for lish hash area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_listex_hash_excp_common(fta_manager_store_item *store_item)
{
  _fta_list_excp_common(store_item);
  return;
}

/*!
 @brief _fta_lisext_hash_record

 operate the list hash record

 @param [in] p_store_item   item for manage
 @param [in] access_info    access information
 @param [in] key_info       key
 @param [in] access_type    FTA_LISTEX_HASH_ACCESS_GET: get record
                            FTA_LISTEX_HASH_ACCESS_ADD: add record
                            FTA_LISTEX_HASH_ACCESS_DEL: del record

 @retval       NULL: not found / over the max record
 @retval     others: pointer of record
*/
void *_fta_listex_hash_record(const fta_manager_store_item * const *p_store_item,
                            const fta_listex_hash_access_info *access_info,
                            const void *key_info, uint32 access_type)
{
  fta_list_store_header *list_header;
  void *list_record = NULL;

  /* parameter check */
  if(p_store_item == NULL || access_info == NULL || key_info == NULL ||
     access_type >= FTA_LISTEX_HASH_ACCESS_MAX) {
    FTA_LOG_MESSAGE(FTA_LIST, FTA_LOG_LEVEL_CRITICAL, "*** FTA LIST INTERNAL ERROR", 0, 0, 0);
    return NULL;
  }
  if(access_info->work_buffer == NULL) {
    FTA_LOG_MESSAGE(FTA_LIST, FTA_LOG_LEVEL_CRITICAL, "*** FTA LIST CONFIG ERROR", 0, 0, 0);
    return NULL;
  }

  if(access_info->lock_access) {

    FTA_ENTER_LOCK();
    /* - - - - - - - - */
    do {
      if(*p_store_item == NULL) {
        break;
      }
      if((*p_store_item)->lock_keeper->manager_status != FTA_MANAGER_STATUS_ENABLED) {
        break;
      }

      list_header = (fta_list_store_header *)((*p_store_item)->addr);
      list_record = _fta_listex_hash_record_i(list_header, access_info, key_info, access_type);

    } while(FALSE);
    /* - - - - - - - - */
    FTA_LEAVE_LOCK();

  } else {
    list_header = (fta_list_store_header *)((*p_store_item)->addr);
    list_record = _fta_listex_hash_record_i(list_header, access_info, key_info, access_type);
  }

  return list_record;
}

/*!
 @brief _fta_lisext_hash_record?i

 internal functionn of operating the list hash record

 @param [in] p_store_item   item for manage
 @param [in] access_info    access information
 @param [in] key_info       key
 @param [in] access_type    FTA_LISTEX_HASH_ACCESS_GET: get record
                            FTA_LISTEX_HASH_ACCESS_ADD: add record
                            FTA_LISTEX_HASH_ACCESS_DEL: del record

 @retval       NULL: not found / over the max record
 @retval     others: pointer of record
*/
static void *_fta_listex_hash_record_i(fta_list_store_header *list_header,
                                     const fta_listex_hash_access_info *access_info,
                                     const void *key_info, uint32 access_type)
{
  uint8 *list_dataarea;
  void *list_record = NULL;

  uint32 fixed_len;
  uint32 key_size;
  uint32 record_par;
  uint32 record_num;

  fta_listex_hash_id_type *work_buffer;
  boolean not_found;

  /* check of a recordable state or an effective state */
  if(access_type != FTA_LISTEX_HASH_ACCESS_GET) {
    if(!FTA_LIB_CHECKBIT(list_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING)) {
      goto _fta_listex_hash_record_exit;
    }
  } else {
    if(list_header->cmn.enabled != FTA_STORE_ENABLED_TRUE) {
      goto _fta_listex_hash_record_exit;
    }
  }

  fixed_len = FTA_LIB_ALIGN4_32(access_info->fixed_len);
  key_size = access_info->key_size;
  list_dataarea = FTA_LIB_OFFSET_ADDR(list_header, list_header->cmn.offset);
  work_buffer = access_info->work_buffer;

  /* registered check */
  not_found = TRUE;
  record_par = 0;
  memcpy(&record_par, key_info, (key_size > sizeof(uint32) ? sizeof(uint32) : key_size));
  record_par %= access_info->hash_size; /* only to 4 bytes of low rank */
  record_par += FTA_LISTEX_HASH_BUF_HASHEDINDEX;
  if(list_header->count > 0) {
    /* search for the same item */
    record_num = work_buffer[record_par];
    while(record_num != FTA_LISTEX_HASH_ID_EMPTY) {
      list_record = (void *)FTA_LIB_OFFSET_ADDR(list_dataarea, record_num * fixed_len);
      if(memcmp(key_info, list_record, key_size) == 0) {
        not_found = FALSE;
        break;
      }
      record_par = record_num;
      record_num = work_buffer[record_par];
    }
  }

  if(not_found) {

    /* add record */
    if(access_type == FTA_LISTEX_HASH_ACCESS_ADD) {

      if(list_header->count == 0) {
        /* simple work area check only once after initialization */
        if(work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_BEGIN] != FTA_LISTEX_HASH_ID_EMPTY ||
           work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_END]   != FTA_LISTEX_HASH_ID_EMPTY ||
           work_buffer[FTA_LISTEX_HASH_BUF_HASHEDINDEX] != FTA_LISTEX_HASH_ID_EMPTY ) {
          FTA_LOG_MESSAGE(FTA_LIST, FTA_LOG_LEVEL_CRITICAL, "*** FTA LIST CONFIG ERROR", 0, 0, 0);
          list_record = NULL;
          goto _fta_listex_hash_record_exit;
        }
      }

      /* a new record is added */
      if((uint32)(list_header->count) >= access_info->entry_max) {
        
        /* acquires from an empty list when the number of registration is a maximum */
        record_num = work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_BEGIN];
        if(record_num == FTA_LISTEX_HASH_ID_EMPTY) {
          list_record = NULL;
          goto _fta_listex_hash_record_exit;
        }
        /* deletes from an empty list */
        work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_BEGIN] = work_buffer[record_num];
        if(work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_END] == record_num) {
          work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_END] = FTA_LISTEX_HASH_ID_EMPTY;
        }

      } else {

        /* adds a new record until the number of registration is a maximum*/
        record_num = list_header->count;
        list_header->count++;
        list_header->bottom = list_header->count * fixed_len;

      }
      /* adds to hash table */
      work_buffer[record_num] = work_buffer[record_par];
      work_buffer[record_par] = (fta_listex_hash_id_type)record_num;

      /* return the record position */
      list_record = (void *)FTA_LIB_OFFSET_ADDR(list_dataarea, record_num * fixed_len);
      memcpy(list_record, key_info, key_size);

    } else {
      /* do noting */
      list_record = NULL;
    }

  } else {

    /* delete record */
    if(access_type == FTA_LISTEX_HASH_ACCESS_DEL) {

      /* set the delete bit */
      ((uint8*)list_record)[key_size - 1] |= 0x80;
      /* deletes from the list and adds to an empty entry */
      work_buffer[record_par] = work_buffer[record_num];
      work_buffer[record_num] = FTA_LISTEX_HASH_ID_EMPTY;
      if(work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_BEGIN] == FTA_LISTEX_HASH_ID_EMPTY) {
        /* with no empty entry */
        work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_BEGIN] = (fta_listex_hash_id_type)record_num;
        work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_END] = (fta_listex_hash_id_type)record_num;
      } else {
        /* add to a termination */
        work_buffer[work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_END]] = (fta_listex_hash_id_type)record_num;
        work_buffer[FTA_LISTEX_HASH_BUF_EMPTY_END] = (fta_listex_hash_id_type)record_num;
      }
    }
  }

_fta_listex_hash_record_exit: ;

  return list_record;
}
