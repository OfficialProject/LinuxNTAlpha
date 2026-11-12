# Linux syscall interception overhead — Syscall User Dispatch

This x86-64 Linux microbenchmark compares 7 batches of direct raw getpid syscalls permitted by Syscall User Dispatch (SUD) against 7 batches of raw getpid syscalls blocked by SUD and intercepted in a SIGSYS handler.

**Run:** make benchmark (20000 calls × 7 timed rounds per condition). For an extremely short correctness smoke test: make test (1000 calls × 7 rounds). Both paths are warmed up with 1000 calls before measurement. The program pins itself to the first CPU permitted by its Linux CPU affinity mask if possible, alternates measurement order between pairs of batches, and uses CLOCK_MONOTONIC_RAW to time each batch.

**Control:** selector is set BLOCK then ALLOW before issuing a native Linux syscall. **Intercepted:** selector is set BLOCK, syscall traps, handler sets ALLOW to permit sigreturn and returns a synthetic result in saved RAX. The handler verifies SIGSYS type and syscall number. Both paths execute machine-level syscall instructions; only the intercepted path delivers a signal. This is not a Windows-origin syscall or a security sandbox.

Program reports minimum, median and maximum **per-call batch averages** in nanoseconds for each condition, plus median-batch ratio. These are NOT individual-call latency percentiles or representative frame-time percentiles. No user-space NT implementation, syscall translation cost, process broker, scheduler model or app workload is included.

**Caveats:** host is a container, actual physical clock/CPU topology and virtual machine co-tenancy are not controlled, and compiler/kernel/security policies may differ. Results should never be used to claim all SUD-based NT implementations are slow or that a kernel NT rewrite is justified. Compare with direct export-stub calls separately, and benchmark realistic rates of raw NT syscalls before deciding if the signal overhead matters.

**Exit:** 0 if correctness checks pass; 77 if the PR_SET_SYSCALL_USER_DISPATCH facility is unavailable; otherwise failure.

Primary source: https://docs.kernel.org/admin-guide/syscall-user-dispatch.html . The kernel explicitly notes the cost of delivering signals and clarifies that SUD is NOT a sandbox.

Full recorded measurements: [R22](../../research/R22_SYSCALL_DISPATCH_LATENCY.md).
