#include "chardev.h"
#include "idt.h"
#include "io.h"
#include "types.h"
#include "console.h"
#include "string.h"
#include "waitqueue.h"
#include "drivers/irq.h"
#include "drivers/input.h"
#include "heap.h"

#include "process.h"
#include "signal.h"

#define KBD_BUFFER_SIZE 256
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_write_idx = 0;
static volatile int kbd_read_idx = 0;
static int shift_pressed = 0;
static int ctrl_pressed = 0;

static struct input_dev *kbd_input_dev;

static wait_queue_t kbd_wq;

#define EINTR 4

static char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static char kbd_us_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static irqreturn_t keyboard_handler(int irq, void *dev_id) {
    (void)irq;
    (void)dev_id;
    uint8_t scancode = inb(0x60);

     
    if (kbd_input_dev) {
         
         
        uint16_t keycode = scancode & 0x7F;
        int value = (scancode & 0x80) ? 0 : 1;
        input_report_event(kbd_input_dev, EV_KEY, keycode, value);
        input_sync(kbd_input_dev);
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return IRQ_HANDLED;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return IRQ_HANDLED;
    }
    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return IRQ_HANDLED;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        return IRQ_HANDLED;
    }

    if (scancode & 0x80) {
         
        return IRQ_HANDLED;
    }
    
    char c = 0;
    if (ctrl_pressed) {
         
         
        if (scancode == 0x2E) {
            c = 0x03;  
            if (current_process) {
                current_process->pending_signals |= (1 << SIGINT);
            }
        }
         
    } else {
        c = shift_pressed ? kbd_us_shift[scancode] : kbd_us[scancode];
    }

    if (c) {
        int next_write = (kbd_write_idx + 1) % KBD_BUFFER_SIZE;
        if (next_write != kbd_read_idx) {
            kbd_buffer[kbd_write_idx] = c;
            kbd_write_idx = next_write;
            wake_up(&kbd_wq);
        }
    }
    return IRQ_HANDLED;
}

static int kbd_read(struct file *file, char *buf, int count, uint64_t *offset) {
    (void)file;
    (void)offset;
    int read = 0;
    while (read < count) {
         
        while (kbd_read_idx == kbd_write_idx) {
             
            if (read > 0) return read;
            
             
            sleep_on(&kbd_wq);
            
             
            if (current_process && current_process->pending_signals) {
                return (read == 0) ? -EINTR : read;
            }
        }
        
        char c = kbd_buffer[kbd_read_idx];
        kbd_read_idx = (kbd_read_idx + 1) % KBD_BUFFER_SIZE;
        buf[read++] = c;
    }
    return read;
}

static struct file_operations kbd_fops = {
    .read = kbd_read,
    .write = 0,
    .release = 0
};

void keyboard_init() {
    wait_queue_init(&kbd_wq);

    if (request_irq(1, keyboard_handler, 0, "keyboard", 0) != 0) {
        kprint_str("[KBD] Failed to register IRQ 1\n");
    }
    
    register_chrdev(1, "keyboard", &kbd_fops);
    
    uint8_t mask = inb(0x21);

    mask &= ~(1 << 1);
    outb(0x21, mask);
    
    kprint_str("[KBD] Keyboard initialized (IRQ 1 enabled)\n");

     
    kbd_input_dev = (struct input_dev*)kmalloc(sizeof(struct input_dev));
    if (kbd_input_dev) {
        memset(kbd_input_dev, 0, sizeof(struct input_dev));
        kbd_input_dev->name = "AT Keyboard";
        
         
        kbd_input_dev->evbit[0] = (1 << EV_KEY);
        
         
        for (int i = 1; i < 128; i++) {
             kbd_input_dev->keybit[i / 64] |= (1ULL << (i % 64));
        }
        
        if (input_register_device(kbd_input_dev) != 0) {
            kprint_str("[KBD] Failed to register input device\n");
            kfree(kbd_input_dev);
            kbd_input_dev = 0;
        }
    }
}
