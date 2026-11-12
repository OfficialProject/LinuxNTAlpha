# R42 — Suspend/resume and thread-context APIs: a cross-kernel correctness gate

Date: 2026-11-12. Status: **PRIMARY-SOURCE DOCUMENTATION REVIEW; NO EXPERIMENT**.

## Why this is separate

R24/R25 establish synthetic GS-based thread state, R26 exercises a single intercepted guest syscall, and R30/R41 discuss unwind and exception handling. None demonstrates Windows-compatible **external thread suspension, context capture/modification, or resume-count behavior**. These are observable NT/Win32 contracts, not incidental Linux scheduler operations.

## VERIFIED FROM MICROSOFT API DOCUMENTATION

1. [SuspendThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-suspendthread) increments a target thread's suspend count, returns the **previous** count on success and `(DWORD)-1` on failure. Its documentation says the function is primarily intended for debuggers, warns of deadlock when suspending a thread holding a synchronization object, and notes a maximum suspend count.
2. [ResumeThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-resumethread) decrements a positive suspend count and returns the **previous** count; if the count is zero, the thread remains runnable. The API documentation explicitly distinguishes a positive remaining count from actual resumption.
3. [GetThreadContext](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext) warns that a valid context cannot be obtained for a running thread and recommends suspending the target first. Calling it on the current thread may succeed but return an invalid context. The `CONTEXT` flags determine which register subsets are requested.
4. [SetThreadContext](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext) similarly requires a suspended target for predictable results and states that some context values cannot be specified or are silently corrected by the OS. This is not an unrestricted write to all machine state.
5. [Wow64GetThreadContext](https://learn.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-wow64getthreadcontext) provides a separate 32-bit-context interface for WOW64 threads; a 64-bit-only prototype must not imply WOW64 support.

These links describe public API contracts; they do **not** prove any specific Linux implementation, exact internal NT scheduling algorithm, or Windows-build-specific race behavior.

## Architectural consequences — HYPOTHESES, NOT VERIFIED IMPLEMENTATION

- A per-guest-thread **logical suspend count** is needed independently of Linux task scheduling state. Mapping one `SuspendThread` to one POSIX signal or `SIGSTOP` is not sufficient: signals are not count-bearing Windows thread handles, and process-directed stop semantics differ.
- A safe guest context representation must include the requested x64 register subsets, guest instruction/stack pointers, guest GS/TIB expectations, and any required floating-point/vector state. The host's signal `ucontext_t` is not automatically equivalent to Windows `CONTEXT`.
- Context writes should be staged at a controlled guest execution boundary rather than modifying a running Linux thread from another host thread without a synchronization protocol. Such a protocol must account for a thread blocked inside a Linux host syscall, inside an NT-service emulation, or inside a signal handler.
- A guest suspend request arriving while the target holds a compatibility-runtime lock can deadlock the runtime. This is a compatibility and engineering hazard, not grounds to silently change the Windows contract.
- Linux `ptrace` might support a **debugger-only** backend, but using it as the ordinary in-process suspension mechanism is an unproven, potentially intrusive design choice. No backend is selected here.

## Proposed falsifiable Windows-reference tests — NOT RUN

1. Create one worker with a monotonic counter and a deterministic synchronization gate. From a second thread, record initial `SuspendThread`, repeated suspend, first resume and final resume return values; confirm the worker remains stopped until the count reaches zero. Avoid inferring exact stop latency from unsynchronized counter reads.
2. With the worker suspended at a known loop/function, request `CONTEXT_CONTROL`, `CONTEXT_INTEGER`, and relevant floating-point/vector flags separately; record returned fields and errors on a pinned Windows x64 build.
3. Try `GetThreadContext` on a running worker and on the calling thread, recording actual outcomes without treating success as proof of valid registers.
4. Exercise a controlled `SetThreadContext` redirect to a second known instruction location on a disposable Windows VM; compare requested versus returned state and final execution trace. No arbitrary code injection.
5. Test a worker blocked on a synchronization wait, a worker in a system call, and a worker using compiler TLS. Compare observable suspension and resumption, avoiding deadlocks in the test harness by using watchdog processes.
6. Later LinuxNTAlpha fixture: implement a logical suspend-count state machine and a *cooperative* guest safe point; verify counts and state transitions first, then separately test asynchronous guest suspension and context fidelity. Label cooperative-only behavior as an incomplete subset, not full `SuspendThread`.

## OPEN

- Exact NT-native `NtSuspendThread`/`NtResumeThread` access-check, return-status and prior-count details need primary native API evidence and Windows oracle tests; Win32 documentation alone does not settle them.
- Races among termination, suspension, APC delivery, context changes, guest/host ABI transitions and SUD signals are untested.
- How to preserve xstate/AVX, debug registers and guest GS while allowing glibc host TLS and Linux signal delivery is unresolved.
- Performance, security, and compatibility of any proposed Linux backend have not been measured.

## Decision

Keep thread-control semantics in the NT compatibility architecture backlog. Do **not** equate Linux thread signals or debugger ptrace stops with Windows suspension. No code was compiled or run for this note, and no NT/Windows compatibility claim follows from it.
