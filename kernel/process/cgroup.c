#include "cgroup.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "process.h"

struct css_set init_css_set;
struct cgroup root_cgroup;
struct cgroup_subsys *subsystems[CGROUP_SUBSYS_COUNT] = {0};

static spinlock_t cgroup_lock;

void cgroup_init() {
    spinlock_init(&cgroup_lock);
    
    memset(&root_cgroup, 0, sizeof(struct cgroup));
    strcpy(root_cgroup.name, "/");
    INIT_LIST_HEAD(&root_cgroup.children);
    INIT_LIST_HEAD(&root_cgroup.sibling);
    INIT_LIST_HEAD(&root_cgroup.pid_lists);
    root_cgroup.ref_count = 1;
    
    memset(&init_css_set, 0, sizeof(struct css_set));
    init_css_set.ref_count = 1;
    INIT_LIST_HEAD(&init_css_set.list);
    INIT_LIST_HEAD(&init_css_set.tasks);
    
    for (int i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
        root_cgroup.subsys[i] = 0;  
        init_css_set.subsys[i] = 0;
    }
    
    kprint_str("Cgroup Subsystem Initialized\n");
}

int cgroup_register_subsys(struct cgroup_subsys *ss) {
    spinlock_acquire(&cgroup_lock);
    
    int id = -1;
    for (int i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
        if (!subsystems[i]) {
            id = i;
            break;
        }
    }
    
    if (id == -1) {
        spinlock_release(&cgroup_lock);
        return -1;  
    }
    
    subsystems[id] = ss;
    ss->subsys_id = id;
    
    if (ss->create) {
        struct cgroup_subsys_state *css = ss->create(&root_cgroup);
        if (css) {
            root_cgroup.subsys[id] = css;
            init_css_set.subsys[id] = css;
            css->cgroup = &root_cgroup;
            css->ss = ss;
        }
    }
    
    spinlock_release(&cgroup_lock);
    kprint_str("Registered Cgroup Subsystem: ");
    kprint_str(ss->name);
    kprint_newline();
    return 0;
}

void cgroup_fork(struct process *child) {
    if (!current_process) {
         
        child->cgroups = &init_css_set;
        init_css_set.ref_count++;
        return;
    }
    
    spinlock_acquire(&cgroup_lock);
    child->cgroups = current_process->cgroups;
    child->cgroups->ref_count++;
    
    for (int i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
        struct cgroup_subsys *ss = subsystems[i];
        if (ss && ss->fork) {
            struct cgroup_subsys_state *css = child->cgroups->subsys[i];
            if (css) {
                ss->fork(css, child);
            }
        }
    }
    
    spinlock_release(&cgroup_lock);
}

void cgroup_exit(struct process *p) {
    if (!p->cgroups) return;
    
    spinlock_acquire(&cgroup_lock);
    
    for (int i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
        struct cgroup_subsys *ss = subsystems[i];
        if (ss && ss->exit) {
            struct cgroup_subsys_state *css = p->cgroups->subsys[i];
            if (css) {
                ss->exit(css, p);
            }
        }
    }
    
    p->cgroups->ref_count--;
    if (p->cgroups->ref_count <= 0) {
         
        if (p->cgroups != &init_css_set) {
            kfree(p->cgroups);
        }
    }
    p->cgroups = 0;
    
    spinlock_release(&cgroup_lock);
}
