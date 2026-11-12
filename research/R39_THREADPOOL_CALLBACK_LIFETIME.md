# R39 — Windows thread-pool callback lifetime and cleanup-group gates

Status: **documentation review only** (2026-11-12). No Windows-reference experiment, Linux backend, or performance result.

## Verified documented behavior

1. The Windows thread-pool API is object-based: pools, work objects, waits, timers, I/O objects, and cleanup groups are distinct user-mode objects. A work object may have multiple outstanding posts, each generating a callback. [Microsoft: Thread Pool API](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-pool-api).
2. `SetThreadpoolWait` associates **one** waitable handle with a wait object and replaces any previous association. A NULL handle prevents **new** callbacks but does not prevent already queued callbacks. The wait must be re-registered to trigger a subsequent callback. [Microsoft: SetThreadpoolWait](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolwait).
3. `CloseThreadpoolWait` by itself does **not** guarantee that callbacks cannot execute afterward. Microsoft prescribes disarming the wait, waiting for/canceling outstanding callbacks, then closing it when that guarantee is required. [Microsoft: CloseThreadpoolWait](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolwait).
4. `CloseThreadpoolCleanupGroupMembers(group, TRUE, context)` cancels callbacks not yet started and blocks for running callbacks; with FALSE it waits for pending callbacks as well. It releases the member objects, **not** the cleanup group itself. Concurrent object creation against cleanup is explicitly hazardous. [Microsoft: CloseThreadpoolCleanupGroupMembers](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolcleanupgroupmembers).
5. A cleanup group can contain work, wait, timer, and I/O callback objects. The group must be empty before `CloseThreadpoolCleanupGroup`. [Microsoft: CreateThreadpoolCleanupGroup](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolcleanupgroup); [CloseThreadpoolCleanupGroup](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolcleanupgroup).
6. The default callback environment does not promise persistent worker-thread identity across callbacks. Microsoft's `CreateThreadpoolWait` documentation specifically warns against APIs needing persistent thread identity, including registry notifications without `REG_NOTIFY_THREAD_AGNOSTIC`. [Microsoft: CreateThreadpoolWait](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolwait).

## Architectural implications — hypotheses, not established implementation facts

- **HYPOTHESIS:** A Linux pthread pool plus a dispatch queue may implement work callbacks, but cannot alone reproduce Windows callback-object cancellation, wait rearming, and cleanup-group lifetime. A separate state machine and reference ownership model are needed.
- **HYPOTHESIS:** `epoll`/eventfd/timerfd or other Linux mechanisms may serve as backend wakeup sources. They do not automatically establish Windows object-manager wait semantics (R10/R13), timer behavior (R33), or I/O completion behavior (R15).
- **OPEN:** Which NT thread-pool exports and Win32 entry points are necessary for the first targeted executable? No binary import census exists here.
- **OPEN:** How callback cancellation interacts with reentrant cleanup, callback-instance disassociation, and DLL unload. These require separate primary-source review and Windows-reference tests.

## Proposed conformance matrix (not executed)

On a Windows reference host, with a fixed OS build and test harness:

1. Post a work object N times; assert N invocations and correct completion barrier.
2. Register one wait; signal once; verify callback; signal again without rearming; then rearm and verify another callback.
3. Disarm a wait while a callback is already queued; observe queued versus running callbacks under a controlled barrier.
4. Close a wait without draining; demonstrate why post-close callback use is unsafe **without** dereferencing freed memory; compare the prescribed disarm/drain/close sequence.
5. With blocked running callback and queued callback, test cleanup-group cancellation TRUE and FALSE separately; assert return only after required callbacks finish, and that member objects are no longer usable.
6. Test simultaneous group membership creation/cleanup only in a contained stress harness, with documented synchronization and no assumption of deterministic ordering.
7. Verify callback execution under two distinct callback environments/pools; record thread IDs but do **not** assert fixed identity.

Record exact Windows build, CPU, iteration counts, scheduling/barrier logic, timeout limits, outcomes and failures. Reproduce with a Linux implementation **only after** reference observations. Do not infer latency advantage from an API mapping.

## Integration boundary

Prioritize callback-object lifecycle correctness before optimization. A backend must own explicit states (created, armed/queued, executing, disarmed, draining, released), distinguish queued from running callbacks, and prevent use-after-release. These states are a **design proposal**, not a claim about Microsoft's internal implementation. Cross-check R13 (alertable waits), R15 (I/O completion), R33 (timers), and R38 (registry notifications).

Sources accessed 2026-11-12; documentation URLs above are the primary evidence. No source code imported.
