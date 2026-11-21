# R210 — Process-wide memory barrier: FlushProcessWriteBuffers versus Linux membarrier

Research date: 2026-11-20. Status: documentation review only; **no experiment performed**.

## Scope and source contracts

**VERIFIED (Microsoft API documentation):** `FlushProcessWriteBuffers(void)` returns `VOID`, flushes write queues of processors running threads of the current process, and guarantees visibility of writes between processors. The documentation describes interprocessor interrupts to processors in the process affinity. This is a process-wide synchronization primitive, not an instruction-cache flush or file durability operation. [M1]

**VERIFIED (Linux manual/kernel documentation):** `membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0)` ensures that running sibling threads of the *calling Linux process* have passed through a state that orders user-space memory accesses; non-running threads are treated as already satisfying this condition. The command requires successful prior `MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED` registration, and support is discoverable using `MEMBARRIER_CMD_QUERY`. The kernel describes scheduler and architecture barriers underpinning this contract. [L1][L2]

**VERIFIED (Microsoft .NET API documentation):** `Interlocked.MemoryBarrierProcessWide` is described as wrapping `FlushProcessWriteBuffers` on Windows and `sys_membarrier` on Linux. This is unusually direct primary-source evidence for the Linux primitive as a candidate, but it does **not** establish bit-for-bit compatibility for all native Windows callers or every Linux kernel configuration. [M2]

## Architecture consequences

**HYPOTHESIS:** A single Linux process hosting one NT guest process could implement a guest process-wide memory-barrier entry point with registered `MEMBARRIER_CMD_PRIVATE_EXPEDITED`, provided all guest threads belong to that Linux process and the call is made from the correct host process context. Register once during process initialization, then issue the expedited barrier on demand. Registration and feature-query failures must not silently become no-ops.

**OPEN:** A broker or split-process design in which guest threads live in multiple Linux processes cannot assume `PRIVATE_EXPEDITED` reaches all guest threads. The guest NT process boundary and host Linux process boundary must be mapped explicitly. The global-expedited variant only targets processes that opted into global registration, so it is not an unconditional substitute. [L1]

**OPEN:** Microsoft's documented processor-affinity wording and Linux's running-sibling-thread wording differ. The public documents do not establish equivalence under dynamic affinity changes, CPU hotplug, concurrent thread creation/exit, or a guest process whose threads span multiple host processes. No claim of exact Windows equivalence is made.

**OPEN:** Interaction with a syscall-dispatch/signal-based NT entry point: if a guest `FlushProcessWriteBuffers` is forwarded through a helper process, that helper's private membarrier is scoped to the wrong Linux process. The operation should be executed in the guest host process or use an explicitly validated cross-process protocol. No signal-handler-safe implementation is inferred.

**SECURITY GATE:** An untrusted guest may call the barrier repeatedly, causing cross-CPU overhead. Treat the primitive as potentially expensive; benchmark and account for resource-exhaustion without changing its observable synchronization guarantee.

## Proposed conformance gates (NOT RUN)

1. On Windows, create an independently compiled multi-thread stress fixture calling `FlushProcessWriteBuffers` with controlled shared-memory publication; record compiler atomics, CPU affinity and OS build. Avoid interpreting an absence of failures as a proof of memory-model equivalence.
2. On Linux, query `membarrier` capabilities, register private expedited mode, verify expected return values and negative paths (`EPERM` before registration, `ENOSYS`/unsupported handling where applicable). Report actual kernel version and syscall availability.
3. Test one guest process hosted in one Linux process with many threads; repeat with deliberately split host-process topology to expose the scope mismatch.
4. Run latency distributions across core counts and contention; separate registration cost from barrier-call cost and record tail latency. No benchmark numbers are currently available.
5. Audit all guest entry paths and broker boundaries to ensure the barrier targets the guest's executing threads; do not substitute `atomic_thread_fence` or a single-core fence for a process-wide rendezvous.

## Sources (primary, retrieved 2026-11-20)

- [M1] Microsoft, FlushProcessWriteBuffers API: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-flushprocesswritebuffers
- [M2] Microsoft, Interlocked.MemoryBarrierProcessWide: https://learn.microsoft.com/en-us/dotnet/api/system.threading.interlocked.memorybarrierprocesswide
- [L1] Linux man-pages, membarrier(2): https://man7.org/linux/man-pages/man2/membarrier.2.html
- [L2] Linux kernel documentation, membarrier architecture requirements: https://docs.kernel.org/scheduler/membarrier.html

No kernel version or source commit was pinned for this documentation-only review. Before implementing, pin the target Linux kernel revision and inspect its `kernel/sched/membarrier.c` and relevant architecture paths.
