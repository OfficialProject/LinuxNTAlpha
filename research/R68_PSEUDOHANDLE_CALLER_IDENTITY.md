# R68 — Process/thread pseudo-handles and caller-relative identity

Date: 2026-11-13. Status: **VERIFIED FROM MICROSOFT API DOCUMENTATION ONLY; NO EXPERIMENT**.

## Question and distinction from R67

R67 concerns duplication of real handles between NT process handle tables. This note addresses a different, easily missed input class: pseudo-handles for the *calling process or thread*. A pseudo-handle is not an ordinary transferable table entry, and resolving it once at process startup would be incorrect for threads.

## VERIFIED — primary Windows API contracts

1. [GetCurrentProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocess) returns a special constant interpreted as the **calling process**, currently `(HANDLE)-1`. Microsoft explicitly recommends using the function rather than hard-coding the constant. It is not inherited. Its documented modern access is `PROCESS_ALL_ACCESS`, with older-version qualifications.
2. [GetCurrentThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthread) returns a pseudo-handle interpreted as the **thread making the use**, not necessarily the thread that originally obtained/stored the value. Microsoft explicitly warns that passing the pseudo-handle to another thread does not create a reference to the original thread. It is not inherited. Its documented modern access is `THREAD_ALL_ACCESS`, with older-version and impersonation qualifications.
3. Both API pages state that `CloseHandle` on the respective pseudo-handle has no effect; a **real** handle obtained by duplication *does* need closing.
4. [DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle) explicitly permits a source pseudo-handle from these APIs when the source-process argument identifies the calling process. Duplication converts the pseudo-handle into a real process/thread handle. Thus resolving the pseudo-handle must occur in the correct caller context before inserting the target's table entry. The real duplicate then has stable object identity, unlike a pseudo-handle reinterpreted by another thread.
5. [Thread handles and identifiers](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-handles-and-identifiers) distinguishes stable real thread handles from thread identifiers and documents that a real handle remains valid until closed even after the thread terminates. [Process handles and identifiers](https://learn.microsoft.com/en-us/windows/win32/procthread/process-handles-and-identifiers) gives the corresponding process distinction.

These are published API contracts, not observed results for a particular Windows build. The exact native `NtCurrentThread` constant and the treatment of every pseudo-handle by every `Nt*` service were **not** independently verified here; do not infer them from Win32 documentation.

## HYPOTHESIS — NT compatibility runtime design

Introduce a **caller-context pseudo-handle resolver** ahead of ordinary per-process handle-table lookup. Its inputs should include the guest calling process, guest calling thread, requested operation, and a typed handle representation. It must distinguish:
- special caller-relative pseudo-handle values;
- real guest handles backed by table entries with granted access and reference lifetime;
- invalid/reserved values that cannot be treated as Linux descriptors.

For a thread pseudo-handle, resolve on **each API invocation**, not when the constant is returned or copied. When duplicating, materialize a new real table entry referencing the resolved thread object, with the normal R67 access/attribute checks. The backend Linux PID/TID and fd are not suitable public guest-handle values. This is a design proposal, **not an implemented or validated resolver**.

**Security invariant to test:** no special-value collision with allocated handles, no accidental interpretation of a foreign process's pseudo-handle in a privileged broker, and no authority granted by accepting a caller-supplied process/thread identity without authentication. A broker must derive caller identity from its trusted session/thread context, not from a forged guest argument.

## Differential conformance gates — NOT RUN

1. On a Windows x64 reference build, retrieve `GetCurrentThread()` in thread A; pass the value to B. In each thread compare `GetThreadId(pseudo)` against its own ID. Duplicate in A, then use the **real duplicate** in B; record whether it names A. Capture return and `GetLastError`.
2. Obtain `GetCurrentProcess()` and duplicate it; check real-handle lifetime, access mask via `GetHandleInformation` where applicable, and close behavior. Do not presume `GetHandleInformation` reports access masks; use an appropriate rights-sensitive operation for access tests.
3. Verify `CloseHandle` on both pseudo-handles and real duplicates; record behavior with a debugger attached separately, because debugger exception behavior can differ.
4. Test `DuplicateHandle` with pseudo source, valid/invalid source-process handle, restricted target process rights, and `DUPLICATE_CLOSE_SOURCE`. Compare to R67's failure/close matrix.
5. Repeat thread cases under impersonation with a controlled token; record rights-sensitive outcomes, Windows version and whether creation occurred while impersonating.
6. Exercise native `NtDuplicateObject` and `NtClose` only in a separately labeled native-contract matrix; do not equate Win32 errors with NTSTATUS.

## OPEN

- Exact native pseudo-handle set, reserved encodings, and per-service acceptance require pinned Windows SDK/WDK headers and Windows-reference testing.
- The interplay of impersonation, process/thread security descriptors and pseudo-handle rights needs measured reference cases.
- No performance result or Linux kernel modification follows from this note.

**Decision gate:** do not model guest handles as raw Linux fds or treat pseudo-handles as transferable integer aliases. Validate caller-relative semantics before integrating a cross-process handle broker.
