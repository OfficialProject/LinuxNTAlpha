# R137 — io_uring cancellation result versus NT I/O lifetime

Research date: 2026-11-17. **Status: primary-documentation analysis; no code or experiment executed.**

## Narrow unresolved question

R43 describes Windows cancellation and completion semantics. This note asks a different implementation question: **what can a Linux io_uring cancellation completion actually prove about the target operation, and when can a guest OVERLAPPED/buffer be released?** R15 compares I/O backends; neither establishes a safe per-request correlation protocol for cancel CQEs and target CQEs.

## VERIFIED FROM SOURCE (documentation, not runtime validation)

1. Microsoft [CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex) explicitly says a successful call requests cancellation but does not wait for the target to finish. The target can finish normally, be canceled, or fail for another reason. The OVERLAPPED storage cannot be reused until the target operation has completed.
2. Microsoft [Canceling Pending I/O Operations](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations) warns that a driver may not support cancellation and that concurrent I/O submission races with broad cancellation.
3. The liburing project's [io_uring cancellation manual](https://man7.org/linux/man-pages/man7/io_uring_cancelation.7.html) explicitly distinguishes a **cancel request CQE** from the **target request CQE**. Its documented races include the target having already completed, failed independently, or become noncancelable. It describes `-ENOENT` for no matching request and `-EALREADY` for cancellation already in progress, and notes that `IORING_ASYNC_CANCEL_ALL` reports a count in the cancellation CQE. This is liburing documentation, **not** proof of behavior on every kernel version or operation class.
4. The same liburing documentation describes cancellation matching by user data or file descriptor and flags for multiple matches. A guest `OVERLAPPED *` is not itself a safe, globally unique Linux cancellation key: applications can reuse pointer values after completion and the same host fd may serve multiple guest operations.

## HYPOTHESIS — explicit request registry

Maintain a runtime-assigned monotonically advancing **request generation/id**, separate from both the guest OVERLAPPED address and host fd. Store immutable guest handle identity, issuing guest thread, guest OVERLAPPED identity/generation, host operation id, and notification policy in a request record. Match host CQEs by opaque request id, not by a bare guest pointer. Hold request-owned host buffers and guest-memory safety obligations until a **target terminal outcome** has been processed. A cancel CQE alone must not release them.

Suggested states are `REGISTERED`, `SUBMITTED`, `TERMINAL`, `PUBLISHED`, with `cancel_requested` and `cancel_result` as separate fields. A terminal event must be idempotent; publication policy must separately account for IOCP suppression, event/APC delivery, and handle-close races. This is a design candidate, **not** a verified NT model.

### Failure modes that should block an implementation

- **ABA:** guest reuses an OVERLAPPED address while an old host cancellation CQE is in flight; cancellation must not affect the newer operation.
- **fd reuse:** guest closes a handle and the host reuses the numeric fd; cancellation by fd alone can select an unrelated operation.
- **cancel success mistaken for terminal:** freeing the buffer after a cancel CQE while the target CQE remains pending.
- **missing target CQE:** request bookkeeping is reclaimed on ring teardown without proving completion and reconciling guest notification obligations.
- **cross-thread mismatch:** `CancelIo` and `CancelIoEx` differ in issuing-thread scope, even when Linux cancellation uses the same ring.

## Falsifiable tests — NOT RUN

1. Use a controlled Linux pipe/socket and io_uring with distinct user_data for target and cancel. Submit target, then cancellation; collect **both** CQEs, including completion-before-cancel and cancel-before-completion races. Record exact kernel, liburing version, operation, flags and CQE results.
2. Repeatedly reuse host fd numbers and guest OVERLAPPED addresses while retaining generation-tagged request records; prove stale cancellations cannot select a newer request. Inject a delayed cancel CQE.
3. Exercise multiple simultaneous requests on one fd and cancellation by user_data versus fd, with and without ALL; verify the number and identity of target completions, not only cancel result.
4. Inject target failure, unsupported cancellation, ring teardown, and request timeout; assert no buffer release before terminal processing and no duplicate guest completion publication.
5. Separately run a pinned Windows-reference `CancelIo`/`CancelIoEx` matrix from R43. **Do not** call Linux tests NT conformance or extrapolate results across all filesystems/devices.

## Decision gate

Treat io_uring cancellation as a **best-effort host mechanism**, not the guest contract. No claim of Windows-compatible cancellation, safe guest pointer lifetime, or performance improvement is justified until the two-CQE accounting, generation identity and Windows-reference gates pass.

## Evidence limitations

Sources are live Microsoft and liburing manual pages inspected 2026-11-17; no source-code revision or Linux kernel version was pinned, no reference machine was available, and no benchmark or executable test was run. Linux version-dependent flags and error behavior require a pinned kernel-header/source audit before implementation.
