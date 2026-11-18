# R169 — Windows INIT_ONCE retry, asynchronous races, and pthread_once mismatch

Status: PRIMARY-SOURCE DOCUMENTATION REVIEW (2026-11-18). No Windows differential experiment, Linux runtime test, implementation, or performance claim.

## Verified contracts

1. Windows one-time initialization is process-local and stores state in an opaque, immovable INIT_ONCE object; successful initialization can publish a context whose low reserved bits are zero. The API supports both synchronous and asynchronous modes, which cannot be mixed for one object. Microsoft: [One-Time Initialization](https://learn.microsoft.com/en-us/windows/win32/sync/one-time-initialization), [InitOnceBeginInitialize](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initoncebegininitialize).
2. In synchronous mode, competing initializers block while the current attempt runs. If that attempt fails, a subsequent contender may attempt initialization; success publishes the context for later callers. The callback form (`InitOnceExecuteOnce`) similarly permits retry after failure. Microsoft: [One-Time Initialization](https://learn.microsoft.com/en-us/windows/win32/sync/one-time-initialization).
3. `INIT_ONCE_CHECK_ONLY` does not start initialization; it reports whether completion has occurred and makes completed context retrievable. `InitOnceBeginInitialize` documents that the object must not be copied or moved. Microsoft: [InitOnceBeginInitialize](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initoncebegininitialize).
4. In asynchronous mode, several callers can begin attempts concurrently. Exactly one completion can win; losers must dispose of their speculative contexts and retrieve the winner's published context. `INIT_ONCE_INIT_FAILED` cannot be combined with `INIT_ONCE_ASYNC`; asynchronous failure is handled by abandoning the attempt without completion. Microsoft: [One-Time Initialization](https://learn.microsoft.com/en-us/windows/win32/sync/one-time-initialization), [InitOnceComplete](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initoncecomplete).
5. POSIX `pthread_once` invokes a void-returning initializer and subsequent callers do not rerun it after normal completion. POSIX specifies cancellation of an initializer at a cancellation point as leaving the control as though `pthread_once` had not been called; it does **not** provide Windows-style ordinary callback failure or asynchronous competing initializer publication. `pthread_once` itself is not a cancellation point. This is the POSIX contract, not evidence of any particular libc implementation. IEEE/The Open Group POSIX Programmer's Manual: [pthread_once(3p)](https://man7.org/linux/man-pages/man3/pthread_once.3p.html).

## Architectural inferences, not established implementation facts

- **HYPOTHESIS:** A direct one-to-one `INIT_ONCE` to `pthread_once` mapping fails observable synchronous retry-after-callback-failure and asynchronous winner/loser behavior. A guest-visible state machine with per-object publication, waiter coordination, retry and context alignment validation is a stronger candidate.
- **HYPOTHESIS:** An internal mutex/condition variable or atomic state plus futex could coordinate a synchronous state machine; a separate asynchronous completion-CAS path may publish the winner. Correct memory ordering and loser reclamation must be proved, not assumed.
- **OPEN:** Exact errors for invalid combinations, reentrant initialization, thread termination while pending, and racing check-only/completion should be verified against a pinned Windows build. The cited contracts do not license inventing these results.
- **OPEN:** A guest address reused after deallocation must not inherit stale host-side metadata. The opaque object lifetime and allocator integration need a documented policy.
- **SECURITY:** Context pointers are guest-controlled. Never dereference or treat them as host synchronization objects merely because the Windows API allows publishing a context.

## Falsifiable experiments proposed (NOT executed)

1. Windows reference: synchronous initializer fails once then succeeds, with multiple blocked contenders; record callback counts, return codes, contexts and scheduling traces.
2. Windows reference: asynchronous two-to-eight contender race with distinct aligned contexts; require exactly one published context and cleanup of every loser.
3. Windows reference: check-only before, during and after initialization; probe documented mode-mixing failures and invalid context alignment in isolated processes.
4. Linux prototype: model-check or exhaustively schedule begin/fail/retry/complete/check-only transitions, forced thread exits and guest object-address reuse. Confirm no lost wake or stale context.
5. Compare guest-visible outcomes to a version-pinned Windows oracle; only then benchmark uncontended fast paths, contention and memory overhead. Record Windows build, kernel, libc, compiler, CPU and sample variance.

## Decision

Keep INIT_ONCE in the userspace NT synchronization layer for now. Do not select pthread_once as the sole backend and do not propose a kernel modification without a demonstrated semantic or performance need.

Related: R161 critical sections; R168 condition variables; R143 process/thread termination. Sources checked 2026-11-18.
