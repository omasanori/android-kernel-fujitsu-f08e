/*
 * mkdrv_common.h
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

#ifndef _MKDRV_COMMON_H
#define _MKDRV_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <linux/ioctl.h>

/*------------------------------------------------------------------*/
// IOCTL
/*------------------------------------------------------------------*/
typedef struct mkdrv_data {
  unsigned int	iFuncno;			//Function No
  unsigned int	iInputdata[5];		//Input data
  unsigned int	iOutputdata[5];		//Output data
} mkdrv_func_data_t;

#define IOC_MAGIC_MKDRV 'w'

#define IOCTL_MKDRV_FUNCTION_EXE	_IOW(IOC_MAGIC_MKDRV, 1, mkdrv_func_data_t)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* _MKDRV_COMMON_H */
