#ifndef _DRIVERS_USB_USB_H
#define _DRIVERS_USB_USB_H

#include "types.h"
#include "list.h"

enum usb_speed {
    USB_SPEED_UNKNOWN = 0,
    USB_SPEED_LOW,      
    USB_SPEED_FULL,     
    USB_SPEED_HIGH,     
    USB_SPEED_SUPER,    
};

#define USB_DT_DEVICE           1
#define USB_DT_CONFIG           2
#define USB_DT_STRING           3
#define USB_DT_INTERFACE        4
#define USB_DT_ENDPOINT         5
#define USB_DT_DEVICE_QUALIFIER 6
#define USB_DT_OTHER_SPEED_CONF 7
#define USB_DT_INTERFACE_POWER  8
#define USB_DT_OTG              9
#define USB_DT_DEBUG            10
#define USB_DT_INTERFACE_ASSOC  11
#define USB_DT_BOS              15
#define USB_DT_DEVICE_CAP       16
#define USB_DT_SS_ENDPOINT_COMP 48

#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE     0x0A
#define USB_REQ_SET_INTERFACE     0x0B
#define USB_REQ_SYNCH_FRAME       0x0C

#define USB_RT_DIR_HOST_TO_DEV  (0 << 7)
#define USB_RT_DIR_DEV_TO_HOST  (1 << 7)
#define USB_RT_TYPE_STANDARD    (0 << 5)
#define USB_RT_TYPE_CLASS       (1 << 5)
#define USB_RT_TYPE_VENDOR      (2 << 5)
#define USB_RT_RECIP_DEVICE     0
#define USB_RT_RECIP_INTERFACE  1
#define USB_RT_RECIP_ENDPOINT   2
#define USB_RT_RECIP_OTHER      3

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

struct usb_config_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed));

struct usb_interface_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed));

struct usb_endpoint_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed));

struct usb_setup_packet {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed));

struct usb_device {
    int port;
    int speed;
    int slot_id;
    int address;
    
    struct usb_device_descriptor desc;
    
    void *controller;  
    
    struct list_head list;
};

 
void usb_init(void);
void usb_register_host(void *controller);
struct usb_device *usb_alloc_device(void *controller);

#endif
