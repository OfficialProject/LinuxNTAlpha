# R236 — Syscall User Dispatch inclusive/exclusive mode compatibility gate

Date: 2026-11-22. **Documentation review only.** No kernel probe, PE execution, seccomp installation, or benchmark was performed in this research run.

## Why this is distinct

[R234](R234_SUD_REARM_TRANSITION_INVARIANTS.md) specifies repeated guest-to-host trap/rearm invariants. This note isolates a prerequisite that can invalidate its address-region comparison: **the upstream kernel documentation now describes two opposite address-region policies**, while the published Linux man page describes the older single `PR_SYS_DISPATCH_ON` interface. Code must not infer a mode's meaning from its English name or assume the running kernel supports a recently documented UAPI.

## VERIFIED from primary Linux kernel documentation (retrieved 2026-11-22)

The Linux kernel [Syscall User Dispatch documentation](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html), Interface section, specifies:

- `PR_SYS_DISPATCH_EXCLUSIVE_ON`: the configured interval **always executes Linux syscalls directly**; outside it the selector determines dispatch. The signal trampoline should be included in the direct region, unless the relevant vDSO trampoline is exempt as described by the documentation.
- `PR_SYS_DISPATCH_INCLUSIVE_ON`: the configured interval is **subject to the selector**; outside it syscalls execute directly. This is *not* the same policy with reversed terminology.
- `PR_SYS_DISPATCH_OFF` disables dispatch for the thread; the other parameters must be zero.
- The selector is a user-memory byte taking `SYSCALL_DISPATCH_FILTER_ALLOW` or `SYSCALL_DISPATCH_FILTER_BLOCK`. Invalid values are fatal.
- `PTRACE_GET_SYSCALL_USER_DISPATCH_CONFIG` and `PTRACE_SET_SYSCALL_USER_DISPATCH_CONFIG` can inspect/change a task's configuration.
- SUD is **not a sandbox**: allowed code regions and writable selectors can be used to bypass it. The documentation says fork/exec reset dispatch.

Source: https://docs.kernel.org/admin-guide/syscall-user-dispatch.html (Interface and Security Notes, accessed 2026-11-22).

The [Linux man-pages PR_SET_SYSCALL_USER_DISPATCH reference](https://man7.org/linux/man-pages/man2/PR_SET_SYSCALL_USER_DISPATCH.2const.html) describes `PR_SYS_DISPATCH_ON` and `PR_SYS_DISPATCH_OFF` with one direct-allowed interval, and explicitly states that the configuration is not preserved across fork, clone or exec. **This is a documented interface-description difference, not evidence that any particular installed kernel implements both new modes.** Source accessed 2026-11-22.

## HYPOTHESIS / design consequence

For an NT-origin syscall interceptor, EXCLUSIVE mode may allow a small trusted host syscall island while intercepting guest-origin syscall instructions outside it, but guest-controlled jumps into that island still defeat isolation. INCLUSIVE mode is unsuitable for this *same* narrow-host-island layout: it would dispatch the island based on the selector and let the rest of the address space call Linux directly. INCLUSIVE mode could instead be considered for a bounded guest-code interval, but guest code outside that interval becomes a direct-syscall escape if it can reach arbitrary executable memory.

Therefore **mode choice must be coupled to a proven mapping/instruction-origin policy**, not simply toggled to see which passes a one-syscall fixture. A guest-writable selector cannot be treated as a privilege boundary in either mode.

## OPEN — version and source-audit gate

1. Pin the exact kernel release, running `uname -r`, distribution backports, and its `include/uapi/linux/prctl.h` and dispatch implementation. Compare the installed headers against the running kernel. This run did **not** verify the introduction commit or release for INCLUSIVE mode.
2. Compile mode constants from the *actual* headers, and perform a **safe, subprocess-isolated** feature probe of each mode; report unsupported as SKIP, not as proof of equivalence.
3. For EXCLUSIVE, test syscall instructions **inside and outside** the permitted interval with both selector values, plus `rt_sigreturn` and host-bridge calls. For INCLUSIVE, repeat with the opposite expected region behavior. Require recorded instruction pointers and expected SIGSYS counts, not merely a successful PE return.
4. Repeat after fork, clone/thread creation, and exec as separate cases. Record per-thread setup, actual kernel behavior and differences from the man-page wording; do not extrapolate across kernel versions.
5. Test guest attempts to jump into the trusted syscall island or write the selector in an isolated adversarial fixture. Such bypass is expected from the documented threat model and **must not be counted as a security regression unique to a particular implementation**.
6. Treat seccomp and host-process isolation as separate security controls. A seccomp policy must be independently tested for legitimate host syscalls and prohibited guest escapes; SUD itself does not provide this guarantee.

## Decision impact

**OPEN:** R234's region-dispatch alternative is not yet validated. The next prototype must select and feature-probe a precise SUD mode and prove region semantics, rearming, signal-return liveness, and thread behavior. No performance or compatibility claim follows from this documentation review.
