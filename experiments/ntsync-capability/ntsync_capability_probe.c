#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
/* Older distro/kernel header packages may not ship this header at all. */
#if defined(__has_include)
# if __has_include(<linux/ntsync.h>)
#  define HAS_NTSYNC_UAPI_HEADER 1
# endif
#endif
#ifdef HAS_NTSYNC_UAPI_HEADER
# include <linux/ntsync.h>
#endif

int main(void) {
#ifndef HAS_NTSYNC_UAPI_HEADER
    puts("SKIP: installed libc kernel headers do not provide linux/ntsync.h");
    return 77;
#else
#ifdef NTSYNC_IOC_WAIT_ANY
    _Static_assert(sizeof(struct ntsync_sem_args) == 8, "unexpected ntsync semaphore UAPI struct size");
    _Static_assert(sizeof(struct ntsync_wait_args) == 40, "unexpected ntsync wait UAPI struct size");
    _Static_assert(offsetof(struct ntsync_wait_args, alert) == 32, "unexpected ntsync alert offset");
    printf("ntsync_header=full sem_size=%zu wait_size=%zu wait_any=0x%lx create_sem=0x%lx\n",
           sizeof(struct ntsync_sem_args), sizeof(struct ntsync_wait_args),
           (unsigned long)NTSYNC_IOC_WAIT_ANY, (unsigned long)NTSYNC_IOC_CREATE_SEM);
    int dev = open("/dev/ntsync", O_RDWR | O_CLOEXEC);
    if (dev == -1) {
        printf("SKIP: /dev/ntsync inaccessible: %s\n", strerror(errno));
        return 77;
    }
    struct ntsync_sem_args args = { .count=0, .max=1 };
    int obj = ioctl(dev, NTSYNC_IOC_CREATE_SEM, &args);
    if (obj == -1) {
        fprintf(stderr, "FAIL: create semaphore: %s\n", strerror(errno));
        close(dev);
        return 1;
    }
    close(obj);
    close(dev);
    puts("PASS: modern ntsync UAPI compiled and semaphore creation succeeded");
    return 0;
#else
    printf("SKIP: installed linux/ntsync.h lacks NTSYNC_IOC_WAIT_ANY; "
           "old CREATE_SEM=0x%lx, sem_args_size=%zu\n",
           (unsigned long)NTSYNC_IOC_CREATE_SEM, sizeof(struct ntsync_sem_args));
    return 77;
#endif
#endif
}
