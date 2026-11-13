# R64 — Mapped-file flush boundaries: FlushViewOfFile versus msync

Research date: 2026-11-13. Status: **primary documentation reviewed; no experiment run**.

## Question

Can an NT-compatible mapped-file flush operation be implemented by blindly forwarding `FlushViewOfFile` to Linux `msync(MS_SYNC)`? This is distinct from R59's image/data-section layout and R62's timestamp semantics.

## Verified from primary documentation

1. Microsoft defines `FlushViewOfFile(base, length)` over a **byte range** within a mapped view. A zero length means from the specified base to the end of the mapping. It initiates writing dirty pages in that range; it **does not flush file metadata** and **does not wait for physical disk-cache persistence**. Microsoft recommends `FlushViewOfFile` followed by `FlushFileBuffers` for the stronger combined operation. On network mappings it does not guarantee remote stable storage. Microsoft also warns that the last-modification timestamp may not automatically update after mapped writes. [Microsoft FlushViewOfFile](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-flushviewoffile) (documentation inspected 2026-11-13).
2. Microsoft documents `FlushFileBuffers` as a separate operation on an **open file handle**, which must have `GENERIC_WRITE` access for a file; the same API has different semantics for communication devices and named pipes. [Microsoft FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers) (inspected 2026-11-13).
3. Linux `msync` flushes modifications to a file-backed memory mapping; `MS_SYNC` waits for the update, while `MS_ASYNC` schedules it. Linux documents `EINVAL` for an address not aligned to the page size and `ENOMEM` for an unmapped range. Linux currently treats `MS_ASYNC` as a no-op because dirty pages are already tracked. [Linux man-pages msync(2)](https://man7.org/linux/man-pages/man2/msync.2.html) (inspected 2026-11-13).
4. POSIX states that `msync` on a `MAP_PRIVATE` mapping does not write private changes to the backing object; POSIX leaves unspecified whether additional file attributes are written. [POSIX msync(3p)](https://man7.org/linux/man-pages/man3/msync.3p.html) (inspected 2026-11-13).

These are documented interface contracts, **not** measurements of particular Windows or Linux builds. The linked documentation is not pinned to immutable commits.

## Architectural consequences — hypotheses, not implementation claims

- **HYPOTHESIS:** Linux `msync(MS_SYNC)` may provide a *stronger wait condition* than Windows `FlushViewOfFile` alone, but that does not establish identical success/failure behavior, metadata handling, network durability or latency. A direct alias would conceal the distinction between flushing a view and flushing a file handle.
- **HYPOTHESIS:** A compatibility layer should resolve the guest view's logical byte interval against its registered mapping(s), check arithmetic overflow and access rights, round the host `msync` start **down** to host page boundaries, and extend the covered length to include the last requested byte without escaping the actual mapped range. Whether the native NT entrypoint accepts non-page-aligned bases must be determined from a Windows oracle, not guessed from Win32's description.
- **OPEN:** A guest `length=0` is **not** a Linux `msync(addr,0,...)` contract: the implementation needs its own mapping registry to determine the remaining view length.
- **OPEN:** Translating `FlushFileBuffers` to `fsync` or `fdatasync` needs handle-type, access-check, error-mapping and filesystem-specific tests; the mapped view may outlive the originating file handle. A pathname-only lookup would be wrong under rename/unlink scenarios (R49–R61).
- **OPEN:** Image sections, copy-on-write data views, anonymous/pagefile-backed mappings, network filesystems, and memory mapped device regions must not all be treated as identical file-backed `MAP_SHARED` mappings.
- **OPEN:** A stronger Linux flush might impose measurable performance costs. Do not label it a correctness win or regression without comparative tests.

## Falsifiable differential test plan — NOT RUN

1. On a documented Windows version and filesystem, create a file mapping with two views; dirty one byte in the middle and call `FlushViewOfFile` with lengths 1, page size, zero, and a range crossing a page boundary. Record return, `GetLastError` on failure, backing-file bytes, and exact input addresses.
2. Test an unaligned view-relative pointer, unmapped pointer, out-of-range length, read-only view, and `FILE_MAP_COPY` view. Capture failure modes without assuming they match Linux errno.
3. Repeat after closing the original file handle while retaining the mapping/view; compare a separate `FlushFileBuffers` call made while a valid write-capable handle exists.
4. Record modification timestamp behavior separately (R62). On Windows SMB, distinguish client acknowledgement from remote durable write; do **not** claim to test power-loss safety without a controlled crash/fault-injection environment.
5. On Linux, use `MAP_SHARED` and `MAP_PRIVATE` with `msync(MS_SYNC)` and `msync(MS_ASYNC)`; test aligned versus unaligned ranges, `fsync` separately, and report kernel, filesystem, mount options, storage and observed errors. Do not infer durable physical media writes from a successful return alone.
6. Measure latency distributions only after correctness gates; record CPU/storage/cache state and avoid comparing one cold flush against one warm flush.

## Acceptance gates

- Maintain separate *view flush* and *file-handle flush* operations in the interface design.
- Validate interval bounds with checked arithmetic and preserve guest view identity independently of pathname.
- Establish Windows-reference behavior for zero-length and unaligned ranges before deciding translation policy.
- Define and test error translation and handle-access behavior, including non-file handle types.
- Keep durability guarantees explicitly limited to the source-backed contracts and tested storage configurations.

**No code, benchmark, Windows oracle, or Linux experiment was executed for this note.**
