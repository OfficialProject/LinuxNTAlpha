# R108 — Linux signal ownership versus NT exception delivery

Date: 2026-11-15. Status: **PRIMARY-SOURCE REVIEW ONLY**. No new code, Windows-reference run, or Linux experiment was performed.

## Architectural question

Can a Linux-hosted NT runtime use Linux synchronous signals as the *entry point* for guest hardware exceptions without equating Linux signal delivery with the Windows exception-dispatch contract? This is distinct from R9/R30 (unwind metadata), R41/R93 (vectored/debugger ordering), R65 (stack guards), and R90 (syscall-dispatch provenance). The unresolved integration issue is **ownership of host signal handlers and guest-thread state across multiple concurrent Linux threads**.

## Verified source contracts

1. **Linux signal disposition is process-wide, but masks are per-thread.** Hardware-triggered faults such as invalid-memory SIGSEGV are thread-directed; process-directed signals can be delivered to any eligible unblocked thread. Therefore installing a fault handler is a process-level coordination decision, not an isolated per-guest-thread registration. Source: [Linux signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html).
2. **A signal handler normally executes on the interrupted thread's stack**, with an alternate stack available through sigaltstack. This is an entry mechanism, not a Windows exception record or stack-unwind engine. Sources: [signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html), [sigaltstack(2)](https://man7.org/linux/man-pages/man2/sigaltstack.2.html).
3. **Windows SEH includes hardware and software exceptions** and supports debugger handling and language-level termination handling. A Linux fault alone cannot account for application-generated RaiseException or Windows debugger disposition. Source: [Microsoft Structured Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/structured-exception-handling).
4. **Windows vectored exception handlers run after first-chance debugger notification and before stack unwinding.** This ordering is a guest-runtime obligation, not the order of Linux sigaction handlers. Source: [Microsoft Vectored Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/vectored-exception-handling).
5. **Linux signal handlers have async-signal-safety constraints.** Ordinary allocation, locking and arbitrary runtime calls are not generally safe in a handler; direct invocation of complex guest exception machinery from a POSIX handler must not be presumed safe. Source: [Linux signal-safety(7)](https://man7.org/linux/man-pages/man7/signal-safety.7.html).

## Derived architecture constraints (reasoned, NOT experimentally verified)

- A proposed **host fault ingress** should classify the faulting instruction context and thread ownership before any guest exception translation. Faults from Linux runtime, loader, ABI bridges or signal machinery must not be blindly converted into guest access violations.
- A **minimal signal-context capture path** and a separate, carefully designed guest exception dispatcher are preferable to calling general-purpose guest callbacks directly inside a POSIX signal handler. The precise safe context-switch/return mechanism remains **OPEN**, particularly with nested faults.
- Installing SIGSEGV/SIGILL/SIGFPE/SIGTRAP handlers can conflict with host language runtimes, sanitizers and debugging tools. A single handler-ownership policy and explicit compatibility exclusions are necessary.
- Windows debugger first-chance ordering, vectored handlers, frame-based handlers, continuation and second-chance dispatch must be modeled as guest state transitions; Linux sigaction chaining is not an adequate substitute.
- A Linux process containing multiple guest threads needs a **per-thread guest/host execution-state marker** that remains valid at asynchronous fault time. R24/R25's synthetic GS fixture is not evidence that this is already implemented.
- Signal-based exception ingress is **not a security boundary**. R90's syscall-provenance and containment questions remain unresolved.

## Falsifiable experiment gates (PROPOSED; NOT RUN)

1. In an isolated Linux ELF harness, install SA_SIGINFO handlers and alternate stacks on two threads; trigger controlled guest-marked and host-marked faults, record thread ID, interrupted RIP, signal number and provenance. No arbitrary PE execution.
2. Exercise nested synchronous faults, alternate-stack exhaustion and handler reentry with watchdog and subprocess isolation; require deterministic failure rather than silent host-fault conversion.
3. Build a Windows x64 reference program for RaiseException, hardware access violation, vectored exception/continue handlers, debugger first chance, and __try/__except/__finally; log ordering and context mutation, with OS build/toolchain pinned.
4. Build a Windows-target PE fixture with one deliberate hardware fault and one software exception, and compare observable callback order against the Windows baseline. Keep parser trust restrictions from R23.
5. Run with host sanitizers and a debugger attached, documenting incompatibilities instead of assuming composability. Ensure all test programs run in disposable subprocesses.

## Decision gate

**Do not select Linux signals as the complete NT exception subsystem.** They are a plausible hardware-fault ingress mechanism, conditional on measured context fidelity, safe nested-fault behavior, debugger ordering and per-thread guest/host provenance. None of those integration gates has passed. No implementation or benchmark is claimed.

Sources reviewed 2026-11-15; URLs above are primary Microsoft API documentation and Linux man-pages project documentation. No upstream commit or kernel version was pinned for this documentation-only review.
