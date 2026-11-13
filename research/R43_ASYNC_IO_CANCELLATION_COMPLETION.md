# R43 — Asynchronous I/O cancellation and completion-suppression gates

Date: 2026-11-12. Status: **VERIFIED FROM MICROSOFT PUBLIC DOCUMENTATION; NO EXPERIMENT**.

## Scope and relationship to R15

R15 identifies IOCP scheduling versus io_uring but leaves cancellation and completion suppression unmodeled. This note isolates the observable Win32 API contracts that a Linux backend must preserve. It does not establish NT-native `NtCancelIoFileEx` semantics or choose an I/O backend.

## VERIFIED FROM PRIMARY DOCUMENTATION

1. [CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex) requests cancellation of outstanding I/O for a handle issued by the **current process**, including requests from other threads. A non-NULL `OVERLAPPED` argument selects matching requests; NULL targets all outstanding requests on that handle. This differs from [CancelIo](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelio), which targets I/O issued by the calling thread.
2. Successful `CancelIoEx` means cancellation was **requested**, not that the operation has completed or was canceled. The operation can finish normally, finish canceled with `ERROR_OPERATION_ABORTED`, or fail for another reason. `ERROR_NOT_FOUND` means no matching request was found. The caller must not reuse/free the associated `OVERLAPPED` until completion. Source: [CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex), [Canceling Pending I/O Operations](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations).
3. For pending asynchronous operations associated with a completion port, cancellation still results in a completion packet; the CancelIoEx documentation distinguishes successfully canceled synchronous operations, for which no port packet is queued. This does **not** imply every canceled request yields an aborted status. Source: [CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex).
4. [SetFileCompletionNotificationModes](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfilecompletionnotificationmodes) permits `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS` (0x1): for an associated asynchronous handle, a request that succeeds immediately without `ERROR_PENDING` does not queue its otherwise-normal port entry. The mode is sticky once set. `FILE_SKIP_SET_EVENT_ON_HANDLE` (0x2) changes signaling of the **file object**, while an explicitly provided event remains separately relevant. These flags are not a universal suppression of failed or pending completions.
5. [GetQueuedCompletionStatus](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus) distinguishes three observable cases: TRUE and dequeued successful packet; FALSE with non-NULL `OVERLAPPED` and dequeued failed packet; FALSE with NULL `OVERLAPPED` and **no packet** (including timeout). Closing the port while a wait is outstanding yields `ERROR_ABANDONED_WAIT_0` on supported modern Windows, with documented older-version differences. A boolean return alone cannot encode whether a packet was consumed.
6. [GetQueuedCompletionStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatusex) can return TRUE after dequeuing entries even if individual operations failed; each returned entry's status must be examined. This differs from the single-packet API's failure convention.

## HYPOTHESIS — minimum compatibility model

Represent each guest asynchronous operation with a stable identity comprising guest handle/object reference, issuing guest thread, `OVERLAPPED` pointer plus generation/lifetime, completion target and an explicit terminal outcome. Cancellation is a **request transition**, not a terminal state. Separate the backend operation's outcome from whether a Windows-visible packet/event/APC should be emitted.

A tentative transition graph is `SUBMITTED -> PENDING -> {COMPLETED_OK, COMPLETED_ERROR, COMPLETED_CANCELED}`, with `CANCEL_REQUESTED` as an orthogonal flag. Exactly-once terminal publication is a **proposed invariant**, not yet a verified Windows-wide property: different notification channels and API-specific exceptions require Windows-reference testing. Never free guest buffers or `OVERLAPPED` memory on cancellation request alone.

An io_uring cancel CQE, if used, is a backend result; it must not automatically be treated as the canceled guest I/O's completion packet. The backend may race with normal completion. No io_uring kernel-version-specific behavior is asserted in this note.

## Falsifiable Windows-reference matrix — NOT RUN

- Async file or named-pipe operation: issue pending I/O, cancel from the same thread and a different thread; compare `CancelIo` versus `CancelIoEx`; record return/error and eventual completion independently.
- Repeat with `OVERLAPPED=NULL` and with a specific pointer, including two pending requests and a nonmatching pointer; distinguish `ERROR_NOT_FOUND` from eventual completion.
- Race cancellation against successful completion using a controllable pipe peer; record every terminal outcome, number of packets and `OVERLAPPED` reuse timing.
- With and without `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS`, test immediate success, pending success, pending cancellation and failure; use a known asynchronous handle and inspect `GetQueuedCompletionStatus` with a finite timeout.
- Independently test `FILE_SKIP_SET_EVENT_ON_HANDLE` and a separately supplied `OVERLAPPED.hEvent`; avoid conflating file-object and explicit-event signaling.
- Dequeue a failed operation through `GetQueuedCompletionStatus` and `GetQueuedCompletionStatusEx`; assert distinct return/entry-status conventions. Close a port during a blocked dequeue on a pinned Windows build.
- Later Linux prototype: inject deterministic completion/cancel races into an in-memory state machine before introducing io_uring. Verify one terminal operation result, correct packet suppression, no early buffer release, and no loss of failed completion metadata.

Record Windows build, filesystem/device, handle flags, operation type, thread identity, `OVERLAPPED` addresses, API return values and errors. Use disposable fixtures; never infer semantics from timing-only tests.

## OPEN

- Native NT I/O status and cancellation API mapping; socket/AFD and filesystem-specific exceptions.
- Completion suppression using low-bit `OVERLAPPED.hEvent` and other notification mechanisms needs separate primary-source audit and oracle tests.
- Port association and handle-close races, user-buffer pinning, thread exit, APC delivery, and callback lifetime remain unresolved.
- No Linux backend, latency advantage, or application compatibility has been demonstrated.

## Decision

Keep cancellation bookkeeping and Windows-visible notification policy above any proposed io_uring/epoll backend. Do not equate a cancellation request with successful cancellation or a backend CQE with a guest IOCP packet.
