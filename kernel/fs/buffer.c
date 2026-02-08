#include "fs/buffer.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "spinlock.h"

#define BH_HASH_BITS 10
#define BH_HASH_SIZE (1 << BH_HASH_BITS)
#define BH_HASH_MASK (BH_HASH_SIZE - 1)
#define MAX_BUFFER_CACHE 1024

static struct buffer_head *bh_hash[BH_HASH_SIZE];
static spinlock_t bh_hash_lock;
static int buffer_cache_count = 0;

void buffer_init(void) {
    spinlock_init(&bh_hash_lock);
    memset(bh_hash, 0, sizeof(bh_hash));
    kprint_str("[VFS] Buffer Cache Initialized\n");
}

static int hash(struct gendisk *bdev, uint64_t block) {
     
    uint64_t key = (uint64_t)bdev ^ block;
    return key & BH_HASH_MASK;
}

 
static struct buffer_head *__find_get_block(struct gendisk *bdev, uint64_t block, uint32_t size) {
    int idx = hash(bdev, block);
    
    spinlock_acquire(&bh_hash_lock);
    struct buffer_head *bh = bh_hash[idx];
    while (bh) {
        if (bh->b_bdev == bdev && bh->b_blocknr == block && bh->b_size == size) {
            bh->b_count++;  
            spinlock_release(&bh_hash_lock);
            return bh;
        }
        bh = bh->b_next_hash;
    }
    spinlock_release(&bh_hash_lock);
    return 0;
}

 
static struct buffer_head *alloc_buffer_head(struct gendisk *bdev, uint64_t block, uint32_t size) {
    struct buffer_head *bh = (struct buffer_head*)kmalloc(sizeof(struct buffer_head));
    if (!bh) return 0;
    
    memset(bh, 0, sizeof(struct buffer_head));
    bh->b_data = (char*)kmalloc(size);
    if (!bh->b_data) {
        kfree(bh);
        return 0;
    }
    memset(bh->b_data, 0, size);
    
    bh->b_bdev = bdev;
    bh->b_blocknr = block;
    bh->b_size = size;
    bh->b_count = 1;
    bh->b_state = 0;
    wait_queue_init(&bh->b_wait);
    
    buffer_cache_count++;
    return bh;
}

 
struct buffer_head *getblk(struct gendisk *bdev, uint64_t block, uint32_t size) {
    struct buffer_head *bh = __find_get_block(bdev, block, size);
    if (bh) return bh;
    
     
    bh = alloc_buffer_head(bdev, block, size);
    if (!bh) return 0;
    
     
    int idx = hash(bdev, block);
    spinlock_acquire(&bh_hash_lock);
    
     
    struct buffer_head *exist = bh_hash[idx];
    while (exist) {
        if (exist->b_bdev == bdev && exist->b_blocknr == block && exist->b_size == size) {
            exist->b_count++;
            spinlock_release(&bh_hash_lock);
            kfree(bh->b_data);
            kfree(bh);
            return exist;
        }
        exist = exist->b_next_hash;
    }
    
    bh->b_next_hash = bh_hash[idx];
    if (bh_hash[idx]) bh_hash[idx]->b_pprev_hash = &bh->b_next_hash;
    bh_hash[idx] = bh;
    bh->b_pprev_hash = &bh_hash[idx];
    
    spinlock_release(&bh_hash_lock);
    return bh;
}

 

void brelse(struct buffer_head *bh) {
    if (!bh) return;
    
    spinlock_acquire(&bh_hash_lock);
    bh->b_count--;
    if (bh->b_count == 0) {
        if (!buffer_dirty(bh) && buffer_cache_count > MAX_BUFFER_CACHE) {
             
            if (bh->b_next_hash) bh->b_next_hash->b_pprev_hash = bh->b_pprev_hash;
            if (bh->b_pprev_hash) *bh->b_pprev_hash = bh->b_next_hash;
            
            kfree(bh->b_data);
            kfree(bh);
            buffer_cache_count--;
        }
    }
    spinlock_release(&bh_hash_lock);
}

static void end_buffer_io_async(struct bio *bio) {
    struct buffer_head *bh = (struct buffer_head *)bio->private_data;
    
    if (bio->flags & 1) {  
        set_buffer_uptodate(bh);
    } else {
         
    }
    
    unlock_buffer(bh);
    if (bio->io_vec) {
        kfree(bio->io_vec);
    }
    kfree(bio);  
}

void ll_rw_block(int rw, int nr, struct buffer_head *bhs[]) {
    for (int i = 0; i < nr; i++) {
        struct buffer_head *bh = bhs[i];
        if (!bh) continue;
        
        if (rw == READ && buffer_uptodate(bh)) continue;
        if (rw == WRITE && !buffer_dirty(bh)) continue;
        
        lock_buffer(bh);
        
        struct bio *bio = (struct bio*)kmalloc(sizeof(struct bio));
        if (!bio) {
            unlock_buffer(bh);
            continue;
        }
        memset(bio, 0, sizeof(struct bio));
        
        bio->sector = bh->b_blocknr * (bh->b_size / 512);  
        bio->size = bh->b_size;
        bio->disk = bh->b_bdev;
        bio->rw = rw;
        bio->private_data = bh;
        bio->end_io = end_buffer_io_async;
        
        struct bio_vec *vec = (struct bio_vec*)kmalloc(sizeof(struct bio_vec));
        if (vec) {
             vec->page = bh->b_data;
             vec->len = bh->b_size;
             vec->offset = 0;
             bio->io_vec = vec;
             bio->vc_cnt = 1;
             bio->idx = 0;
        } else {
              
             kfree(bio);
             unlock_buffer(bh);
             continue;
        }
        
        submit_bio(bio);
    }
}

struct buffer_head *bread(struct gendisk *bdev, uint64_t block, uint32_t size) {
    struct buffer_head *bh = getblk(bdev, block, size);
    if (!bh) return 0;
    
    if (buffer_uptodate(bh)) return bh;
    
    ll_rw_block(READ, 1, &bh);
    
    wait_on_buffer(bh);
    
    if (buffer_uptodate(bh)) return bh;
    
    brelse(bh);
    return 0;
}

int mark_buffer_dirty(struct buffer_head *bh) {
    bh->b_state |= (1 << BH_Dirty);
    return 0;
}

int sync_dirty_buffer(struct buffer_head *bh) {
    if (buffer_dirty(bh)) {
        ll_rw_block(WRITE, 1, &bh);
         
        wait_on_buffer(bh);
        bh->b_state &= ~(1 << BH_Dirty);
        return 0;
    }
    return 0;
}

void lock_buffer(struct buffer_head *bh) {
    if (!bh) return;
    while (test_and_set_bit(BH_Locked, &bh->b_state)) {
        sleep_on(&bh->b_wait);
    }
}

void unlock_buffer(struct buffer_head *bh) {
    if (!bh) return;
    clear_bit(BH_Locked, &bh->b_state);
    wake_up(&bh->b_wait);
}

void wait_on_buffer(struct buffer_head *bh) {
    if (!bh) return;
    while (buffer_locked(bh)) {
        sleep_on(&bh->b_wait);
    }
}
