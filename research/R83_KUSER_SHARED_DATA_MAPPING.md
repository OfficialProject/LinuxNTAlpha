# R83 — KUSER_SHARED_DATA fixed-address guest mapping and time coherence

Research date: 2026-11-14. Status: **primary-documentation review only; no Windows/Linux runtime experiment executed**.

## Why this is a separate loader gate

R19–R28 demonstrate narrow PE64 execution and a synthetic NTDLL import, but do not establish a Windows process address-space contract. Some Windows-target code can read the shared user data page directly without importing an API or issuing a syscall. A PE loader that resolves imports and supplies a TEB but omits this fixed-address mapping can therefore fail even for code that never calls the runtime.

## VERIFIED FROM PRIMARY DOCUMENTATION

1. Microsoft's WinDbg `!kuser` documentation identifies `KUSER_SHARED_DATA` as the shared user-mode page and shows the structure at **0x7ffe0000** in its debugger example. This is direct evidence for the address in that documented example, not a blanket guarantee for every future Windows build or process type. [M1]
2. Microsoft's WDK-derived `KUSER_SHARED_DATA` declaration includes `TickCountMultiplier`, `InterruptTime`, `SystemTime`, `TimeZoneBias`, OS-version fields, and other data. This is a structured, version-sensitive ABI surface, not an arbitrary block of timestamps. [M2]
3. Microsoft's `NtQuerySystemTime` documentation specifies a 64-bit count of **100-nanosecond intervals since 1601-01-01 UTC**. A guest implementation that exposes time via both a shared page and the NTDLL API must define their consistency relationship rather than independently synthesizing incompatible epochs. [M3]
4. Linux `mmap(2)` documents that `MAP_FIXED_NOREPLACE` refuses an overlapping mapping with `EEXIST`, whereas `MAP_FIXED` replaces existing mappings. A fixed-address guest-page reservation should not overwrite unrelated host mappings; on older kernels, verify the returned address because unsupported flags can be ignored. [L1]
5. Linux `vdso(7)` documents a kernel-provided userspace interface that can accelerate some system calls. It does **not** establish that the Linux vDSO occupies the Windows shared-page address, exposes Windows field layouts, or provides Windows epoch/clock semantics. [L2]

## HYPOTHESIS — implementation boundary, not demonstrated

Reserve the Windows guest address range **before** mapping guest PE images and stacks, with collision detection; create a guest-visible read-only view and a separate host-controlled update mechanism. The host must not allow arbitrary PE writes to the page. Use a version-pinned Windows layout and explicitly document which fields are populated, emulated, zeroed, or unsupported. The mapping and its writer must be scoped to the guest process's observable state; host process identity and Linux vDSO are not substitutes.

For time fields, derive guest `SystemTime` from a specified UTC source with the 1601 epoch conversion, and choose/validate appropriate clock sources for interrupt time and tick count. **Do not assume** `CLOCK_MONOTONIC`, `CLOCK_BOOTTIME`, and Windows interrupt-time APIs agree on suspend/resume behavior. Avoid torn multiword updates; the exact required `KSYSTEM_TIME` read/write protocol and ordering must be checked against version-pinned Windows headers and Windows-reference behavior.

## OPEN questions and falsifiable gates (not executed)

- **Address ownership:** On a Linux x86-64 test process, inspect `/proc/self/maps` and attempt `MAP_FIXED_NOREPLACE` at 0x7ffe0000. Verify the exact returned address, read-only protection, collision rejection, and cleanup. Never use destructive `MAP_FIXED` as the initial probe. Record kernel, libc and page size.
- **Guest fixture:** Compile a freestanding Windows-target PE64 fixture that reads only a documented, stable field at the fixed address; demonstrate failure without the mapping and correct value with it. Keep execution limited to trusted fixtures.
- **Layout/version:** Compare Windows SDK/WDK layout and field offsets for each targeted OS build; confirm x64 `KSYSTEM_TIME` reader protocol and any process exceptions to the shared mapping. Do not derive offsets solely from a third-party header.
- **Time:** On a controlled Windows reference, sample `NtQuerySystemTime`, `GetSystemTimeAsFileTime`, `GetTickCount64`, `QueryInterruptTime`, and documented shared-page fields across normal progression, wall-clock adjustment, and suspend/resume. Record build and uncertainty; distinguish monotonic from wall-clock semantics.
- **Concurrency:** Stress multiple readers during writer updates and prove no impossible torn values. Explicitly test page permissions and guest attempts to modify the mapping.
- **Security:** This page is *not* a sandbox boundary. It does not prevent raw Linux syscalls, direct host-memory access, or guest writes to other mapped pages. R26's single intercepted syscall does not prove containment.

## Decision gate

Do not claim that a PE64 process has a functional Windows execution environment merely because its IAT, GS/TIB and syscall stub work. Add the shared-page contract to the startup/address-space checklist and require measured Windows-reference conformance before declaring time semantics compatible.

## Sources (primary)

[M1] Microsoft, WinDbg `!kuser` documentation: https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/-kuser

[M2] Microsoft WDK documentation, `KUSER_SHARED_DATA`: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-kuser_shared_data

[M3] Microsoft, `NtQuerySystemTime`: https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntquerysystemtime

[L1] Linux man-pages, `mmap(2)`: https://man7.org/linux/man-pages/man2/mmap.2.html

[L2] Linux man-pages, `vdso(7)`: https://man7.org/linux/man-pages/man7/vdso.7.html

No experiment was run and no Windows build-specific layout was independently measured in this note.
