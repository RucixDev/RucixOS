#include "syscall.h"
#include "console.h"
#include "process.h"
#include "idt.h"
#include "vfs.h"
#include "pipe.h"
#include "signal.h"
#include "module.h"
#include "seccomp.h"
#include "io.h"

#define MAX_SYSCALLS 256

typedef uint64_t (*syscall_handler_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static syscall_handler_t syscall_table[MAX_SYSCALLS];

static uint64_t sys_yield_wrapper(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    process_yield();
    return 0;
}

static uint64_t sys_exit_wrapper(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    process_exit((int)code);
    return 0;
}

static uint64_t sys_print_wrapper(uint64_t str, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    kprint_str((const char*)str);
    return 0;
}

static uint64_t sys_fork_wrapper(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)process_fork();
}

static uint64_t sys_exec_wrapper(uint64_t path, uint64_t argv, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)process_exec((const char*)path, (const char**)argv);
}

static uint64_t sys_wait_wrapper(uint64_t pid, uint64_t status, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)process_wait((int)pid, (int*)status);
}

static uint64_t sys_pipe_wrapper(uint64_t fds, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)pipe_create((int*)fds);
}

static uint64_t sys_read_wrapper(uint64_t fd, uint64_t buf, uint64_t count, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)vfs_read((int)fd, (char*)buf, (int)count);
}

static uint64_t sys_write_wrapper(uint64_t fd, uint64_t buf, uint64_t count, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)vfs_write((int)fd, (const char*)buf, (int)count);
}

static uint64_t sys_close_wrapper(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)vfs_close((int)fd);
}

static uint64_t sys_dup_wrapper(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)vfs_dup((int)fd);
}

static uint64_t sys_open_wrapper(uint64_t path, uint64_t flags, uint64_t mode, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)vfs_open((const char*)path, (int)flags, (int)mode);
}

static uint64_t sys_init_module_wrapper(uint64_t img, uint64_t len, uint64_t args, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)sys_init_module((void*)img, (unsigned long)len, (const char*)args);
}

static uint64_t sys_delete_module_wrapper(uint64_t name, uint64_t flags, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)sys_delete_module((const char*)name, (unsigned int)flags);
}

static uint64_t sys_getpid_wrapper(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    if (current_process) return current_process->pid;
    return (uint64_t)-1;
}

static uint64_t sys_sleep_wrapper(uint64_t ticks, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    process_sleep((int)ticks);
    return 0;
}

static uint64_t sys_chdir_wrapper(uint64_t path, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)vfs_chdir((const char*)path);
}

static uint64_t sys_getcwd_wrapper(uint64_t buf, uint64_t size, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)vfs_getcwd((char*)buf, (size_t)size);
}

static uint64_t sys_lseek_wrapper(uint64_t fd, uint64_t offset, uint64_t whence, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)vfs_lseek((int)fd, (int)offset, (int)whence);
}

static uint64_t sys_kill_wrapper(uint64_t pid, uint64_t sig, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    return (uint64_t)sys_kill((int)pid, (int)sig);
}

static uint64_t sys_reboot_wrapper(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    outb(0x64, 0xFE);
    return 0;
}

static uint64_t sys_unknown_wrapper(uint64_t n, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    kprint_str("Unknown Syscall: ");
    kprint_hex(n);
    kprint_newline();
    return (uint64_t)-1;
}


void syscall_handler(struct interrupt_frame* frame) {
    uint64_t syscall_num = frame->rax;
    
    if (seccomp_check_syscall(syscall_num) != 0) {
        frame->rax = -1; 
        return;
    }

    if (syscall_num < MAX_SYSCALLS && syscall_table[syscall_num]) {
        frame->rax = syscall_table[syscall_num](frame->rsi, frame->rdx, frame->rcx, 0, 0, 0);
    } else {
        frame->rax = sys_unknown_wrapper(syscall_num, 0, 0, 0, 0, 0);
    }

    handle_pending_signals(frame);
}

void syscall_init() {
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        syscall_table[i] = sys_unknown_wrapper;
    }

    syscall_table[SYS_YIELD] = sys_yield_wrapper;
    syscall_table[SYS_EXIT] = sys_exit_wrapper;
    syscall_table[SYS_PRINT] = sys_print_wrapper;
    syscall_table[SYS_FORK] = sys_fork_wrapper;
    syscall_table[SYS_EXEC] = sys_exec_wrapper;
    syscall_table[SYS_WAIT] = sys_wait_wrapper;
    syscall_table[SYS_PIPE] = sys_pipe_wrapper;
    syscall_table[SYS_READ] = sys_read_wrapper;
    syscall_table[SYS_WRITE] = sys_write_wrapper;
    syscall_table[SYS_CLOSE] = sys_close_wrapper;
    syscall_table[SYS_DUP] = sys_dup_wrapper;
    syscall_table[SYS_OPEN] = sys_open_wrapper;
    syscall_table[SYS_INIT_MODULE] = sys_init_module_wrapper;
    syscall_table[SYS_DELETE_MODULE] = sys_delete_module_wrapper;
    syscall_table[SYS_GETPID] = sys_getpid_wrapper;
    syscall_table[SYS_SLEEP] = sys_sleep_wrapper;
    syscall_table[SYS_CHDIR] = sys_chdir_wrapper;
    syscall_table[SYS_GETCWD] = sys_getcwd_wrapper;
    syscall_table[SYS_LSEEK] = sys_lseek_wrapper;
    syscall_table[SYS_KILL] = sys_kill_wrapper;
    syscall_table[SYS_REBOOT] = sys_reboot_wrapper;
}
