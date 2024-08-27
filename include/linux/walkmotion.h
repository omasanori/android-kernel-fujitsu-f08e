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


#ifndef _FJ_WALKMOTION_H
#define _FJ_WALKMOTION_H


#include <linux/ioctl.h>

#define FJ_WM_IOC_MAGIC '~'

/* Initialize */
#define FJ_WM_IOCT_INITIALIZE 		_IO(FJ_WM_IOC_MAGIC, 0)
/* Request IRQ */
#define FJ_WM_IOCS_REQUESTMOTIONIRQ	_IOW(FJ_WM_IOC_MAGIC, 2, unsigned int)
/* Cancel request IRQ */
#define FJ_WM_IOCT_CANCELMOTIONIRQ	_IO(FJ_WM_IOC_MAGIC, 3)

/* WakeUp */
#define FJ_WM_IOCS_WAKEUPCONTROL	_IOW(FJ_WM_IOC_MAGIC, 5, unsigned int)

/* Detection of high edge */
#define FJ_WM_EDGE_HIGH			1
/* Detection of low edge */
#define FJ_WM_EDGE_LOW			0

/* Wakeup High */
#define FJ_WM_WAKEUP_HIGH		1
/* Wakeup Low */
#define FJ_WM_WAKEUP_LOW		0

/* Walk Motion MC Platform Data */
struct fj_wm_platform_data {
	/* Motion IRQ */
	int motion_irq;
	/* Delay */
	int mc_init_delay;	
};

#endif /** _FJ_WALKMOTION_H */
