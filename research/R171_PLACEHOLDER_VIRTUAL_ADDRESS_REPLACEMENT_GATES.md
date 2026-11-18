# R171 — Windows placeholder reservations versus Linux fixed-address mappings

Date reviewed: 2026-11-18. Status: **VERIFIED FROM SOURCE** for documented contracts; **HYPOTHESIS** for translation design; **OPEN** for conformance and concurrency. No code was run and no performance measurement was made.

## Why this is a separate gate

R65/R69/R105/R170 address guard growth, executable memory, commitment and large pages. This note instead isolates **address-space placeholder identity and reversible replacement**: a caller can reserve an address range, split it into precisely identified subranges, replace a subrange with a private allocation or section view, then restore its reservation without surrendering the address to another allocator. Treating this as ordinary mmap/munmap would omit an observable Windows contract.

## Windows primary contracts

1. Microsoft documents `VirtualAlloc2(MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS)` as creating a placeholder reservation. `MEM_REPLACE_PLACEHOLDER` requires the supplied base and size to **exactly** match a placeholder; replacement supports private allocations, with restrictions on section type. [Microsoft VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2).
2. `VirtualFree` documents `MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER` for splitting a placeholder or reverting a replaced private allocation, and `MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS` for merging adjacent placeholders. These are metadata transitions, not simply requests to unmap an arbitrary range. [Microsoft VirtualFree](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree).
3. `MapViewOfFile3(MEM_REPLACE_PLACEHOLDER)` replaces an exactly matching placeholder with a data/pagefile-backed section view; the documentation explicitly exempts this mode from the normal 64-KiB offset/base alignment requirements. Do not impose the ordinary MapViewOfFile alignment unconditionally. [Microsoft MapViewOfFile3](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile3).
4. `UnmapViewOfFile2(MEM_PRESERVE_PLACEHOLDER)` returns such a mapped view to a placeholder. The base must be the original mapped-view base, not an arbitrary interior address. [Microsoft UnmapViewOfFile2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-unmapviewoffile2).
5. Microsoft explicitly cites ring buffers and extendable reserved regions as uses. That makes stable virtual addresses and repeated view replacement meaningful compatibility behavior, not merely an allocation optimization. [Microsoft VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2).

## Linux substrate and mismatch

Linux `mmap` with `PROT_NONE` can reserve inaccessible virtual addresses. `MAP_FIXED` replaces existing mappings at the selected range, including mappings unrelated to the guest. `MAP_FIXED_NOREPLACE` instead fails on collision and therefore cannot itself replace a live placeholder reservation. `munmap` releases an address range, introducing a potential race before a subsequent mmap. [Linux man-pages mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html); [munmap(2)](https://man7.org/linux/man-pages/man2/munmap.2.html).

**VERIFIED FROM SOURCE:** these are Linux syscall semantics as documented, not proof that any combination implements the Windows state machine. **OPEN:** whether an individual Linux kernel/filesystem combination permits the required same-address replacement and view restoration with the needed failure atomicity and alignment, and whether races with other host mappings can be excluded by process ownership policy.

## Candidate translation — HYPOTHESIS, not implementation decision

Maintain a process-wide, synchronized guest VAD/placeholder interval ledger with typed states: FREE, PLACEHOLDER, PRIVATE_REPLACEMENT and SECTION_VIEW_REPLACEMENT. Each interval records start/end, backing identity, section offset, protection, and an allocation generation. Split/coalesce only when adjacent intervals satisfy the documented guest operation; replacement must match one complete placeholder interval exactly.

A `PROT_NONE` host mapping could retain the address while a placeholder exists. A carefully controlled `MAP_FIXED` operation could overlay a guest-owned reservation, **but only after verifying ownership under the runtime's address-space lock**. The lock cannot serialize unrelated host libraries, signal handlers or threads that call raw mmap independently. A design requiring exclusive address-space mediation is therefore a security and integration constraint, not a solved problem. Never apply `MAP_FIXED` to an address merely because guest metadata claims it is available.

Restoration may require remapping `PROT_NONE` at the same address; test whether failures preserve the previous view, and specify rollback/recovery before treating the operation as atomic. The guest ledger must not report success before the host mapping transition has succeeded. File-descriptor lifetime, section-object references and shared-page coherence must survive repeated map/unmap cycles.

## Falsifiable acceptance gates (not executed)

- **Windows reference:** reserve 3×allocation granularity, split into unequal page-aligned segments, reject partial/oversized replacement, replace the middle segment, restore it, then coalesce; record exact return values, errors, `VirtualQuery` regions and accessible contents.
- **Section views:** map the same pagefile-backed section twice into adjacent placeholders, mutate one view, verify alias visibility and stable addresses; unmap one with preservation, remap and verify backing/offset behavior. Repeat for file-backed views and invalid image-section replacement.
- **Alignment:** test page-sized offsets in placeholder replacement separately from ordinary 64-KiB-aligned mapping; never extrapolate the documented exemption to other modes.
- **Linux prototype:** in a disposable unprivileged process, reserve `PROT_NONE`, replace one segment using `MAP_FIXED`, restore it, inspect `/proc/self/maps`, and deliberately provoke collision/failure paths. Run on a pinned kernel, libc and filesystem; report PASS/FAIL/SKIP separately.
- **Concurrency/security:** adversarially race a second thread's mmap/munmap, signals and loader activity against replacement. Verify no host mapping outside the runtime-owned region is destroyed. If ownership cannot be guaranteed, the operation must fail closed.
- **Failure recovery:** inject invalid offsets, inaccessible backing, resource exhaustion and simultaneous split/coalesce; verify no guest ledger/host VMA divergence and no address-space escape.

## Architecture consequence

A future NT-compatible runtime needs a **guest virtual-address-region state machine** independent of Linux VMA bookkeeping. Linux provides useful mapping primitives but does not directly supply the Windows placeholder lifecycle. Whether a fully mediated userspace address-space manager is sufficient remains **OPEN** pending the differential and race gates. No performance, compatibility or security improvement is claimed.

## Source limitations

Microsoft pages are living API documentation, not a pinned Windows build or executable reference test. The Linux man-pages links document userspace interfaces, not a pinned kernel commit. Before implementation, pin the selected Linux release's `mm/mmap.c` and UAPI headers, record the target Windows build, and reconcile observed behavior against the documented contracts.
