# R192 — Named-pipe peek and transaction semantics versus packet receive

**Research date:** 2026-11-19. **Status:** VERIFIED for cited published Microsoft/Linux interface contracts; HYPOTHESIS for adapter design; OPEN for differential observations. No experiment or implementation was performed.

## Nonduplicative scope

R37 and R78 already cover named-pipe namespaces, instances, access checks, transport choices, and basic message/byte reads. This note isolates **non-consuming peeks**, **read-mode disagreement**, and **request/response transaction continuation**. These are separate compatibility gates: preserving packet boundaries alone does not reproduce them.

## Verified platform contracts

1. Microsoft documents that `PeekNamedPipe` copies data **without consuming** it and exposes both total bytes available and bytes remaining in the current message. In byte-type pipes or anonymous pipes, the latter count is zero. For a message-type pipe, **PeekNamedPipe uses the read mode specified when the pipe was created even if SetNamedPipeHandleState subsequently changes the handle's ReadFile read mode**. In message mode, a short peek returns success with the remainder count, whereas a short message-mode `ReadFile` may return `ERROR_MORE_DATA`. [W1][W2]
2. Microsoft documents that `PeekNamedPipe` may block on a synchronous pipe handle in a multithreaded application despite its immediate-return behavior in a single-threaded application. Thus an adapter must not promise universally nonblocking guest peeks. [W1]
3. `TransactNamedPipe` requires a duplex **message-type** pipe and a **message-read-mode handle**; it combines writing a request with reading a response. If the output buffer is too small, it returns `ERROR_MORE_DATA` and a subsequent `ReadFile`, `ReadFileEx`, or `PeekNamedPipe` can access the remainder. Microsoft documents a maximum **guaranteed** transaction size of 64 KiB, not a universal maximum message size. [W3][W4]
4. `CallNamedPipe` combines open/wait/transaction/close; if its response buffer is insufficient, the extra response bytes are lost when its temporary handle closes. This differs from continuing a transaction on a persistent handle. [W4]
5. Linux AF_UNIX `SOCK_SEQPACKET` is connection-oriented, ordered and message-boundary preserving. This property does **not** by itself specify Windows peek counts, independent guest read modes, or continuation of partially consumed messages. [L1]

## Architectural hypothesis — explicit guest message ledger

Maintain per-instance queued messages with per-reader consumption offset and separate **creation-time pipe type**, **per-handle current read mode**, and **peek presentation mode**. `PeekNamedPipe` should snapshot visible bytes without advancing offsets; `ReadFile` should advance the offset and retain a partial message as appropriate; `TransactNamedPipe` should link request-write and response-read operation lifetimes while retaining response continuation after `ERROR_MORE_DATA`. The backend may use `SOCK_SEQPACKET`, but an adapter must avoid silently discarding excess packet bytes on undersized host receives. This is a design proposal, not a proven implementation.

**Security and resource hypothesis:** cap queued message bytes and ledger metadata per pipe instance and guest principal; enforce guest ACL/handle access separately from host socket permissions. Define lock ownership for concurrent peeks, reads, cancellation, and handle closure. No claim is made about Windows' exact atomicity under concurrent peeks/reads; obtain Windows-reference observations first.

## Falsifiable gates — proposed, not run

| Gate | Windows reference observation | Linux candidate check |
|---|---|---|
| Peek without consumption | Peek twice, then read; compare bytes/counts | Ledger offsets unchanged by peek |
| Mode disagreement | Create message pipe; switch handle to byte-read; compare PeekNamedPipe and ReadFile | Separate peek-mode and read-mode dispatch |
| Short message | Peek short buffer then ReadFile short buffer; compare return, error, remainder | No host truncation/discard |
| Transaction continuation | Short TransactNamedPipe output then ReadFile remainder | Pending response survives initial short read |
| CallNamedPipe closure | Short response and subsequent access attempt | Temporary guest handle fully closed |
| Empty/closed pipe | Peek before data, after disconnect, and during close | Precise status and wakeup semantics |
| Concurrency | Peek/read racing with writer and close across threads | Locking, lifetime, and cancellation discipline |

Capture Windows build, exact flags, buffer sizes, returned byte counts, `GetLastError`, synchronous/overlapped mode, and repetition. Compare against Linux implementation only after collecting reference data. Do not infer transaction atomicity or exact Windows scheduling from API names.

## Primary sources (checked 2026-11-19)

- [W1] Microsoft, PeekNamedPipe: https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe
- [W2] Microsoft, Named Pipe Client: https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-client
- [W3] Microsoft, TransactNamedPipe: https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-transactnamedpipe
- [W4] Microsoft, Transactions on Named Pipes: https://learn.microsoft.com/en-us/windows/win32/ipc/transactions-on-named-pipes
- [L1] Linux man-pages project, unix(7): https://man7.org/linux/man-pages/man7/unix.7.html

**Boundary:** These are documented interface claims, not a claim of implemented LinuxNTAlpha compatibility, a Windows trace, or a benchmark.
