/*
 * Copyright(C) 2012-2013 FUJITSU LIMITED
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
#ifndef __NFC_IOCTL__
#define __NFC_IOCTL__

#ifdef CONFIG_FELICA
#include "../felica/felica_ioctl.h"
#endif

//#define NFC_DEBUG			1		/* When this define is 1, you can see debug logs.                     */
#ifdef FELICA_UID_CHECK
#define NFC_UID_CHECK 		1		/* This define is used for UID check.                                 */
#endif

#define WKUP_MODE_MC		1		/* This value is kind of wake up mode. 1:MC mode 0:Normal mode        */

#define NFCCTL_UID1			5001
#define NFCCTL_UID2			5003
#define NFCCTL_UID3			5006
#define NFCCTL_UID4			5008
#define NFCCTL_UID5			1000
#define NFCCTL_UID6			0
#define NFCCTL_UID7			1027

#define SNFC_DEV_INTUPL "snfc_intu_poll"
#define SNFC_DEV_HSEL   "snfc_hsel"
#define SNFC_DEV_CEN    "snfc_cen"
#define SNFC_DEV_ABTR   "snfc_arbiter"
#define SNFC_DEV_RFS    "snfc_rfs"
#define SNFC_DEV_OBSRV  "snfc_power_observe"
#define SNFC_DEV_PON    "snfc_pon"
#define SNFC_DEV_AVLPL  "snfc_available_poll"

extern uint8_t g_managelog_flg;

#define MANAGELOG(fmt, ...)	\
	if( g_managelog_flg ) {	\
		printk(pr_fmt(fmt), ##__VA_ARGS__);	\
	}

#endif /* __NFC_IOCTL__ */
