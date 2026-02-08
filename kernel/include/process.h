#ifndef PROCESS_H
#define PROCESS_H

#include "vmm.h"
#include "vfs.h"
#include "types.h"
#include "spinlock.h"
#include "drivers/irq.h"
#include "namespace.h"
#include "cgroup.h"
#include "seccomp.h"

 
#define PROCESS_STATE_READY 0
#define PROCESS_STATE_RUNNING 1
#define PROCESS_STATE_BLOCKED 2
#define PROCESS_STATE_TERMINATED 3
#define PROCESS_STATE_SLEEPING 4

 
#define SCHED_OTHER 0  
#define SCHED_FIFO  1  
#define SCHED_RR    2  
#define SCHED_CFS   3  

 
#define PRIO_MAX_RT 99
#define PRIO_MIN_RT 0
#define PRIO_DEFAULT 20
#define MLFQ_LEVELS 4

 
#define RLIMIT_CPU 0
#define RLIMIT_FSIZE 1
#define RLIMIT_DATA 2
#define RLIMIT_STACK 3
#define RLIMIT_CORE 4
#define RLIMIT_RSS 5
#define RLIMIT_NPROC 6
#define RLIMIT_NOFILE 7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS 9
#define RLIMIT_NLIMITS 10

#define RLIM_INFINITY ((uint64_t)-1)

struct rlimit {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

 
struct process;
struct interrupt_frame;

 
struct sched_class {
    const struct sched_class *next;
    void (*enqueue_task)(struct process *p);
    void (*dequeue_task)(struct process *p);
    struct process *(*pick_next_task)(struct process *prev);
    void (*task_tick)(struct process *p);
};

struct process {
     
    uint64_t pid;
    uint64_t rsp;
    uint64_t kernel_stack;
    uint64_t cr3;
    int state;
    struct interrupt_frame *tf;
    char name[64];
    void *thread_arg;  
    
     
    int policy;            
    const struct sched_class *sched_class;
    int priority;          
    int base_priority;     
    int time_slice;        
    int quantum;           
    uint64_t cpu_time;     
    uint64_t last_run;     
    
     
    uint64_t vruntime;
    
     
    uint64_t cpu_affinity;  

     
    struct process* next;        
    struct process* prev;
    struct process* parent;
    struct process* next_ready;  
    
     
    uint64_t gid;          
    uint64_t sid;          

     
    struct list_head held_locks;
    
     
    struct dentry *cwd;    
    struct vfsmount *cwd_mnt;  
    
     
    struct nsproxy *nsproxy;
    
     
    struct css_set *cgroups;
    
     
    struct seccomp_filter *seccomp_filter;

     
    int exit_code;
    struct file* fd_table[MAX_FILES];
    struct rlimit rlimits[RLIMIT_NLIMITS];
    
     
    uint64_t signal_handler[32];
    uint32_t pending_signals;
    uint32_t signal_mask;
    
     
    void* waiting_on;      
};

extern struct process* current_process;

void process_init();
struct process* process_create(void (*entry_point)());
struct process* process_create_kthread(void (*entry_point)(void*), void *arg);
struct process* process_create_kernel_thread(void (*entry)(void), const char *name);
void enqueue_process(struct process* proc);  
void process_schedule();
irqreturn_t scheduler_tick(int irq, void *dev_id);  
void process_yield();
void process_exit(int code);
struct process* get_process_by_pid(int pid);
struct process* get_process_list_head();  
int process_fork();
int process_wait(int pid, int* status);
int process_exec(const char* path, const char** argv);
void process_sleep(int ticks);
int process_set_priority(int pid, int priority);
int process_get_priority(int pid);

int check_rlimit(int resource, uint64_t amount);
#endif
