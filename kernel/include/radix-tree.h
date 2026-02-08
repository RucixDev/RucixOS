#ifndef RADIX_TREE_H
#define RADIX_TREE_H

#include <stdint.h>
#include "types.h"

#define RADIX_TREE_MAP_SHIFT  6
#define RADIX_TREE_MAP_SIZE   (1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK   (RADIX_TREE_MAP_SIZE - 1UL)

struct radix_tree_node {
    unsigned int    count;
    void            *slots[RADIX_TREE_MAP_SIZE];
};

struct radix_tree_root {
    unsigned int            height;
    struct radix_tree_node  *rnode;
};

#define RADIX_TREE_INIT(mask)   { .height = 0, .rnode = 0 }

void radix_tree_init(struct radix_tree_root *root);
int radix_tree_insert(struct radix_tree_root *root, unsigned long index, void *item);
void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index);
void *radix_tree_delete(struct radix_tree_root *root, unsigned long index);

#endif
