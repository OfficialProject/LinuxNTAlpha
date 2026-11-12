# R2 — Windows x64 ABI and Linux execution boundary
Date: 2026-11-12. Status: specification audit, not a working implementation.

## Primary references
- https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- https://learn.microsoft.com/en-us/cpp/build/x64-exception-handling
- https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- https://man7.org/linux/man-pages/man2/arch_prctl.2.html

## Verified contract
Microsoft x64 uses RCX, RDX, R8, R9 for the first four integer/pointer parameters and XMM0–XMM3 for floating-point parameters by position; caller reserves 32 bytes of shadow space; stack alignment and nonvolatile register preservation differ from System V AMD64. A function pointer cast alone does not bridge the ABIs. On x64, PE .pdata/.xdata describe stack unwinding for non-leaf functions. PE TLS has per-module templates, an index, and callbacks; Windows x64 code accesses thread data through GS-based TEB. Linux x86-64 generally uses FS for native TLS, but a bridge must verify actual runtime behavior rather than assume compatibility.

## Architectural implications
1. Define an explicit ABI bridge for each native-to-Windows and Windows-to-native call boundary, including integer/floating point/variadic calls and unwind boundaries.
2. Keep Windows GS thread context distinct from Linux FS TLS. Do not assume arbitrary GS state changes are safe across signals or thread transitions.
3. Treat process initialization as a sequence: validate image; reserve/map; relocate; load dependencies; initialize per-process/per-thread state; initialize TLS; call initializers; enter entry point; handle shutdown.
4. Never claim Windows exceptions work merely because Linux signals can catch faults; Windows unwind, vectored exceptions, and structured handlers need a distinct compatibility design.
5. Use compiler ABI attributes or verified assembly trampolines; write cross-ABI conformance tests before loading complex programs.

## Open questions
- Linux signal delivery/restore behavior with GS base and asynchronous faults on target kernels.
- Loader-lock and DLL initialization order requirements.
- Per-thread TEB lifetime and fork/clone semantics.
- Full unwind interoperability with DWARF/ELF native frames.
- Windows API coverage and syscall numbering differences across Windows versions.

## Prototype acceptance tests
- Cross-ABI calls for 0–8 integer arguments, mixed FP arguments, stack alignment, nonvolatile registers.
- GS-backed TEB read and thread isolation.
- TLS callback order and per-thread data separation.
- Deliberate faults and controlled exception translation, with no silent swallowing.
- Clean thread exit and process exit.
