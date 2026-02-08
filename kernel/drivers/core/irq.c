#include "drivers/irq.h"
#include "console.h"
#include "heap.h"
#include "drivers/pic.h"
#include "process.h"
#include "waitqueue.h"

#define NR_IRQS 16

static struct irqaction *irq_desc[NR_IRQS] = {0};

static void irq_thread(void *data) {
    struct irqaction *action = (struct irqaction *)data;
    
    while (!action->should_stop) {
        sem_wait(&action->wake_sem);  
        
        if (action->should_stop) break;

        if (action->thread_fn) {
            action->thread_fn(0, action->dev_id);  
        }
    }
    
     
    kfree(action);
    process_exit(0);
}

int request_threaded_irq(unsigned int irq, irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags, const char *name, void *dev) {
    if (irq >= NR_IRQS) return -1;
    if (!handler && !thread_fn) return -1;

    struct irqaction *action = (struct irqaction *)kmalloc(sizeof(struct irqaction));
    if (!action) return -1;

    action->handler = handler;
    action->thread_fn = thread_fn;
    action->flags = flags;
    action->name = name;
    action->dev_id = dev;
    action->next = 0;
    action->should_stop = 0;
    
    if (thread_fn) {
        sem_init(&action->wake_sem, 0);
         
        action->thread = process_create_kthread(irq_thread, action);
        if (!action->thread) {
            kfree(action);
            return -1;
        }
    } else {
        action->thread = 0;
    }

    if (irq_desc[irq] == 0) {
        irq_desc[irq] = action;
    } else {
        if ((irq_desc[irq]->flags & IRQF_SHARED) && (flags & IRQF_SHARED)) {
            struct irqaction *curr = irq_desc[irq];
            while (curr->next) curr = curr->next;
            curr->next = action;
        } else {
            kprint_str("IRQ conflict for IRQ ");
            kprint_dec(irq);
            kprint_newline();
             
            kfree(action);
            return -1;
        }
    }

    return 0;
}

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev) {
    return request_threaded_irq(irq, handler, 0, flags, name, dev);
}

void free_irq(unsigned int irq, void *dev) {
    if (irq >= NR_IRQS) return;

    struct irqaction **curr = &irq_desc[irq];
    while (*curr) {
        if ((*curr)->dev_id == dev) {
            struct irqaction *to_free = *curr;
            *curr = (*curr)->next;
            
            if (to_free->thread) {
                 
                to_free->should_stop = 1;
                sem_post(&to_free->wake_sem);
                 
            } else {
                kfree(to_free);
            }
            return;
        }
        curr = &((*curr)->next);
    }
}

void generic_handle_irq(unsigned int irq) {
    if (irq >= NR_IRQS) {
        kprint_str("Spurious IRQ: ");
        kprint_dec(irq);
        kprint_newline();
        return;
    }

    struct irqaction *action = irq_desc[irq];
    if (!action) {
        return;
    }

    while (action) {
        irqreturn_t ret = IRQ_NONE;
        
        if (action->handler) {
            ret = action->handler(irq, action->dev_id);
        } else {
             
            ret = IRQ_WAKE_THREAD;
        }
        
        if (ret == IRQ_WAKE_THREAD && action->thread) {
            sem_post(&action->wake_sem);
        }
        
        action = action->next;
    }
}

void irq_dump_stats() {
    kprint_str("IRQ Statistics:\n");
    for (int i = 0; i < NR_IRQS; i++) {
        struct irqaction *action = irq_desc[i];
        if (action) {
            kprint_str("IRQ ");
            kprint_dec(i);
            kprint_str(": ");
            while (action) {
                if (action->name) {
                    kprint_str(action->name);
                } else {
                    kprint_str("Unknown");
                }
                if (action->thread) {
                    kprint_str(" [Threaded]");
                }
                if (action->next) kprint_str(", ");
                action = action->next;
            }
            kprint_newline();
        }
    }
}
