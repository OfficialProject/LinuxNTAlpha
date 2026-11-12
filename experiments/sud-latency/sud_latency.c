#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <linux/prctl.h>

#ifndef SYS_USER_DISPATCH
#define SYS_USER_DISPATCH 2
#endif
#ifndef SYSCALL_DISPATCH_FILTER_ALLOW
#define SYSCALL_DISPATCH_FILTER_ALLOW 0
#define SYSCALL_DISPATCH_FILTER_BLOCK 1
#endif
#if !defined(__x86_64__)
#error "This test needs x86-64"
#endif

static volatile unsigned char selector = SYSCALL_DISPATCH_FILTER_ALLOW;
static volatile sig_atomic_t traps, unexpected;
static volatile uint64_t sink;
static const long fake_value = 0x12345;
static void on_sigsys(int signal, siginfo_t *si, void *opaque) {
    ucontext_t *ctx = opaque;
    if (signal != SIGSYS || si->si_code != SYS_USER_DISPATCH ||
        si->si_syscall != SYS_getpid) unexpected++;
    ++traps;
    selector = SYSCALL_DISPATCH_FILTER_ALLOW;
    ctx->uc_mcontext.gregs[REG_RAX] = fake_value;
}
static inline long syscall_getpid(void) {
    long x;
    __asm__ volatile("syscall" : "=a"(x) : "a"(SYS_getpid) : "rcx", "r11", "memory");
    return x;
}
static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) { perror("clock_gettime"); exit(2); }
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}
static double run_batch(unsigned iterations, int intercepted, int verify) {
    uint64_t begin = now_ns();
    uint64_t checksum = 0;
    for (unsigned i=0; i<iterations; i++) {
        if (intercepted) {
            selector = SYSCALL_DISPATCH_FILTER_BLOCK;
            long result = syscall_getpid();
            if (verify && result != fake_value) { fputs("invalid intercepted result\n", stderr); exit(1); }
            checksum += (uint64_t)result;
        } else {
            /* The control also writes both selector states but never traps. */
            selector = SYSCALL_DISPATCH_FILTER_BLOCK;
            selector = SYSCALL_DISPATCH_FILTER_ALLOW;
            long result = syscall_getpid();
            if (verify && result <= 0) { fputs("invalid Linux getpid\n", stderr); exit(1); }
            checksum += (uint64_t)result;
        }
    }
    uint64_t elapsed = now_ns() - begin;
    sink = checksum;
    return (double)elapsed / iterations;
}
static void sort7(double a[], unsigned n) {
    for (unsigned i=1;i<n;i++) {
        double v=a[i]; unsigned j=i;
        while (j && a[j-1] > v) { a[j]=a[j-1]; j--; }
        a[j]=v;
    }
}
static int pin_first_allowed_cpu(void) {
    cpu_set_t allowed;
    if (sched_getaffinity(0,sizeof(allowed),&allowed)) return -1;
    for (int c=0;c<CPU_SETSIZE;c++) {
        if (CPU_ISSET(c,&allowed)) {
            cpu_set_t only;
            CPU_ZERO(&only); CPU_SET(c,&only);
            if (sched_setaffinity(0,sizeof(only),&only)) return -1;
            return c;
        }
    }
    return -1;
}
int main(int argc, char **argv) {
    unsigned iterations = 20000;
    if (argc>2) { fprintf(stderr,"usage: %s [iterations:1000..200000]\n", argv[0]); return 2; }
    if (argc==2) {
        char *end = NULL;
        errno = 0;
        unsigned long value = strtoul(argv[1],&end,10);
        if (errno || !end || *end || value < 1000 || value > 200000) {
            fputs("iterations outside 1000..200000\n",stderr); return 2;
        }
        iterations = (unsigned)value;
    }
    int cpu = pin_first_allowed_cpu();
    struct sigaction sa={0};
    sa.sa_sigaction=on_sigsys;
    sa.sa_flags=SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSYS,&sa,NULL)) { perror("sigaction"); return 2; }
    if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,0UL,0UL,&selector)) {
        fprintf(stderr,"SKIP: SUD unsupported: %s\n",strerror(errno)); return 77;
    }
    /* Warm both paths and verify expected trap counts outside timed rounds. */
    unsigned warm = 1000;
    run_batch(warm,0,1);
    run_batch(warm,1,1);
    if (unexpected || (unsigned)traps != warm) {
        fputs("FAIL: warm-up did not deliver precisely one SIGSYS per blocked syscall\n",stderr);
        return 1;
    }
    const unsigned rounds=7;
    double base[rounds], dispatched[rounds];
    sig_atomic_t before=traps;
    for (unsigned i=0;i<rounds;i++) {
        /* Alternate order to reduce systematic run-order bias. */
        if (!(i&1)) { base[i]=run_batch(iterations,0,0); dispatched[i]=run_batch(iterations,1,0); }
        else { dispatched[i]=run_batch(iterations,1,0); base[i]=run_batch(iterations,0,0); }
    }
    if (unexpected || (unsigned)(traps-before) != rounds*iterations) {
        fputs("FAIL: mismatched trap count or siginfo code\n",stderr); return 1;
    }
    if (prctl(PR_SET_SYSCALL_USER_DISPATCH,PR_SYS_DISPATCH_OFF,0UL,0UL,0UL)) {
        perror("disable SUD"); return 1;
    }
    sort7(base,rounds);sort7(dispatched,rounds);
    struct utsname system;
    const char *kernel = uname(&system) == 0 ? system.release : "unknown";
    printf("kernel=%s cpu_pinned=%d iterations=%u rounds=%u traps=%d\n",
           kernel,cpu,iterations,rounds,traps);
    printf("control_ns_per_call: min=%.1f median=%.1f max=%.1f\n",base[0],base[3],base[6]);
    printf("trapped_ns_per_call: min=%.1f median=%.1f max=%.1f\n",dispatched[0],dispatched[3],dispatched[6]);
    printf("median_ratio_trapped_vs_control=%.2f (batch medians; NOT per-call p95/p99)\n",
           dispatched[3]/base[3]);
    return 0;
}
