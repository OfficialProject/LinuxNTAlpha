# R244 — Windows handle protection-from-close versus Linux fd flags

Status: **DOCUMENTATION-BASED DESIGN REVIEW; NO EXPERIMENT**  
Reviewed: 2026-11-22  
Scope: user-mode Win32 handle metadata, not kernel-mode handle-table internals.

## Question

Can a Linux file descriptor (including `FD_CLOEXEC`) directly represent the Windows `HANDLE_FLAG_PROTECT_FROM_CLOSE` bit, and what must the guest handle manager enforce?

## Primary contracts and evidence

**VERIFIED (documented API contract, not reproduced here).** Microsoft's `SetHandleInformation` specifies two independent flags: `HANDLE_FLAG_INHERIT` and `HANDLE_FLAG_PROTECT_FROM_CLOSE`. The latter protects a handle from being closed by `CloseHandle`. `GetHandleInformation` retrieves the handle's flags. These are *handle* properties, not object-wide properties. Sources: [SetHandleInformation](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-sethandleinformation), [GetHandleInformation](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-gethandleinformation).

**VERIFIED (documented API contract).** `CloseHandle` documents that closing a protected handle raises an exception when the process is running under a debugger; therefore a compatibility implementation must not treat all protected-close failures as merely an ordinary `FALSE` return without Windows-reference tests for debugger state. Source: [CloseHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle).

**VERIFIED (documented API contract).** `DuplicateHandle` can duplicate a handle within or between processes and supports `DUPLICATE_CLOSE_SOURCE`. Its documentation states that this option closes the source handle **regardless of any error status returned**. The interaction with a protected source handle, including exact error, debugger behavior and whether protection blocks this forced source close, is **OPEN** pending Windows-reference tests; do not infer it from `CloseHandle` alone. Source: [DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle).

**VERIFIED (documented Linux contract).** `FD_CLOEXEC` is an fd flag controlling descriptor survival across `execve`, not an fd-close prohibition; `close(2)` has no corresponding Windows protect-from-close flag. Linux `dup(2)` creates another descriptor referring to the same open file description, with independently managed descriptor flags. Sources: [fcntl(2)](https://man7.org/linux/man-pages/man2/fcntl.2.html), [close(2)](https://man7.org/linux/man-pages/man2/close.2.html), [dup(2)](https://man7.org/linux/man-pages/man2/dup.2.html).

## Architectural consequence

**HYPOTHESIS.** Store `protect_from_close` and `inheritable` in the **guest process's handle-table entry**, separate from the underlying Linux fd, shared guest object, and object access rights. All guest close paths must pass through one guest authorization/transition point. A raw host `close(fd)`, `close_range`, or worker crash must never be interpreted as successful guest `CloseHandle` semantics.

**HYPOTHESIS.** A per-entry generation identifier is necessary if host descriptors or guest handle values can be reused asynchronously. The handle manager should distinguish: (a) user-requested guest close, (b) process teardown, (c) duplicate-with-close-source, and (d) backend fd loss. Whether teardown bypasses protection is an explicit Windows-reference question, not an established result.

**OPEN.** `SetHandleInformation` mask/flag behavior on pseudohandles, invalid handles, non-closeable handle types and duplicated handles; precise debugger exception conditions; source-close behavior when `DUPLICATE_CLOSE_SOURCE` meets protection; cross-process duplication of flag state. Existing R34/R67/R68 cover inheritance, duplication and pseudohandles broadly, but not this per-handle close-protection gate.

## Proposed conformance matrix (NOT RUN)

On a Windows reference machine, for each test record OS build, architecture, debugger attached/detached, return value, `GetLastError`, exception code and surviving handle state:

1. Create an event and file handle; set protection, query flags, attempt `CloseHandle`, query again; clear protection and close.
2. Duplicate a protected source into the same process, then into another process; inspect protection/inheritance independently on both entries and verify their lifetimes.
3. Repeat duplication with `DUPLICATE_CLOSE_SOURCE` for protected/unprotected sources and deliberate target-access failures; verify whether the source survives and the precise error/exception.
4. Test masks with both flags and partial masks, plus invalid and pseudo-handle inputs.
5. Exercise process termination and handle reuse while pending asynchronous completions retain references.

On Linux, separately validate that a guest handle-table shim can reject protected guest closes while allowing internal backend cleanup, and that fd reuse cannot authorize stale guest handles. **Do not describe these tests as passed until executed.**

## Decision gate

Do not map `HANDLE_FLAG_PROTECT_FROM_CLOSE` onto `FD_CLOEXEC` or object-wide reference counts. Require Windows differential results for duplicate-close-source and debugger-sensitive behavior before freezing the guest handle-table ABI.

## Evidence limitations

This note identifies official Microsoft API and Linux manual-page contracts, but the linked pages were not live-fetched during this run. Exact version-dependent behavior and ambiguous interactions remain open; no Windows or Linux experiment was conducted.
