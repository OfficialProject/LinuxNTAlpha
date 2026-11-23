# R255 — Batched IOCP dequeue, alertable waits, and Linux completion-ring boundaries

Date: 2026-11-22. Status: **primary-documentation research only**. Microsoft and Linux documentation were consulted in this run. **No Windows reference execution, Linux prototype, benchmark, or compatibility claim.**

## Question and relation to earlier work

[R15](R15_IOCP_IO_URING.md) establishes that an io_uring CQ is not an IOCP scheduler; [R43](R43_ASYNC_IO_CANCELLATION_COMPLETION.md) identifies failed-packet versus no-packet distinctions. This note narrows a remaining integration boundary: a *single* Windows `GetQueuedCompletionStatusEx` call may consume a batch, optionally enter an alertable wait, and associate the waiting thread with a port. None of these are implied by an io_uring CQE batch.

## VERIFIED — Windows API contracts (documentation, not observed test results)

1. `GetQueuedCompletionStatusEx(port, entries, count, removed, timeout, alertable)` removes **up to** `count` entries into caller-provided `OVERLAPPED_ENTRY` storage; `removed` reports the number actually removed. A successful call can contain failed I/O operations, so callers must inspect the corresponding `OVERLAPPED` status rather than treating the batch boolean as a per-operation result. [Microsoft GetQueuedCompletionStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatusex).
2. A zero timeout polls, `INFINITE` does not time out, and `fAlertable=TRUE` performs an alertable wait **when no entries are available**. The documentation ties alertable wakeup to an APC or initiating-thread I/O completion routine. The page does **not** specify a universally applicable return/error code for every APC-vs-packet race; measure rather than invent one. [Microsoft GetQueuedCompletionStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatusex).
3. The call associates its calling thread with the port; a thread can be associated with at most one port. Queued completions are FIFO, while a waiting call can receive one or more queued entries for that **one** thread. The documentation distinguishes `FALSE` with no entries removed from successful batches; closing the port while the call waits can yield `ERROR_ABANDONED_WAIT_0`. [Microsoft GetQueuedCompletionStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatusex).
4. The public `OVERLAPPED_ENTRY` structure exposes completion key, `OVERLAPPED*`, `Internal` and transferred-byte count; Microsoft labels the `Internal` field **reserved**. Do not infer that it is a stable, documented per-entry NTSTATUS API. [Microsoft OVERLAPPED_ENTRY](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped_entry).
5. Microsoft documents a version-dependent low-power timeout accounting distinction: Windows 8/Server 2012 and later exclude low-power-state time, unlike earlier listed versions. Version-pin the reference system before testing suspend/resume. [Microsoft GetQueuedCompletionStatusEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatusex).

## VERIFIED — Linux backend contracts (documentation, not Windows equivalence)

- `io_uring_enter(2)` submits work and optionally waits for completions through a Linux ring. Its `min_complete`, ring flags and feature requirements describe **host** CQ readiness, not a Windows port's thread association, APC dispatch, guest completion-key ownership or guest packet dequeue. [Linux io_uring_enter(2)](https://man7.org/linux/man-pages/man2/io_uring_enter.2.html).
- `io_uring_wait_cqe_nr(3)` is a liburing wait for a requested number of CQEs; its return value reports wait success/failure rather than the individual CQE operation result. It does not define Windows `GetQueuedCompletionStatusEx` semantics. [liburing io_uring_wait_cqe_nr(3)](https://man7.org/linux/man-pages/man3/io_uring_wait_cqe_nr.3.html).

## HYPOTHESIS — guest-owned dequeue adapter (not an architecture decision)

Maintain a guest IOCP packet queue independently of backend CQ consumption. On each guest dequeue, validate guest handle rights and output-array bounds, select at most `ulCount` *guest* packets, atomically assign them to one waiting guest thread, and publish the count only after guest output writes succeed under a defined fault policy. Keep the initiating-thread APC/completion-routine queue separate from IOCP packets; an alertable wait should consult guest APC eligibility rather than map host signal delivery to an APC by default. Preserve guest thread-to-port association and configured worker-admission accounting independently of io_uring ring ownership. These are proposed implementation invariants requiring differential evidence, not verified Windows internal algorithms.

**Security concern:** never pass guest-supplied `OVERLAPPED*` or output-array pointers to a privileged backend as trusted host addresses; retain a generation-tagged request/object identity and revalidate guest writes. Host CQEs must not be allowed to mutate recycled guest handles or resurrect closed guest ports.

## Proposed Windows-reference conformance matrix — NOT RUN

1. Post 0, 1, 2, and 8 synthetic packets, call `GetQueuedCompletionStatusEx` with capacities 1, 2 and 8 and timeouts 0/finite/INFINITE. Record return, removed count, order, keys, pointer identity and GetLastError **only where applicable**.
2. Mix successful and failed real overlapped operations with synthetic packets; compare batched API with single-packet `GetQueuedCompletionStatus` without assuming the reserved `Internal` field has a stable contract.
3. Run two or more waiters on one port with different batch capacities; observe packet distribution and worker admission under concurrency 1 and 2. Do not infer scheduling solely from packet FIFO.
4. Queue a user APC to the *waiting guest thread* while the port is empty and `fAlertable=TRUE`; repeat with `FALSE`, and race APC delivery with packet posting. Record callbacks, returns, removed count and timeouts on a specified Windows build.
5. Close the last port handle while a thread waits, then separately close an associated file handle; record behavior and object lifetimes. Repeat with process/thread exit and with no pending I/O.
6. Test invalid `ulCount`, invalid output pointers and output-buffer page boundaries in a disposable process; record actual Windows errors before specifying the guest fault/rollback policy.
7. On Linux, prototype a deterministic **in-memory** guest packet scheduler first, injecting APC/post/close/cancel races; only then connect io_uring or an alternative backend. Log every host CQE and guest dequeue separately.

## OPEN

The exact Windows-visible APC-interruption return/error convention under all races, partial guest-output faults, batched fairness, version-dependent suspend accounting, and interaction with IOCP concurrency require Windows oracle runs. Kernel-version-specific liburing feature negotiation and backend performance remain untested. This note adds no executable code and makes no claim that the project currently implements any of the above.

## Architectural implication

Treat batched IOCP dequeue as a guest scheduling and state-publication operation, not a bulk copy of Linux CQEs. Keep ADR-003 open pending [R235](R235_ARCHITECTURE_SELECTION_EVIDENCE_GATES.md) experimental gates.
