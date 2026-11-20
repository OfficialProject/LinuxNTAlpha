# R204 — Winsock network-event latching versus Linux epoll readiness

Date: 2026-11-20. Status: **DOCUMENTATION REVIEW ONLY**. No Windows differential tests, Linux probes, or benchmarks were run.

## Question

Can a native Linux-backed Winsock implementation directly translate `WSAEventSelect` and `WSAAsyncSelect` into epoll notifications without retaining guest socket-event state?

## VERIFIED — primary contracts

1. Microsoft documents that `WSAEventSelect` associates **one event object** with a socket and a bitmask of network events. A subsequent selection replaces earlier `WSAEventSelect` or `WSAAsyncSelect` registration and clears its internal event record. Selection forces nonblocking mode, including a zero mask until blocking mode is explicitly restored. [Microsoft WSAEventSelect](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect).
2. Network-event occurrences and their error codes are recorded; `WSAEnumNetworkEvents` retrieves and clears the record and can atomically reset the designated event object with respect to that record. The event object's signaled state is **not** itself a substitute for the recorded FD_* bits and errors. [Microsoft WSAEventSelect](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect); [Microsoft asynchronous event notification](https://learn.microsoft.com/en-us/windows/win32/winsock/asynchronous-notification-using-event-objects-2).
3. Microsoft documents operation-specific reenabling: `FD_READ` on receive calls, `FD_WRITE` on send calls, `FD_ACCEPT` on accept calls, even when the reenabling call fails. `FD_CONNECT` and `FD_CLOSE` have no reenabling function. `FD_READ`, `FD_OOB` and `FD_ACCEPT` are level-triggered *upon reenabling*, allowing partial reads to produce a subsequent notification. [Microsoft WSAEventSelect](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect).
4. `WSAAsyncSelect` delivers window messages instead of signaling a Winsock event object; it also forces nonblocking mode and uses operation-specific message reenabling. It therefore additionally depends on guest window/message-queue lifecycle. [Microsoft WSAAsyncSelect](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaasyncselect).
5. Linux `epoll` exposes readiness via level-triggered or edge-triggered monitoring; `EPOLLONESHOT` requires explicit `epoll_ctl(... EPOLL_CTL_MOD ...)` rearming. Edge-triggered consumers may miss buffered data if they fail to drain until `EAGAIN`. These mechanisms do **not** themselves implement Winsock's per-event latch, event-error array or guest message delivery. [Linux epoll(7)](https://man7.org/linux/man-pages/man7/epoll.7.html).

## HYPOTHESIS — candidate compatibility state machine

Represent each guest socket by a generation-tagged object with (a) selected event mask, (b) delivery target and mode, (c) pending FD_* bits and corresponding Winsock error slots, (d) event-specific reenable state, and (e) ownership of the host socket and epoll registration. Treat epoll as a **readiness input**, not as the guest event queue. Under a per-socket synchronization discipline, translate readiness and terminal errors into latched guest events, publish the associated event signal/message, and clear recorded bits with the documented enumeration/reset semantics. Reevaluate still-valid readiness when a qualifying guest socket operation reenables an event. Close/reselection must invalidate stale backend notifications by generation.

This is an unimplemented design hypothesis, **not** a claim of exact Windows behavior. In particular, simultaneous error and readiness ordering, FD_CLOSE behavior with unread data, interaction with overlapped I/O/IOCP, socket duplication and accepted-socket inheritance require Windows-reference tests.

## Proposed falsifiable gates — NOT EXECUTED

1. On Windows, select `FD_READ|FD_WRITE|FD_CLOSE`, verify initial writable event, enumerate twice, attempt failed and successful send/receive, and record exact bit/error/rearm behavior.
2. Receive 100 bytes, read only 50, enumerate and verify when the second FD_READ is recorded. Repeat with no recv and with a recv that returns WSAEWOULDBLOCK.
3. Replace event-object selection and alternate between `WSAAsyncSelect` and `WSAEventSelect`; verify pending-record clearing, window messages and socket blocking-mode transitions.
4. Trigger connection refusal, half-close and reset; capture `FD_CONNECT`/`FD_CLOSE` ordering and error arrays. Test closing or reusing socket handles while backend events are queued.
5. Run a separate Linux epoll prototype with LT, ET and ONESHOT; record readiness traces and assert the guest state machine's outcomes against the Windows reference, including race/TSAN and stale-generation checks.

## Architecture implication

**OPEN:** Do not select an epoll mode as the definitive Winsock implementation strategy until differential tests establish the required guest state transitions. A stateful translation layer appears necessary from the published contracts; this does not imply a kernel modification. The module intersects R191 (IOCP admission), R195 (asynchronous request ownership) and R196 (AcceptEx/ConnectEx) but covers a distinct Winsock event-notification contract.

Sources accessed 2026-11-20; Microsoft pages are living documentation (no Windows build pinned). The Linux epoll manual is not a substitute for a pinned kernel-source audit. No code imported.
