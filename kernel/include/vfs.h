#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include "list.h"
#include "spinlock.h"
#include "mm/page.h"
#include "radix-tree.h"

#define MAX_FILES 128
#define MAX_PATH_LEN 4096
#define MAX_FILENAME_LEN 255

 
#define O_RDONLY 00000000
#define O_WRONLY 00000001
#define O_RDWR   00000002
#define O_CREAT  00000100
#define O_EXCL   00000200
#define O_TRUNC  00001000
#define O_APPEND 00002000
#define O_DIRECTORY 00200000
#define O_NOFOLLOW  00400000

 
#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#define S_ISCHR(mode)  (((mode) & S_IFMT) == S_IFCHR)
#define S_ISBLK(mode)  (((mode) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISLNK(mode)  (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

 
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct dirent {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

 
struct timespec {
    long tv_sec;
    long tv_nsec;
};

 
struct super_block;
struct inode;
struct dentry;
struct file;
struct vfsmount;
struct nameidata;
struct file_system_type;
struct gendisk;
struct address_space;
struct writeback_control;
struct page;
struct vm_area_struct;
struct kiocb;
struct iovec;
struct kstatfs;
struct iattr;
struct kstat {
    uint64_t ino;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    int64_t atime;
    int64_t mtime;
    int64_t ctime;
    uint64_t blocks;
    uint32_t blksize;
};

struct poll_table_struct;
struct file_lock;

struct qstr {
    const char *name;
    unsigned int len;
    unsigned int hash;
};

 

struct address_space_operations {
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    int (*readpage)(struct file *file, struct page *page);
    int (*writepages)(struct address_space *mapping, struct writeback_control *wbc);
    int (*set_page_dirty)(struct page *page);
    int (*readpages)(struct file *filp, struct address_space *mapping,
                    struct list_head *pages, unsigned nr_pages);
    int (*write_begin)(struct file *, struct address_space *mapping,
                      long long pos, unsigned len, unsigned flags,
                      struct page **pagep, void **fsdata);
    int (*write_end)(struct file *, struct address_space *mapping,
                    long long pos, unsigned len, unsigned copied,
                    struct page *page, void *fsdata);
    int (*bmap)(struct address_space *, long);
    void (*invalidatepage)(struct page *, unsigned long);
    int (*releasepage)(struct page *, int);
    int (*direct_IO)(int, struct kiocb *, const struct iovec *,
                    long long, unsigned long);
};

struct address_space {
    struct inode *host;
    struct radix_tree_root page_tree;
    spinlock_t lock;
    struct address_space_operations *a_ops;
    unsigned long flags;
    unsigned long nrpages;
    struct list_head i_mmap;     
};

struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *inode);
    void (*dirty_inode)(struct inode *inode);
    int (*write_inode)(struct inode *inode, int wait);
    void (*drop_inode)(struct inode *inode);
    void (*delete_inode)(struct inode *inode);
    void (*put_super)(struct super_block *sb);
    void (*write_super)(struct super_block *sb);
    int (*sync_fs)(struct super_block *sb, int wait);
    int (*freeze_fs)(struct super_block *sb);
    int (*unfreeze_fs)(struct super_block *sb);
    int (*statfs)(struct dentry *, struct kstatfs *);
    int (*remount_fs)(struct super_block *, int *, char *);
    void (*clear_inode)(struct inode *);
    void (*umount_begin)(struct super_block *);
};

struct inode_operations {
    int (*create)(struct inode *dir, struct dentry *dentry, int mode);
    struct dentry *(*lookup)(struct inode *dir, struct dentry *dentry);
    int (*link)(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry);
    int (*unlink)(struct inode *dir, struct dentry *dentry);
    int (*symlink)(struct inode *dir, struct dentry *dentry, const char *target);
    int (*mkdir)(struct inode *dir, struct dentry *dentry, int mode);
    int (*rmdir)(struct inode *dir, struct dentry *dentry);
    int (*mknod)(struct inode *dir, struct dentry *dentry, int mode, int dev);
    int (*rename)(struct inode *old_dir, struct dentry *old_dentry, 
                  struct inode *new_dir, struct dentry *new_dentry);
    int (*readlink)(struct dentry *dentry, char *buffer, int buflen);
    int (*permission)(struct inode *inode, int mask);
    int (*setattr)(struct dentry *dentry, struct iattr *attr);
    int (*getattr)(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat);
    int (*setxattr)(struct dentry *dentry, const char *name, const void *value, size_t size, int flags);
    size_t (*getxattr)(struct dentry *dentry, const char *name, void *value, size_t size);
    size_t (*listxattr)(struct dentry *dentry, char *list, size_t size);
    int (*removexattr)(struct dentry *dentry, const char *name);
};

struct file_operations {
    long long (*llseek)(struct file *, long long, int);
    int (*read)(struct file *file, char *buf, int count, uint64_t *offset);
    int (*write)(struct file *file, const char *buf, int count, uint64_t *offset);
    int (*readdir)(struct file *file, void *dirent, int count);
    unsigned int (*poll)(struct file *, struct poll_table_struct *);
    int (*ioctl)(struct inode *, struct file *, unsigned int, unsigned long);
    int (*mmap)(struct file *file, struct vm_area_struct *vma);
    int (*open)(struct inode *inode, struct file *file);
    int (*flush)(struct file *file);
    int (*release)(struct inode *inode, struct file *file);
    int (*fsync)(struct file *file, struct dentry *dentry, int datasync);
    int (*fasync)(int, struct file *, int);
    int (*lock)(struct file *, int, struct file_lock *);
};

