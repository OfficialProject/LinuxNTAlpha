# R241 — futex_waitv wake index is not NT WaitAny selection authority

Date: 2026-11-22. Status: **primary-documentation research; NOT TESTED**.

## Scope and distinction

R102 addresses signal-object wait handoff; R54/R97 address WaitOnAddress; R166/R237 address keyed-event rendezvous. This note isolates a separate implementation trap: **a Linux futex_waitv return index is not a specification of the guest-visible winner of a Windows WaitForMultipleObjects(Ex) wait-any**, particularly when multiple heterogeneous dispatcher objects are signaled.

## VERIFIED FROM SOURCE — documented interface, not measured behavior

1. [Linux kernel futex2 userspace API](https://docs.kernel.org/userspace-api/futex2.html), inspected 2026-11-22, describes `futex_waitv` as waiting on up to 128 futex words, returning the index of **some** woken futex. If a value differs before blocking, the syscall rolls back the enqueues and returns `-EAGAIN`. The documented supported word size is 32 bits. The timeout is absolute, with CLOCK_MONOTONIC or CLOCK_REALTIME. These are Linux wait-word contracts, not NT object semantics.
2. [Microsoft WaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects), inspected 2026-11-22, specifies wait-any selection in ascending handle-array order when multiple objects are signaled; a wait-all must not consume object state until *all* requested objects are signaled. Some object types change state on successful wait (e.g. semaphore count decreases). The function accepts heterogeneous waitable objects.
3. [Microsoft WaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjectsex), inspected 2026-11-22, adds alertable APC completion with `WAIT_IO_COMPLETION`, and documents abandoned mutex result and ownership transfer. The Linux futex_waitv API documents neither NT APC delivery nor ownership/abandonment.
4. [Microsoft KeWaitForMultipleObjects](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kewaitformultipleobjects), inspected 2026-11-22, documents distinct kernel-mode wait outcomes including `STATUS_USER_APC`, `STATUS_ALERTED`, `STATUS_TIMEOUT`, and abandoned mutex status. Do not assume Win32 return codes and kernel NTSTATUS values are interchangeable.

The Linux futex2 page is unpinned current documentation. These findings are **documentation-level**, not a verified source revision or an assertion about the actual host kernel.

## HYPOTHESIS — backend notification versus guest selection

A futex_waitv result may serve as a **wake hint** for a guest dispatcher, but a separately synchronized guest object-state transaction must decide which object is eligible, which index wins, whether ownership transfers, and which object state is consumed. Recheck state under the guest dispatcher lock after wake. Do not return the Linux index directly as the guest WaitAny result.

For WaitAll, a sequence of successful independent futex wakeups is insufficient: guest state acquisition must be atomic with respect to competing waiters or rolled back without consuming any object. A futex word by itself does not encode process/thread termination, abandoned ownership, APC queue state, or object-specific access rights. This is a design hypothesis, not proof that a userspace dispatcher meets latency or correctness requirements.

## OPEN — falsifiable differential matrix (NOT RUN)

| Scenario | Required comparison |
| --- | --- |
| Two manual-reset events already signaled, reversed wake order | Windows reference selected handle index vs backend wake index; require guest result to follow Windows selection contract |
| Auto-reset event plus semaphore signaled simultaneously | Exactly which state changes, and whether losing object remains available |
| WaitAll on semaphore and mutex with competing waiter | No partial consumption while second object unavailable; eventual ownership and counts |
| Abandoned mutex plus signaled event | Windows return-code priority and ownership, with exact index |
| Alertable wait with APC queued during concurrent signal | Windows reference ordering and return, not Linux signal interruption guessed as APC |
| Guest handle close or object destruction during pending wait | Document undefined Windows cases separately; prevent host UAF regardless |
| Futex word changed before enrollment | Handle `-EAGAIN` with recheck; no lost wake |
| Timeout and wake at boundary | Compare guest clock policy and observed result across repetitions |
| Cross-process shared futex mapping | Verify shared/private flags and object identity across distinct address spaces |

Record Windows build and architecture, exact handles/object initial states, per-thread ordering, raw result, and repeated outcomes. Record Linux kernel release, futex2 availability, syscall errno, clock, shared mappings, and source revision. The experiments above were **not executed**.

## Architecture impact

**OPEN:** A generic futex_waitv-backed multiwait cannot be declared an NT dispatcher implementation until its index-selection, atomic consumption, APC, abandoned mutex and cross-process semantics are independently demonstrated. An event-driven userspace broker, ntsync-assisted backend, and any proposed kernel extension must be compared against the *same* guest-facing matrix. No performance conclusion or kernel-boundary decision follows from these documents.

## Evidence limits

Primary vendor/kernel documentation was consulted on 2026-11-22. No pinned Linux kernel source audit, Windows VM trace, local test, code change, benchmark or PASS/FAIL observation is claimed.
