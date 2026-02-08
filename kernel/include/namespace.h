#ifndef NAMESPACE_H
#define NAMESPACE_H

#include "types.h"
#include "list.h"
#include "spinlock.h"

 
#define CLONE_NEWNS   0x00020000  
#define CLONE_NEWUTS  0x04000000  
#define CLONE_NEWIPC  0x08000000  
#define CLONE_NEWUSER 0x10000000  
#define CLONE_NEWPID  0x20000000  
#define CLONE_NEWNET  0x40000000  

 
struct uts_namespace {
    char nodename[65];
    char domainname[65];
    int ref_count;
};

 
struct pid_namespace {
    uint64_t last_pid;
    struct pid_namespace *parent;
    int ref_count;
};

 
struct ipc_namespace {
    struct list_head shm_ids;
    struct list_head msg_ids;
    struct list_head sem_ids;
    int ref_count;
};

 
struct mnt_namespace {
    struct list_head list;  
    struct vfsmount *root;
    int ref_count;
};

 
struct net_namespace;

 
struct nsproxy {
    struct uts_namespace *uts_ns;
    struct ipc_namespace *ipc_ns;
    struct mnt_namespace *mnt_ns;
    struct pid_namespace *pid_ns;
    struct net_namespace *net_ns;
    int ref_count;
};

 
extern struct nsproxy init_nsproxy;

 
void namespaces_init();
struct nsproxy *create_nsproxy();
void free_nsproxy(struct nsproxy *ns);
struct nsproxy *copy_nsproxy(struct nsproxy *orig, unsigned long flags);

#endif
