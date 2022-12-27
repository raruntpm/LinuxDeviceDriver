#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/nls.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/uaccess.h>

#define SIZE 50
static void string_desc(struct usb_device *dev, int index)
{
    char buf[SIZE];
    char *tbuf;
    int err;

    tbuf = kmalloc(256, GFP_KERNEL);

    err = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			(USB_DT_STRING << 8) + index, 0x0409,
		       	tbuf, SIZE, USB_CTRL_GET_TIMEOUT);

    err = utf16s_to_utf8s((wchar_t *) &tbuf[2], (err - 2) / 2,
			UTF16_LITTLE_ENDIAN, buf, SIZE/2);
    buf[err] = 0;
    pr_info("string index %d is %s", index, buf);

    kfree(tbuf);
}

static int pen_probe(struct usb_interface *interface,
		     const struct usb_device_id *id)
{
    struct usb_device *device;

    device = interface_to_usbdev(interface);

    pr_info("Pen Probe control msg Function");

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
