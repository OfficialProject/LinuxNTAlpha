# R219 — Windows write-watch allocation semantics versus Linux dirty-page tracking

Status: RESEARCH ONLY — no implementation, Windows reference run, Linux probe, or performance result.
Reviewed: 2026-11-21. Scope: native x86-64 guest memory tracking; not an architecture decision.

## Verified primary-source contracts

1. **Windows allocation opt-in.** Microsoft documents `VirtualAlloc(MEM_WRITE_WATCH)` as an allocation-time request that **must** accompany `MEM_RESERVE`; tracking remains enabled until the region is freed. `GetWriteWatch` requires an address inside a write-watch allocation and reports written page addresses since allocation or reset. It takes an address-array capacity in/out and returns page granularity separately. Its successful return is zero, unlike the NTSTATUS convention. [M1, M2]
2. **Reset has a semantic boundary.** `GetWriteWatch(WRITE_WATCH_FLAG_RESET)` retrieves and resets tracking in one API call. Microsoft explicitly warns that a separate `GetWriteWatch` followed by `ResetWriteWatch` can lose intervening writes unless the caller prevents concurrent writes. This warning does **not** by itself prove every detail of atomicity or buffer-shortage behavior. [M1, M3]
3. **Linux soft-dirty is not allocation-scoped write-watch.** The documented soft-dirty procedure writes `4` to `/proc/PID/clear_refs`, then reads bit 55 of pagemap entries. That reset is at **task address-space** scope rather than a guest `VirtualAlloc` reservation. The kernel documentation says newly mapped/expanded regions are marked soft-dirty, even without a guest write, to flag mapping replacement. [L1]
4. **Linux page-table tracking has loss and privilege considerations.** Kernel pagemap documentation identifies bit 55 as soft-dirty, bit 57 as userfaultfd write-protect tracking, and warns that PTE-only soft-dirty state for shared memory can be lost when entries are zapped/swapped. PFN disclosure is restricted, although that restriction does not itself prohibit reading the dirty bit; permission to access a target pagemap must be tested separately. [L2]
5. **Linux offers a different candidate: userfaultfd write-protect.** `UFFDIO_REGISTER_MODE_WP` plus `UFFDIO_WRITEPROTECT` can capture first writes and unprotect pages; behavior for absent anonymous PTEs differs from shmem/hugetlbfs, with `UFFD_FEATURE_WP_UNPOPULATED` offered on newer kernels. Kernel documentation also describes asynchronous WP tracking and `PAGEMAP_SCAN`, including a scan-and-write-protect operation, `PAGE_IS_WRITTEN`, partial scans and `walk_end`. Availability must be negotiated on the actual kernel. [L2, L3]

## Architectural inference (HYPOTHESIS, not verified behavior)

Keep guest reservation identity, committed subranges, generation, guest page size and dirty epochs in a **guest-owned virtual-memory ledger**. Do not expose raw `clear_refs` or pagemap state as the Windows contract. A candidate Linux backend is userfaultfd async WP plus pagemap scanning; another is synchronous WP with an event-handling thread. Compare both with a software fault-based fallback, but only after capability negotiation and tests. Soft-dirty alone is unlikely to satisfy isolated per-allocation resets and unmap/remap semantics without supplementary bookkeeping.

An **important unresolved question** is whether `PAGEMAP_SCAN` scan-and-reprotect and guest ledger epoch publication can implement the expected `GetWriteWatch(RESET)` observation/reset boundary under concurrent writes, particularly when the caller's address array is too small. Do not claim equivalent atomicity merely because the Linux scan operation is described as atomic.

## Conformance gates to implement later (PROPOSED; none executed)

- Windows reference: reserve/commit with and without `MEM_WRITE_WATCH`; write first/last/adjacent pages; query repeatedly with and without reset; record exact page order, granularity, capacity handling, return codes and output mutation on failure.
- Windows reference: partial decommit/recommit, release/reallocate at same virtual address, protection changes, guard faults, copy-on-write, and thread races around both reset forms. Distinguish documented behavior from observed OS-build-specific behavior.
- Linux isolated test: capability probes for userfaultfd creation, WP registration, WP-unpopulated, WP-async and pagemap scan; test anonymous missing pages, THP, swapping, remapping, concurrent writes and scan buffer exhaustion. Record kernel version, page size, privileges, feature masks and results.
- Security: verify no cross-process write-watch state leaks; reset of one guest allocation must not clear another; stale events from a released generation must never dirty a reused allocation.
- Performance: compare first-write fault latency, steady-state writes, reset cost and scan throughput with matching untracked baseline and repeated distributions; no performance outcome is currently known.

## Source ledger (primary; accessed 2026-11-21)

- [M1] Microsoft, GetWriteWatch: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-getwritewatch
- [M2] Microsoft, VirtualAlloc: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
- [M3] Microsoft, ResetWriteWatch: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-resetwritewatch
- [L1] Linux kernel, Soft-Dirty PTEs: https://docs.kernel.org/admin-guide/mm/soft-dirty.html
- [L2] Linux kernel, Examining Process Page Tables: https://docs.kernel.org/admin-guide/mm/pagemap.html
- [L3] Linux kernel, Userfaultfd: https://docs.kernel.org/admin-guide/mm/userfaultfd.html

No upstream source revision was pinned for these rolling documentation pages. Before implementation, pin Linux kernel source/tag and Windows build and revalidate the exact UAPI behavior.
