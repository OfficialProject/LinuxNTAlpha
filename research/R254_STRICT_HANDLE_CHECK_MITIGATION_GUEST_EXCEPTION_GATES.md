# R254 — strict invalid-handle mitigation and guest exception authority

Status: DOCUMENTATION-ONLY, 2026-11-23 UTC. No Windows or Linux experiments performed. Complements R224 (granted access), R244 (protect-from-close), and R246 (cross-process duplication); it does **not** repeat those handle-rights studies.

## Verified primary-source contracts

1. Microsoft defines `PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY` with `RaiseExceptionOnInvalidHandleReference` and `HandleExceptionsPermanentlyEnabled`. Normal `SetProcessMitigationPolicy` use requires these bits to agree, and strict checking ordinarily cannot be disabled once enabled. Debugging tools such as Application Verifier may temporarily enable invalid-handle exceptions, yielding a queried state with the first bit set and the permanent bit clear. Source: [Microsoft strict-handle policy](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_strict_handle_check_policy).
2. Microsoft documents `ProcessStrictHandleCheckPolicy` in the process mitigation query/set interfaces; the policy changes the outcome of manipulating invalid handles, not merely the numerical representation of handles. `GetProcessMitigationPolicy` requires `PROCESS_QUERY_INFORMATION` on the queried process handle. Sources: [GetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-getprocessmitigationpolicy), [SetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy).
3. Linux descriptor operations have their own failure contract: POSIX `close()` reports `EBADF` for an invalid descriptor; Linux `dup2()` also rejects an invalid source descriptor, and its destination replacement is atomic. Neither API implements Windows process mitigation policy or a guest Windows exception. Sources: [close(3p)](https://man7.org/linux/man-pages/man3/close.3p.html), [dup(2)](https://man7.org/linux/man-pages/man2/dup.2.html).

## Architectural consequence (reasoned, not experimentally verified)

The NT guest handle-table layer must own *both* validity and the process mitigation state. Translating an invalid guest HANDLE to an arbitrary Linux fd and returning `EBADF` is not a faithful implementation. A policy-aware dispatcher should validate the guest handle and operation first, then choose the guest-visible failure/exception path according to a pinned Windows profile. A host fd is an implementation resource, never a guest handle authority.

A candidate per-guest-process state record contains the effective strict-check flag, permanent-enable flag, debug/verifier override provenance, OS profile, and synchronization generation. Policy transitions and handle validation must be ordered against concurrent closes and duplicate operations. This is a **hypothesis/design candidate**, not a claim about undocumented Windows internal data structures.

The documented verifier exception matters: never infer that the effective flag being true implies a permanent guest policy. Equally, do not expose a host-side verifier or Linux debugging setting as if it were a Windows verifier state. Do not silently turn a fatal guest exception into a recoverable Win32 error, and do not let it terminate the shared host supervisor if multiple guest processes share one worker.

## Open compatibility questions

- Which precise invalid-handle operations trigger fatal behavior, and which return ordinary errors (CloseHandle, DuplicateHandle, NtClose, NtQueryObject, pseudo-handles, wrong-type valid handles)?
- Which Windows exception/status code, debugger first-chance sequence, and process termination path occur for each operation and Windows build?
- How does the strict policy interact with `HANDLE_FLAG_PROTECT_FROM_CLOSE`, concurrent close/reuse, and inherited or duplicated handles?
- How do the debug/verifier temporary state and permanent state behave across process creation and debugger attach/detach?

Microsoft's policy pages alone do not answer these operation-level questions. **Do not hard-code guessed NTSTATUS or exception codes.**

## Proposed unrun differential gates

1. On explicitly recorded Windows builds, query default policy, enable both bits, query again, attempt disable, and record Win32 return/GetLastError and native exceptions under a debugger.
2. Independently test invalid, stale, wrong-type, pseudo, protected and cross-process handles for `CloseHandle`, `NtClose`, `DuplicateHandle`, and `NtQueryObject`; record first-chance/second-chance exception behavior, not just exit code.
3. Race close/reuse against invalid-handle operations in isolated child processes; compare outcome classes and verify no host fd from another guest is affected.
4. Verify that policy-query access rights are checked using guest handle rights, not host pid/fd access, and that one guest's fatal path cannot kill unrelated guests.
5. Repeat with verifier-enabled Windows reference only if tooling is available and its version/configuration is recorded; otherwise mark that branch **SKIP**.

## Decision gate

Do not declare strict-handle compatibility until the Windows reference matrix is collected and the guest dispatcher reproduces policy transitions, operation-specific failures, debugger delivery and isolation. This note establishes an implementation risk and test plan only; it does not select a kernel boundary or claim a benchmark.

## Source audit

Primary sources consulted 2026-11-23 UTC: Microsoft Learn pages linked above; Linux man-pages/POSIX close and Linux dup pages linked above. No source code was copied. No Windows version was tested.
