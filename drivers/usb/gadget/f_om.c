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

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include <linux/usb/android_composite.h>
#include <linux/usb/cdc.h>

#include "u_serial.h"
#include "om.h"

#define OM_SIZE_MAX		2053
#define OM_BULK_BUFFER_SIZE	(OM_SIZE_MAX * 2 + 2) /* == 4108 */

/* number of tx requests to allocate */
#define TX_REQ_MAX 4

static const char om_shortname[] = "om";

struct om_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;
	spinlock_t notify_lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;
	struct usb_ep *ep_notify;

	atomic_t online;
	atomic_t error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	struct usb_request *notify_req;
	int rx_done;

	u8 ctrl_id, data_id;
};

/*-------------------------------------------------------------------------*/

/* interface and class descriptors: */

static struct usb_interface_assoc_descriptor
om_iad_descriptor = {
	.bLength =		sizeof(om_iad_descriptor),
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount = 	2,	// control + data
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ACM_OM,
	.bFunctionProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	.iFunction =		0
};

static struct usb_interface_descriptor om_control_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ACM_OM,
	.bInterfaceProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	.iInterface =		0
};

static struct usb_interface_descriptor om_data_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	.iInterface =		0
};

static struct usb_cdc_header_desc om_header_desc = {
	.bLength =		sizeof(om_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor
om_call_mgmt_descriptor = {
	.bLength =		sizeof(om_call_mgmt_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,
	.bmCapabilities =	USB_CDC_CAP_BOTH,
	/* .bDataInterface = DYNAMIC */
};

static struct usb_cdc_acm_descriptor om_descriptor = {
	.bLength =		sizeof(om_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,
	.bmCapabilities =	USB_CDC_CAP_VS_REQ,
};

static struct usb_cdc_union_desc om_union_desc = {
	.bLength =		sizeof(om_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

static struct usb_om_interface_desc om_obex_desc = {
	.bLength =		sizeof(om_obex_desc),
	.bDescriptorType =	OM_SPECIFIC_INTERFACE,
	.bDescriptorSubType =	USB_MACM_TYPE,
	.bType =		OM_BTYPE_AB_2,
//	.bType =		OM_BTYPE_AB_1,
	.bMode_1 =		OM_BMODE_OBEX,
	.bMode_2 =		OM_BMODE_VENDOR1,
};

/* high speed support: */
static struct usb_endpoint_descriptor om_hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(0x10),
	.bInterval =		0x08,
};

static struct usb_endpoint_descriptor om_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor om_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

/* full speed support: */
static struct usb_endpoint_descriptor om_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(0x10),
/* FUJITSU:2011-08-12 USB start */
	.bInterval =		0x08,
//	.bInterval =		0x10,
/* FUJITSU:2011-08-12 USB end */
};

static struct usb_endpoint_descriptor om_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
/* FUJITSU:2011-08-12 USB start */
//	.wMaxPacketSize =	cpu_to_le16(64),
/* FUJITSU:2011-08-12 USB end */
};

static struct usb_endpoint_descriptor om_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
/* FUJITSU:2011-08-12 USB start */
//	.wMaxPacketSize =	cpu_to_le16(64),
/* FUJITSU:2011-08-12 USB end */
};

/* dummy descriptors for pipe group 3 */
static struct usb_descriptor_header *om_hs_function[] = {
	(struct usb_descriptor_header *) &om_iad_descriptor,
	(struct usb_descriptor_header *) &om_control_interface_desc,
	(struct usb_descriptor_header *) &om_header_desc,
	(struct usb_descriptor_header *) &om_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &om_descriptor,
	(struct usb_descriptor_header *) &om_union_desc,
        (struct usb_descriptor_header *) &om_obex_desc,
	(struct usb_descriptor_header *) &om_hs_notify_desc,
	(struct usb_descriptor_header *) &om_data_interface_desc,
	(struct usb_descriptor_header *) &om_hs_in_desc,
	(struct usb_descriptor_header *) &om_hs_out_desc,
	NULL,
};

static struct usb_descriptor_header *om_fs_function[] = {
	(struct usb_descriptor_header *) &om_iad_descriptor,
	(struct usb_descriptor_header *) &om_control_interface_desc,
	(struct usb_descriptor_header *) &om_header_desc,
	(struct usb_descriptor_header *) &om_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &om_descriptor,
	(struct usb_descriptor_header *) &om_union_desc,
        (struct usb_descriptor_header *) &om_obex_desc,
	(struct usb_descriptor_header *) &om_fs_notify_desc,
	(struct usb_descriptor_header *) &om_data_interface_desc,
	(struct usb_descriptor_header *) &om_fs_in_desc,
	(struct usb_descriptor_header *) &om_fs_out_desc,
	NULL,
};


/* temporary variable used between om_open() and om_gadget_bind() */
static struct om_dev *_om_dev;

//static atomic_t om_enable_excl;

/* string descriptors: */

#define OM_CTRL_IDX	0
#define OM_DATA_IDX	1
#define OM_IAD_IDX	2

/* static strings, in UTF-8 */
static struct usb_string om_string_defs[] = {
	[OM_CTRL_IDX].s = "CDC Object Exchange (OBEX)",
	[OM_DATA_IDX].s = "CDC OBEX Data",
	[OM_IAD_IDX ].s = "CDC Serial",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings om_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		om_string_defs,
};

static struct usb_gadget_strings *om_strings[] = {
	&om_string_table,
	NULL,
};

/* FUJITSU:2011-08-12 USB start */
//extern void android_debugusb_set_port(int product_id);
/* FUJITSU:2011-08-12 USB end */

static inline struct om_dev *func_to_om(struct usb_function *f)
{
	return container_of(f, struct om_dev, function);
}

static struct usb_request *om_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void om_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int om_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void om_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

/* add a request to the tail of a list */
void om_req_put(struct om_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
struct usb_request *om_req_get(struct om_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void om_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct om_dev *dev = _om_dev;

	if (req->status != 0)
		atomic_set(&dev->error, 1);

	om_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void om_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct om_dev *dev = _om_dev;
printk(KERN_DEBUG "USB_DEBUG:%s (%dline)\n",__func__,__LINE__);

	dev->rx_done = 1;
	if (req->status != 0)
		atomic_set(&dev->error, 1);

	wake_up(&dev->read_wq);
}
#if 1
static int om_ack_notify(struct om_dev *dev, u16 value, u16 index)
{
	struct usb_ep			*ep = dev->ep_notify;
	struct usb_request		*req;
	struct usb_ctrlrequest		*ack_notify;
	int				status;

	req = dev->notify_req;
	dev->notify_req = NULL;
	req->length = sizeof(*ack_notify);
	ack_notify = req->buf;

	ack_notify->bRequestType = USB_REQ_TYPE_NOIFICATION;
	ack_notify->bRequest = USB_REQUEST_ACKNOWLEDGE;
	ack_notify->wValue = cpu_to_le16(value);
	ack_notify->wIndex = cpu_to_le16(index);
	ack_notify->wLength = 0;

	/* ep_queue() can complete immediately if it fills the fifo... */
	spin_unlock(&dev->notify_lock);
	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	spin_lock(&dev->notify_lock);

	if (status < 0) {
		printk(KERN_ERR "%s:OM can't notify serial state, %d\n",__func__,status);
		dev->notify_req = req;
	}

	return status;
}
static int om_ack_notify_locked(struct om_dev *dev, u16 value, u16 index)
{
	int status;
	spin_lock(&dev->notify_lock);
	if (dev->notify_req) {
		status = om_ack_notify(dev, value, index);
	} else {
		status = 0;
	}
	spin_unlock(&dev->notify_lock);
	return status;
}
#endif
static void om_cdc_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct om_dev	*dev = req->context;

	spin_lock(&dev->notify_lock);
	dev->notify_req = req;
	spin_unlock(&dev->notify_lock);
}

static int om_create_endpoints(struct om_dev *dev,
				struct usb_endpoint_descriptor *notify_desc,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;
	DBG(cdev, "create_endpoints dev: %p\n", dev);
	ep = usb_ep_autoconfig(cdev->gadget, notify_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_notify failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_notify got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_notify = ep;

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	/* allocate notification */
	req = om_request_new(dev->ep_notify, sizeof(struct usb_ctrlrequest));
	if (!req)
		goto fail;
	req->complete = om_cdc_notify_complete;
	req->context = dev;
	dev->notify_req = req;

	/* now allocate requests for our endpoints */
	req = om_request_new(dev->ep_out, OM_BULK_BUFFER_SIZE);
	if (!req)
		goto fail;
	req->complete = om_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = om_request_new(dev->ep_in, OM_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = om_complete_in;
		om_req_put(dev, &dev->tx_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "om_bind() could not allocate requests\n");
	return -1;
}

static ssize_t om_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	struct om_dev *dev = fp->private_data;
//	struct usb_composite_dev *om_cdev = dev->cdev;
	struct usb_request *req;
	int r = count, xfer = 0;
	int ret;

	pr_debug("om_read(%d)\n", count);
	if (!_om_dev)
		return -ENODEV;

	if (count > OM_BULK_BUFFER_SIZE)
		return -EINVAL;

	if (om_lock(&dev->read_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(atomic_read(&dev->online) || atomic_read(&dev->error))) {
		pr_debug("om_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->read_wq,
			(atomic_read(&dev->online) ||
			atomic_read(&dev->error)));
		if (ret < 0) {
			om_unlock(&dev->read_excl);
			return ret;
		}
	}

	if (atomic_read(&dev->error)) {
		printk(KERN_ERR "om_read return before the LABEL requeue_req:\n");
		r = -EIO;
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req;
	req->length = count;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		pr_debug("om_read: failed to queue req %p (%d)\n", req, ret);
		r = -EIO;
		atomic_set(&dev->error, 1);
		goto done;
	} else {
		pr_debug("rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		atomic_set(&dev->error, 1);
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}

	if (!atomic_read(&dev->error)) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		pr_debug("rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		/* xfer = (req->actual < count) ? req->actual : count; */
		r = xfer;

		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;

	} else
		r = -EIO;

done:
	om_unlock(&dev->read_excl);
	pr_debug("om_read returning %d\n", r);
	return r;
}

static ssize_t om_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct om_dev *dev = fp->private_data;
//	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	if (!_om_dev)
		return -ENODEV;
	pr_debug("om_write(%d)\n", count);

	if (om_lock(&dev->write_excl))
		return -EBUSY;

	while (count > 0) {
		if (atomic_read(&dev->error)) {
			pr_debug("om_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = om_req_get(dev, &dev->tx_idle)) ||
			 atomic_read(&dev->error)));

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > OM_BULK_BUFFER_SIZE)
				xfer = OM_BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_debug("om_write: xfer error %d\n", ret);
				atomic_set(&dev->error, 1);
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		om_req_put(dev, &dev->tx_idle, req);

	om_unlock(&dev->write_excl);
	pr_debug("om_write returning %d\n", r);
	return r;
}

static int om_open(struct inode *ip, struct file *fp)
{
	printk(KERN_INFO "om_open\n");
	if (!_om_dev)
		return -ENODEV;

	if (om_lock(&_om_dev->open_excl))
		return -EBUSY;

	fp->private_data = _om_dev;

	/* clear the error latch */
	atomic_set(&_om_dev->error, 0);

	return 0;
}

static int om_flush(struct file *fp, fl_owner_t id)
{
	printk(KERN_INFO "om_flush\n");
	if (om_lock(&_om_dev->read_excl)) {
		atomic_set(&_om_dev->error, 1);
		wake_up(&_om_dev->read_wq);
	}
	else {
		om_unlock(&_om_dev->read_excl);
	}
	return 0;
}

static int om_release(struct inode *ip, struct file *fp)
{
	printk(KERN_INFO "om_release\n");
	om_unlock(&_om_dev->open_excl);
	return 0;
}

/* file operations for OM device /dev/android_om */
static struct file_operations om_fops = {
	.owner = THIS_MODULE,
	.read = om_read,
	.write = om_write,
	.open = om_open,
	.flush = om_flush,
	.release = om_release,
};

static struct miscdevice om_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = om_shortname,
	.fops = &om_fops,
};

static int om_function_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct om_dev		*dev = func_to_om(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

#if 0
	if (dev->function.disabled) {
		return value;
	}
#endif
	printk(KERN_INFO "OM req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	/* USB_REQ_SET_LINK ... just read and save what the host sends */
	case (USB_REQ_TYPE_REQUEST << 8)
			| USB_REQ_SET_LINK:
		printk(KERN_DEBUG "####om SETLINK len:[%d]\n",w_length);
		if (w_length != 2)
			goto setup_skip;

		value = w_length;
		cdev->gadget->ep0->driver_data = dev;
		printk(KERN_DEBUG "####om SETLINK value:[%d]\n",value);
		break;

	/* ACTIVATE_MODE: ... return what host sent, or initial value */
	case (USB_REQ_TYPE_REQUEST << 8)
			| USB_REQ_ACTIVATE_MODE:
		printk(KERN_DEBUG "####om ACTIVATE_MODE\n");
		if (w_index != dev->ctrl_id)
			goto invalid;

		value = w_length;
		cdev->gadget->ep0->driver_data = dev;
		printk(KERN_DEBUG "####om ACTIVATE_MODE value:[%d]\n",value);
		break;

	default:
invalid:
		printk(KERN_ERR "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		printk(KERN_INFO "OM req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			printk(KERN_ERR "%s:OM response, error %d\n",__func__,value);
//			ERROR(cdev, "OM response, err %d\n", value);
		}
		value = om_ack_notify_locked(dev, USB_WVALUE_ACK, ctrl->wIndex);
	}
setup_skip:

	/* device either stalls (value < 0) or reports success */
	return value;
}

/*-------------------------------------------------------------------------*/

/* connect == the TTY link is open */

/*-------------------------------------------------------------------------*/

/* OM function driver setup/binding */
static int
om_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct om_dev		*dev = func_to_om(f);
	int			id;
	int			ret;

	dev->cdev = cdev;
	DBG(cdev, "om_function_bind dev: %p\n", dev);

	/* allocate instance-specific interface IDs, and patch descriptors */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;

	dev->ctrl_id = id;
	om_iad_descriptor.bFirstInterface = id;
	om_control_interface_desc.bInterfaceNumber = id;
	om_union_desc.bMasterInterface0 = id;

	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->data_id = id;

	om_data_interface_desc.bInterfaceNumber = id;
	om_union_desc.bSlaveInterface0 = id;
	om_call_mgmt_descriptor.bDataInterface = id;

	/* allocate endpoints */
	ret = om_create_endpoints(dev, &om_fs_notify_desc,
			&om_fs_in_desc,
			&om_fs_out_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		om_hs_in_desc.bEndpointAddress =
				om_fs_in_desc.bEndpointAddress;
		om_hs_out_desc.bEndpointAddress =
				om_fs_out_desc.bEndpointAddress;
		om_hs_notify_desc.bEndpointAddress =
				om_fs_notify_desc.bEndpointAddress;
	}

	DBG(cdev, "OM : %s speed IN/%s OUT/%s NOTIFY/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			dev->ep_in->name, dev->ep_out->name,
			dev->ep_notify->name);

	return 0;
}

static void
om_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct om_dev	*dev = func_to_om(f);
	struct usb_request *req;

	atomic_set(&dev->online, 0);
	atomic_set(&dev->error, 1);

	wake_up(&dev->read_wq);

	om_request_free(dev->rx_req, dev->ep_out);
#if 0
	while ((req = om_req_get(dev, &dev->tx_idle)))
	{
		om_request_free(req, dev->ep_in);
		om_request_free(req, dev->ep_notify);
	}
#else
	om_request_free(dev->notify_req, dev->ep_notify);
	while ((req = om_req_get(dev, &dev->tx_idle)))
		om_request_free(req, dev->ep_in);
#endif
}

static int om_function_set_alt(struct usb_function *f,
		 unsigned intf, unsigned alt)
{
	struct om_dev	*dev = func_to_om(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "om_function_set_alt intf: %d alt: %d\n", intf, alt);
/* FUJITSU:2013-01-16 USB ep-config adjust start */
#if 0
	ret = usb_ep_enable(dev->ep_notify,
			ep_choose(cdev->gadget,
				&om_hs_notify_desc,
				&om_fs_notify_desc));
#endif
	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_notify);
	if (ret) {
		dev->ep_in->desc = NULL;
		return ret;
	}
	ret = usb_ep_enable(dev->ep_notify);
	if (ret)
		return ret;

#if 0
	ret = usb_ep_enable(dev->ep_in,
			ep_choose(cdev->gadget,
				&om_hs_in_desc,
				&om_fs_in_desc));
#endif

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret) {
		dev->ep_in->desc = NULL;
		return ret;
	}
	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

