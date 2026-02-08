#include "drivers/blockdev.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "spinlock.h"
#include "vfs.h"

#define MAX_BLKDEV 255

static char *blkdev_names[MAX_BLKDEV];
static LIST_HEAD(gendisk_head);
static spinlock_t gendisk_lock;

int register_blkdev(unsigned int major, const char *name) {
    if (major >= MAX_BLKDEV) return -1;
    if (blkdev_names[major]) return -1; 
    
    int len = 0;
    while(name[len]) len++;
    
    char *n = (char*)kmalloc(len+1);
    if (!n) return -1;
    for(int i=0; i<=len; i++) n[i] = name[i];
    
    blkdev_names[major] = n;
    
    kprint_str("[Block] Registered: ");
    kprint_str(name);
    kprint_newline();
    return 0;
}

void unregister_blkdev(unsigned int major, const char *name) {
    (void)name;
    if (major < MAX_BLKDEV && blkdev_names[major]) {
        kfree(blkdev_names[major]);
        blkdev_names[major] = 0;
    }
}

struct gendisk *alloc_disk(int minors) {
    struct gendisk *disk = (struct gendisk*)kmalloc(sizeof(struct gendisk));
    if (!disk) return 0;
    memset(disk, 0, sizeof(struct gendisk));
    disk->minors = minors;
    return disk;
}

void add_disk(struct gendisk *disk) {
    kprint_str("[Block] Added disk: ");
    kprint_str(disk->disk_name);
    kprint_str(" Capacity: ");
    kprint_hex(disk->capacity);
    kprint_newline();
    
    spinlock_acquire(&gendisk_lock);
    list_add_tail(&disk->list, &gendisk_head);
    spinlock_release(&gendisk_lock);  
}

void blockdev_register_devices(void) {
    struct list_head *pos;
    char path[64];

    vfs_mkdir("/dev", 0755);
    
    spinlock_acquire(&gendisk_lock);
    list_for_each(pos, &gendisk_head) {
        struct gendisk *disk = list_entry(pos, struct gendisk, list);

        const char *prefix = "/dev/";
        int i=0;
        while(prefix[i]) { path[i] = prefix[i]; i++; }
        int j=0;
        while(disk->disk_name[j]) { path[i++] = disk->disk_name[j++]; }
        path[i] = 0;

        int dev = (disk->major << 8) | disk->first_minor;

        vfs_mknod(path, S_IFBLK | 0600, dev);
        
        kprint_str("[Block] Registered device node: ");
        kprint_str(path);
        kprint_newline();
    }
    spinlock_release(&gendisk_lock);
}

struct gendisk *get_gendisk(const char *name) {
    struct gendisk *disk = 0;
    struct list_head *pos;
    
    spinlock_acquire(&gendisk_lock);
    list_for_each(pos, &gendisk_head) {
        struct gendisk *d = list_entry(pos, struct gendisk, list);
        if (strcmp(d->disk_name, name) == 0) {
            disk = d;
            break;
        }
    }
    spinlock_release(&gendisk_lock);
    return disk;
}

void del_gendisk(struct gendisk *disk) {
    if (disk) {
        spinlock_acquire(&gendisk_lock);
        list_del(&disk->list);
        spinlock_release(&gendisk_lock);
        kfree(disk);
    }
}

static struct request *blk_get_request(struct request_queue *q, int flags) {
    (void)q; (void)flags;
    struct request *rq = (struct request*)kmalloc(sizeof(struct request));
    if (!rq) return 0;
    memset(rq, 0, sizeof(struct request));
    INIT_LIST_HEAD(&rq->queuelist);
    rq->q = q;
    return rq;
}

static void __make_request(struct request_queue *q, struct bio *bio) {
    struct request *req;

    spinlock_acquire(&q->lock);

    if (elv_merge_bio(q, bio)) {
        spinlock_release(&q->lock);
        return;
    }
    
     
    req = blk_get_request(q, bio->flags);
    if (!req) {
        spinlock_release(&q->lock);
        if (bio->end_io) bio->end_io(bio);  
        return;
    }

    req->bio = bio;
    req->buffer = bio->io_vec ? bio->io_vec[0].page : 0;  
    req->sector = bio->sector;
    req->nr_sectors = bio->size / 512;  
    req->flags = bio->rw;  

     
    __elv_add_request(q, req, 0);

    spinlock_release(&q->lock);

    if (q->request_fn) {
        q->request_fn(q);
    }
}

 

struct request_queue *blk_init_queue(request_fn_proc rfn, spinlock_t *lock) {
    (void)lock;
    struct request_queue *q = (struct request_queue*)kmalloc(sizeof(struct request_queue));
    if (!q) return 0;
    memset(q, 0, sizeof(struct request_queue));
    
    q->request_fn = rfn;
    spinlock_init(&q->lock);
    INIT_LIST_HEAD(&q->queue_head);

    if (elevator_init(q, "noop") != 0) {
        kprint_str("[Block] Failed to init elevator\n");
    }

    q->make_request_fn = __make_request;
    
    return q;
}

void blk_queue_make_request(struct request_queue *q, make_request_fn mfn) {
    q->make_request_fn = mfn;
}

void submit_bio(struct bio *bio) {
    if (!bio) return;
    
    struct gendisk *disk = bio->disk;
    if (!disk || !disk->queue) {
        kprint_str("[Block] Error: submit_bio with no disk/queue\n");
        if (bio->end_io) bio->end_io(bio);
        return;
    }
    
    struct request_queue *q = disk->queue;
    
    if (q->make_request_fn) {
        q->make_request_fn(q, bio);
    } else {
        kprint_str("[Block] Error: No make_request_fn\n");
        if (bio->end_io) bio->end_io(bio);
    }
}
