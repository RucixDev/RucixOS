#include "drivers/dma.h"
#include "pmm.h"
#include "console.h"

void* dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle) {
    (void)dev;
    uint64_t pages = PAGE_ALIGN(size) / PAGE_SIZE;
    
    void *addr = pmm_alloc_pages(pages);
    if (!addr) return 0;
    
    if (dma_handle) *dma_handle = (dma_addr_t)addr; 

    return addr;
}

void dma_free_coherent(struct device *dev, size_t size, void *vaddr, dma_addr_t dma_handle) {
    (void)dev;
    (void)dma_handle;
    
    uint64_t pages = PAGE_ALIGN(size) / PAGE_SIZE;
    pmm_free_pages(vaddr, pages);
}

dma_addr_t dma_map_single(struct device *dev, void *ptr, size_t size, enum dma_data_direction dir) {
    (void)dev;
    (void)size;
    (void)dir;
    return (dma_addr_t)ptr;
}

void dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir) {
    (void)dev;
    (void)addr;
    (void)size;
    (void)dir;
}
