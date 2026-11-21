# R214 — Winsock DisconnectEx reuse identity versus Linux socket replacement

Status: **primary-source documentation review (2026-11-21)**. No Windows reference execution, Linux experiment, implementation, or performance measurement. Architecture **OPEN**.

## Boundary and nonduplication

R196 covers initial AcceptEx/ConnectEx publication; R195 covers generic asynchronous request ownership; R205 covers closesocket/linger; R211 covers cross-process socket sharing. This note isolates a different contract: **the same guest SOCKET value remains reusable after an asynchronous DisconnectEx**, despite the Linux backend ordinarily needing a *different* transport socket object for a new connection.

## VERIFIED — published API contracts

1. Microsoft's DisconnectEx is a runtime-discovered extension via WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER, WSAID_DISCONNECTEX), applies to connected connection-oriented sockets rather than datagrams, and requires dwReserved=0. With TF_REUSE_SOCKET, **after request completion** the socket may be passed to AcceptEx or ConnectEx. A pending overlapped call can return FALSE/ERROR_IO_PENDING; initiation is not terminal success, and transport TIME_WAIT may delay disconnect/reuse. [M1]
2. Microsoft's shutdown documentation explicitly says shutdown cannot re-enable a direction of the existing connection, and directs applications needing reuse to DisconnectEx with TF_REUSE_SOCKET. Thus implementing reuse as Linux shutdown followed by connect is not justified by these contracts. [M2]
3. Microsoft's ConnectEx documentation states that after DisconnectEx with TF_REUSE_SOCKET the socket is disconnected **but remains bound**, and can be used for ConnectEx. It also documents an important *different* path: after TransmitFile(TF_DISCONNECT|TF_REUSE_SOCKET), a socket previously used for ConnectEx cannot be reused for AcceptEx, and vice versa. These distinctions must not be flattened into a universal reset. [M3]
4. Linux accept/accept4 creates a **new connected socket and descriptor** and leaves the listening socket unaffected. It does not populate a previously allocated Linux descriptor as Windows AcceptEx does with its caller-supplied accept socket. [L1]
5. Linux dup/dup2/dup3 creates aliases to the **same open file description**, not a reset transport object. dup2 can atomically replace a *host descriptor slot*, but it does not itself provide Windows guest socket state or guarantee correct behavior for outstanding host operations referencing the old object. [L2] The latter is an architectural inference, not a measured claim.

## HYPOTHESIS — candidate ownership model, not selected

Represent each guest socket by a stable guest handle identity plus a generation-scoped backend transport identity. DisconnectEx(TF_REUSE_SOCKET) transitions CONNECTED -> DISCONNECT_PENDING -> REUSABLE only after terminal completion. Reuse creates or associates a fresh backend transport object without changing the guest SOCKET value. An accept-side reuse path may need a distinct UNBOUND_REUSABLE state; do **not** assume the bound-state description of ConnectEx also defines all AcceptEx reuse cases. Preserve the distinction between DisconnectEx reuse and TransmitFile reuse.

Old host operations retain their own backend generation until canceled/drained. A completion from the old generation must not affect the new connection, even if Linux recycles an fd number. A guest-facing socket lookup must not expose the replacement backend before the Windows-visible reuse transition. This is a proposal to test, **not** evidence that a Linux-only backend can perfectly reproduce Windows behavior.

## OPEN — conformance gates

- **Windows reference:** pin OS build, Winsock provider, protocol and socket options; run DisconnectEx with zero flags and TF_REUSE_SOCKET, synchronous versus OVERLAPPED; record return, WSAGetLastError, event/IOCP notification, peer FIN/RST, elapsed time and state before/after terminal completion.
- **Reuse matrix:** original socket created for ConnectEx versus AcceptEx; attempt the matching and opposite extension after DisconnectEx and separately after TransmitFile with TF_DISCONNECT|TF_REUSE_SOCKET. Record bind state, local port, option persistence, SO_UPDATE_*_CONTEXT requirements, exact error codes and TIME_WAIT sensitivity. Do not assume the documented TransmitFile restriction automatically applies to DisconnectEx.
- **Race matrix:** queue receive/send, initiate disconnect, cancel or close while pending, reuse after completion, and rapidly recycle host fds; prove each old request terminates once without delivery to a new generation. Avoid claiming Windows-defined ordering for undocumented races.
- **Linux backend:** compare socket replacement behind a stable guest handle against dup2 descriptor-slot replacement; examine epoll/io_uring references and cleanup; verify no cross-generation notifications, leaked transport sockets, or mistaken binding inheritance.
- **Security/performance:** reject stale guest handle generations and unauthorized reuse across processes; only benchmark once semantic tests pass.

## Sources (accessed 2026-11-21)

- [M1] Microsoft, LPFN_DISCONNECTEX: https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_disconnectex
- [M2] Microsoft, shutdown: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-shutdown
- [M3] Microsoft, LPFN_CONNECTEX: https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_connectex
- [L1] Linux man-pages project, accept(2): https://man7.org/linux/man-pages/man2/accept.2.html
- [L2] Linux man-pages project, dup(2): https://man7.org/linux/man-pages/man2/dup.2.html

No upstream source revisions are pinned for these live documentation pages. A future Windows-reference fixture and pinned Linux kernel test are required before treating the proposal as validated.
