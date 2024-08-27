/*
 * fta_config.c - FTA (Fault Trace Assistant) 
 *                config setting
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
 * Data erea config information
 * -------------------------------- */
const fta_manager_config_store_item fta_config_store_kmsg = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_KMSG,
    0,                         /* variable size */
    FTA_LOG_STORE_HEADER_LEN,
    _fta_log_init_common,
    _fta_log_stop_common,
    _fta_log_excp_common,
    &fta_reference.fta_store_item_kmsg,
  );
const fta_manager_config_store_item fta_config_store_diag = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_DIAG,
    FTA_TAG_DIAG_MEMORY_SIZE,
    FTA_LOG_STORE_HEADER_LEN,
    _fta_log_init_common,
    _fta_log_stop_common,
    _fta_log_excp_common,
    &fta_reference.fta_store_item_diag,
  );
const fta_manager_config_store_item fta_config_store_proc0 = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_PROC0,
    FTA_TAG_PROC0_MEMORY_SIZE,
    FTA_LOG_STORE_HEADER_LEN,
    _fta_log_init_common,
    _fta_log_stop_common,
    _fta_log_excp_common,
    &fta_reference.fta_store_item_proc0,
  );
const fta_manager_config_store_item fta_config_store_proc1 = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_PROC1,
    FTA_TAG_PROC1_MEMORY_SIZE,
    FTA_LOG_STORE_HEADER_LEN,
    _fta_log_init_common,
    _fta_log_stop_common,
    _fta_log_excp_common,
    &fta_reference.fta_store_item_proc1,
  );
const fta_manager_config_store_item fta_config_store_proc2 = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_PROC2,
    FTA_TAG_PROC2_MEMORY_SIZE,
    FTA_LOG_STORE_HEADER_LEN,
    _fta_log_init_common,
    _fta_log_stop_common,
    _fta_log_excp_common,
    &fta_reference.fta_store_item_proc2,
  );
const fta_manager_config_store_item fta_config_store_proc3 = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_PROC3,
    FTA_TAG_PROC3_MEMORY_SIZE,
    FTA_LOG_STORE_HEADER_LEN,
    _fta_log_init_common,
    _fta_log_stop_common,
    _fta_log_excp_common,
    &fta_reference.fta_store_item_proc3,
  );                                                     
extern const fta_listex_hash_access_info fta_list_taskidtable_access_info;
const fta_manager_config_store_item fta_config_store_tasktable = 
  FTA_CONFIG_STORE_ITEM_DEF2(
    FTA_TAG_TASKTABLE,
    FTA_TAG_TASKTABLE_MEMORY_SIZE,  
    FTA_LIST_STORE_HEADER_LEN,
    _fta_listex_hash_init_common,
    _fta_listex_hash_stop_common,
    _fta_listex_hash_excp_common,
    &fta_reference.fta_store_item_tasktable,
    &fta_list_taskidtable_access_info,
  );
#if 0
const fta_manager_config_store_item fta_config_store_dispatch = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_DISPATCH,
    FTA_TAG_DISPATCH_MEMORY_SIZE,
    FTA_VAR_STORE_HEADER_LEN,
    _fta_var_init_common,
    _fta_var_stop_common,
    _fta_var_excp_common,
    &fta_reference.fta_store_item_dispatch,
  );
#endif
const fta_manager_config_store_item fta_config_store_log = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_VAR_LOG,
    FTA_TAG_VAR_LOG_MEMORY_SIZE,
    FTA_VAR_STORE_HEADER_LEN,
    _fta_var_init_common2,
    _fta_var_stop_common,
    _fta_var_excp_common,
    &fta_reference.fta_store_item_log,
  );

const fta_manager_config_store_item fta_config_store_terminfo = 
  FTA_CONFIG_STORE_ITEM_DEF(
    FTA_TAG_TERMINFO,
    FTA_TAG_TERMINFO_MEMORY_SIZE,
    FTA_VAR_STORE_HEADER_LEN,
    _fta_var_init_common,
    _fta_var_stop_common,
    _fta_var_excp_common,
    &fta_reference.fta_store_item_terminfo,
  );

/* --------------------------------
 *  Data area arrangement information
 * -------------------------------- */
const fta_manager_config_store_item * const fta_initial_config_stores[FTA_CONFIG_STORE_INITIAL_COUNT] = {
  &fta_config_store_kmsg,
  &fta_config_store_diag,
  &fta_config_store_proc0,
  &fta_config_store_proc1,
  &fta_config_store_proc2,
  &fta_config_store_proc3,
  &fta_config_store_tasktable,
//  &fta_config_store_dispatch,                        
  &fta_config_store_log,
  &fta_config_store_terminfo,
};

/* --------------------------------
 *  management area
 * -------------------------------- */
fta_lock_keeper_info fta_lock_keeper = {0};   /* access manage   */
fta_manager_store_header *fta_manager = {0};  /* data manage     */
fta_reference_pointers fta_reference = {0};   /* reference area  */
const fta_reference_pointers fta_reference_null = {0};

DEFINE_SPINLOCK(fta_lock);

/* --------------------------------
 *  initial information area
 * -------------------------------- */
fta_manager_initializer_info fta_manager_initializer = 
  FTA_MANAGER_INITIALIZER_INFO_DEF(
    0,
    fta_initial_config_stores,
    FTA_CONFIG_STORE_INITIAL_COUNT,
    0,
    FTA_CONFIG_STORE_MEMORY_SIZE - FTA_CALC_MANAGER_SIZE(FTA_CONFIG_STORE_INITIAL_COUNT),
    &fta_lock_keeper,
  );

