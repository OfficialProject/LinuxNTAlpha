# R218 — Copy-on-write page identity versus VirtualQuery region classification

Date: 2026-11-21. Status: **verified documentation only**; no Windows reference, Linux experiment, or performance measurement executed.

## Narrow unresolved question

How can an NT-on-Linux runtime report Windows-compatible memory-region metadata after a private write to a mapped view, when Linux backing and resident-page state may change independently of the guest-visible section classification?

This is distinct from R59's image/data section mapping and commit-charge requirements, R71/R139's dirty tracking, and R202's durability questions. It addresses the **query contract after copy-on-write**.

## VERIFIED FROM PRIMARY DOCUMENTATION

1. Microsoft states that `VirtualQuery` groups consecutive pages by state, original allocation, and protection, reporting `MEM_PRIVATE`, `MEM_MAPPED`, or `MEM_IMAGE` as the region type. Importantly, a modified copy-on-write page **continues to be reported as MEM_MAPPED or MEM_IMAGE**, not MEM_PRIVATE. To determine whether an individual page has been privatized, Microsoft instructs callers to make it resident and use `QueryWorkingSetEx`'s Shared bit. [M1]
2. Microsoft documents `FILE_MAP_COPY` as producing private pages on writes; modifications do not flow back to the original file. It also documents a commit charge for the entire copy-on-write view. [M2]
3. Microsoft documents that ordinary file-mapping views can be shared between processes, and that the section object and view are distinct concepts. [M3]
4. Linux `mmap(MAP_PRIVATE)` is a private copy-on-write mapping whose changes are not carried through to the underlying file; the effect of later external modifications to the file is unspecified. This is a backing mechanism, **not** a Windows memory-query API implementation. [L1]

## HYPOTHESIS — required guest metadata model

Keep at least two distinct dimensions per live guest view:

- **Guest allocation provenance:** allocation base, original section identity, image-versus-data mapping, original view length, and Windows-facing protection/state transitions. This drives `VirtualQuery`'s region type and allocation grouping even after COW.
- **Physical-sharing observation:** whether a particular resident page is currently shared. This may require a separate backend query and must not be inferred from `MEM_MAPPED` or `MEM_IMAGE` alone.

A naïve implementation that reports `MEM_PRIVATE` whenever Linux has instantiated an anonymous COW page would contradict [M1]. Conversely, simply reporting `MEM_IMAGE` for a PE mapping does **not** establish correct `QueryWorkingSetEx` Shared-bit behavior. Avoid inventing a Linux-to-Windows Shared-bit translation until reference and backend tests exist.

The Linux host's `/proc/*/smaps` or pagemap observations, where available, are potential diagnostics, not automatically a stable or permission-independent guest ABI. Whether they can implement accurate, race-resistant `QueryWorkingSetEx` behavior is **OPEN**.

## Proposed falsification matrix — NOT RUN

1. On a pinned Windows build, create file-backed read-only, writable and copy-on-write views, plus a genuine image view; record `VirtualQuery` before and after writing one permitted COW page. Check allocation base, type, protection and region boundaries.
2. Make queried pages resident, then record `QueryWorkingSetEx` Shared for untouched versus written pages. Run the same sequence with two processes mapping the same section, and explicitly distinguish resident status from shared status.
3. Change protection with `VirtualProtect` where legal; check whether `VirtualQuery` splits regions by protection while retaining original mapping provenance.
4. On Linux, compare `MAP_PRIVATE` views and observable `/proc/self/smaps` accounting before/after writes. Record kernel version, filesystem, page size, process permissions and any unavailable counters as SKIP.
5. Prototype a **metadata-only** guest query layer without exposing Linux virtual-memory types as Windows types; test stale view generations, partial unmaps, remaps at recycled addresses and concurrent writes.

## Decision gate

Do not claim `VirtualQuery` or `QueryWorkingSetEx` conformance until Windows-reference outputs and Linux backend behavior are compared for the same cases. The metadata model is a design hypothesis, not an accepted kernel/userspace boundary decision (ADR-003 remains OPEN).

## Primary sources inspected 2026-11-21

- [M1] Microsoft, VirtualQuery: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery
- [M2] Microsoft, MapViewOfFile: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile
- [M3] Microsoft, File Mapping: https://learn.microsoft.com/en-us/windows/win32/memory/file-mapping
- [L1] Linux mmap(2) manual: https://man7.org/linux/man-pages/man2/mmap.2.html

No Linux source revision or Windows build was inspected for this note; therefore none is claimed.
