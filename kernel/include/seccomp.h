#ifndef SECCOMP_H
#define SECCOMP_H

#include "types.h"
#include "bpf.h"

struct process;  

#define SECCOMP_MODE_DISABLED 0
#define SECCOMP_MODE_STRICT   1
#define SECCOMP_MODE_FILTER   2

#define SECCOMP_RET_KILL    0x00000000U
#define SECCOMP_RET_TRAP    0x00030000U
#define SECCOMP_RET_ERRNO   0x00050000U
#define SECCOMP_RET_TRACE   0x7ff00000U
#define SECCOMP_RET_ALLOW   0x7fff0000U

#define SECCOMP_RET_ACTION  0x7fff0000U
#define SECCOMP_RET_DATA    0x0000ffffU

struct seccomp_data {
    int nr;
    uint32_t arch;
    uint64_t instruction_pointer;
    uint64_t args[6];
};

struct seccomp_filter {
    int ref_count;
    struct sock_fprog prog;
    struct seccomp_filter *prev;
};

 
int seccomp_check_syscall(int syscall_nr);
int seccomp_attach_filter(struct sock_fprog *prog);
void seccomp_fork(struct process *child, struct process *parent);
void seccomp_exit(struct process *p);

#endif
