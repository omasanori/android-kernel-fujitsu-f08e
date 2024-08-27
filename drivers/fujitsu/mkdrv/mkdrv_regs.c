/*
 * mkdrv_regs.c
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

//==============================================================================
// include file
//==============================================================================
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/io.h>

#include "mkdrv_common.h"
#include "mkdrv_regs.h"

/**
 * mkdrv_reg_read
 * 
 * @param
 *
 * @retval success(0),error(non zero)
 */
int mkdrv_reg_read(mkdrv_func_data_t * data)
{
    int ret = 0;
    unsigned int addr_p;
    unsigned int addr_l;

    addr_p = data->iInputdata[0];

    addr_l = (unsigned int)ioremap(addr_p, 4);
    if (addr_l) {
        data->iOutputdata[0] = readl(addr_l);
    } else {
        ret = -1;
    }

    printk(KERN_INFO "%s : AddrP=0x%08X, AddrL=0x%08X, Data=0x%08X ret=%d\n",
            __func__, addr_p, addr_l, data->iOutputdata[0], ret);

    return ret;
}

/**
 * mkdrv_reg_write
 * 
 * @param
 *
 * @retval success(0),error(non zero)
 */
int mkdrv_reg_write(mkdrv_func_data_t * data)
{
    int ret = 0;
    unsigned int addr_p;
    unsigned int addr_l;
    unsigned int wdata;

    addr_p = data->iInputdata[0];
    wdata  = data->iInputdata[1];

    addr_l = (unsigned int)ioremap(addr_p, 4);
    if (addr_l) {
        writel(wdata, addr_l);
    } else {
        ret = -1;
    }

    printk(KERN_INFO "%s : AddrP=0x%08X, AddrL=0x%08X, Data=0x%08X ret=%d\n",
            __func__, addr_p, addr_l, wdata, ret);

    return ret;
}

MODULE_AUTHOR("FUJITSU");
MODULE_DESCRIPTION("mkdrv device");
MODULE_LICENSE("GPL");
