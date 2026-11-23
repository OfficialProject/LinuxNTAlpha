# R258 — SUD SIGSYS delivery, alternate stacks, and handler reentrancy gates

Date reviewed: 2026-11-23. **Status: documentation synthesis; no code executed and no Windows conformance measurement.**

## Scope and relation to prior work

R16/R26 demonstrate narrow SUD interception; R22 measures an exploratory trap cost; R90/R108/R234 discuss provenance, signal ownership, and selector rearming. This note isolates a remaining **implementation safety question**: can the NT syscall interception entry path remain correct when guest code has exhausted its stack, the host is interrupted inside non-reentrant library code, or a second fault arrives during a SIGSYS handler? This is **not** a new claim that SUD provides containment.

## Verified upstream contracts

1. Linux SUD routes selected system calls through SIGSYS, with a per-thread selector and an address-range dispatch policy. The upstream documentation expressly says it is **not a security sandbox**, and requires careful treatment of the signal-return trampoline. Current kernel documentation describes both inclusive and exclusive modes; do not assume older headers expose both. [Linux SUD documentation](https://docs.kernel.org/6.19-rc7/admin-guide/syscall-user-dispatch.html).
2. Linux alternate signal stacks are installed **per thread** with sigaltstack; SA_ONSTACK selects the alternate stack for a handler. SS_AUTODISARM (Linux 4.7+) disarms the alternate stack on handler entry and restores it on return, supporting certain context-switch patterns. A thread's alternate stack is not an automatically shared process-wide exception stack. [sigaltstack(2)](https://man7.org/linux/man-pages/man2/sigaltstack.2.html).
3. Signal handlers cannot generally call arbitrary C library/runtime functions safely. In particular, a handler interrupting non-reentrant state and re-entering the same unsafe routine can corrupt state; errno must be preserved when appropriate. [signal-safety(7)](https://man7.org/linux/man-pages/man7/signal-safety.7.html).
4. A Linux alternate stack is especially relevant when ordinary stack exhaustion triggers SIGSEGV: handling that fault on the exhausted stack may itself be impossible. This does **not** establish that every SIGSYS needs a separate alternate stack, nor that an alternate stack alone fixes nested dispatch. [sigaltstack(2)](https://man7.org/linux/man-pages/man2/sigaltstack.2.html).

## Proposed design hypothesis — not an implementation decision

Keep the SIGSYS entry path bounded and minimal: save the interrupted guest register context and provenance into preallocated per-thread storage, avoid allocator/stdio/locks in the handler, and transfer complex NT service work to a separately defined safe execution context. The exact transfer mechanism is **OPEN**: returning to a controlled trampoline by validated ucontext adjustment, or an explicitly designed signal-context transition, each needs architecture-specific proof. Do not presume editing ucontext is universally safe or that arbitrary C++/Rust code can execute from the handler.

Maintain **separate state** for (a) Linux host-syscall eligibility, (b) guest NT syscall service identity and arguments, (c) active signal-handler nesting, and (d) guest/host stack ownership. The SUD selector alone is not an authorization boundary. Host-native calls needed during a transition must be routed through a known permitted dispatcher and validated for nested-signal behavior.

## Falsifiable gates before enabling arbitrary PE execution

- **G1 — per-thread setup:** prove each guest-bearing pthread has its own appropriately sized guarded alternate stack and SUD configuration; verify thread creation, teardown and unexpected signal disposition. Record kernel, libc, compiler and mode availability.
- **G2 — stack exhaustion:** invoke a controlled guest raw syscall near a guarded stack boundary, then separately generate a guest stack-overflow fault; verify signal entry uses valid storage, preserves guest context, and never recursively consumes the exhausted guest stack.
- **G3 — reentrancy:** interrupt host allocator/stdio/locking activity with a guest-origin trap under controlled instrumentation. Handler must not re-enter unsafe operations or acquire a lock held by the interrupted thread. Use subprocess isolation and watchdogs.
- **G4 — nested signals:** inject SIGSEGV/SIGSYS during entry, service dispatch, and return; define explicit fail-closed behavior for unsupported nesting. Verify no infinite SIGSYS recursion on rt_sigreturn and no accidental Linux syscall execution under a colliding guest number.
- **G5 — context fidelity:** compare all relevant GPRs, RFLAGS, FS/GS bases, stack alignment, XMM nonvolatile state and signal mask before/after a synthetic intercepted service. Treat CET/shadow-stack hosts as a separate capability gate.
- **G6 — mode/stack feature detection:** probe inclusive/exclusive SUD availability and SS_AUTODISARM support at runtime. SKIP unsupported configurations with precise reason; do not silently reinterpret a missing feature as PASS.
- **G7 — concurrency and cancellation:** run multiple threads with independent selectors/alternate stacks while host signals arrive. Check teardown races and whether handler-in-progress prevents freeing its backing storage.

## Open questions and acceptance

What exact code region contains rt_sigreturn on each supported libc/vDSO configuration? How is a handler-to-dispatch transition made without violating async-signal-safety, stack ownership, or unwind assumptions? What is the recovery policy when a nested fault occurs after guest context capture but before NTSTATUS publication? These are **OPEN**, not experimentally answered.

**Acceptance:** a documented reproducible harness must demonstrate G1–G7 (or mark unsupported cases SKIP) on pinned host configurations, and separate functional correctness from containment testing. No benchmark or universal safety claim is made here.

## Source provenance

Primary Linux kernel documentation and Linux man-pages links above, reviewed 2026-11-23. Kernel URL is a **6.19-rc7 documentation snapshot**, not evidence that the project's test host runs that kernel. No source code copied. Related local research: R16, R22, R26, R90, R108, R234, R236.
