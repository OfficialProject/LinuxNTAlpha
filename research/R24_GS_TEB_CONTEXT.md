# R24 — Per-thread GS-based NT TIB prefix and Linux FS separation

Date: 2026-11-12. Status: **REPRODUCED** in one isolated Linux x86-64 environment; not a functioning Windows TEB or process startup.

## Hypothesis

Windows x64 machine code expects thread-local access via GS to TEB/TIB fields, while Linux glibc and pthreads commonly use FS for their own TLS. If Linux user threads can independently set GS without corrupting FS, a process could host a Windows TEB-like structure alongside native Linux runtime state. The crucial risk is whether GS state survives scheduling and signals, and whether setting it breaks native TLS.

## Pinned-source evidence

- Linux upstream v6.18 [Using FS and GS in user space applications](https://github.com/torvalds/linux/blob/v6.18/Documentation/arch/x86/x86_64/fsgs.rst), blob **6bda4d16d3f7476ca982c3fdc3ea1a1f8ab5add9**, explicitly states Linux TLS conventionally uses FS and GS has no common userspace use. It documents ARCH_GET_FS/GS and ARCH_SET_FS/GS via arch_prctl and warns ARCH_SET_GS can be restricted by kernel configuration/security. Faster FSGSBASE instructions exist where hardware and kernel both enable them; no FSGSBASE performance study was performed.
- ReactOS [sdk/include/xdk/ketypes.h](https://github.com/reactos/reactos/blob/master/sdk/include/xdk/ketypes.h), blob **7e4d9fe5537b726bbba0cedfa662f979b86afe4a**, defines a seven-pointer NT_TIB64 prefix: ExceptionList, StackBase, StackLimit, SubSystemTib, FiberData, ArbitraryUserPointer, Self. Six preceding pointer-size fields put **Self at offset 0x30 on x64**.
- ReactOS [sdk/include/ndk/inline_ntcurrentteb.h](https://github.com/reactos/reactos/blob/master/sdk/include/ndk/inline_ntcurrentteb.h), blob **7d555f2cda60406b5b9d9f0568edf2a4ce10c436**, implements AMD64 NtCurrentTeb using __readgsqword(FIELD_OFFSET(NT_TIB, Self)). This is open-source compatibility-project reference evidence, **not** a guarantee that every undocumented Windows internal field is stable.
- Microsoft documents [NtCurrentTeb](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-ntcurrentteb), and public [TEB fields](https://learn.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-teb) but does not promise every internal field's cross-version stability.

## Experiment performed

See source [experiments/gs-teb-probe](../experiments/gs-teb-probe/).

Test code:
1. Define a *synthetic*, explicitly **not complete** 64-bit TIB-like prefix with a self pointer at byte 0x30 and an extra test-only cookie at byte 0x38. Use compile-time offsetof assertions. The cookie is not claimed to be a Windows TEB field.
2. Main thread saves ARCH_GET_FS and ARCH_GET_GS, sets GS to a fake structure, loads GS:0x30/0x38 using real x86-64 instructions, restores original GS and checks values.
3. Create four pthread worker threads. Each saves its own FS and GS, sets GS to a distinct stack-allocated fake TIB, and executes 2048 iterations loading GS:0x30 and GS:0x38. Each worker periodically schedules with sched_yield.
4. Deliver SIGUSR1 to the current pthread every 64 iterations; a process-wide signal handler verifies GS:0x30 and GS:0x38 against that thread's expected addresses stored in native C thread-local storage. This explicitly combines GS-relative reads with FS-backed compiler TLS.
5. Ensure FS remains unchanged, GS remains the assigned fake structure until restored, and the main thread's FS/GS are unchanged after workers join. Handler uses lock-free C11 atomic integer counters; unexpected state returns failure.

**Reproduced environment:** Linux 6.18.57 x86-64, Debian GNU/Linux 13 (trixie) container, GCC 14.2.0, Clang 17.0.0. Compiled with -std=c11 -Wall -Wextra -Werror -pthread under GCC -O0, -O2 and -O3; additionally with Clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer. All four variants produced successful exit and:

    threads=4 iterations_per_thread=2048 signal_hits=128 expected=128 errors=0 main_fs_same=1 main_gs_same=1
    PASS: per-thread GS context, signal context and Linux FS TLS separation

## Interpretation (narrow)

**VERIFIED in this environment:** Linux x86-64 user threads can establish a per-thread GS data context while leaving native FS TLS usable; the expected GS values were observed in 128 signal-handler entries from four concurrent threads and through voluntary yields. This establishes an enabling mechanism for NT-compatible thread-context experiments; it does not mean the actual Windows TEB has been initialized, that an unmodified Windows PE has used GS, or that Windows TLS/SEH and loader lifecycle requirements are satisfied.

The probe's main early checks return **SKIP (77)** if the architecture access or setting is unavailable, rather than a false PASS. Per-worker failures and context mismatches return FAIL; a permission-denied GS configuration should not silently be treated as success.

## Open gates

- Integrate a Windows-target PE64 fixture that explicitly reads GS:0x30 and confirm it points to a synthetic TIB in Linux during PE entry and imported calls.
- Populate an actual minimal TEB/PEB contract: thread ID/client ID, stack bounds, TLS, LastErrorValue, exception-related data; validate against Windows-target golden fixtures.
- Test GS on involuntary preemption, asynchronous signals from other threads, thread startup/exit, alternate signal stack, fibers and runtime callbacks; no signal nesting/SEH verified.
- Compare ARCH_SET_GS vs FSGSBASE where enabled and permitted. Pin CPUID+auxv and kernel configuration, never assume user WRGSBASE is available.
- Validate security: a GS pointer is not a sandbox boundary, and hosting Windows executable code does not prevent syscalls from reaching Linux unless separately intercepted.
- No performance claim or permanent kernel architecture choice.

**No third-party source code was copied into the test.** External code was inspected only for specification/layout reference.
