#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include "driver.h"

 
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

 
struct pci_device {
    struct device dev;  
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    
    uint32_t bar[6];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
};

struct pci_driver {
    struct device_driver driver;  
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class;
    uint8_t subclass;
};

void pci_init(void);
int pci_enable_device(struct pci_device *dev);
int pci_register_driver(struct pci_driver *driver);
void pci_unregister_driver(struct pci_driver *driver);

uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);

#define PCI_COMMAND         0x04
#define PCI_COMMAND_MASTER  0x4
#define PCI_COMMAND_MEMORY  0x2

#define to_pci_device(d) container_of(d, struct pci_device, dev)
#define to_pci_driver(d) container_of(d, struct pci_driver, driver)

#endif