#if 0
	ret = usb_ep_enable(dev->ep_out,
			ep_choose(cdev->gadget,
				&om_hs_out_desc,
				&om_fs_out_desc));
#endif

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret) {
		dev->ep_out->desc = NULL;
		usb_ep_disable(dev->ep_in);
		return ret;
	}
	ret = usb_ep_enable(dev->ep_out);
/* FUJITSU:2013-01-16 USB ep-config adjust end */

	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	atomic_set(&dev->online, 1);
	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void om_function_disable(struct usb_function *f)
{
	struct om_dev	*dev = func_to_om(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	pr_debug("om_function_disable cdev %p\n", cdev);
	atomic_set(&dev->online, 0);
	atomic_set(&dev->error, 1);
	usb_ep_disable(dev->ep_notify);
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	pr_debug("%s:%s disabled\n", __func__,dev->function.name);
}

int om_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct om_dev *dev = _om_dev;
	int ret;

	printk(KERN_INFO "om_bind_config\n");

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string IDs, and patch descriptors */
	if (om_string_defs[OM_CTRL_IDX].id == 0) {
		ret = usb_string_id(c->cdev);
		if (ret < 0)
			return ret;
		om_string_defs[OM_CTRL_IDX].id = ret;

		/* om_control_interface_desc.iInterface = ret; */

		ret = usb_string_id(c->cdev);
		if (ret < 0)
			return ret;
		om_string_defs[OM_DATA_IDX].id = ret;

		/* om_data_interface_desc.iInterface = ret; */

		ret = usb_string_id(c->cdev);
		if (ret < 0)
			return ret;
		om_string_defs[OM_IAD_IDX].id = ret;

		/* om_iad_descriptor.iFunction = ret; */
	}

	dev->cdev = c->cdev;
	dev->function.name = "om";
	dev->function.descriptors = om_fs_function;
	dev->function.hs_descriptors = om_hs_function;
	dev->function.strings = om_strings;
	dev->function.bind = om_function_bind;
	dev->function.unbind = om_function_unbind;
	dev->function.set_alt = om_function_set_alt;
	dev->function.setup = om_function_setup;
	dev->function.disable = om_function_disable;

	return usb_add_function(c, &dev->function);
}

static int om_setup(void)
{
	struct om_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	INIT_LIST_HEAD(&dev->tx_idle);

	_om_dev = dev;

	ret = misc_register(&om_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	printk(KERN_ERR "om gadget driver failed to initialize\n");
	return ret;
}

static void om_cleanup(void)
{
	misc_deregister(&om_device);

	kfree(_om_dev);
	_om_dev = NULL;
}

