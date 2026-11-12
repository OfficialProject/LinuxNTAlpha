# ntsync header/device capability probe

Use **make test** on Linux with a C compiler. The probe deliberately distinguishes **PASS**, **SKIP** and **FAIL**.

1. Compile against the *installed* linux/ntsync.h. If it lacks NTSYNC_IOC_WAIT_ANY, exit 77 / SKIP and report the available old ioctl number and old semaphore structure size.
2. If a modern complete UAPI is found, assert the expected structure sizes and alert offset at compile time, then attempt to open /dev/ntsync.
3. If the device is missing or inaccessible, exit 77 / SKIP, never PASS.
4. If device opens, attempt to create a semaphore using the modern ioctl and close the object. Only success reports PASS; unexpected ioctl failure reports FAIL.

On the research container (2026-11-12), kernel 6.18.57, Debian 13, linux-libc-dev 6.12.107-1, the installed header has a 12-byte ntsync_sem_args, old CREATE_SEM ioctl value 0xc00c4e80, no NTSYNC_IOC_WAIT_ANY, and the probe reports SKIP. Device /dev/ntsync is also absent from the container; whether it exists or is enabled on the underlying host was not determined.

When compiling against a *locally prepared v6.14+ header-shape fixture* during research, the probe reported: semaphore structure 8 bytes, wait structure 40 bytes, WAIT_ANY ioctl 0xc0284e82 and CREATE_SEM ioctl 0x40084e80, then SKIP because /dev/ntsync did not exist. The fixture was for compiler-path validation only, not claimed as an independently version-certified header installation.

This code is a capability check, not an implementation, kernel compatibility test across all versions, synchronization conformance suite, or performance benchmark.

Verified upstream source refs:
- https://github.com/torvalds/linux/blob/v6.13/include/uapi/linux/ntsync.h
- https://github.com/torvalds/linux/blob/v6.14/include/uapi/linux/ntsync.h

For the wider ABI comparison and consequences see [R18](../../research/R18_NTSYNC_ACTUAL_UAPI_TRANSITION.md).
