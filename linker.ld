ENTRY(start)

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    note PT_NOTE FLAGS(4);
}

SECTIONS {
     
    . = 0x100000;

    .text :
    {
        *(.trampoline)
         
        KEEP(*(.multiboot_header))
        *(.text .text.*)
        *(.ltext .ltext.*)
        *(.rodata .rodata.*)
        *(.lrodata*)
    } :text

    .note : {
        *(.note .note.*)
    } :text :note

    .ksymtab : {
        __start___ksymtab = .;
        *(.__ksymtab)
        __stop___ksymtab = .;
    } :text

    .ksymtab_strings : {
        *(.__ksymtab_strings)
    } :text

    .data :
    {
        *(.data .data.*)
        *(.ldata .ldata.*)
    } :data

    .bss :
    {
        . = ALIGN(4096);
        __bss_start = .;
        *(.bss .bss.*)
        *(.lbss .lbss.*)
        *(COMMON)
        . = ALIGN(4096);
        __bss_end = .;
    } :data

    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.comment)
    }

    _kernel_end = .;
}
