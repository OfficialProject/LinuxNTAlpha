# R64 — DLL dependency graph identity, publication, and rollback gates

Date: 2026-11-13. Status: **documentation review only**; no Windows-reference run, Linux implementation, or benchmark.

## Why this is a separate gate

R20 establishes a single synthetic import-to-host call. R29 treats API-set and delay-import resolution; R32 addresses search policy; R45 treats loader-lock and entry-point callbacks; R51 addresses export forwarders. **None establishes a process-wide module graph with stable identity, partially initialized nodes, reference accounting, and failure rollback.** That graph is required before a multi-DLL loader can claim even a small subset of Windows behavior.

## VERIFIED — Microsoft primary documentation

1. Windows DLL lookup considers redirection, API sets, SxS, already-loaded modules and Known DLLs *before* ordinary filesystem search in the documented unpackaged safe-search path. A dependent DLL is searched by its **module name**, even when its parent was loaded using an absolute path. Search policy varies with packaging, OS version and loader flags. [Microsoft, Dynamic-link library search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
2. A run-time LoadLibrary load calls DLL initialization before returning. FreeLibrary decrements the module reference count; when it reaches zero the DLL may be unmapped. A DLL dynamically loaded after threads already exist does not receive historical thread-attach callbacks for those threads. [Microsoft, Run-Time Dynamic Linking](https://learn.microsoft.com/en-us/windows/win32/dlls/run-time-dynamic-linking); [DLL entry-point function](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function).
3. DLL entry-point notifications are serialized. Calling LoadLibrary from DllMain is unsafe and can create dependency/initialization-order failures. The loader lock is held during DllMain, and waiting for another thread can deadlock. [Microsoft, DllMain](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain); [DLL best practices](https://learn.microsoft.com/en-us/windows/desktop/dlls/dynamic-link-library-best-practices).
4. If DLL_PROCESS_ATTACH returns FALSE during an explicit LoadLibrary, LoadLibrary fails, the entry point receives DLL_PROCESS_DETACH, and the module is unloaded. If the failure occurs during process startup, startup fails. This is **not** evidence that all dependent modules receive the same rollback callbacks or in any particular order. [Microsoft, DllMain](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain).
5. Unqualified DLL loading is a security boundary: attacker-controlled search directories permit DLL preloading attacks. [Microsoft, Dynamic-Link Library Security](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-security).

## HYPOTHESIS — candidate internal representation, not a Windows implementation claim

Use a process-local module registry keyed by resolved module identity, separate from requested import spelling and physical pathname. Model states `discovered → mapped → imports_resolving → ready_for_initialization → initializing → initialized`, plus `failed` and `unloading`. Keep references from load requests separate from dependency edges and transient loader pins. Reserve an in-progress registry entry *before* recursively walking dependencies to detect cycles without unbounded recursion; do **not** assume a cyclic import graph is accepted by Windows or that callbacks can safely observe any in-progress state.

This state machine is an engineering proposal. Exact Windows identity equivalence, reference count behavior, cyclic dependencies, initialization order, and error codes remain **OPEN**. In particular, a pathname-only cache would be incorrect as a general architecture because loaded-module lookup can precede path search, while treating every same-basename request as identical would also be unproven.

## Proposed conformance matrix — NOT RUN

Build tiny signed-independent test DLLs A, B and C on Windows and log to a preallocated buffer (avoid logging APIs that themselves load DLLs from DllMain). Compare clean-process runs on specified Windows build/architecture, then an independent Linux implementation:

- A imports B; B imports C: capture mapping, process-attach order, detach order and explicit FreeLibrary outcomes.
- A and C both import B: test shared module identity and whether unloading one top-level reference leaves B usable.
- Same basename in two directories, full-path top-level load versus bare-name dependency: capture actual loaded module paths and handles with documented GetModuleFileName/GetModuleHandle APIs; repeat under controlled search flags.
- A fails DLL_PROCESS_ATTACH: log A detach, other modules' detach and surviving module handles; repeat at process startup and explicit LoadLibrary.
- Attempt circular A↔B imports and forwarders with strict timeouts in disposable child processes; record success/failure and initialization observations rather than assuming an outcome.
- Concurrent LoadLibrary/FreeLibrary calls and a DllMain that attempts a loader-sensitive operation: run only in disposable timeout-bounded child processes, never in a production loader.
- Load a DLL after a worker thread already exists, then start another thread; compare thread-attach notifications.

Record module bytes/hashes, OS build, toolchain, search flags, process bitness, call trace, last-error values and per-test pass/fail. An independent Linux runner must not execute untrusted PE code without an explicit execution gate (see R23).

## Acceptance boundary and prioritization

**Near-term priority:** extend R20's parse-only import inspection to multiple descriptors and construct a *nonexecuting* dependency graph with strict limits on node count, depth, cycles, string length and file bytes. Keep module identity/search policy abstract and test with synthetic graphs. Only after Windows-reference observations should initialization and rollback behavior become executable.

**Not established:** Windows-compatible module loading, correct graph traversal ordering, safe reentrant callbacks, DLL search security, or general application compatibility.

Sources checked 2026-11-13. These are API contracts, not experiments; Windows version-sensitive behavior requires differential confirmation.
