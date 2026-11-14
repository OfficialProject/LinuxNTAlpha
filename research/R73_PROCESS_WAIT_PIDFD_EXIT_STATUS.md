# R73 — Waitable process lifetime, exit status, and Linux pidfd boundaries

Date: 2026-11-14. Evidence: **VERIFIED FROM PRIMARY DOCUMENTATION** for stated Windows and Linux contracts; **HYPOTHESIS** for the proposed service design; **OPEN** for Windows-reference and Linux-runtime conformance. **No experiment was run.**

## Why this is distinct

R12 covers creation and lifecycle broadly, R42 covers thread suspension, and R67–R68 cover handle identity. None establishes that a process's *waitable NT object* must remain observable after its host execution task is reaped, or that a POSIX child status can represent the complete Win32 exit code. This affects the object manager, wait subsystem, handle duplication and termination races.

## Verified Windows contracts

1. Windows process termination sets the process exit code and signals the **process object**. Remaining threads are marked for termination, process-owned handles close, but other open handles can keep referenced kernel objects alive. Terminating a parent does **not** automatically terminate its children. [Microsoft, Terminating a Process](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process).
2. `WaitForSingleObject` accepts process and thread handles and requires `SYNCHRONIZE` access. A zero timeout polls; `WAIT_OBJECT_0` reports signaled, `WAIT_TIMEOUT` reports timeout. Closing a handle while a wait on it is pending has **undefined** behavior in the documented Win32 API; do not invent a guaranteed outcome. [Microsoft, WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject).
3. `GetExitCodeProcess` requires process query access and returns immediately. While running, it reports `STILL_ACTIVE` (259); after termination, it can return an explicit exit value, main return value, or unhandled exception code. The documentation warns that 259 can also be an application exit code, so treating the numeric value alone as a reliable liveness flag is wrong. [Microsoft, GetExitCodeProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess).
4. `ExitProcess` termination notifies attached DLLs of process detach, whereas `TerminateProcess` does not; therefore the two exit paths cannot be reduced to an undifferentiated host kill operation. [Microsoft, Terminating a Process](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process).

## Verified Linux backend constraints

1. Linux `pidfd_open` exists since Linux 5.3. A pidfd can be polled: `EPOLLIN` when the referenced task exits and becomes a zombie; `EPOLLHUP` on reaping. Without `PIDFD_THREAD`, readiness follows termination of the thread group; `PIDFD_THREAD` is documented since Linux 6.9. A pidfd is a useful host *identity/liveness primitive*, **not** itself a Windows process handle. [Linux pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html).
2. `waitid(P_PIDFD,...)` exists since Linux 5.4 but waits for a **child** of the calling process; `WNOWAIT` allows observing a child exit without reaping it. `waitpid` exposes only the least significant **eight bits** of a normal `exit` status via `WEXITSTATUS`. Therefore a plain host exit code cannot faithfully encode arbitrary 32-bit Windows process statuses. [Linux wait(2)](https://man7.org/linux/man-pages/man2/wait.2.html).
3. The documented `pidfd_open` race-avoidance conditions for already-exited children depend on SIGCHLD disposition and reaping; `CLONE_PIDFD` is suggested when those conditions cannot be assured. Avoid a PID-number-only lookup that may race reuse. [Linux pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html).

The cited Microsoft pages and Linux man-pages were inspected on 2026-11-14. These are documentation contracts, **not** pinned kernel implementation revisions, benchmark results or proof of Windows-build-specific behavior.

## Architecture hypothesis — not implemented

Represent the guest process as a separately reference-counted NT object with a monotonic `running -> terminating -> terminated` state machine, durable 32-bit guest exit status, wait notification, access-controlled handles, and explicit lifetime ownership. Keep the host pidfd as an optional execution-backend reference rather than the guest object's sole identity. Record the guest exit status in a supervised channel before or during orderly teardown; specify how crash, signal, supervisor failure and forced termination translate to NTSTATUS **only after** reference tests. Publish terminal state and wake waiters atomically under a defined synchronization protocol. Reaping the host child must not destroy the guest process object's recorded status while handles remain.

Potential pitfall: delaying reaping merely to keep pidfd readable can leak zombies; instead capture termination information once, reap safely, and retain a separate guest process record. Another pitfall: `pidfd_send_signal` is not a substitute for Windows `TerminateProcess` authorization, DLL detach semantics, or exact exit-code selection.

## Falsifiable tests — NOT RUN

- **Windows reference:** create child returning 0x12345678; wait for process signal; query full exit code before and after duplicating/closing other process handles. Separately return 259 and prove signaled state is independent of numeric `STILL_ACTIVE`.
- **Windows reference:** parent exits while child continues; compare normal `ExitProcess` versus external `TerminateProcess` for DLL detach notification using a trusted instrumented DLL; log OS build.
- **Windows reference:** attempt wait with a process handle lacking `SYNCHRONIZE`, and query without process query rights; record exact failures. Do **not** assert outcomes for concurrent close-while-wait, which is documented undefined.
- **Linux backend:** record kernel version, `pidfd_open` and `waitid(P_PIDFD)` support, then verify poll transitions before and after reaping, `WNOWAIT`, status truncation, PID reuse protection, and multi-waiter notification. Distinguish unsupported (**SKIP**) from a failed semantic assertion (**FAIL**).
- **Service model:** test duplicate guest handles surviving child reaping, independent access rights, multiple waiters, termination races, supervisor crash recovery and bounded resource cleanup. No performance claim until correctness gates pass.

## Decision gate

**Do not** equate Linux PID, pidfd, zombie, or `waitpid` status with an NT process object. The separate guest lifetime/exit-status record is a design candidate; exact exit-path behavior, status translation and multi-process supervisor architecture remain **OPEN**.
