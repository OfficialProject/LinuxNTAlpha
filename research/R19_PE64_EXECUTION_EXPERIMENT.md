# R19 — A freestanding PE32+ executes native x86-64 machine code on Linux

Date: 2026-11-12. **Status: REPRODUCED for one deliberately restricted fixture.** Neither broad Windows application compatibility nor a complete NT loader has been demonstrated.

## Question and falsifiable milestone

Can we map a real PE32+ x86-64 image produced by a Windows-targeting compiler, apply base relocations, respect executable section permissions, and call its entry point natively under Linux using the Microsoft calling convention, without Wine/Proton or CPU emulation? If this fails for even one deliberately minimal no-import fixture, more ambitious approaches are premature.

The answer for the specific tested fixture/environment is **YES**. The milestone is narrow but stronger than executing a GCC-produced ms_abi function inside a Linux ELF (R17).

## Evidence and reproduction

Implementation: [experiments/pe64-image](../experiments/pe64-image/). Build with make; test with **make -C experiments/pe64-image test**. Toolchain uses clang targeting x86_64-pc-windows-msvc and lld-link to create a real PE32+ executable, with no Windows SDK or CRT dependency. The host launcher is compiled as Linux ELF with a typed **ms_abi** entry function pointer. All executable code in the PE is x86-64 bytes executed by the CPU directly.

**Fixture semantics:** a global 64-bit pointer references a global 64-bit value 37. Its entry reads through that pointer, adds 5 and returns 42. Its link-produced .reloc entry must patch the pointer when the image is not at 0x140000000.

**Observed in container:** Linux x86-64 kernel 6.18.57, Debian 13, GCC 14.2.0, clang and lld-link based on LLVM 17. PE has sections .text, .data, .reloc; entry RVA 0x1000; preferred base 0x140000000; image size 0x4000; relocation directory RVA 0x3000 size 0x0c. Objdump shows all import/TLS/exception/delay-import directory sizes zero. The launcher maps anonymous memory, copies validated sections, processes IMAGE_REL_BASED_DIR64 entries by adding (mapped_base − preferred_base) modulo 2^64, applies mmap protection changes and invokes entry.

**Test results:** Python unittest has 2 test methods: positive fixture invoked five times with assertions that entry returned 42, DIR64 count is nonzero, mapped address differs from preferred base; negative cases include invalid MZ/PE signatures, wrong CPU/PE flavor, impossible optional header/image sizes, section overlap and W+X, nonzero import/TLS directory, missing/malformed/unsupported relocation entries and raw file bounds. **19 corrupted-image cases were rejected with explicit REJECT**. Entire suite passed with GCC -O0, -O2, -O3 and clang ASan+UBSan launcher build. No instrumentation of generated PE code and no Windows-side golden baseline were performed.

## Why this matters

Verified: x86-64 Windows-target machine instructions are native x86-64 instructions, and a carefully limited PE image can be loaded into a Linux address space and entered without an instruction interpreter or virtual machine. The two unavoidable tasks are (a) PE image semantics (address layout, relocations, permissions, imports, TLS etc.) and (b) the Windows ABI and operating-system services used by real programs.

**NOT proved:** complete PE loader correctness, Windows ntdll or Win32 services, dynamic DLL imports, per-thread TEB at GS, kernel system service dispatch, SEH/unwind, loader lock, Windows process semantics, anti-cheat support, graphics API compatibility, broad file path semantics, practical game compatibility, security or speed. The experiment **is not a safe sandbox**; running arbitrary untrusted PE files in it is dangerous. The parser intentionally refuses most real executables.

## Architecture implications and hypotheses

- The user-space execution path is viable for *some* Windows-target native instructions. This does **not** select a permanent user-space vs kernel NT architecture; system and memory services still need evaluation.
- GCC/Clang ABI-annotated function pointers suffice for this artificial entry signature. General import function stubs, callbacks, varargs, SEH and asynchronous stack unwinding need distinct tests.
- Strict unsupported-feature rejection is preferable to silently proceeding with incorrectly initialized imports/TLS/exception state.
- Applying relocations on a writable but nonexecutable mapping, then transitioning pages to final protections, can avoid simultaneous writable-executable mappings in this example. It does not provide an isolation boundary.

## Next falsifiable milestones

1. **Import address table**: one controlled PE64 fixture using a deliberately named test DLL/import, resolve an ABI-correct stub in the launcher, verify bound addresses and import-array termination. Restrict to self-owned fixture; do not claim Windows APIs are implemented.
2. **PE TLS and callbacks**: well-formed TLS directory with relocation of VA fields, callback order, static per-thread template indexing and thread isolation.
3. **Unwind**: PE .pdata/.xdata and x64 runtime function lookup; deliberate exception across a Microsoft/System V boundary.
4. **TEB/GS and startup**: explicitly verify GS base handling, per-thread TEB and process initializers.
5. **Fuzz and harden parser** before exposing third-party files. Add AFL/libFuzzer *parse-only* mode that never executes parsed image code; avoid treating negative tests as a security guarantee.
6. **Cross-compiler Windows baseline**, source/toolchain pinning and complete CI matrix.
7. **Performance experiment**: only after correctness, compare memory-mapping/relocation/startup latency to controlled native Windows and Linux baselines. No speed claim now.

## Primary sources

- Microsoft, [PE Format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) — optional headers, section table, reloc directory, IMAGE_REL_BASED_DIR64=10, machine 0x8664, PE32+ magic 0x20b
- Microsoft, [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention)
- LLVM, [lld Windows support](https://lld.llvm.org/windows_support.html)
- Linux, [mmap](https://man7.org/linux/man-pages/man2/mmap.2.html) / [mprotect](https://man7.org/linux/man-pages/man2/mprotect.2.html)

## Environment and claims policy

Source/build files are in the repository. The passing results above were reproduced in the research container; no GitHub-hosted CI has independently reproduced them yet. The PE image is disposable build output and intentionally not committed. Future agents should **re-run make test** in their own environment; they must not assume identical results on other hosts.
