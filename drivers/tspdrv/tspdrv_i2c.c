/*
  * Copyright(C) 2012 FUJITSU LIMITED
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

/*============================================================================
                        INCLUDE FILES
============================================================================*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <../board-8064.h>
#include <asm/mach-types.h>
#include "tspdrv_i2c.h"
#include "tspdrv.h"
#include "../vibdrv/vibdrv_i2c.h"

/*============================================================================
                        INTERNAL FEATURES
============================================================================*/

/*============================================================================
                        CONSTANTS
============================================================================*/

/*===========================================================================
    FUNCTION      tspdrv_i2c_initialize
===========================================================================*/
int tspdrv_i2c_initialize(void)
{
    return vibdrv_i2c_initialize();
}

/*===========================================================================
    FUNCTION      tspdrv_i2c_terminate
===========================================================================*/
int tspdrv_i2c_terminate(void)
{
    return vibdrv_i2c_terminate();
}

/*===========================================================================
    FUNCTION      tspdrv_i2c_read
===========================================================================*/
int tspdrv_i2c_read(struct tspdrb_i2c_cmd_type *pI2CCmd)
{
    int            rc = 0;
    struct vibdrv_i2c_cmd_type i2c_cmd;

    i2c_cmd.pwdata = pI2CCmd->pwdata;
    i2c_cmd.wlen   = pI2CCmd->wlen;
    i2c_cmd.prdata = pI2CCmd->prdata;
    i2c_cmd.rlen   = pI2CCmd->rlen;

    vibdrv_i2c_lock();
    rc = vibdrv_i2c_read(&i2c_cmd);
    vibdrv_i2c_unlock();

    return rc;
}

/*===========================================================================
    FUNCTION      tspdrv_i2c_write
===========================================================================*/
int tspdrv_i2c_write(struct tspdrb_i2c_cmd_type * pI2CCmd)
{
    int            rc = 0;
    struct vibdrv_i2c_cmd_type i2c_cmd;

    i2c_cmd.pwdata = pI2CCmd->pwdata;
    i2c_cmd.wlen   = pI2CCmd->wlen;
    i2c_cmd.prdata = pI2CCmd->prdata;
    i2c_cmd.rlen   = pI2CCmd->rlen;

    vibdrv_i2c_lock();
    rc = vibdrv_i2c_write(&i2c_cmd);
    vibdrv_i2c_unlock();

    return rc;
}

/*===========================================================================
    FUNCTION      tspdrv_i2c_read_one_transaction
===========================================================================*/
int tspdrv_i2c_read_one_transaction(struct tspdrb_i2c_cmd_type * pI2CCmd)
{
    int            rc = 0;
    struct vibdrv_i2c_cmd_type i2c_cmd;

    i2c_cmd.pwdata = pI2CCmd->pwdata;
    i2c_cmd.wlen   = pI2CCmd->wlen;
    i2c_cmd.prdata = pI2CCmd->prdata;
    i2c_cmd.rlen   = pI2CCmd->rlen;

    vibdrv_i2c_lock();
    rc = vibdrv_i2c_read_one_transaction(&i2c_cmd);
    vibdrv_i2c_unlock();

    return rc;
}
