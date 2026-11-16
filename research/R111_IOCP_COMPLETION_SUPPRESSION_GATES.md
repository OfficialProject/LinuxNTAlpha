# R111 — Per-handle completion suppression and IOCP packet provenance

Status: **VERIFIED FROM PRIMARY DOCUMENTATION** for the public Win32 contracts; **HYPOTHESIS** for Linux-hosted design; **NO EXPERIMENT**. Reviewed 2026-11-15.

## Architectural question

A Linux async I/O backend (including io_uring) cannot decide whether a Windows completion-port packet exists solely from whether a host operation finished. Windows completion behavior also depends on **guest file-handle notification mode**, per-request OVERLAPPED event bits, immediate versus pending return, and association with a completion port. This note isolates packet *suppression*, not general IOCP queue scheduling (R15/R43/R66).

## Verified contracts

1. [Microsoft SetFileCompletionNotificationModes](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfilecompletionnotificationmodes) documents two additive, non-removable modes per file handle. `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS` suppresses the packet only if the handle is associated with a port, opened for asynchronous I/O, and the request returns immediate success **without ERROR_PENDING**. `FILE_SKIP_SET_EVENT_ON_HANDLE` suppresses file-object event signaling under its documented conditions, while an explicitly supplied event is still signaled. Socket LSP/IFS restrictions are documented separately.
2. [Microsoft GetQueuedCompletionStatus](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus) documents an independent suppression mechanism: the low bit of a valid OVERLAPPED.hEvent suppresses the port packet. A failed dequeue with non-NULL OVERLAPPED represents a dequeued failed operation; NULL OVERLAPPED indicates no packet was dequeued. A closed port may produce ERROR_ABANDONED_WAIT_0 on supported systems.
3. [Microsoft CreateIoCompletionPort](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-createiocompletionport) documents that a file handle may be associated with only one completion port for its lifetime and that a completion key is attached on association.
4. [Microsoft I/O Completion Ports](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports) documents completion-packet FIFO ordering and waiting-thread LIFO release, with concurrency accounting. Those are guest-visible policies distinct from host backend completion order.

These are documented Windows API contracts, **not** evidence that a Linux implementation or a specific Windows build was tested.

## Hypothesis: broker boundary

Keep per-guest-handle immutable-once-enabled notification flags and port association; keep per-request state containing the OVERLAPPED identity, explicit event, hEvent low bit, initial guest return classification (immediate success / pending / immediate failure), final completion status, and exactly-once completion transition. The guest I/O service decides whether to enqueue a packet from that state; io_uring CQEs or worker completion callbacks are only backend evidence of work completion.

An operation which completed quickly on Linux is **not necessarily** a Windows immediate-success operation: guest-visible synchronous return classification must be established before applying skip-on-success. Conversely, a guest operation returned pending should not retroactively be labeled immediate because its host completion raced the return path. The precise synchronization protocol is OPEN and needs a race-focused prototype.

Do not confuse file-object event signaling with explicit OVERLAPPED event signaling. Do not treat suppression of a port packet as cancellation or absence of an I/O result.

## Falsification plan — not executed

On a disposable Windows reference VM, record OS build, handle creation flags, file type, completion-port key, OVERLAPPED address and hEvent value, call return, GetLastError, event states and dequeued packets.

1. Baseline overlapped file with associated IOCP; compare immediate successful operations and forced pending operations, without either suppression mode.
2. Enable skip-on-success before operations; repeat immediate success, pending-success and pending-failure; check packet counts with bounded GQCS timeouts. A timeout alone does not establish suppression without confirming operation completion separately.
3. Compare valid explicit events with low-bit clear/set, with and without skip-on-success. Verify event signaling independently of packet suppression.
4. Enable skip-set-event-on-handle; distinguish file-object signaling from explicit OVERLAPPED event signaling.
5. Attempt to remove a previously enabled mode; test invalid flags and unsupported handle classes, recording actual error codes instead of assuming them.
6. Race operation completion against port close, handle close and cancellation; verify no duplicate packets, no use-after-free of guest OVERLAPPED storage, and correct failed-packet versus no-packet distinction.
7. Repeat with a named pipe and an IFS socket where available; do not generalize regular-file observations to every I/O provider.

## Open questions / decision

- Precise native NtSetInformationFile/FileIoCompletionNotificationInformation mapping and file-object versus duplicated-handle scope require upstream native source or Windows differential tests.
- Exact behavior of immediately failed operations and exceptional providers must be measured rather than inferred from the skip-on-success rule.
- The minimum lock/state machine required to preserve the guest return classification across backend races remains unproven.

**Decision impact:** IOCP packet production must remain in the guest I/O semantics layer; do not directly expose Linux io_uring CQEs as Windows completion packets. No claim of performance or compatibility is made.
