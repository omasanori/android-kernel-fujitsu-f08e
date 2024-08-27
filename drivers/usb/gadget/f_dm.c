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

/* #define VERBOSE_DEBUG */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb/android_composite.h>

#include "u_serial.h"
#include "om.h"
#include "gadget_chips.h"

struct dm_ep_descs {
	struct usb_endpoint_descriptor	*in;
	struct usb_endpoint_descriptor	*out;
	struct usb_endpoint_descriptor	*notify;
};

struct f_dm {
	struct	gserial					port;
	u8						ctrl_id, data_id;
	u8						port_num;
	enum transport_type				transport;

	u8						pending;

	/* lock is mostly for pending and notify_req ... they get accessed
	 * by callbacks both from tty (open/close/break) under its spinlock,
	 * and notify_req.complete() which can't use that lock.
	 */
	spinlock_t					lock;

	struct dm_ep_descs				fs;
	struct dm_ep_descs				hs;

	struct usb_ep					*notify;
	struct usb_endpoint_descriptor			*notify_desc;
	struct usb_request				*notify_req;

	struct usb_cdc_line_coding			port_line_coding;	/* 8-N-1 etc */

	/* SetControlLineState request -- CDC 1.1 section 6.2.14 (INPUT) */
	u16						port_handshake_bits;
#define ACM_CTRL_RTS	(1 << 1)	/* unused with full duplex */
#define ACM_CTRL_DTR	(1 << 0)	/* host is ready for data r/w */

	/* SerialState notification -- CDC 1.1 section 6.3.5 (OUTPUT) */
	u16				serial_state;
#define ACM_CTRL_OVERRUN	(1 << 6)
#define ACM_CTRL_PARITY		(1 << 5)
#define ACM_CTRL_FRAMING	(1 << 4)
#define ACM_CTRL_RI		(1 << 3)
#define ACM_CTRL_BRK		(1 << 2)
#define ACM_CTRL_DSR		(1 << 1)
#define ACM_CTRL_DCD		(1 << 0)
};

#if 0
static unsigned int no_tty_ports;
static unsigned int no_sdio_ports;
static unsigned int no_smd_ports;
static unsigned int nr_ports;
#endif

static struct dm_port_info {
        enum transport_type     transport;
        unsigned                port_num;
        unsigned                client_port_num;
} gdm_ports[GSERIAL_NO_PORTS];

static inline struct f_dm *func_to_dm(struct usb_function *f)
{
	return container_of(f, struct f_dm, port.func);
}

static inline struct f_dm *port_to_dm(struct gserial *p)
{
	return container_of(p, struct f_dm, port);
}

#if 0
static char *transport_to_str(enum transport_type t)
{
	switch (t) {
	case USB_GADGET_FSERIAL_TRANSPORT_TTY:
		return "TTY";
	case USB_GADGET_FSERIAL_TRANSPORT_SDIO:
		return "SDIO";
	case USB_GADGET_FSERIAL_TRANSPORT_SMD:
		return "SMD";
	}

	return "NONE";
}

