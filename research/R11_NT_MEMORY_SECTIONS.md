# R11 — NT memory, sections, and process bootstrap

Date: 2026-11-12. Scope: source-oriented architecture research, no runtime prototype or performance measurement.

## Primary references
- Windows virtual memory: https://learn.microsoft.com/en-us/windows/win32/memory/virtual-memory-functions
- VirtualAlloc: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
- VirtualProtect: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect
- MapViewOfFile: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile
- SEC_IMAGE: https://learn.microsoft.com/en-us/windows/win32/memory/file-mapping
- Linux mmap: https://man7.org/linux/man-pages/man2/mmap.2.html
- Linux mprotect: https://man7.org/linux/man-pages/man2/mprotect.2.html
- Linux userfaultfd: https://docs.kernel.org/admin-guide/mm/userfaultfd.html

## VERIFIED: Windows reserve/commit is a separate contract
Windows VirtualAlloc distinguishes MEM_RESERVE (reserving address range) from MEM_COMMIT (committing pages) and tracks allocation granularity and page sizes. PAGE_GUARD provides one-shot guard semantics with STATUS_GUARD_PAGE_VIOLATION. VirtualProtect changes protections on committed regions, subject to restrictions. Linux mmap/mprotect are building blocks, not drop-in equivalents.

## VERIFIED: image mappings differ from ordinary data mappings
Windows SEC_IMAGE creates an executable-image section and applies image-defined protections. Linux mmap of a PE file does not perform PE relocation, section alignment, IAT resolution, TLS initialization, or Windows-specific image-section semantics.

## HYPOTHESIS: userspace VM ledger
A per-process region ledger could model Windows reservation, commitment, allocation base, state, type, and protection while backing storage with Linux mmap/mprotect. But kernel-enforced VMA changes, signal handling, fork/clone and external mappings can desynchronize the ledger. A dedicated runtime owning the process address space may simplify correctness.

## Memory correctness test plan
- Reserve/commit/decommit/release with arbitrary sizes, address hints, overlapping ranges and alignment.
- MEM_TOP_DOWN, zero-fill, protection changes, executable page transitions, and W^X policy.
- Guard-page single-shot behavior, stack growth and fault propagation.
- File mappings, copy-on-write, section views and unmapping; shared views across processes.
- Allocation granularity and Windows vs Linux page-size assumptions.
- Race tests with concurrent protection changes, address reuse, and thread faults.

## Security
Reject integer overflow in image size/offset calculations, prevent writable-executable mappings by default where possible, and audit loader trust boundaries. Never assume a Windows app that uses JIT or self-modifying code can run under unconditional W^X without compatibility design.

## Research decision gate
Prototype reservation/commit ledger and guarded stack independently before any claim of NT virtual-memory compatibility. Benchmark page faults, mapping throughput, RSS/PSS, and address-space fragmentation under matched conditions.
