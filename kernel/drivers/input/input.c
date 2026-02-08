#include "drivers/input.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "spinlock.h"

static LIST_HEAD(input_dev_list);
static LIST_HEAD(input_handler_list);
static spinlock_t input_lock;

void input_init(void) {
    spinlock_init(&input_lock);
    kprint_str("Input Subsystem Initialized\n");
}

struct input_dev *input_allocate_device(void) {
    struct input_dev *dev = (struct input_dev*)kmalloc(sizeof(struct input_dev));
    if (!dev) return 0;
    memset(dev, 0, sizeof(struct input_dev));
    return dev;
}

void input_free_device(struct input_dev *dev) {
    kfree(dev);
}

int input_register_device(struct input_dev *dev) {
    if (!dev) return -1;
    
    spinlock_acquire(&input_lock);
    list_add_tail(&dev->list, &input_dev_list);
    spinlock_release(&input_lock);
    
    kprint_str("Input: Registered device ");
    if (dev->name) kprint_str(dev->name);
    kprint_newline();
    
    return 0;
}

void input_unregister_device(struct input_dev *dev) {
    if (!dev) return;
    
    spinlock_acquire(&input_lock);
    list_del(&dev->list);
    spinlock_release(&input_lock);
}

int input_register_handler(struct input_handler *handler) {
    if (!handler) return -1;
    
    spinlock_acquire(&input_lock);
    list_add_tail(&handler->list, &input_handler_list);
    spinlock_release(&input_lock);
    
    return 0;
}

void input_unregister_handler(struct input_handler *handler) {
    if (!handler) return;
    
    spinlock_acquire(&input_lock);
    list_del(&handler->list);
    spinlock_release(&input_lock);
}

void input_report_event(struct input_dev *dev, uint16_t type, uint16_t code, int32_t value) {
    struct input_event ev;
    ev.timestamp = 0;  
    ev.type = type;
    ev.code = code;
    ev.value = value;

    struct input_handler *handler;
    struct list_head *pos;

    list_for_each(pos, &input_handler_list) {
        handler = list_entry(pos, struct input_handler, list);
        if (handler->event) {
            handler->event(handler, dev, &ev);
        }
    }
     
}

void input_sync(struct input_dev *dev) {
    input_report_event(dev, EV_SYN, SYN_REPORT, 0);
}

void input_print_devices(void) {
    struct input_dev *dev;
    struct list_head *pos;
    
    spinlock_acquire(&input_lock);
    kprint_str("Input Devices:\n");
    list_for_each(pos, &input_dev_list) {
        dev = list_entry(pos, struct input_dev, list);
        kprint_str("  - Name: ");
        kprint_str(dev->name ? dev->name : "Unknown");
        kprint_str("\n    Caps: EV=");
        kprint_hex(dev->evbit[0]);
        kprint_newline();
    }
    spinlock_release(&input_lock);
}
