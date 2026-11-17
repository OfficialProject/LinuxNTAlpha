# R143 — orderly versus forced process termination and Linux exit-group boundaries

Date: 2026-11-17. Status: **DOCUMENTATION-VERIFIED API contracts; HYPOTHESIZED runtime architecture; no experiments executed**.

## Why this is an independent integration gate

R73 covers waitable process objects and persistent exit status, R74 covers thread-object lifetime, R91 covers loader notifications, and R130 covers child creation. None alone specifies the **termination transaction** that joins guest thread shutdown, DLL detach callbacks, outstanding I/O, object handles and externally observable process signaling. The R19 entry returning 42 is a fixture-specific host call return, **not** evidence of Windows process termination semantics.

## Verified primary-source contracts

1. Microsoft [ExitProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitprocess) documents an ordered path: other threads terminate without DLL_THREAD_DETACH; attached DLL entry points receive DLL_PROCESS_DETACH; the caller terminates; handles close; exit status is published and process object becomes signaled. A terminated thread holding a lock needed by detach code can deadlock the exit path. The documentation says returning from application main results in ExitProcess; this does **not** mean a freestanding PE entry-point return in R19 has equivalent CRT startup/teardown.
2. Microsoft [TerminateProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess) requires PROCESS_TERMINATE rights on its process handle. Self-termination does not return; remote termination is asynchronous, and successful return is **not** proof the target is already signaled. It requests cancellation of pending I/O, and the process may await completion/cancellation. The [process termination overview](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process) says TerminateProcess does **not** deliver DLL detach notifications.
3. Microsoft [Terminating a Thread](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-thread) distinguishes ExitThread and thread-function return from forced termination, and documents persistent waitable thread objects until final handles close. Do not conflate thread exit with process exit.
4. Linux [exit_group(2)](https://man7.org/linux/man-pages/man2/exit_group.2.html) terminates the entire calling thread group and does not return; raw Linux [exit(2)](https://man7.org/linux/man-pages/man2/exit.2.html) terminates the calling thread. Neither syscall promises guest DLL detach callbacks, guest handle-table finalization, NT process-object retention, or 32-bit Windows exit-status visibility. The libc _exit wrapper's use of exit_group is documented separately in [Linux _exit(2)](https://man7.org/linux/man-pages/man2/_exit.2.html).
5. Microsoft's [ExitProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitprocess) states children are not automatically terminated and process objects may persist until the last handle closes. Linux child reaping or host PID disappearance must not be mistaken for the guest object-lifetime contract.

These are documented API properties, not measurements of particular Windows builds or Linux kernels. URLs reviewed 2026-11-17.

## Proposed architecture (HYPOTHESIS, not implementation)

Maintain a guest process record with separate **running / orderly-exit-in-progress / forced-termination-requested / terminated** states, plus a durable 32-bit guest exit code, guest thread-object states, outstanding-I/O registry and retained handle/object references. Treat the guest termination coordinator as authoritative; Linux exit_group is only a final host teardown mechanism. The process broker must preserve waitable guest process identity beyond host task death while references remain.

**Orderly path:** serialize process detach against loader operations (R91), prevent new guest thread entry, account for other thread termination, invoke eligible detach callbacks exactly once, finalize guest handles and publish guest exit status/signal. Because detach callbacks can deadlock, do not assert this path always completes.

**Forced path:** validate guest PROCESS_TERMINATE access, request termination without running DLL detach callbacks, reconcile outstanding I/O (R43/R137), and signal only when the guest termination contract has actually reached its terminal state. Remote API return must not be conflated with wait completion.

**Host boundary:** direct guest syscalls or hostile guest code may call Linux exit_group or kill itself before coordinator cleanup. R90/R129 security gates therefore remain prerequisites to any claim that the coordinator reliably observes all termination paths. A host monitor can detect death but cannot retroactively run guest callbacks. Preserve a distinction between observed host death and confirmed guest orderly termination.

## Falsifiable conformance matrix — NOT RUN

- On a Windows reference VM with exact build recorded, create self-owned executable + two DLLs and multiple threads. Record DLL_PROCESS_ATTACH/THREAD_ATTACH/THREAD_DETACH/PROCESS_DETACH events, callback thread identity and relative ordering for main return, explicit ExitProcess, self-TerminateProcess, remote TerminateProcess and last-thread ExitThread. Do not assert unspecified ordering across unrelated callbacks.
- Keep a second process handle open; terminate the child; compare WaitForSingleObject, GetExitCodeProcess, GetExitCodeThread and handle lifetime after child death. Include 32-bit exit codes with nonzero upper 24 bits to expose Linux wait-status truncation (R73).
- Use pending overlapped I/O to distinguish **termination request accepted**, I/O completion/cancellation and process signaled. Time-bound tests with watchdogs; never assume forced termination has bounded latency.
- Test a DLL detach callback that requests a lock held by another thread **only in an isolated disposable VM** with watchdog and explicit deadlock expectation, not the normal CI path.
- In a separate Linux-only mock broker, model termination state transitions and object-reference retention; use a disposable child to observe exit_group and host wait status. Such a mock would not prove Windows equivalence.
- Security negatives: invalid PROCESS_TERMINATE rights, duplicate concurrent termination requests, process exit racing child handle duplication, host task killed externally, and guest direct Linux syscall escape. Record what was actually observed versus modeled.

## Decision impact

**OPEN:** broker location and trust boundary; exact event ordering for implementation; behavior when host task dies before guest teardown; remote forced-termination completion tracking; robust 32-bit guest exit-code storage. **Do not** implement ExitProcess as a direct exit_group wrapper or claim the existing PE fixture return is a Windows process-exit test.

No code changed, tests run, benchmarks collected or Windows-reference behavior measured for R143.
