#include "process.h"
#include "vfs.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "console.h"
#include "heap.h"
#include "idt.h"

 
struct elf_header {
    char magic[4];
    uint8_t class;
    uint8_t data;
    uint8_t version;
    uint8_t osabi;
    uint8_t abiversion;
    uint8_t pad[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

 
struct elf_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

#define PT_LOAD 1

 
int process_exec(const char* path, const char** argv) {
    kprint_str("Exec: ");
    kprint_str(path);
    kprint_newline();

    int fd = vfs_open(path, 0, 0);  
    if (fd < 0) {
        kprint_str("Exec: File not found\n");
        return -1;
    }

    struct elf_header header;
    if (vfs_read(fd, (char*)&header, sizeof(header)) != sizeof(header)) {
        vfs_close(fd);
        return -1;
    }

    if (header.magic[0] != 0x7F || header.magic[1] != 'E' || 
        header.magic[2] != 'L' || header.magic[3] != 'F') {
        kprint_str("Exec: Invalid ELF magic\n");
        vfs_close(fd);
        return -1;
    }

    struct elf_phdr ph;
    for (int i = 0; i < header.phnum; i++) {
        vfs_lseek(fd, header.phoff + i * header.phentsize, SEEK_SET);
        if (vfs_read(fd, (char*)&ph, sizeof(ph)) != sizeof(ph)) {
            vfs_close(fd);
            return -1;
        }

        if (ph.p_type == PT_LOAD) {
             
            uint64_t vaddr_start = ph.p_vaddr & ~0xFFF;
            uint64_t vaddr_end = (ph.p_vaddr + ph.p_memsz + 0xFFF) & ~0xFFF;
            
            for (uint64_t v = vaddr_start; v < vaddr_end; v += 4096) {
                 if (vmm_get_phys(v) == 0) {
                     void *page = pmm_alloc_page();
                     if (!page) {
                         vfs_close(fd);
                         return -1;
                     }
                     vmm_map_page(v, (uint64_t)page, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
                     memset((void*)v, 0, 4096);
                 }
            }
            
            vfs_lseek(fd, ph.p_offset, SEEK_SET);
             
            if (vfs_read(fd, (char*)ph.p_vaddr, ph.p_filesz) != (int)ph.p_filesz) {
                 
            }
            
            if (ph.p_memsz > ph.p_filesz) {
                memset((void*)(ph.p_vaddr + ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
            }
        }
    }
    
    vfs_close(fd);

    uint64_t stack_top = 0x7FFFFFFFF000;
    uint64_t stack_base = stack_top - 4 * 4096;  
    
    for (uint64_t v = stack_base; v < stack_top; v += 4096) {
         void *page = pmm_alloc_page();
         if (!page) return -1;
         vmm_map_page(v, (uint64_t)page, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
         memset((void*)v, 0, 4096);
    }

    int argc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }
    
    uint64_t sp = stack_top;
    
     
    uint64_t *argv_ptrs = (uint64_t*)kmalloc((argc + 1) * sizeof(uint64_t));
    if (!argv_ptrs) return -1;
    
    if (argv) {
        for (int i = argc - 1; i >= 0; i--) {
            int len = strlen(argv[i]) + 1;
            sp -= len;
            strcpy((char*)sp, argv[i]);
            argv_ptrs[i] = sp;
        }
    }
    argv_ptrs[argc] = 0;
    
    sp &= ~0xF;

    sp -= sizeof(uint64_t);
    *(uint64_t*)sp = 0;
    
    sp -= (argc + 1) * sizeof(uint64_t);
    memcpy((void*)sp, argv_ptrs, (argc + 1) * sizeof(uint64_t));
    uint64_t argv_base = sp;
    
    kfree(argv_ptrs);
    
    sp -= sizeof(uint64_t);
    *(uint64_t*)sp = argc;
    
    strncpy(current_process->name, path, 63);
    current_process->name[63] = 0;
    
    struct interrupt_frame *tf = current_process->tf;
    memset(tf, 0, sizeof(struct interrupt_frame));
    
    tf->rip = header.entry;
    tf->cs = 0x1B;  
    tf->rflags = 0x202;  
    tf->rsp = sp;  
    tf->ss = 0x23;  
    
    tf->rdi = argc;
    tf->rsi = argv_base;
    tf->rdx = 0;  
    
    kprint_str("Exec: Success, jumping to user mode\n");
    
    return 0;
}
