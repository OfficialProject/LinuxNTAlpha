# R274 — SUD, seccomp and syscall observability ordering

Reviewed 2026-11-24. Source audit only; no experiment or benchmark. Extends R273's address oracle and R90's security distinction with an entry/exit instrumentation oracle for the R271 multi-trap fixture.

## Verified (Linux v6.12)

- [kernel/entry/common.c](https://github.com/torvalds/linux/blob/v6.12/kernel/entry/common.c), blob 5b6934e23c21d36a3238dc03e391eb9e3beb4cfb: syscall_trace_enter checks SUD first and returns -1 immediately when dispatch intercepts. Ptrace entry handling, seccomp, syscall-entry tracepoints and audit entry occur later. Seccomp follows ptrace to inspect tracer modifications. The syscall number is subsequently reread from registers.
- The same file's syscall_exit_work checks on_dispatch when SUD work is enabled; when set, it clears the flag and returns before audit exit, syscall-exit tracepoint and ptrace exit reporting. This is not a claim that all kernel exit work or all signal observation is suppressed.
- [kernel/entry/syscall_user_dispatch.c](https://github.com/torvalds/linux/blob/v6.12/kernel/entry/syscall_user_dispatch.c), blob 5340c5aa89e7dd33f30a3543e4f73bfc08ea5a4e: ordinary blocked dispatch sets on_dispatch, rolls back the syscall and generates SIGSYS. The invalid/unreadable selector paths differ; do not extrapolate ordinary trap exit behavior to those fatal paths.
- [Documentation/admin-guide/syscall-user-dispatch.rst](https://github.com/torvalds/linux/blob/v6.12/Documentation/admin-guide/syscall-user-dispatch.rst), blob e3cfffef5a63364624136b6bc36a0e3856e244b9: intercepted foreign syscalls are not instrumentable through ordinary ptrace syscall or syscall tracepoint interfaces; SUD is explicitly not sandboxing.

## Architectural implications (derived, not measured)

A seccomp policy cannot serve as a second-stage classifier for an already SUD-intercepted foreign syscall on this entry path. It can still constrain Linux syscalls admitted past SUD, including host bridge calls. A guest NT service must therefore be authorized by the guest dispatcher independently of host syscall filtering. A syscall trace with no matching Linux getpid entry is insufficient evidence that the guest instruction never executed: it may have been intercepted before tracing. Conversely, a trace of host bridge activity does not count NT requests.

Use separate counters for guest requests, SIGSYS ingress, guest service completion, host syscall activity and host-policy denial. Correlate with fixture sequence IDs and thread identity. Do not derive NT throughput from Linux syscall tracepoint counts. Instrumentation overhead must be measured separately from baseline timings.

## Proposed gates (all unrun)

1. In a disposable trusted fixture, configure SUD and a seccomp filter returning a distinctive errno for a harmless Linux getpid call, allowing required setup/return/exit calls. Verify a guest collision outside the allowed region reaches SYS_USER_DISPATCH and receives a synthetic result, while a host getpid admitted by SUD receives the filter errno. Record actual ordering and statuses; do not assume the filter is installed successfully.
2. Repeat without seccomp, and without SUD, as independent controls. Use a host call known to issue the actual syscall rather than a potentially cached library result.
3. Observe syscall entry/exit tracepoints if privileges and kernel configuration permit. Compare guest trap counts against host call counts, excluding setup and signal-return activity. Record missing tracing capability as SKIP, not PASS. Ptrace signal-delivery stops are distinct from syscall stops; use a separate tracing run and preserve signal delivery.
4. Repeat the R271 three-trap sequence with the host bridge between guest calls. Require exactly three guest dispatch records and the expected host-policy result. Reject unexpected signal provenance or unmatched completions; no synthetic success for unexpected traps.
5. Repeat on each candidate kernel version only after auditing its entry path and configuration. Record kernel build, source revision, filter, binary hash, tracing configuration and ordered results. No extrapolation to other architectures or newer SUD modes.

## Decision

OPEN. Maintain separate guest authorization and host containment policies. Treat syscall tracepoints as host-path telemetry, not an NT service accounting oracle. This narrows the acceptance criteria for the existing multi-trap prototype; it does not select an architecture or establish security or performance.
