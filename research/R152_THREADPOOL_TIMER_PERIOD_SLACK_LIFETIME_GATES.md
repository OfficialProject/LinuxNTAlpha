# R152 — Threadpool timer period, coalescing window and callback lifetime gates

Date: 2026-11-17. **Evidence: directly inspected Microsoft Learn API contracts; no Windows-reference run, Linux prototype or performance benchmark.**

## Distinct boundary

R33/R104 concern NT waitable timers and clock policy; R149 covers threadpool work, R150 threadpool waits, and the most recent cleanup-group note addresses group ownership. A **threadpool timer** queues worker callbacks; it is not itself a waitable timer handle and is not equivalent to Linux timerfd expiration counts. This note isolates periodic scheduling, coalescing and teardown.

## VERIFIED — published Microsoft contracts (not experimentally verified behavior)

1. [SetThreadpoolTimer](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpooltimer) replaces a previous setting. Non-NULL FILETIME due time arms the timer; NULL stops *new* callbacks, not callbacks already queued. Zero due time expires immediately. A negative FILETIME is relative; a nonnegative value represents an absolute UTC timestamp in 100-ns units since 1601-01-01.
2. The same API defines **msPeriod = 0** as one-shot and **msPeriod > 0** as periodic automatic reactivation until cancellation. **msWindowLength** permits the system to delay a callback to batch work for power conservation; it is not a promised precise callback deadline.
3. The SetThreadpoolTimer documentation explicitly distinguishes sleep/hibernation accounting: relative due times exclude sleep, absolute due times include it; an expiration during sleep is signaled after resume. This matters when choosing Linux clocks.
4. [SetThreadpoolTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpooltimerex) returns TRUE when a previously set timer was canceled; FALSE does **not** prove that no earlier callback is imminent. The documentation specifically warns that FALSE for a previously set timer can mean a callback is in progress or about to commence. Unlike the original function, Ex has a documented Windows 8 / Server 2012 minimum.
5. [WaitForThreadpoolTimerCallbacks](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-waitforthreadpooltimercallbacks) waits for outstanding callbacks and can cancel queued, not-yet-executing callbacks. It does not promise to terminate callbacks already executing.
6. [CloseThreadpoolTimer](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpooltimer) can defer reclamation until outstanding callbacks finish; callbacks may execute after Close returns. Its documented safe sequence is disarm with NULL due time and zero period/window, wait with cancellation TRUE, then close. Cleanup-group member closure is an alternative ownership route.
7. The SetThreadpoolTimer, WaitForThreadpoolTimerCallbacks and CloseThreadpoolTimer pages document Vista / Server 2008 minimums. This is a **documented API availability** distinction, not a test of particular binaries.

## HYPOTHESES — implementation candidates, not verified Windows internals

- **H1:** Track guest timer identity, arming generation, next nominal expiration, queued callback instances, running instances and cleanup-group references separately. A host timerfd/epoll backend can signal scheduling work, but must not be treated as the guest callback object or as evidence of one callback per host expiration count.
- **H2:** Preserve a distinct absolute-UTC versus relative-awake-time deadline model. Linux CLOCK_MONOTONIC and CLOCK_BOOTTIME differ across suspend; test the exact Linux clock and suspend behavior rather than assuming either matches every Windows timer mode.
- **H3:** Coalescing-window policy should be explicit, and initially conservative; do not claim that a Linux timer slack or timerfd automatically reproduces Windows power/coalescing policy.
- **H4:** Disarm and replacement should invalidate future scheduling for the old generation without freeing callback/context/module state used by queued or executing invocations. Ex's return is **not** a drain acknowledgment.
- **H5:** Worker callback entry needs the guest Microsoft ABI, TEB/PEB and per-thread state from R24/R25/R125/R149; pool reuse must not bypass loader/module lifetime.

## PROPOSED differential gates — NOT RUN

| Case | Record on pinned Windows build | Acceptance question |
| --- | --- | --- |
| One-shot due time zero, negative relative, positive absolute | Arm/return/callback times, counts and UTC changes | Are conversions and immediate expiry faithful? |
| Periodic callback intentionally blocks beyond period | Number queued/running, overlap and callback timestamps | Does guest runtime reproduce observed backlog policy without assuming timerfd expiration counts are callbacks? |
| Nonzero coalescing window | Distribution of callback start times under load/idle | Are documented delay bounds respected without inventing precision? |
| Suspend/resume with relative and absolute due times | Awake duration, wall time and callback after resume | Does guest deadline model distinguish sleep accounting? |
| SetThreadpoolTimerEx while callback starts | Return value, callback start/end IDs | Is FALSE correctly treated as ambiguous? |
| Disarm, wait(FALSE), wait(TRUE), close | Queued/running/completed counts and lifetime | Does cancel affect pending only; is teardown safe? |
| Timer callback closes or disarms its own timer | Return/hang/crash and ownership | Establish reference behavior before permitting reentrant teardown |
| Cleanup group closure with active periodic timer | Group/member callbacks and close ordering | No callbacks into reclaimed guest context or unloaded module |

For each case record Windows build, SDK version, timer period/window, time source, power state, callback IDs, concurrency barriers and uncertainty. Separate Windows observations from Linux-host tests and integrated PE fixture tests. Do not treat arbitrary scheduling jitter as a compatibility failure.

## OPEN

- The exact periodic backlog/coalescing behavior under blocked callbacks is **not** established by these API pages.
- Check [IsThreadpoolTimerSet](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-isthreadpooltimerset), callback disassociation and cleanup-group reentrancy before finalizing state transitions.
- Compare kernel clock availability and suspend accounting on the actual target Linux kernel; no kernel version or device capabilities were tested here.
- Inventory imported threadpool timer APIs in real candidate PE binaries before prioritizing implementation.

## Provenance

Primary Microsoft Learn pages linked inline were directly read on 2026-11-17. Living API documentation, not pinned implementation source. No experiment or benchmark was performed.
