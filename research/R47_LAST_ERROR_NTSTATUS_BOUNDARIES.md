# R47 — Win32 last-error state versus NTSTATUS and Linux errno

Evidence date: 2026-11-13 UTC. **Documentation review only; no experiment was run.** This complements R26 (raw syscall interception), R28 (one NTDLL export), R34 (process state), and R46 (dynamic TLS); it does not claim a working Windows error subsystem.

## Verified API contracts (Microsoft primary documentation)

1. `SetLastError(DWORD)` sets the **calling thread's** last-error code; Microsoft says the value is kept in thread-local storage. Other threads must not overwrite it. Some APIs set last-error on success, while many set it only on failure. Source: [SetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setlasterror).
2. `GetLastError()` retrieves that thread's last-error code. Its meaning depends on the **individual preceding API contract**; do not assume every successful API clears it. Source: [GetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror), [Last-Error Code](https://learn.microsoft.com/en-us/windows/win32/debug/last-error-code).
3. `RtlNtStatusToDosError(NTSTATUS)` maps an NTSTATUS to a system/Win32 error code; an unmapped status yields `ERROR_MR_MID_NOT_FOUND`. Microsoft documents **no inverse function** with the corresponding contract. Source: [RtlNtStatusToDosError](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-rtlntstatustodoserror).
4. Microsoft documents `HRESULT_FROM_WIN32` as a conversion from a Win32 error to an HRESULT; that is a **separate representation**, not the thread's last-error slot. Source: [GetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror), [Retrieving the Last-Error Code](https://learn.microsoft.com/en-us/windows/win32/debug/retrieving-the-last-error-code).

## Architecture implications (reasoned proposals, NOT verified implementations)

- Maintain a guest Windows **per-thread last-error value** separate from Linux `errno`, Linux signal state, NTSTATUS return registers and HRESULT. Host syscalls and runtime-library calls must not silently mutate guest last-error. The exact TEB field placement and inline access ABI require independent primary-source/Windows-reference validation before implementation.
- For each Win32 wrapper, define a table-driven **per-API rule**: result convention, failure predicate, last-error update policy (failure only / success and failure / explicitly preserved), and mapping source. A blanket `errno -> GetLastError` mapping is unjustified.
- Keep the original NTSTATUS intact at native NT call boundaries; translate only when the documented Win32 wrapper requires it. Avoid claiming `RtlNtStatusToDosError` is bijective or using its output to reconstruct an NTSTATUS.
- Do not treat an error message produced by `FormatMessage` as a substitute for correct numeric error identity.

## Proposed Windows-reference conformance gates (NOT RUN)

1. Two native Windows threads set distinct sentinel values via `SetLastError`, synchronize, and verify independent `GetLastError` reads. Repeat after context switches and fiber switches; the latter must not be presumed fiber-local merely because FLS exists (see R35).
2. On a Windows reference, set a sentinel; call documented APIs with controlled success and failure paths; record return value and immediate `GetLastError` for **each API**, including cases where success leaves the sentinel intact. Capture OS build and architecture.
3. Probe `RtlNtStatusToDosError` on selected documented NTSTATUS values and an unmapped candidate; record numeric mappings on each tested Windows build. Do not invent an exhaustive mapping table from a small sample.
4. In the future Linux PE harness, invoke a guest Windows-target import of `SetLastError` / `GetLastError` on multiple host threads while deliberately changing host `errno`; verify isolation. This is a proposed fixture, not an executed one.
5. Audit the R26 SIGSYS return path for accidentally replacing NTSTATUS with a Linux negative errno or altering guest last-error. A successful R26 synthetic trap alone does not establish this contract.

## Open questions

- Exact x64 TEB layout, access convention, and effects of Windows runtime initialization across supported OS versions.
- Precise last-error behavior for the first chosen real Win32 wrappers and the completeness/versioning of NTSTATUS mapping data.
- Whether any guest callbacks, APCs or reentrant Win32 calls alter a thread's last-error during a wrapper call; verify on Windows before specifying save/restore behavior.

**Status:** sourced API semantics VERIFIED; implementation recommendations HYPOTHESES; reference tests OPEN. No new measurements, Windows reference runs, compatibility claims or imported third-party code.
