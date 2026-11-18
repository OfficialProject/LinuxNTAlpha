# R155 — Threadpool callback disassociation, deferred actions and DLL lifetime

Status: **VERIFIED FROM PRIMARY DOCUMENTATION** for Microsoft contracts; **HYPOTHESIS** for proposed LinuxNTAlpha design; **OPEN** for Windows-reference behavior. Reviewed 2026-11-18. **No experiment, implementation or benchmark was run.**

## Scope / nonduplication

R149–R154 cover callback-object scheduling, queue cancellation, cleanup groups and pool capacity. R153 explicitly leaves callback disassociation and DLL pinning open. This note focuses on **two distinct completion boundaries**: the callback's association with its initiating object and the actual return of the executing function, including actions deferred until return. Neither boundary can be replaced by a simple Linux worker-task completion counter.

## Verified Microsoft API contracts

1. `DisassociateCurrentThreadFromCallback(instance)` removes the currently executing callback's association with the initiating object. If it was the last associated callback, waiters for that object's callbacks can be released. **The thread remains associated with the cleanup group until it returns to the pool**; this is specifically intended to allow DLL shutdown to synchronize with outstanding callbacks and avoid unloading executing code. Microsoft also says the callback-generating object remains valid for the duration of the callback, while object reuse/release may occur after disassociation subject to cleanup-group synchronization. Source: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-disassociatecurrentthreadfromcallback
2. `CallbackMayRunLong(instance)` always marks the callback as long-running, even when it returns FALSE. TRUE means another worker is available or was created; FALSE does **not** mean the call failed to mark it. It must be called by the executing callback thread and only once. This is a capacity hint, not an ownership-release operation. Source: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-callbackmayrunlong
3. `SetThreadpoolCallbackRunsLong(environment)` indicates in advance that callbacks using the environment may run long; Microsoft describes it as a scheduling hint and an inline API. It does not state that long-running callbacks are automatically disassociated. Source: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadpoolcallbackrunslong
4. Microsoft's threadpool documentation warns that a worker thread can execute successive callbacks and that the modern pool does **not** automatically impersonate the registering thread. A guest scheduler must explicitly manage callback-specific security context rather than treating the Linux worker's current credentials as the guest callback token. Source: https://learn.microsoft.com/en-us/windows/win32/procthread/thread-pools
5. `CloseThreadpoolCleanupGroupMembers` releases group members and waits for running callbacks; cancellation applies to callbacks not yet started. This aggregate ownership boundary is distinct from object-specific callback disassociation. Source: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolcleanupgroupmembers

## Architectural hypothesis — separate lifetime counters

For each callback instance, maintain (a) a **callback-object association** token, which disassociation may release once, (b) a **physical execution** token, held until the callback actually returns and deferred actions complete, and (c) a **cleanup-group / module-lifetime** token, whose release must follow the documented group and DLL rules. Keep the callback-instance record alive while any of these references remain. Model disassociation as a one-way state transition; never infer that callback code has returned merely because object-specific waiters have been released.

The guest callback must enter with a defined NT-compatible thread context (TEB, last error, effective impersonation token, ABI and exception handling) and restore the worker's host/guest context afterward. This is a design requirement to test, **not** evidence that a Linux worker can already execute Windows callbacks.

**OPEN:** The exact ordering and interaction of `FreeLibraryWhenCallbackReturns`, `SetThreadpoolCallbackLibrary`, `SetEventWhenCallbackReturns` and disassociation require additional API-page and Windows-reference review. Do not promise a particular ordering or pinning implementation from the five contracts above.

## Proposed differential gates — NOT EXECUTED

1. On a Windows reference machine, create a work object in a cleanup group. Have its callback signal *entered*, call `DisassociateCurrentThreadFromCallback`, signal *disassociated*, then block on an event. On another thread wait for the object's callbacks and separately attempt cleanup-group-member close. Record which wait returns before the callback is released. Use finite timeouts and never unload callback code prematurely.
2. Repeat without disassociation; distinguish the two wait boundaries. Run separate tests for queued versus already-executing callbacks and cancellation of pending callbacks.
3. Exercise `CallbackMayRunLong` with constrained pool capacity; record its return and subsequent callback scheduling without assuming TRUE/FALSE guarantees future thread availability.
4. Load callback code from a test DLL and instrument its detach. Compare object-specific wait, cleanup-group close and actual callback return; verify that the module cannot be reclaimed while executing its code under the documented supported lifetime protocol. Use a disposable process for intentionally invalid sequences.
5. In an isolated Linux mock scheduler, implement association and execution counters separately. Add deterministic barriers around disassociation, callback return, group close, object close and module release; require no use-after-free or premature unload. This is a **future test**, not a result.

Record exact Windows build, compiler, test binaries, kernel/toolchain revisions for Linux comparisons, trace timestamps, timeout outcomes and PASS/FAIL/SKIP. Undefined races must not be promoted to compatibility requirements.

## Decision gate

**OPEN:** Before implementing Windows threadpool APIs, define an explicit callback-instance ownership protocol and a guest module pinning strategy. A single `running_callbacks` counter is insufficient to express the documented disassociation versus cleanup-group distinction. No performance conclusion follows.

## Sources

- Microsoft, DisassociateCurrentThreadFromCallback: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-disassociatecurrentthreadfromcallback
- Microsoft, CallbackMayRunLong: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-callbackmayrunlong
- Microsoft, SetThreadpoolCallbackRunsLong: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadpoolcallbackrunslong
- Microsoft, Thread Pools: https://learn.microsoft.com/en-us/windows/win32/procthread/thread-pools
- Microsoft, CloseThreadpoolCleanupGroupMembers: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolcleanupgroupmembers
