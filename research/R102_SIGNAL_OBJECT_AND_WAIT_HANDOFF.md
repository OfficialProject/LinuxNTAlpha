# R102 — SignalObjectAndWait handoff ordering and non-atomicity

Date: 2026-11-15. Status: **PRIMARY-DOCUMENTATION ANALYSIS; NO EXPERIMENT**.

## Scope and distinction from R75

R75 concerns wait-any/wait-all eligibility and consumption *within* a multi-object wait. This note examines a different API: `SignalObjectAndWait`, which signals one object and then waits on a second. Despite its description as a single operation, **Microsoft explicitly disclaims atomicity between the signal and the wait**. Do not import R75's wait-all transaction requirement into this operation.

## Verified documentation contracts

1. Microsoft's [SignalObjectAndWait](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-signalobjectandwait) documents signal targets as event, semaphore or mutex. A mutex signal requires ownership; otherwise `ERROR_NOT_OWNER` is reported. The wait target requires `SYNCHRONIZE` rights and may be a different supported waitable type.
2. The same documentation explicitly states that signaling and waiting **are not guaranteed to be atomic**: another processor can observe the first object's signaled state before the caller begins waiting on the second. Therefore a Linux backend must not claim an indivisible two-object transaction as a Windows contract.
3. The API supports an alertable flag and distinct `WAIT_IO_COMPLETION`, `WAIT_ABANDONED`, `WAIT_OBJECT_0`, `WAIT_TIMEOUT` and `WAIT_FAILED` outcomes. Alertable completion invokes Windows APC/completion semantics, not generic Linux `EINTR`.
4. Microsoft's documentation cautions against combining this call with `PulseEvent` because of possible missed signaling/deadlock; an implementation must not promise that `SignalObjectAndWait` closes that race.
5. Linux [futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html) guarantees atomic *compare-and-block on one 32-bit futex word*. It does not document an atomic signal-one-NT-object-and-wait-on-another primitive. This does not preclude building the API in a compatibility runtime.

These are **verified statements about the cited API contracts**, not measurements or claims of Windows implementation internals.

## Architectural hypothesis

Implement the two operations under a common guest-object rights and lifetime layer: validate handles and signal ownership, apply the signal transition, then enter the wait dispatcher with its own APC, timeout and abandonment handling. A runtime may choose internal synchronization to prevent *its own* lost-wakeup bugs, but must not assume a Windows-observable atomic signal/wait handoff. Conversely, non-atomicity is not permission to lose an already-persistent signal on the second object.

An implementation using two Linux calls may be viable for a subset of object types, but correct handling of guest object state, cross-process references, signal ownership and APC delivery remains unproven. No performance ranking of Linux futex, ntsync or a broker is justified by this documentation.

## Proposed reference conformance tests — NOT EXECUTED

- **Ownership failure:** signal a mutex not owned by the caller, capture `WAIT_FAILED` and `GetLastError`, and confirm the wait object is not accidentally consumed; determine precise ordering of validation/side effects experimentally rather than assuming it.
- **Basic handoff:** one thread signals an event and waits on a second event; another thread observes the first and signals the second. Repeat with manual- and auto-reset events and finite timeouts.
- **Non-atomic window:** use coordinated contenders to observe whether a third thread can run after the first signal and before the wait registration. The documentation permits the window; a particular outcome must not be required in every trial.
- **Semaphore bounds:** signal at maximum count and record error and second-object state; check whether failed signal causes any wait registration.
- **Alertable wait:** queue a user APC to the caller and record callback execution, `WAIT_IO_COMPLETION`, and remaining signal/wait-object states.
- **Abandoned wait target:** terminate the owner of the target mutex, then record `WAIT_ABANDONED` and resulting ownership.
- **Cross-process:** use named event/semaphore objects with separately opened handles; test rights failures and concurrent close without assuming undefined handle-close behavior is deterministic.
- **Timeout and cancellation:** test zero/finite/infinite waits and timeouts at transition boundaries, recording object state independently.

Record Windows build, bitness, exact handles/access rights, outcomes, final states, and repeated trials. A Linux model test should first prove absence of unintended token consumption, deadlock and missed *persistent* notifications. Only then compare against the Windows reference.

## Decision impact

**Do not require an atomic two-object kernel syscall for this API solely because its name suggests atomicity.** This is a narrow, source-backed correction to a potential architecture assumption. It does **not** resolve R75 wait-all atomicity or validate a particular runtime design.

Sources accessed 2026-11-15:
- Microsoft, SignalObjectAndWait API contract: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-signalobjectandwait
- Linux man-pages, futex(2): https://man7.org/linux/man-pages/man2/futex.2.html
