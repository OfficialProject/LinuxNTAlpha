# R157 — Threadpool stack sizing and persistent-worker compatibility gates

Date: 2026-11-18. Status: **primary-source research only; no Windows or Linux runtime experiment**.

## Scope and relationship to earlier research
R154 addresses pool capacity/priority and R155 callback execution lifetime. This note isolates a different contract: **stack settings apply only to newly created pool workers**, and callback-environment **persistent-thread selection** is distinct from the **long-running callback hint**. It does not reassert that either is equivalent to a Linux pthread pool.

## Verified from Microsoft API documentation
1. `SetThreadpoolStackInformation(pool, info)` sets the reserve and commit sizes for **new** threads in the specified pool; **existing threads are unchanged**. It returns BOOL, with GetLastError on failure. Its documented minimum is Windows 7 / Server 2008 R2. [S1]
2. `QueryThreadpoolStackInformation` retrieves pool stack reserve/commit configuration; it also returns BOOL. Its documentation does **not** promise that all currently running workers share those sizes. [S2]
3. `SetThreadpoolCallbackPersistent(environment)` specifies that the callback should run on a persistent thread. It is an inline environment-setting function, supported from Windows 7 / Server 2008 R2. The page does **not** specify worker identity, affinity, a dedicated thread, or how many persistent threads exist. [S3]
4. `SetThreadpoolCallbackRunsLong(environment)` is a **hint** that callbacks may not return quickly; the pool **may** use it when deciding whether to create another worker. It is not a promise of a dedicated worker or guaranteed additional capacity. [S4]
5. A callback environment is initialized before modifying its settings; custom pools are selected through `SetThreadpoolCallbackPool`. Destroying the environment is documented for when it is no longer needed for creating new pool objects. [S5][S6]

## Architectural implications — hypotheses / proposed design, NOT verified behavior
- **H1:** Keep per-pool stack policy separate from per-worker actual stack allocation. Updating a pool must not mutate live worker stacks; a generation tag on newly created workers is a possible implementation aid, not an NT requirement.
- **H2:** A persistent-worker scheduling class may be necessary, but avoid assuming a one-callback/one-thread mapping or Linux CPU affinity. Preserve observable behavior first, then select a scheduler.
- **H3:** A Linux pthread stack size, guard area and VM reservation may need guest stack reserve/commit bookkeeping rather than direct field-for-field translation. Cross-reference R65/R126 for guest stack-growth and probe issues.
- **H4:** A long-running hint should be a scheduling input, not a guarantee that max-thread limits are bypassed. Cross-reference R154.

## Unresolved questions / falsifiable reference tests
1. **Mixed worker generations:** create a pool, prewarm workers, query initial policy, change stack settings, submit callbacks on existing and newly spawned workers, and compare observed stack boundaries to documented policy. Use Windows-reference tests with controlled pool size and worker-blocking barriers. Do not infer a specific worker allocation order.
2. **Reserve vs commit:** probe both fields separately, boundary/invalid combinations, error codes and effective stack growth; log OS build, process bitness, image stack defaults and page size. Avoid treating a query result as measurement of every worker.
3. **Persistent vs runs-long:** exercise four environment configurations (neither, persistent only, runs-long only, both) under bounded concurrency; measure thread IDs, pool saturation, latency, and shutdown. Thread ID reuse alone cannot prove a persistent guarantee.
4. **Lifetime:** test environment destruction after work-object creation versus worker completion, with independent object teardown and no use-after-free. Verify Windows-reference behavior before copying a lifetime policy.
5. **Host implementation:** separately record Linux pthread attributes, mapping sizes, guest stack-probe behavior, and exceptions on guard-page exhaustion. This remains a proposed experiment; **none was executed**.

## Evidence limitations
Microsoft's API descriptions specify the configuration surface, not a deterministic scheduling algorithm. No Linux implementation, Windows differential probe, performance result, or compatibility claim follows from these pages. The precise semantics of persistent workers under pool shutdown remain **OPEN**.

## Primary sources (consulted 2026-11-18)
- [S1] Microsoft, SetThreadpoolStackInformation: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolstackinformation
- [S2] Microsoft, QueryThreadpoolStackInformation: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-querythreadpoolstackinformation
- [S3] Microsoft, SetThreadpoolCallbackPersistent: https://learn.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-setthreadpoolcallbackpersistent
- [S4] Microsoft, SetThreadpoolCallbackRunsLong: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadpoolcallbackrunslong
- [S5] Microsoft, InitializeThreadpoolEnvironment: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-initializethreadpoolenvironment
- [S6] Microsoft, DestroyThreadpoolEnvironment: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-destroythreadpoolenvironment
