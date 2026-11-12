#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <linux/prctl.h>
/* Kernel UAPI asm-generic/siginfo.h defines this code, older libc headers may omit it. */
#ifndef SYS_USER_DISPATCH
#define SYS_USER_DISPATCH 2
#endif

#if !defined(__x86_64__)
#error "Probe currently supports only x86_64"
#endif

static volatile unsigned char selector = SYSCALL_DISPATCH_FILTER_ALLOW;
static volatile sig_atomic_t traps;
static volatile sig_atomic_t unexpected;
static volatile sig_atomic_t observed_syscall = -1;
static const long synthetic_result = 0x12345L;

static void on_sigsys(int signo, siginfo_t *info, void *context) {
    ucontext_t *uc = context;
    if (signo != SIGSYS || info->si_code != SYS_USER_DISPATCH) unexpected = 1;
    observed_syscall = info->si_syscall;
    traps++;
    selector = SYSCALL_DISPATCH_FILTER_ALLOW; /* avoid intercepting the sigreturn trampoline */
    uc->uc_mcontext.gregs[REG_RAX] = synthetic_result;
}

static long raw_getpid(void) {
    long result;
    __asm__ volatile("syscall" : "=a"(result) : "a"(SYS_getpid) : "rcx", "r11", "memory");
    return result;
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_sigsys;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSYS, &sa, NULL) != 0) { perror("sigaction"); return 2; }
    if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, 0UL, 0UL, &selector) != 0) {
        fprintf(stderr, "UNSUPPORTED: prctl: %s\n", strerror(errno));
        return 77;
    }
    selector = SYSCALL_DISPATCH_FILTER_BLOCK;
    const long blocked_result = raw_getpid();
    /* Handler already restored ALLOW state; disable dispatch explicitly. */
    if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_OFF, 0UL, 0UL, 0UL) != 0) {
        perror("disable prctl"); return 3;
    }
    const long actual_result = raw_getpid();
    printf("blocked_return=%ld actual_pid=%ld traps=%d observed_syscall=%d unexpected=%d\n",
           blocked_result, actual_result, traps, observed_syscall, unexpected);
    if (blocked_result != synthetic_result || actual_result <= 0 ||
        traps != 1 || observed_syscall != SYS_getpid || unexpected) return 1;
    return 0;
}
