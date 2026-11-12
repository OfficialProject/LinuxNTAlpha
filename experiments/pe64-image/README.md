# PE32+ native code execution experiment (Linux x86-64)

**Purpose:** Prove tightly controlled Windows-targeted x86-64 PE32+ images can be parsed, relocated and executed **directly** in a Linux process, and that one synthetic imported function can be called through the image's real Import Address Table (IAT), without Wine, Proton, virtualization or CPU emulation. This is NOT a general-purpose Windows loader, a launcher for downloaded applications, a security sandbox, or an NT subsystem.

### Reproduce

Dependencies: x86-64 Linux with 4 KiB pages; GCC/Clang C compiler, **clang** (Windows MSVC COFF target), **lld-link**, Python 3 and Make. No Windows SDK or CRT is used.

Run **make test** in this directory. To rerun from clean state: **make clean && make test**.

**Modes:** the executable now requires an explicit choice:
- **./pe64_probe --parse-only fixture.exe** validates/maps the image but does NOT enter its machine code.
- **./pe64_probe --execute-trusted-fixture fixture.exe** intentionally calls the entry point of a self-generated trusted fixture.
- **./pe64_probe --execute-synthetic-gs-fixture gs_fixture.exe** saves the host thread's GS, installs a temporary TIB-shaped test context with Self at GS:0x30, executes a Windows-target PE fixture that reads GS and uses the synthetic imported function, then restores GS. Returns SKIP (77) if arch_prctl GS support is unavailable.
- **./pe64_probe --execute-synthetic-sud-fixture sud_fixture.exe** enables Linux Syscall User Dispatch, enters a freestanding Windows-target PE whose NT-*style* raw syscall uses the intentionally artificial service number 39 (which collides with Linux getpid), traps SIGSYS, returns a synthetic value and restores the signal handler/dispatch state. Returns SKIP (77) if SUD is unavailable. This is **not a genuine Windows NT syscall**.

Do not use either execution mode with third-party or downloaded executables.

All mutated/invalid cases use parse-only mode. The actual-entry-point tests use the explicit trusted-fixture execution mode only.

All four PE fixtures are compiled by clang for **x86_64-pc-windows-msvc**, linked by lld-link with /nodefaultlib, /dynamicbase, /entry:win_entry, /subsystem:console and /nxcompat. The first fixture includes a global pointer to a global counter so a live IMAGE_REL_BASED_DIR64 relocation is required at a different base. The second fixture additionally imports **probe.dll!add_five** by name, via a generated import library. A Linux-native function with Microsoft x64 ABI is bound into the IAT; the built probe.dll is **not** loaded or executed. The Windows-ABI function reads the pointer and returns 42. The Linux launcher calls the entry with a compiler-declared Microsoft x64 ABI function pointer.

The probe deliberately supports only this narrow image family:
- AMD64 PE32+, valid DOS/PE signatures and COFF/optional headers
- 4 KiB section alignment and 512-byte–64 KiB power-of-two file alignment
- <=96 ordered, page-disjoint image sections; valid raw file and virtual extents
- a nonempty base relocation directory and the ABSOLUTE/DIR64 relocation types
- either no imports **or exactly one DLL descriptor named probe.dll with one named import add_five and a 16-byte IAT**; all other imports, TLS, exceptions, delay imports, security, CLR or other data directories are unsupported
- no writable and executable section; image mapped writable for relocation, then protected per section before the PE entry runs
- entry must reside in an executable section; execution is permitted only for the **two trusted fixtures**
- image <=64 MiB, input file <=16 MiB, page size exactly 4096
- no Windows process startup, PEB/TEB, GS TLS, DLL loader, SEH, Win32 calls, system calls, memory-mapped NT sections, APCs or threads

The trusted freestanding fixtures are inherently artificial. A normal Windows executable almost always requires many imports or runtime startup support, and should be rejected. The imported test DLL exists **only** as a linker input; this tool never loads it. A third fixture, gs_fixture.exe, executes a real GS:0x30 access from Windows-target machine code and validates an explicitly synthetic TIB and cookie (R25). A fourth fixture, sud_fixture.exe, issues a raw NT-*style* syscall and confirms Linux SUD interception, without implementing Windows system services (R26).

### Results, recorded 2026-11-12

Host: Debian GNU/Linux 13 (trixie) x86-64 container on Linux 6.18.57. GCC 14.2.0, clang/LLD 17 family toolchain. The fixture has PE machine AMD64, optional header PE32+, preferred base 0x140000000, entry RVA 0x1000, SizeOfImage 0x4000, and DIR64 relocation directory RVA 0x3000, length 12. LLVM objdump independently shows no import, TLS or exception directories.

**PASS:** five invocations of each of the four fixture types returned 42 and applied at least one DIR64 relocation; the original no-import fixture mapped at an ASLR address different from its preferred base. The import fixture additionally reported **imports_patched=1** and invoked the Linux host function via a PE IAT. The Python suite passed **19 header/relocation corruption cases** and **seven import corruption cases**, each explicitly rejected without entering PE code; it also exercised **200 seeded bit-flipped/truncated fixture variants** in parse-only mode without crashing. The suite now has eight methods.

**PASS:** both fixtures and 26 negative cases under GCC -O0, -O2 and -O3, plus clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer (tests instrumenting the **launcher**, not the Windows-target PE). Build from clean state also passed.

This is evidence of **four small relocatable PE64 functions executing natively, two via a synthetic IAT import, one reading GS:0x30, and one issuing a deliberately intercepted raw syscall**, not production Windows application compatibility or a validated general PE parser. The negative-test set is not an exhaustive fuzzing effort, and this experiment is **not safe for untrusted PE files**. PE bytes run with the full privilege of the launching Linux process. Do not run third-party binaries using this tool.

### Primary sources

- Microsoft PE/COFF specification: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- Microsoft Windows x64 ABI: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- LLVM/LLD COFF linker: https://lld.llvm.org/windows_support.html
- Linux mmap/mprotect: https://man7.org/linux/man-pages/man2/mmap.2.html and https://man7.org/linux/man-pages/man2/mprotect.2.html

See [R19](../../research/R19_PE64_EXECUTION_EXPERIMENT.md) for the initial no-import result and [R20](../../research/R20_PE64_IAT_BRIDGE.md) for the import binding result, architecture implications and next acceptance gates.

[R23](../../research/R23_PARSE_ONLY_FUZZ_GATE.md) records the explicit non-executing mode, deterministic mutation tests and their narrow limitations. [R25](../../research/R25_PE64_GS_TIB_EXECUTION.md) records the GS-aware Windows-target PE fixture and its limits. [R26](../../research/R26_PE64_RAW_SYSCALL_SUD.md) records the Windows-target raw syscall SIGSYS trap, without any real NT system services.
