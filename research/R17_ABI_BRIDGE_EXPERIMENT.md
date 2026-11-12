# R17 — Compiler-managed Windows x64/System V call bridge

Date: 2026-11-12. Status: small cross-ABI executable smoke test **PASSED** on one Linux x86-64 container; no Windows PE binary loaded.

## Problem and primary sources

Microsoft documents the 64-bit calling convention: four positional register argument slots; RCX/RDX/R8/R9 for integer arguments, XMM0–XMM3 for floating arguments according to position; 32-byte caller-allocated shadow store; fifth and later arguments on the stack; distinct callee-preserved GPRs and XMM6–XMM15. Source: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention ; https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions .

GCC explicitly supports x86 function attributes ms_abi and sysv_abi, allowing the compiler to synthesize ABI-compliant calls on a GNU/Linux ELF build. Source: https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html . This is useful as a known-good low-risk bridge candidate but not necessarily sufficient for arbitrary imported PE functions, exception unwinding or untrusted pointers.

## Reproduced experiment

Source and instructions: [experiments/abi-bridge](../experiments/abi-bridge/).

**Host:** x86-64 Linux 6.18.57 container, Debian 13, GCC 14.2.0.

**Code behavior:**
- System V main calls Microsoft-ABI zero-argument function.
- System V main calls Microsoft-ABI eight-integer function through an explicitly annotated function pointer, testing arguments passed beyond the four Windows register slots.
- Microsoft-ABI function calls a System V eight-integer function, testing the reverse bridge.
- System V calls a Microsoft-ABI mixed integer, double and float function, including positional register selection and stack arguments.
- Microsoft-ABI mixed-argument function calls a System V mixed-argument function.
- All observed values are compared to separately computed expectations; failed checks cause nonzero exit.

Compiled with -std=c11 -Wall -Wextra -Werror and optimization levels -O0, -O2 and -O3. Each of the three executable variants returned exit 0 and printed:

    PASS expected=954 windows=954 reverse=954 zero=2882395322 mixed=25.50 reverse_mixed=25.50

**What this supports:** a compiler-generated x64 ABI bridge can correctly pass the selected function signatures across both ABIs in this environment. It is direct machine-code execution without instruction-set emulation, as expected on a shared x86-64 ISA.

## Limits and pitfalls

This proof is much narrower than arbitrary Windows compatibility: both callers and callees were produced by GCC into one Linux ELF binary. It does not load PE/COFF, implement Windows imports, establish a Windows TEB at GS, handle SEH or demonstrate Windows DLL execution. It does not test callbacks from external code, C variadics, by-reference aggregates, vector register preservation, unusual stack probes, longjmp, stack unwinding or asynchronous signals while in a cross-ABI frame. The test uses one compiler and Linux configuration only.

**Architecture hypothesis:** prefer compiler-aware ms_abi/sysv_abi stubs for known signatures before considering custom assembly. For dynamically generated bridge code, construct a vetted signature description and test ownership/lifetime, pointer safety and unwind compatibility. Never cast an unannotated function pointer and assume its calling convention is correct.

## Next acceptance gates

1. Add naked/assembly controlled fixtures checking 32-byte shadow store, stack alignment and 5th–8th stack argument locations.
2. Add callee-preserved GPR and XMM6–XMM15 register tests, plus variadic floating-point duplication requirements.
3. Build a minimal PE64 fixture with an MSVC-compatible or mingw-w64 toolchain and call it from a Linux-native host; inspect binary sections/imports rather than silently depending on GCC's ELF ABI handling.
4. Exercise callback paths in both directions and asynchronous signal arrival.
5. Validate unwind behavior and cancellation at ABI boundaries independently of successful ordinary returns.
6. Build cross-compiler comparison matrix and tests in CI.

No performance claim or full Windows ABI compliance claim is made.