struct dentry_operations {
    int (*d_revalidate)(struct dentry *, struct nameidata *);
    int (*d_hash)(struct dentry *, struct qstr *);
    int (*d_compare)(struct dentry *, struct qstr *, struct qstr *);
    int (*d_delete)(struct dentry *);
    void (*d_release)(struct dentry *);
    void (*d_iput)(struct dentry *, struct inode *);
};

 

struct file_system_type {
    const char *name;
    int fs_flags;
    struct super_block *(*mount)(struct file_system_type *fs_type, int flags, 
                                 const char *dev_name, void *data);
    void (*kill_sb)(struct super_block *sb);
    struct file_system_type *next;
    struct list_head fs_supers;
};

struct super_block {
    struct list_head s_list;         
    unsigned long s_blocksize;
    unsigned char s_blocksize_bits;
    unsigned char s_dirt;
    uint64_t s_maxbytes;             
    struct file_system_type *s_type;
    struct super_operations *s_op;
    unsigned long s_flags;
    unsigned long s_magic;
    struct dentry *s_root;           
    struct list_head s_inodes;       
    struct list_head s_dirty;        
    void *s_fs_info;                 
    spinlock_t s_lock;
    int s_count;
    char s_id[32];                   
    uint8_t s_uuid[16];              
    struct gendisk *s_bdev;
};

struct inode {
    struct list_head i_hash;
    struct list_head i_list;         
    struct list_head i_sb_list;
    struct list_head i_dentry;       
    
    unsigned long i_ino;
    unsigned int i_mode;
    unsigned int i_nlink;
    unsigned int i_uid;
    unsigned int i_gid;
    uint64_t i_rdev;           
    uint64_t i_size;
    uint64_t i_blocks;
    unsigned long i_blkbits;
    
    struct timespec i_atime;
    struct timespec i_mtime;
    struct timespec i_ctime;
    
    unsigned int i_flags;
    struct super_block *i_sb;
    
    struct inode_operations *i_op;
    struct file_operations *i_fop;
    
    struct address_space *i_mapping;
    struct address_space i_data;
    
    void *i_private;  
    
    spinlock_t i_lock;
    unsigned long i_state;
    int i_count;
};

struct dentry {
    unsigned int d_count;
    unsigned int d_flags;
    struct inode *d_inode;           
    struct dentry *d_parent;         
    struct list_head d_hash;         
    struct list_head d_lru;          
    struct list_head d_subdirs;      
    struct list_head d_child;        
    struct super_block *d_sb;        
    
    struct qstr d_name;
    
    struct list_head d_alias;        
    
    struct dentry_operations *d_op;
    void *d_fsdata;                  
    spinlock_t d_lock;
};

struct vfsmount {
    struct list_head mnt_hash;
    struct vfsmount *mnt_parent;
    struct dentry *mnt_mountpoint;
    struct dentry *mnt_root;
    struct super_block *mnt_sb;
    struct list_head mnt_mounts;
    struct list_head mnt_child;
    int mnt_flags;
    const char *mnt_devname;
};

struct file {
    struct list_head f_list;
    struct dentry *f_dentry;
    struct vfsmount *f_vfsmnt;
    struct file_operations *f_op;
    unsigned int f_flags;
    unsigned int f_mode;
    int f_count;
    uint64_t f_pos;
    void *private_data;
    struct address_space *f_mapping;
};

void vfs_init(void);
int register_filesystem(struct file_system_type *fs);
int unregister_filesystem(struct file_system_type *fs);

struct super_block *vfs_mount(const char *fs_type, int flags, const char *dev_name, void *data);
int vfs_open(const char *path, int flags, int mode);
int vfs_close(int fd);
int vfs_read(int fd, char *buf, int count);
int vfs_write(int fd, const char *buf, int count);
int vfs_lseek(int fd, int offset, int whence);
int vfs_readdir(int fd, struct dirent *dirp, int count);
int vfs_mkdir(const char *path, int mode);
int vfs_chdir(const char *path);
int vfs_mknod(const char *path, int mode, int dev);
int vfs_stat(const char *path, void *stat_buf); 
int vfs_dup(int oldfd);
int vfs_dup2(int oldfd, int newfd);

int vfs_unlink(const char *path);
int vfs_rmdir(const char *path);
int vfs_rename(const char *oldpath, const char *newpath);
int vfs_getcwd(char *buf, int size);

struct dentry *vfs_lookup(const char *path);

struct dentry *alloc_dentry(struct dentry *parent, const char *name);
void d_add(struct dentry *entry, struct inode *inode);
void d_instantiate(struct dentry *entry, struct inode *inode);
struct dentry *d_lookup(struct dentry *parent, const char *name);
struct inode *new_inode(struct super_block *sb);

int generic_file_read(struct file *filp, char *buf, int count, uint64_t *ppos);
int generic_file_write(struct file *filp, const char *buf, int count, uint64_t *ppos);

struct inode *alloc_inode(struct super_block *sb);
struct dentry *alloc_dentry(struct dentry *parent, const char *name);

extern struct vfsmount *root_mnt;
extern struct dentry *root_dentry;

#endif
