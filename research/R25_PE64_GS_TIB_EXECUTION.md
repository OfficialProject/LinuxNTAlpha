# R25 — Actual Windows-target PE64 code accesses a synthetic TIB through GS on Linux

Date: 2026-11-12. Status: **REPRODUCED locally** on a Linux x86-64 research container, including execution of a real PE32+ image, IAT import binding and a GS:0x30 read. No Windows OS startup, real TEB, API library or native syscall implementation.

## New falsifiable milestone

R24 tested GS with Linux-native ELF code; R19/R20 tested Windows-target PE64 execution with an IAT import but did not access GS. This experiment asks whether these two proven narrow mechanisms can coexist: **can Clang-compiled Windows-targeted PE64 machine code read a thread-local TIB-like self pointer at GS:0x30 and then call one imported Microsoft-ABI function implemented inside an ELF Linux host?**

For one deliberately restricted fixture, yes.

## Primary sources

- Linux [FS/GS usage on x86-64](https://github.com/torvalds/linux/blob/v6.18/Documentation/arch/x86/x86_64/fsgs.rst), v6.18 blob 6bda4d16d3f7476ca982c3fdc3ea1a1f8ab5add9. Linux FS is conventionally glibc TLS; userspace GS base can be managed by arch_prctl where permitted.
- ReactOS [NT_TIB64](https://github.com/reactos/reactos/blob/master/sdk/include/xdk/ketypes.h) Self at offset 0x30, blob 7e4d9fe5537b726bbba0cedfa662f979b86afe4a; [AMD64 NtCurrentTeb](https://github.com/reactos/reactos/blob/master/sdk/include/ndk/inline_ntcurrentteb.h) uses __readgsqword(FIELD_OFFSET(NT_TIB, Self)), blob 7d555f2cda60406b5b9d9f0568edf2a4ce10c436. These are compatibility-project source references, not stability guarantees.
- Microsoft [PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) and [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention).

## Implementation

Add [gs_fixture.c](../experiments/pe64-image/gs_fixture.c), compiled with Clang's x86_64-pc-windows-msvc target and linked with LLD via /nodefaultlib, /dynamicbase and an import library for the synthetic **probe.dll!add_five** function. This Windows-target source uses an actual inline x86-64 instruction to load GS:0x30 and treats its result as a fake TIB self pointer. It validates a **test-only** cookie at offset 0x38; it reads a global value through a relocation-bearing pointer; and it calls the imported add_five function.

The Linux PE loader gets a new **explicit** mode: **--execute-synthetic-gs-fixture**. Only in this mode, it:
1. Loads/relocates the bounded image and binds the one permitted IAT import, as in R20.
2. Saves the thread's existing GS base with SYS_arch_prctl/ARCH_GET_GS.
3. Places a deliberately synthetic TIB-shaped struct in the host stack with self at offset 0x30 and a marker at 0x38, and uses ARCH_SET_GS to expose it.
4. Calls the PE entry via compiler-generated **ms_abi** function pointer. During the call, *actual Windows-targeted PE machine code* dereferences GS:0x30; it then enters the ELF-hosted imported function with Microsoft's x64 ABI.
5. Restores the original GS base immediately following the entry return and before cleanup.

If ARCH_GET_GS or ARCH_SET_GS is unavailable, the special mode explicitly prints SKIP and returns status 77, rather than falsely indicating Windows TIB support. The default **--parse-only** mode and the existing **--execute-trusted-fixture** mode do not alter GS.

## Reproduced observations

Test environment: Linux 6.18.57 x86-64 Debian 13 container, GCC 14.2.0 and Clang/LLD 17.0.0.

Compiled the PE fixture with Clang Windows target and LLD and verified PE header with LLVM objdump: image base 0x140000000; import directory RVA 0x2000, length 0x28; base relocation RVA 0x4000, length 0x0c; no exception directory. Loader reported:

    pe64_entry_return=42 applied_dir64=1 imports_patched=1 image_size=20480 actual_base=0x7fbac5bc4000 preferred_base=0x140000000

Five invocations in the expanded Python suite returned expected 42 and each patched one import and applied a relocation. The six earlier tests (two trusted execution fixtures, corrupted-image rejection, import rejection, parse-only acceptance and deterministic 200-case mutation sweep) also passed, for **seven unittest methods** total. Source compiled with GCC -O2, -std=c11, -Wall -Wextra -Werror, and fixture compiled with Clang x86_64-pc-windows-msvc; original R24 GS/FS threading test separately passed under GCC optimizations and sanitizers.

The entry routine's cookie comparison provides a negative branch **inside the trusted fixture** if GS points to the wrong structure. We have **not** independently confirmed a Windows kernel's runtime values or executed this fixture under Windows.

## Carefully limited conclusion

**REPRODUCED on one Linux container:** a Windows-target PE64 entry can directly read a synthetic GS TIB self pointer and call a Linux ELF-hosted function through a real PE IAT using Microsoft's x64 calling convention, with no CPU emulation, Wine, Proton or Windows SDK runtime dependency.

This is **not** an implementation of the real Windows TEB/PEB, NtCurrentTeb API, Windows process bootstrap, Windows TLS, SEH, NT syscalls, complete DLL loading, broad compatibility, or a security isolation boundary. Changing GS and resuming the original base is an application/runtime responsibility; incorrectly doing so around asynchronous signals or third-party code can destabilize the process.

## Next acceptance gates

1. Add a valid Windows-compatible minimal TEB/PEB with verified per-field offsets, initialized thread and process IDs, stack bounds and LastError semantics, with corresponding PE-target fixture checks.
2. Exercise GS during PE->ELF->PE callbacks, signals and nested mixed-ABI calls, including exception/unwind boundaries.
3. Test native thread creation from a PE-origin call and per-thread Windows TLS/GS state alongside Linux pthread TLS.
4. Safely handle Windows TLS data directories and callbacks as a distinct loader milestone. Avoid executing untrusted images.
5. Re-run on CI and independent Linux kernels/configurations, with explicit SKIP if GS modification is disallowed. No performance claim has been made.
