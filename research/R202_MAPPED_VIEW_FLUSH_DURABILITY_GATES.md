# R202 — Mapped-view flush and file durability boundaries (2026-11-20)

Status: **DOCUMENTATION VERIFIED** for the cited API contracts; **HYPOTHESIS** for the proposed LinuxNTAlpha design; **OPEN** for Windows-reference results. No experiment was executed.

## Scope and nonduplication
This note isolates *mapped dirty-page flush, file metadata persistence and underlying storage completion*; it complements R11 (sections), R178 (mapped fault provenance), R186 (registry persistence) and R201 (unbuffered I/O). It is not a claim that Linux and Windows flush calls are equivalent.

## Verified contracts

1. Microsoft documents `FlushViewOfFile(base, length)` as initiating writeback of dirty mapped pages. A zero length means from the supplied base through the mapping's end. It **does not flush file metadata** and **does not wait for underlying hardware cache persistence**. Microsoft recommends following it with `FlushFileBuffers` for the stronger file-data/metadata durability intent. Network mappings can return before remote server storage persistence. [M1]
2. Microsoft states that `UnmapViewOfFile` and mapping-object closure are **not** substitutes for explicit flush: dirty pages may be written lazily after unmapping. [M2]
3. Microsoft warns that file modification time may not automatically update after mapped writes; applications may need `SetFileTime`. [M1]
4. Linux `msync` synchronizes changes in a mapped range; `MS_SYNC` waits for completion, while `MS_ASYNC` requests asynchronous synchronization. Linux has treated `MS_ASYNC` as effectively a no-op since 2.6.19 because ordinary dirty-page tracking already schedules writeback. The Linux manual specifies alignment and range/error conditions that cannot be blindly exposed as Windows errors. [L1]
5. Linux `fsync` synchronizes file data and associated metadata, but does **not** necessarily make a newly created/renamed directory entry durable; synchronizing the containing directory may also be needed. `fdatasync` may omit metadata not needed for data retrieval. [L2]
6. Microsoft documents `FlushFileBuffers` for file handles and also specifies *different* semantics for named pipes and communications devices; the NT service layer must dispatch by object type, not translate all calls to `fsync`. [M3]

## Architectural hypothesis (not implemented)
Maintain guest mapping/view identity and backing-file handle ownership independent of the host `mmap` region. A candidate file-backed implementation translates view-range flush to validated page-rounded `msync(MS_SYNC)`, then translates guest file-buffer flush to `fsync` on the backing file descriptor, with filesystem-specific error conversion. **Caution:** `MS_SYNC` may be stronger than Microsoft's documented `FlushViewOfFile` minimum; this is a candidate correctness-first implementation, not a demonstrated performance-optimal or perfectly equivalent mapping. Avoid asserting that successful `fsync` proves remote-server or power-loss durability across all hardware.

Guest `FlushViewOfFile` should validate that the supplied pointer/range belongs to a live guest view and prevent host-address aliasing, overflow and stale-view use. A zero-length request needs explicit guest view-bound resolution. No generic Linux host pointer supplied by an untrusted PE may be forwarded directly to `msync`.

## Proposed conformance gates (NOT RUN)
- **Windows reference:** create a writable mapped file; dirty multiple pages; flush a subrange and a zero-length suffix; verify return codes for invalid base, beyond-view ranges and read-only mappings. Record exact Windows build, filesystem and storage configuration.
- **Lifetime:** unmap before flush; close file handle while a view remains; duplicate handles; remap at a reused address; ensure stale view generations cannot target unrelated mappings.
- **Persistence:** distinguish visibility to a second mapping/process from crash durability. On disposable test media, compare view flush alone, file-buffer flush alone and ordered pair, with controlled fault injection. Do not claim power-loss testing without actual controlled hardware.
- **Linux backend:** test page-alignment normalization, `msync` failure translation, file `fsync` errors, and directory-entry durability on ext4/btrfs and a network filesystem separately.
- **Security:** refuse host fd confusion, cross-guest mapping flushes, overflowed offsets, and inappropriate volume-level flush privilege.

## Open questions
- Exact native `NtFlushVirtualMemory` status, in/out address and size behavior by Windows version (Win32 documentation is insufficient to settle this).
- Whether guest-compatible semantics require a weaker asynchronous backend in latency-sensitive workloads, and how to preserve error reporting if deferred writeback fails.
- Filesystem and remote-share failure/durability guarantees under real hardware and server cache configurations.

## Primary sources (accessed 2026-11-20)
- [M1] Microsoft, FlushViewOfFile: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-flushviewoffile
- [M2] Microsoft, Closing a File Mapping Object: https://learn.microsoft.com/en-us/windows/win32/memory/closing-a-file-mapping-object
- [M3] Microsoft, FlushFileBuffers: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers
- [L1] Linux man-pages, msync(2): https://man7.org/linux/man-pages/man2/msync.2.html
- [L2] Linux man-pages, fsync(2): https://man7.org/linux/man-pages/man2/fsync.2.html
