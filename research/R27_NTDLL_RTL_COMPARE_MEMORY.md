# R27 — First independently implemented Windows NTDLL export invoked by a PE32+ image

Date: 2026-11-12. Status: **REPRODUCED** on Linux x86-64 research container, with 8 direct host unit cases and 5 genuine PE32+ import calls. This is a single, narrow Windows user-mode API, **not** a general Windows-compatible ntdll/runtime.

## Question

Can a Windows-target PE64 program import a **real documented NTDLL function** by its actual DLL/name and receive matching behavior through a new Linux-native function obeying Microsoft's x64 calling convention? This extends R20's artificially named probe.dll!add_five stub to an actual Windows interface contract.

## Primary evidence

- Microsoft [RtlCompareMemory](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlcomparememory), last updated July 16, 2025, specifies Source1, Source2, Length and SIZE_T return: **number of equal bytes from the start of the two blocks**, stopping at first mismatch or length. Its Requirements table explicitly lists **NtDll.dll (user mode)**.
- Wine's independent [dlls/ntdll/ntdll.spec](https://github.com/wine-mirror/wine/blob/master/dlls/ntdll/ntdll.spec) contains RtlCompareMemory(ptr ptr long), git blob SHA **14608fd3f4d363322f1eff5a95689ecbd7374670**.
- ReactOS' [dll/ntdll/def/ntdll.spec](https://github.com/reactos/reactos/blob/master/dll/ntdll/def/ntdll.spec) lists the same export, blob SHA **034aa75190abce8c8f64021dc84633c51030c40c**.
- Microsoft [PE/COFF format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) defines import descriptors and IAT; [Windows x64 ABI](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention) defines the RCX/RDX/R8 and stack contracts.

## Implementation

1. Add **ntdll_minimal.h/.c**. The original Linux ELF-hosted lnta_RtlCompareMemory is declared with ms_abi and returns size_t. It compares byte-by-byte and returns the matching prefix length. No code was copied from Wine, ReactOS or Windows; this is an original straightforward implementation following Microsoft's public specification.
2. Add **ntdll_minimal_test.c**, testing 8 calls: first-byte mismatch, mid mismatch, final mismatch, full match, zero length, short prefix and zero-length NULL pointer arguments.
3. Add **ntdll_stub.c** solely for creating a Windows COFF ntdll.lib via Clang + LLD; resulting fake ntdll.dll is **not executed or loaded by Linux**.
4. Add **ntdll_fixture.c**, a Clang x86_64-pc-windows-msvc image importing exactly ntdll.dll!RtlCompareMemory. Two static byte blocks differ at index 3. Loader's explicitly allowlisted IAT slot binds to the ELF host MS-ABI function.
5. Windows-targeted PE code calls the real-named imported routine and returns 42 only if matching prefix is 3. Global data pointers cause DIR64 relocations. Compiler flags intentionally suppress unwind metadata for this narrow fixture, **not** a Windows exception implementation.
6. Other DLL/name pairs remain rejected; two negative tests mutate the NTDLL name and function name and require explicit REJECT without PE execution.

## Reproduced results

Research container: Linux 6.18.57 x86-64 Debian GNU/Linux 13, GCC 14.2, Clang/LLD 17. Command: **make -C experiments/pe64-image test**.

**Unit test:** printed PASS: 8 RtlCompareMemory cases under MS x64 ABI.

**PE fixture:** five invocations returned entry code 42 with two DIR64 relocations, imports_patched=1 and a relocated image base distinct from preferred 0x140000000.

**Full Python test:** nine test methods, including five invocations each of the original no-import, synthetic add_five IAT, GS TIB and Syscall User Dispatch PE fixtures, plus the new RtlCompareMemory fixture; 26 previous known malformed-image/import cases, two extra NTDLL import-name rejections, parse-only acceptance and 200 deterministic mutation variants. All locally passed at GCC -O2.

The original synthetic probe.dll import remains available, making it easy to distinguish the fake API demonstration from this real named export.

## Scope and limitations

**REPRODUCED:** a Linux-native function can implement the documented behavior of one real Windows NTDLL export, bind through a real PE64 IAT slot and be called from Windows-compiled machine code using the Microsoft x64 ABI. This is **one small compatible routine**, not Windows or game compatibility.

**NOT established:** complete NTDLL, process startup, real TEB/PEB, Windows handles, SEH, NT syscalls, import search/forwarding, DLL lifecycle, app compatibility, security isolation or any performance advantage. The host routine assumes input memory is valid, which is inadequate for running untrusted PE bytes without isolation. Synthetic ntdll.dll is not actually loaded. No Windows-hosted golden baseline or independent API fuzzing was done.

## Next gates

1. Obtain genuine Windows baseline outputs across length/pointer boundary cases, with controlled fixtures.
2. Generalize import parsing in **parse-only** mode before allowing additional Windows exports, ordinals and API sets; fuzz parser without executing unknown code.
3. Implement a coherent and independently tested minimal NTDLL subset rather than accumulating disconnected symbols; use typed ABI stubs and explicit lifecycle contracts.
4. Establish TEB/PEB, NTSTATUS, TLS and loader state; never infer compatibility from memory function calls alone.
5. Benchmark representative mixed-ABI calls only after correctness, and keep R21 third-party license gates.

No permanent userspace/kernel architecture choice is made.
