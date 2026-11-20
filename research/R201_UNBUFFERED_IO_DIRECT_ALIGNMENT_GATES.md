# R201 — Windows unbuffered file I/O versus Linux O_DIRECT alignment gates

Status: **VERIFIED FROM SOURCE** for cited public API contracts; **HYPOTHESIS** for the proposed translation; **OPEN** for versioned conformance, backend behavior and performance. Research date: 2026-11-20. **No experiment was run.**

## Architectural question

Can a Linux-hosted NT file service implement Windows `FILE_FLAG_NO_BUFFERING` by forwarding the flag to Linux `O_DIRECT` without changing guest-visible alignment errors, file-pointer behavior, or cache semantics?

**Finding:** Not as an unconditional one-to-one mapping. The documented alignment constraints and failure/fallback behavior are not identical. This is a contract comparison, not a measured incompatibility on a particular host.

## Verified source contracts

1. Microsoft's [File Buffering](https://learn.microsoft.com/en-us/windows/win32/fileio/file-buffering) describes `FILE_FLAG_NO_BUFFERING` as bypassing *system* file-data caching, not hardware disk caching. On seekable files, transfer sizes and explicit offsets must be sector-size multiples. Buffer addresses should be physical-sector aligned, but enforcement may vary. Logical and physical sector sizes can differ (e.g. 512e). The documentation explicitly cautions that alignment rules for nonseekable devices can differ.
2. Microsoft's [SetFilePointerEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointerex) documents that seeking to a non-sector-aligned offset on an unbuffered handle fails with `ERROR_INVALID_PARAMETER`. The shared file pointer is separate from the `OVERLAPPED` explicit offset. Therefore validating only a `pread`/`pwrite` submission is insufficient for the Win32 API surface.
3. Microsoft's [GetFileInformationByHandleEx](https://learn.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-getfileinformationbyhandleex) supports `FileAlignmentInfo`, returning [`FILE_ALIGNMENT_INFO`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_alignment_info). The documented structure has `AlignmentRequirement`; its API availability starts with Windows 8 / Server 2012. **Do not assume this one value alone is the volume's logical and physical sector size.**
4. Linux [open(2)](https://man7.org/linux/man-pages/man2/open.2.html) documents that `O_DIRECT` alignment requirements depend on kernel and filesystem and that misaligned I/O may fail with `EINVAL` **or fall back to buffered I/O**. Linux `O_DIRECT` is not a universal promise of a specific Windows-like alignment or error code.
5. Linux [statx(2)](https://man7.org/linux/man-pages/man2/statx.2.html) documents `STATX_DIOALIGN` since Linux 6.1, with `stx_dio_mem_align` and `stx_dio_offset_align`; availability is filesystem dependent. Newer `STATX_DIO_READ_ALIGN` can distinguish read alignment. A missing reported capability must not be interpreted as proof that direct I/O works with arbitrary alignment.

Sources above were consulted 2026-11-20. The Linux man-pages URLs are living documentation, **not** pinned kernel-source commits; this note does not claim to verify a specific running kernel.

## Hypothesis: separate guest contract from host backend

Maintain guest-visible file-handle state: unbuffered flag, synchronous versus overlapped mode, guest file pointer, guest logical/physical sector properties, and backend capability. Validate the **guest** request (length, offset, buffer alignment where enforced, seek operation) before submitting host I/O. Select `O_DIRECT` only after determining host per-file support and alignment; a backend fallback or explicit unsupported result needs a documented policy and must not silently claim uncached I/O.

Avoid using Linux `statx` alignment numbers as a replacement for Windows volume geometry. The guest's advertised properties and accepted requests must remain internally consistent. A bounce buffer might satisfy host alignment for some guest-valid requests but changes memory-copy cost and potentially caching behavior; it is an unvalidated candidate, not a free optimization.

This proposal does **not** resolve Windows file sharing, asynchronous cancellation, completion-port notification, sparse files or durable flush ordering; those require their own contracts.

## Falsifiable conformance gates

- **Windows reference matrix:** record OS build, filesystem, device sector geometry and handle flags. Test aligned/misaligned buffer pointers, lengths and offsets; `SetFilePointerEx` seek alignment; explicit `OVERLAPPED` offsets; zero-byte operations; EOF; and simultaneous buffered/unbuffered handles. Capture exact return values, bytes transferred and `GetLastError` rather than assuming all devices enforce buffer alignment equally.
- **Host capability matrix:** record Linux kernel, filesystem, mount, file type and `statx` mask/values. Test `O_DIRECT` aligned and misaligned reads/writes on disposable files and determine whether the host rejects or buffers. Never infer cache bypass merely from a successful return.
- **Translation state machine:** compare guest error ordering for invalid handle, bad alignment, invalid offset, access denial and cancellation; test that explicit-offset operations do not mutate a shared synchronous file pointer.
- **Isolation:** prohibit `O_DIRECT` or block-device probing on arbitrary host paths or raw devices; constrain tests to disposable, broker-owned files. Verify that geometry and error reporting do not expose unowned host device metadata.
- **Performance:** compare buffered I/O, direct I/O and any bounce-buffer path over sequential/random workloads with identical durability and cache conditions. No speedup or latency reduction is asserted.

## Scope and nonduplication

This note addresses the **buffering/alignment contract**, distinct from [R100 file-pointer/explicit-offset gates](R100_FILE_POINTER_EXPLICIT_OFFSET_GATES.md), [R195 asynchronous request ownership](R195_ASYNC_REQUEST_OWNERSHIP_COMPLETION_GATES.md) and [R64 mapped-view flush durability](R64_MAPPED_VIEW_FLUSH_DURABILITY.md). No architecture decision is changed.

**Decision gate:** reject unconditional `FILE_FLAG_NO_BUFFERING` → `O_DIRECT` forwarding until guest geometry, host capability, error translation and cache behavior have been validated on a documented Windows/Linux matrix.
