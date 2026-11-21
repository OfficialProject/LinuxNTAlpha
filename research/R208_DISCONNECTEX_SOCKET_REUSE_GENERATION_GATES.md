# R208 — DisconnectEx socket reuse and Linux transport-generation gates

Status: **VERIFIED FROM SOURCE** for cited interface contracts; **HYPOTHESIS** for proposed LinuxNTAlpha design; **OPEN** for Windows-version differential behavior. Research only, 2026-11-20. **No experiment executed.**

## Scope and relationship to prior work

R196 addresses AcceptEx/ConnectEx publication; R195 covers asynchronous request ownership; R205 covers linger/close. This note isolates a different requirement: *preserving one Windows SOCKET identity across successive transport connections* after DisconnectEx/TransmitFile reuse. It does not establish a new kernel boundary or claim benchmark results.

## Primary-source contracts

1. Microsoft [LPFN_DISCONNECTEX](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_disconnectex) documents connection-oriented sockets only; `dwReserved` must be zero; `TF_REUSE_SOCKET` prepares the handle for subsequent AcceptEx/ConnectEx **after completion**. An overlapped call returning `ERROR_IO_PENDING` is initiated, not necessarily successful. The extension function pointer is obtained through `WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER, WSAID_DISCONNECTEX)`. TCP TIME_WAIT can delay disconnect.
2. Microsoft [LPFN_CONNECTEX](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_connectex) states that DisconnectEx with reuse returns the socket to *unconnected but bound* state. It documents restrictions on reuse paths, the post-connect `SO_UPDATE_CONNECT_CONTEXT` requirement and warns that reconnecting after arbitrary connection failure should generally use a new socket.
3. Microsoft [TransmitFile](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-transmitfile) documents `TF_REUSE_SOCKET` only together with `TF_DISCONNECT`; the connection establishment path constrains subsequent reuse (AcceptEx versus ConnectEx).
4. Linux [accept(2)](https://man7.org/linux/man-pages/man2/accept.2.html) returns a **new connected descriptor**, leaving the listener unchanged. Linux [shutdown(2)](https://man7.org/linux/man-pages/man2/shutdown.2.html) shuts down directions of an existing full-duplex connection; neither interface specifies Windows-style rebinding of a connected socket object to a fresh accepted connection. POSIX [connect(3p)](https://man7.org/linux/man-pages/man3/connect.3p.html) says failed-connect socket state is unspecified and recommends closing/recreating.

These are interface documentation findings, **not** pinned kernel implementation or observed Windows outcomes. Microsoft API pages are living documentation, not a fixed OS build contract.

## Architecture hypothesis (not implemented)

Keep a stable guest SOCKET object and a **replaceable host transport generation**, rather than treating a host fd as the guest identity. Suggested state machine:

`CREATED -> BOUND -> CONNECTING/ACCEPTING -> CONNECTED -> DISCONNECTING -> REUSABLE_BOUND -> CONNECTING/ACCEPTING`.

Each async operation captures guest socket ID, generation, operation ID, completion destination and cancellation state. A successful disconnect with reuse can retire the old transport and install a fresh host socket without changing the guest handle. Do not publish `REUSABLE_BOUND` until the disconnect completion is committed. Preserve guest-visible bound address/options only to the extent established by differential Windows tests. Restrict AcceptEx/ConnectEx reuse path according to the original connection path; exact enforcement needs Windows tests. **Do not** claim host fd replacement is behaviorally equivalent without proving option, bind, port and pending I/O semantics.

Race hazard: a late epoll/io_uring completion from generation N must never be attributed to generation N+1, even when the kernel recycles the same integer fd. Completion records should reference retained operation and transport objects, not fd integers alone. This is a design proposal, not a verified implementation requirement.

## Falsifiable conformance plan (not run)

- Windows reference matrix by recorded build, socket provider, IPv4/IPv6 and synchronous/overlapped modes: disconnect with/without reuse, pending versus immediate completion, invalid reserved/flags, UDP rejection, unconnected socket, repeated disconnect, TIME_WAIT and post-disconnect `getsockname`.
- Reuse path matrix: ConnectEx-created and AcceptEx-created sockets; attempt each subsequent AcceptEx/ConnectEx; record exact WSA error, handle value, local endpoint and context option behavior. Repeat with TransmitFile `TF_DISCONNECT | TF_REUSE_SOCKET`.
- Completion race: issue send/recv and disconnect, cancel/close at controlled barriers, reuse handle, and verify **exactly one** terminal completion per operation and no stale delivery to new generation. Repeat with completion-port association and skip-on-success settings (see R195/R111).
- Linux backend: separately probe socket creation/bind, accept4/new fd, shutdown/close, option reapplication, source port/TIME_WAIT and fd-number reuse. Compare to Windows before choosing emulation behavior.
- Security: ensure socket handles never expose host fd values, stale async completions cannot cross tenant/process ownership, and local bind/rebind policy does not bypass host network isolation.

## Decision impact

**OPEN:** whether a userspace transport-generation adapter is sufficient for the chosen application subset; whether host socket churn measurably affects latency; whether a targeted kernel mechanism is justified. No performance or compatibility claim follows from these contracts. Prioritize correctness and a reproducible Windows/Linux differential harness before optimization.
