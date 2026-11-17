# R145 — Windows synchronous I/O cancellation versus Linux request cancellation

Status: **VERIFIED FROM SOURCE** for cited API contracts; **HYPOTHESIS** for the proposed LinuxNTAlpha design; **OPEN** for runtime conformance. Reviewed 2026-11-17. No experiment was executed.

## Scope and distinction from prior work

R137 addresses io_uring cancellation completion and buffer lifetime. This note instead isolates the **thread-scoped synchronous** Windows `CancelSynchronousIo` contract, and its non-equivalence to file-handle-scoped `CancelIoEx` and Linux request cancellation. It does not claim a Linux backend already implements the contract.

## Verified primary-source contracts

1. Microsoft documents `CancelSynchronousIo(hThread)` as marking pending synchronous I/O issued by the specified thread for cancellation. The caller requires `THREAD_TERMINATE` access. If no request is found, it fails with `ERROR_NOT_FOUND`. Cancellation is an attempt, not a join: the call does not wait for outstanding operations to terminate; a request may complete normally, abort with `ERROR_OPERATION_ABORTED`, or fail for another reason. [Microsoft CancelSynchronousIo](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelsynchronousio).
2. Microsoft explicitly warns that the target thread can finish operation X and begin Y before cancellation is applied, potentially canceling Y instead. This matters particularly for thread-pool worker reuse. [Microsoft cancellation guidelines](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations).
3. `CancelIoEx(hFile, lpOverlapped)` is **process- and file-handle-scoped**, optionally selecting an OVERLAPPED request; it can target requests from other threads. Successful submission of cancellation does not mean completion. An OVERLAPPED structure must not be freed/reused before terminal completion. Microsoft documents differing IOCP behavior for successfully canceled synchronous operations versus pending asynchronous operations. [Microsoft CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex).
4. `CancelIo(hFile)` is narrower still: pending overlapped I/O issued by the **calling thread** on the file handle; it does not cancel requests from other threads and does not meaningfully cancel non-overlapped operations on a synchronous handle. [Microsoft CancelIo](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelio).
5. The Linux liburing manual states that io_uring operations may remain alive after application fd closure because pending operations retain file references; cancellation can be asynchronous, some operations cannot be canceled, and a separate synchronous-cancel registration interface exists. This is not evidence of Windows thread-scoped semantics. [io_uring_cancelation(7), liburing manual](https://man7.org/linux/man-pages/man7/io_uring_cancelation.7.html).

These are published API contracts, not a pinned implementation snapshot or Windows-version conformance measurement.

## Architectural implications (hypotheses)

- **Guest request registry:** associate each guest I/O operation with a stable guest process identity, guest thread identity/generation, guest file-object identity, operation ID/generation, request kind (synchronous/overlapped), and terminal-completion state. A Linux fd, host TID or user-supplied OVERLAPPED address alone is not sufficient identity.
- **Access check first:** resolve the guest thread handle and enforce its requested access against the guest token/object policy before searching the request registry. Do not translate `THREAD_TERMINATE` permission into host `tgkill`, `ptrace` or permission to terminate a Linux thread.
- **Race-preserving cancellation:** linearize registry lookup against request registration and completion, but do not pretend that `CancelSynchronousIo` provides an operation-specific cancellation guarantee. Record the request actually selected; a target thread that reuses its worker context must not inherit stale cancel state.
- **Terminal ownership:** preserve the operation and its buffers until an authoritative terminal result. Cancellation submission is not terminal completion. Translate backend errors only after confirming the guest operation's final state; a canceled request can still succeed.
- **Backend choice:** prefer an explicit cancelable async backend plus guest-visible synchronous waiting when applicable, but verify its behavior per file/device class. A blocking Linux `read` in an arbitrary host worker cannot be assumed safely interruptible by signals, fd close, or thread cancellation. io_uring does not universally cancel in-flight hardware I/O.
- **Completion semantics:** route IOCP, event and synchronous return outcomes according to guest operation mode; do not automatically queue one packet for every cancellation request.

## Falsifiable acceptance matrix (not executed)

1. Windows reference: blocked synchronous read in thread T; cancel with a valid thread handle; separately test insufficient `THREAD_TERMINATE` rights, no outstanding request, and thread exit/reuse.
2. Windows reference: arrange X to complete while cancellation races with Y on the same thread. Record which operation terminates and exact Win32 error, without asserting deterministic timing.
3. Windows reference: issue two overlapped requests on the same file from different threads; contrast `CancelIo`, `CancelIoEx(file,NULL)`, `CancelIoEx(file,&overlapped)`, and `CancelSynchronousIo(thread)`.
4. Linux backend: test pipe/socket, regular file, and (only on an approved test host) a genuinely slow device; capture kernel, liburing, filesystem, io_uring feature probes, cancellation CQE and target CQE independently.
5. Lifetime/security: race guest handle closure and duplication, fd reuse, OVERLAPPED address reuse, thread termination, cancellation and late completion. Assert exactly one guest terminal result and no cross-tenant cancellation.
6. Performance: compare a synchronous guest API backed by worker threads versus io_uring under matched load and record distributions, not just medians. Only after correctness gates pass.

## Open questions

- Which Windows synchronous operations beyond file reads are cancelable in practice on each supported Windows version and driver?
- How should an NT-native `NtCancelIoFile` / `NtCancelIoFileEx` layer relate to the Win32 wrappers? Requires separately pinned native declarations and differential testing.
- Can a backend cancel blocked host calls without creating signal-handler, allocator or lock-reentrancy hazards?
- Which IOCP notification cases differ for synchronous versus overlapped cancellation, including skip-on-success flags? Cross-check R111 and R137.

**Decision:** no kernel extension or Linux signal-based cancellation strategy is justified by these documents alone. Prototype a guest request registry and differential harness before choosing a backend.
