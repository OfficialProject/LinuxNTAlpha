# R269 — Per-handle completion notification suppression versus Linux I/O CQEs

Reviewed: 2026-11-23. Status: **documentation-grounded architecture analysis; no experiment performed**.

## Scope and nonduplication

R15, R43, R66, R225 and R255 address IOCP translation, cancellation, batching and Linux io_uring. This note isolates a separate **per-file-handle notification policy** that can change whether an otherwise completed request produces a guest IOCP packet or file-object wakeup. It does not claim a functioning backend.

## VERIFIED — primary Microsoft contracts

1. `SetFileCompletionNotificationModes(FileHandle, Flags)` accepts `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS = 0x1` and `FILE_SKIP_SET_EVENT_ON_HANDLE = 0x2`. Once a mode has been set for a handle, it cannot be removed through that API. A failed call returns zero and extended error is available via `GetLastError`. Source: [Microsoft, SetFileCompletionNotificationModes](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfilecompletionnotificationmodes).
2. The first flag suppresses an otherwise expected IOCP completion entry **only when all three conditions hold**: an associated completion port, an asynchronous-open file handle, and immediate success without `ERROR_PENDING`. Do not generalize it to pending requests, immediate failures, or arbitrary completion mechanisms. Same source.
3. The second flag suppresses signaling of the **file object** for the documented success/pending cases; an explicitly supplied request event is still signaled. It is not a blanket suppression of all waitable objects. Same source.
4. Independently, `GetQueuedCompletionStatus` documents that setting the low-order bit of a valid `OVERLAPPED.hEvent` suppresses IOCP packet enqueueing for that overlapped request. Its failed-dequeue and timeout return forms must remain distinguishable from a completed failed I/O. Source: [Microsoft, GetQueuedCompletionStatus](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus).
5. Microsoft documents a socket-specific qualification for the first flag: layered providers must return IFS handles. This is a provider compatibility constraint, not permission to assume all sockets behave like disk files. Source: first Microsoft API reference above.

## HYPOTHESIS — implementation boundary

Keep a guest-owned **notification policy record** associated with the correct Windows handle/file-object identity, alongside each request's captured `OVERLAPPED` event flags and IOCP association. A Linux `io_uring` CQE or worker completion is a **backend fact**, not automatically a guest completion packet. At terminal request transition, a single guest completion arbiter should independently decide (a) guest I/O status publication, (b) explicit event signaling, (c) file-object signaling and (d) IOCP enqueue. This is a proposed design, not a verified Windows internal implementation.

The precise sharing of notification modes across duplicated handles, and the interaction with reassociation or concurrent flag-setting, are **OPEN**. Do not infer identity or atomicity from the phrase 'file handle' alone. The low-bit `hEvent` rule must be considered separately from handle-level suppression.

## Unrun conformance gates

- **Windows reference, same handle:** create an overlapped file associated with an IOCP; compare immediate success, pending success, immediate failure, pending failure and cancellation with flags 0, 1, 2, 3. For each case capture API return, `GetLastError`, `OVERLAPPED.Internal`, explicit event, file-handle wait and IOCP dequeue with a bounded timeout. Ensure a request is actually observed to return `ERROR_IO_PENDING` before labeling it pending.
- **Per-request override:** repeat with a valid `OVERLAPPED.hEvent` whose low bit is set, and separately with a normal explicit event. Check whether packet suppression and event signaling are independent.
- **Identity/lifetime:** test `DuplicateHandle` before and after setting flags; close one alias; check whether another alias inherits observable behavior. Test unsupported handle types and invalid flag combinations without assuming outcomes.
- **Race and publication:** issue concurrent overlapped operations while changing modes, closing handles, canceling I/O and dequeuing packets. Record exactly-once terminal state, no fabricated packet and no lost explicit-event signal.
- **Linux backend:** in an isolated harness, synthesize both immediate and deferred backend completions and verify the proposed arbiter emits the Windows-reference-visible sequence. Do not use raw CQE counts as guest IOCP packet counts.

## Security and performance implications

A wrongly generated completion packet can double-complete an application's request; a wrongly suppressed packet can deadlock its consumer. Conversely, the skip-on-success fast path may avoid guest queue operations, but **no latency or throughput benefit is established** until measured with equivalent Windows and Linux workloads. Guest pointer validation, handle-right checks and completion ownership remain prerequisites.

## Source and evidence ledger

- Microsoft Win32 `SetFileCompletionNotificationModes`: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfilecompletionnotificationmodes (accessed 2026-11-23).
- Microsoft Win32 `GetQueuedCompletionStatus`: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus (accessed 2026-11-23).
- Related project research: R15, R43, R66, R225, R255. No upstream source code was copied; no Windows reference run, Linux experiment or benchmark was performed.
