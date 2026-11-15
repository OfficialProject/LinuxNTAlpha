# R96 — Fiber-local storage callbacks, index reclamation and module lifetime

Status: **RESEARCH ONLY** (2026-11-15). No Windows reference run, Linux prototype, or performance measurement was performed.

## Scope and relationship to earlier work

R91 covers DLL notification serialization and load failure; R64 covers dependency graph rollback; R74 covers thread-object lifetime. This note isolates a different compatibility boundary: **FLS callbacks are associated with process-wide indexes but carry values for individual fibers, and callback execution can outlive the module that registered the callback**. Do not collapse FLS into static PE TLS, Win32 TlsAlloc, or pthread thread-specific data.

## Verified primary-source contracts

1. **Windows FLS is per-fiber storage with process-scoped indexes.** `FlsAlloc` optionally associates a callback with an index. Each fiber can store its own value; indexes are not portable between processes. Microsoft: [FlsAlloc](https://learn.microsoft.com/en-us/windows/win32/api/fibersapi/nf-fibersapi-flsalloc).
2. **The callback has multiple trigger paths.** Microsoft documents a non-NULL slot callback at fiber deletion, thread exit, and FLS-index free. The callback receives the stored pointer. Microsoft: [PFLS_CALLBACK_FUNCTION](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nc-winnt-pfls_callback_function).
3. **Freeing an index has process-wide effects.** `FlsFree` releases the index for all fibers in the process and calls the associated callback for each non-NULL fiber slot. It does **not** itself free memory referenced by those pointers. Microsoft: [FlsFree](https://learn.microsoft.com/en-us/windows/win32/api/fibersapi/nf-fibersapi-flsfree).
4. **Module unload is a real callback lifetime hazard, not merely theoretical.** Microsoft's Visual C++ troubleshooting note documents a static-CRT DLL whose registered FLS callback can point into unmapped module memory after exceptional unload, leading to an access violation on thread exit. This documents a Windows failure mode; it does **not** establish that a compatible runtime should silently suppress such callbacks. Microsoft: [Error at thread exit if FLS callback isn't freed](https://learn.microsoft.com/en-us/troubleshoot/developer/visualstudio/cpp/libraries/fatal-error-thread-exit-fls-callback).
5. **Linux pthread keys are not a drop-in implementation.** POSIX `pthread_key_delete` does not invoke destructors; Linux pthread-key destructors run on thread termination, with unspecified order and a bounded iteration policy in glibc. Neither contract provides the documented process-wide, per-fiber `FlsFree` callback traversal. Sources: [pthread_key_delete(3p)](https://man7.org/linux/man-pages/man3/pthread_key_delete.3p.html), [pthread_key_create(3)](https://man7.org/linux/man-pages/man3/pthread_key_create.3.html).

## Architectural hypotheses (not verified)

- **H1:** A guest-owned FLS registry keyed by (process, slot generation) and per-fiber value maps is a more faithful baseline than directly exposing host pthread keys. Generations may help prevent stale guest index reuse internally, but guest-visible reuse and error codes require Windows reference tests.
- **H2:** Fiber deletion, guest thread termination, `FlsFree`, and DLL unload should converge through a coordinated lifecycle state machine. Callback invocation must occur in an appropriate guest execution/ABI context, not from an arbitrary Linux housekeeping thread.
- **H3:** Module-unload handling needs explicit policy for registered callbacks pointing into unloading images. Do not assume Windows pins the module; the documented dangling-callback failure contradicts an unconditional pinning claim.
- **H4:** The runtime must coordinate callback enumeration with concurrent fiber switches, index reclamation and module unload. Locking and reentrancy strategy is unresolved; holding a global registry lock while calling arbitrary guest code is a candidate deadlock hazard.

## Falsifiable conformance plan (NOT executed)

Use a Windows x64 reference executable and DLL with logged event sequence, thread/fiber IDs, slot index, callback data, and return/error results. Keep each scenario isolated so crashes do not contaminate later cases.

| Gate | Reference action | Observation required |
| --- | --- | --- |
| FLS-01 | Allocate index, set distinct values on two fibers, switch repeatedly | Per-fiber isolation; zero initial values |
| FLS-02 | Delete one fiber with a non-NULL value | Callback invocation, identity/context, exactly observed count |
| FLS-03 | Exit a thread owning fiber/FLS state | Callback invocation and ordering relative to DLL_THREAD_DETACH; do not presume ordering |
| FLS-04 | Free an index used by multiple live fibers | Which callbacks run, context, per-fiber values, post-free access errors |
| FLS-05 | Reallocate after free and probe stale index use | Numeric reuse, initialization, error behavior; avoid asserting generation visibility |
| FLS-06 | Callback mutates its own/another FLS slot or frees another index | Reentrancy, iteration and callback ordering; isolate hangs |
| FLS-07 | Register callback in DLL, then unload normally or exceptionally | Whether callback survives, unload sequencing and crash behavior; run in disposable process |
| FLS-08 | Concurrent index free and fiber teardown on separate threads | Race outcomes, callback multiplicity, deadlock/crash behavior; stress only after deterministic cases |

For Linux-side prototype, log each transition and ensure callbacks execute on a guest-capable thread with correct Windows x64 calling convention, TEB/GS state and exception boundary. Do **not** treat a passing pthread destructor test as Windows FLS conformance.

## Open questions / exit criteria

- Callback ordering across index free, fiber deletion and DLL thread notifications is not established here.
- Exact reentrancy guarantees, callback exception behavior and behavior under concurrent index reclamation need reference observations.
- Whether native NT APIs expose additional FLS details beyond the documented Win32 surface is not established.
- Acceptance requires repeatable Windows reference traces, an independently implemented Linux state machine, differential comparison, and a negative test for stale callbacks after DLL unload.

**Evidence classification:** items 1–5 VERIFIED from cited primary documentation; H1–H4 HYPOTHESES; test gates and open questions UNTESTED. Source URLs reviewed 2026-11-15; no upstream code revision audited in this note.
