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
#ifndef __FJ_CHARGER_LOCAL_H
#define __FJ_CHARGER_LOCAL_H


/* Device Type */
#define FJ_CHG_DEVICE_TYPE_FJDEV001  0x50
#define FJ_CHG_DEVICE_TYPE_FJDEV002  0x40
#define FJ_CHG_DEVICE_TYPE_FJDEV003  0x70
#define FJ_CHG_DEVICE_TYPE_FJDEV004  0x60
#define FJ_CHG_DEVICE_TYPE_FJDEV005  0x10
#define FJ_CHG_DEVICE_TYPE_FJDEV014  0x20

/* Model Type */
typedef enum {
    FJ_CHG_MODEL_TYPE_NONE = 0,
    FJ_CHG_MODEL_TYPE_FJDEV001,
    FJ_CHG_MODEL_TYPE_FJDEV002,
    FJ_CHG_MODEL_TYPE_FJDEV003,
    FJ_CHG_MODEL_TYPE_FJDEV004,
    FJ_CHG_MODEL_TYPE_FJDEV005,
    FJ_CHG_MODEL_TYPE_FJDEV014
} FJ_CHG_MODEL_TYPE;

/* chg device */
#define FJ_CHG_DEVICE_SMB      0     // SMB347
#define FJ_CHG_DEVICE_BQ       1     // BQ24192
#define FJ_CHG_DEVICE_NONE     0xFF  // None

enum {
	CHG_SOURCE_HOLDER = 0,		/* for Cradle     */
	CHG_SOURCE_USB,				/* for USB        */
	CHG_SOURCE_AC,				/* for AC adaptor */
	CHG_SOURCE_MHL,				/* for MHL(USB)   */
	CHG_SOURCE_NUM
};

#define FJ_CHG_SOURCE_AC       (BIT(0) << CHG_SOURCE_AC)
#define FJ_CHG_SOURCE_USB      (BIT(0) << CHG_SOURCE_USB)
#define FJ_CHG_SOURCE_MHL      (BIT(0) << CHG_SOURCE_MHL)
#define FJ_CHG_SOURCE_HOLDER   (BIT(0) << CHG_SOURCE_HOLDER)

#define FJ_CHG_SOURCE_USB_PORT (FJ_CHG_SOURCE_AC |FJ_CHG_SOURCE_USB |FJ_CHG_SOURCE_MHL )

#define FJ_CHG_SOURCE(x)       (BIT(0) << x)

#define FJ_CHG_SOURCE_IS_USB_PORT(x)  ((FJ_CHG_SOURCE_USB_PORT) & FJ_CHG_SOURCE(x))

#endif /* __FJ_CHARGER_LOCAL_H */
