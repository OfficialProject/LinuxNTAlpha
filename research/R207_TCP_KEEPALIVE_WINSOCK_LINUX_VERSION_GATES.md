# R207 — TCP keepalive Winsock/Linux option and version gates

Date: 2026-11-20. Status: **SOURCE REVIEW ONLY**. No Windows reference run, Linux socket experiment, packet capture, or performance measurement.

## Question

Can guest Winsock `SO_KEEPALIVE`, `SIO_KEEPALIVE_VALS`, and `IPPROTO_TCP` keepalive options be translated to Linux TCP options without silently changing timeout units, probe counts, or errors?

## VERIFIED FROM PRIMARY PUBLISHED DOCUMENTATION

1. Microsoft documents `SIO_KEEPALIVE_VALS` as a `WSAIoctl` control accepting `tcp_keepalive { u_long onoff, keepalivetime, keepaliveinterval; }`. Both time and interval are **milliseconds**. `SO_KEEPALIVE` enables keepalive with defaults unless per-connection values have been changed. The documented default time is two hours and interval one second when their registry keys are absent. Datagram sockets can return `WSAENOPROTOOPT`; unsupported providers can return `WSAEOPNOTSUPP`. Source: [Microsoft SIO_KEEPALIVE_VALS](https://learn.microsoft.com/en-us/windows/win32/winsock/sio-keepalive-vals).
2. Microsoft's `SIO_KEEPALIVE_VALS` page states that on Windows Vista and later the number of probes is fixed at ten. **Do not generalize this statement to every modern configuration without qualification:** Microsoft's separate `IPPROTO_TCP` socket-option table documents `TCP_KEEPCNT` support beginning with Windows 10 version 1703, and `TCP_KEEPIDLE` / `TCP_KEEPINTVL` beginning with Windows 10 version 1709. This is a documentation scope/version tension, not evidence of a runtime contradiction; it needs a pinned Windows-build test. Source: [Microsoft IPPROTO_TCP socket options](https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options).
3. Linux `tcp(7)` documents `TCP_KEEPIDLE`, `TCP_KEEPINTVL`, and `TCP_KEEPCNT` as **seconds**, **seconds**, and **count**, respectively; they apply when `SO_KEEPALIVE` is enabled. Source: [Linux tcp(7)](https://man7.org/linux/man-pages/man7/tcp.7.html). This manual is living documentation, **not** a pinned Linux kernel implementation or evidence of exact range/rounding behavior.

## HYPOTHESIS — compatibility boundary, not an architecture decision

A socket-specific guest option state should preserve the originally requested millisecond values and Windows-version/provider-specific semantics, then translate to host `TCP_KEEPIDLE` / `TCP_KEEPINTVL` / `TCP_KEEPCNT` only after checking Linux-supported ranges and quantization. Naively dividing milliseconds by 1000 truncates subsecond requests and may yield zero or an invalid host option. The Linux host defaults must not be mistaken for the guest Windows defaults. A policy for requests not exactly representable on Linux must be explicit (reject, approximate with disclosed compatibility deviation, or alternative transport management); no option is yet validated.

Guest `SIO_KEEPALIVE_VALS` errors and `WSAIoctl` overlapped/completion behavior require separate conformance checks. The existing R195 async-request ownership research is relevant, but does not establish keepalive IOCTL semantics. Host packet behavior may also differ under connection activity, retransmissions, and network failure.

## OPEN — falsification matrix (NOT EXECUTED)

1. On a **pinned Windows build** and provider, test `SO_KEEPALIVE` toggles, `SIO_KEEPALIVE_VALS` with 0/1/999/1000/1500 ms and large boundary values, and `TCP_KEEPIDLE`, `TCP_KEEPINTVL`, `TCP_KEEPCNT` where supported. Capture return values, `WSAGetLastError`, subsequent `getsockopt` values, and probe timing with a packet trace. Distinguish disabled keepalive from zero timeout.
2. On a **pinned Linux kernel**, test `SO_KEEPALIVE` and `TCP_KEEP*` option range/error behavior and query-back rounding. Observe actual packets and failure detection with a controlled unresponsive peer, rather than inferring probe timing from option values.
3. Compare a Windows version lacking newer `TCP_KEEP*` options with one supporting them; determine how `SIO_KEEPALIVE_VALS` interacts with `TCP_KEEPCNT` and whether Microsoft's fixed-ten statement describes only the legacy IOCTL/default path.
4. Verify `WSAIoctl` input size, null pointers, UDP rejection, provider unsupported errors, and overlapped request completion; keep failures separate from Linux `EINVAL`/`ENOPROTOOPT`.
5. Inspect pinned Linux `net/ipv4/tcp.c` option handling and Microsoft SDK definitions before writing a compatibility mapper. No kernel source revision was pinned in this review.

## Decision impact

**OPEN.** TCP keepalive is an observable protocol and failure-detection contract. A one-to-one integer copy is demonstrably dimensionally wrong for the documented IOCTL milliseconds versus Linux TCP-option seconds; exact Windows/Linux behavior, representability, and version policy remain untested. No code imported, no experiments claimed.
