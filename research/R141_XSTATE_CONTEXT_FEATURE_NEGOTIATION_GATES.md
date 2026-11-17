# R141 — x64 extended processor state and Windows CONTEXT/XSTATE gates

Status: **DOCUMENTATION-BASED ARCHITECTURE RESEARCH; NO EXPERIMENT**  
Reviewed: 2026-11-17  
Scope: x86-64 Windows guest code executed directly in Linux processes. Complements R55 (cross-ABI SIMD), R109 (CET), and R120 (context continuation); does not supersede them.

## Why this is a distinct gate

A correct Microsoft x64 calling-convention bridge is not a complete Windows thread-context implementation. Windows exposes extended processor state through feature-aware CONTEXT/XSTATE interfaces, while Linux independently manages architectural register state, signal frames, and per-thread enabled features. An NT-compatible layer must distinguish (a) registers preserved across ordinary calls, (b) state captured/restored at exceptions and context operations, and (c) features available to a guest process. Conflating these can silently corrupt vector state or report unsupported features.

## Primary-source contracts (documentation-verified)

1. **Windows context feature negotiation:** Microsoft documents `GetEnabledXStateFeatures` and `InitializeContext`/`InitializeContext2` for context buffers and feature selection. The size/layout of a usable context cannot safely be inferred from a fixed historical `CONTEXT` allocation. `SetXStateFeaturesMask` and `GetXStateFeaturesMask` concern a context's enabled feature mask, not a promise that all requested CPU features are available.  
   Sources: [GetEnabledXStateFeatures](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getenabledxstatefeatures), [InitializeContext](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-initializecontext), [InitializeContext2](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-initializecontext2), [GetXStateFeaturesMask](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getxstatefeaturesmask), [SetXStateFeaturesMask](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setxstatefeaturesmask).

2. **Architectural layout discovery:** Intel specifies CPUID leaf 0xD for XSAVE-managed component enumeration and size/offset information, with XCR0 and (where supported) XSS governing different state classes. CPUID availability alone is not equivalent to OS enablement.  
   Source: [Intel 64 and IA-32 Architectures Software Developer's Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), Vol. 1, Extended State Save chapter and CPUID reference.

3. **Linux host state is not a Windows CONTEXT:** Linux `sigaction(2)` with SA_SIGINFO supplies a `ucontext_t` for signal handlers, and `sigreturn(2)` restores saved signal context. These describe Linux signal handling; neither is a Windows `CONTEXT` or guarantees Windows feature-mask semantics.  
   Sources: [sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html), [sigreturn(2)](https://man7.org/linux/man-pages/man2/sigreturn.2.html), [getcontext(3)](https://man7.org/linux/man-pages/man3/getcontext.3.html).

4. **Linux x86 feature-specific enablement:** Linux documents per-thread userspace shadow-stack enablement via `arch_prctl`; this is a separate compatibility/policy issue, not proof of Windows CET policy equivalence.  
   Source: [Linux userspace shadow stack](https://docs.kernel.org/arch/x86/shstk.html).

## Architecture implications — HYPOTHESES, not verified implementations

- Keep a *guest-visible* feature profile distinct from host CPUID, host XSAVE layout, and Windows-reference feature masks. Any guest-exposed feature must have a validated capture/restore path for exception and thread-context APIs.
- Represent guest context as an explicitly versioned logical object with bounded conversion to/from host signal state; do not memcpy Linux `ucontext_t` into a Windows `CONTEXT`.
- For guest/host transitions, save only state required by the actual calling convention for ordinary calls, but treat exception delivery, `GetThreadContext`, `SetThreadContext`, and `RtlRestoreContext` as separate full-context correctness gates.
- Do not enable AVX-512, AMX, CET or other optional state merely because a CPU advertises it. Host kernel, process permissions, memory cost, and Windows guest contracts require independent checks.

## Proposed falsifiable gates — NOT RUN

**G1 — feature enumeration:** On a version-recorded Windows x64 reference, record OS build, CPU, `GetEnabledXStateFeatures`, `InitializeContext2` buffer sizing, and mask acceptance/rejection. Compare with Linux CPUID/XGETBV and kernel capability. Do not assume the masks must match.

**G2 — context round-trip:** On Windows, initialize patterned nonvolatile XMM6–XMM15 and supported extended state in isolated worker threads. Capture/modify/restore using documented context APIs; verify preservation and explicit unsupported-mask behavior. Test both ordinary ABI calls and exception continuations.

**G3 — Linux host boundary:** In a dedicated, disposable process, run nested SA_SIGINFO signals during Microsoft-ABI to SysV-ABI transitions and verify state restoration with a host-owned signal stack. Record compiler versions, flags, kernel, CPU and enabled features; never treat this as Windows compatibility by itself.

**G4 — guest feature denial:** Exercise a PE fixture that probes feature masks and tries a feature not exposed by the guest profile. Require deterministic rejection or a documented guest exception rather than host-state corruption.

**G5 — thread lifecycle:** Repeat across independently initialized threads and after thread teardown; test feature-specific per-thread permission and context-buffer lifetime. Preserve Linux host TLS/FS and guest GS invariants from R24/R25.

## Open questions

- Which minimum Windows build/profile should the first guest target expose? A CPU-specific host feature list is not a substitute for this decision.
- Which extended components are actually required by the first real PE workloads, and which are optional until conformance evidence exists?
- Can all required guest context operations be implemented in user space without kernel changes under simultaneous signal delivery and guest thread suspension? **Unproven.**
- How will xstate buffer sizes and expensive components be accounted for in per-thread memory benchmarks?

## Evidence limits

This note relies on linked primary documentation and architecture contracts. No primary source was fetched live in this run, no source revision was pinned, no Windows reference machine was used, and **no code was executed or benchmarked**. Specific OS-build behaviors and CPU-dependent layout details remain OPEN until source-version review and reproducible experiments. This is a research gate, not an implementation claim.
