# R22 — Measured Linux Syscall User Dispatch SIGSYS overhead

Date: 2026-11-12. Status: **REPRODUCED microbenchmark in one Linux container**. Not a Windows ABI benchmark, whole-system result, p99 estimate, or basis for selecting a permanent architecture.

## Question

How expensive is one syscall-instruction interception through Linux Syscall User Dispatch (SUD), compared with allowing a raw Linux getpid syscall in the same process on the same CPU? If the cost is high per trap, a design that traps every Windows NT syscall may need more careful analysis than an import-level ABI bridge using normal calls.

## Mechanism and source

The Linux kernel documents SUD at https://docs.kernel.org/admin-guide/syscall-user-dispatch.html . It permits dispatch of selected user syscalls into a SIGSYS handler, including a userspace selector. The docs caution that signal delivery costs time and clarify that SUD is **not a sandbox**.

See executable source and build instructions at [experiments/sud-latency](../experiments/sud-latency/).

## Method

- Linux 6.18.57 x86-64, Debian GNU/Linux 13 (trixie) container, GCC 14.2.0, Clang 17.0.0. Physical host, power governor and virtualization overhead not established.
- Program attempts to pin to the first CPU allowed by sched_getaffinity; all recorded runs printed **cpu_pinned=0**.
- Enable PR_SET_SYSCALL_USER_DISPATCH, install a SIGSYS SA_SIGINFO handler that recognizes SYS_getpid, sets the selector to ALLOW for signal return, and writes synthetic 0x12345 into saved RAX.
- **Control:** for each iteration write selector BLOCK, then ALLOW, then execute the raw Linux getpid syscall which succeeds normally.
- **Intercepted:** for each iteration write selector BLOCK and execute the same raw Linux getpid syscall, which enters the SIGSYS handler; handler sets selector to ALLOW for sigreturn. This returns the same synthetic value in every trapped call. Count and check all traps and siginfo.
- Time each whole batch using CLOCK_MONOTONIC_RAW. Warm each path with 1000 iterations before timing. Seven timed batches per condition, alternating order to reduce directional bias, then report min/median/max **per-call batch averages**.
- **NOT** individual-call latency percentiles. No frequency/governor controls, signal-context benchmark against Microsoft Windows, or workloads using actual Windows ntdll/NT syscalls.

## Observed exploratory results

Numbers are nanoseconds per call, each a **summary of seven batch averages**, not 7 × 10000 direct latency samples.

| Iterations per timed batch | Control batch median | SIGSYS intercepted batch median | Median ratio | Control min–max | Intercepted min–max |
|---|---:|---:|---:|---:|---:|
| 10000 | 72.3 ns | 1049.8 ns | 14.51× | 70.4–158.0 ns | 1016.2–1327.1 ns |
| 20000 | 72.3 ns | 1031.9 ns | 14.28× | 71.1–80.7 ns | 1019.7–1058.1 ns |
| 100000 | 87.5 ns | 1135.1 ns | 12.97× | 71.8–110.5 ns | 1031.8–1171.1 ns |

A separate earlier 10000-iteration run showed control 74.6 ns, interception 1064.1 ns, ratio 14.27×. The spread demonstrates environment variability even with pinning.

The 20000-iteration run logged **141000 traps** including 1000 warmup and all seven timed batches; SIGSYS type and syscall number checks passed. The correctness test also passed when the launcher was compiled with Clang AddressSanitizer and UndefinedBehaviorSanitizer (1000 iterations, not a comparable performance measurement).

## Interpretation: a measured tradeoff, not architecture verdict

For this kernel and probe, **signal-based interception of a raw Linux getpid** cost approximately one microsecond in median batch-average latency, versus tens of nanoseconds when allowed normally. This establishes nontrivial per-intercept cost and supports keeping SUD interception off hot paths when correctness permits.

However:
- Real Windows binaries usually invoke Windows DLL exports rather than issuing arbitrary NT syscalls directly in their own code; the mix and frequency depend on the binary and runtime. The cost of LinuxNTAlpha's actual export/API implementation, ntdll syscall stubs, broker, and guest-to-host transitions has **not** been measured.
- SUD may be useful for raw NT syscall interception even if slower than a direct in-process ABI stub. An architecture may use both mechanisms.
- The kernel context and signal semantics differ substantially from a genuine Windows kernel transition; this experiment is **not** a benchmark of Windows OS syscall execution.
- The ratio is not a whole-application slowdown, and no assertion about gaming performance, frame-time percentiles, 1% lows, power or kernel architecture follows from it.
- The container host is not controlled hardware. Results must be repeated across kernel versions, CPU classes and representative loads, with uncertainties and controls.

## Next experiments

1. Compare import-level direct calls, vDSO or native Linux syscalls and SUD-trapped calls under matched source and measurement, with verified compiler output and syscall rates.
2. Build a PE64 fixture that emits a raw *Windows-origin* syscall and confirm safe diversion without executing the same number through Linux; include selector rearming and multi-thread signal correctness.
3. Characterize mean, robust per-call latency distribution and p95/p99 with low-overhead sampling and independent calibrations.
4. Re-run on controlled bare metal with pinned frequency/performance counters where available.
5. Quantify actual application raw-NT-syscall rate before estimating total CPU/latency impact. No aggregate model should be presented as a result without this input.

**Decision:** keep SUD as a candidate interception mechanism with a measurable signal-path cost. Continue to prioritize ABI-level calls where they preserve semantics, but do not eliminate SUD or select a kernel fork from this isolated benchmark.
