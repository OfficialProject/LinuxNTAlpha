#define _GNU_SOURCE
#include <asm/prctl.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <unistd.h>

#if !defined(__x86_64__)
#error "The Windows x64 GS probe requires x86-64"
#endif

/* Synthetic NT_TIB-like prefix. Not a complete Windows TEB. */
struct fake_tib {
    uintptr_t exception_list;
    uintptr_t stack_base;
    uintptr_t stack_limit;
    uintptr_t subsystem_tib;
    uintptr_t fiber_data;
    uintptr_t arbitrary_user_pointer;
    struct fake_tib *self;
    uint64_t cookie;
};
_Static_assert(offsetof(struct fake_tib, self) == 0x30, "x64 NT_TIB Self offset");
_Static_assert(offsetof(struct fake_tib, cookie) == 0x38, "test cookie offset");

static _Thread_local struct fake_tib *expected_tib;
static _Thread_local uint64_t expected_cookie;
static _Thread_local unsigned local_signal_hits;
static _Atomic int errors;
static _Atomic int signals_seen;
static _Atomic int threads_completed;
static _Atomic int go;
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "signal handler requires lock-free atomic int");

static int get_base(int which, unsigned long *base) {
    return (int)syscall(SYS_arch_prctl, which, base);
}
static int set_base(int which, unsigned long base) {
    return (int)syscall(SYS_arch_prctl, which, base);
}

static inline uintptr_t read_gs_self(void) {
    uintptr_t result;
    __asm__ volatile("movq %%gs:0x30, %0" : "=r"(result));
    return result;
}
static inline uint64_t read_gs_cookie(void) {
    uint64_t result;
    __asm__ volatile("movq %%gs:0x38, %0" : "=r"(result));
    return result;
}
static void on_sigusr1(int signo) {
    if (signo != SIGUSR1 || read_gs_self() != (uintptr_t)expected_tib ||
        read_gs_cookie() != expected_cookie) {
        atomic_fetch_add_explicit(&errors, 1, memory_order_relaxed);
    }
    ++local_signal_hits;
    atomic_fetch_add_explicit(&signals_seen, 1, memory_order_relaxed);
}

#define THREADS 4
#define ITERATIONS 2048
#define SIGNAL_PERIOD 64

static void *worker(void *raw) {
    unsigned index = (unsigned)(uintptr_t)raw;
    struct fake_tib tib = {0};
    tib.self = &tib;
    tib.cookie = UINT64_C(0x6E74546562415631) + index;
    expected_tib = &tib;
    expected_cookie = tib.cookie;
    local_signal_hits = 0;
    unsigned long original_fs, original_gs, after_fs, after_gs;
    if (get_base(ARCH_GET_FS, &original_fs) != 0 ||
        get_base(ARCH_GET_GS, &original_gs) != 0) {
        atomic_fetch_add(&errors, 1);
        return NULL;
    }
    if (set_base(ARCH_SET_GS, (unsigned long)(uintptr_t)&tib) != 0) {
        atomic_fetch_add(&errors, 1);
        return NULL;
    }
    while (!atomic_load_explicit(&go, memory_order_acquire)) sched_yield();
    for (unsigned i = 0; i < ITERATIONS; ++i) {
        if (read_gs_self() != (uintptr_t)&tib || read_gs_cookie() != tib.cookie)
            atomic_fetch_add(&errors, 1);
        if ((i % SIGNAL_PERIOD) == 0) {
            if (pthread_kill(pthread_self(), SIGUSR1) != 0)
                atomic_fetch_add(&errors, 1);
            sched_yield();
        }
    }
    if (get_base(ARCH_GET_FS, &after_fs) != 0 || after_fs != original_fs)
        atomic_fetch_add(&errors, 1);
    if (get_base(ARCH_GET_GS, &after_gs) != 0 ||
        after_gs != (unsigned long)(uintptr_t)&tib)
        atomic_fetch_add(&errors, 1);
    if (local_signal_hits != ITERATIONS / SIGNAL_PERIOD)
        atomic_fetch_add(&errors, 1);
    if (set_base(ARCH_SET_GS, original_gs) != 0)
        atomic_fetch_add(&errors, 1);
    expected_tib = NULL;
    atomic_fetch_add(&threads_completed, 1);
    return NULL;
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_sigusr1;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) != 0) { perror("sigaction"); return 1; }
    unsigned long original_fs, original_gs, end_fs, end_gs;
    if (get_base(ARCH_GET_FS, &original_fs) != 0 ||
        get_base(ARCH_GET_GS, &original_gs) != 0) {
        perror("arch_prctl GET FS/GS"); return 77;
    }
    struct fake_tib main_tib = {0};
    main_tib.self = &main_tib;
    main_tib.cookie = 123456;
    if (set_base(ARCH_SET_GS, (unsigned long)(uintptr_t)&main_tib) != 0) {
        printf("SKIP: ARCH_SET_GS unavailable: %s\n", strerror(errno));
        return 77;
    }
    int initial_ok = read_gs_self() == (uintptr_t)&main_tib &&
                     read_gs_cookie() == main_tib.cookie;
    if (set_base(ARCH_SET_GS, original_gs) != 0) {
        perror("restore original GS"); return 1;
    }
    if (!initial_ok) { fputs("FAIL: main GS test read mismatch\n", stderr); return 1; }

    pthread_t threads[THREADS];
    unsigned started = 0;
    int create_failed = 0;
    for (unsigned i = 0; i < THREADS; ++i) {
        if (pthread_create(&threads[i], NULL, worker, (void *)(uintptr_t)i) != 0) {
            fputs("FAIL: unable to create all worker threads\n", stderr);
            create_failed = 1;
            break;
        }
        ++started;
    }
    atomic_store_explicit(&go, 1, memory_order_release);
    for (unsigned i = 0; i < started; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            fputs("FAIL: pthread_join\n", stderr); return 1;
        }
    }

    if (get_base(ARCH_GET_FS, &end_fs) != 0 || get_base(ARCH_GET_GS, &end_gs) != 0) {
        fputs("FAIL: final arch_prctl GET\n", stderr); return 1;
    }
    int failures = atomic_load(&errors);
    int completed = atomic_load(&threads_completed);
    int hits = atomic_load(&signals_seen);
    printf("threads=%d iterations_per_thread=%d signal_hits=%d expected=%d errors=%d main_fs_same=%d main_gs_same=%d\n",
           completed, ITERATIONS, hits, THREADS * ITERATIONS / SIGNAL_PERIOD,
           failures, end_fs == original_fs, end_gs == original_gs);
    if (create_failed || failures || completed != THREADS || hits != THREADS * ITERATIONS / SIGNAL_PERIOD ||
        end_fs != original_fs || end_gs != original_gs) return 1;
    puts("PASS: per-thread GS context, signal context and Linux FS TLS separation");
    return 0;
}
