# R93 — Windows first-/second-chance exception dispatch and Linux signal/debugger boundaries

Date: 2026-11-15. Status: **primary-source contract review and architectural hypothesis only**. No Windows reference test, Linux test, debugger trace, or new implementation was performed.

## Architectural question

R9/R30 cover x64 unwind metadata and R41 covers vectored handlers. Neither establishes the **debugger-facing dispatch order** of an exception, the distinction between first and second chance, or the lifecycle of a pending debug event. A Linux signal handler or `ptrace` stop cannot simply be relabeled an NT exception dispatch.

## VERIFIED — documented contracts (not reproduced here)

1. Microsoft documents that a debugger is offered a **first-chance** notification before application exception handling; if the exception is not handled, the debugger receives a **last-chance** notification. An unhandled exception can then terminate the process. Source: [Microsoft, Debugger Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/debugger-exception-handling).
2. A Windows debugger waits for events using `WaitForDebugEvent`; it must use `ContinueDebugEvent` to continue a reported event, with `DBG_CONTINUE` and `DBG_EXCEPTION_NOT_HANDLED` expressing materially different dispositions for exception events. Sources: [WaitForDebugEvent](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-waitfordebugevent), [ContinueDebugEvent](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-continuedebugevent), [DEBUG_EVENT](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-debug_event).
3. Windows vectored exception handlers are a separate process-wide mechanism from frame-based exception handling. Their ordering relative to debugger first-chance notification matters to any proposed dispatch model. Source: [Vectored Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/vectored-exception-handling).
4. Linux `ptrace` reports a variety of stops, including signal-delivery stops and syscall stops, with tracer-mediated signal injection/suppression. The Linux interface does not define Windows first-/second-chance debug events. Source: [Linux ptrace(2)](https://man7.org/linux/man-pages/man2/ptrace.2.html) (Linux man-pages project; primary Linux API reference).
5. Linux `sigaction` can deliver a signal with `SA_SIGINFO` and a `ucontext_t` context; this supplies host signal context, not an NT `EXCEPTION_RECORD` or `CONTEXT` contract. Source: [Linux sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html).

## HYPOTHESIS — candidate architecture

Maintain a **guest exception-dispatch state machine** separate from both Linux signal handling and the guest debugger event queue. The host signal handler should capture minimal machine context and hand off to a controlled dispatcher; the dispatcher should synthesize guest exception records, coordinate guest debugger first-chance delivery, invoke the applicable guest exception mechanisms, and emit a second-chance event only when the guest exception remains unhandled.

This is a design candidate, **not** proof that arbitrary Linux synchronous signals can safely be converted to Windows exceptions. In particular, nested faults during dispatch, asynchronous host signals, SUD SIGSYS traps, Linux faults caused by runtime bugs, and guest-requested `RaiseException` require different provenance and recovery rules. An NT debugger API cannot be implemented merely by forwarding raw `ptrace` events.

## Proposed Windows-reference conformance gates — NOT RUN

1. **No debugger:** create a controlled exception with `RaiseException`, a vectored handler, and a frame-based handler. Log handler entry, return disposition and continuation. Repeat with no handler, in an isolated child.
2. **Debugger first chance:** attach a small Windows debug-event harness, log `EXCEPTION_DEBUG_EVENT` code, thread ID, `dwFirstChance`, and `ContinueDebugEvent` disposition. Compare `DBG_CONTINUE` with `DBG_EXCEPTION_NOT_HANDLED`, including which guest handlers execute.
3. **Second chance:** deliberately leave the exception unhandled at first chance; log second-chance notification and terminal disposition in a sacrificial process. Do not assume the same behavior for every exception code.
4. **Handler and debugger interactions:** exercise a vectored handler returning `EXCEPTION_CONTINUE_EXECUTION`, a frame-based handler, and a handler that declines the exception. Validate ordering and that handled exceptions do not spuriously generate second-chance events.
5. **Context edits:** modify the guest instruction pointer in a controlled debugger/handler scenario, then verify exact continuation location and nonvolatile register state. Gate this separately from R30 unwind and R55 ABI preservation.
6. **Concurrency and reentrancy:** inject two independent guest exceptions in distinct threads, then nest a guest exception inside an exception handler. Track debugger-event serialization, per-thread exception context and runtime failure isolation.
7. **Linux prototype only after reference capture:** translate selected synchronous host faults in a disposable child; record signal number, `si_code`, fault address, host register context, and the synthesized guest exception. Do not report conformance from signal conversion alone.

For every gate, record Windows version/build, architecture, debugger attachment state, handler registrations, exception code/flags, event sequence and exact outcome. Record Linux kernel version, libc, compiler, SUD/seccomp configuration and signal masks for any Linux follow-up.

## Decision gate

**OPEN:** Define the ownership and ordering of guest exception dispatch, debugger notifications and Linux fault handling *before* generalizing PE execution to arbitrary applications. The initial runtime may explicitly defer guest debugging, but must not claim Windows exception semantics based on R9/R30/R41 or successful SIGSYS interception alone.

## Nonduplication

R9/R30 concern unwind metadata, R41 vectored-handler lifecycle, and R90 syscall provenance. R93 adds the **debugger event protocol and first-/second-chance exception state machine**, which those notes do not establish.
