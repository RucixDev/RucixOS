#include "drivers/usb.h"
#include "console.h"
#include "heap.h"
#include "string.h"

static struct usb_driver *usb_drivers[32];  
static int usb_driver_count = 0;

void usb_core_init() {
    kprint_str("USB: Core Initialized\n");
}

int usb_register_driver(struct usb_driver *driver) {
    if (usb_driver_count >= 32) return -1;
    usb_drivers[usb_driver_count++] = driver;
    kprint_str("USB: Registered driver ");
     
    kprint_newline();
    return 0;
}

int usb_new_device(struct usb_device *udev) {
    kprint_str("USB: New Device Addr=");
    kprint_dec(udev->address);
    kprint_str(" Vendor="); kprint_hex(udev->desc.idVendor);
    kprint_str(" Product="); kprint_hex(udev->desc.idProduct);
    kprint_newline();
    
     
    for (int i = 0; i < usb_driver_count; i++) {
        struct usb_driver *drv = usb_drivers[i];
        if (drv->idVendor == udev->desc.idVendor && 
            drv->idProduct == udev->desc.idProduct) {
            
            kprint_str("USB: Found driver for device\n");
            if (drv->probe) {
                drv->probe(udev);
            }
            return 0;
        }
    }
    
    kprint_str("USB: No driver found\n");
    return -1;
}