static int gport_setup(struct usb_configuration *c)
{
        int ret = 0;

        pr_debug("%s: no_tty_ports:%u no_sdio_ports: %u nr_ports:%u\n",
                        __func__, no_tty_ports, no_sdio_ports, nr_ports);

        if (no_tty_ports)
                ret = gserial_setup(c->cdev->gadget, no_tty_ports);
        if (no_sdio_ports)
                ret = gsdio_setup(c->cdev->gadget, no_sdio_ports);
        if (no_smd_ports)
                ret = gsmd_setup(c->cdev->gadget, no_smd_ports);

        return ret;
}
static int gport_connect(struct f_acm *acm)
{
        unsigned port_num;

        port_num = gdm_ports[acm->port_num].client_port_num;


        pr_debug("%s: transport:%s f_acm:%p gserial:%p port_num:%d cl_port_no:%d\n",
                        __func__, transport_to_str(acm->transport),
                        acm, &acm->port, acm->port_num, port_num);

        switch (acm->transport) {
        case USB_GADGET_FSERIAL_TRANSPORT_TTY:
                gserial_connect(&acm->port, port_num);
                break;
        case USB_GADGET_FSERIAL_TRANSPORT_SDIO:
                gsdio_connect(&acm->port, port_num);
                break;
        case USB_GADGET_FSERIAL_TRANSPORT_SMD:
                gsmd_connect(&acm->port, port_num);
                break;
        default:
                pr_err("%s: Un-supported transport: %s\n", __func__,
                                transport_to_str(acm->transport));
                return -ENODEV;
        }

        return 0;
}
static int gport_disconnect(struct f_acm *acm)
{
        unsigned port_num;

        port_num = gdm_ports[acm->port_num].client_port_num;

        pr_debug("%s: transport:%s f_acm:%p gserial:%p port_num:%d cl_pno:%d\n",
                        __func__, transport_to_str(acm->transport),
                        acm, &acm->port, acm->port_num, port_num);

        switch (acm->transport) {
        case USB_GADGET_FSERIAL_TRANSPORT_TTY:
                gserial_disconnect(&acm->port);
                break;
        case USB_GADGET_FSERIAL_TRANSPORT_SDIO:
                gsdio_disconnect(&acm->port, port_num);
                break;
        case USB_GADGET_FSERIAL_TRANSPORT_SMD:
                gsmd_disconnect(&acm->port, port_num);
                break;
        default:
                pr_err("%s: Un-supported transport:%s\n", __func__,
                                transport_to_str(acm->transport));
                return -ENODEV;
        }

        return 0;
}
#endif

/*-------------------------------------------------------------------------*/

/* notification endpoint uses smallish and infrequent fixed-size messages */

#define GS_LOG2_NOTIFY_INTERVAL		5	/* 1 << 5 == 32 msec */
/* FUJITSU:2013-01-16 USB mod start */
#if 0
#define GS_NOTIFY_MAXPACKET		10	/* notification + 2 bytes */
#else
#define GS_NOTIFY_MAXPACKET		16	/* default value @ LA161 */
#endif
/* FUJITSU:2013-01-16 USB mod end */

/* interface and class descriptors: */

static struct usb_interface_assoc_descriptor
dm_iad_descriptor = {
	.bLength =		sizeof dm_iad_descriptor,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount = 	2,	// control + data
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ACM_OM,
	.bFunctionProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	.iFunction =		0
};


static struct usb_interface_descriptor dm_control_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ACM_OM,
	.bInterfaceProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	.iInterface =		0
};

static struct usb_interface_descriptor dm_data_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	.iInterface =		0
};

