# R6 — Verified NTSYNC API details and implications
Date: 2026-11-12
Primary source: https://docs.kernel.org/userspace-api/ntsync.html (consulted 2026-11-12)
Status: documentation audit, not measured performance.

## Verified API contract
- NTSYNC provides NT-style semaphores, recursive mutexes and manual/auto-reset events through /dev/ntsync and ioctls.
- Objects are represented by file descriptors and are scoped to an instance of the ntsync device. Closing all descriptors deletes an object.
- Wait-any atomically acquires one eligible object; wait-all atomically acquires all, or none on timeout.
- Abandoned mutex ownership is not automatically inferred from thread death. Userspace must communicate death using NTSYNC_IOC_MUTEX_KILL.
- Mutex owner IDs are not verified by the kernel to correspond to a calling thread.
- Wait timeout is absolute nanoseconds, monotonic by default; optional realtime flag. Signals may interrupt waits.
- NTSYNC is explicitly intended for NT compatibility, not general Linux synchronization.

## Implications for LinuxNTAlpha
1. A user-space NT object manager remains necessary for handle tables, access control, names, object lifetime, and process/thread bookkeeping. NTSYNC alone is not an NT executive.
2. Thread-exit paths must track mutex ownership and issue abandonment notifications; otherwise semantics diverge.
3. Handle inheritance/duplication and file descriptor lifetime require careful translation.
4. NT wait status conversion must distinguish signaled, timeout, abandoned, alert and interrupted cases.
5. Correctness tests must cover duplicate objects in wait arrays, wait-all atomicity, ownership, and process exit.
6. An NTSYNC microbenchmark is meaningful only if competing implementations reproduce the same semantics.

## Negative finding
Documentation does not establish that NTSYNC always beats other designs, that a complete Windows process can run without a compatibility runtime, or that Windows kernel drivers/anti-cheat will function.

## Next source-audit questions
- Determine actual upstream kernel version and feature gates for NTSYNC on target distributions.
- Read driver source and tests; identify locking, fairness and possible contention paths.
- Cross-check documented semantics against Windows wait and synchronization specifications.
- Quantify syscall and scheduling costs with repeatable benchmarks on real hardware.

## Research hygiene
No executable code or hardware benchmark was run in this audit. All performance proposals remain hypotheses.
