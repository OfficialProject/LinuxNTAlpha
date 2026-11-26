# R276 — x64 signal-frame partial restoration and dispatch commit boundaries

Date: 2026-11-25. Status: pinned upstream source audit; no experiment executed.

## Scope
R120 separates NT CONTEXT from Linux signal return; R164/R258 cover frame lifetime and reentrancy; R141 covers extended state; R275 covers SUD configuration. This note adds a specific source-level failure ordering constraint: host signal restoration is not an atomic guest transaction.

## Verified in Linux v6.12
- [arch/x86/kernel/signal_64.c](https://github.com/torvalds/linux/blob/v6.12/arch/x86/kernel/signal_64.c), blob ee9453891901b71d6958b135848dc9cdeff0e364: native rt_sigreturn reads the frame mask and flags, applies the blocked mask, restores alternate-stack state, restores machine context, then restores the signal shadow stack. Each fallible restoration stage can branch to badframe. There is no rollback of preceding stages in this function.
- In restore_sigcontext, restart_block.fn is set to do_no_restart_syscall before copying the user context. After the copy, GPRs, stack and instruction pointers, segment selectors and permitted flags are assigned; orig_ax is set to -1; FPU restoration is then attempted. Thus failure of FPU restoration can occur after integer-register assignments. This is a source ordering observation, not a measured externally observable state.
- The same file's x64 setup path writes zero to the saved FS and GS selector fields; the restoration routine does not assign FS/GS bases from that structure. Do not infer a complete NT TEB/TLS context from those fields.
- [arch/x86/kernel/signal.c](https://github.com/torvalds/linux/blob/v6.12/arch/x86/kernel/signal.c), blob 5f441039b5725fd1a23416a686f68a667e0a8774: signal_fault forces SIGSEGV; this is not an NTSTATUS return or a rollback protocol. Signal setup also applies rseq fixup before constructing the frame, and on successful setup clears DF/RF/TF and clears user FPU state for handler entry. The handler's live state is therefore not a substitute for the saved interrupted context.

## Architectural inference (unverified)
Separate guest service completion, continuation validation and host resume into distinct states. Do not report successful guest continuation merely because a frame was populated or a service completed. Treat malformed host-frame restoration as a worker-integrity failure pending a demonstrated recovery protocol; do not automatically reinterpret the resulting SIGSEGV as an ordinary guest exception. Preserve host signal-mask and alternate-stack bookkeeping independently of guest CONTEXT. A broker side effect may already be committed before a resume failure; automatic replay could duplicate that side effect. This is a proposed failure policy, not a selected architecture.

## Proposed falsification gates (not run)
1. In isolated children, compare valid continuation with deliberately invalid frame components at different restoration stages. Capture terminal status and any observable state through a separate supervisor; do not assume the child can recover or that every intermediate state is externally observable.
2. Verify guest completion IDs and broker side-effect counts when a synthetic service succeeds but continuation fails. Require no automatic replay of a non-idempotent operation; record the terminal worker outcome separately.
3. Compare saved context against handler-live flags and SIMD state, and independently check FS/GS bases across valid return. Qualify CET-enabled and disabled configurations separately.
4. Inject an ordinary guest fault and a host continuation failure in distinct child runs; require distinct classification or explicit fail-stop when provenance cannot be established.
5. Test signal-mask and alternate-stack ownership across valid continuation and failure, with a watchdog and subprocess isolation. Record kernel tag/configuration, CPU features, libc and raw observations.

## Decision impact
Add a non-atomic restoration/failure gate to R235 before selecting a signal-return-based dispatcher. The source establishes ordering, not the feasibility of recovery, Windows compatibility, or performance. No experiments, benchmarks or implementation changes were made.
