#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "console.h"

 
#define PTE_PRESENT 1
#define PTE_WRITABLE 2
#define PTE_USER 4
#define PTE_NO_EXEC 0x8000000000000000

static uint64_t* current_pml4;

static void* phys_to_virt(uint64_t phys) {
    return (void*)phys;  
}

void vmm_init() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    current_pml4 = (uint64_t*)phys_to_virt(cr3);
    
    kprint_str("VMM: Initialized. CR3=");
    kprint_hex(cr3);
    kprint_newline();
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdp_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;
    
    uint64_t* pml4 = current_pml4;
    
    if (!(pml4[pml4_index] & PTE_PRESENT)) {
        uint64_t* pdp = pmm_alloc_page();
        memset(pdp, 0, PAGE_SIZE);
        pml4[pml4_index] = (uint64_t)pdp | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    
    uint64_t* pdp = (uint64_t*)phys_to_virt(pml4[pml4_index] & 0xFFFFFFFFFF000);
    
    if (!(pdp[pdp_index] & PTE_PRESENT)) {
        uint64_t* pd = pmm_alloc_page();
        memset(pd, 0, PAGE_SIZE);
        pdp[pdp_index] = (uint64_t)pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    uint64_t* pd = (uint64_t*)phys_to_virt(pdp[pdp_index] & 0xFFFFFFFFFF000);
    
    if (!(pd[pd_index] & PTE_PRESENT)) {
        uint64_t* pt = pmm_alloc_page();
        memset(pt, 0, PAGE_SIZE);
        pd[pd_index] = (uint64_t)pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    
    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_index] & 0xFFFFFFFFFF000);
    
    pt[pt_index] = phys | flags;
    
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void vmm_unmap_page(uint64_t virt) {
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdp_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;
    
    uint64_t* pml4 = current_pml4;
    
    if (!(pml4[pml4_index] & PTE_PRESENT)) return;
    uint64_t* pdp = (uint64_t*)phys_to_virt(pml4[pml4_index] & 0xFFFFFFFFFF000);
    
    if (!(pdp[pdp_index] & PTE_PRESENT)) return;
    uint64_t* pd = (uint64_t*)phys_to_virt(pdp[pdp_index] & 0xFFFFFFFFFF000);
    
    if (!(pd[pd_index] & PTE_PRESENT)) return;
    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_index] & 0xFFFFFFFFFF000);
    
    pt[pt_index] = 0;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdp_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;
    
    uint64_t* pml4 = current_pml4;
    
    if (!(pml4[pml4_index] & PTE_PRESENT)) return 0;
    uint64_t* pdp = (uint64_t*)phys_to_virt(pml4[pml4_index] & 0xFFFFFFFFFF000);
    
    if (!(pdp[pdp_index] & PTE_PRESENT)) return 0;
    uint64_t* pd = (uint64_t*)phys_to_virt(pdp[pdp_index] & 0xFFFFFFFFFF000);
    
    if (!(pd[pd_index] & PTE_PRESENT)) return 0;
    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_index] & 0xFFFFFFFFFF000);
    
    if (!(pt[pt_index] & PTE_PRESENT)) return 0;
    
    return (pt[pt_index] & 0xFFFFFFFFFF000) | (virt & 0xFFF);
}

uint64_t vmm_get_pte(uint64_t virt) {
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdp_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;
    
    uint64_t* pml4 = current_pml4;
    
    if (!(pml4[pml4_index] & PTE_PRESENT)) return 0;
    uint64_t* pdp = (uint64_t*)phys_to_virt(pml4[pml4_index] & 0xFFFFFFFFFF000);
    
    if (!(pdp[pdp_index] & PTE_PRESENT)) return 0;
    uint64_t* pd = (uint64_t*)phys_to_virt(pdp[pdp_index] & 0xFFFFFFFFFF000);
    
    if (!(pd[pd_index] & PTE_PRESENT)) return 0;
    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_index] & 0xFFFFFFFFFF000);
    
    return pt[pt_index];
}

void vmm_dump_stats() {
    kprint_str("VMM Stats (CR3): ");
    kprint_hex((uint64_t)current_pml4);
    kprint_newline();
}

void vmm_free_user_space() {
     
    for (int i = 0; i < 256; i++) {
        if (current_pml4[i] & PTE_PRESENT) {
            uint64_t* pdp = (uint64_t*)phys_to_virt(current_pml4[i] & 0xFFFFFFFFFF000);
            for (int j = 0; j < 512; j++) {
                if (pdp[j] & PTE_PRESENT) {
                    uint64_t* pd = (uint64_t*)phys_to_virt(pdp[j] & 0xFFFFFFFFFF000);
                     
                    for (int k = 0; k < 512; k++) {
                        if (pd[k] & PTE_PRESENT) {
                            if (pd[k] & 0x80) {  
                                pmm_free_page((void*)phys_to_virt(pd[k] & 0xFFFFFFFFFF000));
                            } else {
                                uint64_t* pt = (uint64_t*)phys_to_virt(pd[k] & 0xFFFFFFFFFF000);
                                for (int l = 0; l < 512; l++) {
                                    if (pt[l] & PTE_PRESENT) {
                                        pmm_free_page((void*)phys_to_virt(pt[l] & 0xFFFFFFFFFF000));
                                    }
                                }
                                pmm_free_page(pt);
                            }
                        }
                    }
                    pmm_free_page(pd);
                }
            }
            pmm_free_page(pdp);
            current_pml4[i] = 0;
        }
    }
     
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" :: "r"(cr3));
}

int vmm_swap_out_victim() {
    return -1;  
}

 
static uint64_t mmio_virt_base = 0xFFFF800000000000;

void *ioremap(uint64_t phys_addr, uint64_t size) {
    uint64_t offset = phys_addr & 0xFFF;
    uint64_t base_phys = phys_addr & ~0xFFF;
    uint64_t pages = (size + offset + 0xFFF) / 0x1000;
    
    uint64_t virt_addr = mmio_virt_base;
    mmio_virt_base += pages * 0x1000;
    
    for (uint64_t i = 0; i < pages; i++) {
        vmm_map_page(virt_addr + i * 0x1000, base_phys + i * 0x1000, PTE_PRESENT | PTE_WRITABLE);
    }
    
    return (void*)(virt_addr + offset);
}

void iounmap(void *virt_addr) {
    (void)virt_addr;
}
