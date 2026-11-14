# R85 — InitOnce initialization state, retry and publication gates

Evidence date: 2026-11-14 UTC. **Documentation-based research only; no Windows or Linux execution was performed.** This complements [R48](R48_SRW_CONDITION_VARIABLE_GATES.md) (process-local synchronization) and [R39](R39_THREADPOOL_CALLBACK_LIFETIME.md) (callback lifetime). It does not select a Linux implementation.

## VERIFIED FROM PRIMARY MICROSOFT API CONTRACTS

1. Windows one-time initialization uses an `INIT_ONCE` object, statically initialized with `INIT_ONCE_STATIC_INIT` or initialized by `InitOnceInitialize`. The initialization APIs coordinate concurrent threads without requiring the caller to implement its own once flag. [Microsoft: One-Time Initialization](https://learn.microsoft.com/en-us/windows/win32/sync/one-time-initialization); [InitOnceInitialize](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initonceinitialize).
2. `InitOnceExecuteOnce` calls a supplied `PINIT_ONCE_FN` callback to initialize and optionally publish a context. **A callback returning FALSE indicates unsuccessful initialization**; the operation can be attempted again. This is materially different from a once primitive that permanently caches a failed attempt. [Microsoft: InitOnceExecuteOnce](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initonceexecuteonce); [PINIT_ONCE_FN](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nc-synchapi-pinit_once_fn).
3. `InitOnceBeginInitialize` supports a pending/result protocol, including `INIT_ONCE_CHECK_ONLY` and `INIT_ONCE_ASYNC`; `InitOnceComplete` completes successful or failed initialization. Async initialization has additional completion and context restrictions; a synchronous-only approximation cannot be assumed equivalent. [Microsoft: InitOnceBeginInitialize](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initoncebegininitialize); [InitOnceComplete](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initoncecomplete).
4. `INIT_ONCE` is a guest-visible data structure, not a kernel handle. The Windows SDK declaration and initializer must be treated as the ABI reference for the target architecture; neither a host `pthread_once_t` nor a C++ `std::once_flag` has a guaranteed matching layout or failure semantics. [Microsoft: INIT_ONCE structure](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/ns-synchapi-init_once); [One-Time Initialization](https://learn.microsoft.com/en-us/windows/win32/sync/one-time-initialization).

## ARCHITECTURAL HYPOTHESES — NOT VERIFIED IMPLEMENTATIONS

- An independent process-local atomic state machine with a Linux blocking/wake mechanism is a plausible backend. A kernel NT object or `ntsync` device is not established as necessary for this API.
- Model **uninitialized**, **initializing**, **completed-success**, and **retryable-failure** transitions explicitly, including the synchronous/async modes. Publish the context only after successful completion with correct acquire/release visibility to waiting threads. The precise guest representation and memory-order protocol are unresolved.
- A straightforward `pthread_once` adapter is **not sufficient without additional logic** for callback failure and retry; merely embedding host pthread state in guest memory is also ABI-unsafe.
- Avoid calling guest callbacks while holding a global runtime lock. Reentrant initialization, callback exceptions and loader-lock interactions require separate policy and Windows-reference evidence; do not guess undocumented behavior.

## FALSIFIABLE CONFORMANCE PLAN — NOT RUN

1. **ABI:** on Windows x64, record `sizeof(INIT_ONCE)`, alignment, static initializer bytes, and SDK/OS build; repeat for x86 if 32-bit support is considered.
2. **Contention:** start N threads against one static `INIT_ONCE`; block the initializer callback until all contenders arrive, then release. Verify exactly one successful initializer and identical published context for all successful callers. Use watchdogs, not assumed scheduling order.
3. **Failure/retry:** force the first callback to return FALSE and a subsequent callback to succeed. Capture each return value, callback count, published context, and immediate `GetLastError` only where documented. Test repeated failure before success.
4. **Manual synchronous protocol:** exercise `InitOnceBeginInitialize` pending/result states, `InitOnceComplete` success and `INIT_ONCE_INIT_FAILED`; test `INIT_ONCE_CHECK_ONLY` before and after completion. Preserve documented error returns without inventing behavior for invalid flags.
5. **Asynchronous protocol:** test two contenders with `INIT_ONCE_ASYNC` and the documented completion rules; record successful/failed completion and visible context. Keep this separate from synchronous tests.
6. **Publication stress:** repeatedly initialize fresh objects whose callback constructs a multi-field payload; assert readers never observe partial initialization. Compare a Linux guest implementation against a Windows reference using the same test logic.
7. **Negative controls:** invalid parameter combinations and callback recursion belong in isolated child processes with timeouts; do not promote observed crashes or hangs into contractual behavior.

## OPEN QUESTIONS

- Guest ABI layout for each supported architecture and Windows SDK version.
- Exact ordering, retry, and context restrictions for asynchronous completion, to be pinned to Windows-reference observations before implementation.
- Cancellation and exception behavior when an initializing thread terminates unexpectedly.
- Correct guest memory-ordering implementation and lifecycle when storage is reused; no stress test or performance measurement has been performed.

**Boundary:** this note is a primary-documentation-derived contract and proposed test plan, not evidence of a functioning NT initialization subsystem. Source URLs are primary Microsoft references; their live content was not re-fetched in this run.
