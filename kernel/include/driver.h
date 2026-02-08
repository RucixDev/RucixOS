#ifndef DRIVER_H
#define DRIVER_H

#include <stdint.h>
#include "list.h"

 
struct device;
struct device_driver;

 
struct bus_type {
    const char *name;
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    void (*remove)(struct device *dev);

    struct list_head bus_list;
    struct list_head dev_list;
    struct list_head drv_list;
};

 
struct device {
    char name[32];
    struct bus_type *bus;
    struct device_driver *driver;
    void *driver_data;
    
     
    uint64_t resource_start;
    uint64_t resource_end;
    uint32_t irq;

    struct list_head bus_list;
    struct list_head global_list;
};

 
struct device_driver {
    const char *name;
    struct bus_type *bus;

    int (*probe)(struct device *dev);
    void (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);
    
     
    int (*suspend)(struct device *dev);
    int (*resume)(struct device *dev);

    struct list_head bus_list;
    struct list_head global_list;
};

 
void driver_core_init(void);

int bus_register(struct bus_type *bus);
void bus_unregister(struct bus_type *bus);

int driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);

int device_register(struct device *dev);
void device_unregister(struct device *dev);

 
int device_suspend(struct device *dev);
int device_resume(struct device *dev);

void device_for_each(void (*fn)(struct device *));
void device_dump_all();

 
void bus_rescan_devices(struct bus_type *bus);

#endif  
