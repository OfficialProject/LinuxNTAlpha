# R185 — Windows x64 stack-probe helper and guest stack-growth integration gates

Date: 2026-11-19. Status: **primary-source documentation and upstream code reviewed; no experiment run**.

## Scope and nonduplication

R65 covers Windows reserved/committed stack regions, PAGE_GUARD and Linux fault delivery; R30 covers x64 unwind metadata; R55 covers cross-ABI register preservation. This note isolates the **compiler-generated x64 stack-probe call**, its import/runtime symbol, and the requirement to integrate page-touch behavior with the guest stack-growth implementation. It does not repeat R65's general stack architecture.

## VERIFIED — primary contracts and source

1. Microsoft's `__chkstk` documentation describes a compiler helper for large stack locals; it specifies an x64 threshold of **8 KiB** and says the helper is exported from **kernelbase.dll**. This threshold is a documented compiler invocation condition, **not** a claim that Windows memory pages are 8 KiB or that every compiler/version emits identical calls. [Microsoft — __chkstk Routine](https://learn.microsoft.com/en-us/windows/win32/devnotes/-win32-__chkstk).
2. Microsoft's x64 ABI documentation requires unwind-described prologs for functions that allocate stack space or call other functions. Stack-probe integration must therefore preserve the PE's unwind and calling conventions, not simply provide a symbol with the same name. [Microsoft — x64 ABI conventions](https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions); [Microsoft — x64 exception handling](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64).
3. Microsoft's Arm64EC ABI document explicitly describes `__chkstk` as touching allocated pages so a stack allocation cannot skip the guard page. It warns that x64 and Arm64EC use distinct helpers and calling conventions. **This is explanatory cross-architecture evidence, not an x64 implementation specification.** [Microsoft — Arm64EC ABI, Stack Checkers](https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi).
4. LLVM's historical x86-64 compiler-rt implementation of `___chkstk_ms` touches descending 4096-byte intervals and preserves the incoming stack pointer rather than allocating the frame itself. Its comments explicitly distinguish MSVC x64 `__chkstk` and MinGW `___chkstk_ms`. This is **a particular LLVM implementation**, not proof of current Windows kernelbase internals or all compiler symbol spellings. The referenced repository is archived; inspect a pinned revision before reuse. [LLVM compiler-rt archived x86_64/chkstk.S](https://github.com/llvm-mirror/compiler-rt/blob/master/lib/builtins/x86_64/chkstk.S).
5. Microsoft's `/GS` buffer-security cookie is a **different feature** from `__chkstk` stack probing. Neither can substitute for the other. [Microsoft — /GS](https://learn.microsoft.com/en-us/cpp/build/reference/gs-buffer-security-check).

## HYPOTHESIS — runtime boundary

A freestanding PE fixture with a deliberately large x64 stack frame may import `__chkstk` (or link a compiler-specific helper) before ordinary Windows API dependencies. A Linux-hosted NT runtime would need to resolve the actual fixture's symbol, honor its compiler-specific calling convention, and ensure page touches interact with **guest** reserved/committed/guard stack state. Merely linking a host-side C function that adjusts `rsp` is unsafe: stack probing and frame allocation are distinct operations in the inspected LLVM x64 helper.

A proposed implementation would treat the guest stack-growth ledger (R65) as authoritative and dispatch probe-triggered faults through a tightly controlled guest-fault path. It must not classify an ordinary host-stack fault as guest growth. This design is **not implemented or tested**.

## Falsifiable tests — NOT RUN

1. Build minimal Windows x64 PE32+ fixtures using **recorded compiler and linker versions** (MSVC and Clang/LLD separately). Generate stack frames near 4 KiB, 8 KiB, 12 KiB and 64 KiB, plus dynamic `_alloca` cases. Inspect disassembly, import tables, and unwind records before any execution. Record actual helper spelling, calling sequence and stack adjustments rather than assuming the Microsoft documentation's threshold holds for every toolchain.
2. On a pinned Windows reference build, log stack limits and page state around successive large-frame calls; compare successful growth, deliberate guard-page skipping, reserve exhaustion and exception codes. Isolate destructive cases in disposable child processes with timeouts.
3. On Linux, add an **explicitly trusted fixture** to the existing PE harness (R19/R20) and an independently implemented helper only after import and ABI observations. Test register preservation, return address, stack alignment, unwind metadata, nested probes and cross-thread independence.
4. Negative cases: missing helper import; helper invoked on a host stack; probe landing beyond the reserved region; multiple guard boundaries skipped; signal handler fault; guest stack exhaustion while an NT service is in progress; deliberate guest corruption of the probe target.
5. Validate both **correct execution and exact failure semantics** against Windows. Measure latency only after correctness. Do not extrapolate to arbitrary Windows apps.

## OPEN

- What exact helper symbol and prolog sequence do current MSVC and Clang Windows x64 targets emit at each frame size and optimization setting?
- Can a guest stack-growth fault be resumed safely without violating R108/R164 signal ownership and alternate-stack continuation rules?
- What differences arise between PE main-thread, CreateThread-created, and fiber stacks?
- How will the runtime prevent a guest stack probe from reading/writing outside its owned mapping under concurrent address-space changes?

No Windows reference runs, Linux fixture runs, measurements, or implementation are claimed.
