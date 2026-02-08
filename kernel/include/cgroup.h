#ifndef CGROUP_H
#define CGROUP_H

#include "types.h"
#include "list.h"
#include "spinlock.h"

#define CGROUP_NAME_LEN 64
#define CGROUP_SUBSYS_COUNT 12  

struct cgroup;
struct cgroup_subsys;
struct cgroup_subsys_state;
struct process;  

 
struct cgroup_subsys_state {
    struct cgroup *cgroup;
    struct cgroup_subsys *ss;
    int ref_count;
    unsigned long flags;
};

 
struct cgroup {
    char name[CGROUP_NAME_LEN];
    
    struct cgroup *parent;
    struct list_head children;  
    struct list_head sibling;   
    
     
    struct cgroup_subsys_state *subsys[CGROUP_SUBSYS_COUNT];
    
    struct list_head pid_lists;  
    
    struct dentry *dentry;  
    
    int ref_count;
};

 
struct css_set {
    struct cgroup_subsys_state *subsys[CGROUP_SUBSYS_COUNT];
    int ref_count;
    struct list_head list;  
    struct list_head tasks;  
};

 
struct cgroup_subsys {
    const char *name;
    int subsys_id;
    
     
    struct cgroup_subsys_state *(*create)(struct cgroup *cgrp);
    void (*destroy)(struct cgroup *cgrp, struct cgroup_subsys_state *css);
    int (*attach)(struct cgroup *cgrp, struct cgroup_subsys_state *css, struct process *task);
    void (*fork)(struct cgroup_subsys_state *css, struct process *task);
    void (*exit)(struct cgroup_subsys_state *css, struct process *task);
};

 
extern struct css_set init_css_set;
extern struct cgroup root_cgroup;

 
void cgroup_init();
void cgroup_fork(struct process *child);
void cgroup_exit(struct process *p);
int cgroup_attach(struct cgroup *cgrp, struct process *p);

 
int cgroup_register_subsys(struct cgroup_subsys *ss);

#endif
