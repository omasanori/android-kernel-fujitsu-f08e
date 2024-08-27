/*
 * fta_var.c - FTA (Fault Trace Assistant) 
 *             variable area internal
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
#define FTA_LOG_CATEGORY_FTA_VAR        FTA_LOG_CATEGORY_FTA_DEFAULT
#define FTA_LOG_CATEGORY_SW_FTA_VAR     FTA_LOG_CATEGORY_ENABLED

/* --------------------------------
 *  static API
 * -------------------------------- */
static void *_fta_var_alloc_i(fta_var_store_header *var_header,
                              const fta_var_record_access_info *access_info, const void *key_info, uint32 alloc_len);


/* ==========================================================================
 *  INTERNAL FUNCTIONS
 * ========================================================================== */

/*!
 @brief _fta_var_init_common

 initialize for variable area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_var_init_common(fta_manager_store_item *store_item)
{
  fta_var_store_header *var_header = (fta_var_store_header *)(store_item->addr);
  
  var_header->count = 0;
  var_header->version = FTA_VAR_VERSION_DEFAULT;
  var_header->quota = FTA_VAR_QUOTA_DEFAULT;
  var_header->bottom = 0;

  var_header->cmn.enabled = TRUE;

  FTA_LIB_CHANGEBIT_SET(var_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING);

  return;
}
EXPORT_SYMBOL(_fta_var_init_common);

/*!
 @brief _fta_var_init_common2

 initialize for variable area (for ver 2)

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_var_init_common2(fta_manager_store_item *store_item)
{
  fta_var_store_header *var_header = (fta_var_store_header *)(store_item->addr);
  
  var_header->count = 0;
  var_header->version = 3;
  var_header->quota = FTA_VAR_QUOTA_DEFAULT;
  var_header->bottom = 0;

  var_header->cmn.enabled = TRUE;

  FTA_LIB_CHANGEBIT_SET(var_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING);

  return;
}
EXPORT_SYMBOL(_fta_var_init_common2);

/*!
 @brief _fta_var_stop_common

 stop for variable area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_var_stop_common(fta_manager_store_item *store_item)
{
  fta_var_store_header *var_header = (fta_var_store_header *)(store_item->addr);

  FTA_LIB_CHANGEBIT_CLR(var_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING);

  return;
}
EXPORT_SYMBOL(_fta_var_stop_common);

/*!
 @brief _fta_var_excp_common

 exceoption for variable area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_var_excp_common(fta_manager_store_item *store_item)
{
  /* do nothing */
  return;
}
EXPORT_SYMBOL(_fta_var_excp_common);

/*!
 @brief _fta_var_alloc

 allocate the list record

 @param [in] p_store_item   item for manage
 @param [in] access_info    access information
 @param [in] key_info       key
 @param [in] alloc_len      size of the record

 @retval       NULL: over the max record
 @retval     others: pointer of record
*/
void *_fta_var_alloc(const fta_manager_store_item * const *p_store_item,
                     const fta_var_record_access_info *access_info, const void *key_info, uint32 alloc_len)
{
  fta_var_store_header *var_header;
  void *var_record = NULL;

  if(p_store_item == NULL || access_info == NULL || key_info == NULL) {
    FTA_LOG_MESSAGE(FTA_VAR, FTA_LOG_LEVEL_CRITICAL, "*** FTA VAR INTERNAL ERROR", 0, 0, 0);
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

      var_header = (fta_var_store_header *)((*p_store_item)->addr);
      var_record = _fta_var_alloc_i(var_header, access_info, key_info, alloc_len);

    } while(FALSE);
    /* - - - - - - - - */
    FTA_LEAVE_LOCK();

  } else {
    var_header = (fta_var_store_header *)((*p_store_item)->addr);
    var_record = _fta_var_alloc_i(var_header, access_info, key_info, alloc_len);
  }

  return var_record;
}
EXPORT_SYMBOL(_fta_var_alloc);

/*!
 @brief _fta_var_alloc_i

 allocate the list record(internal)

 @param [in] var_header     header of the area
 @param [in] access_info    access information
 @param [in] key_info       key
 @param [in] alloc_len      size of the record

 @retval       NULL: over the max record
 @retval     others: pointer of record
*/
static void *_fta_var_alloc_i(fta_var_store_header *var_header,
                              const fta_var_record_access_info *access_info, const void *key_info, uint32 alloc_len)
{
  uint8 *var_dataarea;
  void *var_record = NULL;

  uint32 key_size;
  uint32 datalen_size;
  uint32 datalen;

  if(!FTA_LIB_CHECKBIT(var_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING)) {
    goto _fta_var_alloc_exit;
  }

  key_size = access_info->key_size;
  datalen_size = access_info->datalen_size;
  if(alloc_len < key_size + datalen_size ||
     alloc_len > var_header->cmn.size - var_header->cmn.offset) {
    goto _fta_var_alloc_exit;
  }
  datalen = alloc_len - (key_size + datalen_size);
  if((datalen_size == 0 && datalen > 0) ||
     (datalen_size == 1 && datalen > 0xFF) ||
     (datalen_size == 2 && datalen > 0xFFFF) ||
     (datalen_size == 3 && datalen > 0xFFFFFF)) {
    goto _fta_var_alloc_exit;
  }
  alloc_len = FTA_LIB_ALIGN4_32(alloc_len);

  if(access_info->check_func != NULL) {
    if(access_info->check_func(var_header, key_info, alloc_len) == FALSE) {
      goto _fta_var_alloc_exit;
    }
  }

  if(alloc_len > var_header->cmn.size - var_header->cmn.offset - var_header->bottom) {
    goto _fta_var_alloc_exit;
  }
  var_dataarea = FTA_LIB_OFFSET_ADDR(var_header, var_header->cmn.offset);

  /* append the record */
  var_record = (void *)FTA_LIB_OFFSET_ADDR(var_dataarea, var_header->bottom);
  memcpy(var_record, key_info, key_size);
  if(datalen_size > 0) {
  if(datalen_size <= sizeof(uint32)) {
    memcpy(FTA_LIB_OFFSET_ADDR(var_record, key_size), &datalen, datalen_size);
  } else {
    memcpy(FTA_LIB_OFFSET_ADDR(var_record, key_size), &datalen, sizeof(uint32));
      memset(FTA_LIB_OFFSET_ADDR(var_record, key_size + sizeof(uint32)), 0, datalen_size - sizeof(uint32));
    }
  }
  var_header->count++;
  var_header->bottom += alloc_len;

_fta_var_alloc_exit: ;

  return var_record;
}

