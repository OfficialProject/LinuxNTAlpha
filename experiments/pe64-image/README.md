# Narrow PE32+ execution and NTDLL import experiments (Linux x86-64)

**These are research fixtures, NOT a Windows loader for arbitrary programs and NOT a sandbox.** The loader executes x86-64 PE machine code with the same OS privileges as its Linux host; never run untrusted/downloaded PE files through the execution modes.

## Reproduce

Requires x86-64 Linux with 4 KiB pages, GCC/Clang, Clang's x86_64-pc-windows-msvc compilation target, lld-link, Make and Python 3. No Windows SDK/CRT, Wine or Proton runtime dependency. Run **make clean && make test** here, or **make -C experiments test** from the repository root.

The binary has four **explicit** modes:

- **--parse-only file.exe**: validate, map, relocate and protect the image; never enter PE executable code. This is still not a secure parser for untrusted data.
- **--execute-trusted-fixture file.exe**: enter one of the repo's self-generated fixtures and check the entry returns 42. Never use with a third-party PE.
- **--execute-synthetic-gs-fixture gs_fixture.exe**: temporarily install a synthetic TIB-shaped per-thread GS base, call the PE entry that reads GS:0x30, and restore the original GS base. It returns SKIP (77) if ARCH_SET_GS is unavailable.
- **--execute-synthetic-sud-fixture sud_fixture.exe**: intercept one raw x86-64 syscall instruction originating from the Windows-compiled PE using Linux Syscall User Dispatch (SIGSYS), return a synthetic value, and restore the signal handler and dispatch mode. It returns SKIP (77) if SUD is unavailable. The service number 39 is intentionally the Linux getpid number, **not a genuine Windows NT service ID**.

## Five freestanding Windows-target PE fixtures

1. **fixture.exe**: no imports, relocation-bearing pointer, MS-ABI entry returns 42 when relocated correctly.
2. **import_fixture.exe**: imports **probe.dll!add_five**, then calls a synthetic Linux-hosted function through an actual PE IAT slot.
3. **gs_fixture.exe**: also calls probe.dll!add_five but verifies synthetic GS:0x30 TIB self pointer and test-only cookie. This is **not a real Windows TEB/PEB**.
4. **sud_fixture.exe**: executes Windows-targeted machine instructions loading a chosen service number in EAX and issuing syscall; the Linux SIGSYS handler captures the instruction and substitutes a result. **No real NT syscall is implemented.**
5. **ntdll_fixture.exe**: imports the **actual documented ntdll.dll!RtlCompareMemory** symbol by name and calls the original host implementation lnta_RtlCompareMemory (MS-ABI), which returns the number of matching bytes until first difference. A fake ntdll.dll is built **only to make ntdll.lib for linking**; no Windows or copied Wine/ReactOS DLL is loaded.

LLVM/LLD links the PE images with /nodefaultlib, /dynamicbase, /nxcompat and a custom entry. ntdll_fixture explicitly disables compiler-emitted unwind metadata to meet this research loader's restrictive PE format acceptance rules; it does not simulate Windows exception unwinding.

## Format limitations and safety boundaries

The loader requires AMD64 PE32+, valid signatures and PE32+ optional headers, exactly 4 KiB section alignment, bounded ordered page-disjoint sections, relocatable image with nonempty DIR64 relocations, no W+X section, and an executable entry RVA. It supports **either no imports or exactly one import descriptor containing one named function**: probe.dll!add_five OR ntdll.dll!RtlCompareMemory. All other data directories (TLS, exceptions, delay imports, CLR etc.) are rejected. The file is capped at 16 MiB, mapped image at 64 MiB, and the test requires 4 KiB Linux pages.

Normal Windows apps use more imports, DLL lifecycle, Win32 and NT services and metadata. Such programs are **not supported**. A syntactically accepted image is not necessarily safe to run. --parse-only still performs memory mapping and pointer processing in-process and is not fuzz-hardened or isolated.

## Verified results — 2026-11-12

Local execution environment: Debian GNU/Linux 13 x86-64 container running Linux 6.18.57, GCC 14.2.0, Clang/LLD 17 family.

- **PASS:** five invocations of **each of the five** PE fixtures return entry value 42, apply relocations, and for three import-dependent fixtures patch one PE IAT slot. SUD fixture reports exactly one expected SIGSYS and 0 unexpected traps; GS fixture reads a temporary TIB-like context.
- **PASS:** 8 direct host tests for the independently written RtlCompareMemory implementation under Microsoft's x64 calling convention, covering mismatch location, complete match, prefix length and zero-length inputs.
- **PASS:** 9 Python unittest methods. They include the trusted execution cases, 19 original malformed-image cases, 7 original malformed synthetic-import cases, 2 malformed real-NTDLL-import cases, and a **200-seed deterministic mutation smoke test** that only uses parse-only mode. The untrusted-byte parser has not received coverage-guided fuzzing.
- **PASS:** GCC -O0/-O2/-O3 and Clang -O1 with ASan+UBSan for the Linux host loader/unit tests. PE code is **not instrumented by host sanitizers**.
- **PASS in GitHub CI:** GitHub Actions workflow exercises the complete experiment suite on an independent Ubuntu 24.04 x86-64 runner. NTSYNC subtest SKIPs when no kernel header/device is available; this does not signify NTSYNC kernel correctness.

These are **deliberately bounded experimental results**, not application compatibility, speed claims, reliable NT semantics, Windows kernel driver support, anti-cheat compatibility or a secure executable loader.

## Primary sources and research notes

- Microsoft PE/COFF: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- Microsoft x64 ABI: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- Microsoft RtlCompareMemory: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlcomparememory
- Linux Syscall User Dispatch: https://docs.kernel.org/admin-guide/syscall-user-dispatch.html
- Linux FS/GS: https://docs.kernel.org/arch/x86/x86_64/fsgs.html
- LLVM/LLD: https://lld.llvm.org/windows_support.html

Research: [R19 PE entry](../../research/R19_PE64_EXECUTION_EXPERIMENT.md), [R20 IAT](../../research/R20_PE64_IAT_BRIDGE.md), [R23 parse-only](../../research/R23_PARSE_ONLY_FUZZ_GATE.md), [R25 GS](../../research/R25_PE64_GS_TIB_EXECUTION.md), [R26 SUD](../../research/R26_PE64_RAW_SYSCALL_SUD.md), and [R28 real NTDLL import](../../research/R28_NTDLL_RTL_COMPARE_MEMORY.md).
