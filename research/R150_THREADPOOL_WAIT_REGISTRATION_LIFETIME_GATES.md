# R150 — Threadpool wait registration, timeout and callback teardown gates

Date: 2026-11-17. **Status: Microsoft primary API documentation inspected; no Windows reference test, Linux prototype or performance measurement.**

## Boundary

R149 covers posted threadpool **work** callbacks. This note covers the distinct **wait registration** object, which observes one waitable handle at a time and queues a callback on signaling or timeout. It does not establish a general NT wait dispatcher, mutex support, or timer-object equivalence.

## VERIFIED — documented Microsoft interface contracts

1. [CreateThreadpoolWait](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolwait) creates an opaque wait object associated with a callback, caller context and optional callback environment. Creation does not itself register a handle; the application subsequently calls SetThreadpoolWait.
2. [SetThreadpoolWait](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolwait) replaces any prior handle. Each wait object observes **one** handle; a new registration is required to trigger another callback after a completed registration. A NULL handle stops queuing **new** callbacks but does not prevent already queued callbacks from running.
3. SetThreadpoolWait requires a valid waitable handle when non-NULL, explicitly **does not support mutex handles**, and documents a STATUS_THREADPOOL_HANDLE_EXCEPTION for that misuse. Closing the observed handle while its wait is pending yields **undefined behavior**; the documented safe lifecycle cancels the wait before closing the handle.
4. SetThreadpoolWait accepts FILETIME-based deadlines: positive values are absolute UTC timestamps in 100-ns units since 1601-01-01, negative values are relative durations, zero means immediate timeout, and NULL means no timeout. This must not be silently replaced by a Linux monotonic-only relative timer.
5. [WaitForThreadpoolWaitCallbacks](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-waitforthreadpoolwaitcallbacks) waits for outstanding callbacks and may cancel **queued callbacks that have not started**, but does not promise to interrupt executing callbacks.
6. [CloseThreadpoolWait](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolwait) can release the object asynchronously after outstanding callbacks finish. Its documented prevention sequence is SetThreadpoolWait(NULL), WaitForThreadpoolWaitCallbacks(TRUE), then CloseThreadpoolWait. Cleanup-group closure is another documented ownership path.
7. The documented API minimum is Windows Vista / Server 2008, with _WIN32_WINNT >= 0x0600 for compilation.

**Documentation caution:** CloseThreadpoolWait's remarks refer to a "timer object" when describing deferred reclamation, although the page concerns a wait object. Treat that wording as an apparent documentation inconsistency, not proof of internal timer identity.

## HYPOTHESES — candidate Linux-hosted architecture, not established behavior

- **H1:** Maintain separate identities for guest wait object, current registration generation, observed guest object reference, and callback invocation. This should prevent a late host readiness event from an old registration being attributed to a replacement registration; Windows behavior at the exact replacement race still needs measurement.
- **H2:** Use a guest waitable-object abstraction and explicit registration ownership rather than directly exposing host file descriptors. Not every NT waitable object is pollable; epoll readiness alone cannot implement the entire guest wait contract.
- **H3:** Deadline conversion must distinguish UTC absolute deadlines from relative durations, clock changes, and overflow. A host timerfd or userspace scheduler is an implementation candidate, not an asserted equivalent.
- **H4:** Cancel/drain/close must retain callback code and context while callbacks execute and preserve guest thread bootstrap/ABI state (R24/R25/R125/R149). Never free guest callback state solely because registration was disarmed.

## PROPOSED Windows-reference differential gates — NOT EXECUTED

| Gate | Reference observations | Candidate acceptance check |
| --- | --- | --- |
| Signaled event; one registration | Callback count and result; repeat signaling without rearm | No accidental recurring callback; explicitly test rearm |
| Timeout: NULL, zero, negative, positive | Timeout-versus-signal outcome and callback result | Correct sign and epoch interpretation; no assumed fixed scheduling latency |
| Replace registered handle while old signal races | Callback count, result, and handle attribution | No internal use-after-free; compare observable reference ordering |
| SetThreadpoolWait(NULL) with callback queued/running | Which callbacks run; drain behavior | Disarm is not mistaken for cancellation of already queued callbacks |
| WaitForThreadpoolWaitCallbacks(FALSE/TRUE) | Return and callback-start/completion counts | Distinguish queued from executing callbacks |
| Close without drain; close with documented sequence | Whether callbacks can outlive close; module/context safety | Asynchronous close safe; documented drain path prevents late callbacks |
| Mutex handle, invalid handle, premature handle close | Exception/failure versus undefined cases | Preserve documented error where defined; do not make conformance claims for undefined case |
| Auto-reset/manual-reset events and multiple wait objects | Signaling consumption and registration behavior | No invented fairness or ordering guarantee |

Instrument Windows OS build, compiler/SDK, handle type, registration generation, callback start/end IDs, clock source, and test barriers. Do not treat scheduling nondeterminism as a conformance failure. Keep host-only tests, Windows-reference tests and integrated PE tests separate.

## OPEN

- Inspect [SetThreadpoolWaitEx](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolwaitex) and callback disassociation before specifying race ordering.
- Determine how callback environments, cleanup groups and DLL pinning interact with wait callback teardown; R149's work-object conclusions must not be assumed identical.
- Inventory first-cohort PE imports to decide whether wait registrations outrank timers and threadpool I/O.
- Validate whether time adjustments affect already registered positive FILETIME deadlines on each target Windows build.

## Provenance

Primary Microsoft Learn API pages linked above were directly inspected on 2026-11-17 (living documentation, not pinned Windows source). No experiments were performed; no implementation or compatibility result is claimed.
