# R191 — IOCP concurrency admission and worker association versus Linux readiness

Status: DOCUMENTATION-VERIFIED contracts; proposed implementation is HYPOTHESIS; no experiment.
Reviewed: 2026-11-19. Scope: x86-64 Windows guest IOCP scheduling semantics on a Linux host.

## Question
Can an NT-compatible Linux runtime implement Windows I/O completion port dequeue and worker admission by forwarding Linux epoll/io_uring events to arbitrary waiters, or does it need a separate guest port scheduler?

## Verified primary-source contracts
1. Microsoft describes a **FIFO queue of completion packets** and **LIFO release of waiting threads**. Packet enqueue order does not guarantee processing/completion order. A worker first waiting on a port becomes associated with it; a worker can be associated with at most one port and loses association when it exits, selects another port, or closes that port. [Microsoft IOCP overview](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports).
2. `CreateIoCompletionPort` sets `NumberOfConcurrentThreads` when creating the port; a value of zero chooses the processor count. The argument is ignored when attaching to an existing port. A file handle can be associated with only one port until the file handle closes. [Microsoft CreateIoCompletionPort](https://learn.microsoft.com/en-us/windows/win32/fileio/createiocompletionport).
3. Microsoft describes admission in terms of **running threads associated with that port** relative to its concurrency value. This is not a simple cap on the number of outstanding I/O operations or on the total number of threads permitted to call dequeue. [Microsoft IOCP overview](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports).
4. `PostQueuedCompletionStatus` injects application packets without performing I/O. `GetQueuedCompletionStatus` distinguishes timeout/closed-port (NULL overlapped) from failed I/O packet (non-NULL overlapped). A blocked dequeue on a closed port reports `ERROR_ABANDONED_WAIT_0` on supported modern Windows versions; XP/Server 2003 differ. [Microsoft IOCP overview](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports); [Microsoft GQCS](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus).
5. `GetQueuedCompletionStatusEx` may dequeue several packets in one call, and a waiting thread receives its own returned batch. This creates an additional scheduling/ownership decision absent from a simplistic single-packet wake. [Microsoft GQCSEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatusex).
6. Linux `EPOLLEXCLUSIVE` reduces wakeups across multiple epoll instances watching a target fd; it is a readiness wakeup facility, not an API promising Windows packet FIFO, waiter LIFO, port association, or Windows concurrency accounting. [Linux epoll_ctl(2)](https://man7.org/linux/man-pages/man2/epoll_ctl.2.html).

## Architectural inference (HYPOTHESIS, not implementation)
Maintain a guest IOCP object independent of the host event backend:
- A packet queue with enqueue sequence and source (posted or I/O), guest completion key, overlapped pointer, byte count and result.
- A per-port waiter stack and explicit per-guest-thread association; maintain worker state (blocked, admitted/running, transitioned to another port, exited).
- A concurrency-admission controller whose transitions are driven by guest dequeue/block/association state, not inferred solely from Linux runnable task count. **Open:** precise handling of an admitted worker blocked in unrelated host calls and transient scheduler preemption needs Windows-reference observation.
- A generation-safe lifetime for port, file association, packets and overlapped memory. Do not recycle host fd numbers as guest identity.
- An optional host epoll/io_uring adapter that *produces* packets but does not decide guest delivery order.

Do not claim the overview implies strict deterministic worker choice under every race: concurrency, scheduling and batched dequeues require differential tests. Do not assume Linux epoll's EPOLLEXCLUSIVE is an IOCP implementation.

## Proposed conformance gates (NOT RUN)
1. Windows reference: port concurrency=1, 2, 0; 2–8 named worker threads; post a sequence of unique completion keys and track dequeue order, waiter order, running worker count and timing. Repeat across runs; classify nondeterminism.
2. Compare `GetQueuedCompletionStatus` with `GetQueuedCompletionStatusEx` batching under multiple waiters. Check FIFO packet identity without assuming callback execution order.
3. Have a worker block outside the port after receiving a packet; determine whether/when another waiter is admitted. Repeat for thread exit, switching ports, closing the port and reentering dequeue.
4. Confirm `NumberOfConcurrentThreads` ignored when associating another handle with an existing port, and single-port file-handle association restrictions.
5. Inject `PostQueuedCompletionStatus` packets without I/O and separately failed overlapped I/O; check NULL versus non-NULL overlapped and error paths.
6. Host-only prototype: compare an explicit guest scheduler against direct epoll wakeup for ordering, fairness and latency, with pinned kernel/CPU versions, CPU affinity, repetitions and variance. No benchmark or prototype is reported here.

## Relation to existing research
R190 covers per-handle *notification suppression* (whether a completion packet is emitted). This note covers the distinct downstream *port scheduling and admission* question. Earlier IOCP/cancellation notes remain authoritative for request lifetime, cancellation and completion routing. This note does not revise their findings.

## Open evidence
- Exact Windows running-thread accounting during unrelated blocking, APC delivery and scheduler preemption.
- Port close with outstanding file associations and packet lifetime under concurrent close/dequeue.
- Behavior differences across supported Windows builds, processor groups and constrained CPU affinity.
- Fairness/starvation and performance cost of guest scheduler relative to host wakeups.

No Windows/Linux differential experiment was performed and no architecture has been selected.
