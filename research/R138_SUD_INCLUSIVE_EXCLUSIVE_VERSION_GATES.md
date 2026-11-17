# R138 — Syscall User Dispatch inclusive/exclusive upstream drift and runtime capability gate

Reviewed: 2026-11-17. **Documentation/source review only; no kernel experiment was executed.** This extends R16/R26/R128/R129 rather than repeating their syscall-interception proof.

## VERIFIED: upstream documentation has changed

The Linux upstream *master* documentation, blob `c1768d9e80fa2d462846f4c9425b1153357ebd63`, now names two enabled modes: `PR_SYS_DISPATCH_EXCLUSIVE_ON` and `PR_SYS_DISPATCH_INCLUSIVE_ON`. In **exclusive** mode the specified address interval always executes syscalls directly, and the selector controls dispatch outside it. In **inclusive** mode the specified interval is subject to selector-based dispatch, and syscalls outside it execute directly. The selector remains a userspace byte taking ALLOW/BLOCK values. This is a material difference for selecting a guest-PE-only trapping region versus a host-library exemption region.

Primary source: Linux upstream master Documentation/admin-guide/syscall-user-dispatch.rst, lines 55–83, retrieved 2026-11-17:
https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux.git/+/refs/heads/master/Documentation/admin-guide/syscall-user-dispatch.rst

By contrast, the versioned Linux 6.12 documentation describes the older `PR_SYS_DISPATCH_ON` interface with an always-allowed interval. **Do not assume the newer modes are available on a deployed kernel merely because upstream master documents them.** Primary source:
https://www.kernel.org/doc/html/v6.12/admin-guide/syscall-user-dispatch.html

The upstream documentation also explicitly states that SUD is **not a sandbox**: a guest may branch into an allowed dispatcher or modify the selector. A security boundary must be independently enforced (for example, evaluated seccomp policy). The kernel docs state fork/exec resets dispatch. The Linux man-pages SUD page specifies thread-local configuration, thread-directed SIGSYS, `SYS_USER_DISPATCH` in `si_code`, the saved program counter after the syscall instruction, and register-context editing for emulated return values. Sources:
https://www.kernel.org/doc/html/v6.12/admin-guide/syscall-user-dispatch.html
https://man7.org/linux/man-pages/man2/PR_SET_SYSCALL_USER_DISPATCH.2const.html

## OPEN: actual release and installed-header support

This review did **not** establish the first tagged kernel release implementing inclusive dispatch, the numeric UAPI constants, or availability on LinuxNTAlpha's target machines. The master documentation is not proof of a particular stable release. Resolve with a pinned upstream kernel commit/tag and inspect `include/uapi/linux/prctl.h`, implementation and selftests for that exact revision. A local compiler header may lag the running kernel; report compile-time and runtime capability separately. Never silently map an unsupported inclusive mode to exclusive mode.

## HYPOTHESIS: inclusive dispatch can reduce guest/host selector switching

If the runtime can arrange a bounded executable guest syscall-stub region, inclusive mode could trap only calls originating there while leaving host syscalls alone. This might reduce boundary-switch overhead, but is **not established**: Windows applications may issue raw syscalls outside canonical stubs, relocated/JIT code changes address ranges, and the model has one interval rather than arbitrary scattered ranges. An NT compatibility layer must define its intended coverage before measuring. No performance result is asserted.

## Proposed reproducible gates (NOT EXECUTED)

1. Pin kernel release, `uname -r`, UAPI header revision, architecture and libc; detect both mode constants from the pinned source rather than guessing numbers.
2. In a disposable child process, install a SIGSYS handler and test exclusive mode: a syscall inside the allowed interval bypasses interception while a syscall outside it traps with BLOCK.
3. Where supported, test inclusive mode with reversed placement: only syscall instructions inside the selected interval trap under BLOCK; outside calls bypass. Treat `EINVAL` or missing constants as unsupported, not PASS.
4. Check ALLOW/BLOCK transitions, handler return register values, nested signal restoration, multiple threads with independent selectors, and fork/exec reset. Run dangerous invalid-selector tests only in isolated child processes.
5. Verify that no untrusted guest-controlled selector or executable-region jump is accepted as a security boundary. Separately evaluate seccomp interaction and the threat model.
6. Benchmark host/guest transitions and trapped calls against R22's pinned protocol, retaining kernel version, frequency, confidence intervals and tails. No comparative performance claim until run.

## Architectural consequence

Maintain SUD as a **capability-negotiated interception mechanism**, not a fixed API or security guarantee. Preserve R26's verified narrow exclusive-style proof as historical evidence; do not reclassify it as inclusive-mode validation. Implement a feature matrix with `exclusive_supported`, `inclusive_supported`, `signal_delivery_verified`, `thread_lifecycle_verified`, and `security_boundary_independent`, each supported by an actual test record.
