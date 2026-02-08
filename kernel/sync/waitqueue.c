#include "waitqueue.h"
#include "process.h"
#include "console.h"

extern struct process* current_process;

void wait_queue_init(wait_queue_t* wq) {
    wq->head = 0;
    wq->tail = 0;
    spinlock_init(&wq->lock);
}

void sleep_on(wait_queue_t* wq) {
    spinlock_acquire(&wq->lock);
    
    wait_queue_entry_t entry;
    entry.task = current_process;
    entry.next = 0;
    
    if (wq->tail) {
        wq->tail->next = &entry;
        wq->tail = &entry;
    } else {
        wq->head = &entry;
        wq->tail = &entry;
    }
    
    current_process->state = PROCESS_STATE_BLOCKED;
    
    spinlock_release(&wq->lock);
    
    process_schedule();
}

void wake_up(wait_queue_t* wq) {
    spinlock_acquire(&wq->lock);
    
    if (wq->head) {
        wait_queue_entry_t* entry = wq->head;
        wq->head = entry->next;
        if (!wq->head) wq->tail = 0;
        
        entry->task->state = PROCESS_STATE_READY;
        enqueue_process(entry->task);
    }
    
    spinlock_release(&wq->lock);
}

void wake_up_all(wait_queue_t* wq) {
    spinlock_acquire(&wq->lock);
    
    while (wq->head) {
        wait_queue_entry_t* entry = wq->head;
        wq->head = entry->next;
        if (!wq->head) wq->tail = 0;
        
        entry->task->state = PROCESS_STATE_READY;
        enqueue_process(entry->task);
    }
    
    spinlock_release(&wq->lock);
}

 
void sem_init(semaphore_t* sem, int count) {
    spinlock_init(&sem->lock);
    wait_queue_init(&sem->wait);
    sem->count = count;
}

void sem_wait(semaphore_t* sem) {
    while (1) {
        spinlock_acquire(&sem->lock);
        if (sem->count > 0) {
            sem->count--;
            spinlock_release(&sem->lock);
            return;
        }
        spinlock_release(&sem->lock);
        sleep_on(&sem->wait);
    }
}

void sem_post(semaphore_t* sem) {
    spinlock_acquire(&sem->lock);
    sem->count++;
    spinlock_release(&sem->lock);
    wake_up(&sem->wait);
}

 
void mutex_init(mutex_t* mutex) {
    spinlock_init(&mutex->lock);
    wait_queue_init(&mutex->wait);
    mutex->owner = 0;
    mutex->recurse_count = 0;
}

void mutex_lock(mutex_t* mutex) {
    struct process* me = current_process;
    
    while (1) {
        spinlock_acquire(&mutex->lock);
        
        if (mutex->owner == 0) {
            mutex->owner = me;
            mutex->recurse_count = 1;
            spinlock_release(&mutex->lock);
            return;
        }
        
        if (mutex->owner == me) {
            mutex->recurse_count++;
            spinlock_release(&mutex->lock);
            return;
        }
        
         
        if (mutex->owner->priority > me->priority) {
             int old_prio = mutex->owner->priority;
             (void)old_prio;
             process_set_priority(mutex->owner->pid, me->priority);
        }
        
        spinlock_release(&mutex->lock);
        sleep_on(&mutex->wait);
    }
}

void barrier_init(barrier_t* barrier, int count) {
    spinlock_init(&barrier->lock);
    wait_queue_init(&barrier->wait);
    barrier->count = count;
    barrier->current = 0;
}

void barrier_wait(barrier_t* barrier) {
    spinlock_acquire(&barrier->lock);
    barrier->current++;
    
    if (barrier->current >= barrier->count) {
        barrier->current = 0;  
        spinlock_release(&barrier->lock);
        wake_up_all(&barrier->wait);
    } else {
        spinlock_release(&barrier->lock);
        sleep_on(&barrier->wait);
    }
}

void mutex_unlock(mutex_t* mutex) {
    spinlock_acquire(&mutex->lock);
    
    if (mutex->owner == current_process) {
        mutex->recurse_count--;
        if (mutex->recurse_count == 0) {
            mutex->owner = 0;
            spinlock_release(&mutex->lock);
            wake_up(&mutex->wait);
            return;
        }
    }
    
    spinlock_release(&mutex->lock);
}

 
void cond_init(cond_t* cond) {
    wait_queue_init(&cond->wait);
}

void cond_wait(cond_t* cond, mutex_t* mutex) {
    mutex_unlock(mutex);
    sleep_on(&cond->wait);
    mutex_lock(mutex);
}

void cond_signal(cond_t* cond) {
    wake_up(&cond->wait);
}

void cond_broadcast(cond_t* cond) {
    wake_up_all(&cond->wait);
}

 
void rwlock_init(rwlock_t* rw) {
    spinlock_init(&rw->lock);
    wait_queue_init(&rw->read_wait);
    wait_queue_init(&rw->write_wait);
    rw->readers = 0;
    rw->writers = 0;
}

void rwlock_read_lock(rwlock_t* rw) {
    while (1) {
        spinlock_acquire(&rw->lock);
        if (rw->writers == 0) {
            rw->readers++;
            spinlock_release(&rw->lock);
            return;
        }
        spinlock_release(&rw->lock);
        sleep_on(&rw->read_wait);
    }
}

void rwlock_read_unlock(rwlock_t* rw) {
    spinlock_acquire(&rw->lock);
    rw->readers--;
    if (rw->readers == 0) {
        spinlock_release(&rw->lock);
        wake_up(&rw->write_wait);  
    } else {
        spinlock_release(&rw->lock);
    }
}

void rwlock_write_lock(rwlock_t* rw) {
    while (1) {
        spinlock_acquire(&rw->lock);
        if (rw->writers == 0 && rw->readers == 0) {
            rw->writers = 1;
            spinlock_release(&rw->lock);
            return;
        }
        spinlock_release(&rw->lock);
        sleep_on(&rw->write_wait);
    }
}

void rwlock_write_unlock(rwlock_t* rw) {
    spinlock_acquire(&rw->lock);
    rw->writers = 0;
    spinlock_release(&rw->lock);
    wake_up_all(&rw->read_wait);
    wake_up(&rw->write_wait);
}
