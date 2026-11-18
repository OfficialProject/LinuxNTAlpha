# R151 — Threadpool cleanup-group membership and teardown transaction gates

Research date: 2026-11-17. Scope: documented Windows Vista+ threadpool API contract, not NT kernel internals. **No experiment was run.** This note extends R149/R150 from individual work/wait lifetime to group-wide ownership.

## Verified from primary Microsoft documentation

1. `SetThreadpoolCallbackCleanupGroup` associates a cleanup group and optional cleanup callback with a callback environment. Microsoft documents it as an inline function requiring `_WIN32_WINNT >= 0x0600`; environment association alone is not group membership. [Microsoft: SetThreadpoolCallbackCleanupGroup](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadpoolcallbackcleanupgroup).
2. A group acquires members through creation of threadpool I/O, timer, wait, or work objects with the associated callback environment; corresponding close functions remove members. [Microsoft: CreateThreadpoolCleanupGroup](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolcleanupgroup).
3. `CloseThreadpoolCleanupGroupMembers(group, cancel, context)` releases member objects and blocks for executing callbacks. With `cancel=TRUE`, not-yet-started callbacks are canceled; with `FALSE`, pending callbacks are allowed to finish. It does **not** close the group or destroy the callback environment. Previously released members must not be individually closed again. [Microsoft: CloseThreadpoolCleanupGroupMembers](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolcleanupgroupmembers).
4. Microsoft explicitly warns that concurrent member creation and group cleanup require application synchronization: an object created near cleanup can either be released with the old set or survive as a new member; use of a released object can raise an exception. Thus a guest runtime must not invent a promise that every racing creation is included in the same cleanup operation. [Microsoft: CloseThreadpoolCleanupGroupMembers](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolcleanupgroupmembers).
5. The published sample associates work, wait and timer objects with a custom pool and cleanup group, showing that group ownership spans distinct callback sources. [Microsoft: Using the Thread Pool Functions](https://learn.microsoft.com/en-us/windows/win32/procthread/using-the-thread-pool-functions).

## Architectural hypothesis (not verified)

Represent a cleanup group as a guest-lifetime object holding *membership references* to callback objects. Keep callback execution references separately. A group-close-members transaction should snapshot/claim a membership generation, prevent claimed objects from being resubmitted, optionally cancel queued-but-not-running invocations, await claimed in-flight callbacks, invoke documented cleanup callbacks under their appropriate semantics, and release each claimed member exactly once. Retain the group and callback environment independently. **Do not** equate a host pthread-pool shutdown or Linux fd close with this Windows contract.

This is an implementation proposal, not a proven ordering guarantee. In particular, the Microsoft documentation does not specify a globally atomic membership snapshot or ordering of cleanup callbacks relative to each other. The proposed generation/claim scheme is internal bookkeeping, not a claim about Windows' implementation.

## Conformance gates / falsification plan (OPEN)

- Windows reference: create two work objects, a wait and a timer in one group; queue multiple callbacks, hold one running on a barrier, then close members with cancel TRUE and FALSE in separate runs. Record started/completed/canceled counts, cleanup callback invocations, and whether close returns before held work exits.
- Reuse: close members, then create another object using the *same* group and environment; verify it can be submitted and cleaned in a later call. Destroy environment and group only after all members have drained.
- Race: orchestrate object creation before/during/after cleanup with barriers. Observe possible outcomes without relying on undefined use-after-close; document which operations require application-side synchronization.
- Mixed ownership: explicitly close one member before group cleanup, then verify group cleanup does not double-release it. Track outstanding callback references separately from membership.
- Host prototype: run with sanitizers and fault injection for cancellation/close races. Assert no UAF, double free, or callback after a successfully completed group drain. Such host tests are *internal safety checks*, not Windows conformance.
- Record Windows build, SDK, compiler, exact code, event ordering, repetitions and raw logs before promoting any hypothesis to reproduced behavior.

## Relation to earlier work

R149 covers work callback submission/teardown; R150 covers wait registration. This note isolates the **cross-object group membership, reuse, cancellation and ownership transaction** missing from single-object lifecycle models. It does not demonstrate compatibility, correctness or performance.
