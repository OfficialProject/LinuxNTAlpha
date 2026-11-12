# R33 — Waitable timer semantics and Linux timer backends

Date: 2026-11-12. Status: **primary-documentation review only; no experiment executed**.

## Scope

R6/R18 examine ntsync synchronization objects; R13 examines APC and alertable waits. This note addresses a distinct unresolved boundary: Windows waitable **timer objects**, their signaling and callback lifecycles, and timer-resolution policy. A Linux timer source is not by itself a compatible Windows dispatcher object.

## Verified from primary Microsoft documentation

1. **Timer object type and access.** `CreateWaitableTimerExW` creates or opens a waitable timer. The manual-reset flag selects a notification timer; otherwise the timer automatically resets after releasing a single waiting thread. Named timers may be shared across processes and handles inherited or duplicated. The high-resolution creation flag is documented for Windows 10 version 1803 and later. [CreateWaitableTimerExW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw).
2. **Object namespace matters.** `CreateWaitableTimerW` documents case-sensitive timer names, the Global/Local namespace convention, and name collisions with other named synchronization-object types yielding `ERROR_INVALID_HANDLE`. A private Linux file descriptor cannot directly reproduce those object-manager rules. [CreateWaitableTimerW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerw).
3. **Arming and rearming are state transitions.** `SetWaitableTimer` specifies that rearming an active timer stops then restarts it, without signaling waiting threads, and cancels pending completion routines. Periodic manual-reset timers remain signaled after their initial expiration until reset; synchronization timers become nonsignaled when a waiter consumes the signal. Absolute due times track system-time adjustments. [SetWaitableTimer](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer).
4. **Callback and signal delivery are separate.** An optional timer completion routine is queued as a user APC for the thread that armed the timer; execution occurs when that thread enters an alertable wait. Timer expiration also changes the timer object's signaled state. `SetWaitableTimerEx` adds a tolerable-delay argument and documents cancellation when the setting thread exits. [SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex); [APC overview](https://learn.microsoft.com/en-us/windows/win32/sync/asynchronous-procedure-calls).
5. **Resolution is not universally global.** Microsoft documents a change in `timeBeginPeriod` behavior starting Windows 10 version 2004, and a further visibility/occlusion qualification on Windows 11. Increased resolution can affect power use; it does not improve high-resolution performance-counter precision. [timeBeginPeriod](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod).
6. **Wait interruption matters.** Microsoft's kernel wait documentation distinguishes alertable and nonalertable waits, user APC interruption and thread termination; `STATUS_USER_APC` is not interchangeable with a successful timer-object wait. [Waits and APCs](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/waits-and-apcs).

## Architecture hypotheses — not verified implementations

- **HYPOTHESIS:** Model timer identity, access rights, signaling mode, due-time clock domain, period, cancellation generation, arming-thread identity and APC delivery separately from the host timing mechanism. A Linux `timerfd`, POSIX timer or scheduler thread may provide expiration notifications, but cannot be assumed to implement Windows object, wait, namespace or APC semantics.
- **HYPOTHESIS:** A per-process timer-resolution policy should be version-profiled and independent of `QueryPerformanceCounter` emulation; avoid unconditionally raising Linux system timer frequency.
- **HYPOTHESIS:** Centralized dispatcher bookkeeping can make timer waits compose with events/semaphores and wait-multiple operations; whether ntsync can represent any relevant timer state directly is **OPEN** and requires a version-pinned Linux UAPI/source audit.
- **OPEN:** Exact missed-period/coalescing behavior, clock discontinuities, simultaneous waiter fairness, cancellation races, NT-native timer APIs, Windows build differences and Linux backend precision remain unmeasured.

## Proposed conformance and measurement gates (NOT run)

1. On a documented Windows reference build, test manual-reset versus synchronization timers with one and several waiters, single-shot and periodic arming, rearm-before-expiry and cancel-before-expiry. Record wait results and signaled state without inferring fairness from one trial.
2. Test optional APC callbacks with alertable and nonalertable waits; separately test setting-thread exit, rearming, and multiple pending expirations. Record callback thread identity and return statuses.
3. Test absolute versus relative due times across controlled wall-clock changes in an isolated VM; compare `SetWaitableTimer` and `SetWaitableTimerEx` tolerable delays.
4. Test named-object collisions and handle duplication/inheritance across processes, including access-mask denial; connect outcomes to R10/R14.
5. On equivalent hardware, benchmark Linux backend candidates with distributions of wake-up latency, CPU use and power under load; do **not** infer Windows compatibility from backend latency alone.
6. Compare version-specific `timeBeginPeriod` behavior under visible and occluded process states, while separately measuring `QueryPerformanceCounter` behavior.

## Decision impact

Do not collapse Windows timer handles into Linux timer file descriptors or claim a timer backend is complete merely because expiration works. The first useful deliverable is a deterministic state-machine model and reference conformance matrix, followed by host-backend experiments. This review does **not** establish application compatibility, latency improvement or a need for kernel changes.

## Primary sources (reviewed 2026-11-12)

- Microsoft, CreateWaitableTimerExW: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw
- Microsoft, CreateWaitableTimerW: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerw
- Microsoft, SetWaitableTimer: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer
- Microsoft, SetWaitableTimerEx: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex
- Microsoft, Asynchronous Procedure Calls: https://learn.microsoft.com/en-us/windows/win32/sync/asynchronous-procedure-calls
- Microsoft, timeBeginPeriod: https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
- Microsoft, Waits and APCs: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/waits-and-apcs
