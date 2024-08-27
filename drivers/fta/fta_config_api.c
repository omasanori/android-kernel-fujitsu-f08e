/*
 * fta_config_api.c - FTA (Fault Trace Assistant) 
 *                    API for configuration
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

/* filter */
fta_log_message_filter_func fta_log_message_filter_register = NULL;

/* structure */
static const fta_var_record_access_info fta_var_alloc_log_access_info =
  FTA_VAR_RECORD_ACCESS_INFO_DEF(
    sizeof(uint32),                                           /* key_size     */
    sizeof(uint32),                                           /* datalen_size */
    NULL,                                                     /* check_func   */
    TRUE,                                                     /* lock_access  */
  );
static const fta_var_record_access_info fta_var_alloc_terminfo_access_info =
  FTA_VAR_RECORD_ACCESS_INFO_DEF(
    sizeof(uint32),                                           /* key_size     */
    sizeof(uint32),                                           /* datalen_size */
    NULL,                                                     /* check_func   */
    TRUE,                                                     /* lock_access  */
  );
struct fta_convert_table_type {
   uint16 label;
   uint8  area;
};
struct fta_convert_table_type fta_convert_table[] = {
   {0x0000,0 },
   {0x0001,1 },
   {0x0002,2 },
   {0x0003,3 },
   {0x0004,4 },
   {0x0005,5 },
   {0x0006,2 },
   {0x000a,8 },
};

/* ==========================================================================
 *  FUNCTIONS
 * ========================================================================== */
/* EXTERNAL FUNCTION */
extern const fta_manager_config_store_item * const fta_initial_config_stores[FTA_CONFIG_STORE_INITIAL_COUNT];


/* --------------------------------------------------------------------------
 *  log API
 * -------------------------------------------------------------------------- */
/*!
 @brief fta_log_message

 print log message (fixed arguments)

 @param [in] srcname   filename
 @param [in] info      info(high 16bit=line,low 16bit=level)
 @param [in] msgformat format
 @param [in] arg1      argument #1
 @param [in] arg2      argument #2
 @param [in] arg3      argument #3

 @retval     none
*/
void fta_log_message(const uint8 *srcname, uint32 info, const uint8 *msgformat,
                          uint32 arg1, uint32 arg2, uint32 arg3)
{
  fta_log_message_filter_func filter_func;
  int32 srcname_len=0;

  filter_func = fta_log_message_filter_register;
  if(filter_func != NULL) {
    if(filter_func(srcname, info) == FALSE) {
      return;
    }
  }

  if(srcname != NULL) {
    srcname_len = fta_lib_strlen(srcname);
    if(srcname_len < 0)
    srcname = "";
    else if(srcname_len > 31)
    srcname = &srcname[srcname_len - 31];
  } else
    srcname = "";

  _fta_log_message(&fta_reference.fta_store_item_kmsg, srcname, info, msgformat, arg1, arg2, arg3);

}
EXPORT_SYMBOL(fta_log_message);

/*!
 @brief fta_log_message_kmsg

 print log message (for printk)

 @param [in] srcname   filename
 @param [in] info      info(high 16bit=line,low 16bit=level)
 @param [in] time      time string
 @param [in] msg       message
 @param [in] msglen    size of message (included NULL character)

 @retval     none
*/
void fta_log_message_kmsg(const uint8 *srcname, uint32 info, const uint8 *time, const uint8 *msg,uint32 msglen)
{
  int32 srcname_len = 0;
  int32 time_len = 0;
  uint16 alloc_len;
  fta_log_message_record *log_message_record;

  if(srcname != NULL) {
    srcname_len = fta_lib_strlen(srcname);
    if(srcname_len < 0) {
    srcname = "";
      srcname_len = 0;
    } else if(srcname_len > 31) {
    srcname = &srcname[srcname_len - 31];
    srcname_len = 31;
  }
  } else
    srcname = "";
  srcname_len++;

  if(time != NULL) {
    time_len = fta_lib_strlen(time);
    if(time_len < 0) {
      time = "";
      time_len = 0;
    }
  } else
    time = "";
  
  alloc_len = (uint16)(FTA_LOG_MESSAGE_RECORD_LEN + srcname_len  + time_len + msglen);

  log_message_record = (fta_log_message_record *) _fta_log_alloc(&fta_reference.fta_store_item_kmsg, alloc_len, 
                       FTA_LOG_MESSAGE_INFO_GET_LEVEL(info), NULL, NULL);
  if(log_message_record == NULL) {
      goto end;
  }

  log_message_record->output_line = FTA_LOG_MESSAGE_INFO_GET_LINE(info);
  memcpy(FTA_LIB_OFFSET_ADDR(log_message_record, FTA_LOG_MESSAGE_RECORD_LEN), srcname, srcname_len);
  *FTA_LIB_OFFSET_ADDR(log_message_record, FTA_LOG_MESSAGE_RECORD_LEN + srcname_len - 1) = '\0';
  memcpy(FTA_LIB_OFFSET_ADDR(log_message_record, FTA_LOG_MESSAGE_RECORD_LEN + srcname_len), time, time_len);
  memcpy(FTA_LIB_OFFSET_ADDR(log_message_record, FTA_LOG_MESSAGE_RECORD_LEN + srcname_len+ time_len),msg,msglen);
  *FTA_LIB_OFFSET_ADDR(log_message_record, alloc_len - 1) = '\0';

  log_message_record->cmn.log_kind = FTA_LOG_KIND_MESSAGE;

end:
  return;
}
EXPORT_SYMBOL(fta_log_message_kmsg);

