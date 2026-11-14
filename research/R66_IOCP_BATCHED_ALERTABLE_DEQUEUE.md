# R66 — IOCP batched dequeue and alertable-wait boundary

Date: 2026-11-13. Status: **PRIMARY-DOCUMENTATION REVIEW ONLY; NO EXPERIMENT**.

## Scope

R15 covers port scheduling and io_uring; R43 covers cancellation and suppression. This note narrows the unresolved *consumer-side* boundary: `GetQueuedCompletionStatusEx` batching, per-entry result interpretation, alertable interruption, and the distinction between an IOCP packet and an io_uring CQE. It does not propose a completed NT service implementation.

## VERIFIED — documented Windows API behavior

1. [GetQueuedCompletionStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatusex) accepts a caller-allocated `OVERLAPPED_ENTRY` array, a maximum entry count, an output count and `fAlertable`. It can remove multiple packets in one call. The count is an output, not an assertion that the full requested batch was returned.
2. The same Microsoft page specifies that `fAlertable=TRUE` permits an alertable wait **when no entries are available**; APC/completion-routine execution can end the wait. A compatibility layer must not interpret every wake as a dequeued packet. The exact last-error and output-count values for every APC race are **not established by this documentation review**.
3. Microsoft documents FIFO ordering of queued packets and that one or more requests can wake a single waiting thread; the order in which workers actually process packets is not guaranteed to be FIFO. The single-entry API [GetQueuedCompletionStatus](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus) distinguishes a failed dequeued I/O (`FALSE`, non-NULL `OVERLAPPED`) from no dequeued packet (`FALSE`, NULL `OVERLAPPED`).
4. [GetQueuedCompletionStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatusex) returns entries containing bytes transferred, completion key and original `OVERLAPPED` identity; it does not give the same single-operation failure boolean as `GetQueuedCompletionStatus`. The implementation must preserve each entry's status information and not classify the whole batch by one boolean.
5. [Linux io_uring(7)](https://man7.org/linux/man-pages/man7/io_uring.7.html) describes CQEs containing `user_data`, `res` (negative errno for errors) and flags. Requests can complete out of submission order. Its CQ consumer and the Windows port's waiting-thread/concurrency semantics are distinct abstractions.

## HYPOTHESIS — architectural separation

Use an NT-facing **port packet queue and waiter arbiter** independent of any backend CQ. Backend completion ingestion should translate Linux results into stable guest operation outcomes before publishing packets. A single guest dequeue operation must atomically claim up to the requested number of available packets, preserving packet order within the claimed batch, while the waiter arbiter separately handles port concurrency limits and alertable interruption. This is a proposed design, **not verified to match Windows under all races**.

A backend `io_uring` CQE must never directly become an `OVERLAPPED_ENTRY` without checking completion-notification policy, guest operation identity and NT/Win32 error translation (R43/R52). User-posted packets (`PostQueuedCompletionStatus`) must also work without a backend I/O CQE.

## Proposed Windows reference conformance gates — NOT RUN

1. Create one port and post three uniquely keyed synthetic packets. Dequeue with capacities 1, 2 and 8; record returned count, packet order and remaining queue contents.
2. Test `GetQueuedCompletionStatusEx` on an empty port with zero timeout, finite timeout and infinite wait; record return, `GetLastError`, output count and unchanged/sentinel array bytes. Do not assume undocumented output values.
3. With `fAlertable=TRUE`, arrange a user APC to a blocked waiting thread and compare against `FALSE`. Capture whether an entry was consumed, return/error, APC execution thread and queue contents afterward.
4. Race APC delivery with `PostQueuedCompletionStatus`; repeat under multiple waiting threads and capacities, recording both packet loss/duplication and worker identity. Compare against single-entry `GetQueuedCompletionStatus`.
5. Generate one successful and one failed overlapped I/O on an associated handle; compare single-entry and batched dequeue, including each `OVERLAPPED_ENTRY.Internal` status and the API return/error. Check the target Windows SDK structure definition before asserting precise NTSTATUS representation.
6. Close the port while an Ex waiter blocks; record target Windows build and failure status. Re-run with queued packets and multiple waiters. Avoid inferring all handle-lifetime behavior from one build.

## Proposed isolated Linux prototype — NOT RUN

Implement a deterministic in-memory packet queue and waiter state machine with injected APC, timeout, close and packet-publication events; exhaustively enumerate short interleavings before integrating io_uring. Assertions: no packet consumed twice, no packet silently discarded by APC interruption, no capacity overrun, consistent returned count, and no backend errno leaking as a guest status. These are **proposed invariants**, subject to Windows-oracle results.

## OPEN / decision gate

- Windows-build-specific APC versus packet race outcomes, precise `OVERLAPPED_ENTRY.Internal` mapping, and handle-close lifetime remain unmeasured.
- The precise scheduler behavior of batched dequeues under a configured port concurrency cap requires a Windows reference and must not be inferred from io_uring.
- No kernel modification or io_uring backend choice is justified by this note.

**Decision:** retain a separately testable IOCP packet/waiter abstraction; defer backend selection and performance claims until reference behavior and Linux prototype agree.
