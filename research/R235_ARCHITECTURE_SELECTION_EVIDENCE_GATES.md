# R235 — Architecture selection: evidence thresholds and falsification gates

Date: 2026-11-22. Status: **research synthesis / proposed decision protocol**, not an architecture decision. No new experiment, kernel execution, Windows differential measurement, or benchmark was performed. Source URLs below are primary specifications already cited in this repository; they were **not freshly fetched in this run**.

## Question and scope

ADR-003 in [DECISIONS](DECISIONS.md) remains OPEN between user-space NT services, targeted Linux kernel extensions, and deeper kernel integration. The project's native-PE successes ([R19](R19_PE64_EXECUTION_EXPERIMENT.md), [R20](R20_PE64_IAT_BRIDGE.md), [R25](R25_PE64_GS_TIB_EXECUTION.md), [R26](R26_PE64_RAW_SYSCALL_SUD.md), [R28](R28_NTDLL_RTL_COMPARE_MEMORY.md)) demonstrate isolated mechanisms, **not** an NT process model. [R232](R232_GUEST_PROCESS_SUPERVISOR_FAILURE_BOUNDARY.md), [R233](R233_NT_PROCESS_ISOLATION_CLONE_TOPOLOGY.md), and [R234](R234_SUD_REARM_TRANSITION_INVARIANTS.md) establish recent boundary questions. This note adds a **cross-cutting decision protocol**: the minimum evidence needed before architecture selection and the specific results that would disqualify a candidate.

## VERIFIED — relevant upstream contracts (documentation evidence, not measured implementation)

1. Linux [Syscall User Dispatch](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html) is designed for compatibility, **not security isolation**. A working SIGSYS fixture cannot prove a secure guest boundary. The Linux [seccomp user-notification documentation](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html) also explicitly warns against treating user notification as a general security-policy mechanism; see [R90](R90_GUEST_HOST_SYSCALL_PROVENANCE.md), [R129](R129_SECCOMP_USER_NOTIFICATION_BROKER_GATES.md), and [R184](R184_SECCOMP_USER_NOTIFICATION_NT_BROKER_GATES.md).
2. Linux [clone(2)](https://man7.org/linux/man-pages/man2/clone.2.html), [namespaces(7)](https://man7.org/linux/man-pages/man7/namespaces.7.html), and [pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html) provide host process and identity primitives. They do not, by themselves, implement Windows process objects, handle tables, or security tokens; see R232/R233.
3. Microsoft's [CreateProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw), [DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle), and [WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject) document independently observable guest-facing creation, handle and wait behavior. Host PIDs and file descriptors cannot simply be exposed as substitutes.
4. Linux [io_uring](https://man7.org/linux/man-pages/man7/io_uring.7.html) is a host asynchronous I/O interface, while Microsoft's [I/O completion ports](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports) define guest completion queues and worker admission semantics. See [R15](R15_IOCP_IO_URING.md), [R191](R191_IOCP_CONCURRENCY_WORKER_ADMISSION_GATES.md), and [R195](R195_ASYNC_REQUEST_OWNERSHIP_COMPLETION_GATES.md).
5. Existing measured evidence is narrowly bounded: [R22](R22_SYSCALL_DISPATCH_LATENCY.md) records a single-container exploratory SUD trap comparison, not whole-application latency, tail behavior, or an architecture benchmark. R26 is a one-trap synthetic Windows-target fixture, and R234 identifies an unproven rearm invariant.

## HYPOTHESIS — candidate boundaries to compare, without selecting one

| Candidate | Guest semantic owner | Host mechanism | Principal falsification risk |
| --- | --- | --- | --- |
| A: user-space NT authority | Explicit guest process/object/handle/token and request state in a supervisor or service | Linux processes, mappings, fds, signals, IPC, optional ntsync/io_uring | Untrusted guest shares an address space with privileged authority; failure/restart breaks guest identity or in-flight completions |
| B: A plus minimal kernel extension | Same explicit guest authority unless a measured kernel boundary requires relocation | Carefully scoped kernel UAPI for a proven bottleneck | ABI maintenance, kernel security exposure, Linux driver compatibility and upstream sustainability exceed demonstrated benefit |
| C: deep NT integration into Linux kernel | Substantial guest semantics moved into kernel | Extensive kernel hooks or subsystem changes | Kernel coupling and attack surface grow before correctness and compatibility are demonstrated |

These are evaluation models, **not verified implementation architectures**. A user-space service does not require all guest threads to share its address space. A kernel extension is not automatically faster end-to-end; queueing, copying, wakeups, and lock contention must be measured.

## Proposed hard gates before ADR-003 can close

**G1 — Identity/isolation:** two guest processes have independent address spaces and guest handle tables; duplicating an authorized handle preserves object identity and rights, while unauthorized duplication fails. Host process/worker termination does not silently transfer authority. Test stale PID/fd reuse, supervisor crash and deterministic teardown. Cross-reference R224/R232/R233.

**G2 — Interception completeness:** a multi-syscall PE fixture demonstrates no guest-origin raw syscall falls through to Linux across repeated traps, host callbacks, signals and threads. Require the ordered-trap proof in R234 before extrapolating from R26. A failure is an architecture blocker for that interception path, not a reason to waive the test.

**G3 — Asynchronous ownership:** cancellation, handle close, process termination and completion delivery have one guest-visible terminal result per request, with no stale-generation completion reaching a recycled handle. Compare Windows reference traces with Linux backend traces; cross-reference R142/R195/R228.

**G4 — Security:** untrusted PE code cannot modify supervisor policy, service state or host credentials by writing guest-addressable memory, calling arbitrary host functions or issuing a Linux syscall. Run adversarial negative tests under explicit Linux sandbox configuration; do not label SUD, seccomp user notification or PE parse-only mode a sandbox.

**G5 — Reproducible performance:** only after G1–G4, compare A against a Windows reference and Linux-native control on equivalent hardware, with recorded kernel versions, workload definitions, p50/p95/p99, CPU usage, RSS, warm/cold start, and confidence intervals. Separate syscall-trap microbenchmarks from app frame-time claims. Kernel modification B/C needs a measured, repeatable bottleneck that cannot be acceptably resolved in A.

**G6 — Lifecycle and maintenance:** publish a versioned guest/host boundary, crash recovery policy, kernel/UAPI compatibility matrix, licenses and upstream maintenance burden. Reject any design that requires undocumented permanent host ABI assumptions.

## Decision rule (proposed, not accepted)

Keep ADR-003 OPEN until G1–G4 are experimentally demonstrated for at least one candidate and G5/G6 have decision-quality evidence. Prefer the **least invasive** architecture satisfying those gates, but do not treat this as a precommitted selection of A. Escalate to B only with a measured deficiency and a narrowly specified extension; C requires a separate, stronger justification including maintenance and security cost.

## OPEN — next falsifiable work

Implement the R234 repeated-trap fixture **before** broadening the NT service table; independently prototype two-guest-process identity/handle-lifetime tests and a supervisor-kill fault injection, without claiming Windows conformance until Windows reference observations exist. Record kernel, compiler, source revision, exact command, raw results, and PASS/FAIL/SKIP separately. No tests described here have been run.

## Evidence limits

The cited Linux and Microsoft pages specify interfaces, not that LinuxNTAlpha implements them. Links are stable primary documentation locations, but this synthesis does not establish current upstream revision-specific behavior. Existing repository experiments must be inspected at their own recorded environments before being reused as performance evidence.
