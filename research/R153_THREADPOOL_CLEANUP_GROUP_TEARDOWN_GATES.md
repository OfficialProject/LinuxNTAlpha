# R153 — Threadpool cleanup-group membership and teardown gates

Status: **VERIFIED FROM SOURCE** for cited Microsoft API contracts; **HYPOTHESIS** for LinuxNTAlpha design; **OPEN** for observed behavior and implementation. Reviewed 2026-11-18. No experiment was run.

## Scope and nonduplication

R149–R152 address individual work, wait, and timer callback lifetimes. This note addresses **aggregate cleanup-group ownership**, membership creation races, and the distinction between callback completion and freeing callback-generating objects. It does not claim the Linux host threadpool is API-equivalent.

## Verified contracts (Microsoft primary documentation)

1. `CreateThreadpoolCleanupGroup` creates an opaque group. Creating work, wait, timer, or I/O objects with the associated callback environment adds them as members; their respective close functions remove them. Source: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolcleanupgroup
2. `SetThreadpoolCallbackCleanupGroup` associates a cleanup group and optional cleanup callback with a callback environment; the function is inline. The environment is not itself the group. Source: https://learn.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-setthreadpoolcallbackcleanupgroup
3. `CloseThreadpoolCleanupGroupMembers(group, cancel, context)` releases the group's member objects and blocks for running callbacks. With `cancel=TRUE`, callbacks not yet started are canceled; with `FALSE`, outstanding callbacks are allowed to complete. After return, the caller must not use or individually close the released members. The group survives the call and can be reused. Source: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolcleanupgroupmembers
4. Microsoft explicitly documents a race between creating members and closing existing members: new objects may be added before, during, or after the close operation, and subsequent use of released objects can raise an exception. This is **not** a documented atomic freeze of all future membership. Same source as (3).
5. `CloseThreadpoolCleanupGroup` requires the group to have **no members**; closing the group does not substitute for releasing its members. Source: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolcleanupgroup
6. `CloseThreadpoolWait` may free asynchronously when callbacks remain; Microsoft directs callers requiring callback quiescence to disarm, wait/cancel callbacks, then close. Cleanup-group member closure provides the aggregate release path. Source: https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolwait

## Architectural hypothesis

Represent each guest callback object with independent identity, generation, in-flight callback references, queued callback references, group membership, and an explicit closing state. Associate the object with the group **at successful creation**, not at first submission. Group-member close should snapshot/transition the selected membership set under a synchronization boundary, stop future submissions for those objects, cancel only not-yet-started callbacks when requested, and wait for executing callbacks before returning. Retain memory until all internal references drain. New members created concurrently require a deliberately specified ordering rather than silently assuming a global freeze.

A Linux worker pool, eventfd, timerfd, or io_uring completion mechanism may supply scheduling but does not itself implement these guest object lifetimes. This is a proposed design, **not** a demonstrated compatibility or performance result.

## Falsification / differential test plan (not executed)

- Windows reference: create a cleanup group, two work objects, one wait and one timer; record membership creation, callback entry/exit, optional cleanup callback, group-member close return, and explicit close attempts. Test both cancellation values, with a blocked running callback and queued work.
- Create a member before any submission and confirm group-member close owns its release. Separately test disarmed waits and never-armed timers.
- Serialize a concurrent creator and closer with barriers across many schedules; classify only behavior allowed by Microsoft's documented creation race. Never deliberately dereference a freed guest pointer in a production test process; isolate invalid-use probes in disposable child processes.
- Confirm group reuse after member closure, then empty-group closure. Record whether callback environment destruction requires independent cleanup.
- Compare against a LinuxNTAlpha prototype with generation-tagged identities and sanitizers. Assert no use-after-free, no double cleanup, and no callback execution after aggregate close returns for the members actually released.
- Report Windows build, toolchain, Linux kernel, source revision, counts, event trace, and all skips. Do not infer callback ordering where the public API leaves it unspecified.

## Open questions

- Precise timing and count of the optional per-member cleanup callback under concurrent submission and implicit one-shot callback objects require Windows-reference validation; the public pages above do not establish every interleaving.
- Interaction with `DisassociateCurrentThreadFromCallback`, callback library pinning, and recursive cleanup needs separate primary-source review and deadlock tests.
- Whether a dedicated guest scheduler materially improves latency over Linux primitives requires measured workload traces, not extrapolation from this contract.

## Research frontier

Read R149–R152 before implementation. Next inspect Microsoft's `DisassociateCurrentThreadFromCallback`, `SetThreadpoolCallbackLibrary`, and `TrySubmitThreadpoolCallback` contracts, then build a deterministic mock scheduler to exercise cancellation/closure ownership before running a Windows differential executable.
