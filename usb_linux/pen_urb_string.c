#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/nls.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/uaccess.h>

struct usb_ctrlrequest ctrl_req;
struct completion config_done;

#define SIZE 50

static void ctrl_callback(struct urb *urb)
{
	complete((struct completion *)urb->context);
}

static void string_desc(struct usb_device *dev,
			int index)
{

    struct urb *ctrl_urb;
    char *tbuf;
    char buf[SIZE];
    int ctrl_pipe;
    int err;
    tbuf = kmalloc(256, GFP_KERNEL);

    ctrl_urb = usb_alloc_urb(0, GFP_KERNEL);

    ctrl_req.bRequestType = USB_DIR_IN;
    ctrl_req.bRequest = USB_REQ_GET_DESCRIPTOR;
    ctrl_req.wValue = (USB_DT_STRING << 8) + 1;
    ctrl_req.wIndex = 0x0409;
    ctrl_req.wLength = SIZE;

    ctrl_pipe = usb_sndctrlpipe(dev, 0);

    init_completion(&config_done);

    usb_fill_control_urb(ctrl_urb, dev,
		 ctrl_pipe,
		 (char *) &ctrl_req,
		 tbuf, SIZE, ctrl_callback,
		 &config_done);

    usb_submit_urb(ctrl_urb, GFP_ATOMIC);

    wait_for_completion(&config_done);

    err = utf16s_to_utf8s((wchar_t *) &tbuf[2],
			  (ctrl_urb->actual_length - 2) / 2,
			  UTF16_LITTLE_ENDIAN, buf, SIZE/2);
    buf[err] = 0;

    pr_info("string index %d is %s", index, buf);

    usb_free_urb(ctrl_urb);

    kfree(tbuf);
}

static int pen_probe(struct usb_interface *interface,
		     const struct usb_device_id *id)
{
    struct usb_device *device;

    pr_info("Pen Probe control urb Function");

    device = interface_to_usbdev(interface);

    string_desc(device, 1);
    string_desc(device, 2);
    string_desc(device, 3);

    return 0;
}

static void pen_disconnect(struct usb_interface *interface)
{
	pr_info("Disconnect");
}

/* Table of devices that work with this driver */
static struct usb_device_id pen_table[] =
{
    { USB_DEVICE(0x058F, 0x6387) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, pen_table);

static struct usb_driver pen_driver =
{
    .name = "pen_driver",
    .probe = pen_probe,
    .disconnect = pen_disconnect,
    .id_table = pen_table,
};

module_usb_driver(pen_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email@sarika-pugs.com>");
MODULE_DESCRIPTION("USB Pen Device Driver");
