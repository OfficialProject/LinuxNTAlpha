# R53 — Placeholder virtual-address reservations and mapped-view replacement

Date: 2026-11-13. Status: **VERIFIED FROM MICROSOFT API DOCUMENTATION ONLY**. No Windows reference test, Linux implementation, or performance measurement was performed.

## Question and relationship to prior work

R11 covers reserve/commit and section mappings at a broad level. This note isolates the modern **placeholder** lifecycle and its implications for a Linux-backed NT/Win32 virtual-memory ledger. R19's PE mapping prototype is not evidence of placeholder conformance. R52's error-state rules apply to these Win32 wrappers.

## Primary sources inspected

- Microsoft, [VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2) (accessed 2026-11-13).
- Microsoft, [MapViewOfFile3](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile3) (accessed 2026-11-13).
- Microsoft, [UnmapViewOfFile2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-unmapviewoffile2) (accessed 2026-11-13).
- Microsoft, [VirtualFree](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree) (accessed 2026-11-13).
- Linux [mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html) and [mremap(2)](https://man7.org/linux/man-pages/man2/mremap.2.html), for candidate host mechanisms only.

These are live documentation URLs, **not** a pinned Windows implementation or versioned Linux kernel source. Any behavior not explicit in the API contracts below remains an open reference-test question.

## VERIFIED: API contract distinctions

1. `VirtualAlloc2` accepts `MEM_RESERVE | MEM_RESERVE_PLACEHOLDER` with `PAGE_NOACCESS`. A placeholder is a reserved virtual-address range, not a committed private allocation. `MEM_REPLACE_PLACEHOLDER` replaces a placeholder with a private allocation; its base and size must **exactly** match the placeholder. The documentation restricts replacement to supported backing types, excluding images and physical-memory views.
2. `VirtualFree` distinguishes ordinary `MEM_RELEASE` (which frees the entire original reservation when size is zero) from placeholder-specific `MEM_PRESERVE_PLACEHOLDER` and `MEM_COALESCE_PLACEHOLDERS`. The documented placeholder split and coalesce operations have different range requirements; do not generalize the ordinary release rule to every flag combination.
3. `MapViewOfFile3` supports `MEM_REPLACE_PLACEHOLDER` for a data/pagefile-backed section view, requiring an exact base/size match. The documentation explicitly exempts placeholder replacement from the ordinary 64-KiB base/offset alignment requirements; the offset instead must satisfy the page alignment specified for that mode. An `SEC_IMAGE` view is not supported as a placeholder replacement.
4. `UnmapViewOfFile2` with `MEM_PRESERVE_PLACEHOLDER` returns a previously placeholder-replacing mapped view to a placeholder rather than making its address range generally free.
5. `MapViewOfFile3` describes mapping over reserved regions for ring buffers as a motivating use case. This is an API capability statement, **not** a LinuxNTAlpha performance result or evidence that every double-mapping pattern works.

## HYPOTHESIS: Linux-backed implementation

Model address-space intervals with explicit states `free`, `placeholder`, `private-reserved`, `private-committed`, and `section-view`; retain reservation identity and section backing metadata. A `PROT_NONE` anonymous mapping can reserve address space, while `MAP_FIXED` can replace a mapped interval. But `MAP_FIXED` **can overwrite unrelated mappings** if the runtime's bookkeeping races with host allocations. `MAP_FIXED_NOREPLACE` prevents clobbering but is not itself an atomic replace-existing-placeholder primitive. Neither is sufficient proof of the full Windows contract.

An isolated guest process with serialized VM mutations and a controlled host allocation policy is a plausible starting point. This still leaves thread races, signal handlers, host libraries allocating outside the ledger, mapping ownership, backing lifetime, and remote-process operations unresolved. No kernel extension is justified yet.

## Proposed falsifiable Windows-reference matrix — NOT RUN

1. Reserve a placeholder, split it into unequal page-aligned pieces, inspect each via `VirtualQuery`, and coalesce adjacent pieces. Test nonadjacent, size-mismatched, and overlapping requests.
2. Replace one exact placeholder with a private allocation; verify the resulting state and content, then return it to a placeholder. Reject off-by-one-page base/size replacements and attempts to replace only part of a placeholder without first splitting.
3. Replace an exact placeholder with a pagefile-backed mapped view; write data, unmap with `MEM_PRESERVE_PLACEHOLDER`, remap and compare backing visibility. Include two views of the same section at different addresses, with explicit expected aliasing.
4. Negative controls: `SEC_IMAGE` placeholder replacement, unaligned offset, invalid flags, mismatched view size, ordinary unmap versus preserve, and release of a non-base address.
5. Record return value, immediate `GetLastError`, `VirtualQuery` fields, Windows build, process architecture, page size and allocation granularity. Do not infer an error code without a reference result.
6. On Linux, separately test address reservation and fixed replacement in a dedicated disposable process with guarded neighbor mappings and concurrent allocator stress. Do not use `MAP_FIXED` in a shared host process before ownership/race controls exist.

## OPEN gates

- Exact Windows reference behavior for split/coalesce failures, error codes and region-query transitions.
- Atomicity and safe host replacement when other runtime components can allocate memory.
- Section handle lifetime, cross-process aliases, protection transitions and view identity.
- Windows-version-specific feature availability and compatibility fallback.
- Whether any candidate implementation is faster or more maintainable than a conventional userspace VM ledger.

## Decision boundary

**Verified:** Windows exposes a placeholder-specific reserve/split/coalesce/replace/restore lifecycle that cannot be collapsed into generic `mmap` reserve/unmap calls without preserving additional state. **Hypothesis:** a userspace ledger plus controlled Linux mappings could implement a subset. **Not claimed:** Windows-compatible virtual memory, a working test, a benchmark, or a reason to change ADR-003.
