# R26 — Trap an actual Windows-target PE64 raw syscall before it enters Linux

Date: 2026-11-12. Status: **REPRODUCED** on one Linux x86-64 container and verified in the repository's independent GitHub Actions smoke tests. This is *not* a real Windows NT system-service implementation.

## Problem statement

Linux and Windows on x86-64 use the same CPU opcode for the system-call instruction, but they have different service numbers, entry ABIs, semantics and security models. Running unmodified Windows-origin code must **never** blindly let a raw Windows NT syscall enter Linux under the same number. Previous R16 verified interception of a Linux-compiled raw getpid instruction, while R19/R20/R25 executed Windows-target PE code without raw syscalls. This experiment combines a **real PE32+ image** with an x86-64 instruction that must be redirected.

## Primary sources and fixture

- Linux [Syscall User Dispatch](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html) documents per-thread dispatch of chosen user-mode syscall instructions to SIGSYS, while explicitly warning it is **not a sandbox** and signal delivery adds overhead.
- Linux [prctl.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/prctl.h) provides PR_SET_SYSCALL_USER_DISPATCH and PR_SYS_DISPATCH_ON/OFF.
- Microsoft [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention) and [PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) specify Windows-target executable layout and call ABI. The fixture is *not* an actual Windows ntdll service stub with a verified Windows build-specific service-number table.

In [sud_fixture.c](../experiments/pe64-image/sud_fixture.c), a freestanding Windows-target PE64 image, compiled by clang for x86_64-pc-windows-msvc and linked with LLD, issues:

    movq %rcx, %r10
    movl $39, %eax
    syscall

This **NT-style instruction sequence** deliberately chooses service number **39**, which is Linux x86-64 SYS_getpid. It is **not** presented as a real Windows Nt* service number. The fixture checks that the saved return value was replaced with **0x12345** and that a relocation-bearing global pointer still points to the test value 37; returns 42 only when both checks pass.

LLVM's objdump disassembly verified the actual PE machine code contains movq rcx→r10, eax=0x27 and syscall at the entry. PE has the required DIR64 relocation directory, no imports and no exception directory.

## Linux host interception

An extra explicit PE loader mode **--execute-synthetic-sud-fixture**:
1. Installs a SIGSYS handler with SA_SIGINFO, saves the previous SIGSYS disposition and enables Linux PR_SET_SYSCALL_USER_DISPATCH with a userspace selector.
2. Switches selector to BLOCK before calling the PE entry through its Microsoft x64 ABI function pointer.
3. On SIGSYS, checks code SYS_USER_DISPATCH and intercepted service number SYS_getpid. The handler sets the selector to ALLOW to permit rt_sigreturn and writes 0x12345 into the saved RAX register of ucontext.
4. After returning from the PE function, restores ALLOW, disables dispatch and restores the original SIGSYS handler.
5. Checks precisely **one SIGSYS** and zero unexpected trap metadata; reports the PE's entry result.

This is single-threaded fixture-only logic. It does not build an NT service-number table, implement actual Nt* functions, preserve or rearm interception across all guest/runtime transitions, or constrain a guest process's host syscall permissions.

## Observed result

Local test on Linux 6.18.57 x86-64 Debian 13 container, GCC 14.2.0, Clang/LLD 17.0.0:

    sud_traps=1 unexpected=0 observed_service=39
    pe64_entry_return=42 applied_dir64=1 imports_patched=0 image_size=16384 actual_base=... preferred_base=0x140000000

The updated Python test runs this Windows-target executable **five times** and requires exactly one SIGSYS trap, no unexpected trap, nonzero relocations and entry result 42. It also checks that the ELF-hosted launcher returns **SKIP (77)** when PR_SET_SYSCALL_USER_DISPATCH is unavailable. At time of this research the full PE suite (eight Python unittest methods) passed locally. The independent GitHub Actions CI run at commit 4aa1231b4828611b769a062f9516d5d98d5135c0 is the acceptance target, subject to confirmation of its final conclusion.

**Meaning:** Linux SUD can intercept a *Windows-target-compiled* PE's raw x86-64 syscall instruction and synthesize a value without completing the Linux kernel's syscall numbered 39. This is a narrow interception primitive, **not** a functioning Windows NT syscall.

## Important limitations and research gates

1. **Guest syscall instruction versus host system calls:** the selector returns to ALLOW during signal handling. A real runtime must reliably rearm interception on guest transitions, potentially via trusted-code region selection or guarded syscall stubs, without breaking native Linux runtime syscalls.
2. **Service identity and Windows ABI:** actual NT syscall numbers vary by Windows build and calling-context requirements; we used a deliberately artificial number that collides with Linux getpid. Do not infer compatibility from that number.
3. **Correctness:** test multiple guest syscall instructions, signal arrivals, nested callbacks, all general/vector register preservation, NtStatus/error conversion, thread exit, signal masks, exceptions, and GS/TEB restoration.
4. **Latency:** R22 measured ~1 microsecond per signal interception in one container; the true app-level cost depends on NT call frequency, service implementation, IPC and performance controls. No game/frame-time claim follows.
5. **Security:** syscall user dispatch itself is **not a sandbox**; the example has no untrusted-binary isolation and no Windows anti-cheat, driver or privileged-operation support.
6. **Fallbacks:** use an explicit unsupported/skip status when kernel features are missing. No kernel fork decision is justified by this experiment alone.
7. **Conformance:** next milestone is a **synthetic repeated-NT-service PE fixture** that re-enters interception from a trusted Linux bridge and compares to a Windows-target golden behavior, not a promise of general Windows execution.

## Decision

Keep SUD as a narrow candidate for raw Windows-origin syscall interception. Prefer direct, ABI-correct API entry stubs for known imports when their semantics can be preserved, and benchmark the total tradeoff before considering changes to the kernel.
