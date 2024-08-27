/*
 * fta_log.c - FTA (Fault Trace Assistant) 
 *             log area internal
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

/* ==========================================================================
 *  INTERNAL FUNCTIONS
 * ========================================================================== */

/*!
 @brief _fta_log_init_common

 initialize for log area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_log_init_common(fta_manager_store_item *store_item)
{
  fta_log_store_header *log_header = (fta_log_store_header *)(store_item->addr);
  
  log_header->bottom          = 0;
  log_header->firstpos        = 0;
  log_header->lastpos         = 0;
  log_header->nextpos         = 0;
  log_header->next_log_number = 0;
  log_header->pause_count     = 0;

  log_header->cmn.enabled = TRUE;

  FTA_LIB_CHANGEBIT_SET(log_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING);

  return;
}
EXPORT_SYMBOL(_fta_log_init_common);

/*!
 @brief _fta_log_stop_common

 stop for log area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_log_stop_common(fta_manager_store_item *store_item)
{
  fta_log_store_header *log_header = (fta_log_store_header *)(store_item->addr);

  FTA_LIB_CHANGEBIT_CLR(log_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING);

  return;
}
EXPORT_SYMBOL(_fta_log_stop_common);

/*!
 @brief _fta_log_excp_common

 exceoption for log area

 @param [in] store_item   item for manage

 @retval     none
*/
void _fta_log_excp_common(fta_manager_store_item *store_item)
{
  /* do nothing */
  return;
}
EXPORT_SYMBOL(_fta_log_excp_common);

/*!
 @brief _fta_log_pause

 pause for log area

 @param [in] store_item   item for manage

 @retval     >=0: count of pause
 @retval      <0: failed
*/
int32 _fta_log_pause(const fta_manager_store_item * const *p_store_item)
{
  fta_log_store_header *log_header;
  int32 rc = -1;

  if(p_store_item == NULL) {
    return rc;
  }

  FTA_ENTER_LOCK();

  if(*p_store_item == NULL) {
    goto _fta_log_pause_exit;
  }
  if((*p_store_item)->lock_keeper->manager_status != FTA_MANAGER_STATUS_ENABLED) {
    goto _fta_log_pause_exit;
  }
  log_header = (fta_log_store_header *)((*p_store_item)->addr);
  rc = log_header->pause_count;
  log_header->pause_count++;

_fta_log_pause_exit: ;

  FTA_LEAVE_LOCK();

  return rc;
}
EXPORT_SYMBOL(_fta_log_pause);

/*!
 @brief _fta_log_resume

 resume the log area

 @param [in] store_item   item for manage

 @retval     >=0: count of pause
 @retval      <0: failed
*/
int32 _fta_log_resume(const fta_manager_store_item * const *p_store_item)
{
  fta_log_store_header *log_header;
  int32 rc = -1;

  if(p_store_item == NULL) {
    return rc;
  }

  FTA_ENTER_LOCK();

  if(*p_store_item == NULL) {
    goto _fta_log_resume_exit;
  }
  if((*p_store_item)->lock_keeper->manager_status != FTA_MANAGER_STATUS_ENABLED) {
    goto _fta_log_resume_exit;
  }
  log_header = (fta_log_store_header *)((*p_store_item)->addr);
  log_header->pause_count--;
  rc = log_header->pause_count;

_fta_log_resume_exit: ;

  FTA_LEAVE_LOCK();

  return rc;
}
EXPORT_SYMBOL(_fta_log_resume);

