# R72 — NT address constraints, ZeroBits, and Linux allocation placement

Date: 2026-11-14. Status: **VERIFIED FROM PRIMARY API DOCUMENTATION** for stated contracts; **HYPOTHESIS** for backend design; **OPEN** for reference conformance. **No experiment was run.**

## Why this is a separate gate

R11 discusses reservation/commit generally, and R53 discusses placeholder replacement. Neither establishes the placement contract of an *unspecified* NT base address constrained by `ZeroBits`, nor the modern Win32 address-requirements path. An ELF host's chosen `mmap(NULL,...)` address must not silently be treated as the requested NT placement.

## Verified primary contracts

1. Microsoft's `NtAllocateVirtualMemory` documentation defines `BaseAddress` and `RegionSize` as in/out pointers. When the incoming base is NULL the system selects the address; when non-NULL, the base is rounded down to the host page boundary. The nonzero size is rounded up to the page boundary. `ZeroBits` constrains the high-order address bits only when the OS selects the address; **values greater than 32 are interpreted as a bitmask**, not simply a bit count. The call can target a process specified by handle and returns NTSTATUS, not Win32 last-error. [Microsoft NtAllocateVirtualMemory](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntallocatevirtualmemory).
2. Microsoft's `VirtualAlloc2` documentation says a supplied non-NULL base cannot be combined with nonzero `MEM_ADDRESS_REQUIREMENTS` and must meet allocation granularity. `MEM_TOP_DOWN` requests the highest possible address, not merely a hint to Linux's usual address-selection policy. Its `MEM_COMMIT` of an already committed page is documented to succeed. These Win32 requirements must not be conflated with the Native API's base rounding. [Microsoft VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2).
3. Linux `mmap` treats a non-NULL address as a **hint** without `MAP_FIXED`; `MAP_FIXED` may discard overlapping mappings. `MAP_FIXED_NOREPLACE` (Linux 4.17+) rejects an occupied range with `EEXIST` and supports collision-safe placement attempts, but older kernels may ignore the flag and return a different address, which the caller must check. `MAP_32BIT` only requests the first 2 GiB on x86-64 and is ignored with `MAP_FIXED`; it does not implement arbitrary NT `ZeroBits` constraints. [Linux mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html).

These documentation pages were inspected on 2026-11-14. They are **not pinned Windows implementation revisions** or evidence of Windows build-specific corner cases.

## Architecture hypothesis, not implementation

A guest-VM allocator should accept a normalized constraint object (allowed address range, alignment, size, placement direction, reservation ownership, target process) and separately implement Native `ZeroBits` and Win32 extended-parameter decoding. Validate arithmetic, canonical user addresses, alignment, process permissions, and overlaps **before** attempting a host mapping. For an allocator-owned free interval, `MAP_FIXED_NOREPLACE` is a candidate atomic claim primitive; verify the returned pointer even on ostensibly supported hosts. Do not use `MAP_FIXED` against arbitrary process addresses to satisfy placement.

A search over free intervals might emulate top-down or restricted-range placement, but concurrent host mappings, runtime library mappings, ASLR, and kernel-reserved holes can invalidate a userspace free-space snapshot. An address-space owner and retry/serialization policy remain design hypotheses, not demonstrated correctness. Cross-process allocation requires an additional process/handle/permission mechanism; a local `mmap` cannot implement it.

## Falsifiable conformance matrix — NOT RUN

1. On a version-recorded Windows x64 reference host, call `NtAllocateVirtualMemory` with NULL base and `ZeroBits` values 0, 1, 16, 32, 33 and representative masks. Record status, returned base/size, actual address-bit constraint and repeated allocation distributions. Do **not** infer the exact >32 mask algorithm from the one-sentence documentation.
2. Repeat with explicit aligned and misaligned bases, sizes 0, 1, page-1, page+1, an occupied range and a near-overflow range; record actual rounded outputs and NTSTATUS.
3. Test `VirtualAlloc2` separately with `MEM_TOP_DOWN`, `MEM_ADDRESS_REQUIREMENTS` low/high/alignment boundaries, and explicit base plus nonzero requirements. Compare errors and addresses without assuming equivalence to `ZeroBits`.
4. On Linux, negotiate/record kernel version and mapping availability; test `MAP_FIXED_NOREPLACE` with an occupied range and an available range, checking the returned address. Distinguish kernel unsupported/denied (**SKIP**) from a semantic mismatch (**FAIL**).
5. Stress competing allocator threads and host-library mappings; verify no clobbering, no leaked reservations, correct ledger ownership and failure cleanup. Only then measure placement cost and fragmentation under controlled equivalent workloads.

## Decision gate

Do not adopt `MAP_32BIT`, unconstrained `mmap(NULL)`, or destructive `MAP_FIXED` as the universal NT allocation backend. Exact high-bit mask interpretation, `MEM_ADDRESS_REQUIREMENTS` edge cases, error translation, remote process placement, and Windows build variation remain **OPEN** until Windows-reference tests exist.
