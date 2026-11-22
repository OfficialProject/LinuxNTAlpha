# R232 — Guest process identity across host supervisor failure and restart

Date: 2026-11-22. Status: **architecture synthesis from previously reviewed primary documentation**. No new Windows-reference run, Linux execution, fault injection, or benchmark was performed.

## Question and nonduplication

R73 establishes waitable process objects and durable exit codes; R67 establishes process-relative handle duplication; R194 discusses bootstrap publication; R195 covers asynchronous I/O ownership. This note addresses a distinct **cross-component failure boundary**: what remains observable and authorized when a Linux host worker or its NT service broker dies while a guest process has published handles, waits and outstanding operations. It does not assert that Windows itself uses a broker or that recovery is mandatory for an initial prototype.

## VERIFIED — documented external contracts (not experiments)

1. Windows process objects become signaled on termination, and a process handle can be queried for its exit code; an exit value of 259 is ambiguous without separate liveness evidence. [Microsoft: Terminating a Process](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process), [GetExitCodeProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess). This establishes a guest-visible obligation, **not** a specific host-crash exit-code translation.
2. Duplicated Windows handles reference the same object but occupy process-relative handle tables; cross-process duplication requires access checks on source and target processes. [Microsoft: DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle). Therefore copying an integer handle into a restarted worker is not sufficient proof of authority.
3. Linux pidfds provide host process identity and pollable exit notification, while `waitid(P_PIDFD)` is restricted to children of the caller and ordinary `waitpid` status truncates normal exit codes to eight bits. [Linux: pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html), [wait(2)](https://man7.org/linux/man-pages/man2/wait.2.html). A pidfd alone cannot preserve a guest 32-bit status or an NT object after the supervising process loses its state.
4. Windows overlapped I/O cancellation does not guarantee cancellation; completion remains an independently observable outcome. [Microsoft: CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/fileio/cancelioex-func), [GetQueuedCompletionStatus](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus). A broker restart must not silently transform a prior request into a successful completion or deliver two terminal notifications.
5. Linux pidfd operations and fd duplication have Linux-specific authorization and lifetime rules; `pidfd_getfd` does not perform Windows target-process handle insertion. [Linux: pidfd_getfd(2)](https://man7.org/linux/man-pages/man2/pidfd_getfd.2.html).

These sources are primary vendor/kernel-interface documentation cited in R67, R73 and R195. They have **not been freshly re-fetched in this run**; source/version drift is an explicit follow-up. No source establishes Windows behavior for an invented Linux broker crash.

## HYPOTHESIS — minimal ownership model

Separate (A) **guest authority** (process identity, token, per-process handle table, object references), (B) **guest observable state** (process terminal status, waits, outstanding I/O results, completion queues), and (C) **host execution state** (pidfd, Linux task, fd, io_uring request). A guest object must never be reconstituted from a host PID or raw fd alone.

Proposed invariants for an implementation review:

- **G1: identity generation.** Every guest process and kernel-like object has an internal generation-safe identity; stale handles or backend completion tokens cannot attach to a replacement worker.
- **G2: explicit failure policy.** Define one of two initial architectures: fail-stop the *entire guest session* on broker death, or maintain an independently owned journal/state service capable of recovery. Do not promise transparent broker restart without durable authority and completion state.
- **G3: terminal publication.** Each guest process publishes at most one terminal transition; a Linux worker crash may require a documented synthetic guest termination outcome, but its exact NTSTATUS is **OPEN** pending Windows-reference policy decisions.
- **G4: authority before replay.** Recovery must never reissue a duplicated handle or pending I/O merely because a numeric guest handle, PID or fd matches; revalidate object generation, granted access, target process identity and resource ownership.
- **G5: no fabricated completion.** A backend operation with an unknown outcome after broker loss is not automatically a completed guest operation. Either retain enough authoritative state to reconcile it or apply the explicit fail-stop policy.
- **G6: teardown is bounded.** Avoid retaining zombie host tasks solely to keep guest process objects queryable; reap and retain guest terminal records separately (R73).

The simplest **candidate for an alpha**, not a selected architecture, is fail-stop per guest session with isolated host workers and a supervisor that detects worker death, revokes session capabilities, closes backing resources and records a distinct terminal reason. Whether an NT-style guest exit code should be surfaced for a supervisor failure is unresolved.

## OPEN — falsifiable tests, NOT RUN

| Gate | Proposed procedure | Failure to detect |
| --- | --- | --- |
| Worker crash while another guest process holds a process handle | Inject SIGKILL into an isolated host worker; query surviving guest wait and exit-code behavior against an explicitly chosen failure policy | indefinite wait, reused PID identity, invented status |
| Broker crash during cross-process duplicate | Inject failure before/after target-table publication and source-close processing | leaked authority, duplicate visibility, invalid source-close ordering |
| Broker crash after backend I/O completion but before guest packet publication | Replay/reconcile under a fault-injected mock backend; compare request generation and delivery log | duplicate or lost terminal result |
| Guest handle reuse during worker restart | Repeatedly close/reallocate handles while old completion tokens arrive | stale token controls new object |
| Host PID reuse and child reaping | Exercise pidfd-backed tracking on a recorded kernel, with SKIP if unavailable | attaching old guest identity to a new Linux task |
| Resource exhaustion | Bound pending requests, object records, journal retention and restart retries | unbounded memory/fd growth |

For any Windows differential test, record Windows build, process architecture, access rights, exact status, and object lifetime. For Linux tests, record kernel version, namespaces, pidfd feature availability and exit/reaping ordering. None of these gates has a PASS/FAIL result yet.

## Architecture decision impact

**OPEN:** Choose an initial broker failure domain and whether any component is restartable before implementing cross-process handle duplication, wait publication and asynchronous completion delivery. This is an ownership/security decision, not a benchmark optimization. A single-process prototype can postpone restartability, but must explicitly prohibit stale identity reuse and define what a host crash does to the entire guest session.

Related work: [R67](R67_CROSS_PROCESS_HANDLE_DUPLICATION.md), [R73](R73_PROCESS_WAIT_PIDFD_EXIT_STATUS.md), [R194](R194_PE64_BOOTSTRAP_PUBLICATION_GATES.md), [R195](R195_ASYNC_REQUEST_OWNERSHIP_COMPLETION_GATES.md).
