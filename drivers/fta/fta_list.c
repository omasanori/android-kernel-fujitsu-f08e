/*
 * fta_list.c - FTA (Fault Trace Assistant) 
 *              list area internals
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
 *  static API
 * -------------------------------- */
static void *_fta_list_get_record_i(fta_list_store_header *list_header,
                                    const fta_list_record_access_info *access_info,
                                    const void *key_info, boolean arrival);


/* ==========================================================================
 *  INTERNAL FUNCTIONS
 * ========================================================================== */

/*!
 @brief _fta_list_init_common

 initialize for list area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_list_init_common(fta_manager_store_item *store_item)
{
  fta_list_store_header *list_header = (fta_list_store_header *)(store_item->addr);
  
  list_header->count = 0;
  list_header->version = FTA_LIST_VERSION_DEFAULT;
  list_header->quota = FTA_LIST_QUOTA_DEFAULT;
  list_header->bottom = 0;

  list_header->cmn.enabled = TRUE;

  FTA_LIB_CHANGEBIT_SET(list_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING);

  return;
}
EXPORT_SYMBOL(_fta_list_init_common);

/*!
 @brief _fta_list_stop_common

 stop for list area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_list_stop_common(fta_manager_store_item *store_item)
{
  fta_list_store_header *list_header = (fta_list_store_header *)(store_item->addr);

  FTA_LIB_CHANGEBIT_CLR(list_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING);

  return;
}
EXPORT_SYMBOL(_fta_list_stop_common);

/*!
 @brief _fta_list_excp_common

 exceoption for list area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_list_excp_common(fta_manager_store_item *store_item)
{
  /* do nothing */
  return;
}
EXPORT_SYMBOL(_fta_list_excp_common);

/*!
 @brief _fta_list_get_record

 get and allocate the list record

 @param [in] p_store_item   item for manage
 @param [in] access_info    access information
 @param [in] key_info       key
 @param [in] arrival        TRUE  : append the record when not found
                            FALSE : only search

 @retval       NULL: not found / over the max record
 @retval     others: pointer of record
*/
void *_fta_list_get_record(const fta_manager_store_item * const *p_store_item,
                           const fta_list_record_access_info *access_info,
                           const void *key_info, boolean arrival)
{
  fta_list_store_header *list_header;
  void *list_record = NULL;

 	if(p_store_item == NULL || access_info == NULL || key_info == NULL) {
    FTA_LOG_MESSAGE(FTA_LIST, FTA_LOG_LEVEL_CRITICAL, "*** FTA LIST INTERNAL ERROR", 0, 0, 0);
    return NULL;
  }
	if((access_info->index_array != NULL && access_info->compare_func == NULL) ||
     (FTA_LIB_ALIGN4_32(access_info->fixed_len) < access_info->key_size)) {
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
      list_record = _fta_list_get_record_i(list_header, access_info, key_info, arrival);

    } while(FALSE);
    /* - - - - - - - - */
    FTA_LEAVE_LOCK();

  } else {
    list_header = (fta_list_store_header *)((*p_store_item)->addr);
    list_record = _fta_list_get_record_i(list_header, access_info, key_info, arrival);
  }

  return list_record;
}
EXPORT_SYMBOL(_fta_list_get_record);

/*!
 @brief _fta_list_get_record_i

 get and allocate the list record(internal)

 @param [in] list_header    header of the area
 @param [in] access_info    access information
 @param [in] key_info       key
 @param [in] arrival        TRUE  : append the record when not found
                            FALSE : only search

 @retval       NULL: not found / over the max record
 @retval     others: pointer of record
*/
static void *_fta_list_get_record_i(fta_list_store_header *list_header,
                                    const fta_list_record_access_info *access_info,
                                    const void *key_info, boolean arrival)
{
  uint8 *list_dataarea;
  void *list_record = NULL;

  uint32 fixed_len;
  uint32 key_size;

  fta_list_compare_index_id_type record_num;
  boolean add_new;

  if(arrival) {
    if(!FTA_LIB_CHECKBIT(list_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING)) {
      goto _fta_list_get_record_exit;
    }
  } else {
    if(list_header->cmn.enabled != FTA_STORE_ENABLED_TRUE) {
      goto _fta_list_get_record_exit;
    }
  }

  fixed_len = FTA_LIB_ALIGN4_32(access_info->fixed_len);
  key_size  = access_info->key_size;
  if(fixed_len < key_size ||
     fixed_len > list_header->cmn.size - list_header->cmn.offset) {
    goto _fta_list_get_record_exit;
  }
  list_dataarea = FTA_LIB_OFFSET_ADDR(list_header, list_header->cmn.offset);

  add_new = TRUE;
  if(list_header->count > 0) {
    /* search record */
    if(access_info->index_array != NULL) {
      /* - - - - - - - - */
      for(record_num = 0; record_num < list_header->count; record_num++) {
        list_record = (void *)FTA_LIB_OFFSET_ADDR(list_dataarea, record_num * fixed_len);
        if(memcmp(list_record, key_info, key_size) == 0) {
          add_new = FALSE;
          break;
        }
      }
      /* - - - - - - - - */
    }
  }
  if(add_new) {
    if(arrival) {
      /* append record */
      if(fixed_len * (list_header->count + 1) > list_header->cmn.size - list_header->cmn.offset) {
        goto _fta_list_get_record_exit;
      }
      record_num = list_header->count;
      list_record = (void *)FTA_LIB_OFFSET_ADDR(list_dataarea, record_num * fixed_len);
      memcpy(list_record, key_info, key_size);
      list_header->count++;
      list_header->bottom = list_header->count * fixed_len;
      
    } else {
      /* not found */
      list_record = NULL;
    }
  }

_fta_list_get_record_exit: ;

  return list_record;
}

MODULE_DESCRIPTION("FTA Driver list area  ");
MODULE_LICENSE("GPL v2");
