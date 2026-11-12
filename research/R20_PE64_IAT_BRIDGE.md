# R20 — PE32+ import lookup and IAT patch into a native Linux-hosted Windows-ABI function

Date: 2026-11-12. Status: **REPRODUCED** with a synthetic one-import PE64 fixture on Linux x86-64. Not a production loader, Windows DLL loader or general API implementation.

## Hypothesis and test

A PE32+ image with a real import table can execute an imported function via an Import Address Table (IAT) slot when a Linux-native host explicitly resolves that import to a compatible Microsoft x64 ABI stub. This does not require Wine/Proton or emulation, but requires **a compatible implementation** of the invoked API and the loader's binding logic.

### Primary specification

Microsoft PE/COFF specification:
https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

The import directory consists of 20-byte DLL descriptors terminated by an all-zero descriptor. For PE32+, lookup table entries are 64-bit RVAs to Hint/Name entries or ordinal-coded values, terminated by a zero. The loader overwrites corresponding IAT entries with 64-bit symbol addresses. A real Windows loader must handle far more than the single case below.

Microsoft Windows x64 calling convention:
https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention

LLVM LLD Windows linker:
https://lld.llvm.org/windows_support.html

## Controlled fixture build

The experiment at [experiments/pe64-image](../experiments/pe64-image/) generates two **Windows-targeted** inputs using clang targeting x86_64-pc-windows-msvc and lld-link:

1. A tiny synthetic export DLL named **probe.dll** created only to produce a corresponding **probe.lib import library**. LinuxNTAlpha does **not** load or execute this DLL.
2. An EXE built from **import_fixture.c**, referencing __declspec(dllimport) int add_five(int) and a global 64-bit pointer to a separate global value 37, with /nodefaultlib and /dynamicbase.

LLVM's objdump of the resulting executable reports: PE32+, preferred image base 0x140000000, entry RVA 0x1000; .text, .rdata, .data, .reloc; import directory RVA 0x2000 length 0x28; IAT directory RVA 0x2038 length 0x10; base relocation directory RVA 0x4000 length 0x0c. Exactly one DLL descriptor refers to **probe.dll**, containing exactly one by-name import **add_five**.

The Linux ELF launcher maps and relocates this image, validates the import descriptor table and both null terminators, resolves the only permitted exact pair **probe.dll!add_five**, and writes the function pointer of **host_add_five** (declared **ms_abi**) into the IAT slot before changing image pages to their final permissions. It then invokes the real PE64 entry point; the imported call crosses from Windows-generated code to a Linux ELF function using Windows x64 calling conventions, and returns 42.

**Crucial distinction:** only this exact synthetic DLL/function name is allowed. There is no Windows DLL loaded, no dynamic module search, no ordinal import, no forwarders, no Windows kernel API, no registry, no PEB/TEB or TLS.

## Reproduced observations

Host: Debian GNU/Linux 13 (trixie), Linux kernel 6.18.57, x86-64, GCC 14.2.0 and LLVM clang/lld-link 17.0.0. Command from repo root: **make -C experiments/pe64-image test**.

- The original no-import PE fixture passed 5 invocations: entry result 42, one or more DIR64 relocations, mapped ASLR address not equal to preferred base.
- The **new import fixture passed 5 invocations**: entry result 42, nonzero DIR64 relocations and exactly **imports_patched=1**.
- All 19 previously defined malformed-header/relocation negative cases were explicitly rejected.
- **Seven new IAT/import negative cases** were explicitly rejected: unexpected DLL name; unexpected function; ordinal lookup; second nonzero import; nonzero second descriptor; missing IAT directory; truncated import directory.
- A clean full rebuild and suite passed at GCC -O2. The suite passed with launcher compiled at -O0 and -O3 and with clang AddressSanitizer + UndefinedBehaviorSanitizer enabled. The Windows PE fixture is *not* instrumented by the host sanitizers.

The test suite now has four test methods and 26 individually constructed negative cases. All tests passed in the recorded container. There is no Windows-hosted baseline and no throughput/latency comparison.

## Engineering conclusion

**Verified narrowly:** a Windows-target-compiled PE64 program can make a typed imported call into a Linux-hosted function through an actual PE Import Address Table, without translating CPU instructions. This is a demonstrable building block for a compatibility runtime, **not a claim that arbitrary Win32 software works**. Producing correct implementations for the hundreds or thousands of APIs and NT semantics demanded by real programs is a distinct and much larger problem.

## Unresolved / design gates

1. General-purpose import descriptor parsing (multiple DLLs, multiple named and ordinal imports), import-by-ordinal, forwarded exports, API sets and loader search order. Implement parse-only tests and fuzzing **before** expanding executable support.
2. Independently implemented Windows-compatible API module design, handle and NT object semantics, process and thread startup, security, TEB, PEB, TLS, SEH.
3. Proper DLL loading/relocation with recursive import resolution and loader lock, including attach/detach and TLS callback order.
4. Cross-ABI argument, return, nonvolatile register, exception/unwind and reentrancy correctness for each exported function.
5. SUD and raw NT syscall interception remains a separate service path; a working IAT does not make raw NT syscalls safe.
6. Security: **do not execute downloaded or untrusted PE images in this loader**. It executes input as host CPU code with the launcher's privileges. The allowlist is an experiment guard, not sandbox isolation.
7. Reproducible Windows and Linux conformance fixtures, sanitization, hostile input handling and CI verification before architectural decisions.

No permanent architecture selected.
