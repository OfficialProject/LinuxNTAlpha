# R187 — DisableThreadLibraryCalls, static TLS, and loader notification suppression gates

Research date: 2026-11-19. Status: **VERIFIED FROM PRIMARY MICROSOFT DOCUMENTATION** for the public API contracts below; **HYPOTHESIS** for LinuxNTAlpha design; **OPEN** for Windows-reference conformance. **No tests, builds, benchmarks, or runtime implementation were performed in this research pass.**

## Narrow question and existing-work boundary

R27 covers PE static TLS initialization, R45/R91 DLL loader lifecycle and rollback, and R46 dynamic TLS slots. This note isolates an easily missed *per-module policy transition*: whether a loaded DLL may suppress future thread-attach/detach calls, and how static TLS and thread timing constrain that transition. It does not repeat the general loader architecture.

## VERIFIED — primary Microsoft contracts

1. `DisableThreadLibraryCalls(hLibModule)` suppresses `DLL_THREAD_ATTACH` and `DLL_THREAD_DETACH` notifications for the specified DLL. Microsoft describes it as a possible working-set optimization, not as permission to skip process attach/detach. It fails for an invalid DLL module handle or when the DLL has active **static TLS**. Microsoft explicitly warns against calling it from a DLL linked to the static CRT, which needs thread notifications. [Microsoft, DisableThreadLibraryCalls](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls).
2. The module argument is a DLL module handle; Microsoft says a `GetModuleHandle(NULL)` result is the executable image, **not** the required DLL handle. A failed call returns zero and exposes extended error via `GetLastError`; the documentation does not specify every exact error value for each failure. [Microsoft, DisableThreadLibraryCalls](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls).
3. The initial thread receiving `DLL_PROCESS_ATTACH` does **not** also receive `DLL_THREAD_ATTACH`. When a DLL is dynamically loaded with `LoadLibrary`, already-running threads do not retroactively receive `DLL_THREAD_ATTACH` for it. Nevertheless, a terminating thread can receive `DLL_THREAD_DETACH` even without a prior matching thread-attach notification (for example the initial thread or a thread already running when the DLL loaded). [Microsoft, DllMain](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain).
4. The DLL entry point runs in the relevant process/thread context, and calls into DLL entry points are serialized process-wide. `TerminateThread` and `TerminateProcess` do not trigger the normal thread/process detach notification path. [Microsoft, Dynamic-Link Library Entry-Point Function](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function); [Microsoft, DllMain](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain).
5. Loader-lock restrictions remain relevant even when thread notifications are suppressed: Microsoft's best-practices documentation warns against complex synchronization and potentially recursive loading from `DllMain`. Suppression is **not** a general loader-lock bypass. [Microsoft, Dynamic-Link Library Best Practices](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices).

## HYPOTHESIS — minimal runtime state and ordering

- Track per-loaded-DLL identity, process-attach state, static-TLS presence, notification-enabled state, and unload state in the guest loader, independent of host ELF TLS and pthread destructor behavior.
- On `DisableThreadLibraryCalls`, validate the guest DLL module identity and its static TLS state under loader-coordinated synchronization; do not infer eligibility from whether a host `pthread_key` exists. Return documented success/failure behavior, but do not guess undocumented last-error values.
- On guest thread creation/exit, snapshot or otherwise serialize the eligible loaded-module notification sequence under the guest loader protocol. Do not synthesize retroactive attach calls on dynamic loads. Preserve the possibility of detach without prior attach for a particular thread.
- Treat suppressed callbacks and static TLS initialization as **separate** questions. This API's failure for active static TLS is not evidence that TLS data can be skipped when callbacks are suppressed.
- Do not automatically map guest thread notifications to Linux `pthread_create`/pthread TLS destructors: those mechanisms do not themselves know guest DLL identity, loader order, or Windows termination paths.

## OPEN — falsifiable Windows-reference matrix (NOT RUN)

1. Build controlled x64 DLLs with (a) no TLS, (b) `__declspec(thread)` static TLS, (c) dynamic `TlsAlloc`, and (d) static CRT linkage. Record `DisableThreadLibraryCalls` return values and `GetLastError`, including invalid module and EXE-base cases. Do not assume the CRT warning necessarily translates into a specific API failure.
2. Record process/thread notification sequences for initial thread, preexisting worker during `LoadLibrary`, worker created afterward, normal exit, `FreeLibrary`, and process exit. Include a suppression-on/off comparison.
3. Record whether any TLS callback notifications differ from `DllMain` notifications in each case; do not assume this API suppresses TLS callbacks without a documented or measured basis.
4. Exercise concurrent thread creation and module unload with a watchdog; establish ordering and reentrancy behavior against Windows before implementing guest loader synchronization.
5. Only after correctness tests, compare loader callback counts, working-set effects, and thread startup latency. No performance improvement is claimed.

## Evidence provenance and limits

Primary Microsoft Learn pages above were inspected on 2026-11-19; these are **living API documentation**, not pinned Windows binary builds or source revisions. This note contains no Windows version-specific experimental observations, no Linux kernel-source audit, and no claim of complete DLL/TLS support. Cross-reference R27, R45, R46, R64, R91, and R115 before implementation.
