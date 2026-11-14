# R80 — SUD selector rearming and the untrusted-guest boundary

Date: 2026-11-14. Status: **PRIMARY-SOURCE REVIEW; DESIGN HYPOTHESES ONLY**. No new executable test or Windows compatibility claim.

## Why this is a separate gate

[R26](R26_PE64_RAW_SYSCALL_SUD.md) demonstrates exactly one synthetic syscall in a Windows-target PE image. Its SIGSYS handler switches the selector to ALLOW for signal return. That is sufficient for its trusted, one-shot fixture but does **not** demonstrate that a second syscall, a guest callback, a thread migration, or a malicious guest cannot reach a native Linux syscall path. This note specifies the missing *transition invariant* and failure-oriented acceptance tests rather than repeating R26.

## Verified upstream contracts (not local measurements)

1. Linux Syscall User Dispatch (SUD) is **per-thread**. Its user-space selector is a byte that selects ALLOW or BLOCK. In the traditional exclusive mode, syscalls from a designated address interval run directly regardless of the selector; syscalls elsewhere are controlled by it. The kernel documentation also describes an inclusive mode in newer documentation. **Feature availability and header constants must be checked against the actual target kernel**, rather than assuming documentation from another version matches the running host.
2. A trapped syscall produces thread-directed SIGSYS; the kernel does **not** execute that syscall. The saved instruction pointer is already past the syscall instruction, and a handler can modify the saved return register in ucontext. Signal-return machinery must itself be able to execute; kernel documentation discusses allowing the signal trampoline or native dispatch region.
3. The kernel documentation explicitly states SUD **is not a security sandbox**: guest code can jump to an allowed dispatcher or modify a writable selector. It recommends seccomp for security isolation. Fork/exec reset SUD configuration. The man-page interface also documents reset across clone; the exact thread-creation path must be tested.
4. Linux's kernel SUD documentation says intercepted non-native syscalls are not instrumentable by ptrace syscall tracing or syscall tracepoints as ordinary kernel syscalls. Consequently, observing an absent Linux syscall trace is not sufficient proof of correct guest dispatch.

Primary sources, reviewed 2026-11-14:
- Linux kernel SUD documentation: https://docs.kernel.org/admin-guide/syscall-user-dispatch.html
- Versioned Linux v6.18 documentation: https://kernel.org/doc/html/v6.18/admin-guide/syscall-user-dispatch.html
- Linux man-pages PR_SET_SYSCALL_USER_DISPATCH: https://www.man7.org/linux/man-pages/man2/PR_SET_SYSCALL_USER_DISPATCH.2const.html
- Linux UAPI constants (revision must be pinned for implementation): https://github.com/torvalds/linux/blob/master/include/uapi/linux/prctl.h

## Proposed invariant (HYPOTHESIS / design requirement)

For each guest thread, **every transition into guest-controlled machine code must have interception armed**, and every native Linux syscall issued by trusted runtime code must use an explicitly justified host path. A guest return address, callback, exception handler, imported function, or asynchronous signal must not accidentally inherit an ALLOW window. This is a desired property, **not** a property established by SUD.

Suggested state model for tests, not production code:

- HOST: trusted runtime executes; selector policy permits required host syscalls.
- ENTER_GUEST: save runtime TLS/GS and other ABI state; arm guest dispatch **before** the first guest instruction.
- GUEST: intercept raw syscalls; direct API imports enter vetted host bridges.
- TRAP: verify SIGSYS provenance and syscall metadata, capture guest register context, produce an NT-specific result, then restore guest interception **before resuming guest instructions**.
- RETURN_HOST: regain a trusted return point, disarm only after guest execution has ended; restore runtime thread state.
- FAULT/EXIT: restore signal dispositions and dispatch configuration without accidentally resuming guest code in HOST mode.

**Critical nuance:** A signal handler that writes ALLOW and simply returns via a permitted signal trampoline may resume the guest with ALLOW. That makes a repeated syscall unsafe even if the first trap was correctly emulated. A design must explicitly prove rearming before guest continuation, rather than assuming the signal handler's lifetime does so.

## Proposed falsification matrix (NOT RUN)

| Case | Required observation | Failure signal |
| --- | --- | --- |
| Two consecutive synthetic PE syscalls | Two SIGSYS interceptions, two synthetic returns, no Linux syscall side effect | Second syscall executes in Linux because selector remained ALLOW |
| Guest -> imported host bridge -> guest -> raw syscall | Bridge's native syscall succeeds; subsequent guest syscall is trapped | Selector leaked across ABI bridge |
| Guest callback invoked from host | Guest syscall trapped regardless of callback nesting | Callback enters with HOST selector |
| Async signal during guest/host transition | Return resumes with the correct personality | Selector overwritten or guest resumed unarmed |
| Multiple pthread guest threads | Each thread has independently installed SUD, handler state and selector | Only parent thread protected; shared selector races |
| Guest intentionally writes selector or jumps to allowed host dispatcher | Demonstrate SUD bypass in an isolated sacrificial process | Incorrect assertion that SUD provides containment |
| fork/clone/exec boundary | Explicitly reinitialize or reject unsupported child paths | Guest executes in child with dispatch disabled |
| Unsupported kernel/header feature | Deterministic SKIP, never silently permit guest syscall | Fallback executes guest syscall in Linux |

All tests should use disposable subprocesses with timeout, check the actual Linux syscall side effect where safely observable, and report kernel version, UAPI mode, compiler, signal handler configuration, and PASS/SKIP/FAIL. A malicious bypass probe must be isolated and **must not** be used to infer containment.

## Architectural implication

**VERIFIED:** SUD is a useful compatibility dispatch mechanism, not a security boundary. **HYPOTHESIS:** a narrow trusted PE execution harness can meet the transition invariant through carefully constrained host stubs and signal-return handling. **OPEN:** whether a general-purpose runtime can maintain that invariant under arbitrary callbacks, exceptions, thread creation and hostile guest code without an independent isolation boundary. Do not choose a kernel fork or claim safe arbitrary-PE execution until these gates are measured.

Related: [R16](R16_SYSCALL_USER_DISPATCH_PROTOTYPE.md), [R22](R22_SYSCALL_DISPATCH_LATENCY.md), [R26](R26_PE64_RAW_SYSCALL_SUD.md), [R69](R69_DYNAMIC_CODE_MITIGATION_EXEC_MEMORY.md).
