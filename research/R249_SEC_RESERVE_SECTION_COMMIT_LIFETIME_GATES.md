# R249 — SEC_RESERVE pagefile-section commitment versus Linux mapping lifetime

**Research date:** 2026-11-22. **Method:** primary Microsoft API and Linux man-page review. **Evidence:** documented API contracts only; no Windows reference execution, Linux experiment, benchmark, or NT implementation. **ADR-003:** remains OPEN.

## Architectural question

Can a Linux anonymous/shared mapping plus `mprotect` and `MAP_NORESERVE` directly represent a Windows pagefile-backed section created with `SEC_RESERVE`, including subsequent `VirtualAlloc(MEM_COMMIT)` operations and the lifetime of committed pages?

**Answer (source-based inference):** not as a direct flag-for-flag translation. Windows distinguishes *section creation policy*, *per-view address placement*, and *subsequent commitment*. Linux mapping flags describe different host mechanisms and do not themselves establish Windows section-object identity, guest commitment state, or guest allocation accounting.

## VERIFIED — primary documented contracts

1. **Section creation, not ordinary private reserve.** For `CreateFileMapping(INVALID_HANDLE_VALUE, ..., SEC_RESERVE, ...)`, Windows documents a paging-file-backed mapping whose mapped view is initially reserved. Pages can subsequently be committed with `VirtualAlloc`. The same documentation explicitly states that after commitment those pages **cannot be freed or decommitted using `VirtualFree`**. `SEC_RESERVE` and `SEC_COMMIT` are mutually exclusive. These attributes have no effect for ordinary file-backed or image-backed mappings. [W1]
2. **Different initial charge.** The `SEC_COMMIT` case commits the entire paging-file-backed mapping and requires sufficient committable pages at creation, whereas `SEC_RESERVE` defers commitment. `SEC_COMMIT` is the default when neither attribute is specified. [W1]
3. **Private reservation is not the same contract.** `VirtualAlloc(MEM_RESERVE)` reserves a private address range, and `VirtualAlloc(MEM_COMMIT)` commits pages in an already reserved range. The documented private allocation may later be decommitted via `VirtualFree(MEM_DECOMMIT)`. The `SEC_RESERVE` mapped-section exception in [W1] therefore cannot be erased by a generic reserve/commit ledger. [W2, W3]
4. **View access is separately checked.** `MapViewOfFile` requires a mapping-object handle and compatible view access, and establishes a view in the calling process. The object handle and view are distinct lifetime concepts. [W4]
5. **Linux's `MAP_NORESERVE` is not Windows `SEC_RESERVE`.** Linux `mmap(2)` documents `MAP_NORESERVE` as declining swap reservation, with possible failure on later writes when memory is unavailable. `MAP_SHARED` and `MAP_PRIVATE` control visibility/COW; `munmap` removes a mapping. None of these documented Linux flags defines the Windows irreversible-via-`VirtualFree` mapped-section commitment rule. [L1]

## Derived design constraints — INFERENCE, not tested

- **Separate identities:** maintain a guest *section object* (size, creation attributes, backing and commitment policy), each guest *view* (process, base, offset, length, protection), and section-offset commitment metadata. Do not identify section commitment solely by Linux VMA permissions.
- **Shared-view question remains OPEN:** the cited Win32 pages establish section-backed reserve/commit and view mapping but do **not** by themselves prove exact propagation of a newly committed section page to already mapped views in other processes, nor the precise commit-charge owner/release point. A Windows-reference experiment and, where needed, a more precise native section contract are required before fixing the internal model.
- **Host accounting:** Linux overcommit, swap and cgroup limits may reject or kill work at different stages than guest Windows commitment. A runtime must distinguish guest policy failures from host exhaustion; it cannot promise Windows commit guarantees merely because `mmap` returned successfully.
- **Guest authority:** a process must not commit an arbitrary host mapping by supplying an address inside a runtime-owned VMA. Check the guest view's section identity, extent, protection, caller rights and section policy before touching the host mapping.
- **Address ownership:** fixed-address host replacement can clobber unrelated mappings; coordinate with R171 and R247's guest address-space ledger. An `mprotect` success does not prove the section accounting transaction succeeded.

## Proposed falsifiable conformance gates — NOT RUN

1. **Windows reference matrix:** on a pinned Windows build, compare pagefile-backed `SEC_RESERVE`, `SEC_COMMIT`, and private `MEM_RESERVE` with the same sizes. Capture mapping creation results, `VirtualQuery` fields, `VirtualAlloc(MEM_COMMIT)` results, and `VirtualFree(MEM_DECOMMIT)` results/errors.
2. **Cross-view semantics:** map overlapping offsets of one `SEC_RESERVE` section in two processes; commit a subrange through one view, query and access corresponding offsets through the other, then unmap one view and repeat. Capture faults/errors safely under a test harness. Do not assume propagation in advance.
3. **Section/handle lifetime:** close section handles while views remain, unmap views in different orders, and test whether/when subsequent commits and new views are permitted. Record guest object and page commitment lifetime separately.
4. **Alignment and boundaries:** test page granularity, allocation-granularity offsets, partial views, overlapping commits, already committed pages, zero lengths, integer overflow, and commit beyond view/section extent.
5. **Failure and rollback:** induce low commit limits in an isolated Windows test environment and host Linux resource limits separately. Ensure failed guest operations do not partially publish ledger changes; do not infer identical failure stages.
6. **Security/races:** concurrently unmap/reuse guest virtual addresses while committing through another thread. Require generation-safe view lookup, no stale host-VMA mutation and correct guest-rights checks.

## Decision impact

This is a **specific gap** beyond R11's generic reserve/commit analysis, R171's placeholder replacement, R247's query provenance, and R248's explicit large-page handling. A candidate user-space memory manager must represent **section-level commitment policy and view ownership**, not only per-process page states. No kernel change is justified by this documentation alone. Architecture, cross-process propagation, exact charge accounting and performance remain unverified.

## Primary sources (consulted 2026-11-22)

- [W1] Microsoft, `CreateFileMappingA`, `SEC_RESERVE` / `SEC_COMMIT` descriptions: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga
- [W2] Microsoft, `VirtualAlloc`, `MEM_RESERVE` / `MEM_COMMIT`: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
- [W3] Microsoft, `VirtualFree`, `MEM_DECOMMIT` / `MEM_RELEASE`: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree
- [W4] Microsoft, `MapViewOfFile`, view access and mapping-object handle: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile
- [L1] Linux man-pages, `mmap(2)`, `MAP_NORESERVE`, `MAP_SHARED`, `munmap`: https://man7.org/linux/man-pages/man2/mmap.2.html
