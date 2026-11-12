# R18 — Actual ntsync UAPI change, stale installed headers and runtime capability

Date: 2026-11-12. **Source-verified from pinned upstream tags plus reproduced local compile tests**. No successful ntsync kernel ioctl test; the device was unavailable.

## Why this matters

Earlier R8/R9 identified **documentation-only** name mismatches in v6.18/v6.19; their UAPI headers were actually identical. That conclusion is limited to the audited newer versions. The older upstream **v6.13 -> v6.14** transition is a genuine binary UAPI change and needs separate treatment.

## Pinned source evidence

GitHub repository API (file SHA is Git blob SHA):
- **v6.12 and v6.13**: include/uapi/linux/ntsync.h, blob dcfa38fdc93c61271b602ee6c2052d7f98d03662. The struct ntsync_sem_args has three u32 members: sem, count, max. Header exposes only CREATE_SEM and SEM_POST. CREATE_SEM uses _IOWR('N', 0x80, struct ntsync_sem_args).
- **v6.14 through at least v6.19** (v6.14/v6.15/v6.16/v6.17/v6.18/v6.19 each individually checked) and upstream master: include/uapi/linux/ntsync.h blob 6d06793512b150a80018618b698537a0e3793e54. Semaphore arguments contain two u32 members, count and max; WAIT_ANY, WAIT_ALL, mutex/event and other ioctls are present; CREATE_SEM uses _IOW('N', 0x80, struct ntsync_sem_args).

Sources:
- https://github.com/torvalds/linux/blob/v6.13/include/uapi/linux/ntsync.h
- https://github.com/torvalds/linux/blob/v6.14/include/uapi/linux/ntsync.h
- https://github.com/torvalds/linux/blob/v6.18/include/uapi/linux/ntsync.h

**VERIFIED:** this is **not** just a symbolic macro rename. The argument structure layout and ioctl direction/encoded size differ. Under the standard Linux x86-64 ioctl encoding, CREATE_SEM is 0xc00c4e80 with the old 12-byte struct and 0x40084e80 with the new 8-byte struct. These values were reproduced using locally compiled C expressions with sys/ioctl.h, not guessed from the name. The binary ioctl identifiers therefore differ.

This also gives a concrete resolution of the earlier false-positive ABI concern: **there is a real older change**, but the names mentioned in a later v6.18 documentation heading (KILL_OWNER vs MUTEX_KILL and READ_EVENT vs EVENT_READ) are *not* a new v6.18-to-v6.19 UAPI change.

## Local runtime and header mismatch reproduced

Research execution container:
- Running kernel: 6.18.57 x86-64 (uname -r).
- Distribution: Debian GNU/Linux 13 (trixie).
- Installed linux-libc-dev: 6.12.107-1 (dpkg-query).
- Installed /usr/include/linux/ntsync.h has the *old* 12-byte semaphore structure, CREATE_SEM 0xc00c4e80, SEM_POST, and no WAIT_ANY.
- /dev/ntsync is absent in the container; no kernel synchronization semantics could be tested.

**Observed:** gcc/cc successfully compiled [ntsync-capability fixture](../experiments/ntsync-capability/ntsync_capability_probe.c), which reported:

    SKIP: installed linux/ntsync.h lacks NTSYNC_IOC_WAIT_ANY; old CREATE_SEM=0xc00c4e80, sem_args_size=12

A separate compile of the fixture against a locally prepared v6.14+ UAPI-shape header succeeded and reported:

    ntsync_header=full sem_size=8 wait_size=40 wait_any=0xc0284e82 create_sem=0x40084e80
    SKIP: /dev/ntsync inaccessible: No such file or directory

That second header was a *locally written structural fixture informed by the pinned upstream definition*, not an imported audited header byte-for-byte. The test establishes the new-code-path compiles; it does not establish an available runtime driver.

## Architecture consequences

1. **Running kernel version is not the installed userspace-header version.** A kernel at 6.18.57 can run under a container with older linux-libc-dev 6.12 headers.
2. No code should infer support from uname alone, header symbols alone or /dev path alone. Require a **two-stage** build-time and runtime feature check, followed by a real ioctl conformance test.
3. For a supported target baseline v6.14+, compile against pinned compatible UAPI declarations (respect Linux-syscall-note licensing) and test against actual installed kernel; avoid unreviewed distro-header shadowing.
4. Do not make a modern driver execute old ioctl encodings, or vice versa, without proof. The incompatible CREATE_SEM command values are visible from source.
5. Design explicit failure messages so a missing driver/header produces **SKIP / UNSUPPORTED**, never a successful benchmark or compatibility claim.

## Remaining work

- Test the modern ioctl implementation on a host exposing /dev/ntsync, with a pinned kernel config/build and actual sem, event, mutex, wait-any and wait-all behavior.
- Determine the earliest accepted stable kernel for complete ntsync support via release notes and tags, not a casual assumption based only on header shape.
- Pin source commits, compile-time structure checks and ioctl values in CI.
- Test actual distro backports and permissions separately from upstream tags; container absence does not demonstrate host kernel absence.

## Verification status

Header/source comparisons **VERIFIED**. Compiler-path checks **REPRODUCED**. Runtime synchronization and performance **NOT TESTED**.
