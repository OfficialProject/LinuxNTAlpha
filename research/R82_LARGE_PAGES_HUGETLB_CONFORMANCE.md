# R82 — Windows large-page allocations versus Linux HugeTLB and THP

Evidence status: **documentation-verified contracts; no experiment executed**. Reviewed 2026-11-14. Scope: guest-requested explicit large pages, not general NT VM (R11), placeholders (R53), write-watch (R71), or address constraints (R72).

## Verified primary-source findings

1. **Windows explicit large pages are an API contract, not merely a performance hint.** Microsoft's *Large-Page Support* specifies enabling `SeLockMemoryPrivilege`, querying `GetLargePageMinimum()`, and requesting `MEM_LARGE_PAGES`; size and alignment must be multiples of the reported minimum. `GetLargePageMinimum()` returns zero if unsupported, and the minimum varies by system (typically at least 2 MiB). [W1][W2]
2. **Allocation flags constrain lifecycle.** Microsoft's `VirtualAlloc` and `VirtualAlloc2` documentation requires `MEM_LARGE_PAGES` together with `MEM_RESERVE | MEM_COMMIT`. This differs from ordinary reserve-now/commit-later mappings. Do not silently accept an invalid flag combination as ordinary pages. [W3][W4]
3. **Linux explicit HugeTLB is a resource-managed facility.** Kernel documentation describes dedicated huge-page pools, reservation accounting, size-specific pools, and `mmap`/SysV shared-memory interfaces. HugeTLB pages are not swappable; pool availability and permission/configuration affect success. Therefore Linux `MAP_HUGETLB` cannot be presumed available merely because CPU huge pages exist. [L1]
4. **Linux transparent huge pages (THP) and explicit HugeTLB must not be conflated.** The Linux kernel documents THP as a distinct mechanism, with policy/eligibility and potential fallback. A regular mapping that happens to be backed by THP is not evidence that an explicit guest `MEM_LARGE_PAGES` request has been honored. [L1][L2]

## Architecture hypothesis (not validated)

Keep the guest's large-page request, token privilege decision, alignment, reservation/commit state and failure status in the NT VM layer. A Linux backend *candidate* is `mmap(MAP_HUGETLB)` after explicit size and availability negotiation. Avoid treating THP or a regular 4-KiB mapping as a transparent success path for a strict request. This preserves observable guest failure rather than fabricating support. **OPEN:** exact Windows privilege-check timing, NTSTATUS/Win32 error mapping, cross-process allocation behavior, deallocation granularity, NUMA behavior and section-backed large pages need Windows reference tests and version scoping. No claim that `SeLockMemoryPrivilege` maps to Linux `CAP_IPC_LOCK` or a Linux hugetlb group: guest authorization and host resource authorization are distinct gates.

## Proposed falsifiable gates (not run)

- On a controlled Windows reference machine, record OS build, token privilege enabled/disabled, `GetLargePageMinimum`, allocation flags, base-address alignment, exact requested sizes and `GetLastError`. Test supported and unsupported configurations separately; never assume a particular page size.
- Verify valid `MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES` allocation and release; test reserve-only, commit-only, unaligned sizes, deliberately misaligned addresses, and pool exhaustion. Record both success and precise errors, not just boolean results.
- On Linux, capture kernel release/config, `/sys/kernel/mm/hugepages`, `/proc/meminfo`, hugetlb cgroup limits, privilege/hugetlb-group state, and `MAP_HUGETLB` success/failure. Do not change system-wide huge-page pool settings on shared hosts.
- Differential test: strict guest request must either produce an actually backed eligible large-page mapping with correct guest accounting, or return an error compatible with the measured Windows reference behavior. Explicitly test whether THP backing can change after allocation without affecting guest-reported semantics.
- Repeat under memory pressure and multiple processes; isolate privilege-denial, unavailable-pool and invalid-argument cases. No performance conclusion until comparable repeated Windows/Linux measurements.

## Sources (primary)

[W1] Microsoft, *Large-Page Support*, https://learn.microsoft.com/en-us/windows/win32/memory/large-page-support

[W2] Microsoft, *GetLargePageMinimum*, https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-getlargepageminimum

[W3] Microsoft, *VirtualAlloc*, https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc

[W4] Microsoft, *VirtualAlloc2*, https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2

[L1] Linux kernel, *HugeTLB Pages*, https://docs.kernel.org/admin-guide/mm/hugetlbpage.html

[L2] Linux kernel, *Transparent Hugepage Support*, https://docs.kernel.org/admin-guide/mm/transhuge.html

No Windows or Linux probes were executed for R82; this document is an evidence-based research proposal, not a runtime validation.
