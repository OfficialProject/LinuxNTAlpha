# R55 — Callee-saved SIMD state across Windows x64 / Linux SysV transitions

Date: 2026-11-13. Status: **primary-source specification review only**; no new code or experiment.

## Narrow question

Can a Linux-hosted Windows x64 import/callback bridge preserve the machine state expected by an arbitrary Windows PE caller when execution crosses into a Linux System V function, including when a native helper uses vector instructions?

This note extends [R2](R2_WINDOWS_ABI.md) and [R17](R17_ABI_BRIDGE_EXPERIMENT.md), which establish the basic ABI contract and verify argument passing but **explicitly do not test nonvolatile SIMD preservation**. It does not repeat [R30](R30_X64_UNWIND_TABLE_REGISTRATION.md)'s unwind registration survey.

## Verified contracts (documentation, not measurements)

1. **Windows x64 callee preservation.** Microsoft's register-usage specification lists XMM6–XMM15 as nonvolatile for their low 128 bits; XMM0–XMM5 are volatile. RDI and RSI are also nonvolatile in the Windows x64 convention. Consequently, a Windows caller is entitled to retain these registers across an ordinary conforming call. Source: [Microsoft x64 software conventions](https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions).
2. **ABI selection is a function-type property.** GCC documents `ms_abi` and `sysv_abi` attributes on x86, including Linux targets. A plain SysV function-pointer type is not a substitute for an MS-ABI declaration. Source: [GCC x86 function attributes](https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html).
3. **Windows unwind metadata can describe SIMD restoration.** Microsoft's x64 exception-handling specification defines `UWOP_SAVE_XMM128` and `UWOP_SAVE_XMM128_FAR` to record saved nonvolatile 128-bit XMM state. Successful return-path register preservation is therefore **not** equivalent to valid unwind through a bridge. Source: [Microsoft x64 exception handling](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64).
4. **GCC's blanket preservation attribute is not a shortcut for arbitrary vector state.** GCC documents limitations of `no_caller_saved_registers`, including inability to preserve YMM/ZMM when AVX is enabled. That attribute is not evidence of correct Windows x64 interoperability. Source: [GCC x86 function attributes](https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html).

## Architectural deduction (not experimentally verified)

A bridge that merely shuffles RCX/RDX/R8/R9 into SysV argument registers could appear correct for integer-only smoke tests while allowing a SysV callee to overwrite XMM6–XMM15 that the PE caller expects to survive. The safest first implementation candidate is a **compiler-generated MS-ABI entry stub** that makes a separately typed SysV call; verify its generated code and observable state rather than assuming compiler support covers every signature. If hand-written assembly is introduced, its preservation and unwind obligations must be independently audited.

This is a **candidate design**, not a claim that any existing LinuxNTAlpha bridge currently corrupts registers: R17's tested GCC bridge did not measure that behavior.

## Proposed reproducible acceptance gates — NOT RUN

1. Build a Windows-target freestanding PE fixture that seeds distinct 128-bit sentinel patterns in XMM6 through XMM15, calls an imported MS-ABI stub, and verifies all ten lower lanes after return. Have the Linux-hosted SysV callee deliberately overwrite the same registers (with compiler constraints/assembly to prevent optimizer elision). Keep the PE test self-checking and return a unique error code per mismatched register.
2. Independently seed RDI and RSI and verify their preservation across both directions; compare against a deliberate negative-control bridge that omits preservation. The negative control must run in a dedicated process so corruption cannot contaminate unrelated tests.
3. Repeat with `-O0`, `-O2`, `-O3`, and available GCC/Clang versions. Record disassembly, compiler flags, CPU features, process exit status, and all SKIP cases. Do not extrapolate to untested compilers.
4. Separate **normal return** from **exceptional unwind**: generate a PE function with nontrivial `.pdata/.xdata`, inspect its unwind records, and only claim unwind conformance after a Windows-reference oracle verifies restoration at selected instruction offsets. Do not attempt to infer Windows SEH correctness from a Linux signal handler alone.
5. Test callbacks in both directions, including a native helper that invokes a PE callback before returning. Include vector-heavy native helper variants. Any AVX/AVX-512 upper-lane policy must be evaluated against the exact target ABI and feature set; do not assume 512-bit preservation because 128-bit XMM state is nonvolatile.
6. Add signal-interruption tests **separately** from call-return tests, with per-thread GS/TIB and Linux FS/TLS state checks as in R24/R25. A passing signal test is not proof of Windows exception dispatch.

## Decision gate

**OPEN:** Until preservation, unwind and callback probes pass on an actual PE-to-Linux boundary, the runtime must not advertise general Windows x64 ABI compatibility. Prioritize a deterministic register-preservation PE fixture before performance optimization or broad import coverage.

## Source provenance

Primary documentation consulted 2026-11-13. URLs above point to Microsoft Learn and GCC upstream manuals. No Windows reference host, CPU benchmark, compiler output, or new execution was inspected in this research entry.
