# R176 — QPC frequency, monotonicity and Linux clock-domain translation gates

Research date: 2026-11-18. Status: **primary-documentation synthesis only; no Windows or Linux experiments executed**.

## Scope and nonduplication

R172 addresses the guest fixed-address KUSER_SHARED_DATA mapping and broad clock publication; R33 covers waitable timers. This note isolates **QueryPerformanceCounter (QPC) / QueryPerformanceFrequency (QPF) as an observable pair**, including per-process consistency, host clock selection, and conversion arithmetic. It does not assert that a Linux clock is a drop-in Windows clock.

## VERIFIED — documented contracts

1. Microsoft describes QPC as a high-resolution timestamp source usable for interval measurement; QPF reports ticks per second and the frequency is fixed at system boot and consistent across processors. Microsoft advises retrieving the frequency once rather than on every call. Sources: [QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter), [QueryPerformanceFrequency](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency).
2. Microsoft documents hardware/platform-dependent QPC implementations, historical cross-processor and virtualization concerns, and recommends QPC for short intervals rather than treating the counter as UTC. Its guidance explicitly distinguishes precise UTC timestamps from performance-counter values. Source: [Acquiring high-resolution time stamps](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).
3. Microsoft documents that QPC counts cannot be converted into wall-clock time without a separate correlation, and that the returned counter is not a Unix epoch timestamp. Source: [Acquiring high-resolution time stamps](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).
4. Linux clock_gettime distinguishes CLOCK_MONOTONIC, CLOCK_MONOTONIC_RAW and CLOCK_BOOTTIME: monotonic time is subject to incremental NTP/adjtime adjustments; RAW is not; BOOTTIME additionally includes suspend time. These are different observable semantics. Source: [Linux clock_gettime(2)](https://man7.org/linux/man-pages/man2/clock_gettime.2.html).
5. Linux vDSO may accelerate clock_gettime on supported architectures/clocks, but an application cannot infer Windows QPC frequency or guest clock semantics merely from the availability of a host vDSO. Source: [Linux vDSO(7)](https://man7.org/linux/man-pages/man7/vdso.7.html).

All URLs above are primary Microsoft documentation or Linux API manual pages; consulted as documented contracts, **not independently validated against a specific Windows build or kernel release**. Version-specific behavior remains OPEN.

## HYPOTHESIS — guest clock abstraction

Represent a guest QPC domain as a versioned tuple `(epoch_host, epoch_guest, guest_frequency, host_clock_id, conversion_policy)`. Convert elapsed host time into guest ticks with checked wide-integer arithmetic and a specified rounding rule. Publish a stable guest frequency and ensure every guest thread and the R172 shared-data fast path observe the **same domain**. Use one clock-source policy for the whole guest process, not per-thread ad hoc CPU cycle reads.

A monotonic host clock is a candidate source, not proof of exact QPC compatibility. `CLOCK_MONOTONIC_RAW` avoids host clock slewing but may diverge from Windows' actual QPC behavior; `CLOCK_MONOTONIC` slews and may be better for some reference configurations. No choice is accepted until Windows-reference evidence and host drift tests exist.

## OPEN questions and failure modes

- Suspend/resume behavior, live migration, guest clock scaling, CPU hotplug, host time namespaces and long-running drift require measurements against identified Windows and Linux versions.
- Guest fast-path reads from R172 and imported QPC/QPF API calls must not disagree on frequency, epoch, or monotonic order.
- Counter-to-nanosecond conversion can overflow 64-bit intermediate products even when both inputs individually fit; naïve floating-point conversion can lose precision over long uptime.
- Whether applications directly use RDTSC/RDTSCP, or access shared-data QPC calibration fields, is a **separate** machine-code compatibility question. Intercepting API calls alone cannot make raw instruction readings obey a synthetic QPC domain.
- Cross-process and cross-guest-instance counter correlation is not established by the API descriptions above.

## Proposed acceptance gates — NOT RUN

1. On at least two recorded Windows x64 builds, sample QPC/QPF concurrently across pinned and migrating threads; record CPU topology, virtualization, boot uptime and sleep/resume. Check monotonicity, stable frequency and conversion residuals; distinguish documented guarantees from measured outcomes.
2. Build a Linux harness comparing MONOTONIC, RAW and BOOTTIME over idle/load, time adjustment, suspend/resume and CPU migration. Record kernel version, clocksource and hardware, with distributions rather than a single average.
3. Implement a **pure arithmetic** reference model of the guest clock tuple; fuzz large elapsed times, zero/invalid frequency, rollover boundaries and rounding, including 128-bit intermediate calculations.
4. Construct a Windows-target PE fixture importing QPC and QPF, and separately a fixture reading only independently validated shared-data fields. Compare the two paths with a common guest clock producer.
5. Check direct RDTSC/RDTSCP behavior separately; document any mismatch rather than claiming transparent native compatibility.
6. Stress concurrent clock-domain publication and guest teardown, including failed initialization and mapping collisions. Ensure a guest cannot modify trusted producer state.
7. Benchmark only after correctness gates pass; report both call overhead and timekeeping accuracy, without assuming that avoiding a syscall implies a superior clock.

## Architecture implication

A user-space clock translator is the initial candidate. **No evidence here justifies modifying the Linux kernel**, claiming QPC equivalence, or claiming faster application timing. This work adds a narrow acceptance gate to R172 and should be tested before using clock speed or latency measurements to justify an NT-on-Linux architecture.