/*!
 @brief _fta_var_before_alloc_reload

 check function before allocated

 @param [in] var_header     header of this area
 @param [in] key_info       key
 @param [in] alloc_len      size of the record

 @retval     TRUE : success
 
 @note    initialize the area when size is over
*/
boolean _fta_var_before_alloc_reload(fta_var_store_header *var_header, const void *key_info, uint32 alloc_len)
{
  if(alloc_len > var_header->cmn.size - var_header->cmn.offset - var_header->bottom) {
    var_header->bottom = 0;
    var_header->count = 0;
  }
  return TRUE;
}
EXPORT_SYMBOL(_fta_var_before_alloc_reload);

/*!
 @brief _fta_var_before_alloc_single

 check function before allocated

 @param [in] var_header     header of this area
 @param [in] key_info       key
 @param [in] alloc_len      size of the record

 @retval     TRUE : success
 
 @note    always initialize
*/
boolean _fta_var_before_alloc_single(fta_var_store_header *var_header, const void *key_info, uint32 alloc_len)
{
  var_header->bottom = 0;
  var_header->count = 0;
  return TRUE;
}
EXPORT_SYMBOL(_fta_var_before_alloc_single);

/*!
 @brief _fta_var_find

 sarch for the record

 @param [in] p_store_item   item for manage
 @param [in] key_info       key
 @param [in] alloc_len      size of the record

   @retval       NULL: not found
   @retval     others: pointer of record
*/
void *_fta_var_find(const fta_manager_store_item * const *p_store_item,
                    const fta_var_record_access_info *access_info, const void *key_info)
{
  fta_var_store_header *var_header;
  void *var_record = NULL;

  uint8 *var_dataarea = NULL;
  uint32 dataarea_size = 0;
  uint32 record_count = 0;
  uint32 record_num;
  uint32 record_pos;
  uint32 key_size;
  uint32 datalen_size;
  uint32 datalen;
  boolean bad_condition = FALSE;

  if(p_store_item == NULL || access_info == NULL || key_info == NULL) {
    FTA_LOG_MESSAGE(FTA_VAR, FTA_LOG_LEVEL_CRITICAL, "*** FTA VAR INTERNAL ERROR", 0, 0, 0);
    return NULL;
  }

  FTA_ENTER_LOCK();

  /* - - - - - - - - */
  do {
    if(access_info->lock_access) {
      if(*p_store_item == NULL) {
        bad_condition = TRUE;
        break;
      }
      if((*p_store_item)->lock_keeper->manager_status != FTA_MANAGER_STATUS_ENABLED) {
        bad_condition = TRUE;
        break;
      }
    }
    var_header = (fta_var_store_header *)((*p_store_item)->addr);
    if(var_header->cmn.enabled != FTA_STORE_ENABLED_TRUE) {
      bad_condition = TRUE;
      break;
    }
    var_dataarea = FTA_LIB_OFFSET_ADDR(var_header, var_header->cmn.offset);
    record_count = var_header->count;
    dataarea_size = var_header->bottom;

  } while(FALSE);
  /* - - - - - - - - */

  FTA_LEAVE_LOCK();
  
  if(bad_condition) {
    return var_record;
  }

  key_size = access_info->key_size;
  datalen_size = access_info->datalen_size;
  record_pos = 0;
  /* - - - - - - - - */
  for(record_num = 0; record_num < record_count; record_num++) {
    if(dataarea_size < key_size + datalen_size + record_pos) {
      var_record = NULL;
      break;
    }
    var_record = (void *)FTA_LIB_OFFSET_ADDR(var_dataarea, record_pos);
    if(memcmp(var_record, key_info, key_size) == 0) {
      /* matched */
      break;
    }
    datalen = 0;
    if(datalen_size <= sizeof(uint32)) {
      memcpy(&datalen, FTA_LIB_OFFSET_ADDR(var_record, key_size), datalen_size);
    } else {
      memcpy(&datalen, FTA_LIB_OFFSET_ADDR(var_record, key_size), sizeof(uint32));
    }
    var_record = NULL;
    datalen = FTA_LIB_ALIGN4_32(datalen);
    record_pos += key_size + datalen_size + datalen;
  }
  /* - - - - - - - - */

  return var_record;
}
EXPORT_SYMBOL(_fta_var_find);

MODULE_DESCRIPTION("FTA Driver variable area");
MODULE_LICENSE("GPL v2");
