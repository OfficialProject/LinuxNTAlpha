# R41 — Vectored exception/continue handlers are not x64 unwind tables

Date: 2026-11-12. Status: **PRIMARY-DOCUMENTATION REVIEW ONLY**. No Windows reference execution, Linux signal bridge, PE exception test, or performance measurement was performed.

## Scope and distinction

R9/R30 address x64 unwind metadata and function tables. This note covers a different layer: **process-wide vectored exception and continue handler registration, ordering and lifetime**. A loader that merely registers PE `.pdata` entries does not thereby provide vectored exception handling.

## VERIFIED FROM MICROSOFT DOCUMENTATION

1. Microsoft's [Vectored Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/vectored-exception-handling) describes handlers as non-frame-based; they can be called independent of the active call frame. Exception handlers run after first-chance debugger notification and before stack unwinding begins. The article distinguishes vectored exception from vectored continue handlers.
2. [AddVectoredContinueHandler](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredcontinuehandler) registers a continue handler and returns an opaque registration pointer on success, NULL on failure. Nonzero `First` requests first position, zero requests last; a later first registration can supersede that position. The documentation warns that unloading a DLL containing registered handler code does **not** automatically unregister the handler.
3. [RemoveVectoredContinueHandler](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-removevectoredcontinuehandler) unregisters by the returned handle and reports nonzero success / zero failure.
4. The [errhandlingapi.h API index](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/) lists distinct Add/RemoveVectoredExceptionHandler and Add/RemoveVectoredContinueHandler functions, `RaiseException`, and `RaiseFailFastException`; the latter is described as bypassing frame-based and vectored exception handlers.
5. Microsoft's [Using a Vectored Exception Handler](https://learn.microsoft.com/en-us/windows/win32/debug/using-a-vectored-exception-handler) illustrates registration, removal and ordering. **Its example explicitly warns it is unsuitable for 64-bit Windows**; do not copy its x86 instruction-pointer manipulation into an x64 prototype.

These are API documentation claims, not measured behavior on a pinned Windows build. Pages reviewed 2026-11-12; documentation is not a pinned OS implementation.

## HYPOTHESIS — compatibility architecture

An NT-compatible user-mode exception dispatcher likely needs separate registries for vectored exception and continue handlers, per-process ordering, opaque registration identities and an explicit dispatch state machine. It must coordinate debugger-first-chance policy, frame-based SEH/unwind, continuation, unhandled exceptions, and thread-context restoration. **A POSIX signal handler alone is not an equivalent implementation.** This is a design candidate, not a selected kernel/userspace boundary.

In particular, registration lifetime must be considered when a guest DLL is unloaded: silently treating a callback pointer as owned by a host ELF module or auto-removing it without a reference test could change observable Windows behavior. Guest callback execution also requires a correct Microsoft x64 ABI and a valid guest exception/context representation; the existing R17 bridge is insufficient proof.

## Proposed Windows-reference conformance matrix — NOT EXECUTED

1. **Ordering:** register exception handlers A(last), B(first), C(first), and continue handlers D(last), E(first). Raise a controlled software exception; record callback order and whether continue handlers are reached for each return disposition. Repeat after removal and re-registration.
2. **Exception disposition:** test `EXCEPTION_CONTINUE_SEARCH` versus `EXCEPTION_CONTINUE_EXECUTION`, a frame-based handler, an unhandled exception and debugger-attached first chance. Capture `EXCEPTION_RECORD`, `CONTEXT`, status and actual continuation address.
3. **Thread/process scope:** register from one thread, raise from another; compare callback identity and ordering. Test nested raises and concurrent registration/removal separately; do not assume unspecified race ordering.
4. **DLL lifetime:** register a handler from a test DLL, explicitly unregister before unload as safe baseline; separately examine documented stale registration behavior without deliberately jumping into unmapped code. Do not use unsafe dangling-callback execution as a required test.
5. **Noncontinuable/fail-fast:** compare a noncontinuable exception and `RaiseFailFastException` against normal continuable software exceptions. Record termination and debugger behavior in a disposable Windows VM.
6. **Linux prototype gate:** only after an x64 Windows oracle exists, test controlled `RaiseException`-style software delivery first. Physical faults, Linux SIGSEGV/SIGILL translation, guard-page and SUD SIGSYS interaction are later, distinct gates. Never claim general SEH from a single signal test.

## OPEN

- What is the minimum complete x64 `EXCEPTION_RECORD`/`CONTEXT` contract, including vector state, required for guest callbacks?
- What are exact ordering and continuation rules for mixed vectored, frame-based, and debugger cases on the target Windows build?
- How can a guest exception dispatcher coexist safely with host signals and SUD without recursively misclassifying host faults?
- What synchronization/lifetime guarantees can be proven for concurrent remove and in-flight callbacks?

## Decision gate

Do not mark x64 exceptions supported merely because PE unwind data parses, or because a Linux signal is caught. Require Windows-reference handler-order, disposition, context-restoration and module-lifetime conformance first. No experiment was executed for R41.
