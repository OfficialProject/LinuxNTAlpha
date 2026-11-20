# R196 — Per-request OVERLAPPED event low-bit IOCP suppression

Research date: 2026-11-20 UTC. **Primary documentation review only**; no Windows or Linux tests run.

## Distinct gap

R43 identifies the low-bit event-handle rule as OPEN; R190 studies sticky *per-handle* completion notification flags. This note isolates the **per-request** low-order-bit rule, event synchronization and the result-query path. Neither a Linux backend completion nor a sticky per-handle setting can replace this request-specific choice.

## VERIFIED FROM SOURCE (public Microsoft API contracts)

1. Microsoft explicitly documents that, even for a file associated with an I/O completion port, supplying a **valid event handle** in `OVERLAPPED.hEvent` with its **low-order bit set** prevents that overlapped operation's completion from enqueueing a packet to the port. This is a *per-operation* suppression condition, not the `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS` rule. [M1]
2. `OVERLAPPED` contains `Internal`, `InternalHigh`, offset/pointer union and `hEvent`. Microsoft documents `Internal` as pending/completion status and `InternalHigh` as transferred-byte count on error-free completion, while warning that those members' behavior may change. The structure must not be reused before the previous operation completes. Manual-reset events are recommended; an auto-reset event can be consumed by an intervening wait, causing a later result wait to block. [M2]
3. `GetOverlappedResult` can wait for the explicit event, or (when `hEvent` is NULL) for the file handle; the latter is ambiguous with multiple pending operations. With `bWait=FALSE`, a pending operation reports `ERROR_IO_INCOMPLETE`. [M3]
4. `GetOverlappedResultEx` adds timeout and alertable-wait behavior. Microsoft documents `ERROR_IO_INCOMPLETE` for zero timeout with pending work, `WAIT_TIMEOUT` for an elapsed nonzero timeout and `WAIT_IO_COMPLETION` for an alertable completion/APC interruption; it warns about auto-reset event reuse. [M4]
5. `CancelIoEx` requests cancellation but does not wait for completion; the caller must retain the `OVERLAPPED` until final completion, and the operation may finish normally, canceled or failed. Suppressing an IOCP packet must not imply the underlying operation has retired. [M5]
6. Linux `io_uring` supplies backend CQEs identified by `user_data`, with results in `res`; backend CQE arrival is not itself a Windows guest IOCP packet. [L1]

## HYPOTHESIS — adapter invariants (not Windows internals)

- At submission, record the validated guest event object reference and a request-local `suppress_iocp_packet` decision derived from the documented low-bit rule. Preserve the original guest-visible `hEvent` value; do **not** blindly pass its tagged integer to Linux `eventfd`, `epoll` or a host handle table.
- Maintain distinct request completion status, bytes transferred, explicit-event signaling, file-object signaling, IOCP publication and buffer lifetime. A suppressed packet does **not** suppress status publication or make a canceled request safe to free.
- Treat `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS` (R190) as a separate per-handle predicate evaluated against the operation's immediate/pending return classification. Avoid using Linux CQE timing to decide whether the Windows request was immediate.
- Serialize retirement against cancel, handle closure and `OVERLAPPED` reuse. An event-handle bit is **not** a security authority: resolve and validate the underlying guest event object with normal handle access rules.

## Falsification matrix — NOT RUN

On a pinned Windows build, create an overlapped file or controlled named-pipe pair, associate an IOCP, and use a **distinct manual-reset event per request**. Test ordinary event handle versus same valid handle with low bit set; include immediate completion and reliably pending completion. Record initial API return and error, event state, `OVERLAPPED.Internal/InternalHigh`, `GetOverlappedResult(Ex)`, and finite-timeout `GetQueuedCompletionStatus` outcome. Add an operation without an explicit event; test independent concurrent requests to expose file-handle signaling ambiguity.

Repeat with `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS` enabled and disabled, and with cancellation requested before and after completion. Verify that a suppressed packet does not conceal final status or allow premature `OVERLAPPED` reuse. Negative tests: invalid tagged handles, NULL hEvent, auto-reset event, event closure during pending I/O and unexpected failure; **do not predeclare undocumented error codes or exact race winners**. Run each potentially blocking case with a bounded watchdog and clean disposable resources.

For a Linux-only mock, inject backend completion/cancel orderings and assert that per-request suppression never drops the internal terminal outcome. This would validate bookkeeping only, not Windows conformance. Record kernel/build, API arguments, operation class and exact results. No measurements are available yet.

## OPEN

- Whether low-bit tagging is accepted consistently across every overlapped-capable device/provider and all targeted Windows versions.
- Exact signaling and error outcomes for invalid/closed tagged event handles, and interaction with `FILE_SKIP_SET_EVENT_ON_HANDLE`.
- Native NT `IO_STATUS_BLOCK` / `NtReadFile` event tagging correspondence; do not infer it solely from Win32 documentation.
- Thread/APC completion interactions, duplicate handles, and close/cancel races.

**Decision: OPEN.** Implement no blanket 'one backend CQE = one guest IOCP packet' rule. Require Windows differential evidence before selecting the adapter behavior for undocumented edges.

## Primary sources inspected

- [M1] Microsoft, GetQueuedCompletionStatus: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus
- [M2] Microsoft, OVERLAPPED: https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped
- [M3] Microsoft, GetOverlappedResult: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult
- [M4] Microsoft, GetOverlappedResultEx: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresultex
- [M5] Microsoft, CancelIoEx: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex
- [L1] Linux man-pages, io_uring(7): https://man7.org/linux/man-pages/man7/io_uring.7.html

No third-party code copied. No experiment, Windows oracle result, benchmark or application compatibility is claimed.
