# R225 — Windows I/O rings versus Linux io_uring: submission, completion, and authority gates

Status: **VERIFIED FROM SOURCE** for the linked API descriptions; **HYPOTHESIS** for the proposed translation; **OPEN** for conformance and performance. Reviewed 2026-11-21. No experiment was executed.

## Scope and non-duplication

R195 covers generic asynchronous request ownership; R190 covers I/O completion notification modes; R213 covers Winsock RIO. This note addresses the distinct **Windows I/O ring** API (HIORING, submission and completion queues) and whether Linux io_uring can serve as an implementation backend without exposing host descriptors or misreporting guest API capabilities. This is not evidence that the two APIs have interchangeable ABI layouts or security models.

## Directly inspected primary contracts

1. **VERIFIED FROM SOURCE — create and lifetime.** Microsoft `CreateIoRing` accepts an `IORING_VERSION`, flags, requested submission/completion sizes, and returns an `HIORING`. The system may round queue sizes to powers of two; the completion queue is at least twice the actual submission size. `CloseIoRing`, not `CloseHandle`, releases the ring. Documented minimum Windows build: 22000. Source: https://learn.microsoft.com/en-us/windows/win32/api/ioringapi/nf-ioringapi-createioring (page updated 2025-10-31).

2. **VERIFIED FROM SOURCE — capability discovery.** `QueryIoRingCapabilities` is cached per process; Microsoft explicitly cautions that results can differ between processes and runs. A guest must not blindly expose every operation supported by its host io_uring. Source: https://learn.microsoft.com/en-us/windows/win32/api/ioringapi/nf-ioringapi-queryioringcapabilities (updated 2025-10-31).

3. **VERIFIED FROM SOURCE — construction and identity.** `BuildIoRingReadFile` takes `IORING_HANDLE_REF`, `IORING_BUFFER_REF`, byte count, explicit file offset, caller-supplied `userData` and flags. The documentation requires unique `userData` when the application intends to cancel by that identifier; queue-full and unsupported-required-flag are distinct HRESULT failures. Applications are directed to check `IsIoRingOpSupported`. Source: https://learn.microsoft.com/en-us/windows/win32/api/ioringapi/nf-ioringapi-buildioringreadfile (updated 2024-02-22).

4. **VERIFIED FROM SOURCE — submit/wait semantics.** `SubmitIoRing` submits constructed entries and optionally waits for a requested completion count; a zero wait count does not wait. The documentation distinguishes `IORING_E_WAIT_TIMEOUT` (submission succeeded but wait timed out) from other submission failures (entries remain queued); an individual request error instead produces an error completion. Source: https://learn.microsoft.com/en-us/windows/win32/api/ioringapi/nf-ioringapi-submitioring (updated 2025-10-31).

5. **VERIFIED FROM SOURCE — completion consumption.** `PopIoRingCompletion` removes one CQE; `S_FALSE` means empty and leaves the output structure unchanged. Source: https://learn.microsoft.com/en-us/windows/win32/api/ioringapi/nf-ioringapi-popioringcompletion (updated 2025-10-31).

6. **VERIFIED FROM SOURCE — Linux backend differences.** Linux io_uring uses mmap-shared SQ/CQ and `io_uring_enter`; Linux CQEs return a signed result (`-errno` on error), and `user_data` correlates submissions with completions. Completions need not be ordered. The Linux documentation explicitly identifies exceptional modes (multishot and CQE_SKIP_SUCCESS), so assuming exactly one CQE per request is unsafe without constraining enabled operations/flags. Buffer lifetime extends through I/O completion for read/write payloads. Source: https://man7.org/linux/man-pages/man7/io_uring.7.html (online Linux man-pages text; **not** a pinned kernel revision).

## Architectural hypothesis (not a decision)

Maintain a **guest-owned HIORING object** and its own pending-submission/CQ state machine. Translate supported guest requests through a broker-managed host io_uring, while retaining guest handle rights, offset rules, buffer ownership, HRESULT mapping, and cancellation identity in guest metadata. Associate each backend request with a generation-stamped internal ID rather than using an untrusted guest `userData` as a host pointer or unique internal key. Translate host `-errno` to a documented guest error only after per-operation validation; do not claim a universal mapping.

Submission must be staged so a host partial-submit/failure does not silently violate Microsoft's documented `SubmitIoRing` queue-retention contract. If atomic staging is impossible for a particular backend mode, emulate or reject that mode instead of silently changing semantics. Keep Windows CQEs independently of host CQE draining; a completed host request is not equivalent to a guest-observed popped completion.

## Falsification and acceptance gates (NOT RUN)

- **Windows-reference:** enumerate capabilities/versions and actual queue sizes on specified Windows builds; distinguish queue-full at build time, invalid required flags, unsupported operations, and empty pop (verify output unchanged).
- **Submission fault injection:** enqueue multiple valid operations plus one invalid operation; distinguish whole-submit failure from individual error CQE; compare the queue contents on retry, submitted count, and timeout after successful submission.
- **Completion identity:** submit same `userData` with and without cancellation intent, out-of-order operations, simultaneous cancellation/completion, and close/reopen handle generations. Assert exactly the documented guest completions, without leaking internal IDs.
- **Lifetime/security:** revoke guest handle rights before build and after submission; close ring with in-flight operations; unmap or modify guest buffers during pending work under a controlled, safe fixture; verify broker never passes arbitrary guest host-fd numbers or host pointers.
- **Linux backend:** record kernel release, io_uring feature/probe results, seccomp restrictions, and actual supported opcodes. Test with and without CQE_SKIP_SUCCESS/multishot features, or explicitly disable them.
- **Performance:** compare equivalent read workloads with a synchronous backend and io_uring, including staging and translation costs, repetitions, confidence intervals and tail latency; do not extrapolate from Linux-only microbenchmarks to Windows applications.

## Open questions

1. Exact supported Windows I/O ring versions, opcode matrix, registration and cancellation behavior across target builds need version-pinned SDK header and Windows-reference tests.
2. Windows I/O ring interaction with file sharing, impersonation, asynchronous cancellation and NT file object lifetime remains unproven.
3. Linux io_uring availability under real host seccomp and kernel policy is not established for the project test environment.
4. No Windows I/O ring executable fixture, host io_uring probe or benchmark was run for this note.

**Boundary:** A plausible Linux backend does not establish ABI compatibility, equal completion behavior, isolation, or improved performance.
