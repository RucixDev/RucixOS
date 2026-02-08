#include "namespace.h"
#include "heap.h"
#include "string.h"
#include "console.h"

struct nsproxy init_nsproxy;
struct uts_namespace init_uts_ns;
struct pid_namespace init_pid_ns;
struct ipc_namespace init_ipc_ns;
struct mnt_namespace init_mnt_ns;

void namespaces_init() {    
    strcpy(init_uts_ns.nodename, "rucixos");
    strcpy(init_uts_ns.domainname, "(none)");
    init_uts_ns.ref_count = 1;

    init_pid_ns.last_pid = 0;
    init_pid_ns.parent = 0;
    init_pid_ns.ref_count = 1;
    
    INIT_LIST_HEAD(&init_ipc_ns.shm_ids);
    INIT_LIST_HEAD(&init_ipc_ns.msg_ids);
    INIT_LIST_HEAD(&init_ipc_ns.sem_ids);
    init_ipc_ns.ref_count = 1;
    
    INIT_LIST_HEAD(&init_mnt_ns.list);
    init_mnt_ns.root = 0;  
    init_mnt_ns.ref_count = 1;

    init_nsproxy.uts_ns = &init_uts_ns;
    init_nsproxy.pid_ns = &init_pid_ns;
    init_nsproxy.ipc_ns = &init_ipc_ns;
    init_nsproxy.mnt_ns = &init_mnt_ns;
    init_nsproxy.net_ns = 0;  
    init_nsproxy.ref_count = 1;
    
    kprint_str("Namespaces Initialized\n");
}

struct nsproxy *create_nsproxy() {
    struct nsproxy *ns = (struct nsproxy*)kmalloc(sizeof(struct nsproxy));
    if (!ns) return 0;
    memset(ns, 0, sizeof(struct nsproxy));
    ns->ref_count = 1;
    return ns;
}

struct nsproxy *copy_nsproxy(struct nsproxy *orig, unsigned long flags) {
    struct nsproxy *ns = create_nsproxy();
    if (!ns) return 0;
    
    if (flags & CLONE_NEWUTS) {
        ns->uts_ns = (struct uts_namespace*)kmalloc(sizeof(struct uts_namespace));
        memcpy(ns->uts_ns, orig->uts_ns, sizeof(struct uts_namespace));
        ns->uts_ns->ref_count = 1;
    } else {
        ns->uts_ns = orig->uts_ns;
        ns->uts_ns->ref_count++;
    }
    
    if (flags & CLONE_NEWPID) {
        ns->pid_ns = (struct pid_namespace*)kmalloc(sizeof(struct pid_namespace));
        ns->pid_ns->last_pid = 0;
        ns->pid_ns->parent = orig->pid_ns;
        ns->pid_ns->ref_count = 1;
    } else {
        ns->pid_ns = orig->pid_ns;
        ns->pid_ns->ref_count++;
    }
    
    if (flags & CLONE_NEWIPC) {
        ns->ipc_ns = (struct ipc_namespace*)kmalloc(sizeof(struct ipc_namespace));
        INIT_LIST_HEAD(&ns->ipc_ns->shm_ids);
        INIT_LIST_HEAD(&ns->ipc_ns->msg_ids);
        INIT_LIST_HEAD(&ns->ipc_ns->sem_ids);
        ns->ipc_ns->ref_count = 1;
    } else {
        ns->ipc_ns = orig->ipc_ns;
        ns->ipc_ns->ref_count++;
    }
    
    if (flags & CLONE_NEWNS) {
        ns->mnt_ns = (struct mnt_namespace*)kmalloc(sizeof(struct mnt_namespace));
         
        ns->mnt_ns->root = orig->mnt_ns->root;  
        INIT_LIST_HEAD(&ns->mnt_ns->list);
        ns->mnt_ns->ref_count = 1;
    } else {
        ns->mnt_ns = orig->mnt_ns;
        ns->mnt_ns->ref_count++;
    }

    return ns;
}
