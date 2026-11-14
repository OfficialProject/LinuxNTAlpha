# R75 — Wait-for-multiple-objects atomicity and Linux futex_waitv limits

Date: 2026-11-14. Status: **DOCUMENTATION-BASED ARCHITECTURAL ANALYSIS; NO EXPERIMENT**.

## Why this gate matters

R6/R13/R33/R48/R54 examine synchronization primitives and alertable waits, but a general NT wait dispatcher must also handle **heterogeneous waitable object types**, wait-any versus wait-all, object ownership and side effects, and abandonment. A Linux futex backend is not itself an NT wait dispatcher. This note focuses on the semantic boundary rather than asserting performance.

## Primary-source contracts

1. Microsoft [WaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects) specifies a maximum count, wait-any and wait-all behavior, return values identifying signaled/abandoned objects, and the requirement that wait-all waits until **all** objects are signaled. Its documentation explicitly says that for wait-all, modifications to object states do not occur until all conditions are satisfied. For wait-any with multiple signaled objects, the lowest-indexed eligible handle wins.
2. Microsoft [WaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjectsex) adds alertable waits and WAIT_IO_COMPLETION when an APC or I/O completion routine is dispatched. This is not equivalent to an ordinary EINTR result.
3. Microsoft [Synchronization Objects](https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-objects) distinguishes events, mutexes, semaphores, waitable timers, processes and threads, with object-specific signaling behavior.
4. Linux [futex_waitv(2)](https://man7.org/linux/man-pages/man2/futex_waitv.2.html) documents an array of futex waiters, returning an index for a futex that woke the caller; it does not document an NT-style transactional wait-all operation spanning arbitrary kernel objects.
5. Linux [futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html) documents atomic compare-and-block for a **single futex word**, not an atomic multi-object acquisition protocol.
6. Linux [epoll(7)](https://man7.org/linux/man-pages/man7/epoll.7.html) describes readiness notification on monitored file descriptors. Readiness is not an ownership transfer or an atomic reservation of multiple NT dispatcher objects.

These are interface-level references, not a claim that current Linux source or Windows behavior was experimentally inspected on 2026-11-14.

## Verified from the documented contracts

- **Wait-any ordering is observable.** An implementation that returns whichever Linux wake event happens to arrive first can violate the documented lowest-index rule when multiple handles are already signaled.
- **Wait-all cannot be implemented as sequential destructive waits.** Consuming an auto-reset event or semaphore token before verifying the entire set violates the documented defer-state-modification rule; the operation needs coordinated eligibility testing and commit.
- **Mutex abandonment is not a generic wake event.** The Windows return contract distinguishes abandoned ownership, and a correct dispatcher must preserve that distinction.
- **Alertable waits need a separate dispatch path.** An APC completion can terminate the wait with a distinct status and user-mode callback obligations, not merely wake a futex.
- **A futex_waitv array is not a general wait-all implementation.** It waits on futex values; it does not provide atomic acquisition across events, semaphores, mutexes, processes, and threads.

## Architectural hypothesis (not validated)

Maintain a runtime-owned **dispatcher transaction domain** for NT-compatible waitable objects. Under a well-defined serialization/locking protocol, evaluate every object's eligibility, select the lowest-index eligible object for wait-any, or commit all relevant state changes together for wait-all. Linux futex/ntsync/poll facilities may supply sleeping and wakeup, but the NT-visible decision and state transition must be proved equivalent. Whether Linux ntsync can directly cover a subset of these semantics should be established from the **exact kernel UAPI and tested version**, not assumed.

A process-local mutex alone is insufficient if waitable objects are shared across processes. A single global lock may establish correctness for a prototype but could introduce unacceptable contention; no performance conclusion is possible without measurement.

## Conformance matrix to run on a Windows reference host

1. **Ordering:** two already-signaled manual-reset events in reversed signaling order; wait-any must select the lowest handle index.
2. **Deferred consumption:** wait-all on a signaled auto-reset event plus an unsignaled event with zero timeout; the first event must remain consumable afterward.
3. **Mixed types:** wait-all on semaphore plus event; confirm the semaphore count changes only when the whole set becomes eligible.
4. **Abandonment:** a thread owns a mutex and terminates; test wait-any and wait-all abandoned-status mapping and acquired ownership.
5. **Duplicate handles:** same underlying object appears twice; document the reference behavior and enforce the API's documented invalid/undefined cases rather than guessing.
6. **Concurrent waiters:** two processes race on the same semaphore/event set; assert no token double-consumption and no missed wakeup.
7. **Alertable case:** queue an APC to a thread in WaitForMultipleObjectsEx and check callback execution, status and remaining object state.
8. **Timeout edges:** zero timeout, finite timeout, signal/timeout races, and handle close during wait. Treat documented undefined cases as excluded rather than constructing a false golden oracle.

Each reference case needs OS build, architecture, source, return code, GetLastError where relevant, final object states, repeated trials and timing-independent assertions. Linux prototypes should compare outcomes, not just elapsed time.

## Acceptance and stop conditions

- Build a deterministic **model-only** wait dispatcher test suite before integrating host futex or ntsync wake mechanisms.
- Reject implementations that consume state during a failed wait-all, violate wait-any index selection, or lose cross-process wakeups.
- Record Windows-reference behavior before claiming compatibility for abandoned/alertable/duplicate-handle edge cases.
- Do not infer that futex_waitv, epoll, or ntsync alone solves the complete NT dispatcher contract.

## Relationship to existing research

R6/R7/R18 address ntsync UAPI/versioning; R13 addresses APC; R33 waitable timers; R54 address-based waits; R73/R74 process/thread termination. R75 specifically records the **cross-object atomicity and selection** gate that joins these pieces. No code was run, no benchmark was performed, and no kernel architecture decision is made here.
