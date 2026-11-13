# R48 — SRW locks and condition variables: process-local synchronization gates

Evidence date: 2026-11-13 UTC. **Primary Microsoft API documentation review only. No Linux or Windows experiment was executed.** This complements R3/R6/R18 (ntsync kernel objects), R33 (waitable timers), R39 (thread-pool lifetime), and R47 (last-error). Unlike named NT synchronization objects, these APIs explicitly describe process-local user-mode synchronization.

## VERIFIED FROM SOURCE — Windows API contracts

1. **SRWLOCK is pointer-sized**, can be statically initialized with `SRWLOCK_INIT`, and supports shared readers or one exclusive writer. It cannot be shared across processes. There is **no FIFO or fairness guarantee**, no supported shared-to-exclusive upgrade, and exclusive recursion is invalid; shared recursion is discouraged. [Microsoft: Slim Reader/Writer Locks](https://learn.microsoft.com/en-us/windows/win32/sync/slim-reader-writer--srw--locks).
2. `AcquireSRWLockExclusive` returns **void** rather than a success/failure status. Do not synthesize a failure code for ordinary contention. [Microsoft: AcquireSRWLockExclusive](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-acquiresrwlockexclusive).
3. **Condition variables are user-mode and process-local**; they work with critical sections or SRW locks. Sleep releases the associated lock atomically with entering the wait and reacquires it before returning. Wake-one and wake-all exist; **spurious and stolen wakeups are permitted**, so the caller must loop on its predicate. [Microsoft: Condition Variables](https://learn.microsoft.com/en-us/windows/win32/sync/condition-variables).
4. `SleepConditionVariableSRW` selects shared lock mode only with `CONDITION_VARIABLE_LOCKMODE_SHARED`; otherwise exclusive. A zero timeout is an immediate test, `INFINITE` has no timeout, and timeout returns `FALSE` with `GetLastError()==ERROR_TIMEOUT`. Calling it without holding the specified lock is undefined behavior. [Microsoft: SleepConditionVariableSRW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablesrw).

## Architecture implications — HYPOTHESES, not implementation findings

- Do **not** assume `/dev/ntsync` is required for SRW locks or condition variables. Their documented process-local scope makes a userspace atomic-state + wait/wake backend plausible; kernel blocking may still be useful under contention. The exact algorithm, bit encoding and wakeup protocol require independent design and tests.
- Do not expose a `pthread_rwlock_t` or `pthread_cond_t` directly as the guest `SRWLOCK` or `CONDITION_VARIABLE`: Windows requires pointer-sized guest storage and static initialization, while host type layout, lifetime and fairness are not ABI-compatible by contract. A side table is one candidate but needs teardown, reuse and race analysis.
- A guest implementation must preserve **atomic unlock-and-park** relative to wakeups. A naive sequence of unlock, then futex wait can lose a wake unless a generation/sequence protocol closes the race.
- Preserve the **shared/exclusive mode** across condition sleep and reacquisition. Condition-variable wake is not equivalent to a permanently signaled event; do not automatically route it through the named NT event object layer.
- R47's last-error discipline applies to `SleepConditionVariableSRW` timeouts. No claim is made about last-error changes on other outcomes beyond the documented contract.

## Proposed falsifiable conformance gates — NOT RUN

1. **Representation:** on a Windows x64 reference compile `static SRWLOCK s = SRWLOCK_INIT;` and `CONDITION_VARIABLE c = CONDITION_VARIABLE_INIT;`, capture `sizeof` and alignment, then test first use without an explicit init call. Record compiler, SDK, OS build and architecture.
2. **Shared/exclusive:** hold two shared acquisitions on distinct threads; demonstrate simultaneous readers, writer exclusion, and eventual writer entry after releases. Avoid testing invalid recursion/upgrade in the main test process, since deadlock is documented.
3. **Lost-wakeup stress:** for each of `SleepConditionVariableCS` and `SleepConditionVariableSRW` (shared and exclusive), race predicate publication and wake-one/wake-all over many iterations, with bounded watchdogs. Count timeouts and invariant failures; do not treat scheduling order as guaranteed.
4. **Timeout and state:** capture return, immediate `GetLastError`, and lock ownership after timeout; test zero, short finite and `INFINITE` with an independent waker. Verify both shared and exclusive mode on SRW waits.
5. **Backend differential:** run the same tests against a future Linux guest implementation. Separate functional correctness from contention benchmarks; include CPU affinity, thread count, wakeup rate, percentile latency and CPU use.
6. **Negative controls:** never rely on a particular fairness order or provoke undefined behavior as a required passing result. Test unsupported cross-process sharing only as a documented scope boundary, not as a promised failure mode.

## OPEN

- A correct pointer-sized in-place state representation versus an internal allocation/side table; address reuse and destruction races.
- Linux futex / futex2 backend choice, timeouts and cancellation behavior, and the mapping of Windows timeout results to guest last-error.
- Windows-reference results, cross-version behavior, and measured contention performance. None have been collected in this note.

**Conclusion:** source-level contracts are VERIFIED; proposed Linux implementation and tests are HYPOTHESES/OPEN. No application compatibility or performance gain is claimed.
