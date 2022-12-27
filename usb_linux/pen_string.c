#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/uaccess.h>


static int pen_probe(struct usb_interface *interface,
		     const struct usb_device_id *id)
{
    struct usb_device *device;
    char buf[15];

    device = interface_to_usbdev(interface);

    pr_info("Pen Probe Function");
    usb_string(device, 1, buf, 15);
    pr_info("string 1 is %s", buf);

    usb_string(device, 2, buf, 15);
    pr_info("string 2 is %s", buf);

    usb_string(device, 3, buf, 15);
    pr_info("string 3 is %s", buf);

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
