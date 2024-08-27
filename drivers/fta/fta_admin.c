/*
 * fta_admin.c - FTA (Fault Trace Assistant) 
 *               for administration
 *
 * Copyright(C) 2011 FUJITSU LIMITED
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.s
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
 @brief _fta_manage_check_exportable

 check data for exportable

 @param [out] fta_manager  check data
 @param [in]  limit_size   data size

 @retval     TRUE : data is exportable
 @retval     FALSE: data is not exportable

*/
boolean _fta_manage_check_exportable(fta_manager_store_header *fta_manager, uint32 limit_size)
{
  if(fta_manager == NULL) return FALSE;

  if(!(fta_manager->cmn.tag    == FTA_TAG_MANAGER &&
       fta_manager->cmn.size   >= FTA_CALC_MANAGER_SIZE(fta_manager->count) &&
       fta_manager->cmn.size   <= limit_size &&
       fta_manager->cmn.offset == FTA_MANAGER_STORE_HEADER_LEN &&
       fta_manager->reclen     == FTA_MANAGER_STORE_ITEM_LEN &&
       fta_manager->count      >  0)) {
    return FALSE;
  }

  if(fta_lib_check_parity16((uint16 *)fta_manager, fta_manager->cmn.size / 2, 0) != 0) {
    return FALSE;
  }

  if(!(fta_manager->cmn.enabled == FTA_STORE_ENABLED_TRUE &&
       FTA_LIB_CHECKBIT(fta_manager->cmn.statbit, FTA_STORE_STATBIT_RECORDING))) {
    return FALSE;
  }

  FTA_LIB_CHANGEBIT_CLR(fta_manager->cmn.statbit, FTA_STORE_STATBIT_RECORDING);
  fta_manager->parity = fta_lib_check_parity16(
                                (uint16 *)fta_manager,
                                fta_manager->cmn.size / 2,
                                fta_manager->parity);

  return TRUE;
}
EXPORT_SYMBOL(_fta_manage_check_exportable);

/*!
 @brief _fta_manage_initialize

 initialize manage area

 @param [in]  manager_initializer   pointer of the manage area

 @retval      TRUE : success
 @retval      FALSE: failed

*/
boolean _fta_manage_initialize(const fta_manager_initializer_info *manager_initializer)
{
  fta_manager_store_header *fta_manager;
  const fta_manager_config_store_item * const *p_config_items;
  fta_manager_store_item *store_items;

  uint32 manager_size;
  fta_common_store_header *store_addr;
  uint32 var_size;
  uint32 var_num;
  uint32 i;
  uint16 config_cnt;
  boolean config_err = FALSE;

  if(manager_initializer == NULL) return FALSE;

  if(manager_initializer->fta_manager    == NULL ||
     manager_initializer->p_config_items == NULL ||
     manager_initializer->lock_keeper    == NULL ||
     (manager_initializer->memory_size & 3) != 0) {
    return FALSE;
  }

  fta_manager = manager_initializer->fta_manager;
  p_config_items = manager_initializer->p_config_items;
  store_items = (fta_manager_store_item *) FTA_LIB_OFFSET_ADDR(fta_manager, FTA_MANAGER_STORE_HEADER_LEN);

  manager_size = FTA_CALC_MANAGER_SIZE(manager_initializer->config_count);
  memset(fta_manager, 0, manager_size);
  fta_manager->cmn.tag     = FTA_TAG_MANAGER;
  fta_manager->cmn.size    = manager_size;
  fta_manager->cmn.enabled = FTA_STORE_ENABLED_FALSE;
  FTA_LIB_SETBITVALUE(fta_manager->cmn.statbit, FTA_STORE_STATBIT_ALLCLEAR);
  fta_manager->cmn.offset  = FTA_MANAGER_STORE_HEADER_LEN;
  fta_manager->id          = FTA_MANAGER_ID_DEFAULT;
  fta_manager->reclen      = FTA_MANAGER_STORE_ITEM_LEN;
  fta_manager->count       = 0;
  fta_manager->parity      = fta_lib_check_parity16((uint16 *)fta_manager, fta_manager->cmn.size / 2, fta_manager->parity);

  manager_initializer->lock_keeper->fta_manager    = fta_manager;
  manager_initializer->lock_keeper->manager_status = FTA_MANAGER_STATUS_UNINIT;

  /* set the manage info from the config table */
  store_addr = (fta_common_store_header *)(manager_initializer->memory_addr);
  var_size = manager_initializer->memory_size;
  /* calcurate the variable size */
  config_cnt = 0;
  var_num = 0;
  /* - - - - - - - - */
  for(i = 0; i < manager_initializer->config_count; i++) {
    if(p_config_items[i] != NULL) {
      if(p_config_items[i]->tag != FTA_CONFIG_UNDEFINED) {
        if(config_cnt == 0) {
          var_num = i;
        } else {
          if(var_size < p_config_items[i]->size)                  config_err = TRUE;
          if((p_config_items[i]->size & 3) != 0)                  config_err = TRUE;
          if(p_config_items[i]->offset > p_config_items[i]->size) config_err = TRUE;
          var_size -= p_config_items[i]->size;
        }
        config_cnt++;
      }
    }
  }
  if(config_cnt > 0) {
    if(p_config_items[var_num]->offset > var_size) config_err = TRUE;
  }
  /* - - - - - - - - */
  config_cnt = 0;
  /* - - - - - - - - */
  for(i = 0; i < manager_initializer->config_count; i++) {
    if(p_config_items[i] != NULL) {
      if(p_config_items[i]->tag != FTA_CONFIG_UNDEFINED) {
        store_items[config_cnt].addr        = store_addr;
        store_items[config_cnt].tag         = p_config_items[i]->tag;
        store_items[config_cnt].size        = (i == var_num ? var_size : p_config_items[i]->size);
        store_items[config_cnt].def         = p_config_items[i];
        store_items[config_cnt].lock_keeper = manager_initializer->lock_keeper;
        if(p_config_items[i]->set_to != FTA_CONFIG_UNDEFINED) {
          *(p_config_items[i]->set_to) = &store_items[config_cnt];
        }
        store_addr = (fta_common_store_header *) FTA_LIB_OFFSET_ADDR(store_addr, store_items[config_cnt].size);
        config_cnt++;
      }
    }
  }
  /* - - - - - - - - */

  fta_manager->count       = config_cnt;
  fta_manager->parity      = fta_lib_check_parity16((uint16 *)fta_manager, fta_manager->cmn.size / 2, fta_manager->parity);

  /* cannot use FTA when configuration is unrecongnized */
  if(config_err) {
    return FALSE;
  }

  return TRUE;
}
EXPORT_SYMBOL(_fta_manage_initialize);

