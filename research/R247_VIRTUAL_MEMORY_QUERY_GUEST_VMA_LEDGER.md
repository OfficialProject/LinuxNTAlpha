# R247 — VirtualQuery / NtQueryVirtualMemory versus Linux VMA inspection

**Research date:** 2026-11-22. **Method:** primary API/manual documentation review; no code execution, Windows differential test, or Linux runtime probe. **Status:** design hypothesis, not an architecture decision.

## Question

Can Linux `/proc/<pid>/maps` directly implement guest `VirtualQuery[Ex]` and `NtQueryVirtualMemory(MemoryBasicInformation)` faithfully? **Finding: no direct equivalence is established.** A guest allocation ledger is a candidate, but must be validated against Windows and the actual Linux mapping lifecycle.

## Verified contracts

1. **Windows query aggregation.** `VirtualQuery` rounds the queried address down to a page boundary and returns a contiguous region beginning there, stopping when state, allocation origin, or protection attributes change. It reports the remaining portion of a free region when queried inside it. The Windows documentation explicitly says a COW page that has become private may still be reported as `MEM_MAPPED` or `MEM_IMAGE`, not `MEM_PRIVATE`. [W1]
2. **Windows identity is richer than page permissions.** `MEMORY_BASIC_INFORMATION` includes `BaseAddress`, `AllocationBase`, `AllocationProtect`, `RegionSize`, `State`, `Protect`, and `Type`; explicit 32/64-bit representations are documented. `MEM_COMMIT`, `MEM_RESERVE`, `MEM_FREE` and `MEM_IMAGE`, `MEM_MAPPED`, `MEM_PRIVATE` distinguish different dimensions. [W2]
3. **Native query boundary.** Microsoft's documented `NtQueryVirtualMemory` accepts a process handle, address, information class, buffer length and optional return length; it returns `NTSTATUS` and documents access, invalid-address, invalid-class and length errors. The cited driver-facing reference documents `MemoryBasicInformation`; it is **not** evidence that every information class used by every Windows build is supported. [W3]
4. **Linux observation boundary.** `/proc/pid/maps` enumerates *currently mapped* regions, permissions, private/shared flags, file offsets and backing identities. Its read is subject to a ptrace access check. The documented record does not include Windows allocation-base, original allocation protection, Windows reservation/commit provenance, or Windows image-section type. [L1]

## Derived mismatch (source-based inference, not a measurement)

- A `---p` Linux VMA is not sufficient evidence that Windows should report `MEM_RESERVE`, `MEM_COMMIT|PAGE_NOACCESS`, a guard-related transition, or an unmapped `MEM_FREE` hole. Mapping permissions are not the guest allocation state.
- A Linux file-backed private VMA is not automatically `MEM_IMAGE`: that Windows classification depends on the guest section/image mapping, not the Linux file path or COW bit.
- Linux VMA splits/merges cannot be assumed to preserve `AllocationBase` or `AllocationProtect`. Synthesizing those fields from contiguous `/proc/pid/maps` lines would discard allocation history.
- A supervisor reading another Linux process's maps may face host ptrace permissions distinct from the *guest* process-handle rights. Host permission is an additional gate, not an implementation of NT access checks.

## Candidate implementation (HYPOTHESIS)

Maintain a **guest virtual-address allocation ledger** with immutable allocation identity/origin and initial protection, plus per-subrange guest state, current protection, section type and guest image identity. Publish query results from that ledger after validating guest process-handle rights. Coordinate all guest-visible reserve/commit/decommit/protect/map/unmap transitions with Linux `mmap`/`mprotect`/`munmap` and any placeholder/image-section implementation. Treat out-of-band host mapping mutations as a consistency failure or explicitly scoped host-private mappings, rather than silently presenting them as guest-owned allocations.

**Unresolved:** exact Windows region-coalescing rules across placeholders, mapped images, guard pages, partial releases, allocation granularity, and native information-class/version behavior; locking/snapshot semantics during concurrent VM mutation; treatment of runtime-created host mappings visible in a guest address space; host and guest page-size mismatch.

## Falsifiable gates (NOT RUN)

1. **Windows reference:** reserve 3+ pages, commit/protect only middle page, query from every page and from holes; capture full `MEMORY_BASIC_INFORMATION` bytes, `VirtualQuery` return lengths, `GetLastError`, and native `NTSTATUS`/`ReturnLength`.
2. **Allocation history:** map an image and a data section, trigger COW writes, split protections, and compare `AllocationBase`, `AllocationProtect`, `Type`, `State`, `RegionSize` before/after.
3. **Lifecycle/races:** repeated reserve/decommit/recommit/release and concurrent queries, including remap at a reused address; prove no stale allocation identity leaks.
4. **Access control:** cross-process `VirtualQueryEx` and native queries with deliberately varied guest process rights, alongside Linux host ptrace restrictions. Require guest access checks even when the host can read `/proc`.
5. **Linux backend:** compare ledger records with `/proc/self/maps` only as a diagnostic; inject failed `mmap`/`mprotect` operations and assert atomic rollback of guest-visible state. Never infer correctness from matching map line counts.
6. **ABI/version:** validate 64-bit structure size/alignment, short/excess buffer behavior, boundary addresses, and explicitly supported native information classes on a pinned Windows build.

## Architecture consequence

For ADR-003, a user-space NT memory manager remains plausible, but direct `/proc/pid/maps` translation should **not** be treated as a completed design. Correctness and authorization require guest allocation provenance and an explicit host/guest synchronization model. This does not establish performance, security, or compatibility.

## Primary sources (retrieved 2026-11-22)

- [W1] Microsoft, `VirtualQuery`: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery (page last updated 2024-02-06).
- [W2] Microsoft, `MEMORY_BASIC_INFORMATION`: https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-memory_basic_information (page last updated 2021-04-02).
- [W3] Microsoft WDK, `NtQueryVirtualMemory`: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntqueryvirtualmemory (page last updated 2023-06-28).
- [L1] Linux man-pages 6.19, `proc_pid_maps(5)`: https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html (manual dated 2026-02-08).
