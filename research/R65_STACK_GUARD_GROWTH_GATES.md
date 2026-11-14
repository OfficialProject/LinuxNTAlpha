# R65 — Windows stack growth, guard faults, and Linux signal delivery gates

Date: 2026-11-13. Status: **primary documentation reviewed only; no code or experiment executed**.

## Scope and distinction

R30 studies x64 unwind metadata; R41 studies vectored exception callbacks; R42 studies suspended thread contexts; R35 covers fibers. None proves that a PE64 thread can grow its stack, report stack exhaustion, and safely deliver a Windows exception while running on a Linux host stack. This is an execution prerequisite, not merely an error-code translation detail.

## VERIFIED FROM PRIMARY DOCUMENTATION

1. Microsoft states that each Windows thread/fiber stack has a reserved virtual-address region and an initially committed portion. The OS commits additional reserved pages as needed; the executable's PE header supplies defaults. CreateThread's `STACK_SIZE_PARAM_IS_A_RESERVATION` changes the interpretation of its size parameter. This is a reserve/commit distinction, not simply an allocation of one fixed Linux pthread stack. [Microsoft — Thread Stack Size](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-stack-size).
2. A Windows `PAGE_GUARD` page is a **one-shot** access alarm. Access raises `STATUS_GUARD_PAGE_VIOLATION` (0x80000001) and clears the guard modifier. A system-service access may instead cause that service to fail. A generic guard page and a stack-growth guard must not be conflated without further stack-specific testing. [Microsoft — Creating Guard Pages](https://learn.microsoft.com/en-us/windows/win32/Memory/creating-guard-pages).
3. `SetThreadStackGuarantee` establishes a minimum amount of stack available during stack-overflow exception handling for the calling thread **or fiber**; zero queries the current value; smaller requests do not reduce it. The documentation calls for special recovery, including `_resetstkoflw` for Microsoft C/C++ programs, before resuming after overflow. [Microsoft — SetThreadStackGuarantee](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadstackguarantee).
4. Linux `sigaltstack` provides a per-thread alternate signal stack, used by handlers installed with `SA_ONSTACK`. This is a Linux signal-delivery facility, **not** Windows SEH, and does not by itself grow guest stacks or supply Windows exception records. [Linux man-pages — sigaltstack(2)](https://man7.org/linux/man-pages/man2/sigaltstack.2.html); [POSIX — sigaltstack](https://man7.org/linux/man-pages/man3/sigaltstack.3p.html).
5. Linux `mmap` exposes `MAP_GROWSDOWN` and `MAP_STACK`, but their documented behavior is not a Windows reserve/commit/guard ABI. Their existence does not establish compatible fault ordering, error codes or stack guarantees. [Linux man-pages — mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html).

These sources are public interface documentation, not pinned Windows implementation builds or Linux kernel source revisions. Exact behavior remains version- and architecture-sensitive.

## HYPOTHESIS — candidate architecture, not verified implementation

Represent each guest stack as an explicit address-space reservation with a tracked committed interval, a moving guest guard boundary, stack limits, and a separate emergency exception-delivery resource. A fault dispatcher would distinguish: (a) guest stack-growth guard faults, (b) deliberate non-stack PAGE_GUARD accesses, (c) exhaustion, (d) unrelated access violations, and (e) faults caused by the host runtime itself. A per-host-thread alternate signal stack might allow limited fault handling when the guest stack is exhausted; the handler should avoid non-async-signal-safe operations and must not assume it can directly call a Windows exception dispatcher from signal context.

This design is **unproven**. In particular, Linux `mprotect` plus a SIGSEGV handler does not automatically implement Windows exception ordering or one-shot guard semantics. Mapping guest pages in a signal handler, guest/host reentrancy, concurrent accesses, nested signals and unwind across the host ABI require separate proofs.

## Falsifiable gates — NOT RUN

1. Build a tiny Windows x64 reference executable with known PE stack reserve/commit sizes; record `VirtualQuery` ranges before and after incremental stack probing and large frames, including exact Windows build, compiler flags, thread creation flags and page size.
2. Test an ordinary `PAGE_READWRITE | PAGE_GUARD` allocation twice and log exception order, guard clearing and retry. Keep it separate from thread-stack growth.
3. In a disposable child process with a timeout, exhaust a small reserved stack; capture `EXCEPTION_STACK_OVERFLOW`, exception handler stack availability, `SetThreadStackGuarantee` query/change behavior and documented recovery. Never run uncontrolled recursion in the main test runner.
4. Repeat with a fiber created with `CreateFiberEx`, explicitly tracking fiber switches and guarantee scope. Test an alternate guest thread and host pthread separately.
5. On Linux, prototype a fixed reserved mapping with staged `mprotect` commitment and a `sigaltstack` / `SA_ONSTACK` handler. Log `siginfo_t` and `ucontext_t` in preallocated storage; avoid allocation, locks and stdio inside handlers. Use a child process and strict timeout for nested faults.
6. Add negative controls: non-stack guard fault, unmapped address, write to read-only page, concurrent thread stacks, near-boundary stack-pointer jumps, and a fault in the handler itself. Compare traces with Windows, **not** just whether the process survives.
7. Only after correctness: measure page-fault and handler overhead across repeated controlled trials; no performance claim before this.

## Acceptance criteria and unresolved questions

- Preserve Windows reserve/commit/guard distinctions and PE stack-size metadata; avoid claiming pthread defaults are equivalent.
- Define precise ownership of fault classification and Windows exception delivery; do not let a guest stack fault be misclassified as a host crash or vice versa.
- Specify emergency-stack allocation, cleanup and nested-fault policy per host thread; protect host signal machinery from guest mutation.
- Determine actual Windows guard movement, skipped-page behavior, exhaustion codes and recoverability from a pinned Windows oracle.
- Determine interaction with R26 syscall-user-dispatch SIGSYS and R41 exception ordering without treating independent Linux signals as an NT exception mechanism.
- Treat all proposed tests as **NOT RUN** until their code, environment and output are committed.

**No Windows oracle, Linux runtime test, benchmark or implementation is claimed by this document.**
