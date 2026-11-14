# R74 — NT thread-object lifetime versus Linux TID-clear wakeups

Date: 2026-11-14. **VERIFIED FROM PRIMARY DOCUMENTATION** for the stated API contracts; **HYPOTHESIS** for the proposed implementation; **OPEN** for reference conformance. **No experiment was executed.**

## Distinct gap and relevance

R73 examines *process* wait/exit-code retention after host reaping. R35 concerns fibers/FLS, R42 suspension/context, and R45 DLL loader callbacks. None establishes a separate, durable *thread object* after the Linux execution thread disappears. A Linux TID, a pthread join token, and a futex clear-child-TID word cannot be treated as interchangeable NT thread handles.

## Windows documented contracts

- Terminating a thread sets its exit code and signals its thread object. Other threads waiting on that object can be released. The object persists until all open thread handles are closed. A thread returning from its start routine, calling `ExitThread`, or being terminated externally follows different cleanup paths. [Microsoft: Terminating a Thread](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-thread).
- `ExitThread` takes a 32-bit `DWORD` exit code, performs DLL thread-detach notifications, and documents serialization with thread creation and DLL initialization. If it is the last active thread, process termination follows. In C++, exiting this way skips automatic C++ destructor cleanup; returning from the thread function is advised. [Microsoft: ExitThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitthread).
- `TerminateThread` requires `THREAD_TERMINATE` access and does not allow target user-mode cleanup; the thread object becomes signaled and retains the specified exit code. Microsoft warns of resource and lock hazards. [Microsoft: TerminateThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminatethread).
- `GetExitCodeThread` requires `THREAD_QUERY_INFORMATION` or `THREAD_QUERY_LIMITED_INFORMATION` (version-dependent), returns immediately, and returns `STILL_ACTIVE` while the thread has not terminated. A numeric exit code of 259 must not substitute for the thread object's separate signaled state. [Microsoft: GetExitCodeThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodethread).

## Linux documented primitive and mismatch

Linux `set_tid_address(2)` documents that a thread's `clear_child_tid` address is written with zero at exit and a `FUTEX_WAKE` operation wakes **one** futex waiter; errors from that wake are ignored. The mechanism assists pthread join but **does not** directly provide NT multi-waiter object signaling, persistent 32-bit guest exit status, handle access rights, or guest DLL teardown. [Linux man-pages: set_tid_address(2)](https://man7.org/linux/man-pages/man2/set_tid_address.2.html). This source is a Linux manual, not a pinned kernel-source verification; exact kernel-version behavior is **OPEN**.

## Architecture candidate — not implemented

Maintain a separately reference-counted guest thread object containing stable guest identity, access-controlled handles, full 32-bit exit status, terminal/signaled state, and an explicit waiter registry. The host thread ID and clear-child-TID word are execution-backend details, never the sole guest object identity. A guest termination protocol must coordinate exit-code publication, teardown notifications, completion visibility, and waiters without depending on a single futex wake. Treat asynchronous external termination as a separate semantic problem; simply calling `pthread_cancel` or sending a Linux signal is **not** established as equivalent to `TerminateThread`.

R73's process-state machine should consume terminal thread transitions, including last-thread process exit, but process and thread handle lifetime must remain distinct. Correct ordering between DLL notifications, thread state publication, and last-thread process shutdown remains **OPEN**.

## Falsifiable gates — NOT RUN

1. On a recorded Windows build, start a thread returning `0x12345678`; duplicate its handle; wait from multiple threads; verify full exit code and repeated signaled waits after the original handle closes.
2. Return `259` and distinguish the running `STILL_ACTIVE` observation from a terminated thread whose exit code is 259 by waiting on the thread object.
3. Open/query/wait/terminate using deliberately restricted handle rights and record each exact API result and error.
4. With a trusted DLL, contrast normal return/`ExitThread` and external `TerminateThread` for detach notifications, and record cleanup hazards without assuming deterministic results for unsafe termination.
5. On Linux, instrument a controlled pthread exit and `clear_child_tid` wake with two independent waiters, then compare it with an explicit guest broadcast/wait-state protocol. Record kernel/libc versions and classify unsupported paths as SKIP.
6. Test concurrent thread exit, handle duplication/close, and last-thread process shutdown for no lost wakeups, no stale TID reuse and no prematurely freed guest objects.

## Decision gate

**Do not** implement NT thread waiting by directly exposing a Linux TID, `pthread_join`, or `clear_child_tid` futex word. A durable guest thread object is a plausible design, **not a proven implementation**. No Windows-reference or Linux-runtime measurements were made for this note.
