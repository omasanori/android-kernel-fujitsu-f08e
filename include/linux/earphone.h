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


/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/
/* QNET Audio:2012-01-17 Basic Feat EarphoneDriver start */
#ifndef	__EARPHONE_H__
#define	__EARPHONE_H__

#include <linux/ioctl.h>

#define	HEADSET_FLAG				1
#define	USB_EARPHONE_FLAG			2
#define	MIC_SWITCH_FLAG				3
#define PIERCE_ID_1000_OHM_FLAG		4	/* earphone pierce1 (1.0k-ohm) */
#define PIERCE_ID_1500_OHM_FLAG		5	/* earphone pierce2 (1.5k-ohm) */
#define PIERCE_ID_2200_OHM_FLAG		6	/* earphone pierce3 (2.2k-ohm) */

#define	HEDSET_CHECK_BIT				(1<<HEADSET_FLAG)
#define	USB_EARPHONE_CHECK_BIT			(1<<USB_EARPHONE_FLAG)
#define	MIC_SWITCH_CHECK_BIT			(1<<MIC_SWITCH_FLAG)
#define PIERCE_ID_1000_OHM_CHECK_BIT	(1<<PIERCE_ID_1000_OHM_FLAG)
#define PIERCE_ID_1500_OHM_CHECK_BIT	(1<<PIERCE_ID_1500_OHM_FLAG)
#define PIERCE_ID_2200_OHM_CHECK_BIT	(1<<PIERCE_ID_2200_OHM_FLAG)

#define	EARPHONE_PHI35_USE			_IOW('w', 0x01, int)
#define	EARPHONE_PHI35_UNUSE		_IOW('w', 0x02, int)
#define	USB_EARPHONE_USE			_IOW('w', 0x03, int)
#define	USB_EARPHONE_UNUSE			_IOW('w', 0x04, int)
#define	EARPHONE_MIC_PUSH			_IOW('w', 0x05, int)
#define	EARPHONE_MIC_RELESE			_IOW('w', 0x06, int)
#define	EARPHONE_STATUS				_IOW('w', 0x07, int)
#define	EARPHONE_SWITCH_USE			_IOW('w', 0x08, int)
#define	EARPHONE_SWITCH_UNUSE		_IOW('w', 0x09, int)
#define	EARPHONE_VOICE_CALL_START	_IOW('w', 0x0A, int)
#define	EARPHONE_VOICE_CALL_STOP	_IOW('w', 0x0B, int)
#define	EARPHONE_STATUS_READ    	_IOW('w', 0x0C, int)
#define	EARPHONE_WAKE_UP			_IOW('w', 0x0D, int)
#define	EARPHONE_DET_OUTPUT			_IOW('w', 0x0E, int)
#define	EARPHONE_STATUS_GET			_IOW('w', 0x0F, int)

#define	EARPHONE_KO_XLED_RST_ON		_IOW('w', 0x10, int)
#define	EARPHONE_KO_XLED_RST_OFF	_IOW('w', 0x11, int)
#define	EARPHONE_ILLUMI_ON			_IOW('w', 0x12, int)
#define	EARPHONE_ILLUMI_OFF			_IOW('w', 0x13, int)
#define	EARPHONE_HEADSET_MIC_USE	_IOW('w', 0x14, int)
#define	EARPHONE_HEADSET_MIC_UNUSE	_IOW('w', 0x15, int)

/* TEST */
/* #define	EARPHONE_PIERCE_USE			_IOW('w', 0x10, int) */
/* #define	EARPHONE_PIERCE_UNUSE		_IOW('w', 0x11, int) */

struct earphone_status {
	char plug_status;
	char sw_status;
	char plug_vol[2];
	char pierce_status;
	char pierce_vol[2];
};

extern void edc_usb_insert_detect(void);
extern void edc_usb_remove_detect(void);

#endif	//	__EARPHONE_H__
/* QNET Audio:2012-01-17 Basic Feat EarphoneDriver end */
