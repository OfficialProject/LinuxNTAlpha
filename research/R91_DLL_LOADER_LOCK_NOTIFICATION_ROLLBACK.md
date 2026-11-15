# R91 — DLL loader-lock serialization, notification, and failure rollback gates

Status: **primary-source research / proposed conformance only**, 2026-11-15 UTC. No code, Windows reference execution, Linux experiment, or performance measurement was performed.

## Question

The current PE fixture runner can bind a synthetic import, but does not load real DLL dependency graphs. What observable Windows loader synchronization and initialization behavior must a Linux-hosted NT loader preserve before it can claim DLL support? This note complements R27 (TLS), R32 (search/activation), and R86 (module handles) rather than restating their scope.

## Verified contracts (Microsoft primary documentation)

1. **VERIFIED FROM SOURCE:** DLL entry-point calls are serialized process-wide: only one thread at a time calls an entry point. A load-time DLL receives process attach during process initialization; a dynamically loaded DLL receives process attach before `LoadLibrary` returns. Normal thread creation and termination can trigger thread attach/detach notifications, subject to documented suppression. [Dynamic-Link Library Entry-Point Function](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function).
2. **VERIFIED FROM SOURCE:** `DllMain` runs under the loader lock; holding an unrelated lock while calling loader-sensitive APIs can invert lock order and deadlock. Microsoft advises against `LoadLibrary`, waiting for threads, and many other operations from `DllMain`. These are application restrictions, not a guarantee that the loader may ignore callbacks or release its serialization. [Dynamic-Link Library Best Practices](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices).
3. **VERIFIED FROM SOURCE:** A dynamically loaded DLL's `DLL_PROCESS_ATTACH` returning FALSE causes `LoadLibrary` failure, a process-detach notification and unloading; failure during load-time linking terminates process initialization. Microsoft separately documents that an exception during process attach does **not** generate the same detach notification. [Entry-Point Function](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function); [DllMain](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain).
4. **VERIFIED FROM SOURCE:** Existing threads do not retroactively receive `DLL_THREAD_ATTACH` when a DLL is dynamically loaded; the initial thread that received process attach does not also receive thread attach for that DLL. `lpvReserved` differentiates static versus dynamic process attach and process termination versus dynamic unload for detach. [DllMain](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain).
5. **VERIFIED FROM SOURCE:** `TerminateThread` and `TerminateProcess` do not invoke normal DLL entry-point notifications. DLL unloading generates one process-detach notification rather than a detach callback for every existing thread. [Entry-Point Function](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function).

These are documentation-level contracts, not proof of undocumented loader internals or behavior across all Windows releases.

## Candidate implementation boundaries (HYPOTHESES)

**L1 — One guest loader state machine per process.** Maintain module records with explicit states such as discovered, mapped, imports resolved, initializing, initialized, unloading, failed. Treat states and rollback as design proposals, not Microsoft-defined states. Prevent another guest thread from observing an incompletely initialized module as fully ready.

**L2 — Serialize guest notifications independently of Linux `dlopen`.** A host ELF dynamic-loader lock is not an NT loader-lock contract. Guest entry-point notification ordering must be governed by the NT runtime, including native guest thread startup and shutdown paths. Avoid holding host-global locks while calling arbitrary guest initialization code where possible; determine the exact lock hierarchy experimentally.

**L3 — Preserve distinct failure paths.** Returning FALSE from process attach, throwing an exception, a missing import, and a host mapping failure must not all collapse into one generic cleanup path. Document precisely which modules receive which notifications, whether a handle is published, and what gets unmapped.

**L4 — Callback and TLS ordering is a separate gate.** This note does not assert a universal TLS-callback/DllMain ordering. Coordinate the test matrix with R27; capture actual Windows trace ordering before implementing that contract.

**L5 — Host thread creation is not guest thread initialization.** A pthread can exist before guest TEB/TLS and loader notifications are ready. The guest-visible thread start gate must not run user entry code before required initialization has completed. Signals, cancellation, process teardown, and abnormal exits need separate handling.

## Falsifiable differential test plan — NOT EXECUTED

Build two tiny Windows-target DLLs and a driver executable using an explicitly recorded toolchain and target Windows version. Each callback appends a fixed-size event record (module, reason, thread ID, `lpvReserved` nullness, monotonic sequence) to preallocated storage; do not use loader-unsafe logging APIs inside `DllMain`.

1. **Dynamic load / pre-existing thread:** start worker thread A, then load DLL B on thread C. Assert process attach on C and no retroactive thread attach on A. Create thread D afterward and verify its attach/detach notifications.
2. **Serialization:** make one callback wait on a controlled external event (not another thread needing the loader lock); concurrently attempt load on another thread. Observe callback non-overlap and release ordering with bounded timeouts. Avoid creating an intentional permanent deadlock.
3. **FALSE vs exception:** separate fixtures for `DLL_PROCESS_ATTACH` returning FALSE and raising a structured exception. Record `LoadLibrary` result, `GetLastError`, callback sequence, and module visibility. Do not assume the two paths are equivalent.
4. **Static vs dynamic load:** compare `lpvReserved` on process attach for import-table dependency versus `LoadLibrary`. Compare detach on `FreeLibrary` versus orderly process exit.
5. **Thread notifications:** verify initial-thread exception, thread created after load, suppression via `DisableThreadLibraryCalls`, and abnormal termination separately. Do not rely on nondeterministic scheduler timing.
6. **Graph failure:** a dependent DLL deliberately fails process attach; capture notifications and module visibility of already-initialized dependencies. Require Windows reference behavior before choosing rollback ordering.
7. **Linux candidate:** replay the same fixtures through the eventual PE DLL loader; classify each test PASS/FAIL/SKIP. A `dlopen`-only implementation does not count as a pass.

Record toolchain versions, OS build, architecture, fixture hashes, callback trace, process exit status, timeouts, and unexpected reentrancy. Execute only trusted, purpose-built fixtures. Avoid unsafe code execution in the parse-only harness.

## Decision gate

**OPEN:** Determine an explicit guest loader lock hierarchy and failure/rollback state machine based on reference traces, then implement the smallest two-DLL fixture. No general DLL loader, loader-lock correctness, or performance benefit is established by the current PE fixture tests.

**Nonduplication:** R27 covers TLS data and callbacks; R32 covers path/activation resolution; R86 covers handle lifetime. R91 focuses on cross-thread entry-point serialization, attach/detach eligibility, and divergent initialization failure paths.
