# R52 — NTSTATUS, Win32 last-error, and HRESULT translation boundaries

Date: 2026-11-13. Status: **PRIMARY-DOCUMENTATION ANALYSIS ONLY**. No Windows reference execution, Linux experiment, or compatibility result is claimed.

## Architectural question

After a PE32+ image calls a Linux-native implementation of an NTDLL or Win32 function, which status representation belongs at each boundary, and where is the caller-visible per-thread error state maintained? R28 establishes one real NTDLL export but does not establish error-state semantics; R26 intercepts one synthetic syscall but does not implement NTSTATUS handling. This note is a focused contract for that gap, not an implementation proposal disguised as a result.

## Verified from published Microsoft interfaces

1. **NTSTATUS is a separate status namespace.** The Windows Driver Kit defines `NTSTATUS`, `NT_SUCCESS(Status)`, and severity-oriented predicates. `NT_SUCCESS` tests a signed status value for nonnegative success; therefore it is not equivalent to testing whether the status is exactly `STATUS_SUCCESS`. Warning and informational status handling requires API-specific interpretation. Source: [Using NTSTATUS values](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-ntstatus-values).
2. **Win32 GetLastError is thread-local and API-specific.** Microsoft states that `GetLastError` retrieves the calling thread's last-error code and that functions set it only under their documented conditions. A successful call need not clear the previous error; some APIs set last error even on success. A caller must inspect the relevant API's return contract before consulting it. Sources: [GetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror), [SetLastError](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setlasterror).
3. **NTSTATUS-to-Win32 mapping is an explicit operation, not bit reinterpretation.** The documented `RtlNtStatusToDosError` routine maps an NTSTATUS value to a system error code. The public documentation does not establish that the mapping is reversible or that every NTSTATUS has a distinct Win32 equivalent. Source: [RtlNtStatusToDosError](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-rtlntstatustodoserror).
4. **HRESULT is a third error representation.** The Windows SDK's `HRESULT_FROM_WIN32` conversion and `HRESULT_FROM_NT` conversion encode different facilities/representations; a generic wrapper must not assume an NTSTATUS can be directly treated as a Win32 error or HRESULT. Source: [Structure of COM error codes](https://learn.microsoft.com/en-us/windows/win32/com/structure-of-com-error-codes).
5. **Win32 last error is not automatically the return status of an NTDLL routine.** This follows from the separately documented contracts above: a native API returning `NTSTATUS` and a Win32 API returning a Boolean or handle expose different interfaces. Whether a specific wrapper sets last error must be established per API rather than imposed on every NTDLL return.

## Design implications — HYPOTHESES, not verified compatibility

- Keep `NTSTATUS`, Win32 `DWORD` error codes, and `HRESULT` as distinct types/interfaces. Put status translation at the particular wrapper that requires it, not in a universal syscall return hook.
- Provide **per-Windows-thread** last-error storage tied to the Windows thread abstraction. R24/R25's synthetic GS slot is not yet a genuine TEB, and host Linux `errno` is not an acceptable alias for Win32 last error. The precise TEB layout and access path must be established separately.
- A Linux-hosted syscall implementation should return its declared NT status and leave error mapping to the Win32 facade where applicable. Linux negative `errno` values must never leak as NTSTATUS values by accident.
- Preserve the prior last-error value when the specific API's success contract does not overwrite it. Do not indiscriminately clear errors on every successful call.
- Build mapping tables from independently verified behavior or legally reusable sources only after R21 licensing gates; a small hand-written table is not evidence of complete coverage.

## Proposed Windows-reference conformance gates — NOT RUN

1. For documented Win32 APIs with deterministic failures, capture return value, `GetLastError` immediately after the call, and last-error value after a subsequent successful call. Seed a sentinel with `SetLastError` before each case; never assume the second call preserves it unless the specific API documents that behavior.
2. For independently documented native calls, record full 32-bit NTSTATUS values and test success/information/warning/error cases without reducing them to Boolean success. Verify any wrapper's conversion against the Windows reference, including unmapped statuses.
3. Run two threads with different sentinel last-error values, interleave calls and callbacks, and verify isolation. Repeat through a PE32+ import thunk, Linux-hosted MS-ABI bridge, and a future actual TEB implementation.
4. Test nested callback paths where a helper API changes last error; determine which caller-facing wrappers must preserve/restore it. Do not assume all callbacks preserve it.
5. Keep separate regression cases for `HRESULT_FROM_WIN32` and `HRESULT_FROM_NT`; assert exact bit patterns from documented macros rather than a guessed equivalence.

## Acceptance boundary

**VERIFIED:** Microsoft publishes distinct NTSTATUS, Win32 last-error, and HRESULT contracts and a named status conversion routine. **OPEN:** Windows-reference outputs for the selected APIs, complete conversion coverage, exact per-thread TEB wiring, and behavior under callbacks. **NOT CLAIMED:** a working status layer, successful Windows application execution, or benchmarked overhead.

## Next implementation dependency

Before extending R28 beyond its pure memory routine, specify each exported API's return type and error-state side effects, then add a Windows golden-output harness. This should precede broad import-name allowlisting or generalized NT syscall dispatch.
