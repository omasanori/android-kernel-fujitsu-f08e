/*
 * Copyright (C) 2011-2012 Fujitsu Limited.
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#ifndef __OM_H
#define __OM_H

#include <linux/types.h>

struct usb_om_interface_desc {
	__u8	bLength;
	__u8	bDescriptorType;
	__u8	bDescriptorSubType;
	__u8	bType;
#define OM_BTYPE_AB_1		0x01
#define OM_BTYPE_AB_2		0x02
#define OM_BTYPE_AB_5		0x05
	__u8	bMode_1;
	__u8	bMode_2;
#define OM_BMODE_MODEM		0x01
#define OM_BMODE_AT_COMMAND_CONTROL	0x02
#define OM_BMODE_OBEX		0x60
#define OM_BMODE_VENDOR1		0xC0
} __attribute__ ((packed));

#define USB_CDC_SUBCLASS_ACM_OM	0x88
#define USB_CDC_CAP_BOTH		0x03
#define USB_CDC_CAP_VS_REQ		0x06

#define OM_SPECIFIC_INTERFACE		0x44
/* FIXME USB_DT_VS_INTERFACE	(USB_TYPE_VENDOR | USB_DT_INTERFACE) */

#define USB_MACM_TYPE			0x11

#define USB_REQ_ACTIVATE_MODE		0x60
#define USB_REQ_GET_MODETABLE		0x61
#define USB_REQ_SET_LINK		0x62
#define USB_REQ_CLEAR_LINK		0x63

#define USB_REQ_TYPE_REQUEST		0x41
#define USB_REQ_TYPE_NOIFICATION	0xC1
#define USB_REQUEST_ACKNOWLEDGE		0x31
#define USB_WVALUE_ACK			0x01
#define USB_WVALUE_NACK			0x00

#endif
