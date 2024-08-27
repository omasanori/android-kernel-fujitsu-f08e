/*
 * mkdrv_regs.h
 *
 * Copyright(C) 2012 - 2013 FUJITSU LIMITED
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

#ifndef __MKDRV_REGS_H
#define __MKDRV_REGS_H

//==============================================================================
// functions prototype
//==============================================================================
int mkdrv_reg_read(mkdrv_func_data_t * data);
int mkdrv_reg_write(mkdrv_func_data_t * data);

#endif /* __MKDRV_REGS_H */
