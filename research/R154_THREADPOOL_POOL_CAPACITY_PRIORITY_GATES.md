# R154 — Threadpool pool capacity, callback priority and asynchronous pool close

Evidence status: **VERIFIED documentation contracts**, **HYPOTHESIS implementation design**, **OPEN differential behavior**. Reviewed 2026-11-18. No code executed and no measurements collected.

## Scope and separation from prior work

R149–R153 cover work-item callback lifetime, waits, timers, I/O and cleanup-group ownership. This note isolates **pool-wide capacity, callback-environment routing, priority ordering, and pool-close lifetime**, which must not be conflated with individual callback cancellation or cleanup-group release.

## Verified Microsoft contracts

1. `CreateThreadpool(NULL)` creates a pool; a callback environment initialized with `InitializeThreadpoolEnvironment` can be associated with that pool through `SetThreadpoolCallbackPool`. Without a specified pool, callbacks use the process default pool. Sources [1], [2], [3].
2. `SetThreadpoolThreadMinimum` returns BOOL and may fail; `SetThreadpoolThreadMaximum` returns VOID. The former specifies threads made available to process callbacks, the latter the maximum allocated for callbacks. Do **not** assume changing a maximum proves an immediate resize, or that all requested minima succeed. Sources [4], [5].
3. `SetThreadpoolCallbackPriority` sets **relative priority within the same pool**, with HIGH, NORMAL and LOW values. Higher-priority pending callbacks are guaranteed to run first on the first available worker; **completion before lower-priority callbacks is not guaranteed**. This is not a contract for Linux scheduler priority or preemption. The API requires Windows 7 or later; earlier Vista pool APIs do not imply this extension. Source [6].
4. `CloseThreadpool` closes immediately only if no outstanding work, I/O, timer or wait objects remain bound; otherwise pool release is asynchronous after those objects are freed. Consequently, close is **not** documented as a universal synchronous callback-drain primitive. Source [7].
5. The default callback environment assigns the process default pool, no cleanup group, and leaves outstanding-callback DLL lifetime with the caller. Source [2].

## Architectural hypothesis (not implementation evidence)

Represent pool identity independently from Linux worker threads and from callback-object identity. Keep per-pool admission queues with explicit relative-priority classes; retain a pool reference from each bound work/timer/wait/I/O object until its documented release. A pool-close operation should prevent new ownership acquisition while existing object references remain valid; the precise handling of racing creation/close is **OPEN** pending Windows-reference probes. Keep callback priority separate from host `nice`, `sched_setscheduler` or cgroup CPU weight: none directly implements the documented *intra-pool pending callback ordering*. Avoid promising strict ordering between callbacks that become runnable simultaneously without a controlled reference test.

Capacity configuration is an API-level constraint, not evidence that an arbitrary Linux threadpool has equivalent Windows starvation, blocking, worker injection or minimum-thread behavior. Host thread counts and callback start times must be measured rather than inferred from the configured values.

## Proposed falsifiable gates (not performed)

- **P1 routing:** create two custom pools and one default environment; submit distinguishable callbacks. Verify pool routing using concurrency bounds, not guessed worker thread IDs; test that environment configuration is captured at object creation rather than assuming later mutations apply.
- **P2 capacity:** run blocking and short callbacks with max=1 versus max=2 and successful min configurations. Record actual concurrent callback counts, startup latencies, failure codes and Windows build. Include a separate test for unsuccessful minimum allocation; do not induce system resource exhaustion on a shared host.
- **P3 priority:** queue LOW then HIGH while the only worker is occupied, release it, and record start order over repetitions. Repeat for NORMAL and cross-pool queues; do not assert cross-pool ordering. Distinguish start order from finish order.
- **P4 close:** create a bound work object, call `CloseThreadpool`, then release the work object following documented lifetime rules. Instrument completion and cleanup; never use a freed pool pointer or infer synchronous drain from the close return.
- **P5 concurrency/security:** stress object-creation versus close, pool refcount and guest process teardown with sanitizers and a separate Windows reference implementation. If Windows behavior is undocumented, classify as OPEN rather than inventing a guarantee.
- **P6 versioning:** pin Windows 7+ for callback-priority probes and Vista+ for basic pool APIs; record Windows build, CPU topology, scheduling policy and environment.

## Decision implications

**VERIFIED:** callback priority is a queue-ordering concern, pool close can be asynchronous, and configured minimum and maximum have different failure surfaces. **HYPOTHESIS:** a Linux-hosted dispatcher with explicit pool objects, reference ownership and priority queues can preserve these contracts without kernel changes. **OPEN:** reference-accurate fairness, worker injection, racing close semantics, and performance. No claim of full NT compatibility or speed advantage follows.

## Primary sources

[1] Microsoft, CreateThreadpool: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpool

[2] Microsoft, InitializeThreadpoolEnvironment: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-initializethreadpoolenvironment

[3] Microsoft, SetThreadpoolCallbackPool: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadpoolcallbackpool

[4] Microsoft, SetThreadpoolThreadMinimum: https://learn.microsoft.com/en-us/windows/desktop/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolthreadminimum

[5] Microsoft, SetThreadpoolThreadMaximum: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolthreadmaximum

[6] Microsoft, SetThreadpoolCallbackPriority: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadpoolcallbackpriority

[7] Microsoft, CloseThreadpool: https://learn.microsoft.com/en-us/windows/desktop/api/threadpoolapiset/nf-threadpoolapiset-closethreadpool