/* ==========================================================================
 *  FUNCTIONS
 * ========================================================================== */

/*!
 @brief fta_initialize

 FTA initialize function

 @param      none

 @retval     none
*/
void fta_initialize(void)
{
  boolean dofirst = (fta_manager == NULL ? TRUE : FALSE);

  if(!dofirst) {
    memset(&fta_lock_keeper, 0, sizeof(fta_lock_keeper));
    memset(&fta_reference, 0, sizeof(fta_reference));
  }

  fta_manager = (fta_manager_store_header *)fta_log_area;
  fta_manager_initializer.fta_manager = fta_manager;
  fta_manager_initializer.memory_addr = FTA_LIB_OFFSET_ADDR(((uint8 *)fta_manager), FTA_CALC_MANAGER_SIZE(FTA_CONFIG_STORE_INITIAL_COUNT));
  if(_fta_manage_initialize(&fta_manager_initializer)) {

    _fta_manage_initialize_store_items(fta_manager);

    fta_manager->cmn.enabled = FTA_STORE_ENABLED_TRUE;
    FTA_LIB_CHANGEBIT_SET(fta_manager->cmn.statbit, FTA_STORE_STATBIT_RECORDING);
    fta_manager->parity      = fta_lib_check_parity16((uint16 *)fta_manager, fta_manager->cmn.size / 2, fta_manager->parity);
    FTA_MEMORY_FLUSH();

    fta_lock_keeper.manager_status = FTA_MANAGER_STATUS_ENABLED;

    {
      extern void fta_syslog(void);
      fta_syslog();
    }
    /* start logging */
    FTA_LOG_MESSAGE(FTA_ADMIN, FTA_LOG_LEVEL_NOTICE, "*** FTA INITIALIZED %x %x", fta_log_area, virt_to_phys(fta_log_area),0);
  }

  return;
}
EXPORT_SYMBOL(fta_initialize);

/*!
 @brief fta_terminate

 FTA terminate function

 @param      none

 @retval     none
*/
void fta_terminate(void)
{

  if(fta_manager == NULL) return;

  FTA_ENTER_LOCK();

  fta_lock_keeper.manager_status = FTA_MANAGER_STATUS_STOP;
  FTA_LIB_CHANGEBIT_CLR(fta_manager->cmn.statbit, FTA_STORE_STATBIT_RECORDING);
  fta_manager->cmn.enabled = FTA_STORE_ENABLED_FALSE;
  fta_manager->parity      = fta_lib_check_parity16((uint16 *)fta_manager, fta_manager->cmn.size / 2, fta_manager->parity);
  FTA_MEMORY_FLUSH();

  FTA_LEAVE_LOCK();

  _fta_manage_terminate_store_items(fta_manager);
  fta_reference = fta_reference_null;

  return;
}
EXPORT_SYMBOL(fta_terminate);

/*!
 @brief fta_stop

 FTA stop function

 @param      none

 @retval     none
*/
void fta_stop(fta_stop_reason_enum reason)
{

  if(fta_manager == NULL) return;

  FTA_ENTER_LOCK();

  if(fta_lock_keeper.manager_status == FTA_MANAGER_STATUS_ENABLED) {
    fta_lock_keeper.manager_status = FTA_MANAGER_STATUS_STOP;
    /* - - - - - - - - */
    switch(reason) {
    case FTA_STOP_REASON_NORMAL:
      FTA_LIB_CHANGEBIT_CLR(fta_manager->cmn.statbit, FTA_STORE_STATBIT_RECORDING);
      fta_manager->cmn.enabled = FTA_STORE_ENABLED_FALSE;
      fta_manager->parity      = fta_lib_check_parity16((uint16 *)fta_manager, fta_manager->cmn.size / 2, fta_manager->parity);
      break;
    case FTA_STOP_REASON_ABNORMAL:
      break;
    case FTA_STOP_REASON_INTENTIONAL:
      FTA_LIB_CHANGEBIT_SET(fta_manager->cmn.statbit, FTA_STORE_STATBIT_INTENTIONAL);
      fta_manager->parity      = fta_lib_check_parity16((uint16 *)fta_manager, fta_manager->cmn.size / 2, fta_manager->parity);
    }
    /* - - - - - - - - */
    FTA_MEMORY_FLUSH();
  }

  FTA_LEAVE_LOCK();

  _fta_manage_terminate_store_items(fta_manager);
  fta_reference = fta_reference_null;

  return;
}
EXPORT_SYMBOL(fta_stop);

/*!
 @brief fta_reallocate

 reallocate fta area

 @param [in] p_config_items    new configuration
 @param [in] config_count      counts of the new configuration array

 @retval     TRUE: success
 @retval    FALSE: failed

*/
boolean fta_reallocate(const fta_manager_config_store_item * const *p_config_items, uint32 config_count)
{
  if(fta_manager == NULL)
    return FALSE;

  if( (p_config_items == NULL || config_count > FTA_CONFIG_STORE_INITIAL_COUNT) &&
     !(p_config_items == NULL && config_count == 0) )
    return FALSE;

  if(p_config_items != NULL) {
    fta_manager_initializer.p_config_items = p_config_items;
  fta_manager_initializer.config_count   = config_count;
  }

  fta_initialize();

  return TRUE;
}
EXPORT_SYMBOL(fta_reallocate);

MODULE_DESCRIPTION("FTA Driver configuration");
MODULE_LICENSE("GPL v2");
