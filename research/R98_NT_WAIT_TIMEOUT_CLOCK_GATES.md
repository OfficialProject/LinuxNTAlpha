# R98 — NT wait timeout clocks versus Linux deadlines

Research date: 2026-11-20. **Documentation review only; no Windows or Linux experiments executed.** Scope: the clock and deadline contract shared by NT dispatcher waits, not the object-state atomicity already analyzed in R75, APC delivery in R88, or waitable timer object semantics in R33.

## Verified primary-source contracts

1. Microsoft's [ZwWaitForSingleObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-zwwaitforsingleobject) documentation specifies an optional 100-nanosecond timeout: negative values denote relative intervals; absolute deadlines track system-time changes; relative intervals do not. A zero value tests immediately; a null pointer waits indefinitely. The documented results include STATUS_SUCCESS, STATUS_TIMEOUT, STATUS_ALERTED, STATUS_USER_APC, STATUS_INVALID_HANDLE and STATUS_ACCESS_DENIED. Crucially, STATUS_TIMEOUT is classified as success by NT_SUCCESS, so a generic NT_SUCCESS-only check loses the distinction.
2. Microsoft's [KeWaitForSingleObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kewaitforsingleobject) independently documents the 1601 epoch for positive absolute values and the same relative-versus-absolute clock-change behavior. This is kernel API documentation, not evidence that a proposed Linux-hosted implementation is correct.
3. Microsoft's [NtQuerySystemTime](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntquerysystemtime) defines NT system time as 100-nanosecond intervals since 1601-01-01 UTC.
4. Linux [clock_nanosleep(2)](https://man7.org/linux/man-pages/man2/clock_nanosleep.2.html) documents CLOCK_REALTIME absolute deadlines that follow changes to the real-time clock and recommends a fixed absolute deadline when restarting interrupted sleeps to avoid cumulative drift. The function returns EINTR on interruption and is not automatically restarted even with SA_RESTART. Linux [nanosleep(2)](https://man7.org/linux/man-pages/man2/nanosleep.2.html) describes Linux's CLOCK_MONOTONIC-based relative sleep.
5. The Win32 [NtWaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntwaitforsingleobject) page identifies the native call as deprecated for application use and specifies success, timeout, alert and APC outcomes. The kernel-mode Zw documentation above is the more explicit source for time conversion semantics.

## Architecture hypothesis, not a result

A guest wait engine could classify timeout inputs *before* entering a backend: null -> infinite, zero -> immediate probe, negative -> monotonic interval budget, positive -> guest NT UTC absolute deadline. The absolute deadline requires an explicit 1601-to-Unix epoch conversion and a real-time clock; relative waits should use a monotonic budget that survives spurious wakes and interrupted host waits. This is a **candidate design**, not an assertion of Windows equivalence. Linux host signals, APC injection, dispatcher state transitions, and cancellation require a separate guest-status state machine; EINTR must never be blindly exposed as an NTSTATUS.

The epoch difference is **11644473600 seconds** between 1601-01-01 UTC and 1970-01-01 UTC. Conversion of signed 64-bit 100-nanosecond values needs checked arithmetic, especially around extreme negative values and times outside host time_t range. Converting a negative relative interval into a positive magnitude by negation can overflow at INT64_MIN; use checked or wider arithmetic.

## Explicit unknowns / falsifiable tests

- **OPEN:** exact ordering when an object becomes signaled at the same instant the timeout expires; test single and multiple waits against Windows reference, recording distributions rather than assuming a universal ordering.
- **OPEN:** clock adjustment behavior for native user-mode waits under abrupt forward/backward UTC changes, sleep/resume, and virtualization; the documented contract is not an executed observation.
- **OPEN:** effects of alertable APC delivery and STATUS_ALERTED on remaining relative timeout budget, including repeated interruptions. Cross-reference R13, R75 and R88.
- **OPEN:** whether the selected host futex/ntsync backend can atomically combine guest object state and both clock classes; do not assume Linux timeout API equivalence merely because units can be converted.
- **PROPOSED WINDOWS REFERENCE MATRIX:** null/zero/negative/positive timeout; signaled and nonsignaled event; APC pending versus not pending; past and future absolute deadlines; interrupted wait and retry; deadline boundaries and extreme signed inputs. Record exact NTSTATUS, elapsed monotonic and realtime timestamps, Windows build, hardware and privilege.
- **PROPOSED LINUX ISOLATED TESTS:** checked epoch conversion; fixed monotonic deadline after synthetic EINTR; forward/backward realtime changes only in a privileged disposable time namespace or controlled VM (never change the user's host clock); measure oversleep and status mapping separately. **Not executed.**

## Integration gate

Implement a reusable timeout-normalization and status model only after Windows differential results exist. Keep guest-visible 100-nanosecond values and NTSTATUS separate from host timespec/errno. This closes a *research gap*, not a compatibility milestone; no source code was imported.

Sources checked 2026-11-20: Microsoft Learn URLs above and Linux man-pages project URLs above. Version-specific behavior and source-level implementation remain unverified.
