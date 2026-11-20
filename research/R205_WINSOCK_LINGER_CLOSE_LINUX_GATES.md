# R205 — Winsock linger, socket close and Linux lifetime gates

Status: **documentation-only research**, 2026-11-20. No Windows reference run, Linux experiment, or implementation was performed.

## Scope and relation to prior work
R195 covers general asynchronous request ownership; R204 covers event latching; recent socket duplication/bind research covers different questions. This note isolates **close-time transport policy**, especially the nonblocking linger failure that cannot be implemented by blindly calling Linux `close(2)`.

## VERIFIED — published primary contracts

1. Microsoft's current Winsock2 `closesocket` contract states that default disabled linger returns immediately while connection teardown proceeds gracefully in the background; enabled linger with zero timeout causes abortive close; positive linger on a **blocking** socket waits for delivery or timeout. On a **nonblocking** socket with positive linger, a close that cannot complete immediately returns `WSAEWOULDBLOCK`; the socket remains valid, disconnect has not begun, and the caller must retry. It also warns that socket values may be immediately reused and that concurrent `closesocket` and other Winsock calls are invalid. [Microsoft closesocket](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-closesocket).
2. The same Microsoft page states that outstanding overlapped sends/receives are canceled on successful close, with the specified event, completion routine, or IOCP completion action performed and `WSA_OPERATION_ABORTED` status. `FD_CLOSE` is not posted *after* the application calls `closesocket`. [Microsoft closesocket](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-closesocket).
3. Microsoft explicitly recommends `shutdown` before `closesocket` when an application needs to ensure both directions' data exchange; `shutdown` itself does **not** block regardless of SO_LINGER. [Microsoft shutdown](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-shutdown).
4. Linux `socket(7)` describes positive `SO_LINGER` as making `close(2)` or `shutdown(2)` wait for queued data or timeout; closure during `_exit(3)` lingers in the background. This differs from Microsoft's documented nonblocking-close retry and nonblocking shutdown contracts. [Linux socket(7)](https://man7.org/linux/man-pages/man7/socket.7.html).
5. POSIX `close(3p)` states that socket linger blocking is **not conditional on O_NONBLOCK** and that pending asynchronous I/O cancellation upon close is implementation-defined. [POSIX close(3p)](https://man7.org/linux/man-pages/man3/close.3p.html).
6. Linux io_uring cancellation documentation notes that closing the application's fd does not automatically cancel pending io_uring operations because the ring can hold references. [liburing cancellation](https://man7.org/linux/man-pages/man7/io_uring_cancelation.7.html).

## HYPOTHESIS — candidate guest architecture (not selected)
Maintain a per-guest-socket state machine with OPEN, CLOSE_ATTEMPT, CLOSE_COMMITTED and DEAD states. For positive linger on nonblocking guest sockets, decide whether the Windows call would fail **before** invalidating the guest handle; on `WSAEWOULDBLOCK`, preserve the handle, outstanding request ownership, and transport state. On committed close, atomically revoke the guest handle identity, mark owned outstanding requests for cancellation, and deliver each prescribed guest completion exactly once, independently of when the Linux fd is finally released. Host fd reference lifetime and guest socket identity must be separate; Linux `close` alone cannot represent the guest operation.

This is a design proposal, not proof that Linux exposes a sufficient portable readiness signal for the exact Windows positive-linger decision. Do not claim that `epoll`, `TCP_INFO`, or kernel send-buffer counters reproduce the Windows predicate without a differential test. Avoid closing an fd and attempting to 'undo' a failed guest close.

## OPEN — acceptance and falsification gates

- Windows-reference matrix: TCP loopback and constrained receiver; blocking/nonblocking; linger disabled, zero and positive; queued unsent data; `closesocket` result, elapsed time, `WSAGetLastError`, handle usability after failure, retry, remote FIN/RST and delivered bytes. Use repeatable network impairment; report kernel/build and provider.
- Asynchronous matrix: pending `WSARecv`/`WSASend` with event, IOCP and completion routine; record whether completion occurs once, error status, and whether callback ordering differs between immediate completion, pending completion and close. Explicitly exclude invalid concurrent close/other-call patterns from normative claims.
- Linux backend matrix: `SO_LINGER` with O_NONBLOCK and queued data; compare `close`, `shutdown`, peer-observed FIN/RST, io_uring request completion after fd close and cancellation races. Record Linux kernel/liburing revisions.
- Security: rapid guest handle reuse; duplicate handles/fds; a canceled old request must never be attributed to a new guest socket or leak an underlying fd.
- Failure criteria: any Windows `WSAEWOULDBLOCK` case that destroys the guest handle; missing or duplicate completion notifications; Linux blocking close on a guest path expected to return immediately; cancellation that affects a reused socket.

## Boundary
This research establishes documented semantic differences and proposed test gates, **not** equivalence, performance benefit, verified implementation, or complete Winsock compatibility.
