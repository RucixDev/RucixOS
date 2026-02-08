#ifndef DRIVERS_IRQ_H
#define DRIVERS_IRQ_H

#include <stdint.h>
#include "driver.h"
#include "waitqueue.h"

struct process;

#define IRQF_SHARED 0x00000080
#define IRQF_TRIGGER_RISING 0x00000001
#define IRQF_TRIGGER_FALLING 0x00000002
#define IRQF_TRIGGER_HIGH 0x00000004
#define IRQF_TRIGGER_LOW 0x00000008

enum irqreturn {
    IRQ_NONE = 0,
    IRQ_HANDLED = 1,
    IRQ_WAKE_THREAD = 2,
};
typedef enum irqreturn irqreturn_t;

typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);

struct irqaction {
    irq_handler_t handler;
    irq_handler_t thread_fn;  
    struct process *thread;   
    semaphore_t wake_sem;     
    int should_stop;          
    unsigned long flags;
    const char *name;
    void *dev_id;
    struct irqaction *next;
};

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
int request_threaded_irq(unsigned int irq, irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags, const char *name, void *dev);
void free_irq(unsigned int irq, void *dev);

void generic_handle_irq(unsigned int irq);
void irq_dump_stats();

#endif
