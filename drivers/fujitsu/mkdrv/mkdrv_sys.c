/*
 * mkdrv_sys.c
 *
 * Copyright(C) 2013 FUJITSU LIMITED
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

//==============================================================================
// include file
//==============================================================================
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/io.h>
#include <mach/msm_smsm.h> 
#include <linux/string.h>

#include "mkdrv_common.h"
#include "mkdrv_sys.h"

/**
 * mkdrv_raminfo_get
 * 
 * @param
 *
 * @retval success(0),error(non zero)
 */
int mkdrv_raminfo_get(mkdrv_func_data_t * data)
{
    int ret = 0;
    uint8_t *smem_p;
    
    data->iOutputdata[0] = 0xAA;
	smem_p = (uint8_t*)smem_alloc_vendor0(SMEM_OEM_V0_003);
	if (smem_p == NULL) {
		ret = -EFAULT;
		printk(KERN_ERR "smem_alloc_vendor0 failed.\n");
	}else{
		memcpy(&data->iOutputdata[0], smem_p, 1);
	}
    return ret;
}


MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("mkdrv device");
MODULE_LICENSE("GPL");
