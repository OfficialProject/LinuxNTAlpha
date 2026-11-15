# R104 — Windows timer-resolution requests versus Linux timer scheduling

Date: 2026-11-15. Status: **VERIFIED FROM PRIMARY DOCUMENTATION; NO EXPERIMENT**.

## Scope and nonduplication

This note concerns process timer-resolution policy and the separate semantics of waitable timers, not R39's thread-pool callback lifecycle, R75's wait-all atomicity, R88's APC dispatch or R103's CPU Set scheduling policy. A low host timer interval is not evidence of better game frame times.

## Verified contracts

1. Microsoft [timeBeginPeriod](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod) requests a minimum periodic timer resolution; callers must balance each successful request with a corresponding `timeEndPeriod` call at the same period. A period outside the supported range returns `TIMERR_NOCANDO`. Microsoft documents a behavioral boundary at **Windows 10 version 2004**: before it, requests affect a global setting; starting with it, the request no longer guarantees higher resolution for processes that did not request it. On **Windows 11**, a fully occluded/minimized/invisible or inaudible window-owning process is not guaranteed the requested higher resolution. This is explicitly version- and visibility-sensitive.
2. Microsoft's [timer-resolution guidance](https://learn.microsoft.com/en-us/windows/win32/multimedia/timer-resolution) directs callers to `timeGetDevCaps` to determine supported `wPeriodMin`/`wPeriodMax` and warns that timer-resolution requests can affect power and system performance. Microsoft's timeBeginPeriod reference explicitly says this does **not** increase high-resolution performance-counter accuracy.
3. Microsoft [SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex) describes an independently addressable timer **object** with manual-reset versus synchronization-timer signaling behavior, periodic rearming, optional APC completion routine and `TolerableDelay` in milliseconds. Reprogramming an active timer does not itself signal it. Absolute timers are affected by system-time changes. These are not simply properties of a global timer-resolution knob.
4. Linux [time(7)](https://man7.org/linux/man-pages/man7/time.7.html) documents **per-thread timer slack**: certain blocking operations may be delayed to coalesce wakeups. Linux [timerfd_create(2)/timerfd_settime(2)](https://man7.org/linux/man-pages/man2/timerfd_create.2.html) expose file-descriptor timers, relative/absolute expiration, interval and expiration counts. Timerfd readiness/expiration counting alone does not provide Windows waitable-timer object identity, manual-reset consumption, APC callback delivery or NT handle access checks.
5. Linux timer slack is **not** a documented implementation of Windows `timeBeginPeriod` process policy. In particular, changing `PR_SET_TIMERSLACK` on a host thread does not by itself reproduce Windows version-dependent process-level request aggregation, occlusion rules, or the semantics of all guest wait APIs. Linux time(7) specifies only certain affected blocking operations.

## Architectural implications — HYPOTHESES

Maintain two distinct layers: (a) guest-visible resolution request bookkeeping with process and OS-version policy; (b) guest timer objects with NT identity, signaling, reset and optional APC completion semantics. Linux high-resolution timers/timerfd could serve as a scheduling backend, but a guest object broker must implement missing semantics. Host timer slack could be tuned only after verifying the exact blocking paths and host-wide energy/latency effects; it is not a correctness substitute.

Do **not** equate a successful Windows request with a promise that Linux will wake at that interval. Also do not indiscriminately apply a low host timer slack or realtime scheduling to every thread. Resolution, scheduling latency, APC delivery, and measured application frame pacing are different observables.

## Differential falsification gates — NOT EXECUTED

1. On Windows 10 pre-2004 (if available), 2004+ and Windows 11, query `timeGetDevCaps`; issue nested and unmatched request sequences in a disposable test process; record return codes, effective wakeup distributions and OS build. Do not assume undocumented unmatched-call behavior.
2. Compare requesting and nonrequesting processes and visible versus fully occluded Windows 11 processes. Record whether each process owns a window, audio activity and power state. Separate *documented guarantees* from observed behavior.
3. Create manual-reset and synchronization waitable timers; test one-shot/periodic signaling, two competing waiters, cancel/rearm, absolute due times, clock changes in a disposable VM, and APC delivery with alertable versus nonalertable waits. Record exact return codes and wait outcomes.
4. On Linux, record `uname -a`, clock source, `clock_getres`, `PR_GET_TIMERSLACK`, timerfd flags, kernel configuration and cgroup constraints. Compare `clock_nanosleep`, timerfd expiration counts and wait wakeup distributions across default and permitted per-thread slack values; no root or host-global tuning.
5. Include idle power, wakeups/second, p50/p95/p99 wakeup lateness and frame-time tails in a matched workload. A timer-resolution request is **not** an established performance optimization.

## Decision gate

Implement guest policy and waitable-timer semantics independently of the host timer primitive. Select timerfd or another Linux mechanism only after conformance checks, especially manual-reset/auto-reset consumption, rearm, absolute-time behavior and APC ownership. No benchmark, Windows-reference run or Linux experiment was performed for this note.

Primary sources inspected 2026-11-15:
- https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
- https://learn.microsoft.com/en-us/windows/win32/multimedia/timer-resolution
- https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex
- https://man7.org/linux/man-pages/man7/time.7.html
- https://man7.org/linux/man-pages/man2/timerfd_create.2.html
