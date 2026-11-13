# R45 — DLL loader-lock serialization and entry-point lifecycle gates

Status: DOCUMENTATION REVIEW ONLY (2026-11-12). No Windows reference run, Linux prototype, or performance measurement was performed.

## Why this is a distinct gate

R27 covers PE TLS directory/callback structure; R29 covers API-set/delay imports; R32 covers module search and activation contexts. None establishes the *concurrent loader transaction* semantics of DLL initialization and teardown. R19/R20/R25/R28 execute trusted PE fixtures but do not load a dependency graph with Windows DLL entry-point lifecycle.

## Verified from Microsoft primary documentation

1. The DLL entry-point function receives process attach/detach and (unless suppressed) thread attach/detach notifications. The loader invokes the process-attach entry point before LoadLibrary/LoadLibraryEx returns for runtime linking. Entry-point calls are serialized: only one thread at a time may call them. [S1]
2. DllMain executes while the loader lock is held. Lock-order inversion can deadlock when a callback takes a private lock while another thread holds that lock and calls a loader-lock-taking API. Microsoft explicitly warns against LoadLibrary/LoadLibraryEx and cross-thread synchronization inside DllMain. [S2]
3. Normal thread termination triggers detach notifications, whereas TerminateThread/TerminateProcess do not provide the same callback guarantee. A DLL is notified of process unload once, not once per existing thread. [S1]
4. A DLL can opt out of thread notifications with DisableThreadLibraryCalls; Microsoft's documentation cautions against using this optimization with a statically linked CRT. [S3]
5. CRT entry points may invoke global/static C++ constructors and destructors; the DllMain restrictions apply to that invoked code too. [S3]

These are published API contracts and documented cautions, **not** evidence that LinuxNTAlpha reproduces them.

## Architectural implications (HYPOTHESES, not decisions)

- A process-local loader state machine with an explicit serialized notification phase is likely needed even if Linux ELF threads, futexes and mmap provide the execution substrate. A plain dlopen-style module map is not itself proof of NT lifecycle equivalence.
- Dependency mapping/import resolution, TLS setup/callbacks, entry-point invocation, module publication, and rollback should be modeled as distinguishable stages. Their precise order and failure behavior must be established by Windows-reference tests; this document does **not** assert a universal order for TLS callbacks versus DllMain across every scenario.
- A global loader mutex may model serialization, but naive locking could produce Linux-only deadlocks or mask Windows-reproducible lock inversion. Reentrancy, callback recursion, unload and thread creation must be specified before choosing the lock design.

## Proposed Windows-reference conformance matrix (NOT EXECUTED)

Build minimal x64 EXE + two DLLs that append timestamp-free event records (thread ID, module, reason, sequence number) to a preallocated buffer. Avoid I/O and blocking inside DllMain.

1. **Load/unload:** explicitly LoadLibrary/FreeLibrary A, record process attach/detach counts; test repeated reference acquisition/release and an intentionally failing process-attach return.
2. **Thread lifecycle:** create and join a thread after A is loaded; compare notifications with and without DisableThreadLibraryCalls. Run a separate controlled termination case to characterize missing notifications.
3. **Concurrent loading:** two threads request A and B concurrently, each DLL records entry/exit with atomic counters. Assert no overlapping entry-point calls as documented, without assuming deterministic ordering between modules.
4. **Dependency chain:** A imports B; capture attach/detach event order on reference Windows builds, with and without a second explicit load of B.
5. **TLS + CRT:** separate static TLS callbacks from DllMain and CRT initialization, compare ordering across initial load and subsequent thread creation. Do not conflate this with the synthetic GS/TIB fixture of R25.
6. **Deadlock negative test:** a *separate, watchdog-controlled disposable process* deliberately creates the documented private-lock/loader-lock inversion. Record timeout as observed behavior only; never run it inside the main harness.

Record Windows build, toolchain, bitness, compiler/linker flags, module images, full traces, and pass/fail criteria. Distinguish documented invariants (serialization) from observed version-dependent ordering.

## Linux implementation gates

- No PE DLL execution until dependency and import validation is bounded (R44), TLS lifecycle is accounted for (R27), and all entry-point transitions have explicit error/rollback behavior.
- Do not call arbitrary PE code from a signal handler or while holding unrelated Linux runtime locks.
- Test Linux-hosted MS-ABI callbacks with reentrant loader requests and multiple pthreads only after a reference trace exists; Linux-only success is not Windows conformance.
- Do not interpret a microbenchmark of mutex acquisition as loader performance or Windows compatibility.

## Open questions

- Precise loader initialization rollback sequence after an attach failure, including dependencies and TLS callbacks.
- Interaction of notification serialization with loader APIs called recursively from DLL initialization.
- Whether an implementation can isolate loader state per NT-compatible process without introducing global host process locks.
- Windows-version differences in detach and process shutdown behavior.

## Primary sources (retrieved 2026-11-12)

- [S1] Microsoft, *Dynamic-Link Library Entry-Point Function*, https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function
- [S2] Microsoft, *Dynamic-Link Library Best Practices*, https://learn.microsoft.com/en-us/windows/desktop/dlls/dynamic-link-library-best-practices
- [S3] Microsoft, *DllMain entry point*, https://learn.microsoft.com/en-us/windows/desktop/dlls/dllmain

No code reused. No experiment executed.
