# R46 — Dynamic TLS slot APIs versus PE static TLS and fiber-local storage

Status: DOCUMENTATION REVIEW ONLY (2026-11-13 UTC). No Windows reference execution, Linux prototype, benchmark, or compatibility result is claimed.

## Boundary and why it matters

R27 covers PE TLS directory templates/callbacks, R24–R25 cover a synthetic GS/TIB, R35 covers fibers/FLS, and R45 covers loader callbacks. This note instead specifies the *runtime-allocated TLS index API* and the observable last-error contract. Implementing a PE TLS directory or mapping GS:0x30 does not implement TlsAlloc/TlsGetValue/TlsSetValue/TlsFree.

## Verified primary-source contracts

1. **Process-scoped index, thread-scoped value.** Microsoft's TlsAlloc documentation says an allocated index is usable by threads in the same process, each with its own value slot; newly allocated slots are initialized to NULL; indexes are opaque and not valid between processes. Do not assume a zero-based array index or a stable numeric allocation pattern. [MS TlsAlloc](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlsalloc).
2. **NULL is a legitimate stored value.** TlsSetValue accepts a NULL pointer. TlsGetValue returns NULL both for a stored NULL and for failure, requiring GetLastError to disambiguate; Microsoft explicitly describes TlsGetValue's successful access behavior and last-error handling. [MS TlsSetValue](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlssetvalue), [MS TlsGetValue](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlsgetvalue).
3. **Explicit lifecycle is different from static TLS.** Microsoft's DLL example allocates a TLS index on process attach, sets a separate block for each thread, and cleans up thread-owned blocks and the index; these are application/DLL actions rather than automatic PE TLS template initialization. [MS DLL TLS example](https://learn.microsoft.com/en-us/windows/win32/dlls/using-thread-local-storage-in-a-dynamic-link-library).
4. **Version gate for compiler static TLS.** Microsoft states that DLLs using `__declspec(thread)` could not be explicitly loaded via LoadLibrary/LoadLibraryEx on Windows versions prior to Vista. This historical caveat is not a reason to impose the restriction on a modern target without version-specific evidence. [MS DLL data](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-data).
5. **TlsGetValue2 is a distinct compatibility surface.** Microsoft documents Windows 11 24H2 as the minimum client and says TlsGetValue2 does *not* set the thread's last-error value, unlike TlsGetValue. Its documented relaxed index checking must not be mistaken for a portable security guarantee. [MS TlsGetValue2](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-tlsgetvalue2).

## Proposed subsystem contract (HYPOTHESIS, not selected design)

A process-owned TLS-index allocator could maintain generation/occupancy state; each Windows thread could maintain a sparse mapping of allocated indices to pointer-sized values. This would be logically separate from (a) Linux libc's FS-based TLS, (b) PE static TLS templates, (c) the Windows TEB representation and (d) FLS callbacks. Reuse of freed indices, allocation exhaustion, and exact error codes must be measured on Windows before making the proposed representation normative. A simple Linux `pthread_key_t` wrapper is *not established equivalent*: key limits, destruction semantics and error behavior require explicit comparison.

The guest-visible implementation must also preserve the Windows x64 calling convention at API boundaries and must not overwrite Linux-host FS TLS while accessing guest per-thread state.

## Windows-reference conformance matrix (NOT EXECUTED)

- **Basic isolation:** allocate one index; on two Windows threads store distinct non-NULL pointers; verify each sees its own value and that a new thread initially sees NULL.
- **NULL versus failure:** set NULL in a valid slot; capture return and GetLastError before/after TlsGetValue; separately probe an invalid index. Use SetLastError sentinel values to detect side effects.
- **TlsGetValue2 version gate:** on a host where export exists, compare values and GetLastError side effects against TlsGetValue; on older Windows, record export absence rather than substituting an implementation.
- **Allocation and reuse:** allocate until TlsAlloc fails, record GetLastError, free selected indices, allocate again; report observed reuse but do not assume a fixed maximum or reuse ordering.
- **Thread and DLL lifecycle:** create a thread after allocating an index, terminate a thread with a non-NULL slot, unload a DLL using the documented dynamic TLS pattern, and separately test static PE TLS. Record which cleanup actions are performed by user code versus the loader.
- **Fiber distinction:** switch fibers on the same OS thread and separately compare TLS with FLS values; do not infer FLS destructor behavior from TLS.
- **Negative probes:** invalid/unallocated index, double-free and cross-process numeric index. Run potentially undefined/invalid cases in isolated test processes and report observed behavior rather than codifying accidents.

For each case capture Windows build/architecture, compiler, executable fixture source, return values, last-error values, thread IDs and repeated runs. Compare against an independently implemented Linux-side prototype only after Windows baselines exist.

## Open questions

- Exact allocation exhaustion, reuse and invalid-index error behavior across supported Windows builds.
- Whether guest TLS index lifecycle should share an allocator with PE static TLS slots internally; no compatibility argument currently establishes that choice.
- How guest threads created outside the native Windows-thread creation path acquire their per-thread storage and teardown behavior.
- How loader-lock restrictions in R45 interact with DLL code that allocates TLS indexes or performs cleanup.

## Sources / provenance

Microsoft Learn primary API and DLL documentation linked inline, reviewed 2026-11-13 UTC. No upstream source checkout revision or executable test is associated with this document. Claims about proposed implementation are explicitly hypotheses.
