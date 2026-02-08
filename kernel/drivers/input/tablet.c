#include "drivers/usb.h"
#include "drivers/input.h"
#include "heap.h"
#include "console.h"

#define USB_VENDOR_QEMU 0x0627
#define USB_PRODUCT_TABLET 0x0001

struct usb_tablet {
    struct usb_device *udev;
    struct input_dev *input;
};

static int usb_tablet_probe(struct usb_device *udev) {
    kprint_str("USB Tablet: Probing...\n");
    
    struct usb_tablet *tablet = (struct usb_tablet*)kmalloc(sizeof(struct usb_tablet));
    tablet->udev = udev;
    
    tablet->input = input_allocate_device();
    tablet->input->name = "QEMU USB Tablet";
    
    input_register_device(tablet->input);
    
    kprint_str("USB Tablet: Registered Input Device\n");
    return 0;
}

static void usb_tablet_disconnect(struct usb_device *udev) {
    (void)udev;
    kprint_str("USB Tablet: Disconnected\n");
}

static struct usb_driver tablet_driver = {
    .idVendor = USB_VENDOR_QEMU,
    .idProduct = USB_PRODUCT_TABLET,
    .probe = usb_tablet_probe,
    .disconnect = usb_tablet_disconnect,
};

void usb_tablet_init() {
    usb_register_driver(&tablet_driver);
}
