# R205 — Cross-process Winsock socket duplication and Linux descriptor passing

Date: 2026-11-20. Status: **SOURCE REVIEW ONLY**. No Windows differential test, Linux executable probe, or performance measurement was performed.

## Question

Can Linux `SCM_RIGHTS` implement `WSADuplicateSocket` / `WSASocket` by passing an fd, or must LinuxNTAlpha preserve additional guest-visible socket identity and one-use transfer semantics?

## VERIFIED FROM PRIMARY DOCUMENTATION

1. Microsoft's `WSADuplicateSocketA` contract takes the **target process ID** and returns a special `WSAPROTOCOL_INFO` record. The source must transfer that record to the destination through separate IPC; the destination supplies it to `WSASocket`. The special record is **usable once by the designated target process**. The API cannot be used for a QoS-enabled socket. [Microsoft WSADuplicateSocketA](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaduplicatesocketa).
2. Microsoft documents **multiple descriptors for one underlying socket**. Shared socket options/state are visible through the descriptors, whereas closing one descriptor leaves the underlying socket open until the final descriptor closes. Independent I/O requires coordination between processes. [Microsoft Shared Sockets](https://learn.microsoft.com/en-us/windows/win32/winsock/shared-sockets-2).
3. Crucially, `WSAAsyncSelect` or `WSAEventSelect` registration through **any** shared descriptor cancels the previous network-event registration on that shared socket, even if it was installed from another process. A separate per-fd event-selection implementation would violate the documented cross-process contract. [Microsoft Shared Sockets](https://learn.microsoft.com/en-us/windows/win32/winsock/shared-sockets-2).
4. Linux AF_UNIX `SCM_RIGHTS` sends a reference to an **open file description**, not merely an integer fd. The recipient receives its own descriptor. Linux documents this as ancillary data sent with `sendmsg` and received with `recvmsg`. [Linux unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html). Linux `open(2)` distinguishes file descriptors from shared open file descriptions and their status flags. [Linux open(2)](https://man7.org/linux/man-pages/man2/open.2.html).
5. Microsoft's Winsock service-provider interface explicitly delegates the contents and recognition of the special protocol record to the provider; `dwProviderReserved` may carry provider-specific information. Therefore an application-facing `WSAPROTOCOL_INFO` record is **not** documented as a portable Linux fd encoding. [Microsoft Shared Sockets in the SPI](https://learn.microsoft.com/en-us/windows/win32/winsock/shared-sockets-in-the-spi-2).

## Derived mismatch (not a measured failure)

`SCM_RIGHTS` is a useful **host transport primitive** for the underlying socket reference, but by itself it does not establish (a) one-use redemption of a guest protocol record, (b) target Windows-process binding, (c) guest socket handle generation and lifetime, or (d) a **shared, cross-process** Winsock notification registration and event-error latch. The first three require guest authority/lifecycle logic; the fourth also intersects the event-latch design in R204. These are deductions from the contracts above, **not** a claim that an implementation has been tested.

## HYPOTHESIS — brokered transfer ticket

Maintain a generation-tagged guest socket identity in a trusted broker. `WSADuplicateSocket` creates a target-bound, one-use, expiring ticket tied to that identity and a held host socket reference; its guest-visible protocol record encodes an opaque ticket rather than a raw host fd. After verifying target process identity and ticket freshness, `WSASocket` atomically redeems it, transfers the underlying fd over authenticated AF_UNIX IPC (or another explicitly audited transport), and allocates a new guest descriptor. Keep event-selection ownership and recorded network events attached to the **shared guest socket identity**, not to the recipient's host fd. Final close releases the shared identity only after pending transfers and I/O ownership are settled.

This is a proposed design, not an accepted architecture. Whether the broker must own all network-event observation, and how it can safely publish cross-process event-object or window-message notifications, remain **OPEN**. Ticket expiration is a proposed security policy and may need compatibility-specific adjustment; it is **not** a documented Windows timeout.

## Falsifiable gates — NOT EXECUTED

1. **Windows reference:** duplicate one TCP socket to a known target process, redeem the record once, retry redemption, attempt redemption in a different process, and capture exact Winsock return/error codes. Repeat after the source closes its descriptor, and with a QoS-enabled socket.
2. **Shared notification:** in two processes, register `FD_READ` through A, then `FD_WRITE` through B; verify the resulting notification target, pending event record, and close behavior. Include source and target process exit/restart.
3. **Linux mechanism:** send an AF_INET socket through `SCM_RIGHTS` over AF_UNIX; verify shared socket options, independent descriptor closes, and final close. Compare with duplicate fds within one process. Record kernel version, namespace/credential context and actual observed output.
4. **Security/races:** attempt replay, cross-target redemption, stale Windows PID reuse, guest handle reuse, concurrent redemption, IPC disconnect mid-transfer and source teardown. Test whether a target-bound capability can be forged or reused.
5. **Integration:** test against R204's Winsock event latch, R122 child handle inheritance, R146 handle-reuse races, and R195 asynchronous request ownership; do not infer IOCP completion semantics from fd duplication.

## Architectural consequence

**OPEN:** Do not equate `WSADuplicateSocket` with `dup` or `SCM_RIGHTS` alone. A user-space broker plus Linux fd passing is plausible, but correctness, security and overhead remain unmeasured. No kernel change is justified by this documentation review alone.

Sources inspected 2026-11-20. Microsoft Learn pages are living documentation, not pinned Windows-build conformance evidence; Linux man-pages describe interfaces, not a pinned kernel implementation. No third-party code imported.
