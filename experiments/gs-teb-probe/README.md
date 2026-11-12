# GS/FS and synthetic Windows x64 TIB context experiment

This x86-64 Linux test checks **only** whether Linux can place a small user-controlled, NT_TIB-shaped structure at each thread's GS base without changing the FS base managed by native Linux glibc/pthreads.

## Run

In this directory: **make test**. Direct binary execution returns 0=PASS, 77=SKIP for missing/blocked arch_prctl GS support (as determined in main), or another nonzero value on failure. This test needs gcc or clang and pthreads; it does NOT need Windows, Wine/Proton, NT headers or root privileges.

The test uses Linux SYS_arch_prctl with ARCH_GET_FS, ARCH_GET_GS and ARCH_SET_GS. It preserves each thread's previous GS base and checks that FS is unchanged. A synthetic 64-bit NT_TIB-like struct has 6 64-bit pointer-size fields, a self pointer at offset 0x30, and a non-Windows test cookie at 0x38. Every thread executes machine-code GS:0x30 and GS:0x38 loads. Four pthreads each perform 2048 iterations with sched_yield, and direct SIGUSR1 delivery every 64 iterations. The signal handler checks that the same per-thread GS context remains visible on signal entry. The main thread tests GS set/restore and later checks both FS/GS remain unchanged.

**Reproduced 2026-11-12:** Linux 6.18.57 x86-64, Debian 13, GCC14.2.0. GCC -O0/-O2/-O3 and Clang -O1 ASan+UBSan all passed:

    threads=4 iterations_per_thread=2048 signal_hits=128 expected=128 errors=0 main_fs_same=1 main_gs_same=1
    PASS: per-thread GS context, signal context and Linux FS TLS separation

## Primary sources

- Linux v6.18 x86 FS/GS doc: https://github.com/torvalds/linux/blob/v6.18/Documentation/arch/x86/x86_64/fsgs.rst (blob SHA 6bda4d16d3f7476ca982c3fdc3ea1a1f8ab5add9)
- Linux arch_prctl(2): https://man7.org/linux/man-pages/man2/arch_prctl.2.html
- ReactOS NT_TIB64 declaration: https://github.com/reactos/reactos/blob/master/sdk/include/xdk/ketypes.h (inspected blob SHA 7e4d9fe5537b726bbba0cedfa662f979b86afe4a)
- ReactOS AMD64 NtCurrentTeb access: https://github.com/reactos/reactos/blob/master/sdk/include/ndk/inline_ntcurrentteb.h (blob SHA 7d555f2cda60406b5b9d9f0568edf2a4ce10c436)
- Microsoft public Windows TEB interface: https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-ntcurrentteb

**Not proven:** complete Windows TEB layout, TEB/PEB initialization, Windows TLS APIs, GS preservation across all Linux signals/runtime and thread transitions, fibers, native executable loader integration, Windows SEH, actual Windows process startup, performance, or sandboxing. The test uses original synthetic C structs and copies no upstream implementation code.

See [R24](../../research/R24_GS_TEB_CONTEXT.md) for findings and next gates.
