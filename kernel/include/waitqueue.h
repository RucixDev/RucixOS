#ifndef WAITQUEUE_H
#define WAITQUEUE_H

#include "spinlock.h"

struct process;

typedef struct wait_queue_entry {
    struct process* task;
    struct wait_queue_entry* next;
} wait_queue_entry_t;

typedef struct {
    wait_queue_entry_t* head;
    wait_queue_entry_t* tail;
    spinlock_t lock;
} wait_queue_t;

void wait_queue_init(wait_queue_t* wq);
void sleep_on(wait_queue_t* wq);
void wake_up(wait_queue_t* wq);
void wake_up_all(wait_queue_t* wq);

 
typedef struct {
    spinlock_t lock;
    int count;
    wait_queue_t wait;
} semaphore_t;

void sem_init(semaphore_t* sem, int count);
void sem_wait(semaphore_t* sem);
void sem_post(semaphore_t* sem);

 
typedef struct {
    spinlock_t lock;
    struct process* owner;
    int recurse_count;
    wait_queue_t wait;
} mutex_t;

void mutex_init(mutex_t* mutex);
void mutex_lock(mutex_t* mutex);
void mutex_unlock(mutex_t* mutex);

 
typedef struct {
    wait_queue_t wait;
} cond_t;

void cond_init(cond_t* cond);
void cond_wait(cond_t* cond, mutex_t* mutex);
void cond_signal(cond_t* cond);
void cond_broadcast(cond_t* cond);

 
typedef struct {
    spinlock_t lock;
    int readers;
    int writers;  
    wait_queue_t read_wait;
    wait_queue_t write_wait;
} rwlock_t;

void rwlock_init(rwlock_t* rw);
void rwlock_read_lock(rwlock_t* rw);
void rwlock_read_unlock(rwlock_t* rw);
void rwlock_write_lock(rwlock_t* rw);
void rwlock_write_unlock(rwlock_t* rw);

 
typedef struct {
    spinlock_t lock;
    int count;
    int current;
    wait_queue_t wait;
} barrier_t;

void barrier_init(barrier_t* barrier, int count);
void barrier_wait(barrier_t* barrier);

#endif