/*!
 @brief _fta_manage_initialize_store_items

 initialize for the inidivual area

 @param [in]  fta_manager   manage area

 @retval      none
*/
void _fta_manage_initialize_store_items(fta_manager_store_header *fta_manager)
{
  fta_manager_store_item *store_items;
  fta_common_store_header *store_addr;
  uint32 i;

  if(fta_manager == NULL) return;

  store_items = (fta_manager_store_item *) FTA_LIB_OFFSET_ADDR(fta_manager, FTA_MANAGER_STORE_HEADER_LEN);
  /* - - - - - - - - */
  for(i = 0; i < fta_manager->count; i++) {
    _fta_common_init_common(&store_items[i]);
    store_addr = store_items[i].addr;
    if(store_items[i].def->init_func != FTA_CONFIG_UNDEFINED) {
      store_addr->enabled = FTA_STORE_ENABLED_FALSE;
      FTA_LIB_SETBITVALUE(store_addr->statbit, FTA_STORE_STATBIT_ALLCLEAR);
      (store_items[i].def->init_func)(&store_items[i]);
    } else {
      /* do nothing and do not manage this area when initialize function is undefined */
    }
  }
  /* - - - - - - - - */
}
EXPORT_SYMBOL(_fta_manage_initialize_store_items);

/*!
 @brief _fta_common_init_common

 common setting for initialze

 @param [in]  store_item   item for manage

 @retval      none
*/
void _fta_common_init_common(fta_manager_store_item *store_item)
{
  fta_common_store_header *store_addr;

  if(store_item == NULL) return;

  store_addr = store_item->addr;
  store_addr->tag     = store_item->def->tag;
  store_addr->size    = store_item->size;
  store_addr->offset  = store_item->def->offset;
}
EXPORT_SYMBOL(_fta_common_init_common);

/*!
 @brief _fta_manage_terminate_store_items

 terminate for the inidivual area

 @param [in]  fta_manager   manage area

 @retval      none
*/
void _fta_manage_terminate_store_items(fta_manager_store_header *fta_manager)
{
  fta_manager_store_item *store_items;

  uint32 i;

  if(fta_manager == NULL) return;

  store_items = (fta_manager_store_item *) FTA_LIB_OFFSET_ADDR(fta_manager, FTA_MANAGER_STORE_HEADER_LEN);
  /* - - - - - - - - */
  for(i = 0; i < fta_manager->count; i++) {
    if(store_items[i].def->stop_func != FTA_CONFIG_UNDEFINED) {
      (store_items[i].def->stop_func)(&store_items[i]);
    }
  }
  /* - - - - - - - - */

  return;
}
EXPORT_SYMBOL(_fta_manage_terminate_store_items);

/*!
 @brief _fta_manage_get_store_item

 get the pointer of item for manage

 @param [in]  fta_manager   pointer of the manage aera
 @param [in]  tag           tag

 @retval      NULL: not found
 @retval    others: the pointer of item for manage
*/
fta_manager_store_item *_fta_manage_get_store_item(fta_manager_store_header *fta_manager, uint32 tag)
{
  fta_manager_store_item *store_items;

  uint32 i;

  if(fta_manager == NULL) return NULL;

  store_items = (fta_manager_store_item *) FTA_LIB_OFFSET_ADDR(fta_manager, FTA_MANAGER_STORE_HEADER_LEN);
  /* - - - - - - - - */
  for(i = 0; i < fta_manager->count; i++) {
    if(store_items[i].tag == tag) {
      return &store_items[i];
    }
  }
  /* - - - - - - - - */

  return NULL;
}
EXPORT_SYMBOL(_fta_manage_get_store_item);

MODULE_DESCRIPTION("FTA Driver administration");
MODULE_LICENSE("GPL v2");
