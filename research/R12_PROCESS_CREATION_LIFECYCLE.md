# R12 — Windows process creation and Linux lifecycle mapping

Date: 2026-11-12
Method: Microsoft Learn and Linux manual primary documentation; no prototype or benchmark.
Status: VERIFIED semantics, HYPOTHESIS architecture, OPEN conformance gaps.

## Primary sources
- https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes
- https://learn.microsoft.com/en-us/windows/win32/procthread/inheritance
- https://learn.microsoft.com/en-us/windows/win32/sysinfo/handle-inheritance
- https://learn.microsoft.com/en-us/windows/win32/procthread/process-handles-and-identifiers
- https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects
- https://learn.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-peb
- https://learn.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-rtl_user_process_parameters
- https://man7.org/linux/man-pages/man2/clone.2.html

## VERIFIED: process creation contracts
- CreateProcess returns handles for both the new process and its primary thread. These handles can outlive process/thread termination until closed. A process ID is not a substitute for an open process handle.
- A Windows child may inherit selected handles, environment variables, current directory, console association, error mode, affinity and job association; not every property is inherited.
- Windows handle inheritance is opt-in and handles refer to the same underlying object, with the same access rights and value in the child. A handle-list process attribute can restrict the inherited set.
- Process parameters include image path and command-line strings. Microsoft's public PEB and RTL_USER_PROCESS_PARAMETERS layouts are deliberately incomplete and may change; do not assume public header layouts are a complete stable ABI.
- Windows job objects manage groups of processes, resource limits and termination behavior. Nested jobs and breakaway semantics introduce process-tree behavior beyond ordinary parent/child relationships.
- Linux clone/clone3 expose kernel process/thread creation options and can optionally return pidfds; they do not construct Windows PEB/TEB or automatically translate Windows object handles.

## Design hypothesis: explicit NT process broker
Maintain a compatibility process record containing NT process identity, primary-thread identity, handle table, inherited resources, Windows command line, environment, current directory, security context, job associations and exit status. Back it with Linux processes/threads and pidfds where available. Never expose Linux PIDs as equivalent to Windows handles.

## Required correctness gates
1. Parent creates child with zero inherited handles, then a controlled handle allowlist; verify identity, access rights and absence of leaks.
2. Verify process and primary-thread handles remain valid after exit until final close, including PID reuse.
3. Verify Windows command-line quoting, UTF-16 environment layout and per-drive current directories; never substitute Linux argv/envp behavior without tests.
4. Validate suspended-process startup and resume semantics before advertising CREATE_SUSPENDED support.
5. Model termination and job membership including nested jobs, breakaway, kill-on-close and resource accounting.
6. Validate security token and access-check behavior separately from Linux UID/GID mapping.
7. Test race conditions for process exit, handle duplication, concurrent waiters and close.

## Negative findings
- pidfd solves only part of process identity and lifetime; it is not a complete Windows process object.
- A successful PE entry-point jump does not prove CreateProcess, Windows process environment, or job-object compatibility.
- Public PEB/RTL_USER_PROCESS_PARAMETERS definitions alone cannot guarantee compatibility with undocumented application expectations.

## Next investigation
Prototype a minimal process lifecycle with an explicit handle broker and deterministic fixture; separately audit token/security descriptor and APC/alertable wait behavior.
