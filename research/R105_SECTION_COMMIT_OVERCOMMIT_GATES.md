# R105 — Pagefile-backed section reservation and commitment versus Linux overcommit

Date: 2026-11-15. Status: **VERIFIED FROM PRIMARY DOCUMENTATION; NO EXPERIMENT**.

## Scope

This note isolates **pagefile-backed section commitment accounting** from R11 (general sections), R53 (placeholder replacement), R59 (image section copy-on-write), R65 (stack guards), R70 (flush durability), and R94 (view inheritance). It does not claim a complete implementation of NT section objects or Windows memory accounting.

## Verified contracts

1. Microsoft [CreateFileMappingW](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-createfilemappingw) specifies that `SEC_COMMIT`, for a pagefile-backed mapping (`hFile == INVALID_HANDLE_VALUE`), commits the entire mapping and requires sufficient committable pages **at object creation**; otherwise creation fails. If neither attribute is specified, `SEC_COMMIT` is assumed. `SEC_COMMIT` and `SEC_RESERVE` are mutually exclusive. These attributes have no effect for ordinary data-file or executable-image-backed mappings.
2. The same Microsoft API specifies that `SEC_RESERVE` reserves the pagefile-backed range for later commitment with `VirtualAlloc`. Once such pages are committed, `VirtualFree` cannot free or decommit those pages. This is a section-specific lifetime rule, not the generic private `VirtualAlloc` reserve/commit model.
3. Linux [mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html) documents `MAP_NORESERVE` as disabling swap reservation for a mapping; writes can fault if memory is unavailable. Linux `mmap` does not expose a Windows section object's `SEC_COMMIT` / `SEC_RESERVE` accounting or its `VirtualAlloc` / `VirtualFree` rules.
4. Linux [proc_sys_vm(5)](https://man7.org/linux/man-pages/man5/proc_sys_vm.5.html) documents `overcommit_memory` policies, including strict accounting mode 2 and `overcommit_ratio` / `overcommit_kbytes`. Linux host configuration affects whether an allocation succeeds; it is not equivalent to guest NT commit accounting.

## Architecture implications — HYPOTHESES

Represent pagefile-backed section **maximum size, commit policy, committed subranges, shared backing identity and view references** in a guest section broker. Validate commitment against an explicit guest policy and a host resource budget; never infer NT commitment solely from `mmap` success or `MAP_NORESERVE`. The broker must coordinate cross-process views of one section and must not allow a guest's reported commitment to bypass host cgroup or address-space constraints. Host anonymous shared backing (e.g., memfd) is a candidate, **not** a verified drop-in equivalent.

Do not assume Linux `mprotect` is a full implementation of NT commitment: it changes protection, not the complete guest commitment ledger or cross-process section semantics. Also do not assume that Linux strict overcommit mode can be enabled safely or is necessary for correctness; changing host-wide sysctls is outside this research.

## Falsification gates — NOT EXECUTED

1. Windows reference: create pagefile-backed mappings with `SEC_COMMIT`, `SEC_RESERVE`, neither flag, and invalid combined flags. Record OS build, size, protections, error codes, and `GetProcessMemoryInfo` / system commit observations without assuming accounting precision.
2. Map multiple views in one and two processes; commit a subrange in a `SEC_RESERVE` view and query the same offsets in other views with `VirtualQuery`. Test overlapping commits, repeated commits, bounds, protection changes, and attempts to `VirtualFree(MEM_DECOMMIT)`. Record actual Windows outcomes rather than extrapolating from API wording.
3. Linux isolated process: compare anonymous shared `mmap`, memfd-backed shared mapping, `MAP_NORESERVE`, and `mprotect` under recorded `overcommit_memory`, cgroup memory limits and `RLIMIT_AS`. Observe allocation and write faults in a disposable child process; do not change global kernel configuration.
4. Differential model: verify that section identity, committed-range transitions, close/unmap lifetime, allocation failures and cross-process visibility match the Windows reference. Include negative controls for ordinary file-backed sections and private virtual allocations.
5. Security: reject unbounded reservation/commit requests and ensure failure paths release all broker accounting; do not overpromise host allocation guarantees.

## Decision gate

Before implementing a general section API, require a guest-visible commitment ledger and explicit tests for `SEC_RESERVE` transition/lifetime semantics. Linux `mmap` success is insufficient evidence of NT commit compatibility. No experiment or performance measurement was performed.

Primary documentation consulted 2026-11-15:
- https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-createfilemappingw
- https://man7.org/linux/man-pages/man2/mmap.2.html
- https://man7.org/linux/man-pages/man5/proc_sys_vm.5.html
