#include "idt.h"
#include "drivers/pic.h"
#include "process.h"
#include "syscall.h"
#include "console.h"
#include "mm/swap.h"

struct idt_entry idt[256];
struct idt_ptr idtr;

extern void* isr_stub_table[];

void idt_set_gate(uint8_t vector, void* handler, uint8_t type_attr, uint8_t ist) {
    uint64_t offset = (uint64_t)handler;
    idt[vector].offset_low = (uint16_t)(offset & 0xFFFF);
    idt[vector].selector = 0x08;
    idt[vector].ist = ist;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_middle = (uint16_t)((offset >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((offset >> 32) & 0xFFFFFFFF);
    idt[vector].zero = 0;
}

void idt_init() {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    for (int i = 0; i < 48; i++) {
        idt_set_gate(i, isr_stub_table[i], 0x8E, 0);
    }

    idt_set_gate(8, isr_stub_table[8], 0x8E, 1);

    idt_set_gate(128, isr_stub_table[128], 0xEE, 0);

    __asm__ volatile("lidt %0" : : "m"(idtr));
}

const char* exception_messages[] = {
    "Divide by Zero Error",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Security Exception",
    "Reserved"
};

extern void syscall_handler(struct interrupt_frame* frame);
extern void generic_handle_irq(unsigned int irq);

void exception_handler(struct interrupt_frame* frame) {
    if (frame->int_no == 128) {
        syscall_handler(frame);
        return;
    }

    if (frame->int_no >= 32 && frame->int_no < 48) {
        uint8_t irq = frame->int_no - 32;
        generic_handle_irq(irq);
        pic_send_eoi(irq);
        return;
    }

    if (frame->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        
         
        if (handle_swap_fault(cr2) == 0) {
            return;
        }
    }

    volatile uint16_t* vga_buffer = (volatile uint16_t*)0xB8000;
    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = (uint16_t)(' ' | 0x1F00);
    }

    kprint_str("\n\n");
    kprint_str("    ****************************************************************\n");
    kprint_str("    *                 RUCIXOS SYSTEM EXCEPTION                     *\n");
    kprint_str("    ****************************************************************\n\n");
    
    kprint_str("    A fatal exception has occurred and RucixOS has been halted.\n");
    kprint_str("    Please report this error to the developer.\n\n");

    kprint_str("    EXCEPTION: ");
    if (frame->int_no < 32) {
        kprint_str(exception_messages[frame->int_no]);
    } else {
        kprint_str("Unknown Interrupt");
    }
    kprint_str(" (Vector: 0x"); kprint_hex(frame->int_no); kprint_str(")\n");
    
    if (frame->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        kprint_str("    FAULT ADDR (CR2): "); kprint_hex(cr2); kprint_str("\n");
    } else if (frame->int_no == 13) {
        kprint_str("    ERROR CODE: "); kprint_hex(frame->err_code); kprint_str("\n");
    }
    kprint_newline();

    kprint_str("    --- CPU REGISTERS ---\n");
    kprint_str("    RAX: "); kprint_hex(frame->rax); kprint_str("  RBX: "); kprint_hex(frame->rbx); kprint_str("  RCX: "); kprint_hex(frame->rcx); kprint_newline();
    kprint_str("    RDX: "); kprint_hex(frame->rdx); kprint_str("  RSI: "); kprint_hex(frame->rsi); kprint_str("  RDI: "); kprint_hex(frame->rdi); kprint_newline();
    kprint_str("    RBP: "); kprint_hex(frame->rbp); kprint_str("  RSP: "); kprint_hex(frame->rsp); kprint_str("  RIP: "); kprint_hex(frame->rip); kprint_newline();
    kprint_str("    R8:  "); kprint_hex(frame->r8);  kprint_str("  R9:  "); kprint_hex(frame->r9);  kprint_str("  R10: "); kprint_hex(frame->r10); kprint_newline();
    kprint_str("    R11: "); kprint_hex(frame->r11); kprint_str("  R12: "); kprint_hex(frame->r12); kprint_str("  R13: "); kprint_hex(frame->r13); kprint_newline();
    kprint_str("    R14: "); kprint_hex(frame->r14); kprint_str("  R15: "); kprint_hex(frame->r15); kprint_str("  RFLAGS: "); kprint_hex(frame->rflags); kprint_newline();
    kprint_str("    CS:  "); kprint_hex(frame->cs);  kprint_str("  SS:  "); kprint_hex(frame->ss);  kprint_str("  ERR: "); kprint_hex(frame->err_code); kprint_newline();
    
    kprint_str("\n    System Halted. Press Reset to Reboot.\n");
    
    while(1) {
        __asm__ volatile("hlt");
    }
}
