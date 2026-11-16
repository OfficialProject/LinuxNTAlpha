# R120 — NT context continuation versus Linux signal-frame restoration

Date reviewed: 2026-11-16. Status: **documentation analysis only; no new experiments**.

## Scope and nonduplication

R9/R30 cover x64 unwind metadata, R41 vectored handlers, R93 debugger exception order, and R108 Linux signal ingress. This note isolates a different boundary: **resuming a Windows-target thread after a guest handler modifies its CONTEXT**, including whether Linux signal-frame restoration is a sufficient backend. It does not claim an NT exception dispatcher exists.

## VERIFIED — documented contracts

1. Microsoft describes `RtlRestoreContext(ContextRecord, ExceptionRecord)` as restoring the specified context and **not returning**. It documents special processing for `STATUS_LONGJUMP` and `STATUS_UNWIND_CONSOLIDATE`; a plain register-copy model would miss those paths. [Microsoft RtlRestoreContext](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlrestorecontext).
2. `RtlUnwindEx` initiates stack-frame unwinding to an optional target frame and instruction pointer, accepts an exception record, return-register value and context, and does not return. This is distinct from merely setting RIP/RSP. [Microsoft RtlUnwindEx](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlunwindex).
3. Microsoft places vectored exception handlers after debugger first-chance notification and before stack unwinding. Vectored continue handlers have their own registration API. The two registration classes must not be collapsed into a Linux signal handler list. [Microsoft vectored exception handling](https://learn.microsoft.com/en-us/windows/win32/debug/vectored-exception-handling); [AddVectoredContinueHandler](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredcontinuehandler).
4. Linux signal delivery saves the interrupted machine state, signal mask and alternate-stack state in a signal frame; an SA_SIGINFO handler receives a `ucontext_t` reflecting the interrupted state. The kernel's signal-return mechanism restores state from that frame. This is a **host signal protocol**, not a Windows CONTEXT or unwind contract. [Linux signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html).

## HYPOTHESIS — candidate runtime boundary

Keep a **guest CONTEXT model** separate from the **host signal-frame/ucontext representation**. A signal-entry adapter would capture and classify a guest-origin fault, then invoke a guest exception state machine. A continuation adapter would validate the requested guest register state and translate it to a supported host restoration path. Windows unwind/long-jump consolidation should be modeled above this host mechanism rather than inferred from `sigreturn`.

A host signal-frame edit might be viable for a narrowly scoped synchronous fault on the same thread, but **does not establish** support for arbitrary `RtlRestoreContext`, cross-thread `SetThreadContext`, exception consolidation, or continuation outside signal-handler context. Nor does it provide security containment. The ability to resume with modified registers is a testable implementation hypothesis, not a proven portable guarantee.

## OPEN — architecture and security gates

- Define which guest `CONTEXT` fields can be restored (GPRs, flags, XMM/YMM and extended state, segment-base handling, debug registers) and how unsupported fields fail.
- Separate synchronous guest faults from host-runtime faults; host SIGSEGV/SIGILL must not silently become guest exceptions.
- Establish a reentrancy policy for nested signals during exception dispatch and for exceptions raised by guest callbacks.
- Determine how continuation interacts with SUD selector state (R26/R90), GS-based guest TEB (R24/R25), shadow stacks (R109), and nonvolatile SIMD registers (R55).
- Define a state-ownership rule for the signal mask and alternate signal stack: guest CONTEXT edits must not accidentally override host signal bookkeeping.
- Treat user-controlled context records as untrusted input; validate canonical instruction/stack pointers, executable mappings and host-runtime isolation before attempting a restore. Exact policy requires design and tests.

## Proposed falsifiable tests — **not executed**

**Linux-only isolated fixture:** install SA_SIGINFO on a dedicated guest thread, trigger a deliberate synchronous fault, record `ucontext_t`, redirect instruction pointer to a known continuation, and verify general-purpose and vector state plus original signal-mask restoration. Run with a dedicated alternate signal stack and with nested-fault injection; isolate crashes in child processes. Repeat with SUD enabled and host callbacks.

**Windows-reference fixture:** use a Windows x64 executable to record context changes made by a vectored exception handler, vectored continue handler, and frame-based exception handler; compare observable continuation, ordering, and nonvolatile registers. Separately exercise `RtlRestoreContext` with ordinary context and documented special exception-record cases, and `RtlUnwindEx` with a known unwind target. Do not assume undocumented edge-case behavior from API prose.

**Acceptance:** explicit PASS/SKIP per platform and version; raw register snapshots and traces; no claim of general SEH/C++ EH support from a single successful instruction-pointer redirect.

## Decision impact

**Do not select Linux `sigreturn` as the NT continuation architecture on documentation alone.** It is a candidate low-level mechanism for a subset of same-thread fault returns; Windows unwind semantics, guest context validation, debugger ordering, and host signal ownership remain separate required components.

Sources retrieved/reviewed 2026-11-16; links above are upstream Microsoft and Linux manual documentation. No benchmark, kernel test or Windows-reference test was performed for R120.
