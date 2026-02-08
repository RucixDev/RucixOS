#include "chardev.h"
#include "string.h"
#include "console.h"

static char_device_t chrdevs[MAX_CHAR_DEVICES];

void chardev_init(void) {
    memset(chrdevs, 0, sizeof(chrdevs));
    kprint_str("[CharDev] Initialized\n");
}

int register_chrdev(int major, const char *name, struct file_operations *fops) {
    if (major < 0 || major >= MAX_CHAR_DEVICES) return -1;
    if (chrdevs[major].fops != 0) return -1;

    int i = 0;
    while(name[i] && i < 31) {
        chrdevs[major].name[i] = name[i];
        i++;
    }
    chrdevs[major].name[i] = 0;
    
    chrdevs[major].major = major;
    chrdevs[major].fops = fops;
    
    kprint_str("[CharDev] Registered: ");
    kprint_str(name);
    kprint_newline();
    return 0;
}

int unregister_chrdev(int major, const char *name) {
    if (major < 0 || major >= MAX_CHAR_DEVICES) return -1;
    
    if (name && strcmp(chrdevs[major].name, name) != 0) {
        kprint_str("[CharDev] Name mismatch in unregister\n");
        return -1;
    }

    chrdevs[major].fops = 0;
    return 0;
}

struct file_operations* chardev_get_fops(int major) {
    if (major < 0 || major >= MAX_CHAR_DEVICES) return 0;
    return chrdevs[major].fops;
}
