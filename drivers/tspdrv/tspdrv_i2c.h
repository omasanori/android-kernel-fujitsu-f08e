/*
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


#ifndef _TSPDRV_I2C_H_
#define _TSPDRV_I2C_H_

/* This struct defines a i2c command for I2C slave I/O operation. */
struct tspdrb_i2c_cmd_type {
    unsigned char  *pwdata;             // Pointer to Write data buffer
    unsigned short wlen;                // Count of bytes to transfer
    unsigned char  *prdata;             // Pointer to Read data buffer
    unsigned short rlen;                // Count of bytes to transfer
} __attribute__((packed, aligned(4)));

extern int tspdrv_i2c_initialize(void);
extern int tspdrv_i2c_terminate(void);
extern int tspdrv_i2c_read(struct tspdrb_i2c_cmd_type *);
extern int tspdrv_i2c_write(struct tspdrb_i2c_cmd_type *);
extern int tspdrv_i2c_read_one_transaction(struct tspdrb_i2c_cmd_type *);

#endif // _TSPDRV_I2C_H_

