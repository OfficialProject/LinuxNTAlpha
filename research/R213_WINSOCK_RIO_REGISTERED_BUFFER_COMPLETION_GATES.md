# R213 — Winsock Registered I/O (RIO) versus Linux registered buffers and completion queues

Research date: 2026-11-21. **Status: primary-documentation review only; no experiment, Windows differential run, implementation, or performance measurement.**

## Why this is a separate compatibility gate

R195 describes general asynchronous request ownership; R209 covers datagram ancillary translation. Neither makes Winsock **Registered I/O (RIO)** interchangeable with conventional overlapped Winsock or IOCP. RIO exposes its own runtime-discovered function table, registered-buffer identifiers, request queues, completion queues and notification/dequeue contract. A Linux io_uring backend is a candidate, not a semantic substitute.

## Verified contracts (primary documentation)

1. Microsoft documents `RIO_EXTENSION_FUNCTION_TABLE` with `RIORegisterBuffer`, `RIODeregisterBuffer`, `RIOCreateRequestQueue`, `RIOCreateCompletionQueue`, `RIOReceive[Ex]`, `RIOSend[Ex]`, `RIODequeueCompletion`, `RIONotify` and queue-resize/close operations. The table is obtained at runtime via `WSAIoctl(SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER)` with `WSAID_MULTIPLE_RIO`, not assumed to be normal imported exports. The table documentation lists Windows 8 / Windows Server 2012 minimum support. [MS table](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/ns-mswsock-rio_extension_function_table).
2. `RIOCreateRequestQueue` binds a socket to receive and send RIO completion queues and specifies outstanding-request and buffer-count limits, plus an application socket context. The Windows 8/Server 2012 documentation notes a one-buffer constraint for `MaxReceiveDataBuffers`; version-specific behavior must not be generalized without reference testing. [MS request queue](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_riocreaterequestqueue).
3. `RIOCreateCompletionQueue` takes a queue capacity and optional notification descriptor (event or I/O completion), and `RIONotify` requests notification; this is **not** itself the dequeue operation. [MS completion queue](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_riocreatecompletionqueue).
4. `RIODequeueCompletion` removes up to a specified number of completion entries into an array of `RIORESULT` structures; on error it can return `RIO_CORRUPT_CQ`. The Microsoft reference explicitly distinguishes completion notification from retrieving completed requests. [MS dequeue](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_riodequeuecompletion). The `RIO_CQ` reference also documents nonblocking polling through dequeue. [MS CQ](https://learn.microsoft.com/en-us/windows/win32/winsock/riocqueue).
5. Linux liburing documents registered buffers as a kernel-side mapping/pinning optimization and notes kernel-memory / historical `RLIMIT_MEMLOCK` constraints. These buffers are registered **with a ring** and used with fixed-buffer operations; they are not automatically equivalent to guest-visible `RIO_BUFFERID` lifetimes or RIO completion notification. [Linux registered buffers](https://man7.org/linux/man-pages/man7/io_uring_registered_buffers.7.html), [liburing register buffers](https://man7.org/linux/man-pages/man3/io_uring_register_buffers.3.html).

## Architectural inference — explicitly unverified

**Candidate design:** maintain a guest RIO object registry separate from Linux ring registrations. Give each guest buffer ID, request queue and completion queue a generation-protected identity; retain in-flight references to guest buffers and socket objects until the guest-visible terminal completion is retired. Translate Linux CQEs to guest RIORESULT records and manage RIONotify arming separately from dequeue. Enforce guest queue limits before backend submission. These are design hypotheses, **not claims of Windows behavior**.

**Security boundary:** never expose host ring offsets, host pointers, pinned-buffer identifiers or host file descriptors as guest RIO handles. Validate every guest offset+length with overflow-safe arithmetic against the registered guest buffer extent. The backend may need copying or alternate submission paths if io_uring fixed-buffer constraints do not match RIO; zero-copy/performance benefits are unproven.

**Unknowns to resolve from additional Microsoft API pages and Windows tests:** exact buffer deregistration behavior with outstanding requests; queue-full/corrupt handling; `RIONotify` rearming races; socket closure and outstanding request terminal results; exact request-context/result ordering; request queue resizing under load; per-version restrictions; interplay with normal Winsock calls on the same socket. Do not silently infer these from Linux behavior.

## Proposed conformance gates (NOT RUN)

- On a Windows reference machine, discover the RIO table via the documented WSAIoctl path; record supported socket types and exact errors for unsupported providers and malformed table sizes.
- Register buffers; test zero-length, overlapping slices, out-of-bounds offsets, invalid/stale IDs and deregistration with pending receive/send operations. Record errors and buffer visibility, including races.
- Create small receive/send CQs and RQs; fill outstanding limits, drain in batches, test empty polling and notification-before/after-enqueue; record event versus IOCP notification behavior and any duplicate or lost notifications.
- Exercise socket close, request-queue resize and CQ close while operations are outstanding, including concurrent dequeuers; compare request contexts, byte counts, error results and ordering.
- On Linux, separately probe io_uring availability, fixed-buffer registration, socket operation support, resource limits and cancellation/close races. Compare semantic traces against Windows, not merely throughput.
- Only after correctness: benchmark memory pinning, latency distribution and CPU overhead versus ordinary overlapped Winsock/IOCP on matched workloads. No numerical performance claim exists.

## Decision gate

**Do not select io_uring as the sole RIO implementation strategy yet.** First specify the guest-owned buffer/request/CQ lifetime state machines and obtain Windows-reference traces for the unresolved cases. Keep the backend replaceable and do not merge RIO CQ semantics with ordinary IOCP semantics without evidence.

Related: [R195](R195_ASYNC_REQUEST_OWNERSHIP_COMPLETION_GATES.md), [R209](R209_WINSOCK_DATAGRAM_ANCILLARY_TRANSLATION_GATES.md), [R191](R191_IOCP_CONCURRENCY_WORKER_ADMISSION_GATES.md).
