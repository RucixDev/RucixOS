#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "console.h"
#include "string.h"
#include "spinlock.h"

extern uint64_t* current_pml4;

#define HEAP_BLOCK_SIZE sizeof(struct heap_block)

struct heap_block {
    size_t size;
    struct heap_block* next;
    struct heap_block* prev;
    int free;
    uint64_t magic;  
};

static struct heap_block* free_list = 0;
static spinlock_t heap_lock;

void heap_init(uint64_t start_virt, uint64_t size) {
     
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    spinlock_init(&heap_lock);

    kprint_str("Initializing Heap at ");
    kprint_hex(start_virt);
    kprint_str(" Size: ");
    kprint_dec(size);
    kprint_newline();

    for (uint64_t i = 0; i < pages; i++) {
        vmm_map_page(start_virt + i * PAGE_SIZE, (uint64_t)pmm_alloc_page(), PTE_PRESENT | PTE_WRITABLE);
    }

    free_list = (struct heap_block*)start_virt;
    free_list->size = size - HEAP_BLOCK_SIZE;
    free_list->next = 0;
    free_list->prev = 0;
    free_list->free = 1;
    free_list->magic = 0x12345678;
    kprint_str("Heap Initialized successfully.\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return 0;
    
    spinlock_acquire(&heap_lock);

    size_t aligned_size = (size + 7) & ~7;
    
    struct heap_block* curr = free_list;
    while (curr) {
        if (curr->free && curr->size >= aligned_size) {
             
                    if (curr->size > aligned_size + HEAP_BLOCK_SIZE + 16) {
                        struct heap_block* new_block = (struct heap_block*)((uint8_t*)curr + HEAP_BLOCK_SIZE + aligned_size);
                        new_block->size = curr->size - aligned_size - HEAP_BLOCK_SIZE;
                        new_block->next = curr->next;
                        new_block->prev = curr;
                        new_block->free = 1;
                        new_block->magic = 0x12345678;
                        
                        if (new_block->next) {
                            new_block->next->prev = new_block;
                        }
                        
                        curr->size = aligned_size;
                        curr->next = new_block;
                    }
            curr->free = 0;
            spinlock_release(&heap_lock);
            return (void*)((uint8_t*)curr + HEAP_BLOCK_SIZE);
        }
        curr = curr->next;
    }
    spinlock_release(&heap_lock);
    return 0;  
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    spinlock_acquire(&heap_lock);

    struct heap_block* block = (struct heap_block*)((uint8_t*)ptr - HEAP_BLOCK_SIZE);

    if (block->magic != 0x12345678) {
        kprint_str("Heap Corruption Detected! Addr: ");
        kprint_hex((uint64_t)ptr);
        kprint_str(" Magic: ");
        kprint_hex(block->magic);
        kprint_str("\n");
        spinlock_release(&heap_lock);
        return;
    }
    
    block->free = 1;
    
    if (block->next && block->next->free) {
        block->size += HEAP_BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    
    if (block->prev && block->prev->free) {
        block->prev->size += HEAP_BLOCK_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
         
    }
    
    spinlock_release(&heap_lock);
}

void heap_dump_stats() {
    spinlock_acquire(&heap_lock);
    kprint_str("Heap Statistics:\n");
    struct heap_block* curr = free_list;
    int count = 0;
    size_t total_free = 0;
    
    while (curr) {
        if (curr->free) {
            count++;
            total_free += curr->size;
        }
        curr = curr->next;
    }
    
    kprint_str("Free Blocks: ");
    kprint_dec(count);
    kprint_str("\nTotal Free Bytes: ");
    kprint_dec(total_free);
    kprint_newline();
    spinlock_release(&heap_lock);
}
