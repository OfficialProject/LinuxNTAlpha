# R113 — KUSER_SHARED_DATA fixed-address page versus Linux vDSO

Research date: 2026-11-16 UTC. Status: **primary-source contract review; no experiment executed**.

## Why this is a separate gate

R24/R25 demonstrate synthetic GS-based thread data in a Windows-target PE. That does **not** establish compatibility with Windows code that directly reads the process-global shared user-data page at a constant address. A function-call shim alone cannot intercept ordinary machine-code loads from that address.

## VERIFIED from primary sources

1. Microsoft's Security Response Center describes the x64 Windows **user-mode** `KUSER_SHARED_DATA` mapping at **0x7FFE0000**, read-only to user code. Its 2022 article discusses changes to the **kernel-mode** writable mapping; do not mistake that hardening for a relocation of the user-mode ABI. [MSRC, 2022](https://www.microsoft.com/en-us/msrc/blog/2022/04/randomizing-the-kuser_shared_data-structure-on-windows/).
2. Microsoft's debugger `!kuser` documentation explicitly displays the shared page at `7ffe0000` and examples of tick-count, timezone and other fields. [Microsoft Learn, !kuser](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/-kuser).
3. Microsoft's `NtQuerySystemTime` contract specifies **100-ns units since 1601-01-01 UTC**, an output pointer, and `NTSTATUS` success/error behavior. This API contract is not equivalent to a direct-memory-read ABI. [Microsoft Learn, NtQuerySystemTime](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntquerysystemtime).
4. Linux's timekeeping header explicitly distinguishes monotonic, realtime, boottime and raw clock domains; `CLOCK_MONOTONIC` excludes suspend, while boottime includes it. [Linux source, include/linux/timekeeping.h](https://github.com/torvalds/linux/blob/master/include/linux/timekeeping.h). Linux vDSO exposes clock-related entry points, but that does **not** expose Windows' fixed-address data structure or its layout. [Linux source, include/vdso/gettime.h](https://github.com/torvalds/linux/blob/master/include/vdso/gettime.h).

## HYPOTHESIS — minimal native PE runtime design

Reserve the guest-visible page at `0x7FFE0000` **before** mapping guest images and other guest-controlled regions. Map it read-only to guest code. A privileged-in-process broker could update the backing storage through a *separate* writable mapping of the same memory object, or use a controlled write-protection transition. Neither choice is proven safe here. Avoid making the guest-visible mapping writable merely for convenience.

Keep a per-guest-process page representation with a deliberately versioned field-layout contract; do not assume every field is a direct conversion of one Linux clock. Compute Windows epoch offsets with checked arithmetic, distinguish monotonic/boottime/realtime and guest time-zone state, and define coherent multiword reads under concurrent updates. Fixed-address reservation must fail closed if the region is already occupied; `MAP_FIXED` replacement of unknown mappings is not an acceptable default.

The exact field offsets, synchronization protocol, supported Windows build profiles, page size, guest ability to change process mappings, and resistance to malicious guest direct syscalls remain **OPEN**. A single read-only mapping does not constitute a security boundary against arbitrary code sharing the same Linux address space.

## Proposed reproducible gates — NOT RUN

**Linux fixture gate:** Build a freestanding Windows-target PE32+ image with no imports that reads a known test word at an explicitly selected address within the fixed page; verify a deterministic value, a second read after broker update, read-only protection, and failure on preoccupied address. Keep this test isolated from the existing trusted-fixture execution mode. Probe `MAP_FIXED_NOREPLACE` availability and behavior on the actual target kernel rather than presuming success.

**Coherence gate:** Have a writer update simulated time fields and several guest reader threads sample them under stress; detect torn/inconsistent samples. Test across optimization levels and with a deliberately interrupted writer. A passing synthetic test does not validate Windows' actual layout or update protocol.

**Windows-reference gate:** On an identified Windows x64 build, use documented `!kuser` debugging plus `NtQuerySystemTime`, `GetSystemTimeAsFileTime`, `QueryInterruptTime` and `QueryUnbiasedInterruptTime` to compare epochs, suspend behavior, update rates and documented read semantics. Direct field-offset assertions must be explicitly pinned to the target WDK header/build, not inferred from a third-party table.

**Adversarial gate:** Attempt guest mapping replacement and concurrent reads during updates. Evaluate host-vs-guest address collisions, process forks and process startup order. This is a threat-model exercise, not a claim that untrusted Windows binaries are sandboxed.

## Decision implication

Add **fixed-address guest shared-data mapping** to the PE loader/process-bootstrap dependency graph, separate from GS/TIB, ntdll API wrappers and vDSO fast paths. Do not select a Linux clock-to-Windows-field mapping or performance optimization until Windows-reference behavior and field-layout versioning are established.

## Evidence limitations

Sources above establish the existence/address/permissions of the Windows page and relevant documented time interfaces, plus Linux clock distinctions. They do **not** prove an implementation technique, exact per-build offsets, or performance. No Windows host or Linux mapping experiment was executed for R113.