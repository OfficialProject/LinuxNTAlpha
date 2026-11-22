# R245 — Seccomp user notification as an NT syscall-dispatch alternative

Status: DOCUMENTATION REVIEW ONLY (2026-11-22). No implementation, benchmark, Windows-reference experiment, or security validation performed.

## Question

Could Linux seccomp user notification replace or complement Syscall User Dispatch (SUD) for LinuxNTAlpha's guest NT syscall entry, especially when a separate supervisor owns NT objects? This is a **candidate architecture**, not an adopted decision. See R26, R90, R232, R234–R236 for existing SUD and supervisor research.

## VERIFIED — Linux primary contracts

1. Seccomp-BPF evaluates syscall number, architecture and register argument values; its filter **cannot dereference guest pointers**. The kernel documentation explicitly says seccomp filtering is **not itself a sandbox**. Architecture must be checked because syscall numbers overlap across ABIs. [K1]
2. A filter returning `SECCOMP_RET_USER_NOTIF`, installed with `SECCOMP_FILTER_FLAG_NEW_LISTENER`, can suspend the calling thread and deliver a `seccomp_notif` to a supervisor through a listener FD. A supervisor can respond with a synthetic return or request continuation of the original Linux syscall. The listener belongs to a filter rather than a single task; forked tasks can share its notification stream. [K1, M1]
3. Notification sizes must be queried with `SECCOMP_GET_NOTIF_SIZES`. Notifications include a per-filter ID, triggering TID and `seccomp_data`; `SECCOMP_IOCTL_NOTIF_ID_VALID` exists to address termination/TID-reuse races. Reading pointer arguments from target memory has separate time-of-check/time-of-use hazards. [K1, M1]
4. `SECCOMP_IOCTL_NOTIF_ADDFD` can install a Linux FD into the target, optionally with an atomic response. This does **not** turn that FD into an NT handle with NT access masks, lifetime, duplication or object-type semantics. The latter is an architectural inference from the Linux FD contract and the NT handle research, not a measured incompatibility. [K1; R67, R224]
5. Notifications can be aborted by signals. Current upstream documentation additionally describes opt-in `SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV` and `SECCOMP_FILTER_FLAG_RESTART_BEFORE_RECV`; these change interruption/restart behavior, and the latter may delay receipt indefinitely under repeated signals. **Do not assume these flags are present on a deployment kernel merely because the latest documentation describes them.** Probe actual kernel support and pin its source version. [K1]
6. The Linux man-pages description explicitly warns that seccomp user notification is **not intended as a security-policy enforcement mechanism**. The kernel seccomp documentation separately warns that allowing ptrace with seccomp filters can undermine confinement. [M1, K1]

## HYPOTHESES (not verified)

- A dedicated supervisor could simplify NT service ownership and crash isolation relative to in-process SUD/SIGSYS handling.
- Every trapped NT call would incur cross-thread/process notification and response costs; these may be prohibitive for hot-path services compared with an in-process dispatcher. **No numerical latency or application slowdown is asserted.** R22's SUD microbenchmark cannot be reused as a seccomp-notification measurement.
- Seccomp's filter sees the **Linux** syscall ABI and metadata, not an authenticated NT service identity. An NT-to-Linux syscall-number collision therefore requires explicit guest-origin classification; matching syscall numbers alone is unsafe. A broker's synthetic response is not proof that native Linux execution was prevented along every alternate entry path.

## Architecture gates before considering adoption

**G1 — Dispatch identity:** Pin x86-64 syscall entry ABI and architecture checks; classify guest code versus trusted host runtime without relying solely on the syscall number. Test direct guest `syscall`, alternate call sites, signal handlers, callbacks, and mixed guest/host threads. Fail closed on unclassified entries.

**G2 — Notification lifecycle:** Test one thread, multiple threads, fork/exec, thread exit, supervisor exit, signals before/after notification receipt, cancellation and TID reuse. Ensure no duplicate NT side effects after interruption/restart. Record which kernel supports which optional flags.

**G3 — Guest memory:** Snapshot and validate pointed-to data before decisions, enforce lengths and access rights, and test mutation/unmapping concurrently. An ID-validity check alone cannot make mutable guest buffers atomic.

**G4 — NT semantics:** Return genuine NTSTATUS through an independently specified guest ABI boundary, rather than leaking Linux errno; preserve NT handles, access checks, APC/alertable wait behavior and per-process object identity in guest-owned state.

**G5 — Containment:** Treat seccomp mediation and guest NT authorization as separate security layers. Test disallowed Linux syscall paths, `CONTINUE` misuse, ptrace interactions, privileged supervisor FD injection, and hostile guests. Never equate a notification mechanism with a sandbox.

**G6 — Performance:** On pinned hardware/kernel, compare native Linux syscall, in-process SUD, seccomp user notification with same-process supervisor, and separate-process supervisor. Include medians, tails, wakeups, CPU utilization, signal interruption and concurrent load; use the R5 measurement protocol. No measurements have been run here.

## Decision

**OPEN.** Do not replace the existing SUD prototype or select a supervisor architecture based on this review. Seccomp user notification is a useful *experimental comparator* and potentially a privileged-operation mediation channel, but its lifecycle, syscall provenance, pointer races, guest semantic mapping and overhead must pass G1–G6. Existing R26/SUD success remains a narrow fixture result.

## Sources (retrieved 2026-11-22)

- [K1] Linux kernel documentation, **Seccomp BPF**, https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html (rolling documentation; pin a kernel tag before implementation).
- [M1] Linux man-pages, **seccomp_unotify(2)**, https://man7.org/linux/man-pages/man2/seccomp_unotify.2.html (user-space API description; cross-check installed kernel behavior).
- Local research cross-references: R5, R22, R26, R67, R90, R224, R232, R234–R236.

Evidence limitation: web documentation inspected; repository experiments not executed or modified. No claim about the exact minimum Linux release of the newer restart flag is made.
