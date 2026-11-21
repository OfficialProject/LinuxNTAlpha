# R215 — Windows fiber context switching and Linux user-context boundaries

Research date: 2026-11-21. **Status: primary-documentation review only.** No Windows differential execution, Linux test, implementation, or performance measurement. Kernel boundary remains OPEN.

## Scope and distinction from prior work

R96 addresses fiber-local-storage callback lifetime. R24/R25 address a synthetic TEB/GS thread environment. This note instead asks whether a Linux-hosted runtime can preserve **Windows fiber switching, stack and thread identity** without treating fibers as Linux threads. These are separate gates: a correct FLS table does not prove correct fiber context switching.

## VERIFIED FROM SOURCE — documented contracts

1. Microsoft describes fibers as cooperatively scheduled execution contexts: the application switches fibers explicitly; the OS still schedules the underlying thread. `ConvertThreadToFiber` converts the running thread's context to a fiber, and `CreateFiber` supplies a start function, stack size and fiber data. If a fiber start function returns, the **running thread exits**. [M1]
2. `SwitchToFiber` saves current fiber state and restores the target fiber state; Microsoft permits switching to a fiber created by another thread but requires appropriate synchronization. Microsoft explicitly warns that switching to `GetCurrentFiber()` may cause unpredictable behavior. The documentation does not promise that concurrent execution of the same fiber is safe. [M2]
3. `ConvertThreadToFiberEx` documents `FIBER_FLAG_FLOAT_SWITCH`: on x86, without that flag floating-point state is not switched and data may be corrupted. This source does **not** establish the precise x64 XSAVE/XSTATE behavior; that remains OPEN. [M3]
4. Microsoft states that FLS follows the currently selected fiber; without fiber switching it behaves like thread-local storage. `DeleteFiber` frees fiber resources including its stack, and deleting the currently executing fiber terminates the thread. Deleting a fiber selected by another thread can cause abnormal termination. [M1]
5. Linux's `makecontext`/`swapcontext` API provides user-context switching, but the Linux man-pages project records removal of these interfaces from POSIX.1-2008 on portability grounds. It documents a separate caller-provided stack and cautions that pointer-sized arguments to `makecontext` are not portable. Thus the API is **possible experimental machinery**, not evidence of a complete Windows fiber backend. [L1]
6. Linux `getcontext`/`setcontext` also lost POSIX standardization in POSIX.1-2008. Its signal-context documentation warns against assuming arbitrary nonlocal jumps from signal handlers are well defined. [L2]

## HYPOTHESIS — proposed guest runtime boundary (NOT SELECTED)

Maintain a guest fiber identity and execution-context record independent of Linux task identity. Keep each guest thread's currently selected fiber pointer, and explicitly coordinate switching with guest TEB-visible stack metadata, FLS selection, exception/unwind state and guard-page ownership. Switching must preserve the documented guest ABI's nonvolatile machine state and whichever floating-point/vector state Windows requires for the target architecture/version. **Do not infer the full save set from the generic phrase “saved registers.”**

A user-space context-switch trampoline is a candidate. `ucontext` may be useful for a narrow proof-of-mechanics, but should not be adopted as the ABI contract: its register-state behavior, signal-mask handling, stack allocation, CET/shadow-stack interactions, and portability need explicit source audit and experiments. Neither pthread-per-fiber nor Linux scheduling primitives automatically reproduce cooperative guest fiber identity.

Fiber migration between Linux threads is a **separate high-risk hypothesis**: FLS follows the fiber while thread-local state, signal masks, GS/TEB identity, and host TLS can be thread-associated. No migration strategy is approved until Windows reference traces and host safety tests resolve the boundary.

## OPEN — falsifiable conformance gates (NOT RUN)

- **Windows reference, version pinned:** convert a thread; create two fibers; switch in alternating order; record fiber data, FLS values, `GetCurrentFiber`, thread IDs, stack addresses, last-error and return behavior. Include nested switch chains and conversion back to a thread.
- **Stack and lifetime:** vary reserve/commit stack sizes via `CreateFiberEx`; exercise guard-page growth, stack exhaustion and fiber deletion only in isolated child processes; verify termination semantics when a fiber function returns. Do not assume all failure paths are recoverable.
- **Architecture state:** compare x64 callee-saved GPRs, XMM6–XMM15, floating-point control state, and extended processor state where supported; separately test documented x86 float-switch flag behavior. Validate unwind/exception paths and CET-enabled configurations, not merely a successful normal return.
- **Migration:** create on thread A, switch on thread B under explicit synchronization, check thread identity, FLS, TEB stack fields and host TLS; never concurrently schedule the same fiber. Compare Windows reference behavior before designing Linux mapping.
- **Linux candidate:** pin glibc/kernel/compiler revisions; compare a custom ABI-aware trampoline with `swapcontext` for state fidelity, signal masks, ASan/TSan behavior, stack guard handling and cross-thread restrictions. Include negative controls and process isolation for expected crashes.
- **Security:** ensure guest fiber pointers are validated guest-owned objects, not host context pointers; prohibit freeing an active stack; define cancellation and teardown ownership without exposing host signal context or raw kernel credentials.
- **Performance:** only benchmark after semantic equivalence; report context-switch distributions and cache/TLB effects under matched workloads. No performance conclusion is supported here.

## Decision gate

**Do not map each Windows fiber to a Linux thread or select `ucontext` as the permanent runtime implementation.** First establish guest-visible fiber/TEB/FLS state invariants and differential x64 context preservation. This is a research gate, not an architectural decision.

## Primary sources (accessed 2026-11-21)

- [M1] Microsoft, Fibers: https://learn.microsoft.com/en-us/windows/win32/procthread/fibers
- [M2] Microsoft, SwitchToFiber: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-switchtofiber
- [M3] Microsoft, ConvertThreadToFiberEx: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-convertthreadtofiberex
- [L1] Linux man-pages project, makecontext/swapcontext: https://man7.org/linux/man-pages/man3/swapcontext.3.html
- [L2] Linux man-pages project, getcontext/setcontext: https://man7.org/linux/man-pages/man3/setcontext.3.html

These are live documentation pages, not pinned upstream implementation revisions. No test results are asserted.
