# R195 — NT asynchronous request ownership across cancellation, handles and IOCP

Date: 2026-11-20. Status: **cross-component architecture synthesis / primary API documentation only**. No code, Windows reference execution, Linux experiment or benchmark was performed.

## Distinct integration question

R15, R43, R145, R160, R190 and R191 examine pieces of asynchronous I/O. The remaining integration problem is **who owns an outstanding guest I/O request** when the initiating thread, file handle, completion port and backing Linux operation have different lifetimes. This note proposes a falsifiable ownership model; it does not claim to reproduce undocumented Windows internals.

## Documented contracts (VERIFIED from primary Microsoft API documentation; not experimentally verified here)

1. **Overlapped completion is distinct from submission.** `ReadFile` can return FALSE with `ERROR_IO_PENDING` for an overlapped request; that status is not itself terminal failure. Completion must be determined using the documented overlapped completion mechanisms. [ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile); [GetOverlappedResult](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult).
2. **Cancellation is a request, not proof of cancellation.** `CancelIoEx` targets outstanding I/O on a file handle, optionally narrowing by `OVERLAPPED`; canceled requests can still complete normally, and callers must not reuse/free their `OVERLAPPED` storage before completion. [CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/fileio/cancelioex-func).
3. **The issuing thread matters for some APIs.** `CancelIo` cancels pending operations issued by the calling thread on the given handle, unlike `CancelIoEx`. [CancelIo](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelio).
4. **IOCP association and completion delivery are separate.** `CreateIoCompletionPort` associates eligible file handles with a port and completion key; `GetQueuedCompletionStatus` returns a completion packet or a documented failure/timeout condition. A FALSE return with a non-NULL overlapped pointer indicates a dequeued failed operation, not necessarily an empty queue. [CreateIoCompletionPort](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-createiocompletionport); [GetQueuedCompletionStatus](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus).
5. **Per-handle notification modes affect delivery.** `SetFileCompletionNotificationModes` includes `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS` and `FILE_SKIP_SET_EVENT_ON_HANDLE`, subject to documented applicability restrictions. [SetFileCompletionNotificationModes](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-setfilecompletionnotificationmodes).
6. **Linux io_uring completion/cancellation is a different API contract.** Linux documents request submission, CQEs and asynchronous cancellation operations; neither an io_uring CQE nor a Linux fd is automatically a Windows `OVERLAPPED`, NT handle or IOCP packet. [Linux io_uring documentation](https://docs.kernel.org/io_uring/index.html); [io_uring asynchronous cancel](https://man7.org/linux/man-pages/man3/io_uring_prep_cancel64.3.html).

The cited documentation establishes *public contracts*. Exact race outcomes, unsupported file types, OS-build differences and undocumented NT kernel request internals remain **OPEN**.

## Proposed request state and ownership (HYPOTHESIS)

Use a guest request record keyed by an internal generation-safe ID, **not** solely by a reused guest handle value, Linux fd or caller-owned `OVERLAPPED*` address. Model the following logically separate attributes:

- **Submission:** accepted or rejected; a rejected request must not accidentally acquire a completion obligation.
- **Backend:** not started, pending, finished or cancellation requested; backend cancellation is not equivalent to guest terminal status.
- **Guest completion:** exactly one terminal result and transferred byte count for an accepted request, subject to the selected Windows API's notification rules.
- **Delivery:** event signaling, IOCP packet eligibility/enqueue/dequeue and threadpool notification accounting are separate observations.
- **References:** guest file object, port association, backend resource and guest memory buffers have individually tracked lifetimes.

**Invariant H1:** A completion race and a cancellation race may choose different terminal statuses, but cannot publish two terminal results for the same accepted request.

**Invariant H2:** Closing a guest handle removes one process-relative reference; it must not silently turn a still-owned backend request into a different request after Linux fd reuse. Exact Windows close-versus-pending-I/O behavior requires reference tests.

**Invariant H3:** Never dereference the guest `OVERLAPPED*` after its memory has been unmapped without a validated memory-access protocol. An application is contractually required to keep storage valid until completion, but malformed/untrusted guest code remains a host memory-safety threat.

**Invariant H4:** An IOCP packet is an owned delivery artifact, not the backend CQE itself. An immediate success with suppression flags, a failure packet, a port close and an APC path need independently specified delivery outcomes.

**Invariant H5:** Do not map Linux cancellation errno directly to a guest error without establishing the guest-visible terminal result and `GetLastError`/NTSTATUS conversion rules (R52).

## Falsifiable acceptance gates — NOT RUN

1. **Windows-reference matrix:** on recorded Windows builds, exercise immediate completion, pending completion, request cancellation, handle close, port close, duplicate handles and event signaling. Record API returns, `GetLastError`, `OVERLAPPED.Internal/InternalHigh`, byte counts and IOCP dequeues; do not infer undocumented behavior from one run.
2. **Race matrix:** independently race cancel, backend completion, duplicate-handle close and dequeue across thousands of seeded iterations. Assert at most one terminal guest request result and no use-after-free. Log cases where public Windows behavior is unspecified rather than imposing invented compatibility expectations.
3. **Notification matrix:** repeat with supported `SetFileCompletionNotificationModes` flags, success/failure, synchronous/asynchronous completion, and threadpool I/O credit accounting. Separate notification suppression from request completion.
4. **Backend matrix:** compare a deterministic in-memory mock backend against a real Linux backend (io_uring where available; other backends only if explicitly implemented). Assert backend CQE/fd reuse cannot change the guest request ID.
5. **Fault injection:** inject guest buffer unmapping, `OVERLAPPED` reuse, backend cancellation failure, process termination and host worker failure. Classify contract violations by the guest separately from host safety failures.
6. **Performance only after correctness:** measure request allocations, extra wakeups, queue latency, throughput and p50/p99 on equivalent workloads; do not claim io_uring or IOCP equivalence or performance superiority without measurements.

All future experiments must state Windows build, Linux kernel, io_uring feature availability, request type, handle flags, fixture hashes, repeat count and PASS/FAIL/SKIP.

## Architecture decision

**OPEN:** Adopt the **guest request record and independently owned completion packet** as the *candidate* integration boundary for a first asynchronous-file-I/O implementation. Do not equate backend completion, guest-visible terminal completion, and notification delivery. Before committing to the model, validate the Windows-reference matrix and reconcile any counterexamples.

**Nonduplication:** R43 treats cancellation/completion suppression; R145 thread-scoped synchronous cancellation; R160 threadpool I/O credits; R190 per-handle modes; R191 port scheduling. R195 isolates cross-component ownership and exactly-once guest completion under cancellation, close, reuse and backend races.