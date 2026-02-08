#ifndef SIGNAL_H
#define SIGNAL_H

#include <stdint.h>
#include "idt.h"

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15

typedef void (*sighandler_t)(int);

int sys_kill(int pid, int sig);
sighandler_t sys_signal(int sig, sighandler_t handler);
void handle_pending_signals(struct interrupt_frame* frame);

#endif
