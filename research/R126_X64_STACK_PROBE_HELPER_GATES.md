# R126 — x64 stack-probe helper imports and guest stack-growth boundary

Date: 2026-11-16. Status: **primary-source documentation review; no experiment executed**.

## Why this is separate from R65

R65 covers reserved/committed stack growth, guard faults, and Linux signal delivery. This note covers a distinct **compiler-generated code dependency**: a Windows-target PE function may call a stack-probing helper before changing its stack pointer. A PE loader that resolves only explicitly written application calls can fail before any NT API is invoked.

## VERIFIED — published primary contracts (not observed behavior)

1. Microsoft's `__chkstk` routine documentation says it is a compiler helper called when a function has sufficiently large local variables; it gives an **8 KiB x64 compiler threshold** (and 4 KiB x86 threshold). It identifies a `kernelbase.dll` export. This describes the Microsoft compiler documentation, **not a universal threshold for Clang, GCC, or every compiler flag**. [Microsoft — __chkstk Routine](https://learn.microsoft.com/en-us/windows/win32/devnotes/-win32-__chkstk).
2. Microsoft's Arm64EC ABI documentation explicitly explains the purpose of probing: large stack allocations must not jump past the stack guard page. It also says stack checking uses a **custom calling convention**, and that x64 and Arm64EC have distinct helper implementations (`__chkstk` and `__chkstk_arm64ec`). Do not infer the x64 helper's exact register-level contract from the Arm64EC helper's register description. [Microsoft — Understanding Arm64EC ABI and assembly code, Stack Checkers](https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi).
3. Microsoft's x64 ABI documentation requires correct prolog/epilog and unwind metadata for functions that allocate stack, call functions, save nonvolatile registers, or use exception handling. Stack-probe insertion must therefore be considered together with, but not substituted for, the R30/R120 unwind requirements. [Microsoft — x64 ABI conventions](https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions).
4. Microsoft documents Windows thread stack reserve/commit distinctions; helper availability alone cannot provide the underlying Windows stack-growth behavior. [Microsoft — Thread Stack Size](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-stack-size).

## HYPOTHESIS — architecture consequence

A Windows x64 fixture compiled with a deliberately large frame may carry an unresolved helper import or linker-provided helper implementation. The Linux-hosted PE runtime must classify the helper's actual linkage (import table, statically linked code, or other compiler/linker mechanism), provide the appropriate **guest ABI** when needed, and coordinate probing with the guest stack reservation/commit model. Replacing the helper with a no-op would risk skipping the guest guard boundary and must not be treated as a correctness-preserving shortcut.

The precise x64 register contract, side effects, code generation threshold, symbol decoration and import provenance **must be verified for each compiler/toolchain version** rather than guessed from a generic ABI summary.

## Falsifiable tests — NOT RUN

1. Pin MSVC and LLVM/LLD versions; build freestanding x64 PE fixtures with frame sizes below, at, and above the documented threshold, with optimization variants. Record disassembly, import descriptors, helper symbol resolution and unwind metadata. Do not assume compiler output before inspecting it.
2. On a pinned Windows build, run the fixtures on threads with deliberately small reserve/commit configurations, recording `VirtualQuery` transitions, guard behavior, and exact exception statuses. Use isolated subprocesses and timeouts for overflow cases.
3. On Linux, first add **parse-only** import and unwind assertions. Then use a trusted PE fixture with a tracked guest stack mapping, comparing observed probe accesses and fault order against Windows. This is a future experiment, not a reported result.
4. Negative controls: missing helper import, intentionally incorrect helper ABI, probe crossing more than one page, near-exhausted guest stack, and nested signal handling. Ensure no host runtime stack is accidentally classified as a guest stack.
5. Cross-reference R19/R20 (PE execution/import binding), R30 (unwind), R55 (ABI preservation), R65 (stack growth), R108 (signal ownership) and R120 (context continuation) when designing the integrated test.

## Decision gate

Do **not** claim general Windows x64 code execution merely because a small-frame PE fixture runs. A minimum runtime milestone should demonstrate a real Windows-target compiler-generated large-frame function with correctly resolved helper linkage, preserved registers, correct guest stack growth/failure behavior, and valid unwind state.

**No fixture was built, no Windows oracle was run, no Linux execution occurred, and no benchmark was performed for R126.**
