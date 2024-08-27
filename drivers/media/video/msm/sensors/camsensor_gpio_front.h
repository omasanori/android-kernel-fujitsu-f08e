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

#ifndef CAMSENSOR_GPIO_FRONT_H
#define CAMSENSOR_GPIO_FRONT_H

/**
 * This struct defines a i2c command for I2C slave I/O operation.
 */
#define RET_OK      0
#define RET_ERR     -1

#define boolean int

struct camsensor_i2c_command {
    unsigned char   slave_addr;         // Slave address
    unsigned char   *pwdata;            // Pointer to Write data buffer
    unsigned short  wlen;               // Count of bytes to transfer
    unsigned char   *prdata;            // Pointer to Write data buffer
    unsigned short  rlen;               // Count of bytes to transfer
} __attribute__((packed, aligned(4)));

struct camsensor_i2c_command_isp {
    unsigned char   slave_addr;         // Slave address
	unsigned short  category;           // Parameter Category No.
	unsigned short  byte;               // Parameter Byte.
    unsigned char   *pwdata;            // Pointer to Write data buffer
    unsigned short  wlen;               // Count of bytes to transfer
    unsigned char   *prdata;            // Pointer to Write data buffer
    unsigned short  rlen;               // Count of bytes to transfer
} __attribute__((packed, aligned(4)));

extern int camsensor_gpioi2c_front_read(struct camsensor_i2c_command *);
extern int camsensor_gpioi2c_front_write(struct camsensor_i2c_command *);

extern int camsensor_gpioi2c_front_read_isp(struct camsensor_i2c_command_isp *);
extern int camsensor_gpioi2c_front_write_isp(struct camsensor_i2c_command_isp *);

struct i2c_client;

extern int camsensor_i2c_front_read(struct i2c_client*, struct camsensor_i2c_command *);
extern int camsensor_i2c_front_write(struct i2c_client*, struct camsensor_i2c_command *);

extern int camsensor_i2c_front_read_isp(struct i2c_client*, struct camsensor_i2c_command_isp *);
extern int camsensor_i2c_front_write_isp(struct i2c_client*, struct camsensor_i2c_command_isp *);

#endif /* CAMSENSOR_GPIO_FRONT_H */
