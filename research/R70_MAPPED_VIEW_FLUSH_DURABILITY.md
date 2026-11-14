# R70 — mapped-view flushing versus durable file synchronization

Status: **VERIFIED FROM PRIMARY DOCUMENTATION** for the contracts below; **HYPOTHESIS** for backend design; **OPEN** for conformance. Reviewed 2026-11-14. No experiment was executed.

## Why this is a separate gate

R11/R59 cover NT section mapping, protection and copy-on-write; R62 covers timestamps. This note addresses *writeback and durability*: a correctly mapped shared view does not by itself reproduce Windows flushing contracts. Neither an `msync` success nor an `fsync` success establishes Windows-compatible error behavior without reference tests.

## Verified contracts

1. Microsoft documents `FlushViewOfFile(base, bytes)` as initiating writeback of dirty pages in the requested mapped-view byte range. A zero byte count means from the supplied address to the end of the mapping. The call **does not flush file metadata**, and does **not wait for physical persistence through a hardware disk cache**. Microsoft prescribes `FlushViewOfFile` followed by `FlushFileBuffers` when both mapped data and file metadata must be flushed. For network files, local writeback does not guarantee remote durable storage. Microsoft also cautions that mapped writes may not automatically update last-modification time. [Microsoft FlushViewOfFile](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-flushviewoffile).
2. `FlushFileBuffers` takes a file handle requiring `GENERIC_WRITE`. It is **not exclusively a regular-file API**: on a named-pipe server handle it waits for the client to read buffered data; console-output handles fail with `ERROR_INVALID_HANDLE`. These type-dependent effects rule out blindly routing every NT handle to `fsync`. [Microsoft FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers).
3. Linux `msync(addr,len,MS_SYNC)` waits for the mapped-file update to complete; `MS_ASYNC` requests asynchronous synchronization, but the Linux man-page notes that it has effectively been a no-op since Linux 2.6.19 because dirty pages are tracked. Linux requires `addr` to be page aligned; a missing mapping can yield `ENOMEM`. [Linux man-pages msync(2)](https://man7.org/linux/man-pages/man2/msync.2.html).
4. Linux `fsync(fd)` flushes file data **and file metadata**, waiting for the device to report completion, while `fdatasync` can omit metadata not needed to retrieve the data. `fsync` on the file does **not necessarily persist its containing directory entry**; a separate directory `fsync` may be required. Linux documents deferred writeback failures including `EIO`, `ENOSPC` and `EDQUOT`. [Linux man-pages fsync(2), man-pages 6.19](https://man7.org/linux/man-pages/man2/fsync.2.html).

## Architectural inference (not validated)

A candidate backend would keep *view identity, backing-file identity, requested byte range, mapping type and handle access rights* in the NT layer. For a regular shared-file view, page-align the Linux `msync` interval internally while retaining Windows-facing byte-range validation. A synchronous `MS_SYNC` is a possible conservative implementation but may be **stronger/slower** than the documented Windows `FlushViewOfFile` contract. `MS_ASYNC` cannot be assumed to guarantee an actual scheduled write on current Linux. A separate regular-file `fsync` may serve `FlushFileBuffers`, subject to rights checks, writeback-error translation and filesystem-specific behavior. Neither candidate is yet approved as a faithful implementation.

**Security boundary:** never accept an arbitrary guest pointer as authority to flush a host mapping or an arbitrary guest handle as an fd. Resolve both through owned view/handle tables and check range arithmetic and mapped extent before backend calls.

## Falsifiable conformance matrix (not run)

- Windows reference: shared writable mapped file; dirty a subrange; flush a non-page-aligned interior address with positive length, then a zero-length suffix; record success and `GetLastError` on failures. Compare with page-aligned Linux backing calls, without assuming the error codes match.
- Windows reference: test invalid/unmapped pointer, range crossing view end, read-only view, and copy-on-write view; record actual outcomes instead of guessing.
- Windows reference: test `FlushFileBuffers` on a read-only file handle, writable file handle, console output, and named-pipe server with controlled client reads. Ensure no test blocks indefinitely.
- Windows and Linux separately: verify visibility to a second mapping before/after flush, and file timestamps; do **not** claim power-loss durability from a normal read-after-write test.
- Linux isolated scratch filesystem: inject or simulate writeback failure where safely possible and verify error propagation. Do not induce host-wide faults or power interruption.
- Performance: compare `MS_SYNC`, `fsync`, and batched strategies only after semantic gates pass; report filesystem, storage, kernel, cache conditions and distribution, not a single best-case latency.

## Open questions

Exact Windows validation/rounding for arbitrary subpage ranges, COW behavior, network-file durability, error translation, and which NT-native flush operations correspond to Win32 calls remain unverified here. Windows-reference testing is necessary before selecting a backend. No performance or compatibility result is claimed.
