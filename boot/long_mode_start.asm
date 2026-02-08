bits 64
default rel

global long_mode_start
extern kernel_main
extern multiboot_info_ptr
extern multiboot_magic
extern __bss_start
extern __bss_end

section .text
long_mode_start:
    mov dx, 0x3f8
    mov al, 'A'
    out dx, al

    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor eax, eax
    rep stosb

    mov dx, 0x3f8
    mov al, 'B'
    out dx, al

    mov edi, [multiboot_info_ptr]

    mov esi, [multiboot_magic]

    call kernel_main
    hlt
