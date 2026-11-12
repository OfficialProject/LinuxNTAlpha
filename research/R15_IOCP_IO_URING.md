# R15 — Windows I/O completion ports versus Linux io_uring

Date: 2026-11-12. Scope: verified documentation comparison; no performance measurements.

## Sources inspected
1. Microsoft, I/O completion ports: https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports
2. Linux manual, io_uring(7): https://man7.org/linux/man-pages/man7/io_uring.7.html

## Verified Windows IOCP semantics
- A completion port has a completion-packet queue; packets are queued FIFO, though actual dequeue/processing order can differ.
- Waiting threads are released LIFO, with runnable worker concurrency controlled by the port's configured concurrency value. This is a scheduler contract, not merely a queue.
- CreateIoCompletionPort associates I/O handles with a port; PostQueuedCompletionStatus injects user packets.
- Completion port references can persist through associated I/O handles, affecting lifetime.
- A thread is associated with at most one port at a time.
- An I/O completion port is associated with its creating process and is not shared between processes, though threads in that process share it.
- Supported overlapped I/O endpoints include files, sockets and pipes.

## Verified Linux io_uring semantics
- Submission and completion rings exchange asynchronous operations with the kernel.
- CQEs are correlated with submissions via user_data; execution/completion order is not guaranteed.
- Some features intentionally alter one-CQE-per-SQE behavior (multishot, skip-success).
- Submission queue polling can avoid a syscall per submission, at the cost of kernel polling activity.
- Completion readiness and worker scheduling are separate concerns from IOCP's runnable-thread concurrency gate.

## Non-equivalence and design implications
**HYPOTHESIS:** io_uring is a strong candidate as the Linux I/O execution backend, but its CQ is not directly a Windows IOCP object. A compatibility scheduler must implement packet semantics, completion keys, OVERLAPPED identity, thread association, port lifetime and concurrency gating.

A naive one-to-one CQE -> IOCP packet translation may violate Windows expectations for synchronous completions, cancellation, completion suppression, error propagation and handle closure. Those require explicit conformance tests.

## Conformance plan
1. Associate multiple endpoints to one port; verify completion key and OVERLAPPED identity.
2. Post synthetic completion packets without I/O.
3. Test FIFO packet ordering separately from LIFO waiter release.
4. Measure worker concurrency under configured limit 1, 2, CPU count, including blocking worker transitions.
5. Cancel pending I/O, close endpoint, close port, race completion with cancellation.
6. Compare synchronous success/error, short read/write, socket and file behavior.
7. Ensure cross-process access and handle inheritance follow Windows rules.
8. Validate APC-based completion modes separately from IOCP; do not conflate the two.

## Benchmark design
Measure throughput, p50/p95/p99 completion latency, context switches, CPU use and queue depth. Compare Linux io_uring backend to epoll/threadpool backend, and native Windows IOCP baseline. Pin kernel versions, CPU topology, I/O device, filesystem, operation size and worker counts. **No benchmark results exist yet.**

## Decision
Keep io_uring as a candidate backend; do not claim it reproduces IOCP scheduling or guarantees superior performance without correctness and measurement.
