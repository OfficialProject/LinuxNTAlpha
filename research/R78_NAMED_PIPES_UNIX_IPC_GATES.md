# R78 — Windows named-pipe instances versus Linux local IPC

Status: **VERIFIED FROM SOURCE** for the cited Microsoft API and Linux man-pages contracts (inspected 2026-11-14); **HYPOTHESIS** for the proposed backend; **OPEN** for Windows-reference and Linux implementation tests. **No experiment executed.**

## Distinct semantic gate

R43/R66 cover asynchronous completion generally; R14 covers token access; R58 covers directory notifications. None establishes the Windows named-pipe server-instance contract. A pipe name is not a single Linux FIFO or one socket connection: Microsoft specifies multiple *instances* with one name, each with independent buffers and handles, connecting separate clients. [Microsoft: Named Pipes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipes).

## Verified contracts

1. **Name and instance creation.** `CreateNamedPipe` creates one server instance. Its Win32 path is `\\\\.\\pipe\\pipename`; Microsoft documents case-insensitive pipe names, a 256-character limit, compatible access/type modes across instances, and `FILE_FLAG_FIRST_PIPE_INSTANCE` rejecting a subsequent instance with `ERROR_ACCESS_DENIED`. `nMaxInstances` is a separate per-name capacity parameter. [CreateNamedPipeA](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea). Do not translate the guest name directly into a host filesystem pathname.
2. **Byte versus message semantics.** `PIPE_TYPE_BYTE` is a stream; `PIPE_TYPE_MESSAGE` preserves writes as message units. Message-read mode can report `ERROR_MORE_DATA` on a partial read; message-type pipes may also be read in byte mode. Linux `AF_UNIX` offers `SOCK_STREAM` and `SOCK_SEQPACKET` with distinct semantics; selecting one does not implement Windows read-mode switching or partial-message error reporting. [CreateNamedPipeA](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea); [Linux unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html).
3. **Connection race is observable.** `ConnectNamedPipe` can return false with `ERROR_PIPE_CONNECTED` if a client connected between instance creation and the connect call; Microsoft explicitly says this is still a valid connection. With overlapped I/O, `ERROR_IO_PENDING` denotes a pending operation, not a connection failure. An instance must be disconnected before reuse. [ConnectNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-connectnamedpipe).
4. **Blocking and overlapped are not Linux nonblocking mode.** Microsoft documents `PIPE_NOWAIT` as legacy compatibility behavior and explicitly warns not to use it as an asynchronous I/O substitute. Overlapped read/write/connect have distinct completion behavior. [CreateNamedPipeA](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea); [ConnectNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-connectnamedpipe).
5. **Security and reachability.** Named-pipe clients are subject to security checks. Microsoft documents potential remote accessibility when the server service is running, with advice to deny network access for local-only pipes. Linux pathname sockets use host directory/socket permissions; abstract Unix socket permissions do *not* provide access control. Host socket permission checks cannot stand in for guest NT DACL, impersonation, or remote-client policy. [Named Pipes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipes); [Linux unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html).

## Architectural hypothesis (not implemented)

A guest namespace authority maintains a case-insensitive pipe-name table and per-instance state machine (`created -> listening/connected -> disconnected -> reusable/closed`), access masks, security descriptors, outstanding I/O and completion records. A private `AF_UNIX` transport may carry local bytes/messages but must not be exposed as the NT object or used to determine guest permissions. Explicit framing and buffering may be needed to preserve Windows message reads and `ERROR_MORE_DATA`. Remote named-pipe support is a separate protocol/security milestone, **not** implied by local socket support.

**Security constraint:** never bind untrusted guest-controlled names into arbitrary host paths; isolate per-guest namespaces, validate all length and instance limits, and check guest authorization before connecting or impersonating. Avoid abstract sockets as an authorization mechanism.

## Falsifiable conformance matrix (proposed; not run)

- **Windows reference:** two instances under the same name, incompatible type/access modes, maximum-instance exhaustion, first-instance flag, case-variant opens, and multiple concurrent clients. Record Windows build, each return value, `GetLastError`, and handle cleanup.
- **Connection races:** client before/after `ConnectNamedPipe`; synchronous versus overlapped connect; disconnect/reconnect; client closes during a pending connect/read. Check `ERROR_PIPE_CONNECTED`, `ERROR_IO_PENDING`, event/IOCP signaling and cancellation independently.
- **Message fidelity:** one large message read in short chunks, message-read versus byte-read mode, interleaved writes, and peer closure. Verify exact bytes, boundaries, partial-read status and completion counts.
- **Security:** two distinct Windows security tokens with DACL allow/deny, remote-client restrictions, and client impersonation/reversion. On Linux use two confined principals to probe host socket permissions; never treat Linux success as Windows conformance.
- **Backend:** compare `SOCK_STREAM` and `SOCK_SEQPACKET` against the reference matrix, including short buffers, concurrent accepts and resource exhaustion. No benchmark until correctness gates pass.

## Decision gate

Do not equate one Unix socket with one Windows named pipe or promise IPC latency improvements. First establish a Windows-reference state-transition/error matrix, then test a guest-object authority plus local transport; decide whether kernel changes are warranted only after correctness and measured overhead evidence.

## Source scope

Microsoft Learn API documentation is an evolving API contract, not a pinned implementation revision. The Linux `unix(7)` manual describes Linux behavior, not a particular kernel source commit. No kernel implementation or Windows binary was inspected or executed in this note.
