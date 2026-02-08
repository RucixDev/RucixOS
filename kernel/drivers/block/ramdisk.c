#include "drivers/ramdisk.h"
#include "drivers/blockdev.h"
#include "heap.h"
#include "string.h"
#include "console.h"

#define RAMDISK_SIZE (1 * 1024 * 1024) 
#define RAMDISK_SECTOR_SIZE 512
#define RAMDISK_MAJOR 10

static void ramdisk_make_request(struct request_queue *q, struct bio *bio) {
    (void)q;
    
    struct gendisk *disk = bio->disk;
    if (!disk) {
        if (bio->end_io) bio->end_io(bio);
        return;
    }

    char *ramdisk_data = (char*)disk->private_data;
    uint64_t disk_size = disk->capacity * RAMDISK_SECTOR_SIZE;

    uint64_t offset = bio->sector * RAMDISK_SECTOR_SIZE;
    uint64_t size = bio->size;
    
    if (offset + size > disk_size) {
        kprint_str("[Ramdisk] Error: Out of bounds\n");
         
        if (bio->end_io) bio->end_io(bio);
        return;
    }
    
    char *addr = ramdisk_data + offset;
    struct bio_vec *bv;
    int i;
    
    for (i = 0; i < bio->vc_cnt; i++) {
        bv = &bio->io_vec[i];
        void *buffer = (void*)((char*)bv->page + bv->offset);
        
        if (bio->rw == WRITE) {
            memcpy(addr, buffer, bv->len);
        } else {
            memcpy(buffer, addr, bv->len);
        }
        
        addr += bv->len;
    }
    
    if (bio->end_io) bio->end_io(bio);
}

struct gendisk *create_ramdisk(int minor, int size) {
    char *data = (char*)kmalloc(size);
    if (!data) return 0;
    memset(data, 0, size);
    
    struct gendisk *disk = alloc_disk(1);
    if (!disk) {
        kfree(data);
        return 0;
    }
    
    struct request_queue *q = blk_init_queue(NULL, NULL); 
    if (!q) {
        kfree(disk);
        kfree(data);
        return 0;
    }
    blk_queue_make_request(q, ramdisk_make_request);
    
    disk->major = RAMDISK_MAJOR;
    disk->first_minor = minor;
    disk->fops = NULL;
    disk->queue = q;
    disk->private_data = data;
    disk->capacity = size / RAMDISK_SECTOR_SIZE;
    
    const char *prefix = "ramdisk";
    int i=0;
    while(prefix[i]) { disk->disk_name[i] = prefix[i]; i++; }
    if (minor < 10) disk->disk_name[i++] = '0' + minor;
    disk->disk_name[i] = 0;
    
    return disk;
}

void ramdisk_init(void) {
    if (register_blkdev(RAMDISK_MAJOR, "ramdisk") < 0) {
        kprint_str("[Ramdisk] Failed to register blkdev\n");
        return;
    }

    struct gendisk *disk = create_ramdisk(0, RAMDISK_SIZE);
    if (disk) {
        add_disk(disk);
        kprint_str("[Ramdisk] Initialized 1MB Disk\n");
    }
}
