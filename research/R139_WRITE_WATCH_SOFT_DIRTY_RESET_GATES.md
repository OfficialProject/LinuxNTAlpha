# R139 — Windows write-watch versus Linux soft-dirty: reset scope and race gates

Date: 2026-11-17. Status: primary-source documentation review only; **no experiment**. This is an independent semantic gap, not evidence that either implementation is faster.

## Verified contracts (documented, not experimentally reproduced)

1. Microsoft's `GetWriteWatch` returns addresses of written pages in a `VirtualAlloc` region allocated with `MEM_WRITE_WATCH`; the caller supplies an address-array capacity and receives the count and page granularity. A zero flag reads tracking without resetting; `WRITE_WATCH_FLAG_RESET` resets as part of the query. A standalone `ResetWriteWatch` resets the specified region. Microsoft warns that writes between a separate `GetWriteWatch` and `ResetWriteWatch` can be missed. Sources: [GetWriteWatch](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-getwritewatch), [ResetWriteWatch](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-resetwritewatch).
2. Linux soft-dirty is observed via bit 55 of `/proc/PID/pagemap`; writing `4` to `/proc/PID/clear_refs` clears the task's soft-dirty tracking. The kernel write-protects PTEs to detect subsequent writes. Newly mapped or expanded regions are marked soft-dirty to signal mapping renewal. Source: [Linux kernel soft-dirty documentation](https://docs.kernel.org/7.0/admin-guide/mm/soft-dirty.html), with [procfs clear_refs documentation](https://kernel.org/doc/html/v5.7/filesystems/proc.html).

## Architectural mismatch and implications

**Verified source-level mismatch:** Windows exposes allocation/region-specific queries and resets; the documented Linux `clear_refs=4` operation resets **the process's tracked pages**, not only one selected Windows allocation. Therefore a direct one-call translation would erase unrelated guest write-watch history. Soft-dirty is a useful *candidate observation mechanism*, not a drop-in NT implementation.

**Hypothesis (not validated):** a broker could maintain per-allocation dirty snapshots or epochs and use soft-dirty as a coarse signal, but independent resets of overlapping activity, concurrent writers, remapping and multiple watched allocations require a precise ownership and generation model. A process-wide clear followed by cached snapshots might still lose writes during reset/read transitions; no race-free design has been established.

**Open:** whether Linux `userfaultfd` write-protect notifications or a dedicated guest-owned page-protection scheme can provide better per-region reset semantics and acceptable concurrency without interfering with guest PAGE_GUARD, COW, signal handling and host page permissions. No implementation choice or performance result follows from these documents.

## Falsifiable conformance gates (proposed, NOT executed)

- Windows reference: allocate two separate `MEM_WRITE_WATCH` regions, write both, reset only A, then query B and verify its pre-reset write remains reported. Repeat with `GetWriteWatch(WRITE_WATCH_FLAG_RESET)`.
- Query capacity and granularity: test a buffer too small for all dirty pages, then retry; record actual return code, count, address ordering and whether reset occurred. **Do not assume** behavior not established by the cited API contract.
- Race: run writers against separate query/reset and combined query+reset; record missed and repeated pages with synchronized epochs. Microsoft's documented warning applies specifically to the separate-call interval.
- Mapping lifecycle: commit/decommit/recommit subranges and replace Linux mappings at identical virtual addresses; distinguish write evidence from soft-dirty's new-mapping marker.
- Isolation: query a non-write-watch allocation and verify Windows error behavior; on Linux ensure a guest cannot clear another guest's dirty history or read unrelated pagemap metadata.
- Backend cost: only after semantic conformance, measure fault counts, page scans, lock contention and latency across allocation counts and write rates, with kernel version, permissions and page size recorded.

## Decision impact

Do **not** equate `MEM_WRITE_WATCH` with Linux soft-dirty or globally clear soft-dirty bits on a guest region reset. Keep this capability behind an explicit NT virtual-memory tracking abstraction. Before choosing a backend, verify Windows-reference semantics and run concurrent multi-allocation Linux tests. This review does not modify experiments or assert compatibility.
