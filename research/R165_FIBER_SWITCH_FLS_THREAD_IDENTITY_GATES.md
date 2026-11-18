# R165 — Windows fiber switching, FLS, and host-thread identity gates

Status: **PRIMARY-SOURCE REVIEW; NO EXPERIMENT**  
Reviewed: 2026-11-18 UTC  
Scope: x86-64 NT-compatible user-space runtime on Linux; Windows fiber API surface, not a claim of kernel NT equivalence.

## Verified contracts (Microsoft primary documentation)

1. A Windows fiber is **cooperatively scheduled** by its application inside a scheduled thread. The OS continues to schedule threads; a fiber's operations count as operations of its executing thread. TLS accessed by a fiber is that of its **current thread**, not fiber-private TLS. Fiber state includes a stack, a subset of call-preserved registers, and its creation data. [M1]
2. A thread must first become a fiber using ConvertThreadToFiber or ConvertThreadToFiberEx before it schedules other fibers. CreateFiber supplies a stack size, start routine and fiber data. A fiber start routine returning terminates its executing thread. [M1][M2]
3. SwitchToFiber saves the current fiber's state and restores the selected fiber. Microsoft explicitly permits switching to a fiber created by another thread **with proper synchronization**. Switching to the currently selected fiber is specifically discouraged as potentially unpredictable. [M1][M3]
4. Fiber-local storage (FLS) follows the selected fiber, unlike TLS. FLS functions behave as TLS when no switching occurs. DeleteFiber releases fiber resources; deleting the currently executing fiber exits the thread. Deleting a fiber selected by another thread risks abnormal termination. [M1]
5. ConvertThreadToFiberEx documents FIBER_FLAG_FLOAT_SWITCH, specifically warning that with zero flags **x86** floating-point state is not switched. That statement must not be generalized into a proven x64 register-save policy without separate x64 evidence. [M2]

## Architecture consequences (reasoned, not measured)

- **HYPOTHESIS:** Implement a guest fiber abstraction over existing Linux host threads using explicitly owned guest stacks and an ABI-correct context-switch routine, rather than one host pthread per fiber. This follows the cooperative scheduling contract but requires unwind, stack guard, sanitizer and CPU-state validation.
- **HYPOTHESIS:** Keep Windows guest thread identity, guest TLS, host Linux TLS and guest FLS distinct. Migrating a fiber between host threads must change which guest-thread TLS is visible, while preserving that fiber's FLS. A generic user-space coroutine library is not evidence of this contract.
- **HYPOTHESIS:** Use an ownership/state machine (created, runnable, running-on-thread, suspended, deleted) with synchronized transfer and generation-tagged references to prevent simultaneous execution, stale switch targets and stack use-after-free. The Microsoft documentation's warning does not specify the exact behavior of invalid races.
- **OPEN:** How Windows x64 switching interacts with XSTATE, CET shadow stacks, guest SEH/unwind, TEB fiber fields, fiber migration and FLS callback ordering. No choice of assembly sequence or Linux backend is justified yet. Cross-reference R65 (stack), R109 (CET), R120 (context restoration), R141 (XSTATE) and R125 (thread bootstrap).

## Falsifiable gates (proposed, **not run**)

1. **Windows reference, single thread:** ConvertThreadToFiberEx; create two fibers; alternate switches; assert independent stack locals and FLS values while TLS remains shared. Record Windows build and x64 architecture.
2. **Windows reference, migration:** Create fiber on thread A; hand off only while suspended to converted thread B using synchronized rendezvous; observe GetCurrentThreadId, TLS and FLS from inside fiber. No concurrent switches into one fiber.
3. **Windows reference, lifecycle:** Verify normal fiber-return thread termination and FLS cleanup observations with explicitly instrumented callbacks. Keep invalid concurrent deletion as a separate isolated negative test, not a required compatibility behavior.
4. **Linux isolated prototype:** ABI-preservation fixture across switches, stack guard fault behavior, guest thread identity/TLS/FLS separation, signal interruption, unwind, CET/XSTATE capability probing. A passing Linux test does **not** establish Windows compatibility.
5. **Performance only after correctness:** Compare switches and allocations under matched workloads; report distributions and host configuration. No zero-cost or performance-superiority claim is supported.

## Primary sources

- [M1] Microsoft, *Fibers* (page last updated 2025-07-14), https://learn.microsoft.com/en-us/windows/win32/procthread/fibers — inspected 2026-11-18.
- [M2] Microsoft, *ConvertThreadToFiberEx* (page last updated 2024-02-22), https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-convertthreadtofiberex — inspected 2026-11-18.
- [M3] Microsoft, *SwitchToFiber* (page last updated 2024-02-22), https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-switchtofiber — inspected 2026-11-18.

No Windows reference run, Linux runtime probe, binary inspection, or performance measurement was performed for R165.