/* --------------------------------------------------------------------------
 *  log var API
 * -------------------------------------------------------------------------- */
/*!
 @brief fta_var_alloc_log

 memory allocate for android LOG

 @param [in] key         LOG type
 @param [in] size        size

 @retval     NULL: failed
 @retval   others: pointer of the log area( 0 is the area of all the remainder)
*/
uint8 *fta_var_alloc_log(uint32 key,uint32 size)
{
  uint8 *rec_addr = NULL;

  FTA_ENTER_LOCK();

  if(size == 0) { 
    /* calcurate size */
    fta_var_store_header *var_header;
    if(fta_reference.fta_store_item_log == NULL) {
      goto end;
    }
    if(fta_reference.fta_store_item_log->lock_keeper->manager_status != FTA_MANAGER_STATUS_ENABLED) {
      goto end;
    }
    var_header = (fta_var_store_header *)(fta_reference.fta_store_item_log->addr);
    size = var_header->cmn.size - var_header->cmn.offset - var_header->bottom;
  } else {
    size += FTA_VAR_RECORD_32_LEN;
  }
end:
  FTA_LEAVE_LOCK();

  rec_addr = (uint8 *)_fta_var_alloc(&fta_reference.fta_store_item_log,
                                     &fta_var_alloc_log_access_info,
                                     &key,
                                     size);

  if(rec_addr != NULL) {
  rec_addr += FTA_VAR_RECORD_32_LEN;
  *(uint32 *)rec_addr = size - FTA_VAR_RECORD_32_LEN;
  }

  return rec_addr;
}
EXPORT_SYMBOL(fta_var_alloc_log);

/* --------------------------------------------------------------------------
 *  list hash API
 * -------------------------------------------------------------------------- */

/*!
 @brief fta_list_taskidtable

 add to tasktable

 @param [in] thread_id    thread id
 @param [in] thread_name  thread name

 @retval     TRUE:      success
 @retval     FALSE:     error
*/
#define FTA_LIST_RECORD_COUNT_MAX \
        FTA_LIST_GET_MAX_RECORD( \
            FTA_LIST_RECORD_32_LEN + TASK_COMM_LEN + sizeof(int *), \
            FTA_TAG_TASKTABLE_MEMORY_SIZE - FTA_LIST_STORE_HEADER_LEN)
#define FTA_LIST_TASKTABLE_HASH_SIZE 512
static fta_listex_hash_id_type _fta_list_taskidtable_work[FTA_LISTEX_HASH_WORK_MAX(FTA_LIST_RECORD_COUNT_MAX, FTA_LIST_TASKTABLE_HASH_SIZE)];
const fta_listex_hash_access_info fta_list_taskidtable_access_info =
  FTA_LISTEX_HASH_ACCESS_INFO_DEF(
    FTA_LIST_RECORD_32_LEN + TASK_COMM_LEN + sizeof(int *),   /* fixed_len    */
    sizeof(uint32),                                          /* key_size     */
    FTA_LIST_RECORD_COUNT_MAX,                                /* entry_max    */
    FTA_LIST_TASKTABLE_HASH_SIZE,                             /* hash_size    */
    _fta_list_taskidtable_work,                               /* work_buffer  */
    TRUE,                                                     /* lock_access  */
  );
boolean fta_list_taskidtable(uint32 thread_id, const uint8 *thread_name)
{
  void *rec_addr = NULL;
  boolean rc = FALSE;

  /* get list area */
  rec_addr = (uint8 *)_fta_listex_hash_record(&fta_reference.fta_store_item_tasktable,
                                              &fta_list_taskidtable_access_info,
                                              &thread_id,
                                              FTA_LISTEX_HASH_ACCESS_ADD);
  if(rec_addr != NULL) {
    /* write data */
    struct task_struct *tsk = container_of((char *)thread_name, struct task_struct, comm[0]);

    memcpy(FTA_LIB_OFFSET_ADDR(rec_addr, FTA_LIST_RECORD_32_LEN), thread_name, TASK_COMM_LEN + 1);
    *(unsigned int *)FTA_LIB_OFFSET_ADDR(rec_addr,FTA_LIST_RECORD_32_LEN+TASK_COMM_LEN)
           = (unsigned int)virt_to_phys(tsk);
    printk(KERN_INFO "%s %p %d:%d\n",thread_name,tsk,tsk->pid,tsk->tgid);
    rc = TRUE;
  }

  return rc;
}

