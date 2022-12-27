#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>

static int pen_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *udevice;
	struct usb_device_descriptor u_d_desc;
	struct usb_host_config *uconfig;
	struct usb_config_descriptor u_c_desc;
	/* interfaces in the active configuration */
	struct usb_interface *uinterface;
	/* Alt setting for the interfaces */
	struct usb_host_interface *ualtsetting;
	struct usb_interface_descriptor u_i_desc;
	struct usb_host_endpoint *uendpoint;
	struct usb_endpoint_descriptor u_e_desc;
	u8 i, j, k;

	udevice = interface_to_usbdev(interface);
	u_d_desc = udevice->descriptor;

	/* Device active configuration */
	uconfig = udevice->actconfig;
	u_c_desc  = uconfig->desc;

	printk(KERN_INFO "Pen drive (%04X:%04X) plugged\n", id->idVendor,
	       id->idProduct);

	for(i=0; i<u_c_desc.bNumInterfaces; i++) {
		uinterface =  udevice->actconfig->interface[i];

		for(j=0; j<uinterface->num_altsetting; j++){
			ualtsetting = &uinterface->altsetting[j];
			u_i_desc = ualtsetting->desc;

			for(k=0; k<u_i_desc.bNumEndpoints; k++) {
				uendpoint  = &ualtsetting->endpoint[k];
				u_e_desc = uendpoint->desc;
				printk("Endpoint address = %x\n", u_e_desc.bEndpointAddress);
			}
		}
	}

	return 0;
}

static void pen_disconnect(struct usb_interface *interface)
{
    printk(KERN_INFO "Pen drive removed\n");
}

static struct usb_device_id pen_table[] =
{
    { USB_DEVICE(0x058F, 0x6387) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, pen_table);

static struct usb_driver pen_driver =
{
    .name = "pen_driver",
    .id_table = pen_table,
    .probe = pen_probe,
    .disconnect = pen_disconnect,
};

module_usb_driver(pen_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email@sarika-pugs.com>");
MODULE_DESCRIPTION("USB Pen Registration Driver");
