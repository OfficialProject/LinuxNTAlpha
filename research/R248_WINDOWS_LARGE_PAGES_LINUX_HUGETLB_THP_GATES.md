# R248 — Windows explicit large-page allocations versus Linux HugeTLB and THP

Research date: 2026-11-22. Status: **documentation-only architectural analysis**; no Windows differential run, Linux allocation probe, performance measurement, or architecture decision.

## Scope and relationship to prior work
This note isolates explicit large-page *guest API semantics* from ordinary VirtualAlloc allocation bookkeeping (R247), write-watch (R219), and dynamic executable-memory policy (R241). It does not claim that Linux huge pages are generally faster or that either Linux mechanism is a drop-in Windows implementation.

## VERIFIED — primary-source contracts

1. Microsoft's [Large-Page Support](https://learn.microsoft.com/en-us/windows/win32/memory/large-page-support) requires the caller to obtain **SeLockMemoryPrivilege**, query **GetLargePageMinimum**, and supply **MEM_LARGE_PAGES** with size/alignment multiples of that minimum. It specifies resident, nonpageable read/write memory, reserve-and-commit in one operation, possible fragmentation-related allocation failure, and an explicit distinction from working-set accounting. The same documentation states large-page allocations are not subject to job limits; this is a documented Windows contract, **not** a reason to bypass Linux cgroup limits or host security.
2. Microsoft's [VirtualAlloc reference](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc) requires **MEM_LARGE_PAGES | MEM_RESERVE | MEM_COMMIT** together. The API reports failure through NULL and GetLastError. The large-page size is discovered, not hard-coded.
3. The Linux kernel's [HugeTLB documentation](https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html) documents explicit huge-page mapping through mmap or SysV shared memory, configurable pools, reservation accounting, available-size differences and non-swappable HugeTLB backing. Availability depends on kernel configuration, privileges, resource pools and host configuration.
4. The Linux kernel's [Transparent Hugepage documentation](https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html) describes THP as automatic/promotable/demotable backing with per-range madvise and system policy controls; it explicitly contrasts THP with reserved HugeTLB pools. Therefore observing a large backing page through THP is **not** evidence that a Windows MEM_LARGE_PAGES request was honored.
5. The source URLs above were reviewed on 2026-11-22. The kernel links are moving **latest** documentation, not pinned source commits. Version-specific claims require revalidation against the chosen kernel tree.

## Architectural mismatch and proposed boundary — HYPOTHESIS

Maintain separate **guest allocation intent** and **host physical backing** records. Guest intent should include the requesting token/privilege decision, requested page minimum, base/size, commit/reserve atomicity, protection, lifetime and guest-visible failure. Host backing may be HugeTLB, THP or ordinary pages, but should not silently relabel a successful ordinary mapping as explicit large-page success.

**Candidate strict implementation:** after a guest privilege check, attempt a host HugeTLB mapping of a compatible size with suitable containment and resource accounting; if strict backing cannot be obtained, return a guest-compatible allocation failure rather than falling back to THP while claiming MEM_LARGE_PAGES success. This is a design hypothesis, **not** a proven requirement for every Windows version. An ordinary allocation without MEM_LARGE_PAGES may independently use THP as an invisible host optimization.

**Security boundary:** Windows SeLockMemoryPrivilege is guest-token authority, whereas Linux HugeTLB allocation and host cgroup/memlock/resource controls are host authority. A guest privilege must not grant host privilege, consume unlimited reserved pages, or defeat host quotas. Host policy can reject a guest allocation even when guest privilege checks succeed; mapping that failure to a Windows error requires measured evidence.

**Accounting boundary:** Windows documentation's working-set and job-limit statements do not imply that /proc RSS, Linux memory cgroups or HugeTLB cgroup accounting match Windows observables. Keep guest memory-query/accounting separate from host telemetry (see R247).

## OPEN — concrete unrun conformance gates

- **W1 Windows privilege:** on a version-pinned Windows VM, compare token lacking/present SeLockMemoryPrivilege, privilege disabled/enabled, and invalid alignment/size. Capture GetLargePageMinimum, VirtualAlloc result, GetLastError and VirtualQuery metadata.
- **W2 Windows lifecycle:** attempt reserve-only, commit-only, combined reserve+commit, partial decommit/protection changes and release; record exact outcomes instead of inferring them from generic VirtualAlloc behavior.
- **L1 Linux capability:** on a pinned kernel, capture supported HugeTLB sizes, pool availability, mount/cgroup constraints, MAP_HUGETLB failure errno, and actual backing; never interpret an allocation SKIP as PASS.
- **L2 THP counterexample:** map ordinary anonymous memory with THP policy varied and compare actual backing against explicit HugeTLB mapping. Verify that THP collapse/demotion cannot alter guest large-page allocation identity.
- **S1 security:** concurrent guests with distinct guest tokens and host quotas; test pool exhaustion, cancellation, teardown, and attempts to exceed cgroup limits. Ensure rollback does not leak a reserved page.
- **P1 performance (only after correctness):** same-hardware repeated runs with base pages, THP and HugeTLB; report TLB misses, allocation latency, fragmentation, resident bytes, power and variance, not just best-case throughput.

## Decision impact

**OPEN:** whether strict HugeTLB availability is acceptable as the initial implementation, and which Windows version/error codes are the target. Do not modify the kernel or claim compatibility until W1/W2/L1/S1 pass. Ordinary THP optimization can be evaluated separately from explicit large-page compatibility.
