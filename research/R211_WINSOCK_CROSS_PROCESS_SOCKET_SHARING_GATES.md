# R211 — Winsock cross-process socket sharing versus Linux descriptor transfer

Status: DOCUMENTATION-ONLY research (2026-11-21 UTC). No Windows or Linux experiment performed. This note extends, rather than replaces, R67 (general NT handle duplication), R195 (async request ownership), R204 (event latching), and R205 (close lifetime).

## Question

Can an NT-compatible Winsock implementation on Linux reproduce the *target-process-bound, one-use* WSADuplicateSocket/WSASocket handoff without leaking authority or conflating shared socket state with per-process descriptor state?

## VERIFIED — primary-source contracts

1. Microsoft's WSADuplicateSocket documentation says the source passes a **target process ID** and receives a special WSAPROTOCOL_INFO structure; an independent IPC channel transfers that structure; the target calls WSASocket to obtain a new descriptor. The special structure is usable **once by the target process**. The API does not support QOS-enabled sockets. Source: [Microsoft WSADuplicateSocketA](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaduplicatesocketa) (reviewed 2026-11-21).
2. Microsoft says duplicated descriptors refer to the **same underlying socket**, including common setsockopt-visible state; the underlying socket persists until the last descriptor is closed. Independent I/O calls still require application coordination. WSAAsyncSelect/WSAEventSelect registration on one descriptor replaces previous registration for the shared socket, including across processes. Source: [Microsoft WSADuplicateSocketA](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaduplicatesocketa).
3. The Winsock service-provider interface places responsibility for constructing and recognizing the transferable WSAPROTOCOL_INFO in the provider; its provider-reserved field can hold implementation context. This is a provider contract, **not** permission to expose raw Linux file descriptors in guest data. Source: [Microsoft Shared Sockets in the SPI](https://learn.microsoft.com/en-us/windows/win32/winsock/shared-sockets-in-the-spi-2).
4. Microsoft warns that ordinary DuplicateHandle is not the supported substitute for socket duplication: it can interfere with Winsock's internal reference accounting. Source: [Microsoft DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle).
5. Linux SCM_RIGHTS transfers references to an **open file description** via AF_UNIX ancillary messages; recipient descriptor numbers may differ. Excess descriptors are closed if ancillary storage is insufficient or RLIMIT_NOFILE is exceeded. This transfers a host resource but does **not** itself enforce Winsock's target-PID-bound, single-use guest token. Source: [Linux unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html).

## HYPOTHESIS — candidate boundary, not a design decision

Keep a guest socket identity and guest handle table separate from Linux fds. A trusted runtime broker may issue an opaque, unpredictable, single-consumption capability bound to a *guest process identity plus generation*, the socket identity, and provider flags. Only after authenticating the intended receiver and atomically consuming the capability would the broker transfer an underlying fd (e.g. SCM_RIGHTS) or install a broker-mediated reference. The token's guest-visible encoding must not reveal host fd numbers, broker addresses, or reusable authority. Source APIs do not prescribe this design.

Shared transport state and notification registration need a **socket-global** owner, while per-process descriptor number, handle lifetime, overlapped request and IOCP association require explicit ownership rules. The precise Windows behavior of IOCP association across duplicated sockets remains **OPEN**; do not extrapolate it from general descriptor sharing.

## OPEN — reference and security gates before implementation

- **Target binding/replay:** generate token for process A; try first use in B, second use in A, use after A exits/PID reuse, and concurrent double redemption. Record exact Windows error codes, provider, OS build, and whether the token is consumed on each failure.
- **Lifetime:** close source before redemption, after redemption, and concurrently; distinguish token lifetime from live socket references and verify no unintended fd or broker reference survives.
- **State coupling:** vary SO_* options, nonblocking mode, WSAEventSelect/WSAAsyncSelect registration and event enumeration from both processes; distinguish shared state from descriptor-specific settings and capture notification replacement.
- **Async interactions:** pending WSARecv/WSASend, cancellation, close, handle reuse, and IOCP association in each process; log exact completion count, status, completion key, and ordering. Coordinate with R195/R204/R205.
- **Provider/flag compatibility:** compare A/W protocol-info variants, WSASocket flags, inherited/duplicated sockets, QOS-enabled socket rejection, and unsupported provider families.
- **Host isolation:** adversarial guest process must not forge or replay broker capabilities, send a foreign SCM_RIGHTS fd, exploit host PID recycling, or obtain a socket without intended guest authorization. Test truncation/EMFILE and broker crash cleanup.
- **Performance:** only after correctness, measure token creation/redemption and fd handoff overhead against a Windows reference on comparable hardware; report repetitions and distributions, not a universal speed claim.

## Decision impact

**VERIFIED:** Linux fd passing can transport a shared kernel socket reference, but the published Linux primitive alone cannot establish Winsock's one-use, target-process-specific transfer semantics. **HYPOTHESIS:** a guest capability broker and explicit shared-socket state may bridge that semantic gap. **OPEN:** precise error/notification/IOCP behavior and broker overhead need reference tests. No architecture selection or performance result follows from this note.
