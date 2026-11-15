# R89 — Syscall User Dispatch transition-state invariants for mixed PE/ELF execution

Status: **architecture analysis and proposed test protocol only**, 2026-11-15 UTC. No new LinuxNTAlpha experiment, Windows reference run, or benchmark was performed for this note. This note builds on the *executed* R16/R26 single-syscall fixtures but does not extend their results.

## Architectural question

A Windows-target PE thread must sometimes execute guest-origin `syscall` instructions, then call Linux-hosted NT service implementations that themselves use Linux syscalls, then return to guest code. Can Linux Syscall User Dispatch (SUD) maintain the correct interception state through this cycle under concurrency, callbacks, exceptions and signals?

**Primary source:** Linux kernel Syscall User Dispatch documentation: https://docs.kernel.org/admin-guide/syscall-user-dispatch.html . Linux UAPI selector and control constants: https://github.com/torvalds/linux/blob/master/include/uapi/linux/prctl.h . These document the Linux mechanism; they do **not** specify an NT runtime architecture. The source documents SUD as a compatibility mechanism rather than a sandbox, and describes a per-thread selector plus an optional always-allowed instruction-pointer region.

**Repository evidence:** [R16](R16_SYSCALL_USER_DISPATCH_PROTOTYPE.md) tests host-compiled interception. [R26](R26_PE64_RAW_SYSCALL_SUD.md) executes a real PE64 fixture issuing one synthetic raw syscall, returns a synthetic value, and reports five successful fixture iterations. R26's handler switches the selector to ALLOW to permit signal return; its test only expects **one** intercepted instruction. [R22](R22_SYSCALL_DISPATCH_LATENCY.md) measures signal-interception overhead in one container, not mixed-runtime transition cost. [R69](R69_DYNAMIC_CODE_MITIGATION_EXEC_MEMORY.md) separates compatibility dispatch from hostile-code containment.

## Verified mechanism versus unproven implementation

- **DOCUMENTED (Linux):** SUD selects syscall interception by thread state and instruction-pointer rules; a blocked syscall generates `SIGSYS` for userspace dispatch. Kernel documentation explicitly rejects treating SUD as a security sandbox.
- **REPRODUCED (LinuxNTAlpha, R26 only):** A synthetic Windows-target PE syscall can be trapped and replaced with a chosen return value in one single-threaded fixture.
- **OPEN:** Whether all guest-origin calls can be intercepted while all legitimate host-origin calls are permitted across repeated transitions, nested callbacks, signal handling and thread migration.
- **OPEN:** Whether any proposed selector state can be used as an isolation or mitigation boundary. R26 does not establish that, and SUD's own documentation rules out a blanket sandbox claim.

## Proposed state machine (HYPOTHESIS, not implemented)

Represent each guest thread with a logical execution domain, not just a global boolean:

| Domain | Intended syscall rule | Transition |
|---|---|---|
| Guest PE instructions | Guest-origin raw syscalls must be dispatched as NT, never passed through as Linux numbers | Guest call into ABI bridge or SIGSYS |
| Trusted host service | Host Linux syscalls must retain their Linux meaning | Entry from verified bridge; exit back to guest |
| SIGSYS trampoline | Handler performs minimal signal-safe state capture and transfer | Resume through audited host dispatch path |
| Guest callback entered from host | Reapply guest-origin interception before invoking PE callback | Restore host domain only after callback unwinds |
| Thread exit/fault | No stale selector or signal-handler state may leak to reused host threads | Cleanup even on error paths |

This table is a **design hypothesis**, not a statement that SUD automatically distinguishes PE from ELF. In particular, setting a per-thread selector to ALLOW for the entire duration of a host function that invokes guest callbacks would create a guest bypass window. An always-allowed IP region may reduce selector toggles for *known host syscall instruction sites*, but its correctness depends on instruction provenance, module loading, code mutation and executable-memory ownership. It is not a blanket security solution.

## Failure modes requiring explicit proof

