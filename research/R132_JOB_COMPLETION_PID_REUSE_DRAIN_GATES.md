# R132 — Job-object completion packets, PID reuse and drain observability

Research date: 2026-11-16 (US Eastern). Status: **primary documentation review only; no code or Windows/Linux experiment executed**.

## Narrow question and relation to existing work

R36 already rejects a direct Windows-job-to-cgroup mapping. This note isolates a different integration hazard: whether an NT job supervisor can treat completion-port notifications as an authoritative process roster or proof that every process has exited. It does **not** replace R36's job membership, limits, or cgroup analysis; nor does it replace R15's general IOCP model.

## VERIFIED — Microsoft documentation

1. A job can associate **one** I/O completion port; messages appear to consumers through `GetQueuedCompletionStatus` with the configured completion key, the job message identifier in the bytes-transferred field, and message-specific data in the overlapped field. In a nested job hierarchy, events can be forwarded to ports of ancestor jobs. [JOBOBJECT_ASSOCIATE_COMPLETION_PORT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).
2. **Except for limits set with `JobObjectNotificationLimitInformation`, delivery is not guaranteed.** In particular, absence of a process exit or active-process-zero packet must not be interpreted as evidence that no such event occurred. The same Microsoft structure documentation warns that a process ID carried by a message can already refer to a terminated process or have been reused unless an open process handle prevents reuse. [JOBOBJECT_ASSOCIATE_COMPLETION_PORT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).
3. Associating the completion port while the job is inactive reduces the chance of missing messages during association. The documented `JOB_OBJECT_MSG_NEW_PROCESS` case includes processes already associated at association time; this does **not** remove the general nondelivery warning. [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects); [JOBOBJECT_ASSOCIATE_COMPLETION_PORT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).
4. Job lifetime and job-member lifetime differ. Without kill-on-close, a job is destroyed when its last handle closes **and** its processes have terminated; with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, closing the last job handle terminates associated processes. An ancestor/child job hierarchy affects termination scope. [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects); [Nested Jobs](https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs).
5. Nested job membership and effective limits are hierarchical; parent-job ports can receive events caused by descendants. Consequently the completion key identifies the associated job/port registration, not necessarily a unique leaf job or an eternally live process. [Nested Jobs](https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs); [JOBOBJECT_ASSOCIATE_COMPLETION_PORT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).

## VERIFIED — Linux documentation

6. Cgroup v2 `cgroup.events` exposes a `populated` indicator for a cgroup **or its descendants**, and cgroup v2 provides a hierarchical membership/control model. That indicator is not an NT completion packet with a guest process ID or completion key. [Linux kernel cgroup v2](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html); [cgroups(7)](https://man7.org/linux/man-pages/man7/cgroups.7.html).
7. Linux pidfds support process-identity/lifetime observation; they are distinct from the numeric PID and from cgroup membership. Their availability does not establish Windows job event-delivery equivalence. [pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html).

## HYPOTHESIS — proposed broker design, not implemented

Maintain an authoritative **guest job membership ledger** with stable guest process identities, generation numbers and process-handle references. Feed an independently modeled NT IOCP packet queue from state transitions, preserving Microsoft's **permitted packet loss** where relevant rather than falsely upgrading every message to a guaranteed durable event. Use host pidfds to reconcile individual Linux process exits and `cgroup.events` as an additional aggregate signal where delegation exists. Neither cgroup population nor receipt/nonreceipt of one completion packet should alone certify Windows-visible job quiescence.

A shutdown protocol should freeze or serialize authorized admission, request termination according to the guest job's rights/policy, then reconcile the membership ledger against observed host process lifetimes. This is a **candidate** protocol: Linux cgroup migration, host-side spawning, PID namespace boundaries, native syscall bypasses and broker crashes can defeat it without an independently verified enforcement boundary.

## OPEN — Windows reference and Linux backend tests to design

| Gate | Windows reference experiment (not run) | LinuxNTAlpha acceptance question |
| --- | --- | --- |
| Attach port before/after admission | Record NEW_PROCESS and exit/active-zero packets across repeated attach races | Can guest packet fields and allowed nondelivery be represented without losing authoritative membership? |
| Reuse process identifiers | Exit members, create replacements, retain/close process handles; record IDs and packets | Are stale packets resolved by stable guest identity rather than numeric PID alone? |
| Nested notification routing | Attach distinct ports to parent/child jobs and create/exit leaf members | Are ancestor notifications and completion keys routed correctly? |
| Last-handle close | Duplicate job handles, close in different orders, toggle kill-on-close | Does guest handle reference counting control termination independently of host cgroup directory lifetime? |
| Drain without packets | Observe job accounting/membership after exits while deliberately delaying dequeue | Can the supervisor determine quiescence without treating packet absence as proof? |
| Host boundary | Compare delegated/nondelegated cgroup v2 and pidfd support, concurrent fork/exit | Does backend fail explicitly when it cannot enforce guest containment? |

Record Windows build, API error/status, exact handle rights, port packet tuple, process handles held, Linux kernel version, pidfd/cgroup delegation and repeat counts. Do not report any test as PASS before execution.

## Decision implication

Do **not** build job membership or kill-on-close around IOCP packets or cgroup population alone. Treat notifications as observable guest events, stable membership as separate state, and host enforcement as a third independently tested boundary. This narrows R36's unresolved job-supervision design without choosing a kernel architecture.

Primary sources checked 2026-11-16. No external code copied.