static struct usb_cdc_header_desc dm_header_desc = {
	.bLength =		sizeof(dm_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor
dm_call_mgmt_descriptor = {
	.bLength =		sizeof(dm_call_mgmt_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,
	.bmCapabilities =	USB_CDC_CAP_BOTH,
	/* .bDataInterface = DYNAMIC */
};

static struct usb_cdc_acm_descriptor dm_descriptor = {
	.bLength =		sizeof(dm_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,
	.bmCapabilities =	USB_CDC_CAP_VS_REQ,
};

static struct usb_cdc_union_desc dm_union_desc = {
	.bLength =		sizeof(dm_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

static struct usb_om_interface_desc om_dm_desc = {
	.bLength =		sizeof(om_dm_desc),
	.bDescriptorType =	OM_SPECIFIC_INTERFACE,
	.bDescriptorSubType =	USB_MACM_TYPE,
	.bType =		OM_BTYPE_AB_2,
//	.bType =		OM_BTYPE_AB_1,
	.bMode_1 =		OM_BMODE_MODEM,
	.bMode_2 =		OM_BMODE_VENDOR1,
};

/* full speed support: */

static struct usb_endpoint_descriptor dm_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(0x10),
	.bInterval =		0x08,
};

static struct usb_endpoint_descriptor dm_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor dm_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *dm_fs_function[] = {
	(struct usb_descriptor_header *) &dm_iad_descriptor,
	(struct usb_descriptor_header *) &dm_control_interface_desc,
	(struct usb_descriptor_header *) &dm_header_desc,
	(struct usb_descriptor_header *) &dm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &dm_descriptor,
	(struct usb_descriptor_header *) &dm_union_desc,
        (struct usb_descriptor_header *) &om_dm_desc,
	(struct usb_descriptor_header *) &dm_fs_notify_desc,
	(struct usb_descriptor_header *) &dm_data_interface_desc,
	(struct usb_descriptor_header *) &dm_fs_in_desc,
	(struct usb_descriptor_header *) &dm_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor dm_hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(0x10),
	.bInterval =		0x08,
};

static struct usb_endpoint_descriptor dm_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor dm_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *dm_hs_function[] = {
	(struct usb_descriptor_header *) &dm_iad_descriptor,
	(struct usb_descriptor_header *) &dm_control_interface_desc,
	(struct usb_descriptor_header *) &dm_header_desc,
	(struct usb_descriptor_header *) &dm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &dm_descriptor,
	(struct usb_descriptor_header *) &dm_union_desc,
        (struct usb_descriptor_header *) &om_dm_desc,
	(struct usb_descriptor_header *) &dm_hs_notify_desc,
	(struct usb_descriptor_header *) &dm_data_interface_desc,
	(struct usb_descriptor_header *) &dm_hs_in_desc,
	(struct usb_descriptor_header *) &dm_hs_out_desc,
	NULL,
};

/* string descriptors: */

#define DM_CTRL_IDX	0
#define DM_DATA_IDX	1
#define DM_IAD_IDX	2

/* static strings, in UTF-8 */
static struct usb_string dm_string_defs[] = {
	[DM_CTRL_IDX].s = "CDC Abstract Control Model (ACM)",
	[DM_DATA_IDX].s = "CDC ACM Data",
	[DM_IAD_IDX ].s = "CDC Serial",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings dm_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		dm_string_defs,
};

static struct usb_gadget_strings *dm_strings[] = {
	&dm_string_table,
	NULL,
};

/*-------------------------------------------------------------------------*/

/* ACM control ... data handling is delegated to tty library code.
 * The main task of this function is to activate and deactivate
 * that code based on device state; track parameters like line
 * speed, handshake state, and so on; and issue notifications.
 */

static void dm_complete_set_line_coding(struct usb_ep *ep,
		struct usb_request *req)
{
	struct f_dm	*dm = ep->driver_data;
//	struct usb_composite_dev *cdev = dm->port.func.config->cdev;

	if (req->status != 0) {
		pr_err("dm ttyGS%d completion, err %d\n",
				dm->port_num, req->status);
		return;
	}

	/* normal completion */
	if (req->actual != sizeof(dm->port_line_coding)) {
		pr_err("dm ttyGS%d short resp, len %d\n",
				dm->port_num, req->actual);
		usb_ep_set_halt(ep);
	} else {
		struct usb_cdc_line_coding	*value = req->buf;

		/* REVISIT:  we currently just remember this data.
		 * If we change that, (a) validate it first, then
		 * (b) update whatever hardware needs updating,
		 * (c) worry about locking.  This is information on
		 * the order of 9600-8-N-1 ... most of which means
		 * nothing unless we control a real RS232 line.
		 */
		dm->port_line_coding = *value;
	}
}

static int dm_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_dm		*dm = func_to_dm(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* composite driver infrastructure handles everything except
	 * CDC class messages; interface activation uses set_alt().
	 *
	 * Note CDC spec table 4 lists the ACM request profile.  It requires
	 * encapsulated command support ... we don't handle any, and respond
	 * to them by stalling.  Options include get/set/clear comm features
	 * (not that useful) and SEND_BREAK.
	 */
#if 0
	if (dm->port.func.disabled){
		return value;
	}
#endif
	pr_debug( "dm ttyGS%d req%02x.%02x v%04x i%04x l%d\n",
		dm->port_num, ctrl->bRequestType, ctrl->bRequest,
		w_value, w_index, w_length);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	/* SET_LINE_CODING ... just read and save what the host sends */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_LINE_CODING:
		printk("####dm SETLINK\n");
		if (w_length != sizeof(struct usb_cdc_line_coding)
				|| w_index != dm->ctrl_id)
			goto invalid;

		value = w_length;
		cdev->gadget->ep0->driver_data = dm;
		req->complete = dm_complete_set_line_coding;
		break;

	/* GET_LINE_CODING ... return what host sent, or initial value */
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_GET_LINE_CODING:
		if (w_index != dm->ctrl_id)
			goto invalid;

		value = min_t(unsigned, w_length,
				sizeof(struct usb_cdc_line_coding));
		memcpy(req->buf, &dm->port_line_coding, value);
		break;

	/* SET_CONTROL_LINE_STATE ... save what the host sent */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		if (w_index != dm->ctrl_id)
			goto invalid;

		value = 0;

		/* FIXME we should not allow data to flow until the
		 * host sets the ACM_CTRL_DTR bit; and when it clears
		 * that bit, we should return to that no-flow state.
		 */
		dm->port_handshake_bits = w_value;
		break;

	default:
invalid:
		pr_debug("invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		pr_err("dm ttyGS%d req%02x.%02x v%04x i%04x l%d\n",
			dm->port_num, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			pr_err("dm response on ttyGS%d, err %d\n",
					dm->port_num, value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int dm_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_dm		*dm = func_to_dm(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	/* we know alt == 0, so this is an activation or a reset */

	if (intf == dm->ctrl_id) {
		if (dm->notify->driver_data) {
			pr_debug("reset dm control interface %d\n", intf);
			usb_ep_disable(dm->notify);
		} else {
			pr_debug("init dm ctrl interface %d\n", intf);
		}
/* FUJITSU:2013-01-16 USB start */
#if 0
		dm->notify_desc = ep_choose(cdev->gadget,
				dm->hs.notify,
				dm->fs.notify);
		usb_ep_enable(dm->notify, dm->notify_desc);
#endif
		if (config_ep_by_speed(cdev->gadget, f, dm->notify))
			return -EINVAL;
		usb_ep_enable(dm->notify);
/* FUJITSU:2013-01-16 USB end */
		dm->notify->driver_data = dm;

	} else if (intf == dm->data_id) {
#if 0
		if (dm->port.in->driver_data) {
			DBG(cdev, "reset dm ttyGS%d\n", dm->port_num);
			gserial_disconnect(&dm->port);
		} else {
			DBG(cdev, "activate dm ttyGS%d\n", dm->port_num);
		}
		dm->port.in_desc = ep_choose(cdev->gadget,
				dm->hs.in, dm->fs.in);
		dm->port.out_desc = ep_choose(cdev->gadget,
				dm->hs.out, dm->fs.out);
		gserial_connect(&dm->port, dm->port_num);
#endif

	} else
		return -EINVAL;

	return 0;
}

static void dm_disable(struct usb_function *f)
{
	struct f_dm	*dm = func_to_dm(f);
//	struct usb_composite_dev *cdev = f->config->cdev;

	pr_debug("dm ttyGS%d deactivated\n", dm->port_num);
	gserial_disconnect(&dm->port);
	usb_ep_disable(dm->notify);
	dm->notify->driver_data = NULL;
}

/*-------------------------------------------------------------------------*/

/**
 * dm_cdc_notify - issue CDC notification to host
 * @dm: wraps host to be notified
 * @type: notification type
 * @value: Refer to cdc specs, wValue field.
 * @data: data to be sent
 * @length: size of data
 * Context: irqs blocked, dm->lock held, dm_notify_req non-null
 *
 * Returns zero on success or a negative errno.
 *
 * See section 6.3.5 of the CDC 1.1 specification for information
 * about the only notification we issue:  SerialState change.
 */
static int dm_cdc_notify(struct f_dm *dm, u8 type, u16 value,
		void *data, unsigned length)
{
	struct usb_ep			*ep = dm->notify;
	struct usb_request		*req;
	struct usb_cdc_notification	*notify;
	const unsigned			len = sizeof(*notify) + length;
	void				*buf;
	int				status;

	req = dm->notify_req;
	dm->notify_req = NULL;
	dm->pending = false;

	req->length = len;
	notify = req->buf;
	buf = notify + 1;

	notify->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	notify->bNotificationType = type;
	notify->wValue = cpu_to_le16(value);
	notify->wIndex = cpu_to_le16(dm->ctrl_id);
	notify->wLength = cpu_to_le16(length);
	memcpy(buf, data, length);

	/* ep_queue() can complete immediately if it fills the fifo... */
	spin_unlock(&dm->lock);
	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	spin_lock(&dm->lock);

	if (status < 0) {
		pr_err("dm ttyGS%d can't notify serial state, %d\n",
				dm->port_num, status);
		dm->notify_req = req;
	}

	return status;
}

static int dm_notify_serial_state(struct f_dm *dm)
{
//	struct usb_composite_dev *cdev = dm->port.func.config->cdev;
	int			status;

	spin_lock(&dm->lock);
	if (dm->notify_req) {
		pr_debug("dm ttyGS%d serial state %04x\n",
				dm->port_num, dm->serial_state);
		status = dm_cdc_notify(dm, USB_CDC_NOTIFY_SERIAL_STATE,
				0, &dm->serial_state, sizeof(dm->serial_state));
	} else {
		dm->pending = true;
		status = 0;
	}
	spin_unlock(&dm->lock);
	return status;
}

static void dm_cdc_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_dm		*dm = req->context;
	u8			doit = false;

	/* on this call path we do NOT hold the port spinlock,
	 * which is why ACM needs its own spinlock
	 */
	spin_lock(&dm->lock);
	if (req->status != -ESHUTDOWN)
		doit = dm->pending;
	dm->notify_req = req;
	spin_unlock(&dm->lock);

	if (doit)
		dm_notify_serial_state(dm);
}

/* connect == the TTY link is open */

static void dm_connect(struct gserial *port)
{
	struct f_dm		*dm = port_to_dm(port);

	dm->serial_state |= ACM_CTRL_DSR | ACM_CTRL_DCD;
	dm_notify_serial_state(dm);
}

static void dm_disconnect(struct gserial *port)
{
	struct f_dm		*dm = port_to_dm(port);

	dm->serial_state &= ~(ACM_CTRL_DSR | ACM_CTRL_DCD);
#if 0
	dm_notify_serial_state(dm);
#endif
}

static int dm_send_break(struct gserial *port, int duration)
{
	struct f_dm		*dm = port_to_dm(port);
	u16			state;

	state = dm->serial_state;
	state &= ~ACM_CTRL_BRK;
	if (duration)
		state |= ACM_CTRL_BRK;

	dm->serial_state = state;
	return dm_notify_serial_state(dm);
}

static int dm_send_modem_ctrl_bits(struct gserial *port, int ctrl_bits)
{
	struct f_dm *dm = port_to_dm(port);

	dm->serial_state = ctrl_bits;

	return dm_notify_serial_state(dm);
}


/*-------------------------------------------------------------------------*/

/* ACM function driver setup/binding */
static int
dm_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_dm		*dm = func_to_dm(f);
	int			status;
	struct usb_ep		*ep;

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	dm->ctrl_id = status;
	dm_iad_descriptor.bFirstInterface = status;

	dm_control_interface_desc.bInterfaceNumber = status;
	dm_union_desc .bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	dm->data_id = status;

	dm_data_interface_desc.bInterfaceNumber = status;
	dm_union_desc.bSlaveInterface0 = status;
	dm_call_mgmt_descriptor.bDataInterface = status;

	status = -ENODEV;

	ep = usb_ep_autoconfig(cdev->gadget, &dm_fs_notify_desc);
	if (!ep)
		goto fail;
	dm->notify = ep;
	ep->driver_data = cdev;	/* claim */

	/* allocate notification */
	dm->notify_req = gs_alloc_req(ep,
			sizeof(struct usb_cdc_notification) + 2,
			GFP_KERNEL);
	if (!dm->notify_req)
		goto fail;

	dm->notify_req->complete = dm_cdc_notify_complete;
	dm->notify_req->context = dm;

	/* allocate instance-specific endpoints */
#if 0
	ep = usb_ep_autoconfig(cdev->gadget, &dm_fs_in_desc);
#endif
	if (!ep)
		goto fail;
	dm->port.in = ep;
	ep->driver_data = cdev;	/* claim */
#if 0
	ep = usb_ep_autoconfig(cdev->gadget, &dm_fs_out_desc);
#endif
	if (!ep)
		goto fail;
	dm->port.out = ep;
	ep->driver_data = cdev;	/* claim */

	/* copy descriptors, and track endpoint copies */
	f->descriptors = usb_copy_descriptors(dm_fs_function);
	if (!f->descriptors)
		goto fail;

/* FUJITSU:2013-01-16 USB start */
#if 0
	dm->fs.in = usb_find_endpoint(dm_fs_function,
			f->descriptors, &dm_fs_in_desc);
	dm->fs.out = usb_find_endpoint(dm_fs_function,
			f->descriptors, &dm_fs_out_desc);
	dm->fs.notify = usb_find_endpoint(dm_fs_function,
			f->descriptors, &dm_fs_notify_desc);
#endif
/* FUJITSU:2013-01-16 USB end */

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		dm_hs_in_desc.bEndpointAddress =
				dm_fs_in_desc.bEndpointAddress;
		dm_hs_out_desc.bEndpointAddress =
				dm_fs_out_desc.bEndpointAddress;
		dm_hs_notify_desc.bEndpointAddress =
				dm_fs_notify_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(dm_hs_function);

/* FUJITSU:2013-01-16 USB start */
#if 0
		dm->hs.in = usb_find_endpoint(dm_hs_function,
				f->hs_descriptors, &dm_hs_in_desc);
		dm->hs.out = usb_find_endpoint(dm_hs_function,
				f->hs_descriptors, &dm_hs_out_desc);
		dm->hs.notify = usb_find_endpoint(dm_hs_function,
				f->hs_descriptors, &dm_hs_notify_desc);
#endif
/* FUJITSU:2013-01-16 USB end */
	}

	pr_debug("dm ttyGS%d: %s speed IN/%s OUT/%s NOTIFY/%s\n",
			dm->port_num,
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			dm->port.in->name, dm->port.out->name,
			dm->notify->name);
	return 0;

fail:
	if (dm->notify_req)
		gs_free_req(dm->notify, dm->notify_req);

	/* we might as well release our claims on endpoints */
	if (dm->notify)
		dm->notify->driver_data = NULL;
	if (dm->port.out)
		dm->port.out->driver_data = NULL;
	if (dm->port.in)
		dm->port.in->driver_data = NULL;

	pr_err("%s/%p: can't bind, err %d\n", f->name, f, status);

	return status;
}

static void
dm_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_dm		*dm = func_to_dm(f);

	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->descriptors);
	gs_free_req(dm->notify, dm->notify_req);
	kfree(dm);
}

/* Some controllers can't support CDC ACM ... */
static inline bool dm_can_support_cdc(struct usb_configuration *c)
{
	/* everything else is *probably* fine ... */
	return true;
}

/**
 * dm_bind_config - add a CDC ACM function to a configuration
 * @c: the configuration to support the CDC ACM instance
 * @port_num: /dev/ttyGS* port this interface will use
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 *
 * Caller must have called @gserial_setup() with enough ports to
 * handle all the ones it binds.  Caller is also responsible
 * for calling @gserial_cleanup() before module unload.
 */
int dm_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct f_dm	*dm;
	int		status;

	if (!dm_can_support_cdc(c))
		return -EINVAL;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string IDs, and patch descriptors */
	if (dm_string_defs[DM_CTRL_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		dm_string_defs[DM_CTRL_IDX].id = status;

		/* dm_control_interface_desc.iInterface = status; */
		 dm_control_interface_desc.iInterface = status;

		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		dm_string_defs[DM_DATA_IDX].id = status;

		/* dm_data_interface_desc.iInterface = status; */
		dm_data_interface_desc.iInterface = status;

		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		dm_string_defs[DM_IAD_IDX].id = status;

		/* dm_iad_descriptor.iFunction = status; */
		dm_iad_descriptor.iFunction = status;
	}

	/* allocate and initialize one new instance */
	dm = kzalloc(sizeof *dm, GFP_KERNEL);
	if (!dm)
		return -ENOMEM;

	spin_lock_init(&dm->lock);

	dm->port_num = port_num;
        dm->transport = gdm_ports[port_num].transport;

	dm->port.connect = dm_connect;
	dm->port.disconnect = dm_disconnect;
	dm->port.send_break = dm_send_break;
	dm->port.send_modem_ctrl_bits = dm_send_modem_ctrl_bits;

	dm->port.func.name = "dm";
	dm->port.func.strings = dm_strings;
	/* descriptors are per-instance copies */
	dm->port.func.bind = dm_bind;
	dm->port.func.unbind = dm_unbind;
	dm->port.func.set_alt = dm_set_alt;
	dm->port.func.setup = dm_setup;
	dm->port.func.disable = dm_disable;

	/* start disabled */
#if 0
	dm->port.func.disabled = 0;
#endif

	status = usb_add_function(c, &dm->port.func);
	if (status)
		kfree(dm);
	return status;
}
