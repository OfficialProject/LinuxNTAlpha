# R13 — Alertable waits, APC delivery and multi-object synchronization

Date: 2026-11-12. Primary-source research; no conformance test executed.

## Verified API contract
Microsoft WaitForMultipleObjectsEx documentation states:
- It waits for one or all synchronization objects, timeout, or user APC / I/O completion if alertable.
- The handles must have SYNCHRONIZE access; duplicate handles in the array are prohibited; closing a handle while waiting yields undefined behavior.
- Wait-any chooses the lowest-index signaled handle. Wait-all does not consume object state until all objects can be satisfied; mutex ownership is not acquired early.
- Return values distinguish WAIT_OBJECT_0 + index, WAIT_ABANDONED_0 + index, WAIT_TIMEOUT, WAIT_FAILED and WAIT_IO_COMPLETION.
- An APC or completion routine is executed in the initiating thread when an alertable wait permits it. ReadFileEx/WriteFileEx completion routines require alertable waits by the initiating thread.
- Waitable object types include events, mutexes, semaphores, processes, threads, timers, console input and notifications.

Primary reference: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjectsex (retrieved 2026-11-12).

## Implication: ntsync is not an entire wait subsystem
Even if ntsync supplies atomic synchronization primitives, LinuxNTAlpha must integrate APC queues, per-thread alertable state, Windows object-handle access checks, process/thread termination, waitable timers and I/O completion. A single ioctl cannot be assumed to cover all types or the alertable contract.

## Candidate architecture (HYPOTHESIS)
1. Resolve NT handles and enforce SYNCHRONIZE rights before waiting.
2. Map supported synchronization objects to kernel ntsync resources where semantics match.
3. Maintain a per-thread APC queue and an explicit wake mechanism.
4. Coordinate APC arrival and object satisfaction without lost wakeups; deliver APC callbacks on the target thread, not in an arbitrary broker thread.
5. Translate completion into precise Windows return codes, preserving abandoned mutex behavior and array ordering.
6. Handle timer, process and thread handles via an adapter with atomic cross-type wait semantics, not by separately polling and racing to report a result.

## Failure modes to test
- APC enqueued before, during and after entering alertable wait.
- Nonalertable wait must not run user APC callbacks.
- APC callback reentrancy, nested alertable waits, callback throws an exception, and thread exits.
- Multiple signaled handles, abandoned mutexes and simultaneous APC.
- Wait-all must not consume any object until all are ready.
- Cross-type wait with process termination and event signal.
- Close/duplicate races, timeout boundary, thread cancellation and queue starvation.

## Architecture gate
Do not advertise Win32 wait compatibility merely because an ntsync semaphore/event benchmark passes. The acceptance suite must exercise alertable waits and heterogeneous object arrays, and performance results must be reported separately from correctness.
