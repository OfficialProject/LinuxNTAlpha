# R206 — Winsock TCP urgent-data versus Linux OOB delivery gates

Status: DOCUMENTATION REVIEW ONLY (2026-11-20). No Windows-reference test, Linux socket test, or implementation was executed. This note is distinct from R204 event latching and R205 linger/close: it concerns TCP urgent-byte delivery and stream boundaries.

## Verified primary-source contracts

1. Microsoft's Winsock OOB model separates ordinary stream reads from urgent-data reads when `SO_OOBINLINE` is disabled. `recv(MSG_OOB)` can retrieve pending urgent data; `select` uses `exceptfds`, and registered asynchronous notifications use `FD_OOB`, not `FD_READ`, for OOB alone. A normal receive must not span the OOB mark. [MS OOB]
2. With `SO_OOBINLINE` enabled, Microsoft's documented model returns urgent bytes in sequence but preserves separate read boundaries around the OOB block; `recv(MSG_OOB)` is documented to fail with `WSAEINVAL`. OOB arrival is then treated as readable normal data rather than `FD_OOB`. [MS OOB] [MS recv]
3. Microsoft documents a TCP-specific, BSD-style urgent-byte interpretation and warns that a newly arriving urgent pointer can replace an unread earlier urgent byte in the non-inline mode. This is **not** a general promise of reliable multi-byte independent OOB messages. [MS OOB]
4. Linux `tcp(7)` documents only a **single byte** of TCP urgent data; when newer urgent data arrives, older urgent data may be inserted into the ordinary stream even if `SO_OOBINLINE` is disabled. Linux also documents the BSD-compatible urgent-pointer interpretation by default, with a system-wide `tcp_stdurg` alternative. [Linux tcp]
5. Linux `tcp(7)` documents `SIOCATMARK`, `recv(MSG_OOB)`, and `select` exceptional readiness, including that a normal read does not cross the urgent mark. These are useful host primitives but are not proof of all Winsock semantics. [Linux tcp]

## Architectural implications (HYPOTHESIS, not a decision)

- A guest Winsock socket object should own the guest-visible inline option, pending OOB event/error state, and the read/urgent mark transitions; merely forwarding host `EPOLLPRI` or `recv` results cannot establish the complete Windows contract. Integrate with the R204 event-notification state machine rather than creating a second independent FD_OOB source.
- **Potential irrecoverable mismatch:** when Linux has already folded an older urgent byte into the normal stream, a wrapper receiving only postprocessed host socket data may not reconstruct the exact Windows stream. Do not claim full fidelity or assume a user-space buffer can reverse this transformation without controlled evidence. A transport-specific mitigation or explicitly documented compatibility limit may be necessary.
- Preserve `SO_OOBINLINE` and OOB mode per guest socket, not as a process-global switch. Treat any host `tcp_stdurg` adjustment as prohibited by default because it changes behavior outside the guest.
- Keep protocol scope narrow: these observations apply to TCP urgent data, not arbitrary Winsock transport providers.

## Proposed differential conformance matrix (OPEN; no results)

Use two controlled endpoints, one native Windows reference and one Linux-hosted candidate, with explicit OS build/kernel version, network topology, packet capture, and socket option values:

1. Send ordinary bytes A, then one `MSG_OOB` byte U, then ordinary bytes B. Compare the number, lengths and exact contents of successive `recv` calls, `MSG_OOB` reads, `SIOCATMARK` before/after each read, and `select` `readfds/exceptfds`.
2. Repeat with `SO_OOBINLINE` enabled before reception. Verify `recv(MSG_OOB)` error mapping and that the three logical regions A/U/B do not get silently coalesced in guest-facing reads.
3. Send two urgent bytes with a controlled delay before the receiver consumes the first. Record whether an older urgent byte is lost, preserved, or reclassified as ordinary data; do not assume either platform matches documentation in every version.
4. Exercise `WSAAsyncSelect`/`WSAEventSelect` FD_OOB/FD_READ transitions, reenabling and error records, including `SO_OOBINLINE` changes, against R204.
5. Test nonblocking reads, `MSG_PEEK`, half-close and concurrent readers. Include mark-boundary races, callback ordering, and negative cases on unsupported socket types.
6. Run with Linux default urgent-pointer interpretation; record (do not silently change) `tcp_stdurg`. Distinguish loopback from remote TCP peers and capture the actual wire sequence.
7. Verify guest-handle lifetime, rights, and cross-process duplicate semantics independently; no host-fd leak or unmediated host ioctl may be introduced by an OOB compatibility path.

## Open questions

- Does the target Windows build preserve the documented three-read inline segmentation under nonblocking and concurrent reads?
- Can the selected Linux backend expose enough urgent-mark provenance to emulate replacement/loss behavior without retaining its own pre-receive stream state?
- Which exact `SIOCATMARK` results apply on the Windows target with inline enabled? Verify rather than extrapolating from Linux.
- What is the compatibility importance of TCP OOB in the initial application corpus? Do not optimize this path before measuring use.

## Sources (primary; retrieved 2026-11-20)

- [MS OOB] Microsoft Learn, *Protocol-Independent Out-of-Band Data*, https://learn.microsoft.com/en-us/windows/win32/winsock/protocol-independent-out-of-band-data-2
- [MS recv] Microsoft Learn, *recv function (winsock2.h)*, https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-recv
- [Linux tcp] Linux man-pages project, *tcp(7)*, https://man7.org/linux/man-pages/man7/tcp.7.html

Evidence boundary: documentation establishes published contracts, not observed parity on any tested Windows/Linux build. No benchmark, compatibility result, or experiment is claimed.
