# R228 — Job completion-port notifications versus cgroup v2 populated events

Status: **documentation research only**, 2026-11-21. No Windows reference test, Linux experiment, performance measurement, or implementation is claimed.

## Scope and prior-work boundary

[R36](R36_JOB_OBJECT_CGROUP_V2_GATES.md) covers broad job/cgroup membership, accounting and nesting. This note isolates a narrower **event delivery and termination-observation** question: whether a Linux cgroup can directly implement the observable completion-port protocol of a Windows job. It cannot be assumed to do so. This is not a claim that an independently implemented adapter is impossible.

## Verified primary-source contracts

1. Microsoft documents `JobObjectAssociateCompletionPortInformation` and messages including `JOB_OBJECT_MSG_NEW_PROCESS`, `JOB_OBJECT_MSG_EXIT_PROCESS`, `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO`, and limit notifications. The completion-port consumer calls `GetQueuedCompletionStatus`. The `lpOverlapped` value is used to carry a process identifier for certain messages; these are **job-specific packets**, not generic file I/O completions. [Microsoft, JOBOBJECT_ASSOCIATE_COMPLETION_PORT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).
2. Microsoft explicitly warns that delivery of ordinary job messages is **not guaranteed**; notifications for limits configured with `JobObjectNotificationLimitInformation` are the documented exception. Association during active process churn can miss events. A compatible implementation must not promise an exact once-per-process event stream merely because the Linux backend can observe exits. [Microsoft, Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects); [Microsoft, association structure](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).
3. For nested Windows jobs, relevant messages propagate to completion ports associated with parent jobs in the chain; resource accounting is aggregated. [Microsoft, Nested Jobs](https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs).
4. Linux cgroup v2 exposes `cgroup.events:populated` recursively. Its transition to zero can be observed via poll/inotify, indicating **no live processes** in that subtree. It does not itself encode Windows `NEW_PROCESS`, `EXIT_PROCESS`, job completion-port packet keys, or per-job limit violation classifications. The Linux documentation also notes that zombie processes do not appear in `cgroup.procs`; a process's cgroup association persists through exit until reaping. [Linux kernel, cgroup v2 — Processes and [Un]populated Notification](https://docs.kernel.org/admin-guide/cgroup-v2.html).
5. Linux controller enablement and delegated cgroup migration are conditional on hierarchy and permissions. A guest job hierarchy must not be assumed to map one-to-one onto a writable host cgroup hierarchy. [Linux kernel, cgroup v2 — Controlling Controllers and Delegation](https://docs.kernel.org/admin-guide/cgroup-v2.html).

## Architectural inference (not yet verified experimentally)

A **guest-owned job event ledger** is the safer candidate abstraction: record guest process membership transitions and guest job-limit state under the NT object manager; enqueue NT-compatible packets to a guest IOCP object; use Linux cgroups for selected resource control and `populated` as a secondary liveness signal. Do **not** synthesize an unconditional `EXIT_PROCESS` from a `populated=0` transition: the latter is subtree-wide, lacks per-process identity, and can collapse several exits into one transition. Conversely, do not treat absence of a Windows ordinary completion packet as proof that no event occurred.

The adapter needs an explicit rule for parent-job packet propagation, port association timing, guest process ID reuse, closing a job or completion-port handle, and transitions across breakaway/reassignment boundaries. It should preserve the documented guarantee distinction between notification-limit packets and other packets rather than upgrading every event to reliable delivery by accident.

## Proposed falsifiable acceptance gates (not executed)

- **Windows baseline:** associate the completion port before process creation; observe packets for a single child and multiple short-lived children, including packet identifier and completion key. Repeat with association during churn; report observed distributions, not an invented guaranteed ordering.
- **Nested jobs:** associate parent and child ports; verify propagation and process identifiers. Repeat with permitted breakaway settings and compare the resulting membership and notifications.
- **Limit classification:** configure notification limits separately from hard limits; query `JobObjectLimitViolationInformation` after notification. Verify which packet types are guaranteed by documentation versus merely observed in trials.
- **Linux backend:** test `cgroup.events` recursive populated transitions, rapid fork/exit, zombies, and migration under a delegated cgroup; separately track guest process generations. Confirm that a subtree-zero observation cannot replace a per-process packet.
- **Concurrency:** test port association, job close, process exit, PID reuse, and nested teardown races. Require a guest-owned generation-safe queue and document any unavoidable behavioral differences.

## Open questions

- Exact reference-Windows packet ordering under simultaneous child exit and job termination (documentation alone does not settle it).
- Which limits can be faithfully enforced using cgroup controllers versus requiring a separate guest accounting layer.
- Whether the intended initial runtime's process model (one Linux process per NT process or otherwise) makes cgroup-based isolation and accounting viable without privileges.

**Decision:** Do not implement Windows job completion-port notifications as a direct translation of Linux `cgroup.events`. Preserve guest job/IOCP ownership and validate with Windows differential tests before selecting a backend.
