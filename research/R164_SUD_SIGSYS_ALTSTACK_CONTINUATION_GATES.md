# R164 — SUD SIGSYS alternate-stack ownership and suspended signal frames

Date: 2026-11-18. Status: **PRIMARY-SOURCE REVIEW ONLY**. No experiment, Windows reference run, or benchmark performed.

## Distinct architectural gate

R80/R89/R90 address Syscall User Dispatch (SUD) selector transitions and provenance. This note isolates a different failure mode: **the lifetime of the Linux signal frame and alternate signal stack when an NT service dispatcher suspends or switches away from a SIGSYS handler**. A one-shot handler that edits ucontext and returns is not evidence that an asynchronous, blocking or nested NT service can safely retain its signal frame.

## Verified from directly reviewed primary documentation

1. Linux SUD delivers SIGSYS for intercepted guest-origin syscall instructions; its documentation explains the need for an allowed signal-return trampoline and explicitly warns that SUD is not a sandbox. SUD does not define an NT dispatcher, a continuation scheduler or a safe signal-frame suspension protocol. [Linux kernel SUD documentation, v6.0](https://www.kernel.org/doc/html/v6.0/admin-guide/syscall-user-dispatch.html).
2. Linux **sigaltstack(2)** establishes an alternate signal stack for the *calling thread*. A handler uses it when installed with **SA_ONSTACK**. An ordinary active alternate stack cannot be replaced while executing on it. Linux-specific **SS_AUTODISARM** clears the alternate-stack setting on handler entry and restores it when the handler returns; the documented motivation is safe switching away from a handler with **swapcontext(3)**, avoiding corruption of a suspended handler by a subsequent signal. The flag can fail with EINVAL on unsupported kernels. [Linux man-pages sigaltstack(2)](https://man7.org/linux/man-pages/man2/sigaltstack.2.html).
3. Linux **sigreturn(2)** restores signal mask, stack state and saved processor context from the signal frame; the runtime must not assume that simply moving to another stack has completed that restoration. [Linux man-pages sigreturn(2)](https://man7.org/linux/man-pages/man2/rt_sigreturn.2.html).
4. POSIX signal-handler safety rules prohibit assuming that allocation, arbitrary locking, stdio or application callbacks are safe in an asynchronous handler. A function's signal safety does not itself guarantee application data-structure consistency. [Linux man-pages signal-safety(7)](https://man7.org/linux/man-pages/man7/signal-safety.7.html); [POSIX sigaction application usage](https://www.man7.org/linux/man-pages/man3/sigaction.3p.html).
5. Linux signal disposition is process-wide while the alternate signal stack is thread-specific. A per-thread guest dispatch installation therefore needs per-thread stack allocation, guard/ownership policy and teardown, not one global stack. [Linux signal(7)](https://www.man7.org/linux/man-pages/man7/signal.7.html); [sigaltstack(2)](https://man7.org/linux/man-pages/man2/sigaltstack.2.html).

These are documented Linux interfaces, **not** measurements of LinuxNTAlpha and not proof that SS_AUTODISARM solves guest selector rearming.

## Design hypotheses, explicitly unverified

**H1 — Minimal handler plus deferred service work.** The SIGSYS handler should capture a bounded, signal-safe dispatch record and arrange a controlled transition to trusted runtime code rather than call general NT services, allocate or wait while a signal frame remains live. The exact transfer and resumption mechanism is **OPEN**: merely returning to guest with SUD selector ALLOW is unsafe (R80).

**H2 — Alternate stack lifetime is tied to suspended frames.** A thread cannot free or recycle its alternate stack while any handler frame can resume there. SS_AUTODISARM permits a particular switch-away pattern but does not free the runtime from owning suspended frame memory, context, signal mask, and guest/host TLS state.

**H3 — Blocking NT services require a continuation protocol.** If a guest syscall may block, dispatching it directly inside SIGSYS risks reentrancy, stack retention and signal-mask problems. A continuation design must account for cancellation, timeout, asynchronous signals, nested guest callbacks and thread termination. Neither a simple swapcontext nor SS_AUTODISARM is by itself a verified solution.

**H4 — Handler entry must be bounded under stack exhaustion.** A guest stack overflow or malformed guest stack must not be able to force an unbounded SIGSYS handler stack consumption. A dedicated guarded alternate stack and strict handler work budget are candidate safeguards, not a complete containment boundary.

## Falsifiable tests to implement later — NOT RUN

1. **Capability gate:** on the actual kernel, install a guarded alternate stack with SS_AUTODISARM; distinguish success from EINVAL/other errors; record kernel release, libc, architecture and SIGSTKSZ/MINSIGSTKSZ policy. Do not infer support from headers.
2. **Signal-frame preservation:** in a disposable process, suspend a handler through a deliberately controlled context switch; deliver a second signal and verify both original and nested contexts resume without stack overwrite, comparing supported SS_AUTODISARM against a safe baseline. Never deliberately corrupt a live stack in the main test runner.
3. **Repeated SUD traps:** run two or more synthetic PE raw syscalls with a host service boundary between them; require exactly matching trap counts, no accidental Linux syscall execution, restored signal masks, and guest selector BLOCK before each guest continuation.
4. **Blocking and cancellation:** simulate an NT wait that is canceled, times out, or is interrupted by another signal while dispatch is pending; verify one terminal completion, no dangling frame or stack allocation, and no handler-held runtime lock.
5. **Per-thread lifecycle:** create multiple guest threads with independent alternate stacks, force interleaved traps and signal delivery, then tear down one thread while others remain active; verify no shared stack or selector reuse.
6. **Guard-page fault:** under a bounded child-process timeout, provoke a controlled guest-stack exhaustion and verify the runtime can classify or fail safely without recursively exhausting the signal handler stack. This is a proposed negative control, not an observed result.

## Decision gate

Do **not** choose a general blocking NT service implementation inside SIGSYS based on R26's single successful trap. Before that architectural decision, require a demonstrated safe signal-frame/continuation lifetime model, selector rearming, per-thread alternate stack management and failure cleanup. If a design cannot satisfy these gates without fragile context manipulation, compare an explicit guest-to-host trampoline architecture before optimizing dispatch latency.

## Source scope and limitations

Primary Linux kernel and Linux man-pages/POSIX documentation reviewed 2026-11-18. The SUD source is explicitly versioned **v6.0**; the other linked manual pages are living documentation, not pinned kernel implementation revisions. Kernel-specific behavior and target-version support remain to be checked against a pinned Linux tag and an executed probe. No external source code was copied.

Related: [R26](R26_PE64_RAW_SYSCALL_SUD.md), [R80](R80_SUD_SELECTOR_REARMING_SECURITY_GATES.md), [R89](R89_SUD_TRANSITION_STATE_INVARIANTS.md), [R90](R90_GUEST_HOST_SYSCALL_PROVENANCE.md), [R108](R108_SIGNAL_OWNERSHIP_NT_EXCEPTION_INGRESS.md).
