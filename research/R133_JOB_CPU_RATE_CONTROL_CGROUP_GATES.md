# R133 — Windows job CPU rate controls versus Linux cgroup v2 scheduling

Research date: 2026-11-17. Status: **primary-source contract review only**. No Windows reference run, Linux cgroup experiment, scheduler benchmark, or implementation occurred.

## Scope and nonduplication

R36 and R76 establish that a Windows job object cannot be represented solely by a Linux cgroup; R132 examines notifications and drain. This note isolates **CPU-rate enforcement**, including weight, hard cap, min/max reservation, hierarchy and scheduler-class gaps. It does not reopen general job identity or membership design.

## Verified contracts

1. Microsoft's `JOBOBJECT_CPU_RATE_CONTROL_INFORMATION` is set/queryable using `JobObjectCpuRateControlInformation`. `ENABLE` must accompany the relevant mode. `WEIGHT_BASED` uses a relative **1–9** weight (default 5); `HARD_CAP` stops job threads after exhausting the interval; `MIN_MAX_RATE` reserves a minimum and caps a maximum. The latter cannot be combined with weight-based or hard-cap mode. `CpuRate` is specified in **1/10,000 of processor cycles**; e.g. 2000 means 20%. A zero CpuRate is rejected. [Microsoft CPU rate control](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_cpu_rate_control_information).
2. Microsoft states nested job CPU rate controls apply relative to the **immediate parent's allocated rate** when the parent has CPU control; otherwise the rate is relative to the whole system. It also documents that total minimum reservations must not exceed 100% for them to work correctly, and a Remote Desktop Services DFSS incompatibility. These are Windows-visible policy constraints, not cgroup defaults. [Microsoft CPU rate control](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_cpu_rate_control_information).
3. `SetInformationJobObject` requires `JOB_OBJECT_SET_ATTRIBUTES`; `JobObjectCpuRateControlInformation` is class 15. It reports failure via a zero return and `GetLastError`. Microsoft lists CPU rate control as unsupported on Windows 7 and older in this API's version notes. [Microsoft SetInformationJobObject](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-setinformationjobobject).
4. Linux cgroup v2 `cpu.weight` is a **1–10000** relative-share control (default 100); `cpu.max` is a time-bandwidth `MAX PERIOD` quota; `cpu.max.burst` permits configured burst bandwidth. They are not expressed in Windows processor cycles. Linux explicitly scopes these controls to fair-class scheduling and certain sched-ext implementations with relevant callbacks. `cpu.uclamp.min` is a utilization clamp, **not a guaranteed CPU-time reservation**. [Linux cgroup v2 CPU interface](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html#cpu).
5. Linux cgroup v2 weights divide resources among **active sibling cgroups**, and parent restrictions propagate hierarchically. `cpu.stat` throttle fields describe the local bandwidth limit and may exclude throttling inherited from ancestors; `cpu.stat.local` has a different scope. Neither statistic is automatically a Windows job CPU accounting result. [Linux cgroup v2 resource distribution and CPU interface](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html).

## Architectural deductions (not validated)

- **Weight mode:** map the guest's 1–9 policy into a documented monotonic Linux weight policy only as a *candidate*. A linear rescale cannot establish behavioral equivalence: sibling contention, host workloads outside the delegated subtree, scheduling classes and nested distribution matter. Preserve the original guest weight in the NT job authority.
- **Hard cap:** `cpu.max` is a plausible Linux enforcement primitive, but a time quota/period is not proof of equivalence to a Windows cycle budget/scheduling interval. CPU frequency, topology, available cores, burst configuration and period-induced latency require measurement. Do not describe it as cycle-accurate.
- **Minimum/maximum:** a `cpu.max` cap plus `cpu.weight` or `cpu.uclamp.min` does **not** prove the Windows minimum reservation contract. The minimum is an unresolved enforcement gate, especially with non-cooperating host tasks.
- **Hierarchy:** guest job ancestry and Linux delegated cgroup ancestry may differ under breakaway, process creation and host management. Preserve NT policy in a guest-side authority; translate only controls with demonstrated containment and no host-escape path.
- **Scheduler escape:** if a guest can choose a Linux scheduling class that bypasses `cpu.max` or `cpu.weight`, the job CPU limit is not a security boundary. Prevent such escalation or explicitly mark the mode unsupported. Host permissions and cgroup delegation are prerequisites.

## Proposed differential gates — NOT RUN

| Gate | Windows reference to record | Linux backend to test |
| --- | --- | --- |
| Validation | Every flag combination; missing ENABLE; zero/out-of-range CpuRate; weight 0, 1, 9, 10; min > max; GetLastError | Validate in guest policy before touching cgroup; verify no partial host state |
| Weight contention | Two equal/different-weight jobs, CPU-bound threads, idle sibling, external workload | Sweep weight mapping with identical affinity and measured runnable time |
| Hard cap | 20% cap, one vs many threads, single vs multiple cores, interval burst and latency | Sweep `cpu.max` periods, burst=0 and permitted burst; compare throttling and wall-time |
| Min/max | One reserved job, several reservations totaling under/over 100%, host contention | Test whether any Linux policy actually meets minimum reservation; reject unsupported promises |
| Nested controls | Parent cap plus child cap, parent weight plus child weight, ancestor updates | Compare effective quotas, sibling contention and migration restrictions |
| Scheduling class | Windows priorities/processor groups with job CPU controls | Fair, RT, deadline and sched-ext configurations where available; detect bypass |
| Lifecycle | Set/query/update controls while processes run; job closure and child creation | Atomicity/rollback of cgroup updates, child placement and delegation loss |

Record exact Windows build, processor topology, power/frequency policy, Linux kernel/configuration, cgroup delegation and scheduler, workload source, affinity, repeated raw observations, failures and confidence intervals. Use a disposable delegated subtree; **SKIP** unavailable capabilities rather than inventing success. The numeric Windows-to-Linux mapping is intentionally undecided.

## Decision gate

**Do not claim Windows job CPU-rate conformance by setting `cpu.max` or `cpu.weight` alone.** Implement guest flag validation and query fidelity separately from host enforcement. Treat minimum reservation and non-fair scheduler enforcement as blocking questions for strict compatibility, and benchmark any proposed mapping against Windows on equivalent hardware.

Sources inspected 2026-11-17; no source code copied, no experiments performed.
