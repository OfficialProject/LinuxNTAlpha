# R36 — Windows job objects versus Linux cgroup v2: compatibility boundaries

Research date: 2026-11-12. Status: **documentation review only; no Windows or Linux conformance experiment performed**.

## Question

Can Linux cgroup v2 directly serve as the NT/Win32 job-object implementation for LinuxNTAlpha, including nested membership, breakaway, termination, accounting, and resource limits?

## Verified from primary documentation

1. Microsoft describes job objects as named, securable, shareable objects for managing associated processes. A process normally cannot leave an assigned job. Starting with Windows 8 / Server 2012, a process may belong to a **hierarchy** of nested jobs, not arbitrary independent job sets. Child processes normally inherit membership, subject to explicit and silent breakaway policies. [Microsoft: Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects); [Nested Jobs](https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs).
2. Explicit job assignment has access-right and topology constraints; assignment to a job with an exceeded active-process limit may fail **and terminate the target process**. Nested limits do not all combine identically: some use the most restrictive ancestor, others the immediate job. [Microsoft: AssignProcessToJobObject](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject).
3. A job's process and job memory limits concern **committed virtual memory**, and are independent; these cannot be assumed equivalent to Linux resident-memory accounting. [Microsoft: JOBOBJECT_EXTENDED_LIMIT_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_extended_limit_information).
4. Linux cgroup v2 organizes processes in a single hierarchical tree. A process belongs to **one** cgroup at a time; migration is supported, and new children inherit their parent's current cgroup. Ancestor controllers constrain descendants. These properties do not by themselves provide NT object handles, immutable membership, job breakaway rules, or NT accounting. [Linux kernel: cgroup v2](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html).
5. Linux pidfds provide stable process references and pollable termination observation. They do **not** themselves implement job-wide membership or resource enforcement. [Linux man-pages: pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html).
6. Windows job completion-port messages and nested notification propagation are separately documented. Do not equate Linux cgroup event files with Windows completion-port packet ordering or delivery without reference tests. [Microsoft: JOBOBJECT_ASSOCIATE_COMPLETION_PORT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).

## Architectural inference — not yet validated

**Candidate:** a userspace NT job-object authority (handles, ACLs, hierarchy, membership, policy, accounting and notifications) backed selectively by delegated Linux cgroup v2 controllers, with pidfds for per-process lifetime tracking.

This is a **partial backend**, not a drop-in translation. Linux cgroup membership can be migrated while Windows job membership is persistent; Windows nested jobs are an NT object hierarchy while Linux cgroups are a single resource tree; Linux memory controller accounting is not automatically Windows committed-memory accounting. Job object close/kill behavior also requires handle-lifetime ownership semantics that a cgroup directory alone does not provide.

Potential design boundary: keep **authoritative Windows-visible membership and counters** in the NT subsystem; treat cgroups as optional enforcement mechanisms where a specific limit has been proven equivalent, and otherwise enforce or explicitly reject unsupported limits. Avoid claiming strict enforcement if an unprivileged deployment cannot obtain cgroup delegation.

## Conformance matrix to execute later (not performed)

| Gate | Windows reference observation | LinuxNTAlpha acceptance criterion |
| --- | --- | --- |
| Basic job lifecycle | Create/open named job, duplicate/close handles, query membership | Same status, access checks and lifetime behavior |
| Nested assignment | Root-to-leaf assignment; invalid topology and UI-limit cases | Same outcomes and effective ancestry |
| Child inheritance | Ordinary spawn, explicit breakaway, silent breakaway | Correct membership under each policy |
| Assignment failure | Active-process limit exceeded and target process state | Match failure and target termination behavior |
| Memory limits | Per-process versus aggregate **commit** with query results | No substitution of RSS for committed bytes |
| Termination | TerminateJobObject and kill-on-last-handle-close | Verify per-member exit and final-handle ownership |
| Notifications | Associate completion port; process-exit/active-zero events | Match documented packet identity, timing constraints and permitted losses |
| Delegation failure | Linux host lacks writable cgroup v2 controllers | Explicit capability reporting; no false claim of enforcement |

For each gate, record Windows build, Linux kernel and cgroup mount/delegation configuration, API arguments, expected and observed statuses, and repeated outcomes. Add adversarial cases for concurrent spawn, assignment, exit and handle closure.

## Open questions

- Can an atomic creation path ensure that a Windows-target process never runs outside the intended NT job hierarchy while retaining Linux driver and process behavior? Compare Linux clone3 CLONE_INTO_CGROUP with Windows process creation/job assignment, but do not presume semantic identity.
- Which limits require custom committed-memory accounting, and what kernel mediation would make enforcement race-free?
- How will object handles, duplicate handles, access checks, process identity and completion notifications be synchronized without lock-order deadlocks?
- What is the security boundary for an untrusted child that can invoke native Linux process/cgroup operations outside NT API wrappers? A userspace-only policy must not be called a sandbox.

## Decision gate

**Do not select cgroup v2 as the complete job-object implementation.** Prototype a narrow job authority with explicit feature flags only after Windows-reference tests define the required semantics. This recommendation is an architectural hypothesis, not a benchmark or proof of feasibility.

Sources accessed 2026-11-12. No code imported; no experiments executed.