/*!
 @brief fta_list_taskidtable_delete

 delete from tasktable

 @param [in] thread_id  thread id

 @retval     TRUE:      success
 @retval     FALSE:     error
*/
boolean fta_list_taskidtable_delete(uint32 thread_id)
{
  void *rec_addr = NULL;
  rec_addr = (uint8 *)_fta_listex_hash_record(&fta_reference.fta_store_item_tasktable,
                                              &fta_list_taskidtable_access_info,
                                              &thread_id,
                                              FTA_LISTEX_HASH_ACCESS_DEL);
  return (rec_addr == NULL ? FALSE : TRUE);
}

/* --------------------------------------------------------------------------
 *  fta write all-purpose function
 * -------------------------------------------------------------------------- */

/*!
 @brief fta_convert_buf

 convert to area from label

 @param [in/out] buf  receive data

 @retval     -1:      not found the area
 @retval    >=0:      area no
*/
int fta_convert_buf(uint8 *buf)
{
   static uint8 seq = 0;
   int i;
   uint16 label = *(uint16 *)buf;
   for(i=0;i<sizeof(fta_convert_table)/sizeof(struct fta_convert_table_type);i++) {
      if(label == fta_convert_table[i].label) {
        buf[0] = fta_convert_table[i].area;
        buf[1] = seq++;
        return buf[0];
      }
   }
   return -1;
}
EXPORT_SYMBOL(fta_convert_buf);

/*!
 @brief fta_write

 fta write data

 @param [in] area   output area
 @param [in] buf    output data
 @param [in] buf    data length

 @retval    none
*/
void fta_write(int area, const uint8 *buf, uint32 len)
{
  const fta_manager_config_store_item *config;

  if(area < 0 || area >= FTA_CONFIG_STORE_INITIAL_COUNT)
    return;
  config = fta_initial_config_stores[area];

  if(config == NULL)
    return;

  switch(config->tag) {
  case FTA_TAG_KMSG:
  case FTA_TAG_DIAG:
  case FTA_TAG_PROC0:
  case FTA_TAG_PROC1:
  case FTA_TAG_PROC2:
  case FTA_TAG_PROC3:
      if(area & 0x40) {
        fta_log_struct_record *log_struct_record;
        uint32 info;
        info = *(uint32 *)buf;
        buf += 4;
        len -= 4;

        if(len > FTA_LOG_COMMON_RECORD_MAX_LOG_SIZE) {
           return;
        }

        log_struct_record = (fta_log_struct_record *) _fta_log_alloc(config->set_to,len+FTA_LOG_COMMON_RECORD_LEN, 
                                               FTA_LOG_MESSAGE_INFO_GET_LEVEL(info), NULL, NULL);
        if(log_struct_record == NULL) {
          return;
        }
        memcpy(FTA_LIB_OFFSET_ADDR(log_struct_record, FTA_LOG_COMMON_RECORD_LEN), buf, len);
        log_struct_record->cmn.log_kind = (info >> 16) & 0x00ff;
      } else {
        fta_log_message_record *log_message_record;
        uint32 info;
        info = *(uint32 *)buf;
        buf += 4;
        len -= 4;
        log_message_record = (fta_log_message_record *) _fta_log_alloc(config->set_to, len + FTA_LOG_MESSAGE_RECORD_LEN, 
                             FTA_LOG_MESSAGE_INFO_GET_LEVEL(info), NULL, NULL);
        if(log_message_record == NULL) {
          goto end;
        }

        log_message_record->output_line = FTA_LOG_MESSAGE_INFO_GET_LINE(info);
        memcpy(FTA_LIB_OFFSET_ADDR(log_message_record, FTA_LOG_MESSAGE_RECORD_LEN), buf, len);
        log_message_record->cmn.log_kind = FTA_LOG_KIND_MESSAGE;
      }
      break;
  case FTA_TAG_TERMINFO:
    {
       uint8 *rec_addr;
       uint32 key;
       key = *(uint32 *)buf;
       buf += 4;
       len -= 4;
       rec_addr = (uint8 *)_fta_var_alloc(config->set_to,
                                          &fta_var_alloc_terminfo_access_info,
                                          &key,
                                          len+FTA_VAR_RECORD_32_LEN);

       if(rec_addr != NULL) {
           rec_addr += FTA_VAR_RECORD_32_LEN;
           memcpy(rec_addr,buf,len);
       }
       break;
    }
  default:
    break;
  }
end: ;

  return;
}
EXPORT_SYMBOL(fta_write);

MODULE_DESCRIPTION("FTA Driver api for configuration");
MODULE_LICENSE("GPL v2");
