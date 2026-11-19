# R189 — Windows debug-event ownership versus Linux ptrace stop lifecycle

Status: RESEARCH ONLY (2026-11-19). No Windows or Linux runtime experiment performed. Scope: public Win32 debugging contracts; **not** a verified specification of undocumented native NT debug-object syscalls.

## Verified primary-source contracts

1. Microsoft's debugger-loop documentation states that a debug event suspends threads in the debuggee and that the debugger continues execution with `ContinueDebugEvent`. It documents context and memory inspection/editing while stopped. [Microsoft, Writing the Debugger's Main Loop](https://learn.microsoft.com/en-us/windows/win32/debug/writing-the-debugger-s-main-loop).
2. Microsoft's `WaitForDebugEvent` reference restricts the wait to the thread that created the debuggee, documents finite/zero/infinite waits and Unicode opt-in through `WaitForDebugEventEx`, and assigns debugger-side cleanup responsibility for CREATE_PROCESS/LOAD_DLL image-file handles. Debug-event process/thread handles have distinct cleanup timing on exit-event continuation. [Microsoft, WaitForDebugEvent](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-waitfordebugevent).
3. First-chance exception notification precedes frame-based handler search; an unhandled exception can yield second-chance notification. Continuation status decides whether exception search resumes. [Microsoft, Debugger Exception Handling](https://learn.microsoft.com/en-us/windows/win32/debug/debugger-exception-handling).
4. `DebugSetProcessKillOnExit` controls whether connected debuggees are terminated or detached when the calling debugger thread exits; it affects current and future connections of that thread. [Microsoft, DebugSetProcessKillOnExit](https://learn.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-debugsetprocesskillonexit).
5. Linux `ptrace` uses tracee stops observed by `waitpid` or related waits, tracer-directed continuation, optional signal injection/suppression, and options for clone, exec and exit notifications. `PTRACE_EVENT_EXIT` occurs before final exit; ptrace attach and authorization are subject to Linux credentials/security restrictions. [Linux man-pages project, ptrace(2)](https://man7.org/linux/man-pages/man2/ptrace.2.html).

## Architecture implications (hypotheses, not implementation decisions)

- **HYPOTHESIS:** A guest debug-session manager must own a per-debuggee event queue, outstanding-event continuation token, debugger-thread identity, guest handle references, and process/thread stop state. Merely forwarding Linux `SIGTRAP` and `waitpid` statuses cannot prove Win32 event-order, all-thread suspension, first/second-chance exception or handle-lifetime equivalence.
- **HYPOTHESIS:** ptrace could serve as a diagnostic/backend mechanism for selected events, but would require explicit arbitration with any host tracer and guest debugging rights. A self-managed guest exception/loader event stream may avoid ptrace overhead for normal execution; this is not a measured performance finding.
- **OPEN:** Which native debug-object operations and security descriptors are necessary for target applications beyond documented Win32 APIs? Do not infer undocumented `NtWaitForDebugEvent` behavior from Win32 wrapper contracts.
- **OPEN:** How to guarantee a consistent guest-wide stop when host threads are in uninterruptible I/O or the runtime broker is processing a guest syscall? An implementation must not claim atomicity without proof.
- **OPEN:** Whether guest debug attach can be offered safely when host ptrace is blocked by Yama, seccomp, namespace restrictions or another tracer.

## Proposed conformance matrix (not executed)

1. On a controlled Windows reference VM: create under debugging, attach to an existing process, enumerate process/thread/DLL creation and exit events; log event order and which thread may wait/continue.
2. Trigger handled and unhandled access violations and breakpoints; record first-chance and second-chance events and continuation effects without conflating guest exception delivery with Linux signals.
3. Duplicate and close file/process/thread handles around creation, DLL-load and exit events; test continued access and leak-free cleanup, keeping independent handles for observation.
4. Terminate the debugger with kill-on-exit true/false, including multiple debuggees; verify process lifecycle and authorization.
5. On Linux, separately probe ptrace clone/exec/exit stops, per-thread stop coverage, wait-status races, tracer conflict and policy denials. Record kernel version, namespaces, credentials, Yama policy and exact ptrace options.
6. Compare externally visible guest results and event trace against Windows. Report PASS/FAIL/SKIP, environment and raw logs; do not treat a Linux-only ptrace success as NT compatibility.

## Security and sequencing gates

Debug attach must be authorized against **guest** process identity and rights, not host PID alone. Event continuation must validate debugger ownership and outstanding event generation to avoid stale continuation after guest PID/TID reuse. Debugger-owned file handles must not leak host paths or bypass the guest file-access model. Any cross-process memory/context editing must be independently authorized. A host tracer must never be implicitly granted guest debugging privileges.

## Decision impact

Keep debug-event delivery as an explicit compatibility subsystem in the architecture comparison; do not select ptrace as a transparent substitute for NT debug objects. Advance only after a Windows-reference event/exception/ownership trace and a Linux backend feasibility trace exist. No experiment or benchmark was performed for this note.

Sources accessed 2026-11-19.