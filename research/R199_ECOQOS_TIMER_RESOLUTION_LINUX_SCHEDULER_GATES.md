# R199 — Windows EcoQoS and timer-resolution policy versus Linux scheduler hints

Status: DOCUMENTATION RESEARCH ONLY (2026-11-20). No Windows-reference execution, Linux experiment, performance result, or architecture selection.

## Scope and verified contracts

**VERIFIED — Microsoft:** `SetThreadInformation(ThreadPowerThrottling)` supports opting a thread into `THREAD_POWER_THROTTLING_EXECUTION_SPEED`; Microsoft calls this EcoQoS on Windows 11 and LowQoS on earlier systems. The OS *tries* to favor efficiency (potentially lower frequency or more efficient cores); this is not a promise of core selection, a fixed clock, or a CPU-time quota. Without explicit opt-in, Windows may infer QoS heuristically. [Microsoft SetThreadInformation](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadinformation).

**VERIFIED — Microsoft:** `SetProcessInformation(ProcessPowerThrottling)` offers process-level execution-speed policy and a distinct `PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION` policy. The latter makes existing timer-resolution requests ineligible for guaranteed higher timer resolution; disabling it restores consideration of prior requests. Windows 11 may ignore timer-resolution requests for fully occluded/minimized/nonvisible and inaudible window-owning processes. The documentation describes conditional policy, not a portable exact timer frequency. [Microsoft SetProcessInformation](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation).

**VERIFIED — Linux:** `sched_setattr` utilization clamps `UCLAMP_MIN/MAX` are scheduler performance hints (0–1024), not Windows QoS levels or strict CPU-time quotas. Their effective values are constrained by cgroup and system settings; placement impact depends on scheduler/topology and frequency impact depends on governors. The kernel explicitly documents that a capped task sharing a runqueue with an uncapped task may not receive the desired frequency cap because runqueue clamps aggregate by maximum. [Linux scheduler utilization clamping](https://docs.kernel.org/scheduler/sched-util-clamp.html). Energy-aware scheduling additionally depends on an asymmetric topology and an available energy model. [Linux scheduler sysctl documentation](https://docs.kernel.org/admin-guide/sysctl/kernel.html).

## Architectural implications (hypotheses, not verified implementations)

**HYPOTHESIS:** Keep a guest process/thread QoS policy ledger separate from host scheduler settings. Preserve whether a guest explicitly opted in, opted out, or left policy unspecified; avoid conflating this with job-object CPU rate control (R133), priority, or hard resource enforcement.

**HYPOTHESIS:** A backend may *consider* uclamp and topology-aware placement for EcoQoS, but cannot claim equivalence without platform-specific validation. It must avoid applying global cpufreq settings to enforce one guest thread's request, because that would change unrelated host workload behavior.

**HYPOTHESIS:** Treat timer-resolution suppression as a guest timer-service policy decision, not merely a scheduler clamp. Integrate it with guest timer registration and effective-resolution bookkeeping; never assume `clock_nanosleep` or Linux timer slack alone reproduces Windows foreground/visibility heuristics.

## Proposed conformance and security gates (NOT RUN)

1. On identified Windows 10 and Windows 11 builds, compare process and thread opt-in/opt-out/unspecified settings, querying documented state where available. Record API return codes, effective scheduling observations, and power/performance traces separately; do not infer guarantees from average frequency.
2. Exercise a thread with EcoQoS in a process without explicit policy and the reverse; establish precedence experimentally rather than assuming it.
3. Test process timer-resolution requests before/after enabling ignore-timer-resolution, including restoration after disable, with foreground, occluded, inaudible and audible cases. Record wakeup distributions, not a single latency sample.
4. On Linux, test homogeneous and heterogeneous machines with documented kernel version, scheduler, governor, EAS availability, cpuset, cgroup policy, uclamp settings, and concurrent uncapped tasks. Record effective settings and power/latency variance.
5. Ensure guest requests cannot alter global host power policy, escape cgroup delegation, or modify another guest's thread. Ensure thread exit, process exit and handle reuse do not leak policy.
6. Test interaction with R133 job CPU quotas and R176 clock translation: distinguish policy hints, enforced CPU budget and timer precision.

## Open questions

- What precedence and inheritance rules apply to process versus thread EcoQoS across supported Windows builds? Requires Windows reference tests.
- Which Linux hardware/driver combinations make uclamp an effective energy hint rather than a no-op? Requires capability probes.
- How should a guest desktop compositor supply foreground/visibility/audio status to timer policy without trusting unvalidated guest claims?
- What are the version-specific Windows timer-resolution interactions with high-resolution waitable timers? Requires a separate source audit.

## Relationship to existing research

R133 addresses **job-object CPU budget and weights**, not EcoQoS; R176 addresses **counter/clock translation**, not suppression of timer-resolution requests. This document deliberately does not repeat either implementation analysis.

Sources reviewed 2026-11-20: Microsoft SetThreadInformation and SetProcessInformation reference pages; Linux upstream scheduler utilization-clamping and kernel sysctl documentation (URLs above). Version-dependent behavior remains OPEN.
