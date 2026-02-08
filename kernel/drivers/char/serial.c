#include "drivers/serial.h"
#include "chardev.h"
#include "io.h"
#include "idt.h"
#include "string.h"
#include "console.h"
#include "drivers/irq.h"
#include "waitqueue.h"
#include "process.h"

#define COM1 0x3F8
#define SERIAL_BUFFER_SIZE 256

static int serial_initialized = 0;
static char serial_buffer[SERIAL_BUFFER_SIZE];
static volatile int serial_write_idx = 0;
static volatile int serial_read_idx = 0;
static wait_queue_t serial_wq;

static irqreturn_t serial_handler(int irq, void *dev_id) {
    (void)irq;
    (void)dev_id;
    if (inb(COM1 + 5) & 1) {
        char c = inb(COM1);
        int next_write = (serial_write_idx + 1) % SERIAL_BUFFER_SIZE;
        if (next_write != serial_read_idx) {
            serial_buffer[serial_write_idx] = c;
            serial_write_idx = next_write;
            wake_up(&serial_wq);
        }
    }
    return IRQ_HANDLED;
}

void serial_init() {
    if (serial_initialized) return;
    
    wait_queue_init(&serial_wq);
    
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    
    outb(COM1 + 1, 0x01);
    
    if (request_irq(4, serial_handler, 0, "serial", 0) != 0) {
        kprint_str("[Serial] Failed to register IRQ 4\n");
    }
    
    outb(0x21, inb(0x21) & ~(1 << 4));
    
    serial_initialized = 1;
    kprint_str("[Serial] COM1 Initialized\n");
}

static int serial_write(struct file *file, const char *buf, int count, uint64_t *offset) {
    (void)file;
    (void)offset;
    for (int i=0; i<count; i++) {
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, buf[i]);
    }
    return count;
}

static int serial_read(struct file *file, char *buf, int count, uint64_t *offset) {
    (void)file;
    (void)offset;
    int read = 0;
    while (read < count) {
        while (serial_read_idx == serial_write_idx) {
            if (read > 0) return read;
            sleep_on(&serial_wq);
            if (current_process && current_process->pending_signals) {
                return (read == 0) ? -4 : read;  
            }
        }
        char c = serial_buffer[serial_read_idx];
        serial_read_idx = (serial_read_idx + 1) % SERIAL_BUFFER_SIZE;
        buf[read++] = c;
    }
    return read;
}

static struct file_operations serial_fops = {
    .write = serial_write,
    .read = serial_read,
    .release = 0
};

void serial_driver_init() {
    serial_init();
    register_chrdev(2, "serial", &serial_fops);
}
