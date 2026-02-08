#ifndef USB_H
#define USB_H

#include <stdint.h>
#include "driver.h"

 
struct usb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed));

struct usb_device {
    struct device dev;
    struct usb_device_descriptor desc;
    int address;
    int speed;
    struct xhci_host *host;  
};

struct usb_driver {
    struct device_driver driver;
    uint16_t idVendor;
    uint16_t idProduct;
    
    int (*probe)(struct usb_device *udev);
    void (*disconnect)(struct usb_device *udev);
};

void usb_core_init();
int usb_register_driver(struct usb_driver *driver);
int usb_new_device(struct usb_device *udev);

#endif
