#ifndef PAGE_H
#define PAGE_H

#include "types.h"
#include "list.h"
#include "spinlock.h"

 
#define PG_locked       0
#define PG_error        1
#define PG_referenced   2
#define PG_uptodate     3
#define PG_dirty        4
#define PG_lru          5
#define PG_active       6
#define PG_slab         7

struct address_space;  

struct page {
    unsigned long flags;
    atomic_t _count;
    struct address_space *mapping;
    uint64_t index;  
    void *private;   
    
    struct list_head lru;  
    
    void *virtual;  
};

 
static inline void get_page(struct page *page) {
     
    page->_count++;
}

static inline void put_page(struct page *page) {
     
    page->_count--;
}

static inline int PageLocked(struct page *page) {
    return (page->flags & (1 << PG_locked));
}

static inline void SetPageLocked(struct page *page) {
    page->flags |= (1 << PG_locked);
}

static inline void ClearPageLocked(struct page *page) {
    page->flags &= ~(1 << PG_locked);
}

static inline int PageUptodate(struct page *page) {
    return (page->flags & (1 << PG_uptodate));
}

static inline void SetPageUptodate(struct page *page) {
    page->flags |= (1 << PG_uptodate);
}

static inline int PageDirty(struct page *page) {
    return (page->flags & (1 << PG_dirty));
}

static inline void SetPageDirty(struct page *page) {
    page->flags |= (1 << PG_dirty);
}

static inline void ClearPageDirty(struct page *page) {
    page->flags &= ~(1 << PG_dirty);
}

#endif
