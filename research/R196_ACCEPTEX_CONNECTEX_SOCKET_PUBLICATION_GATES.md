# R196 — Winsock AcceptEx/ConnectEx state publication versus Linux asynchronous sockets

Status: PRIMARY-SOURCE DOCUMENTATION REVIEW (2026-11-20). No Windows or Linux experiment executed. Architecture remains OPEN.

## Scope and verified contracts

**VERIFIED — Microsoft AcceptEx.** AcceptEx requires an already-listening socket and a separate open accept socket that is neither bound nor connected. Its caller supplies an OVERLAPPED and an output buffer holding optional initial receive bytes plus both addresses. Each address reservation must exceed the protocol maximum address length by at least 16 bytes. With zero receive length the request can complete on connection arrival; with a nonzero receive length completion waits for initial data. On asynchronous initiation, the bytes-received output argument is not populated; the caller obtains the count from its completion mechanism. A successfully accepted socket initially supports only a restricted operation set; SO_UPDATE_ACCEPT_CONTEXT associates listening-socket properties before ordinary socket inspection. The extension pointer is retrieved through WSAIoctl/SIO_GET_EXTENSION_FUNCTION_POINTER and WSAID_ACCEPTEX. [M1]

**VERIFIED — Microsoft GetAcceptExSockaddrs.** The helper interprets the original AcceptEx output buffer using the exact receive/local/remote length arguments supplied for the request; it is not a general sockaddr decoder independent of the buffer layout. [M2]

**VERIFIED — Microsoft ConnectEx.** After successful ConnectEx, the socket does not enable previously set options/properties until SO_UPDATE_CONNECT_CONTEXT is applied. This post-connect state is observable at the Winsock API boundary. [M3]

**VERIFIED — Linux liburing accept.** io_uring_prep_accept and its direct/multishot variants prepare accept-like operations. Each completion exposes a result in CQE.res (negative errno on failure); multishot accepts can emit multiple CQEs and indicate continuation with IORING_CQE_F_MORE. Direct descriptors belong to io_uring's registered-file table, not to the Windows socket handle namespace. [L1]

## Architectural mismatch (inference, not measured)

**HYPOTHESIS H1.** A direct mapping from one Linux accept CQE to one guest AcceptEx completion is insufficient when dwReceiveDataLength is nonzero: the guest request must retain ownership of the accepted connection while awaiting the first receive and while constructing the guest buffer's address layout. This may require a compound guest request with distinct host accept and receive phases.

**HYPOTHESIS H2.** Guest-visible socket state needs an explicit pre-context-update stage for both AcceptEx and ConnectEx; exposing Linux's already-connected fd directly to arbitrary guest socket calls could violate documented Winsock behavior.

**HYPOTHESIS H3.** Linux multishot accept can be an internal optimization for a backlog of distinct guest AcceptEx requests, but cannot itself define guest request identity, output-buffer ownership, OVERLAPPED completion ordering or cancellation. See R195 for cross-component request ownership and R190/R191 for completion policy and worker admission.

## Proposed state machine (design proposal, not implementation)

1. Validate guest listening/accept handles, protocol family, buffer extents and OVERLAPPED identity; reserve request generation and guest buffer lifetime.
2. Associate a unique guest accept socket with one pending request; retain both guest object references independently of recyclable fd numbers.
3. On host accept, capture local/peer address data. If initial receive length is nonzero, continue to an asynchronous receive; do not publish a successful guest completion merely because accept succeeded.
4. Encode received bytes and two guest address regions according to the AcceptEx layout; publish terminal request status exactly once through the selected guest completion channel.
5. Keep the accepted socket in a guest state requiring SO_UPDATE_ACCEPT_CONTEXT before the documented wider operation set is exposed.
6. On cancellation, close, timeout or peer reset, arbitrate one terminal outcome; clean up any accepted-but-unpublished host socket. No assertion is made here about Windows race winner ordering.

## Required conformance gates (all OPEN)

- **Windows reference:** run zero/nonzero initial receive cases; connect without sending data; disconnect before initial data; inspect synchronous/pending byte-count reporting and exact output buffer offsets for IPv4/IPv6.
- **Windows reference:** probe operations before/after SO_UPDATE_ACCEPT_CONTEXT and SO_UPDATE_CONNECT_CONTEXT, and invalid accept socket states; record exact Winsock errors and OS build.
- **Windows reference:** submit multiple AcceptEx operations, then race closes/cancellation/peer resets; observe which completion channels report and whether any operation completes twice.
- **Linux prototype:** compare epoll+accept4+recv with io_uring accept+recv; verify ownership across cancellation, CQE completion and fd reuse; log kernel/liburing versions and capability fallback.
- **Security:** reject guest buffer length overflow and stale OVERLAPPED generations; ensure no accepted socket leaks or cross-request address/data contamination.
- **Performance:** compare latency and throughput only after semantic conformance; never infer performance from the number of syscalls alone.

## Sources (retrieved 2026-11-20)

- [M1] Microsoft, AcceptEx (mswsock.h): https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex
- [M2] Microsoft, GetAcceptExSockaddrs (mswsock.h): https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-getacceptexsockaddrs
- [M3] Microsoft, LPFN_CONNECTEX: https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_connectex
- [L1] liburing upstream manual, io_uring_prep_accept: https://man7.org/linux/man-pages/man3/io_uring_prep_accept.3.html

No upstream source revision pinned for these documentation pages. Verify version-specific behavior against a pinned Windows build and Linux kernel before coding.
