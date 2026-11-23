# R257 — Legacy timer-queue callback lifetime and Linux timer backend gates

Research date: 2026-11-23. Status: **documentation review only**. No Windows differential test, Linux prototype, timing measurement, or implementation is claimed.

## Scope and nonduplication

R33 covers NT waitable timer objects; R39 covers modern thread-pool callback cleanup; R215 covers fibers. This note isolates the **legacy Win32 timer-queue** family (`CreateTimerQueueTimer`, `ChangeTimerQueueTimer`, `DeleteTimerQueueTimer`, `DeleteTimerQueueEx`). It must not be collapsed into waitable timer signaling or a generic Linux timerfd callback.

## Verified primary contracts

1. `CreateTimerQueue` returns a timer-queue handle usable by timer-queue functions; `CreateTimerQueueTimer` can use the default queue when the queue argument is NULL. Timers invoke callbacks through the Windows thread pool; callback scheduling can delay observed firing. [Microsoft CreateTimerQueue](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-createtimerqueue), [CreateTimerQueueTimer](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-createtimerqueuetimer).
2. `ChangeTimerQueueTimer` can be called inside a timer callback. An already-expired one-shot timer cannot be updated; a zero period is one-shot, nonzero is periodic. Microsoft warns not to call Change after Delete on the handle. [Microsoft ChangeTimerQueueTimer](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-changetimerqueuetimer).
3. `DeleteTimerQueueTimer` supports three distinct completion modes: NULL marks deletion without waiting; an event handle signals after cancellation and callbacks finish; `INVALID_HANDLE_VALUE` waits synchronously. With NULL and outstanding callbacks, FALSE/`ERROR_IO_PENDING` means deletion is underway, **not** that the caller should retry. Blocking deletion of one's own timer is prohibited; mutual blocking deletion and persistent-thread callbacks introduce deadlock risks. Microsoft states impersonating callers have undefined behavior. [Microsoft DeleteTimerQueueTimer](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-deletetimerqueuetimer).
4. `DeleteTimerQueueEx` cancels all pending timers and supports completion-event versus synchronous-wait modes. The older `DeleteTimerQueue` is obsolete and does **not** wait for callbacks. [Microsoft DeleteTimerQueueEx](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-deletetimerqueueex), [DeleteTimerQueue](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-deletetimerqueue).
5. Microsoft documents that modern timer-queue due-time accounting excludes sleep/hibernation, with a Windows XP/Server 2003 exception. This is a version-specific contract, not a universal monotonic-clock equivalence. [Microsoft CreateTimerQueueTimer](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-createtimerqueuetimer).

## Architecture hypothesis (not verified)

Represent guest queue and timer identities independently of host timerfd/worker descriptors. Track `ACTIVE -> DELETE_REQUESTED -> QUIESCENT -> RECLAIMED`, outstanding callback references, completion event and callback execution context under one synchronized ownership protocol. A host timer source could schedule work, but **must not** itself determine guest deletion completion. Prevent handle reuse from confusing callbacks with later timer generations. Avoid holding a global queue lock while waiting for guest callbacks. This is a candidate design, not demonstrated equivalence.

## Falsifiable gates

- **Windows reference:** one-shot/periodic expiry; update before/after one-shot firing; change from inside callback; callback overlap and scheduling variance; default versus explicit queue.
- **Deletion:** delete before expiry, while callback blocked, and after callback; capture return/last-error plus event signaling for NULL/event/`INVALID_HANDLE_VALUE`; queue deletion with multiple callbacks. Test deadlock-prone self-delete in a disposable subprocess with watchdog, not the primary test runner.
- **Lifetime:** race update/delete/queue-delete, duplicate or stale handle use, callback reentrancy, timer generation reuse, callback exception, and process shutdown. Record version/build, CPU, timer resolution and callback timing separately.
- **Host backend:** test chosen clock's suspend accounting, wakeup behavior, cancellation races and worker-pool starvation. These tests cannot establish Windows compatibility without reference comparisons.
- **Security:** do not let guest callback pointers execute on a privileged broker thread; ensure guest callback context, ABI, exception routing and process isolation before enabling callbacks.

## Open questions

Exact callback overlap and timing tolerance across supported Windows versions; documented flag-specific dispatch behavior; behavior of queue deletion initiated from its own callback; interaction with guest thread impersonation (Microsoft explicitly labels some calls undefined). Do not fill these gaps with assumptions.

## Decision impact

Legacy timer queues need a callback-lifetime subsystem with observable completion semantics. A simple Linux timerfd-to-callback mapping is insufficient by itself. No kernel change is justified by this documentation alone.
