# R153 — Threadpool I/O notification accounting and overlapped lifetime gates

Status: PRIMARY-SOURCE REVIEW (2026-11-18 UTC). No experiment run. No implementation or performance claim.

## Why this is distinct

R111 covers IOCP completion suppression, R137 covers io_uring cancellation and target CQE lifetime, R145 covers synchronous cancellation, and R149–R152 cover other threadpool callback types. This note addresses the *PTP_IO registration/notification accounting* boundary: a successful host I/O completion is not necessarily a Windows threadpool callback, and canceling a callback does not cancel the I/O.

## VERIFIED FROM SOURCE — Microsoft public API contracts

1. `CreateThreadpoolIo` associates a file handle, callback, context and optional callback environment with a threadpool I/O object. `StartThreadpoolIo` must precede **each** asynchronous operation for callback notification. Microsoft explicitly warns that omitting it may lead to ignored completions and memory corruption. Sources: [CreateThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolio), [StartThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-startthreadpoolio).
2. `CancelThreadpoolIo` cancels a notification registered by Start, **not** the underlying I/O. Microsoft requires it when the operation fails to initiate (other than ERROR_IO_PENDING), and when immediate success is suppressed by FILE_SKIP_COMPLETION_PORT_ON_SUCCESS. Source: [CancelThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-cancelthreadpoolio).
3. `WaitForThreadpoolIoCallbacks(pio, TRUE)` cancels callbacks not yet executing but does **not** cancel pending I/O. Microsoft cautions against freeing OVERLAPPED until the request is complete; GetOverlappedResult and optionally CancelIoEx are relevant to request lifetime. Source: [WaitForThreadpoolIoCallbacks](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-waitforthreadpooliocallbacks).
4. `CloseThreadpoolIo` frees immediately if no outstanding callbacks, otherwise asynchronously after callbacks finish. Microsoft instructs callers to close the associated file handle and await outstanding overlapped operations before closing the threadpool I/O object, with no further operations afterward. Source: [CloseThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolio).

These are currently published API pages, **not** a pinned Windows build-specific internal implementation.

## HYPOTHESIS — proposed Linux-hosted runtime design, not verified

Model separately (a) a guest PTP_IO registration, (b) a per-Start notification credit, (c) a uniquely identified in-flight guest OVERLAPPED operation, (d) a terminal backend completion, and (e) a queued/running guest callback. Keep generation-tagged object and operation identities; do not use a reused host fd or OVERLAPPED address alone as a permanent identity. An immediate-success suppressed completion should retire the backend request while the matching notification credit is explicitly canceled; a queued callback cancellation should not release in-flight request buffers. Ensure callback environment and DLL lifetime are held until execution completes.

**Unresolved:** exact mapping of concurrent Start/Cancel calls to multiple operations; callback ordering; what Windows does on application misuse; interaction with custom IOCP association and completion modes. Public API pages do not settle these. Avoid asserting a one-to-one matching algorithm as Windows behavior without differential tests.

## Falsification gates (not executed)

- Windows reference program: create overlapped file/pipe, register PTP_IO; compare pending completion, immediate success with and without skip-on-success, failed initiation, and explicit CancelThreadpoolIo. Log return status, last error, callback count, OVERLAPPED address and completion status. Use bounded waits and independent request completion checks.
- Teardown: queue callback while worker is blocked, invoke WaitForThreadpoolIoCallbacks(TRUE/FALSE), verify queued versus executing callback behavior; separately prove underlying I/O completion before freeing buffers. Never deliberately access freed memory.
- Concurrency: multiple simultaneous requests on one handle, interleaved Starts, cancellation and close, repeated allocation/reuse of guest and host identities; track every credit and operation through terminal states.
- Linux prototype: mock backend first with deterministic completion and cancellation interleavings; then optionally io_uring on a pinned kernel/liburing build. Record CQEs, completion result, request ownership, skip cases, and callback counts. Do not treat mock results as Windows conformance.
- Security: ensure untrusted guest code cannot forge backend completion IDs, double-retire credits or induce a use-after-free through close/cancel races.

## Architectural consequence

Do **not** implement Windows threadpool I/O as simply `io_uring completion => invoke callback`. A guest-facing notification accounting layer and independent request-lifetime tracking are needed to meet the documented API obligations. Whether the actual runtime should use io_uring, epoll or another host backend remains OPEN and requires measurements.

## Evidence boundary

Only the four linked Microsoft API pages were directly reviewed for this note. No Windows reference binary, Linux probe, benchmark or source-level NT internals were executed or inspected. Related research: R111, R137, R145, R149–R152.
