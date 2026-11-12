# Syscall User Dispatch smoke test

Purpose: establish whether a Linux kernel can intercept a selected x86-64 native syscall instruction and return a synthetic value in the caller's register context without executing the syscall. This is a **Linux mechanism probe**, **not a Windows binary loader** or NT system-call emulator.

## Build and run

On an x86-64 Linux development host with a C compiler and Make, run **make test** in this directory.

- Exit 0: PASS; the intercepted getpid returned the synthetic result, the normal call returned a PID, and exactly one expected SIGSYS was received.
- Exit 77: direct execution reports that PR_SET_SYSCALL_USER_DISPATCH is unsupported or prohibited by the sandbox. The Makefile displays **SKIP** and succeeds in this situation; it does **not** call this a pass.
- Other exit: failed test or unexpected runtime error.

Test is currently x86-64 Linux only. It installs a SIGSYS handler, enables syscall user dispatch, toggles the selector to block, executes raw getpid via the x86-64 syscall instruction, records SIGSYS, supplies a synthetic RAX, restores the selector to permit rt_sigreturn, then disables the feature and runs an unmodified getpid. It changes state only in its own process.

The SYS_USER_DISPATCH si_code constant is 2 per Linux UAPI asm-generic/siginfo.h, provided here only when the C library headers do not declare it. Compile-time macro availability does not prove runtime support.

No syscall latency, Windows x64 compatibility, native NT call number mapping, or security isolation is established by this experiment. Syscall User Dispatch is not a sandbox.

Primary sources:

- [Linux kernel Syscall User Dispatch](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html)
- [Linux kernel prctl UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/linux/prctl.h)
- [Linux kernel SIGSYS UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/asm-generic/siginfo.h)
