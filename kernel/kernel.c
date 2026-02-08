#include <stdint.h>
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "console.h"
#include "multiboot.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "process.h"
#include "syscall.h"
#include "waitqueue.h"
#include "driver.h"
#include "drivers/pci.h"
#include "drivers/keyboard.h"
#include "drivers/serial.h"
#include "drivers/ramdisk.h"
#include "chardev.h"
#include "vfs.h"
#include "ramfs.h"
#include "string.h"
#include "fs/buffer.h"
#include "net/netdevice.h"
#include "net/skbuff.h"
#include "net/ethernet.h"
#include "net/arp.h"

#include "mm/swap.h"
#include "module.h"
#include "virt/vmx.h"
#include "hrtimer.h"

 
extern void jump_to_usermode(void* entry, void* stack);
extern void loopback_init(void);
extern void ahci_init(void);
extern void xhci_init(void);
extern void input_init(void);
extern void mouse_init(void);
extern void bga_init(void);
extern void ac97_init(void);
extern void e1000_init_driver(void);
extern void nvme_init(void);
extern void usb_core_init(void);
extern void usb_tablet_init(void);
extern void ath5k_init(void);
extern void arp_init(void);
extern void ip_init(void);
extern void icmp_init(void);
extern void udp_init(void);
extern void tcp_init(void);
extern void sock_init(void);

 
mutex_t print_mutex;
int shared_counter = 0;

void kernel_main(uint64_t multiboot_addr, uint64_t magic) {
     
    outb(0x3F8, 'K');

    kprint_init();
    kprint_str("Kernel Main Started...\n");
    
    kprint_str("Multiboot Info: Ptr=");
    kprint_hex(multiboot_addr);
    kprint_str(" Magic=");
    kprint_hex(magic);
    kprint_newline();

    gdt_init();

    idt_init();
    kprint_str("IDT Initialized.\n");

    pic_remap(0x20, 0x28);

    kprint_str("Starting PMM Init...\n");

    extern uint64_t _kernel_end;

    pmm_init(multiboot_addr, magic);
    kprint_str("PMM Init Done.\n");

    vmm_init();

    uint64_t heap_start = 0x100000000000;
     
    heap_init(heap_start, 8 * 1024 * 1024);  

    driver_core_init();
    chardev_init();
    keyboard_init();
    serial_driver_init();
    ramdisk_init();
    pci_init();

    net_dev_init();
    loopback_init();
    ahci_init();
    usb_core_init();
    xhci_init();
    usb_tablet_init();
    input_init();
    mouse_init();
    bga_init();
    ac97_init();
    e1000_init_driver();
    ath5k_init();
    nvme_init();
    arp_init();
    ip_init();
    icmp_init();
    udp_init();
    tcp_init();
    sock_init();

    kprint_str("Calling process_init()...\n");
    process_init();
    kprint_str("process_init() completed.\n");

    pit_init(100);  
    hrtimer_init_system();
    __asm__ volatile("sti");  

    kprint_str("Initializing VMX...\n");
    if (vmx_init() == 0) {
        kprint_str("VMX Initialized Successfully.\n");
    } else {
        kprint_str("VMX Initialization Failed (Not supported or disabled).\n");
    }

    mutex_init(&print_mutex);

    buffer_init();
    vfs_init();
    ramfs_init();

    struct super_block *sb = vfs_mount("ramfs", 0, "none", 0);
    if (sb) {
        kprint_str("[Kernel] Root FS mounted successfully\n");

        root_dentry = sb->s_root;
        
        root_mnt = (struct vfsmount*)kmalloc(sizeof(struct vfsmount));
        if (root_mnt) {
            memset(root_mnt, 0, sizeof(struct vfsmount));
            root_mnt->mnt_sb = sb;
            root_mnt->mnt_root = root_dentry;
            root_mnt->mnt_mountpoint = root_dentry;
            root_mnt->mnt_parent = root_mnt;
        }

        if (current_process) {
            current_process->cwd = root_dentry;
            current_process->cwd_mnt = root_mnt;
        }

        blockdev_register_devices();
        
    } else {
        kprint_str("[Kernel] Failed to mount Root FS\n");
    }
    
    module_subsystem_init();
    swap_init();

    kprint_str("Creating Tasks...\n");
    kprint_str("Starting Scheduler...\n");
    
    while (1) {
        process_yield();
        __asm__ volatile("hlt");
    }
}
