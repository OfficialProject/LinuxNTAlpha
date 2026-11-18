# R170 — Windows explicit large-page allocation versus Linux HugeTLB and THP gates

Research date: 2026-11-18. Status: **primary-documentation review only; no experiment executed**. Scope: x86-64 guest Win32 memory contracts on a Linux host. This is distinct from R53 placeholders, R71/R139 dirty tracking, and R142 NUMA placement.

## VERIFIED — guest contracts

1. Windows `VirtualAlloc` and `VirtualAlloc2` accept `MEM_LARGE_PAGES` only with `MEM_RESERVE | MEM_COMMIT`; requested size and alignment must be multiples of `GetLargePageMinimum()`. These are explicit allocation semantics, not a general performance hint. [Microsoft VirtualAlloc](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc); [Microsoft VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2).
2. Microsoft's large-page procedure requires enabling `SeLockMemoryPrivilege` and querying `GetLargePageMinimum` before allocation. A guest token's privilege state must not silently become a Linux host capability. [Microsoft large-page support](https://learn.microsoft.com/en-us/windows/win32/memory/large-page-support).
3. `VirtualAlloc2` separately documents `MEM_64K_PAGES` as a **hint** that may fall back to smaller pages, except for its documented nonpaged combination; it must not be conflated with the stronger `MEM_LARGE_PAGES` request. [Microsoft VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2).

## VERIFIED — Linux host contracts

4. Linux `mmap(MAP_HUGETLB)` requests explicit huge-page-backed mappings; `MAP_HUGE_2MB` and `MAP_HUGE_1GB` select supported sizes. Host availability and policy are not implied by the flag. [Linux mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html).
5. Linux transparent huge pages (THP) are opportunistic and may be split; THP policy, fault behavior and collapse are configurable. Therefore `madvise(MADV_HUGEPAGE)` is not evidence that an explicit Windows large-page allocation succeeded. [Linux kernel THP documentation](https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html).

## HYPOTHESIS — proposed subsystem boundary

Maintain guest-visible virtual allocation records independently of Linux VMAs. For explicit large-page allocations, first validate guest token privilege, supported guest page size, size/alignment, protection and process rights; then attempt an explicit huge-page backend and report failure if the requested contract cannot be met. Never silently downgrade an explicit large-page request to THP or ordinary 4 KiB mappings. A separate advisory path may consider THP for ordinary allocations, but must not claim guaranteed large-page backing.

**Security gate:** a guest's `SeLockMemoryPrivilege` authorization and Linux HugeTLB quota/permission are separate checks. Host privilege escalation, global hugepage-pool modification, or resource-exhaustion bypass is not an acceptable fallback. Isolation should account for multiple guest processes and containers.

**OPEN:** precise Windows-version and hardware behavior for `GetLargePageMinimum` and `MEM_64K_PAGES`; Windows large-page protection, decommit/release, query and process-creation interactions; HugeTLB reservation versus actual fault-time allocation; cgroup hugetlb quotas and RLIMIT_MEMLOCK on target hosts. No Windows-reference or Linux-host measurements have been run.

## Proposed falsifiable gates (NOT EXECUTED)

- **Windows reference:** query minimum page size; test unprivileged and privileged explicit allocations, wrong alignment, insufficient memory, `VirtualQuery` state, `VirtualFree` operations and error codes. Record OS build, CPU and token privileges.
- **Linux host:** enumerate supported HugeTLB sizes, pool and per-cgroup quotas; attempt aligned `MAP_HUGETLB` mapping with and without appropriate host permissions, verify page backing via kernel interfaces, and test failure under pool exhaustion. Record kernel version and resource limits.
- **Subsystem differential:** require explicit large-page requests either satisfy verified backing and guest semantics or fail predictably; test that THP hints do not count as explicit success; verify guest privileges cannot alter host-wide hugepage policy.
- **Performance:** only after correctness, compare page-fault cost, TLB-sensitive workloads, allocation latency and fragmentation with controlled repeats. No performance advantage is asserted.

## Architectural consequence

This gate favors an explicit guest VM/privilege policy layer over a one-to-one `VirtualAlloc`→`mmap` translation. It does **not** justify a kernel fork or establish feasibility of all NT memory semantics.
