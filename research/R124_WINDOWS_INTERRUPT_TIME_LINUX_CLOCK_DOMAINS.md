# R124 — Windows interrupt-time domains versus Linux clocks (2026-11-16)

**Evidence:** documentation review only; **no experiment executed**. This is a time-source semantic gate, not a claim about timer scheduling, the fixed KUSER_SHARED_DATA mapping (R113), or waitable timer implementation (R33/R104).

## VERIFIED: primary documented contracts

1. Microsoft distinguishes **interrupt time** (including time in sleep/hibernation) from **unbiased interrupt time** (excluding sleep/hibernation). The precise variants return 100-nanosecond units and can differ in acquisition cost from tick-based variants. See Microsoft's [Interrupt Time overview](https://learn.microsoft.com/en-us/windows/win32/sysinfo/interrupt-time), [QueryInterruptTime](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryinterrupttime), [QueryInterruptTimePrecise](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryinterrupttimeprecise), and [QueryUnbiasedInterruptTimePrecise](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryunbiasedinterrupttimeprecise).
2. Linux **CLOCK_BOOTTIME** includes suspend time, while **CLOCK_MONOTONIC** excludes it. CLOCK_MONOTONIC_RAW also excludes suspend but is not subject to the frequency adjustments affecting CLOCK_MONOTONIC. CLOCK_REALTIME can jump when wall-clock time is changed. See Linux [clock_gettime(2)](https://man7.org/linux/man-pages/man2/clock_gettime.2.html).
3. Linux time namespaces virtualize CLOCK_MONOTONIC and CLOCK_BOOTTIME; thus host clock readings and guest-visible uptime are not necessarily interchangeable across namespaces. See [time_namespaces(7)](https://man7.org/linux/man-pages/man7/time_namespaces.7.html).

## HYPOTHESIS: minimal backend mapping, not proven equivalent

- Use CLOCK_BOOTTIME as a candidate source for Windows *precise interrupt time* and CLOCK_MONOTONIC for *precise unbiased interrupt time*, subject to measured epoch/boot offset, suspend, clock adjustment and namespace behavior. This is **not** an equivalence assertion: Windows documented values, Windows version differences, VM migration, and guest process lifetime may require a separate offset/translation layer.
- Keep coarse/tick-based APIs distinct from precise APIs. Truncating a precise reading to an arbitrary fixed tick is not justified by the documentation; the tick interval and update cadence require Windows-reference observation.
- Keep elapsed-time counters distinct from wall-clock FILETIME and QueryPerformanceCounter. No single Linux clock can be presumed to implement every Windows time API.

## OPEN: falsifiable conformance gates

1. On a **specified Windows build** and equivalent Linux host, sample QueryInterruptTime, QueryInterruptTimePrecise, QueryUnbiasedInterruptTime, QueryUnbiasedInterruptTimePrecise, GetTickCount64 and QueryPerformanceCounter around idle periods and controlled suspend/resume. Record units, monotonicity, precision, tick steps and offsets; do not assume exact equality.
2. Repeat with NTP frequency correction and (only in isolated, authorized test systems) a wall-clock step. Verify monotonic counters remain stable while wall time changes.
3. Exercise concurrent readers and high-frequency calls; measure overhead distributions separately from semantic correctness. Avoid calling this a performance improvement without a matched Windows baseline.
4. In a Linux time namespace, compare CLOCK_MONOTONIC/CLOCK_BOOTTIME from inside and outside. Define whether the guest exposes host boot identity or namespace-relative boot identity before selecting a mapping.
5. Check overflow-safe conversion from nanoseconds to 100-nanosecond ticks, resolution/rounding, and synchronization with any guest-visible KUSER_SHARED_DATA values (R113). Define coherence tolerances using Windows-reference observations.
6. Confirm availability and version gates for precise APIs on target Windows versions; do not silently substitute a precise call for a coarse contract.

## Architectural consequence (provisional)

Specify a **clock-domain interface** (wall time; suspend-inclusive elapsed time; suspend-exclusive elapsed time; high-resolution performance counter; coarse tick counter) before binding individual NT/Win32 exports. Clock reads may be implemented in user space initially; adding a kernel extension has no justification until conformance and performance measurements show a concrete deficiency. R104 addresses timer resolution and wake scheduling; this note addresses **reported time values**.

**Source check:** Microsoft Learn and Linux man-pages pages retrieved 2026-11-16. No upstream source revision pinned for these documentation pages; version-specific behavior remains open. No prototype, benchmark, or Windows reference run was performed.
