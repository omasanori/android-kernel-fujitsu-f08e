/*
 * Copyright (C) 2011 Fujitsu Limited.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */
#define DEBUG
#define VERBOSE_DEBUG


#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb/android_composite.h>

#include "u_serial.h"
#include "gadget_chips.h"

struct mmp_descs {
	struct usb_endpoint_descriptor	*in;
	struct usb_endpoint_descriptor	*out;
};

struct f_mmp {
	struct gserial			port;
	u8				data_id;
	u8				port_num;

	struct mmp_descs		fs;
	struct mmp_descs		hs;
};

static inline struct f_mmp *func_to_mmp(struct usb_function *f)
{
	return container_of(f, struct f_mmp, port.func);
}

/*-------------------------------------------------------------------------*/

static struct usb_cdc_header_desc mmp_header_desc = {
	.bLength            =	sizeof(struct usb_cdc_header_desc),
	.bDescriptorType    =	0x24,
	.bDescriptorSubType =	0x00,
	.bcdCDC             =	__constant_cpu_to_le16(0x0110),
};

static struct usb_interface_descriptor mmp_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bNumEndpoints =	2,
	.bInterfaceClass =	0x02,
	.bInterfaceSubClass =	0x0A,
	.bInterfaceProtocol =	0x01,
};

static struct usb_cdc_mdlm_desc mmp_mdlm_desc = {
	.bLength = sizeof(struct usb_cdc_mdlm_desc),
	.bDescriptorType = 0x24,
	.bDescriptorSubType = 0x12,
	.bcdVersion = __constant_cpu_to_le16(0x0100),
/*	.bGUID[0] = 0xC2,
	.bGUID[1] = 0x29,
	.bGUID[2] = 0x9F,
	.bGUID[3] = 0xCC,
	.bGUID[4] = 0xD4,
	.bGUID[5] = 0x89,
	.bGUID[6] = 0x40,
	.bGUID[7] = 0x66,
	.bGUID[8] = 0x89,
	.bGUID[9] = 0x2B,
	.bGUID[10] = 0x10,
	.bGUID[11] = 0xC3,
	.bGUID[12] = 0x41,
	.bGUID[13] = 0xDD,
	.bGUID[14] = 0x98,
	.bGUID[15] = 0xA9,*/
};

static struct usb_endpoint_descriptor mmp_fs_in_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor mmp_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *mmp_fs_function[] = {
	(struct usb_descriptor_header *) &mmp_interface_desc,
	(struct usb_descriptor_header *) &mmp_header_desc,
	(struct usb_descriptor_header *) &mmp_mdlm_desc,
	(struct usb_descriptor_header *) &mmp_fs_in_desc,
	(struct usb_descriptor_header *) &mmp_fs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor mmp_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mmp_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_descriptor_header *mmp_hs_function[] = {
	(struct usb_descriptor_header *) &mmp_interface_desc,
	(struct usb_descriptor_header *) &mmp_header_desc,
	(struct usb_descriptor_header *) &mmp_mdlm_desc,
	(struct usb_descriptor_header *) &mmp_hs_in_desc,
	(struct usb_descriptor_header *) &mmp_hs_out_desc,
	NULL,
};

static struct usb_string mmp_string_defs[] = {
	[0].s = "mmp",
	{  }
};

static struct usb_gadget_strings mmp_string_table = {
	.language =		0x0409,
	.strings =		mmp_string_defs,
};

static struct usb_gadget_strings *mmp_strings[] = {
	&mmp_string_table,
	NULL,
};

static int mmp_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_mmp		 *mmp = func_to_mmp(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	/* gserial_disconnect() is deleted.
	 * in_desc and out_desc are set without fail.
	 */
	DBG(cdev, "activate mmp ttyGS%d\n", mmp->port_num);
	if (config_ep_by_speed(cdev->gadget, f, mmp->port.in))
		return -EINVAL;
	if (config_ep_by_speed(cdev->gadget, f, mmp->port.out))
		return -EINVAL;
	usb_ep_enable(mmp->port.in);
	usb_ep_enable(mmp->port.out);

	gserial_connect(&mmp->port, mmp->port_num);
	return 0;
}

static void mmp_disable(struct usb_function *f)
{
	struct f_mmp	         *mmp = func_to_mmp(f);

	pr_debug("mmp ttyGS%d deactivated\n", mmp->port_num);
	gserial_disconnect(&mmp->port);
}

/*-------------------------------------------------------------------------*/

static int
mmp_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_mmp            *mmp = func_to_mmp(f);
	int			 status;
	struct usb_ep		 *ep;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	mmp->data_id = status;
	mmp_interface_desc.bInterfaceNumber = status;

	status = -ENODEV;

	ep = usb_ep_autoconfig(cdev->gadget, &mmp_fs_in_desc);
	if (!ep)
		goto fail;
	mmp->port.in = ep;
	ep->driver_data = cdev;
        DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);

	ep = usb_ep_autoconfig(cdev->gadget, &mmp_fs_out_desc);
	if (!ep)
		goto fail;
	mmp->port.out = ep;
	ep->driver_data = cdev;
        DBG(cdev, "usb_ep_autoconfig for ep_out got %s\n", ep->name);

	f->descriptors = usb_copy_descriptors(mmp_fs_function);

	if (gadget_is_dualspeed(c->cdev->gadget)) {
		mmp_hs_in_desc.bEndpointAddress =
				mmp_fs_in_desc.bEndpointAddress;
		mmp_hs_out_desc.bEndpointAddress =
				mmp_fs_out_desc.bEndpointAddress;

		f->hs_descriptors = usb_copy_descriptors(mmp_hs_function);

	}

	DBG(cdev, "mmp ttyGS%d: %s speed IN/%s OUT/%s\n",
			mmp->port_num,
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			mmp->port.in->name, mmp->port.out->name);
	return 0;

fail:
	if (mmp->port.out)
		mmp->port.out->driver_data = NULL;
	if (mmp->port.in)
		mmp->port.in->driver_data = NULL;

	printk("%s: can't bind, err %d\n", f->name, status);

	return status;
}

static void
mmp_unbind(struct usb_configuration *c, struct usb_function *f)
{
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->descriptors);
	kfree(func_to_mmp(f));
}

int mmp_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct f_mmp *mmp;
	int		status;

	if (mmp_string_defs[0].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		mmp_string_defs[0].id = status;
	}

	mmp = kzalloc(sizeof *mmp, GFP_KERNEL);
	if (!mmp)
		return -ENOMEM;

	mmp->port_num = port_num;

	mmp->port.func.name = "mmp";
	mmp->port.func.strings = mmp_strings;
	mmp->port.func.bind = mmp_bind;
	mmp->port.func.unbind = mmp_unbind;
	mmp->port.func.set_alt = mmp_set_alt;
	mmp->port.func.disable = mmp_disable;

	status = usb_add_function(c, &mmp->port.func);
	if (status)
		kfree(mmp);
	return status;
}