/*!
 @brief _fta_log_alloc

 allocate the log reacord
 
 @param [in] p_store_item   item for manage
 @param [in] alloc_len      record size
 @param [in] set_level      level
 @param [in] set_time       time string
 @param [in] set_thread_id  pid/tid

 @retval       NULL: failed
 @retval     others: pointer of record
*/
void *_fta_log_alloc(const fta_manager_store_item * const *p_store_item, uint16 alloc_len,
                     uint16 set_level, const uint32 *set_time, uint32 *set_thread_id)
{
  fta_log_store_header *log_header;
  uint8 *log_dataarea;
  fta_log_common_record *log_record = NULL;

  uint32 time[2] = {0};
  uint32 thread_id[2] = {0};

  uint32 allocpos;
  uint32 bottom;
  uint32 firstpos;
  uint32 nextpos;
  boolean exclusion;

  if(p_store_item == NULL) {
    return NULL;
  }

  if(set_thread_id != NULL) {
    memcpy(thread_id, set_thread_id, sizeof(thread_id));
  } else {
    FTA_LIB_GET_THREADID(thread_id);
  }

  FTA_ENTER_LOCK();

  if(set_time != NULL) {
    memcpy(time, set_time, sizeof(time));
  } else {
    FTA_LIB_GET_TIME(time);
  }

  if(*p_store_item == NULL) {
    goto _fta_log_alloc_exit;
  }
  if((*p_store_item)->lock_keeper->manager_status != FTA_MANAGER_STATUS_ENABLED) {
    goto _fta_log_alloc_exit;
  }
  log_header = (fta_log_store_header *)((*p_store_item)->addr);
  if(!FTA_LIB_CHECKBIT(log_header->cmn.statbit, FTA_STORE_STATBIT_RECORDING) ||
     log_header->pause_count > 0) {
    goto _fta_log_alloc_exit;
  }
  if(alloc_len < FTA_LOG_COMMON_RECORD_LEN                    ||
     alloc_len > FTA_LOG_COMMON_RECORD_MAX_LOG_SIZE           ||
     alloc_len > log_header->cmn.size - log_header->cmn.offset) {
    goto _fta_log_alloc_exit;
  }
  alloc_len = FTA_LIB_ALIGN4_16(alloc_len);
  log_dataarea = FTA_LIB_OFFSET_ADDR(log_header, log_header->cmn.offset);

  /* calcurate position */
  if(log_header->nextpos + alloc_len <= log_header->cmn.size - log_header->cmn.offset) {
    allocpos = log_header->nextpos;
  } else {
    allocpos = 0;
  }

  /* get latest record */
  bottom   = log_header->bottom;
  firstpos = log_header->firstpos;
  nextpos  = log_header->nextpos;
  exclusion  = FALSE;
  /* - - - - - - - - */
  while(((firstpos >= allocpos && firstpos < allocpos + alloc_len) ||
         (firstpos >= nextpos  && firstpos < nextpos  + alloc_len)) &&
          firstpos <  bottom) {

    log_record = (fta_log_common_record *) FTA_LIB_OFFSET_ADDR(log_dataarea, firstpos);
    if(log_record->log_kind == FTA_LOG_KIND_EXCLUDER) {
      exclusion = TRUE;
      break;
    }
    firstpos += log_record->log_size;

    if(firstpos >= bottom) {
      if(allocpos == 0 && firstpos == nextpos) {
        bottom   = 0;
        firstpos = 0;
        nextpos  = 0;
        break;
      }
      firstpos = 0;
      if( allocpos != nextpos ) {
        bottom = nextpos;
        continue;
      }
      bottom = allocpos + alloc_len;
      break;
    }
  }
  /* - - - - - - - - */

  if(exclusion) {
    log_record = NULL;
    goto _fta_log_alloc_exit;
  }

  if(firstpos != allocpos) {
    log_record = (fta_log_common_record *) FTA_LIB_OFFSET_ADDR(log_dataarea, firstpos);
    log_record->prev_size = 0;
  }

  log_record = (fta_log_common_record *) FTA_LIB_OFFSET_ADDR(log_dataarea, allocpos);
  log_record->log_size = alloc_len;
  if(firstpos != allocpos) {
    log_record->prev_size = ((fta_log_common_record *) FTA_LIB_OFFSET_ADDR(log_dataarea, log_header->lastpos))->log_size;
  } else {
    log_record->prev_size = 0;
  }
  log_record->log_number = log_header->next_log_number;
  log_record->log_kind = FTA_LOG_KIND_EXCLUDER;
  log_record->log_level = set_level;
  memcpy(log_record->time, time, sizeof(log_record->time));
  memcpy(log_record->thread_id, thread_id, sizeof(log_record->thread_id));

  if(bottom < allocpos + alloc_len) {
    bottom = allocpos + alloc_len;
  }
  log_header->bottom   = bottom;
  log_header->firstpos = firstpos;
  log_header->lastpos  = allocpos;
  log_header->nextpos  = allocpos + alloc_len;
  log_header->next_log_number++;

_fta_log_alloc_exit: ;

  FTA_LEAVE_LOCK();

  return log_record;
}
EXPORT_SYMBOL(_fta_log_alloc);

