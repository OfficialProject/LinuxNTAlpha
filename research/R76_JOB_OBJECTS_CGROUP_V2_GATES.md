# R76 — Windows job objects versus Linux cgroup v2: membership, lifetime and notifications

Status: **VERIFIED FROM SOURCE** for the cited API contracts; **HYPOTHESIS** for the proposed architecture; **OPEN** for Windows-reference conformance and Linux runtime tests. No experiment was executed.

## Why this is a separate gate

R73/R74 cover individual process/thread waitable-object lifetime; R75 covers multi-object waits. A Windows *job object* is a distinct named, securable, shareable object that groups processes, constrains resources, retains accounting for exited members, and can deliver completion-port notifications. Neither a pidfd nor a cgroup directory is automatically that NT object.

## Source-grounded contracts

1. **Membership and hierarchy.** Microsoft says job membership cannot be removed once assigned; child processes normally inherit membership, with explicit/silent breakaway policies. Windows 8 / Server 2012 added nested jobs. `AssignProcessToJobObject` requires `JOB_OBJECT_ASSIGN_PROCESS` on the job and `PROCESS_SET_QUOTA | PROCESS_TERMINATE` on the process. Assignment to a job with an exhausted active-process limit can fail *and terminate the target process*. In nested jobs, different resource classes have different inheritance rules; do not flatten all limits into one cgroup setting. [Microsoft Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects); [AssignProcessToJobObject](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject).
2. **Lifetime and waiting.** Microsoft documents job destruction after the final handle closes *and* all associated processes terminate, except `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, which kills associated processes on last-handle close (including descendants for a nested job). The job becomes signaled when all processes terminate **because the end-of-job time limit was exceeded**; ordinary cgroup emptiness must not be assumed equivalent to the job's waitable signaled state. [Microsoft Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects).
3. **Notification is not a reliable event log.** A job can attach an I/O completion port; Microsoft explicitly warns most job completion-port notifications are **not guaranteed**, except the stated notification-limit class. Nested-job messages propagate to ports on the parent chain. One job can have one associated completion port. [Microsoft Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects); [JOBOBJECT_ASSOCIATE_COMPLETION_PORT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).
4. **Linux substrate is not a Windows job API.** The Linux v6.12 cgroup-v2 authoritative kernel documentation describes hierarchical resource-control files and process membership, including `cgroup.procs`, `cgroup.events`, `cgroup.kill`, and controllers such as `pids`, `memory`, `cpu`, and `io`. These expose kernel cgroup operations, not Windows handle access masks, job-object notification packets, nested-job limit semantics, or NT object lifetime. [Linux v6.12 cgroup-v2 documentation](https://github.com/torvalds/linux/blob/v6.12/Documentation/admin-guide/cgroup-v2.rst).

## Architecture hypothesis (not implemented)

Keep a userspace **job-object authority** responsible for NT object identity, handles/access checks, nested membership graph, inherited membership at guest process creation, guest-visible accounting snapshots, wait state, and IOCP packet generation. Consider cgroup v2 as an **optional enforcement backend** for a subset of resource controls, under an explicitly delegated and confined subtree. Do not expose raw host cgroup paths as guest handles.

A mapping must be feature-by-feature, not one-to-one: Windows working-set, commit-charge, process-time, scheduling, and UI restrictions are not interchangeable with Linux `memory.max`, `cpu.max`, `pids.max` or other controllers. Host cgroup permissions/delegation and availability may make enforcement impossible; in that case do not claim conformance.

## Falsifiable test plan (not run)

- **Windows reference:** create two nested jobs; test allowed and disallowed reassignment, inherited child membership, explicit and silent breakaway, and active-process-limit assignment failure (including target survival). Record OS build and each Win32 error.
- **Lifetime:** hold duplicated job handles across processes, close them in different orders, compare normal close against kill-on-close, and check wait behavior on ordinary emptying versus end-of-job-time-limit termination.
- **Notifications:** attach ports before members; record packet keys/message IDs for create/exit/limit events and nested propagation. Do not assert that missing ordinary notifications imply no event.
- **Linux backend:** on a disposable delegated cgroup-v2 subtree, inspect supported controllers and permissions; test process inheritance, migration, `pids.max`, `cgroup.events`, and `cgroup.kill` without root or host-global changes. Mark unsupported controllers **SKIP**. Compare each observation to the Windows reference rather than treating a cgroup test as NT conformance.
- **Security:** attempt guest path escape, cross-guest cgroup mutation, unauthorized job-handle operations and race conditions between last-handle close, member exit and concurrent assignment.

## Decision gate

Do **not** choose cgroup v2 as the sole job-object implementation. First establish a Windows-reference behavior matrix and which Linux controls can enforce each relevant limit without violating host isolation. This note records contracts and test design only; no performance or compatibility result is claimed.
