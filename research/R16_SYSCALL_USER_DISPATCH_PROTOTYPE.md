# R16 — Syscall User Dispatch: source audit and executable smoke test

Date: 2026-11-12. Status: **VERIFIED on one isolated Linux execution environment**, not validated on end-user hardware or benchmarked.
Experiment: [experiments/syscall-user-dispatch](../experiments/syscall-user-dispatch/).

## Design question

Can Windows-origin CPU syscall instructions be prevented from accidentally entering the Linux syscall table while Linux-native helper code continues to issue ordinary Linux syscalls? This is necessary when executing unmodified x86-64 Windows machine code on Linux: the syscall instruction itself is native x86-64, but its service-number namespace, user/kernel ABI and semantics are not interchangeable.

## Primary-source findings

1. [Linux Syscall User Dispatch documentation](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html) explicitly cites compatibility layers including Wine. Dispatch can direct selected syscalls to a thread-directed SIGSYS handler based on a syscall-instruction address range and/or a userspace selector. The documentation says this is **not a sandbox** and notes signal delivery cost.
2. The [Linux prctl UAPI header](https://github.com/torvalds/linux/blob/master/include/uapi/linux/prctl.h) defines PR_SET_SYSCALL_USER_DISPATCH as 59, PR_SYS_DISPATCH_OFF as 0, PR_SYS_DISPATCH_EXCLUSIVE_ON as 1, PR_SYS_DISPATCH_INCLUSIVE_ON as 2, and backward-compatible alias PR_SYS_DISPATCH_ON as exclusive mode. Older installed headers may expose only the legacy ON name; feature probing must be against the **running kernel**, not just header symbols.
3. With dispatch enabled and selector blocked, the kernel does not execute the intercepted syscall. The SIGSYS handler can modify the saved return register in ucontext; returning from the signal handler resumes code after the instruction. See [PR_SET_SYSCALL_USER_DISPATCH(2const)](https://man.archlinux.org/man/core/man-pages/PR_SET_SYSCALL_USER_DISPATCH.2const.en).
4. Fork and exec reset the dispatch setting; configuration is per thread. Thread and lifecycle conformance remain needed.
5. SIGSYS handling must not recursively trap the rt_sigreturn trampoline. A safe transition into normal Linux code requires careful selector/range handling.

## Experiment performed

**Platform:** Linux x86-64, kernel 6.18.57, Debian GNU/Linux 13 (trixie), GCC 14.2.0 (Debian 14.2.0-19). Runtime is a container; its CPU topology, security settings and underlying physical host are not validated as representative.

**Procedure:** compile a standalone C file with -std=c11 -Wall -Wextra -Werror at -O0, -O2 and -O3. Install SIGSYS handler, enable dispatch, block selector, perform raw SYS_getpid using syscall, replace saved RAX with 0x12345 in the handler, set selector back to allow (for sigreturn), then disable dispatch and obtain actual PID with raw syscall.

**Observed outputs (sample for each optimization level):**
- -O0: blocked_return=74565 actual_pid=482 traps=1 observed_syscall=39 unexpected=0
- -O2: blocked_return=74565 actual_pid=476 traps=1 observed_syscall=39 unexpected=0
- -O3: blocked_return=74565 actual_pid=462 traps=1 observed_syscall=39 unexpected=0
- All test processes exited 0. PIDs are per-process and nonrepeatable.

**Interpretation:** on this kernel, intercepted native getpid returned the synthetic value, while a subsequent call completed normally. Exactly one expected SIGSYS was counted. This validates a critical *mechanism*, not Windows NT syscall dispatch.

## Important limitations

- The intercepted instruction in the test is a **Linux** getpid syscall, not actual Windows ntdll-generated machine code. An NT service-number table, ABI bridge, TEB/PEB, exception unwinding, Windows object manager and security remain unimplemented.
- This does not measure interception latency. Kernel documentation explicitly warns signals may be expensive.
- The test turns the selector to ALLOW in the handler to ensure sigreturn works; a full implementation must rearm interception at the correct boundary. Leaving the selector always on ALLOW would let native syscall instructions bypass redirection.
- Signal reentrancy, thread creation, multithread races, ptrace/debuggers, seccomp composition, alternative binary loading and older-kernel compatibility remain OPEN.
- Do not rely on SUD for containment; enforce isolation with Linux-native sandboxing and treat untrusted Windows code as hostile host code.

## Candidate architecture and decision gate (HYPOTHESIS)

Use a known, trusted Linux-native runtime region for Linux syscall issue paths. Use an explicit transition discipline between Windows-code execution and runtime helper code; test address-range vs selector-based SUD. Compare (a) userspace ABI bridges with direct calls at the Win32/NTDLL boundary, (b) SUD exception-path interception for raw NT syscalls, and (c) targeted kernel changes **only after** correctness and benchmark evidence. A hybrid is plausible because many programs call exported APIs rather than hand-written syscalls, but do not assume all binaries follow that convention.

**Next experiments:** capture an actual benign PE64 test fixture emitting raw NT syscall instructions, verify safe routing without invoking Linux syscall numbers; test per-thread isolation, nested signal handling and selector rearming. Pin an upstream kernel release and measure p50/p99 interception overhead with controls. No architecture choice made.

## Potential maintenance burden

A published March 2026 [Wine upstream patch discussion](https://list.winehq.org/hyperkitty/list/wine-gitlab%40list.winehq.org/message/CL53WAN7ATUKCYNI46DP3UELPFYWLZXS/) describes CPU context inconsistency when a signal arrives within an x86-64 syscall dispatcher. This is evidence that signal/context interaction deserves first-class correctness testing, **not** proof that Wine's specific fixes are needed here.
