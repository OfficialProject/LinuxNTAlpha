# R272 — host-side SUD handler source audit: selector lifetime and unexpected-trap disposition

Reviewed: 2026-11-23. **Evidence: direct inspection of repository C source plus upstream Linux documentation. No compilation, runtime test, kernel trace, or Windows comparison was performed.**

## Relationship to existing work

[R271](R271_PE64_SUD_FIXTURE_SOURCE_AUDIT.md) inspected the **guest** fixture and specified a three-trap acceptance test. [R234](R234_SUD_REARM_TRANSITION_INVARIANTS.md) specifies transition invariants, and [R258](R258_SUD_SIGNAL_ALTSTACK_REENTRANCY_GATES.md) covers alternate stacks. This note instead audits the **host implementation** in [`experiments/pe64-image/pe64_probe.c`](../experiments/pe64-image/pe64_probe.c), specifically `on_sud_sigsys` and `call_entry_sud`. It does not repeat a general SUD architecture overview.

## VERIFIED — current source behavior

1. `sud_selector` is a single static `volatile unsigned char`, initialized to `SYSCALL_DISPATCH_FILTER_ALLOW`. `sud_traps` and `sud_unexpected` are static `volatile sig_atomic_t` counters. The current fixture is a single-thread demonstration; these objects do **not** establish a per-thread dispatcher design.
2. `call_entry_sud` installs a process-wide SIGSYS action with `SA_SIGINFO`, enables SUD with `prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, 0UL, 0UL, &sud_selector)`, sets the selector to BLOCK, and calls the Windows-ABI entry function.
3. In `on_sud_sigsys`, a mismatch of signal number, `si_code`, or `si_syscall` merely increments `sud_unexpected`. The handler **still** increments `sud_traps`, sets the selector to ALLOW, and writes `0x12345` to the saved `REG_RAX`. Thus the source has no early rejection branch for an unexpected trap. The final counter check rejects the run later, but the handler has already resumed the interrupted context with a synthetic return.
4. The handler sets ALLOW to permit signal return. The only later explicit BLOCK assignment occurs **before** `fn()`, not after the handler returns. `call_entry_sud` sets ALLOW again after `fn()` returns and disables SUD. **There is no visible selector rearm between successive guest syscalls.** This is a source-level finding, not an observed second-call escape.
5. `sigaction(SIGSYS, &old, NULL)` restores the earlier process-wide disposition after the call; `prctl` toggles SUD for the calling thread. This distinction matters if later code adds concurrent guest threads: a process-wide handler lifetime must not be inferred from one thread's SUD lifetime.
6. The test accepts exactly one counted trap and zero counted mismatches. It does not independently assert the interrupted instruction address, the guest mapping provenance, a second trap, or absence of unintended Linux syscall execution.

## VERIFIED — upstream contract boundaries

- [Linux kernel Syscall User Dispatch documentation](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html): selector and allowed-region dispatch are compatibility mechanisms, **not** a security boundary; dispatch is thread-scoped.
- [Linux sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html): signal dispositions are process-wide, while masks and delivery context have thread-specific aspects; `SA_SIGINFO` supplies `siginfo_t` and context.
- [Linux signal-safety(7)](https://man7.org/linux/man-pages/man7/signal-safety.7.html): handlers cannot assume arbitrary runtime operations are async-signal-safe.
- [Linux prctl(2)](https://man7.org/linux/man-pages/man2/prctl.2.html) and [PR_SET_SYSCALL_USER_DISPATCH(2const)](https://man7.org/linux/man-pages/man2/PR_SET_SYSCALL_USER_DISPATCH.2const.html): Linux-side configuration and selector interface; exact available dispatch modes must be probed, not inferred from current upstream docs.

The source inspection is verified against the repository default branch on the review date. Upstream links are contract references; this run did not pin live upstream source commits or validate host kernel support.

## HYPOTHESES — risks requiring execution to resolve

- **H1: second guest syscall may bypass interception.** Because ALLOW persists after the first handler, a subsequent Linux-colliding syscall in the same guest invocation may enter Linux. This follows the visible state sequence, but needs the R271 three-trap negative control to establish actual behavior on a tested kernel.
- **H2: unexpected-trap continuation may obscure failure origin.** An unexpected SIGSYS currently receives the fixture's synthetic success return before the eventual final FAIL. This could hide intermediate side effects in more complex fixtures; it does **not** imply the current one-syscall test falsely reports PASS.
- **H3: concurrency may invalidate global selector/handler assumptions.** Per-thread SUD configuration plus a shared selector address and process-wide SIGSYS disposition requires a designed ownership protocol. No multithreaded SUD test has been run.

## Required implementation/test gates — proposals, NOT completed work

1. Preserve the existing single-trap fixture and its output as a regression baseline. Build a separate repeated-trap fixture and count *ordered* SIGSYS events and instruction pointers, as specified in R271.
2. Add a distinct negative-control test for mismatched `si_code` or `si_syscall`, and require explicit rejection before synthetic result publication. Run only in a disposable child process with a watchdog; do not assume that every signal with number SIGSYS belongs to SUD.
3. Define a finite state machine with explicit guest BLOCK, host ALLOW, signal-return, and guest rearm states. Prove with disassembly and trace evidence that rearm occurs **before** the next guest-origin `syscall`, without trapping host cleanup or `rt_sigreturn`. A simple assignment after `fn()` is insufficient.
4. Separate per-thread selector/trap state from process-wide handler registration and teardown. Test concurrent threads, handler restoration races, and unexpected signals. Fail/skip rather than claim compatibility when unsupported.
5. Record the tested kernel release, libc, compiler, exact fixture binary hash/disassembly, observed trap addresses and counts, Linux-host syscall liveness, and PASS/FAIL/SKIP. An expected PE return value alone is not an interception oracle.
6. Keep SUD outside the containment trust boundary; use a separately justified security design for untrusted PE input.

## Decision impact

**OPEN.** The host-side code provides evidence of a narrow one-trap demonstration, not a continuously armed NT service dispatcher. No change to the architecture decision log, performance claims, or compatibility status is justified until the multi-trap and host-transition tests are executed.
