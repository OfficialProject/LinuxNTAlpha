# R71 — Windows write-watch allocations and Linux dirty-page tracking

Status: **VERIFIED FROM PRIMARY DOCUMENTATION** for cited API behavior; **HYPOTHESIS** for Linux implementation; **OPEN** for Windows-reference conformance. Reviewed 2026-11-14. **No experiment was run.**

## Distinct architectural question

R11/R53/R59 cover mapping and virtual-address ownership; R65 covers stack guards. This note addresses the separate observable contract of `MEM_WRITE_WATCH` allocations, `GetWriteWatch`, and `ResetWriteWatch`. A page-fault handler that merely tracks write access is not automatically a correct Windows write-watch implementation.

## Verified contracts

1. Microsoft specifies that `GetWriteWatch` retrieves addresses of pages written within a `VirtualAlloc` region allocated with `MEM_WRITE_WATCH`. The caller supplies an address-array capacity in `lpdwCount`; the API returns the number of addresses produced and the tracking granularity (page size). `dwFlags=0` does not reset tracking; `WRITE_WATCH_FLAG_RESET` resets it. Success returns zero, failure nonzero. [Microsoft GetWriteWatch](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-getwritewatch).
2. `ResetWriteWatch` resets the tracking state for a specified write-watch allocation range. Microsoft explicitly warns that separate `GetWriteWatch` then `ResetWriteWatch` calls can lose observations if another thread writes in between; the combined reset flag avoids that particular two-call interval. The documentation does **not** establish all atomicity guarantees under concurrent writers. [Microsoft ResetWriteWatch](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-resetwritewatch).
3. Linux `userfaultfd` supports write-protect registration (`UFFDIO_REGISTER_MODE_WP`), fault notification and subsequent unprotection. A faulting thread is blocked until the handler resolves protection. Feature negotiation through `UFFDIO_API` is required. The Linux manual identifies anonymous-memory WP support from Linux 5.7 and shmem/hugetlbfs WP support from Linux 5.19, subject to feature negotiation. It also warns that WP-only registration does not notify for writes to missing pages. [Linux userfaultfd(2)](https://man7.org/linux/man-pages/man2/userfaultfd.2.html).

## Architectural hypotheses — not validated

- A LinuxNTAlpha allocation record may need to own the original Windows allocation extent, committed subranges, tracking page granularity, dirty-page bitmap and reset epoch independently of host VMA layout. `GetWriteWatch` should never scan arbitrary host addresses based solely on a guest pointer.
- A candidate Linux backend could write-protect committed pages using `userfaultfd`, mark the faulted page dirty, and release its write protection until the next reset. This is **not** yet equivalent to Windows: missing-page writes, unmap/recommit, concurrent reset, host feature availability and other writers require explicit treatment.
- A userspace `SIGSEGV` plus `mprotect` implementation is another research candidate, but interaction with R41/R65 exception delivery, async-signal-safety and multiple threads makes it risky. No performance comparison exists.
- Write-watch is a **compatibility feature**, not an isolation guarantee. Guest code that can modify mapping permissions through unmediated Linux syscalls can undermine a userspace-only tracker; see R69.

## Falsifiable tests (proposed, NOT RUN)

1. On a Windows reference host, allocate a multi-page `MEM_WRITE_WATCH` region, write exactly pages 0 and 2, query without reset twice, then query with `WRITE_WATCH_FLAG_RESET`; record address ordering, count, granularity, return values and post-reset results.
2. Repeat with array capacity 0 and 1, invalid flags, out-of-range address, uncommitted pages and a region not created with `MEM_WRITE_WATCH`; do not assume error values or partial-output behavior.
3. Compare `GetWriteWatch(...RESET)` with separate query/reset while a second thread writes repeatedly; record actual observed races without asserting a linearization point from documentation alone.
4. Exercise decommit/recommit and protection changes within the allocation, and record whether tracking state is retained or cleared.
5. On a controlled Linux host, first negotiate `userfaultfd` WP features; distinguish **SKIP** for unsupported/denied facilities from **FAIL** for an actual semantic mismatch. Test populated and missing pages separately, plus reset under concurrent writes.
6. Only after correctness gates, measure per-first-write fault cost and reset cost against a Windows reference and a Linux baseline, reporting kernel/build, page size, workload and distributions.

## Open decisions

The exact Windows behavior for capacity exhaustion, ordering, page-size variation, decommit/recommit and simultaneous reset/write remains unverified. Linux `userfaultfd` availability and privilege policy must be checked on each host. No backend, performance improvement or Windows application compatibility is claimed.
