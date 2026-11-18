# R168 — Windows condition-variable lock-mode and host pthread boundaries

Status: DOCUMENTATION REVIEW ONLY (2026-11-18). No implementation, Windows differential run, Linux benchmark, or compatibility result is claimed.

## Verified primary-source contracts

1. Windows condition variables are user-mode, **process-local** objects. A sleep atomically releases its associated critical section or slim reader/writer (SRW) lock and enters the wait; after waking the caller reacquires that lock. Wake-one and wake-all exist; spurious and stolen wakeups require a predicate loop. Source: Microsoft, *Condition Variables*, https://learn.microsoft.com/en-us/windows/win32/sync/condition-variables (page updated 2021-01-07; checked 2026-11-18).
2. `SleepConditionVariableCS` requires that the caller enter the critical section **exactly once**. On timeout it reacquires the critical section before returning FALSE with `ERROR_TIMEOUT`. Zero and `INFINITE` have documented special meanings. Source: Microsoft, https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablecs (page updated 2025-10-31; checked 2026-11-18).
3. `SleepConditionVariableSRW` requires the SRW lock held in the mode specified by `CONDITION_VARIABLE_LOCKMODE_SHARED` (otherwise exclusive), releases/reacquires it around the sleep, and documents undefined behavior for an unlocked lock. Timeout returns FALSE/`ERROR_TIMEOUT`. Source: Microsoft, https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablesrw (page updated 2025-10-31; checked 2026-11-18).
4. POSIX `pthread_cond_wait` atomically unlocks/relocks a **pthread mutex**, uses an associated mutex binding while waiters remain, allows spurious wakeups, and is a cancellation point. `pthread_cond_timedwait` uses an absolute deadline in its configured clock domain. This is a POSIX contract, **not** proof of a specific Linux/glibc implementation. Source: POSIX Programmer's Manual as hosted at https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html (checked 2026-11-18).

## Architectural consequences (inferences, not implementation facts)

- **HYPOTHESIS:** A host pthread condition variable plus mutex could support the critical-section path only if guest critical-section ownership, recursion-depth precondition, guest timeout/error translation and atomic unlock/register/relock are preserved. An arbitrary host mutex must not silently replace the guest critical-section state machine (see R161).
- **HYPOTHESIS:** The SRW shared-mode path needs a distinct unlock-and-sleep protocol; pthread_cond_wait accepts a mutex, not a pthread rwlock. A naive release-rwlock then cond-wait sequence can lose a wake between release and waiter publication. Use an explicit registration/sequence handshake or another proved protocol, and validate both lock modes.
- **OPEN:** Whether one condition-variable address may be used concurrently with different guest locks/modes under Windows is not established by the cited Microsoft pages. POSIX's dynamic single-mutex binding must **not** be imposed on guests without Windows-reference evidence.
- **OPEN:** Exact race outcomes between timeout, wake-one, wake-all and lock reacquisition; no fairness or FIFO guarantee is inferred from these documents.
- **OPEN:** Host cancellation must not accidentally escape into guest Windows API behavior merely because POSIX waits are cancellation points. Thread termination and runtime teardown require an independent policy (see R143).

## Proposed falsifiable gates (not executed)

1. Windows reference: two threads share a condition variable and critical section; verify atomic release/reacquire, predicate-loop correctness, timeout code, zero-timeout and INFINITE behavior.
2. Windows reference: repeat with SRW exclusive and shared modes; verify lock mode on return, simultaneous readers, writer contention and wake-one/all.
3. Linux prototype: force scheduling at the release-to-registration boundary for each backend; verify no lost wake using bounded watchdogs and traceable sequence counters.
4. Windows reference: probe multiple guest locks with one condition-variable address **only in an isolated disposable process**, because documentation does not establish its validity; classify observed behavior as version-specific, not a universal guarantee.
5. Test timeout/wake races, spurious wakes, address reuse and teardown with randomized interleavings; separately compare guest-observable return codes against pinned Windows build.
6. Measure uncontended and contended paths only after correctness gates pass; record Windows build, Linux kernel, libc version, CPU, compiler, repetitions and variance.

## Decision impact

No kernel change is justified by this documentation alone. Favor a userspace semantic adapter for evaluation, with a separate SRW-mode-aware protocol. Keep the exact backend and any performance claim OPEN until reproduced experiments exist.

## Related research

R54/R97 (WaitOnAddress), R95/R161 (critical sections), R143 (termination), R166 (keyed events). These are different primitives; their wake guarantees must not be conflated.
