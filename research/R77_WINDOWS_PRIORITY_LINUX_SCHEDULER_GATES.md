# R77 — Windows process/thread priority versus Linux scheduler controls

Status: **DOCUMENTED CONTRACTS** from the primary references linked below; **HYPOTHESIS** for the suggested NT authority; **OPEN** for version-specific Windows reference tests and Linux backend experiments. **No tests were run.**

## Distinct gap

R36/R76 address job-object limits and cgroups; R42 addresses thread suspension. Neither establishes a mapping for guest-visible Windows **process priority classes**, **relative thread priorities**, **priority boosts**, and **background processing mode**. A direct mapping of Windows priorities to Linux `nice` values would conflate different scheduler models and omit important state transitions.

## Primary-source contract inventory

1. **Priority is composed, not a single nice value.** Windows schedules threads using priority levels; a thread's base priority is determined by its process priority class and relative thread priority, with documented exceptions for real-time classes and dynamic boosts. The Windows scheduling documentation distinguishes base priority from dynamic priority. [Microsoft — Scheduling Priorities](https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities); [Microsoft — Priority Boosts](https://learn.microsoft.com/en-us/windows/win32/procthread/priority-boosts).
2. **Process class and thread-relative state have different APIs.** `GetPriorityClass`/`SetPriorityClass` operate on process handles, whereas `GetThreadPriority`/`SetThreadPriority` operate on thread handles. The process class values include idle, below-normal, normal, above-normal, high, and real-time classes. A compatibility layer must retain both guest-facing values rather than exposing a host scheduling policy as the API result. [SetPriorityClass](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass); [SetThreadPriority](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority).
3. **Background processing is a state transition, not merely a numeric priority.** `PROCESS_MODE_BACKGROUND_BEGIN` and `PROCESS_MODE_BACKGROUND_END` are documented for `SetPriorityClass`, and `THREAD_MODE_BACKGROUND_BEGIN`/`END` for `SetThreadPriority`. Microsoft documents effects beyond CPU scheduling, including resource-priority treatment. The process-mode controls apply to the calling process, and thread-mode controls to the calling thread; do not model them as freely assignable priority classes for arbitrary targets. [SetPriorityClass](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass); [SetThreadPriority](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority).
4. **Linux has several distinct mechanisms.** `nice`/CFS-style scheduling, real-time scheduling classes, affinity, and cgroup CPU control have distinct scopes and permission rules. The Linux scheduler API does not provide Windows base/dynamic priority and boost semantics automatically. In particular, a host `SCHED_FIFO` selection cannot be inferred to be a safe implementation of Windows `REALTIME_PRIORITY_CLASS`. [Linux sched(7)](https://man7.org/linux/man-pages/man7/sched.7.html); [Linux setpriority(2)](https://man7.org/linux/man-pages/man2/setpriority.2.html); [Linux kernel cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html).
5. **Permissions and error boundaries matter.** Priority changes require appropriate Windows process/thread handle rights and may be constrained by caller privileges. Linux scheduler changes may fail under host privilege and resource-limit policy. A host permission failure must not silently produce a false successful guest API result. [SetPriorityClass](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass); [SetThreadPriority](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority); [Linux sched(7)](https://man7.org/linux/man-pages/man7/sched.7.html).

## Architecture hypothesis — not implemented

Maintain guest **process priority class**, **thread-relative priority**, **background mode**, and **boost configuration** in a coherent NT process/thread state model. Resolve the guest-visible API contract independently of the host backend. A policy translator may request host `nice`, scheduling, or resource-control changes when permitted, but must explicitly distinguish: (a) accepted guest state, (b) actual host enforcement, and (c) unsupported or unsafe guarantees. Do not grant real-time host privileges to guest applications by default.

This is **not** a claim that storing guest state without enforcement makes scheduling behavior compatible. If host policy cannot satisfy an observable contract, that remains a conformance gap.

## Differential tests to design (not executed)

- **Windows reference matrix:** for each documented process class, set thread priorities `IDLE`, `LOWEST`, `BELOW_NORMAL`, `NORMAL`, `ABOVE_NORMAL`, `HIGHEST`, and `TIME_CRITICAL` where accepted; record `GetPriorityClass`, `GetThreadPriority`, errors, OS build, process rights and privileges. Do not infer numeric effective scheduler priority solely from the getter.
- **Background mode:** test calling-thread versus foreign-thread restrictions, repeated begin/end, interaction with explicit priority changes, and process versus thread mode. Record exact errors and restored state.
- **Boost behavior:** distinguish process/thread boost controls from static base-priority selection; use instrumented CPU/IO workloads to study *observable* wake-up behavior on Windows, without equating one latency measurement to a guaranteed priority level.
- **Linux isolated backend:** in a nonprivileged test environment, enumerate available policies, cgroup delegation, nice limits, and affinity. Record whether each requested mapping is accepted, rejected, or skipped; measure host interference and fairness under load. Never request `SCHED_FIFO` on a shared host as an automated default.
- **Adversarial cases:** guest requests real-time class, rapidly alternates priorities, spawns children with inherited scheduling attributes, and races termination against priority updates. Check that host privileges are never escalated and one guest cannot starve unrelated host workloads.

## Decision gate

Do not implement a universal `Windows priority = Linux nice` table. First capture Windows-reference state and behavior, then define an explicit, privilege-bounded Linux policy with honest unsupported cases. A compatibility result or performance benefit is **not established** by this research note.

## Evidence provenance

Primary references above are published Microsoft Learn API documentation, Linux manual pages, and Linux kernel documentation. This pass did **not** retrieve pinned source snapshots or execute experiments; details that vary by Windows build, Linux kernel, permissions, and CPU scheduler configuration remain open for verification. This note does not claim fresh source inspection beyond the links provided.
