#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

void heap_init(uint64_t start_virt, uint64_t size);
void* kmalloc(size_t size);
void kfree(void* ptr);
void heap_dump_stats();

#endif
