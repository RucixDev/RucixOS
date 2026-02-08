#include "spinlock.h"
#include "console.h"
#include "process.h"  

void spinlock_init(spinlock_t* lock) {
    lock->locked = 0;
    lock->rflags = 0;
    lock->owner_pid = -1;
    lock->owner_cpu = 0;
}

void spinlock_acquire(spinlock_t* lock) {
    uint64_t rflags;
     
    __asm__ volatile (
        "pushfq\n\t"
        "pop %0\n\t"
        "cli"
        : "=r"(rflags)
        : 
        : "memory"
    );
    
     
    int current_pid = (current_process) ? current_process->pid : -2;  
    
    if (lock->locked && lock->owner_pid == current_pid) {
        kprint_str("DEADLOCK DETECTED! Recursive spinlock acquire. PID: ");
        kprint_dec(current_pid);
        kprint_str(" Lock Owner: ");
        kprint_dec(lock->owner_pid);
        kprint_str("\n");
         
        lock->locked = 0; 
    }

    if (lock->locked) {
         kprint_str("Spinlock contention: Lock ");
         kprint_hex((uint64_t)lock);
         kprint_str(" held by PID ");
         kprint_dec(lock->owner_pid);
         kprint_str("\n");
    }

    int timeout = 1000;  
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        while (lock->locked) {
            __asm__ volatile ("pause");
            if (timeout-- <= 0) {
                 kprint_str("SPINLOCK TIMEOUT! Owner PID: ");
                 kprint_dec(lock->owner_pid);
                 kprint_str("\n");
                  
                 lock->locked = 0;
                 break;
            }
        }
    }
     
    lock->rflags = rflags;
    lock->owner_pid = current_pid;
}

void spinlock_release(spinlock_t* lock) {
    uint64_t rflags = lock->rflags;
    
    lock->owner_pid = -1;  
    
    __sync_lock_release(&lock->locked);
    
    if (rflags & 0x200) {
        __asm__ volatile ("sti");
    }
}
