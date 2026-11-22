# R234 — Raw NT syscall interception: transition-state proof obligations

Date: 2026-11-22. Status: **design review using existing primary-source citations and repository experiment records**. No new kernel run, PE execution, Windows differential test, or benchmark. Primary URLs below were not freshly retrieved during this run.

## Scope and nonduplication

[R16](R16_SYSCALL_USER_DISPATCH_PROTOTYPE.md) establishes a one-trap Linux-compiled fixture; [R26](R26_PE64_RAW_SYSCALL_SUD.md) establishes a one-trap Windows-target PE64 fixture; [R22](R22_SYSCALL_DISPATCH_LATENCY.md) reports exploratory interception overhead. This note defines the **missing multi-transition correctness contract** before the next implementation: distinguish a guest instruction's origin from the host helper's privilege, and prove interception stays armed over repeated guest/runtime crossings. It does not reclassify those existing experiments as general NT support.

## VERIFIED — primary interfaces and existing experiment observations

- Linux [Syscall User Dispatch](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html) specifies per-thread dispatch controlled by a permitted instruction-address region and/or user-space selector. Its stated purpose is compatibility; **it is not a security sandbox**. SIGSYS handling carries signal-delivery cost. Exact selector/mode and allowed-region semantics must be checked against the running kernel before relying on them.
- Linux [prctl UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/linux/prctl.h) defines PR_SET_SYSCALL_USER_DISPATCH and its mode constants; installed header availability alone does not prove runtime support. [R16](R16_SYSCALL_USER_DISPATCH_PROTOTYPE.md) records the observed legacy/new-mode naming issue.
- Linux [sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html) and [signal-safety(7)](https://man7.org/linux/man-pages/man7/signal-safety.7.html) constrain handler execution and signal disposition. A general dispatcher must not casually invoke non-async-signal-safe allocators, locks or runtime services from SIGSYS.
- Microsoft [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention) describes guest-facing ABI obligations; an intercepted raw syscall is not automatically an ordinary C ABI call. [R26](R26_PE64_RAW_SYSCALL_SUD.md) deliberately used number 39 to collide with Linux getpid, **not** a verified Windows Nt* service number.
- **Observed only in repository experiments:** R26 records five successful one-trap PE fixture iterations with a synthetic return value. R16 records GCC -O0/-O2/-O3 one-trap results. Neither record proves selector rearming after a signal return, callback nesting, thread isolation, arbitrary NTSTATUS mapping or hostile-binary containment.

## HYPOTHESIS — two distinct mechanisms, not interchangeable

**A. Selector-driven trapping:** mark the thread BLOCK during guest execution and ALLOW for host Linux syscalls. The current fixture sets ALLOW inside SIGSYS to permit return from the signal. **Critical unresolved property:** after sigreturn, a second guest syscall may run with ALLOW unless the runtime explicitly rearms at a safe point. A handler that merely edits saved RAX and returns is insufficient evidence of persistent interception.

**B. Instruction-pointer-region dispatch:** configure a narrow trusted host syscall instruction region and trap calls issued elsewhere, subject to the kernel's exact dispatch mode and executable layout. This may avoid a guest-visible selector rearm gap, but a shared executable region, JIT-generated host stubs, relocated code or host syscalls outside that region can invalidate the premise. Treat executable mappings and code-generation paths as part of the trusted computing base.

Neither mechanism prevents guest code from calling arbitrary host functions, changing process memory, or exploiting the host. Seccomp/namespace/privilege separation remains a separate containment requirement.

## Proposed state machine (NOT IMPLEMENTED)

Represent each host thread with a state owned by the supervisor, not inferred from a guest-writable TEB field:

| State | Allowed operation | Exit condition |
| --- | --- | --- |
| HOST_SETUP | Install alternate signal stack/handler and probe SUD support; initialize guest identity | enter GUEST_ARMED only after all setup succeeds |
| GUEST_ARMED | Execute guest PE instructions with NT-origin syscall redirection active | trap or explicit ABI bridge entry |
| TRAP_CAPTURE | Capture signal metadata, instruction origin, registers and thread identity with signal-safe operations | hand off to a verified host dispatcher without losing interception policy |
| HOST_DISPATCH | Validate service identity/arguments and call Linux services only from trusted host code | stage guest-visible result and restore guest context |
| GUEST_RESUME | Re-establish interception **before** any guest syscall instruction can execute | return to GUEST_ARMED |
| TERMINATING | Disable interception safely, restore signal state, reclaim per-thread resources | thread exit |

The state machine is a **candidate contract**, not a claim that SIGSYS can implement all transitions as ordinary function calls. In particular, a signal handler cannot safely block on arbitrary locks or perform complex object-manager work; any out-of-handler handoff must preserve full interrupted context and prevent unintended guest execution.

## Safety invariants to test before extending service coverage

1. **No unintended Linux entry:** every guest-origin raw syscall instruction must either be intercepted or cause a deliberate fatal unsupported result; no number collision may silently reach Linux.
2. **Rearm before resume:** after each synthetic NT service result, interception is active at the first resumed guest instruction. Test two or more consecutive guest syscalls, not merely a single trap.
3. **Host syscall liveness:** Linux syscalls made by trusted runtime code must not recursively enter SIGSYS, including signal return and termination paths.
4. **Thread locality:** selector and service state belong to the correct host thread; a second thread cannot inherit a stale ALLOW window or overwrite another thread's captured context.
5. **Signal integrity:** unexpected SIGSYS codes, external signals, interrupted syscalls, nested signals and faults have deterministic handling. No lock-taking or non-signal-safe calls in the handler.
6. **Context fidelity:** preserve guest-visible nonvolatile integer/vector state, stack alignment, FS/GS state and exception/unwind obligations across the trap path; record any deliberately modified return registers.
7. **Authorization:** service dispatch must check guest handle/object authority independently of a Linux fd or syscall number. SUD cannot be the authorization layer.
8. **Unsupported kernel behavior:** feature probe failure produces SKIP/unsupported, never silent direct guest execution.

## OPEN — minimal falsifiable next experiment

Build a separate **synthetic PE64 fixture** that issues three raw syscall instructions with deliberately Linux-colliding numbers, separated by imported MS-ABI host calls. Require exactly three ordered traps, three independently synthesized results, no host Linux syscall completion from those guest instructions, and an ordinary Linux syscall executed by the host bridge between traps. Exercise two threads, repeated runs and signal interruption as separate gates; do **not** fold all scenarios into one ambiguous PASS.

Compare selector-only and address-region approaches with identical fixture behavior, explicit allowed-region address bounds, disassembly of syscall instruction sites, captured signal metadata, and recorded kernel/compiler/linker versions. A result is FAIL if the second or third guest syscall executes in Linux, even when the PE returns the expected arithmetic value. Include a timeout and safe subprocess isolation; the fixture is trusted test code, not a sandboxed untrusted application.

Only after correctness gates pass should R22's exploratory cost measurements be repeated with controlled syscall rates, p50/p99, CPU pinning and direct-bridge comparisons. Do not infer frame-time or gaming performance from trap-only latency.

## Decision impact

**OPEN:** selector-only SUD should not be promoted to a general NT syscall strategy on R26's one-trap evidence. The next milestone is proof of continuous interception across transitions; if that fails, evaluate trusted instruction-address dispatch, explicit ABI stubs, or a different host boundary before kernel modifications. No architecture selection or performance claim follows from this document.
