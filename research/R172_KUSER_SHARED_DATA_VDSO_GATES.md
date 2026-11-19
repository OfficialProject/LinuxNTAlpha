# R172 — KUSER_SHARED_DATA fixed-address guest mapping versus Linux vDSO

Research date: 2026-11-18. Status: documentation-only; **no Windows-reference or Linux runtime experiment performed**.

## Question

Can a native x86-64 PE process safely read Windows-style shared system data at a fixed guest virtual address while Linux retains its own independently located vDSO/VVAR and timekeeping interfaces? This is a **separate loader and memory-layout requirement**, not solved by the R19–R28 PE fixtures, R24 synthetic GS/TIB, or R171 placeholder reservation study.

## VERIFIED — primary-source contracts

1. Microsoft documents `KUSER_SHARED_DATA` in `ntddk.h`, including `InterruptTime`, `SystemTime`, `TimeZoneBias`, `TickCount`, `QpcFrequency`, version fields, processor features, QPC calibration fields, and other system metadata. Its fields are not all stable or interchangeable with an API result; Microsoft explicitly directs consumers of some fields to helper APIs. **Source:** [Microsoft KUSER_SHARED_DATA](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-kuser_shared_data), retrieved 2026-11-18.
2. Microsoft's debugger API exposes a target shared-user-data address through `DEBUG_DATA_SharedUserData`. This supports treating the mapping as observable guest address-space state rather than merely a private ntdll implementation detail. **Source:** [Microsoft IDebugDataSpaces3::ReadDebuggerData](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dbgeng/nf-dbgeng-idebugdataspaces3-readdebuggerdata), retrieved 2026-11-18.
3. Microsoft distinguishes precise interrupt time from ordinary tick-based interrupt time, and unbiased time excludes sleep/hibernation. These are distinct semantic clocks; substituting one Linux clock for all Windows clock fields would be unjustified. **Sources:** [QueryInterruptTimePrecise](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryinterrupttimeprecise), [QueryUnbiasedInterruptTimePrecise](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryunbiasedinterrupttimeprecise), retrieved 2026-11-18.
4. Linux provides vDSO time functions with architecture- and kernel-dependent implementation; the vDSO address is obtained from the auxiliary vector rather than assumed fixed. Upstream generic vDSO code uses clock data, read/retry synchronization and syscall fallback paths. This is **not** a Windows-format `KUSER_SHARED_DATA` page. **Sources:** [Linux vDSO manual](https://man7.org/linux/man-pages/man7/vdso.7.html), [Linux generic vDSO implementation](https://github.com/torvalds/linux/blob/master/lib/vdso/gettimeofday.c), [Linux vDSO interface header](https://github.com/torvalds/linux/blob/master/include/vdso/gettime.h), retrieved 2026-11-18. Source links are to moving upstream branches; pin a Linux tag/commit before implementation.

## EVIDENCE LIMITS / OPEN

- The familiar user address `0x7FFE0000` is reported by independent Windows layout research, but **the Microsoft sources above do not themselves establish its universal applicability to every Windows build and process type**. Treat that address as a Windows-reference test target, not an unconditional promise. Secondary cross-check: [Geoff Chappell's KUSER_SHARED_DATA analysis](https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/ntexapi_x/kuser_shared_data/index.htm). Do not conflate this with the randomized writable kernel alias.
- The exact layout, field offsets, and update protocol are Windows-build-sensitive. A header snapshot alone is not evidence that a specific guest build's assembly fast path will work.
- No evidence here shows that Linux vDSO time bases exactly match Windows wall-clock, interrupt-time, unbiased, QPC or sleep semantics.
- No mapping collision test, atomic-read test, clock comparison, sleep/resume test, or security review has been executed.

## HYPOTHESIS — candidate design

Reserve the verified guest fixed-address region **before** mapping untrusted PE image sections; fail closed on collision. Provide a **read-only guest-format page** maintained by a trusted runtime producer. Keep Linux vDSO/VVAR mappings independent and query host clocks through documented interfaces; implement guest-specific epoch, frequency, bias, and consistency protocols separately. Do not map writable runtime bookkeeping or a raw host vDSO at the guest address. A versioned guest-build profile should control exposed fields and offsets.

This is a design candidate, not an architecture decision or a demonstrated implementation.

## Proposed acceptance gates (NOT RUN)

1. On an authorized Windows x64 reference, record OS build, address mapping, page permissions, field offsets and sampled values using supported debugger interfaces; independently test direct reads where legally and technically appropriate.
2. Construct a freestanding Windows-target PE fixture that reads only a deliberately selected, independently verified shared-data field. Verify the exact machine-code access and reject unexpected addresses/offsets.
3. On Linux, test `MAP_FIXED_NOREPLACE` reservation and collision behavior **before** loader placement; confirm that no unrelated mapping is overwritten. Integrate with the R171 interval ledger.
4. Publish a read-only page with a deliberate sequence/update protocol; stress concurrent readers for torn/mixed values and test guest write faults. Include CPU migration and high-rate reads.
5. Compare wall-clock, tick, interrupt, unbiased, QPC, timezone and sleep/resume behavior separately against Windows reference measurements. Do not infer equivalence from monotonicity alone.
6. Check interactions with ASLR, PIE host runtime, `/proc/self/maps`, loader rollback, process fork/exec assumptions and the R26 syscall trap path.
7. Benchmark guest fast reads, API calls and host clock/vDSO fallback under identical test conditions, reporting distribution and overhead rather than claiming improvement.

## Decision implication

Treat shared-user-data placement and clock semantics as **first-class native execution gates**. The R19–R28 successes do not establish this compatibility. Avoid a Linux kernel fork for this feature until a user-space mapping and clock-translation prototype has failed a specific measured requirement.