/*!
 @brief _fta_log_message

 output the log record

 @param [in] p_store_item   item for manage
 @param [in] srcname        filename
 @param [in] info           info(high 16bit=line,low 16bit=level)
 @param [in] msgformat      format
 @param [in] arg1           argument #1
 @param [in] arg2           argument #2
 @param [in] arg3           argument #3

 @retval     TRUE : success
 @retval     FALSE: failed
*/
boolean _fta_log_message(const fta_manager_store_item * const *p_store_item,
                      const uint8 *srcname, uint32 info, const uint8 *msgformat,
                      uint32 arg1, uint32 arg2, uint32 arg3)
{
  int32 srcname_len;
  int32 message_len;
  uint16 alloc_len;
  fta_log_message_record *log_message_record;

  srcname_len = fta_lib_strlen(srcname) + 1;
  message_len = fta_lib_scprintf(msgformat, 3, arg1, arg2, arg3) + 1;

  if(srcname_len <= 0 || message_len <= 0 ||
     FTA_LOG_MESSAGE_RECORD_LEN + srcname_len + message_len > FTA_LOG_COMMON_RECORD_MAX_LOG_SIZE) {
    return FALSE;
  }
  alloc_len = (uint16)(FTA_LOG_MESSAGE_RECORD_LEN + srcname_len + message_len);

  log_message_record = (fta_log_message_record *) _fta_log_alloc(p_store_item, alloc_len,
                       FTA_LOG_MESSAGE_INFO_GET_LEVEL(info), NULL, NULL);
  if(log_message_record == NULL) {
    return FALSE;
  }

  log_message_record->output_line = FTA_LOG_MESSAGE_INFO_GET_LINE(info);
  memcpy(FTA_LIB_OFFSET_ADDR(log_message_record, FTA_LOG_MESSAGE_RECORD_LEN), srcname, srcname_len);
  fta_lib_snprintf(FTA_LIB_OFFSET_ADDR(log_message_record, FTA_LOG_MESSAGE_RECORD_LEN + srcname_len),
                   message_len, msgformat, 3, arg1, arg2, arg3);
  *FTA_LIB_OFFSET_ADDR(log_message_record, alloc_len - 1) = '\0';

  log_message_record->cmn.log_kind = FTA_LOG_KIND_MESSAGE;

  return TRUE;
}
EXPORT_SYMBOL(_fta_log_message);

/*!
 @brief _fta_log_message_filter_common

 filter function

 @param [in] srcname        filename
 @param [in] info           info(high 16bit=line,low 16bit=level)

 @retval     TRUE : permit
 @retval     FALSE: prohibit
*/
boolean _fta_log_message_filter_common(const uint8 *srcname, uint32 info)
{
  return (((info & FTA_LOG_SCENE_FILTER_MASK) == 0) ? TRUE : FALSE);
}
EXPORT_SYMBOL(_fta_log_message_filter_common);

MODULE_DESCRIPTION("FTA Driver log area");
MODULE_LICENSE("GPL v2");
