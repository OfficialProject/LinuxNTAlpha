# R37 — Windows named pipes versus Linux AF_UNIX transport (2026-11-12)

**Status:** documentation research only; no experiment, implementation, or Windows-reference observation in this round.

## Scope and evidence
Windows named pipes are not simply filesystem FIFOs or Unix-domain sockets. This note isolates a candidate transport from the Windows-visible pipe object, access, and I/O contract. **VERIFIED** below means supported by cited primary platform documentation, not measured in LinuxNTAlpha.

## Verified contracts

1. **Windows pipe type and read mode are separate.** A byte-type pipe has a byte stream; a message-type pipe preserves each write as a message. A message-type handle may be read in byte or message mode. In message-read mode, an undersized read consumes a portion and returns an error with `ERROR_MORE_DATA`, leaving the remainder available. Client handles initially use byte-read mode. [W1]
2. **Windows pipe wait mode is not the same as overlapped I/O.** Blocking and nonblocking wait modes affect read, write, and connection behavior; Microsoft explicitly discourages using `PIPE_NOWAIT` as a replacement for overlapped I/O. [W1]
3. **Windows pipe names can have multiple server instances.** Servers can serve multiple clients through multiple instances, with per-instance connection lifecycles and overlapped operations. [W2]
4. **Windows named pipes are securable objects.** A security descriptor and access checks govern server-instance creation and client opening. Microsoft documents the surprising overlap of `FILE_APPEND_DATA` and `FILE_CREATE_PIPE_INSTANCE` rights; using broad `FILE_GENERIC_WRITE` can grant instance-creation permission. [W3]
5. **Linux AF_UNIX offers several distinct transports.** `SOCK_STREAM` is a stream, while `SOCK_SEQPACKET` is connection-oriented, ordered, and message-boundary-preserving; `SOCK_DGRAM` also preserves message boundaries. AF_UNIX can transfer file descriptors with `SCM_RIGHTS`, and exposes peer credentials through `SO_PEERCRED`. [L1]
6. **Linux socket permissions do not implement Windows pipe ACLs.** Pathname sockets have Linux filesystem permission checks; abstract sockets have no meaningful socket-file permissions. Linux peer credentials are not themselves Windows access tokens or impersonation. [L1, W3]

## Architectural implications (hypotheses, not implementation decisions)

- **HYPOTHESIS:** a userspace pipe-object manager can retain Windows names, instance allocation, handle modes, per-handle read state, and NT security decisions, while using AF_UNIX for local byte transport. A byte pipe might use `SOCK_STREAM`; message pipes might use `SOCK_SEQPACKET` plus explicit buffering and partial-message bookkeeping. Neither socket choice alone reproduces `ERROR_MORE_DATA` or the client read-mode defaults.
- **HYPOTHESIS:** an event-driven broker could adapt socket readiness to Windows overlapped completion, cancellation, and eventual IOCP semantics. This requires an explicit operation state machine; readiness is not completion.
- **OPEN:** remote named-pipe transport, SMB integration, impersonation, client identity at connection and after changes, NT object namespace aliasing, and exact NTSTATUS/Win32 error translations. Do not infer remote support from AF_UNIX.
- **OPEN:** how `NtCreateNamedPipeFile`, `NtFsControlFile`, and `NtReadFile` expose semantics beyond these Win32 documents; source-level validation against legally usable references and Windows conformance traces is required before implementation.

## Proposed conformance gates (not executed)

| Gate | Windows reference observation required | Linux candidate under test |
|---|---|---|
| Byte/message separation | Two writes followed by short and long reads | STREAM vs SEQPACKET plus adapter |
| Partial message | Short message-mode read, error, then remaining bytes | Explicit remainder retention |
| Per-handle modes | Client initial mode and `SetNamedPipeHandleState` | Object/handle metadata |
| Instance contention | Two servers sharing name, clients, max-instance exhaustion | Broker instance registry |
| Connect races | Client connects before/while `ConnectNamedPipe` | Connection state machine |
| Overlapped completion | Pending read, completion, cancellation, close races | Event-loop/IOCP adapter |
| Access control | Denied server-instance creation vs allowed client open | Token/ACL enforcement before socket admission |
| Impersonation | Server queries/uses client security context | Credential-to-token policy; no automatic equivalence |

For every gate, record Windows build, access masks, exact API calls, return values, last-error/NTSTATUS, completion ordering, and repeated-run variability. Use dedicated isolated test users for ACL tests. Do not treat Linux-only successes as proof of Windows compatibility.

## Primary sources (accessed 2026-11-12)
- [W1] Microsoft, *Named Pipe Type, Read, and Wait Modes*: https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes
- [W2] Microsoft, *Named Pipe Instances*: https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-instances
- [W3] Microsoft, *Named Pipe Security and Access Rights*: https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights
- [L1] Linux man-pages project, *unix(7)*: https://man7.org/linux/man-pages/man7/unix.7.html

**Boundary:** no claim of working pipe compatibility, no benchmark, and no selection of AF_UNIX as the final backend.
