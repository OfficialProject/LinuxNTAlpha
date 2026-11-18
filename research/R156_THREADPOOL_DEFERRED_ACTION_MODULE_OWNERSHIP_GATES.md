# R156 — Threadpool deferred callback actions and DLL-reference ownership

Evidence: **VERIFIED FROM MICROSOFT PRIMARY DOCUMENTATION** for API descriptions; **HYPOTHESIS** for LinuxNTAlpha's proposed representation; **OPEN** for action ordering, failure/race semantics and Windows-reference conformance. Reviewed 2026-11-18. **No tests or benchmarks executed.**

## Distinct question

R155 establishes that `DisassociateCurrentThreadFromCallback` can release object-specific callback waiters before the callback returns, while cleanup-group association persists. This note examines the separate *deferred-on-return* API family and module-reference ownership. It does not duplicate R149–R155's scheduling or cancellation analysis.

## Directly verified contracts

- `FreeLibraryWhenCallbackReturns(instance, module)` specifies a DLL that the pool unloads **when the current callback completes**. Microsoft documents a void return, Windows Vista/Server 2008 minimum, and `threadpoolapiset.h`. This is a deferred module-release request, **not** proof that a callback can safely unload its own executing instructions early. [1]
- `SetEventWhenCallbackReturns(instance, event)` specifies an event to set when the callback completes. The documentation does not specify ordering relative to other deferred actions. [2]
- `ReleaseMutexWhenCallbackReturns(instance, mutex)` specifies a mutex to release when the callback completes. `LeaveCriticalSectionWhenCallbackReturns(instance, critical_section)` similarly defers leaving a critical section. Neither API page establishes an arbitrary callback thread may release an NT mutex owned by another thread; thread identity and ownership remain a design gate. [3][4]
- `SetThreadpoolCallbackLibrary(environment, module)` ensures a DLL remains loaded while outstanding callbacks exist. **Critically, Microsoft says the threadpool assumes ownership of the library reference passed to this function, and the caller must not subsequently call FreeLibrary on that handle.** The docs warn of loader-lock deadlock patterns and require appropriate outstanding-callback cleanup before unloading a DLL. It is an environment configuration function, not equivalent to the per-callback `FreeLibraryWhenCallbackReturns`. [5]
- Microsoft's threadpool guidance says worker threads can execute successive callbacks, and callbacks must restore TLS, security context, thread priority and COM registration state before returning. A Linux host worker's native state cannot automatically stand in for a Windows guest callback's state. [6]

## Architectural hypothesis, NOT a verified implementation

A callback-instance record should hold an explicit list of *deferred actions*, with separately typed references to guest event, mutex, critical-section and module objects. These references must remain valid until the actions are executed or a verified error/termination path handles them. A callback's **object-wait association**, **physical execution**, **deferred-action completion**, and **cleanup-group/module lifetime** are separate state transitions. Do not mark all four complete on disassociation.

For `SetThreadpoolCallbackLibrary`, model the supplied DLL reference as **transferred ownership**, not an untracked borrowed pointer. Do not automatically apply the same transfer rule to `FreeLibraryWhenCallbackReturns` without checking reference acquisition and reference-count behavior against a Windows test. Ensure deferred mutex/critical-section operations run with an appropriate guest thread identity; simply submitting them to an unrelated Linux worker risks incorrect ownership semantics.

**OPEN:** Microsoft's cited pages do not establish ordering between event signaling, mutex release, critical-section leave and library release when multiple actions are registered. They also do not define repeated registrations of the same type, handle closure races, callback exceptions, process termination, or interaction of deferred actions with early disassociation. Do not manufacture deterministic semantics from documentation silence.

## Falsifiable future gates — NOT EXECUTED

1. Windows reference: create a callback that registers each deferred action, signals an 'inside callback' barrier and blocks. Observe that the deferred event remains unset while the callback is blocked. Release it; measure event state and resource release with finite timeouts. Record Windows build and test source.
2. Repeat with `DisassociateCurrentThreadFromCallback`; separately wait on object callbacks and on the deferred event. Confirm which boundary returns first. Use safe synchronization to avoid racing resource destruction.
3. Hold a mutex and critical section on the callback's thread, register deferred release, and verify post-callback ownership with another thread. Negative control: try an ordinary callback with explicit releases. Do not infer support for cross-thread ownership transfer.
4. Load a disposable test DLL, obtain an owned reference, register the callback library, and instrument callback entry, return and DLL detach. Compare with per-callback deferred FreeLibrary in **separate** runs to avoid double release. Never call FreeLibrary on the transferred reference.
5. Use a Linux-only mock to inject disassociation, callback return, object close, cleanup-group close and deferred action scheduling in controlled orders. Instrument reference counts and check for premature free, double release and deadlock. A mock result would not by itself establish Windows conformance.

## Decision gate

**OPEN:** Do not implement callback disassociation and deferred actions using a single `running_callbacks` counter or a single generic module-handle pointer. Define reference-transfer rules and completion barriers first; verify observable behavior on a Windows reference host before promising equivalence. No performance inference follows.

## Primary sources

[1] Microsoft, FreeLibraryWhenCallbackReturns: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-freelibrarywhencallbackreturns

[2] Microsoft, SetEventWhenCallbackReturns: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-seteventwhencallbackreturns

[3] Microsoft, ReleaseMutexWhenCallbackReturns: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-releasemutexwhencallbackreturns

[4] Microsoft, LeaveCriticalSectionWhenCallbackReturns: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-leavecriticalsectionwhencallbackreturns

[5] Microsoft, SetThreadpoolCallbackLibrary: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadpoolcallbacklibrary

[6] Microsoft, Thread Pools: https://learn.microsoft.com/en-us/windows/win32/procthread/thread-pools
