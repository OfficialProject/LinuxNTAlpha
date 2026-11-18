# R158 — Simple threadpool submission and in-callback long-running admission gates

Date: 2026-11-18. **Documentation research only. No Windows reference execution, Linux runtime experiment, or benchmark was performed.**

## Scope and nonduplication
R149 covers reusable TP_WORK submission and teardown; R154 covers pool capacity and pre-submission callback priority; R155 covers callback disassociation; R157 covers persistent worker policy and preconfigured runs-long hints. This note isolates two different API surfaces: one-shot `TrySubmitThreadpoolCallback` and the *in-callback*, BOOL-returning `CallbackMayRunLong`. These must not be conflated with `SubmitThreadpoolWork` or `SetThreadpoolCallbackRunsLong`.

## Verified primary-source contracts
1. `TrySubmitThreadpoolCallback(PTP_SIMPLE_CALLBACK, context, environment)` **requests** execution by a pool worker, returns BOOL, and documents `GetLastError` on failure. A NULL environment selects the default callback environment. The documented interface does not return a TP_WORK handle or a per-submission cancellation/wait handle. [S1]
2. The `SimpleCallback` signature receives `PTP_CALLBACK_INSTANCE` and optional context. Microsoft's callback page lists the callback-instance operations, including `CallbackMayRunLong`, deferred release actions, and disassociation. It does not expose an application-writable callback-instance layout. [S2]
3. `CallbackMayRunLong(instance)` returns TRUE when another worker is available **or** a new worker was created; FALSE when neither condition holds. On FALSE, documentation says the pool will attempt another worker after a delay, without guaranteeing success or a deadline. [S3]
4. `CallbackMayRunLong` marks the callback long-running **even if it returns FALSE**; Microsoft instructs callers to invoke it only once and only on the callback's executing thread, warning of a race if called elsewhere. Its BOOL result is **not** a verdict that the callback was or was not marked long-running. [S3]
5. `SetThreadpoolThreadMinimum` returns BOOL with GetLastError on failure, whereas `SetThreadpoolThreadMaximum` returns VOID. Their documented purpose is per-pool available-thread minimum and allocation maximum, not a guaranteed latency or callback order. [S4][S5]

## Architectural interpretation — hypotheses, not Windows facts
- **H1: Distinct submission model.** Represent a successful simple submission as an internally owned pending callback with no guest-visible TP_WORK object. Do not invent a public close/cancel handle. Internal lifetime and cleanup-group semantics require reference validation; no guarantee is inferred from the absence of a returned handle.
- **H2: Callback-instance state.** Maintain a per-invocation long-running marker separately from the capacity observation returned by `CallbackMayRunLong`. A FALSE return still updates that marker.
- **H3: Capacity signaling.** Evaluate availability/creation atomically enough to avoid internal data races; do not promise deterministic TRUE/FALSE under concurrent pool activity. Linux pthread counts are an implementation mechanism, not a Windows contract.
- **H4: Module and context safety.** Keep the callback target and context alive until invocation is finished according to documented callback environment/module protections. This is a proposed ownership requirement, not proof that the API automatically retains arbitrary application context.

## Reference-test frontier (not executed)
1. Submit a simple callback with default and custom environments; record success/failure, GetLastError on failure, callback count and pool association. Avoid inferring synchronous completion from TRUE.
2. Saturate a custom pool with controlled blocking callbacks, then call `CallbackMayRunLong` once within a running callback. Record its BOOL, worker IDs and later scheduling under different min/max limits; never assume that FALSE means the hint was rejected.
3. Compare a preconfigured `SetThreadpoolCallbackRunsLong` environment against an in-callback `CallbackMayRunLong` call; test each independently and together. Observe rather than assume scheduling equivalence.
4. For simple callbacks, test environment and cleanup-group interactions, DLL lifetime and deferred callback-return actions. Explicitly verify whether group closure drains simple submissions; do not silently transfer TP_WORK behavior.
5. Stress callback completion and process teardown with pending simple submissions. Record OS build, architecture, pool configuration and all observable completion events. Host Linux experiments should be reported separately.

## Open questions and evidence limits
The cited API pages do not define a precise worker-creation algorithm, time bound for the delayed attempt, total ordering of simple callbacks, or a public cancellation handle. These are **OPEN**, not negative findings. This note does not establish behavioral equivalence of any Linux implementation.

## Primary sources (inspected 2026-11-18)
- [S1] Microsoft, TrySubmitThreadpoolCallback: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-trysubmitthreadpoolcallback
- [S2] Microsoft, SimpleCallback: https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/ms686295(v=vs.85)
- [S3] Microsoft, CallbackMayRunLong: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-callbackmayrunlong
- [S4] Microsoft, SetThreadpoolThreadMinimum: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolthreadminimum
- [S5] Microsoft, SetThreadpoolThreadMaximum: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolthreadmaximum
