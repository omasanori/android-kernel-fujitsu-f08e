/*
 * Copyright(C) 2011 FUJITSU LIMITED
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
#ifndef __ASM_ARCH_MSM_BOARD_FJ_H
#define __ASM_ARCH_MSM_BOARD_FJ_H

#define FJ_MODE_INVALID         0
#define FJ_MODE_FASTBOOT        1
#define FJ_MODE_SD_DOWNLOADER   2
#define FJ_MODE_KERNEL_MODE     3
#define FJ_MODE_SP_MODE         4
#define FJ_MODE_MAKER_MODE      5
#define FJ_MODE_USB_DOWNLOADER  6
#define FJ_MODEM_OFF_CHARGE     7

#define FJ_MODEM_NORMAL         0xFF

/* FUJITSU:2012-02-01 charger add start */
#define FJ_CHARGER_REV_1	1
#define FJ_CHARGER_REV_2	2
/* FUJITSU:2012-02-01 charger add end */

extern int fj_boot_mode;

extern int charger_version_check(void); /* FUJITSU:2012-02-01 charger add */

/* FUJITSU:2012-03-30 CXO vote add start */
extern void fj_cxo_vote_get(void);
extern void fj_cxo_vote_put(void);
/* FUJITSU:2012-03-30 CXO vote add end */

#endif /* __ASM_ARCH_MSM_BOARD_FJ_H */
