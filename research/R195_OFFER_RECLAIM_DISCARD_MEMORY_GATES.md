# R195 — Offered/discarded virtual memory versus Linux page advice

Date: 2026-11-19. Status: **primary-documentation research only**. No Windows reference test, Linux experiment, performance benchmark or implementation is claimed.

## Distinct architectural question

Can Linux page-reclaim hints implement the *observable lifecycle* of Windows `OfferVirtualMemory`, `ReclaimVirtualMemory`, `DiscardVirtualMemory` and `PrefetchVirtualMemory` without losing protection, content-validity or error semantics? R177 addresses working-set limits and reclaim policy; this note instead addresses per-range **offer/reclaim state and validity reporting**.

## VERIFIED — inspected primary interface contracts

1. Microsoft documents `OfferVirtualMemory` as marking pages **inaccessible** and removing them from the working set; offered content may be discarded. Input start and size are page aligned, and an offer priority is supplied. Offering locked pages unlocks them. It returns a Win32 error code directly, not a BOOL. [M1]
2. Microsoft documents `ReclaimVirtualMemory` as restoring access to offered pages. It returns `ERROR_SUCCESS` if contents survived and `ERROR_BUSY` if reclaimed pages were discarded, in which case the application must regenerate content. These two successful reclaim outcomes are semantically distinct. [M2]
3. Microsoft documents `DiscardVirtualMemory` as discarding content **without decommitting** the allocation; subsequent content is undefined. The operation fails unless the protection is `PAGE_READWRITE`; failure leaves content unchanged. [M3]
4. Microsoft documents `PrefetchVirtualMemory` as a performance hint over an array of address ranges, potentially partially ineffective under memory pressure. Prefetched pages are not thereby added to the process working set. Its `Flags` parameter must be zero. [M4]
5. Linux man-pages 6.19 documents `MADV_FREE` as lazily discarding **private anonymous** pages; reads and writes remain permitted, writes cancel pending free, and subsequent access to freed pages yields zero-fill-on-demand. `MADV_DONTNEED` also leaves accesses permitted but changes future page contents according to backing type. `MADV_COLD` is nondestructive; `MADV_PAGEOUT` reclaims or swaps; `MADV_WILLNEED` is an advisory prefetch. These are **not** identical to Windows offer/reclaim state transitions. [L1]
6. The same Linux documentation describes `MADV_POPULATE_READ` as faulting readable mappings in and reporting some errors instead of delivering SIGBUS, with possible partial population on failure. It does not promise Windows `PrefetchVirtualMemory` residency/working-set behavior. [L1]

## HYPOTHESIS — guest memory manager design (not Windows internals)

Maintain a per-guest-allocation interval state machine with `committed / offered / reclaimed / discarded` metadata, original guest page protections, offer priority and an explicit **content validity** status. Keep state changes synchronized with `VirtualProtect`, decommit, unmap and cross-thread memory access. Linux `madvise` can be a backend optimization, but its success is **not** a sufficient guest API success oracle.

- **Offer:** validate range, allocation class, protection and priority using a Windows-reference-tested policy; transition guest access to inaccessible while retaining the previous guest protection. A possible Linux implementation combines `mprotect(PROT_NONE)` with a carefully ordered reclaim hint, but the ordering and race safety must be tested.
- **Reclaim:** restore guest protection only after determining whether original contents are intact. Linux `MADV_FREE` provides no direct per-page authoritative `ERROR_SUCCESS` versus `ERROR_BUSY` report. Without an additional validity mechanism, claiming intact content would be unsound. A conservative strategy that always reports discarded may itself be Windows-incompatible; validate before selecting it.
- **Discard:** preserve committed reservation and guest protection while allowing content invalidation; do not automatically use `MADV_DONTNEED` for shared/file-backed mappings because its backing-file repopulation semantics differ. Windows documents undefined post-discard bytes, not a guarantee of zeros.
- **Prefetch:** treat Linux `MADV_WILLNEED` or alternative readahead as a candidate best-effort backend, not proof of matching Win32 success/error or cross-process rights semantics.

**Security:** A guest inaccessible state cannot rely only on a userspace table if guest code can access the mapped address directly. A host `PROT_NONE` mapping can enforce access denial, but its fault path must be reconciled with guest exception translation and concurrent state transitions. Neither `madvise` nor guest API checks sandbox untrusted PE execution.

## OPEN — differential and falsification matrix (NOT RUN)

| Scenario | Required Windows observations | Linux candidate question |
|---|---|---|
| Offer then read/write before reclaim | exception code, protection query, process outcome | does PROT_NONE ingress produce correct guest exception? |
| Offer then immediate reclaim without pressure | exact return code and original bytes | can intact state be proven rather than guessed? |
| Offer then memory pressure then reclaim | ERROR_SUCCESS vs ERROR_BUSY, content handling | is there a trustworthy discard notification? |
| Offer priority levels | accepted values, observable prioritization if measurable | is a priority mapping possible or only advisory? |
| Locked, shared, image, guard, read-only and decommitted ranges | return codes, state and side effects | backend eligibility and rollback |
| Concurrent VirtualProtect/decommit/reclaim | state ordering and error codes | prevent stale protections and use-after-unmap |
| Discard PAGE_READWRITE vs PAGE_READONLY | exact return and unchanged bytes on failure | do not mutate before validation |
| Prefetch mapped-file and anonymous ranges | success/error, page faults and residency | compare MADV_WILLNEED/POPULATE_READ without conflating semantics |

Reference fixtures should record Windows build, allocation flags/protections, page size, exact errors, VirtualQuery snapshots and contents only when defined. Linux tests must record kernel version/configuration, mapping type, page-fault counts and reclaim conditions. Run each potentially faulting scenario in an isolated child. Memory-pressure experiments must be bounded and opt-in; do not induce global host OOM. Report PASS/FAIL/SKIP individually.

## Architectural decision

**OPEN.** Do not map `OfferVirtualMemory` directly to `MADV_FREE` or infer that `ReclaimVirtualMemory` can accurately return `ERROR_SUCCESS` merely because `madvise` succeeded. Prioritize an isolated Windows differential test for intact/discarded reporting before committing to a Linux backend. No performance or compatibility result has been established.

## Primary sources inspected 2026-11-19

- [M1] Microsoft, OfferVirtualMemory: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-offervirtualmemory
- [M2] Microsoft, ReclaimVirtualMemory: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-reclaimvirtualmemory
- [M3] Microsoft, DiscardVirtualMemory: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-discardvirtualmemory
- [M4] Microsoft, PrefetchVirtualMemory: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-prefetchvirtualmemory
- [L1] Linux man-pages 6.19, madvise(2), dated 2026-02-08: https://man7.org/linux/man-pages/man2/madvise.2.html

No third-party code copied. No experiments performed.
