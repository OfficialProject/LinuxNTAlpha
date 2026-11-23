# R266 — Windows debug-event authority versus Linux ptrace stop semantics

Date: 2026-11-23. Status: **documentation synthesis; no experiment performed**. This note covers Win32 debugger-facing contracts, not an assertion that undocumented NT debug-object internals have been replicated.

## Question and architectural significance

Can a Linux ptrace tracer serve as the complete implementation of Windows process debugging for independently executed PE64 code? **Not by direct substitution.** Windows exposes a process-wide ordered debug-event protocol with explicit continuation and guest handle ownership. Linux ptrace primarily reports *per-thread* trace stops through wait-family calls. A compatibility layer must own the guest-facing event queue, identities, rights, exception disposition and lifetime transitions. Whether ptrace is an appropriate backend remains an untested architecture choice.

This is separate from R41 (vectored exception handlers), R42 (thread suspension/context), R233 (host process topology), R245 (seccomp user notification) and R265 (cross-process memory access).

## Verified primary-source contracts

1. Microsoft [Debugging Events](https://learn.microsoft.com/en-us/windows/win32/debug/debugging-events) states that an event suspends all threads in the affected debuggee until continued. The initial CREATE_PROCESS event precedes user-mode execution; CREATE_THREAD precedes that thread's user-mode execution. The documented event types include process/thread creation and exit, DLL load/unload, exception, debug string and RIP event.
2. [WaitForDebugEvent](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-waitfordebugevent) specifies the debugger event wait, timeout behavior, and a creator-thread restriction. [WaitForDebugEventEx](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-waitfordebugeventex) opts into Unicode OutputDebugStringW handling; this distinction must not be silently erased.
3. [ContinueDebugEvent](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-continuedebugevent) accepts a process/thread pair and a continuation disposition. DBG_CONTINUE marks an exception handled; DBG_EXCEPTION_NOT_HANDLED resumes first-chance exception search and terminates after an unhandled second-chance exception. DBG_REPLY_LATER is version-gated (Windows 10 1507+). The creator-thread restriction is explicitly documented for a process created through CreateProcess; do not extrapolate it without testing to every attach configuration.
4. [DebugActiveProcess](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-debugactiveprocess) requires process access, creates synthetic initial process/thread/module events for attachment, and arranges a breakpoint after resumption. The documentation describes SeDebugPrivilege and debugger-exit behavior; these are guest policy requirements, not host ptrace permissions.
5. Microsoft [Debugging Events](https://learn.microsoft.com/en-us/windows/win32/debug/debugging-events) distinguishes debugger-owned event handles. The CREATE_PROCESS image-file handle and LOAD_DLL file handles must be closed by the debugger; debugger process/thread handles are closed through continuation of exit events. The EXIT_PROCESS event is not final kernel shutdown: the debugger must continue it, then can wait on a separately duplicated process handle to observe completion.
6. Linux [ptrace(2)](https://man7.org/linux/man-pages/man2/ptrace.2.html) says the tracee is one Linux thread, not an entire multithreaded process. PTRACE_SEIZE does not immediately stop the tracee; PTRACE_INTERRUPT requests a stop. Signal-delivery stops, group stops, clone/exec/exit notifications and waitpid state require separate handling. Ptrace access checks and attach behavior are Linux policy, not Windows debug authorization.

## Inferences / candidate design, not verified implementation

A guest debugging coordinator would map guest PID/TID to durable host identities; serialize guest event publication and continuation; enforce a process-wide debug stop barrier; distinguish first/second chance and guest exception dispatch; maintain event-specific handle leases; and coordinate loader, process-exit and debugger-detach state. **Hypothesis:** ptrace can supply some stop/register observations when guest processes have dedicated host thread groups, but it is neither a Windows debug-object replacement nor a proven safe default for all supported process topologies.

An event must not be published as process-wide stopped merely because one host thread reached ptrace-stop. Concurrent host thread creation and guest loader callbacks need an explicit freeze/acknowledgment protocol. Any such protocol must avoid deadlocks with guest syscall dispatch, signal handling and the debug coordinator itself.

A debugger must never receive unrestricted host ptrace or arbitrary host PID authority as a consequence of a guest SeDebugPrivilege or guest process handle. Access checks and targets must remain guest-scoped.

## Unrun conformance and falsification gates

- **D1 — event order and stop barrier:** On a version-pinned Windows reference, record CREATE_PROCESS, CREATE_THREAD, LOAD_DLL, EXCEPTION and exit order for a multithreaded target; assert no target guest instructions execute while a debug event is outstanding. Repeat with concurrent thread creation.
- **D2 — exception disposition:** Compare handled versus unhandled first-chance and second-chance breakpoints/access violations; ensure debugger continuation is not confused with guest vectored/SEH dispatch.
- **D3 — attach snapshot:** Attach to a running multithreaded process and verify initial thread/module event enumeration, null start-address cases, breakpoint delivery and detach/kill-on-exit policy.
- **D4 — event handle lifetime:** Track process, thread and DLL file handles across creation, DLL unload, thread exit, process exit and continuation. Retain an independent duplicated process handle to observe final shutdown.
- **D5 — thread affinity and Unicode:** Verify which debugger thread may wait/continue for create and attach paths; compare OutputDebugStringW behavior with WaitForDebugEvent and WaitForDebugEventEx on supported Windows builds.
- **D6 — host backend:** On a pinned Linux kernel, instrument ptrace seize/interrupt/wait/continue across 2+ host threads, signal races, clone and exit. Demonstrate a complete guest stop barrier rather than assuming ptrace gives one.
- **D7 — security:** Attempt guest PID reuse, stale handles, unauthorized debug attach, host PID guessing and ptrace escalation. Reject any access that would cross the guest security boundary.
- **D8 — topology:** Re-evaluate D1–D7 under both one-host-process-per-guest and any proposed shared-address-space topology before selecting a backend.

Record Windows build, Linux kernel, compiler, architecture, privilege context, event traces and negative results. None of D1–D8 has been executed for this note.

## Decision impact

Do not claim native Windows debugger compatibility from R19–R28 PE fixture execution or Linux ptrace availability. Before selecting an architecture, require a guest event coordinator design with explicit process-wide suspension, exception disposition, lifetime, security and host-thread mapping. **Open:** precise behavior for attach-path thread affinity and any lower-level NT debug-object APIs requires separate primary-source/source-level investigation and Windows-reference testing.
