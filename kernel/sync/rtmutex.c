#include "rtmutex.h"
#include "process.h"
#include "spinlock.h"
#include "list.h"
#include "console.h"

 
extern struct process* current_process;
extern void process_schedule();
extern void enqueue_process(struct process* proc);

void rt_mutex_init(rt_mutex_t *lock) {
    spinlock_init(&lock->wait_lock);
    lock->owner = NULL;
    INIT_LIST_HEAD(&lock->wait_list);
    INIT_LIST_HEAD(&lock->held_list_entry);
    lock->save_state = 0;
}

 
 
static void rt_mutex_adjust_prio_chain(struct process *owner, int priority) {
    struct process *curr = owner;
    int boost_prio = priority;
    int depth = 0;
    
    while (curr && depth < 10) {  
        if (curr->priority <= boost_prio) {
            break;
        }
        
        curr->priority = boost_prio;
       
        if (curr->state == PROCESS_STATE_BLOCKED) {
            break; 
        }
        depth++;
    }
}

 
static void rt_mutex_restore_prio(struct process *owner) {
    if (!owner) return;
    
    int highest_prio = owner->base_priority;  

    struct list_head *lptr;
    struct list_head *wptr;

    for (lptr = owner->held_locks.next; lptr != &owner->held_locks; lptr = lptr->next) {
        rt_mutex_t *lock = (rt_mutex_t *)((char *)lptr - (unsigned long)(&((rt_mutex_t *)0)->held_list_entry));
        
        if (!list_empty(&lock->wait_list)) {
            for (wptr = lock->wait_list.next; wptr != &lock->wait_list; wptr = wptr->next) {
                struct rt_mutex_waiter *waiter = (struct rt_mutex_waiter *)wptr;
                if (waiter->task->priority < highest_prio) {
                    highest_prio = waiter->task->priority;
                }
            }
        }
    }
    
    owner->priority = highest_prio;
}

void rt_mutex_lock(rt_mutex_t *lock) {
    spinlock_acquire(&lock->wait_lock);

    if (lock->owner == NULL) {
        lock->owner = current_process;
        list_add(&lock->held_list_entry, &current_process->held_locks);
        spinlock_release(&lock->wait_lock);
        return;
    }

    if (lock->owner == current_process) {
        spinlock_release(&lock->wait_lock);
        return;
    }

    struct rt_mutex_waiter waiter;
    waiter.task = current_process;
    waiter.lock = lock;
    INIT_LIST_HEAD(&waiter.list);

    list_add_tail(&waiter.list, &lock->wait_list);

    struct process *owner = lock->owner;
    if (owner && current_process->priority < owner->priority) {
        rt_mutex_adjust_prio_chain(owner, current_process->priority);
    }

    while (lock->owner != current_process) {
        current_process->state = PROCESS_STATE_BLOCKED;
        
        spinlock_release(&lock->wait_lock);
        
        process_schedule();
        
        spinlock_acquire(&lock->wait_lock);
        
        if (lock->owner == NULL) {
            lock->owner = current_process;
            list_add(&lock->held_list_entry, &current_process->held_locks);
            break;
        }
    }

    list_del(&waiter.list);
    
    spinlock_release(&lock->wait_lock);
}

int rt_mutex_trylock(rt_mutex_t *lock) {
    spinlock_acquire(&lock->wait_lock);
    if (lock->owner == NULL) {
        lock->owner = current_process;
        list_add(&lock->held_list_entry, &current_process->held_locks);
        spinlock_release(&lock->wait_lock);
        return 1;
    }
    spinlock_release(&lock->wait_lock);
    return 0;
}

void rt_mutex_unlock(rt_mutex_t *lock) {
    spinlock_acquire(&lock->wait_lock);
    
    if (lock->owner != current_process) {
        spinlock_release(&lock->wait_lock);
        return;
    }

    list_del(&lock->held_list_entry);
    lock->owner = NULL;

    if (current_process->priority != current_process->base_priority) {
        rt_mutex_restore_prio(current_process);
    }

    if (!list_empty(&lock->wait_list)) {
        struct rt_mutex_waiter *pos;
        struct rt_mutex_waiter *best_waiter = NULL;
        int best_prio = 999;

        struct list_head *ptr;
        for (ptr = lock->wait_list.next; ptr != &lock->wait_list; ptr = ptr->next) {
            pos = (struct rt_mutex_waiter *)ptr;
            
            if (pos->task->priority < best_prio) {
                best_prio = pos->task->priority;
                best_waiter = pos;
            }
        }

        if (best_waiter) {
            best_waiter->task->state = PROCESS_STATE_READY;
            enqueue_process(best_waiter->task);
        }
    }

    spinlock_release(&lock->wait_lock);
}
