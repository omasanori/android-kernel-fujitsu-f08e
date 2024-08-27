/*
 * Copyright(C) 2011-2013 FUJITSU LIMITED
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

#ifndef __MSM_SERIAL_AUX__
#define __MSM_SERIAL_AUX__

/* Device name definition */
#define MSM_SERIAL_AUX_DEV_NAME "MSM_UART_AUX"

/*
 * define IOCTL macro
 */
#define MSM_AUX_IOC_TYPE  0x56          /* 8bit */
#define MSM_AUX_IOCTL_CHG_IRDA			_IO( MSM_AUX_IOC_TYPE, 0 )
#define MSM_AUX_IOCTL_CHG_UART			_IO( MSM_AUX_IOC_TYPE, 1 )

#endif /* __MSM_SERIAL_AUX__ */
