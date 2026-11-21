# R209 — Winsock datagram ancillary-data translation versus Linux recvmsg/sendmsg

Status (2026-11-20): **VERIFIED FROM SOURCE** for the linked published API contracts; **HYPOTHESIS** for adapter design; **OPEN** for build-specific behavior. Documentation review only. **No experiment executed.**

## Distinct scope

R195 covers asynchronous request lifetime; R196 covers AcceptEx/ConnectEx; R204 covers event notifications. This note isolates datagram payload/control-buffer translation, packet metadata and truncation across the Windows `WSAMSG` and Linux `msghdr` interfaces. It is not a claim that arbitrary Windows network programs already run.

## Primary-source findings

1. Microsoft [LPFN_WSARECVMSG](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_wsarecvmsg) specifies a Winsock extension obtained with `WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER, WSAID_WSARECVMSG)`, restricted to `SOCK_DGRAM` and `SOCK_RAW`. Its input `WSAMSG.dwFlags` permits `MSG_PEEK`; output flags include `MSG_CTRUNC`, `MSG_TRUNC`, `MSG_BCAST` and `MSG_MCAST`. The API supports overlapped completion and warns against reentrant blocking Winsock calls inside interrupting APCs.
2. Microsoft [WSAMSG](https://learn.microsoft.com/en-us/windows/win32/api/ws2def/ns-ws2def-wsamsg) defines address, scatter/gather buffers, a `WSABUF Control` and flags. These are **guest ABI structures**, not interchangeable with host `struct msghdr` by pointer cast.
3. Microsoft [WSASendMsg](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasendmsg) provides outgoing message and optional control data. Microsoft [Winsock ECN](https://learn.microsoft.com/en-us/windows/win32/winsock/winsock-ecn) documents Windows `IP_ECN`/`IPV6_ECN` ancillary controls; [WSASetRecvIPEcn](https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-wsasetrecvipecn) documents receive enablement and a minimum supported build. This metadata is feature/version gated, not universally available.
4. Linux [recv(2)](https://man7.org/linux/man-pages/man2/recv.2.html), [send(2)](https://man7.org/linux/man-pages/man2/send.2.html), [cmsg(3)](https://man7.org/linux/man-pages/man3/cmsg.3.html) describe host scatter/gather and ancillary message layout; `MSG_TRUNC` and `MSG_CTRUNC` report different loss conditions. Linux `cmsghdr` alignment, lengths and protocol-specific option behavior must be interpreted under the host ABI. The documentation does **not** establish identical numeric flag values, metadata payloads or truncation error codes between Windows and Linux.

These citations establish public contracts, not observations of a pinned Windows build or a pinned Linux kernel.

## Candidate adapter (HYPOTHESIS, not implemented)

Use a typed guest-to-host message serializer: validate guest `WSAMSG` and `WSABUF` pointer/count/length ranges before host calls; construct independent host iovecs/control storage; translate known control types and levels explicitly; reject or conservatively handle unknown controls rather than forwarding guest bytes as host kernel structures. Keep host descriptor passing (`SCM_RIGHTS`) and host credentials out of guest-accessible control channels unless a separately authorized guest API defines them. Serialize returned address, payload lengths, control lengths and flags independently; avoid leaking host padding or uninitialized bytes.

For overlapped operations, retain operation-owned metadata buffers and a socket transport generation until one terminal completion is committed (R195/R208); avoid re-reading mutable guest buffers after asynchronous initiation unless Windows-reference tests establish the applicable contract. This ownership proposal requires experimental validation.

## Reproducible conformance gates (NOT RUN)

- On recorded Windows OS builds/providers, test UDP IPv4/IPv6, raw socket permission failures, extension-function lookup, invalid socket types, connected/unconnected sockets and `MSG_PEEK`.
- Vary data buffer length and control buffer length independently (zero, one byte, just short, exact, oversized); record return, `WSAGetLastError`, bytes, flags, `Control.len`, sender address and actual delivered datagram length. Check `MSG_TRUNC` versus `MSG_CTRUNC` without assuming Linux behavior matches.
- Toggle IPv4 `IP_PKTINFO`, IPv6 `IPV6_PKTINFO` and version-supported ECN receive controls; test multicast/broadcast and multiple ancillary headers. Verify type/level/payload sizes against actual Windows SDK headers and reference results.
- Test outgoing `WSASendMsg` control encoding and unsupported options; compare Linux `sendmsg/recvmsg` behavior on a recorded kernel and libc version.
- Stress overlapped completion, cancellation, socket close/reuse and guest buffer mutation with controlled synchronization; verify no stale completion, buffer overrun or host metadata leak. Record whether calls complete immediately or pending; do not infer one from the other.
- Security review: hostile `dwBufferCount`, length arithmetic overflow, invalid pointers, malformed CMSG chains, guest address-family translation, kernel ancillary-buffer caps and cross-process ownership.

## Decision gate

**OPEN:** precise per-version Windows truncation/error semantics, option availability and whether host metadata fully covers the initial application subset. A userspace translation layer is plausible but unproven. No kernel patch or performance assertion is justified by this review alone.
