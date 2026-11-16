# R117 — NT job-object membership versus Linux cgroup v2

Research date: 2026-11-16. Status: **VERIFIED FROM SOURCE** for cited API contracts; **HYPOTHESIS** for proposed implementation; **OPEN** for reference conformance. No experiments performed.

## Architectural question

Can a Linux cgroup v2 hierarchy directly implement Windows nested job objects, including process membership, resource limits, breakaway, notifications and lifetime?

**Finding:** cgroups are useful host enforcement primitives but are not a drop-in NT job-object implementation. Guest job identity, handles, access checks, nested membership, accounting and completion-port notifications require a separate compatibility model.

## Verified source contracts

1. Microsoft documents Windows 8+ nested jobs, child-process inheritance by default, explicit and silent breakaway flags, and job-associated completion-port notifications. Ordinary notifications are not generally guaranteed; notification-limit messages have stronger delivery guarantees. [Microsoft: Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects).
2. `AssignProcessToJobObject` requires job assignment and process quota/termination rights. Assigning a process already in a job is constrained by the existing nested hierarchy; the API documents effective-limit precedence and parent aggregation. [Microsoft: AssignProcessToJobObject](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject).
3. Windows job active-process limit counts active **processes**, and the job memory limit is specified against the sum of committed memory. [Microsoft: JOBOBJECT_BASIC_LIMIT_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_limit_information).
4. Nested job messages propagate to associated completion ports in the parent chain; Microsoft cautions that most job notifications are not guaranteed. [Microsoft: JOBOBJECT_ASSOCIATE_COMPLETION_PORT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port).
5. Linux cgroup v2 places each process in one cgroup in a single hierarchy, with hierarchical controller restrictions and migration support. The pids controller counts kernel tasks/TIDs rather than Windows process objects; `pids.max` failure on fork/clone is `EAGAIN`. [Linux upstream cgroup v2 documentation](https://docs.kernel.org/admin-guide/cgroup-v2.html).

## Mismatch matrix

| Contract | Linux candidate | Gap / gate |
| --- | --- | --- |
| Nested job membership and handle access | Broker-owned job graph plus cgroup hierarchy | cgroup membership alone cannot encode guest handle rights or all assignment restrictions |
| Active **process** limit | `pids.max` | Linux pids counts tasks including threads; a multithreaded guest can exhaust the limit prematurely |
| Job commit-charge limit | `memory.max` | Host resident/memory-controller accounting is not equivalent to Windows committed virtual memory; avoid claiming semantic parity |
| Job completion-port messages | Broker IOCP queue | cgroup events do not directly provide Windows message IDs, completion keys or guaranteed/unguaranteed delivery contracts |
| Breakaway and nested inheritance | Guest process-creation policy plus controlled cgroup placement | Must enforce parent-chain breakaway permissions and avoid host cgroup escape |
| Job object lifetime / termination | Guest handle graph, process membership and host termination primitives | Need precise close-handle, kill-on-close and concurrent-exit ordering tests |

## Hypothesis: split guest semantics from host resource controls

Maintain an NT job graph in a broker, with reference-counted guest job objects, per-process job-chain membership, guest-visible rights and completion-port notification routing. Use delegated cgroup v2 controls only for resource restrictions whose semantics can be demonstrated equivalent or conservatively stricter **without breaking guest compatibility**. Implement active-process admission in the broker using guest process counts; do not substitute `pids.max` as the guest-visible active-process limit. Host cgroups can remain an independent safety ceiling, explicitly reported as such.

This is a design proposal, not a measured performance or correctness result. A privileged host cgroup manager must not grant guest processes arbitrary cgroup filesystem writes.

## Falsifiable tests (not run)

1. On a Windows reference system, create nested jobs and attempt legal/illegal assignment into an existing chain; record Win32 errors, handle rights and UI-restriction interactions.
2. Spawn one process with many threads under an active-process limit of two; compare with two single-thread processes. A `pids.max=2` translation should fail this distinction.
3. Exercise explicit and silent breakaway with different parent-chain policies; verify which ancestor jobs retain the child.
4. Associate distinct completion ports with nested jobs, trigger process exits and limit events, and record message IDs, keys, ordering and losses. Do not assert reliability beyond Microsoft's documented guarantees.
5. Compare guest committed-but-untouched virtual memory with cgroup `memory.current` and `memory.max`; test commit failures independently of RSS.
6. Test concurrent job-handle closure, process exit, kill-on-close and broker restart; verify no stale host process or guest-handle reuse.

## Open questions

- Exact modern Windows build-specific job CPU, memory and kill-on-close behavior requires reference tests.
- Whether delegated cgroup migration can be made race-free for guest process creation on the intended host is untested.
- How broker crashes reconcile kernel-enforced ceilings with guest job lifecycle remains unimplemented.
- No compatibility, security or performance result is claimed here.

## Source version caveat

Microsoft Learn and docs.kernel.org URLs above are live documentation, retrieved 2026-11-16, not immutable version pins. Future implementation work must pin Linux kernel source revision and record the tested Windows build before treating details as version-specific.