1. **Repeated raw calls:** R26's one-trap assertion cannot prove the selector is rearmed before a second PE syscall. Test consecutive guest calls separated by an ordinary host import call, including a guest callback nested inside the host call.
2. **Per-thread isolation:** A selector is thread-scoped. Thread A entering a trusted host service must not change thread B's guest interception state. Test at least two concurrent PE threads, with independent trap counts and deliberately interleaved transitions.
3. **Signal delivery and handler safety:** A signal may arrive while a host lock is held, while the thread is already in a signal handler, or during a guest/host transition. Avoid running general NT services, allocation, logging, or arbitrary guest callbacks directly from a signal handler without an async-signal-safety audit. Test restoration of signal masks, dispositions and context after each failure.
4. **Register and TLS identity:** Save/restore guest x64 nonvolatile GPRs and XMM6–XMM15, guest GS/TEB state, host FS/TLS, stack alignment and guest exception/unwind state. R24/R25/R55 cover adjacent pieces but not their composition with repeated SUD dispatch.
5. **Unexpected service number:** Never treat an unknown guest service number as a Linux syscall number. Fail explicitly with a documented guest-visible status only after Windows-reference behavior is established.
6. **Code-region provenance:** If an allow-region is used, verify the actual instruction pointer at the syscall instruction against the intended trusted host region. Audit dynamically generated code, JITs, mapped aliases and callbacks separately.
7. **Cleanup and cancellation:** Faults, guest exceptions, signal interruptions, thread cancellation and process teardown must not leave the selector in an unsafe or semantically incorrect state.
8. **Performance:** Benchmark complete guest→trap→NT service→guest cycles, separately from R22's bare trap microbenchmark. Report medians, tail distributions, kernel/toolchain versions, CPU affinity and both direct-ABI-stub and trapped-syscall paths.

## Falsifiable test ladder (NOT RUN)

- **T1:** Extend the existing R26 fixture to execute 100 sequential synthetic guest syscalls with distinct input/output pairs; assert 100 interceptions, zero unintended Linux syscalls, and correct results.
- **T2:** Interleave each guest syscall with a Linux-hosted MS-ABI import that performs a harmless real Linux syscall; assert correct guest interception and host syscall semantics.
- **T3:** Make the host import invoke a guest PE callback which issues a raw syscall; verify nested domain reentry and restoration.
- **T4:** Run T1–T3 on multiple threads with independent GS guest context and host FS TLS; verify per-thread trap counters and context.
- **T5:** Inject unrelated signals and controlled error exits at each transition edge; verify no stale selector state or signal disposition after cleanup.
- **T6:** Compare a trusted-host syscall-region approach against explicit selector toggles; inspect generated disassembly and instruction-pointer boundaries before timing either.
- **T7 (adversarial, separate):** Attempt guest-origin Linux syscall bypasses through alternate instruction sites and executable aliases. Record the result as a **security evaluation**, not as a compatibility benchmark. SUD alone must not be advertised as containment.

Each test must report PASS, FAIL or SKIP with kernel capabilities and toolchain. No pass is inferred from this plan. For any guest-visible NTSTATUS, first establish reference behavior on a pinned Windows build; the synthetic service number in R26 is deliberately *not* a Windows NT service mapping.

## Provisional decision

**Do not choose SUD as the sole universal NT syscall boundary yet.** Retain it as a candidate for raw PE-origin syscall interception while direct ABI-compatible imports remain a distinct candidate for known API entry points. Before architectural selection, implement and reproduce T1–T5, establish the executable-region trust model, and compare full transition costs. The compatibility and security milestones must remain explicitly separate.

## Source/evidence ledger

- Linux kernel, *Syscall User Dispatch*, https://docs.kernel.org/admin-guide/syscall-user-dispatch.html — primary mechanism and non-sandbox warning; upstream revision not pinned in this note.
- Linux kernel, `include/uapi/linux/prctl.h`, https://github.com/torvalds/linux/blob/master/include/uapi/linux/prctl.h — primary constants; moving master reference, pin a commit before implementation.
- LinuxNTAlpha R16, R22, R24–R26, R55, R69 — repository-local experiments and related research, as characterized above.

**Evidence limitation:** External primary-source URLs are supplied for independent inspection; this run did not retrieve or diff upstream source revisions. No new empirical claim is based on an unperformed experiment.
