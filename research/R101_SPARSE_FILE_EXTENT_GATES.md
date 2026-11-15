# R101 — Windows sparse-file controls versus Linux extent operations

Status: **primary-source contract research; no implementation or experiment performed** (2026-11-15).

## Scope and verified contracts

1. Windows sparse-file support is a **volume capability** (`FILE_SUPPORTS_SPARSE_FILES`), not a universal property. An application marks a file sparse through `FSCTL_SET_SPARSE`; merely supplying `FILE_ATTRIBUTE_SPARSE_FILE` to `CreateFile` does not create a sparse file. Microsoft documents that clearing the sparse state using a FALSE `FILE_SET_SPARSE_BUFFER` fails in the documented interface. [MS sparse operations](https://learn.microsoft.com/en-us/windows/win32/fileio/sparse-file-operations); [FSCTL_SET_SPARSE](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_sparse).
2. `FSCTL_SET_ZERO_DATA` zeros a byte range **without extending the file**; on sparse or compressed NTFS files it **may** deallocate storage. The word *may* is important: physical allocation reduction is not a universal correctness guarantee. [Microsoft FSCTL_SET_ZERO_DATA](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_zero_data).
3. `FSCTL_QUERY_ALLOCATED_RANGES` returns ranges that **may contain nonzero bytes**, not exact locations of nonzero bytes. For nonsparse/uncompressed files Microsoft documents a single range for the requested span. NTFS may round query boundaries. Mapped remote-file writes may require a view flush before the query coherently reflects allocation. [Microsoft FSCTL_QUERY_ALLOCATED_RANGES](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_query_allocated_ranges).
4. Linux `fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE)` creates holes, zeros partial blocks and preserves file size; support is filesystem-dependent. `FALLOC_FL_ZERO_RANGE` makes reads return zero but may **preallocate unwritten extents**, so it is **not** an equivalent substitute when the objective is deallocation. [Linux fallocate(2)](https://man7.org/linux/man-pages/man2/fallocate.2.html).

## Architecture implication (reasoned, not demonstrated)

A LinuxNTAlpha file-object adapter should keep **guest sparse attribute/capability state** separate from the host's current extent map. A proposed backend decision sequence is: validate guest handle rights and range/EOF constraints; decide whether the guest file is sparse; prefer host hole punching for a sparse zeroing request when supported, otherwise use a safe zero-writing fallback; preserve guest-visible length and readback; report host extent layout only through a guest-compatible allocated-range translation. This is a **candidate design**, not a claim of full Windows equivalence. In particular, zeroing a nonsparse file should not silently change the guest-visible sparse attribute, and `FALLOC_FL_ZERO_RANGE` must not be assumed to reclaim space.

Linux `SEEK_DATA`/`SEEK_HOLE` and Windows allocated-range reporting may disagree on granularity, delayed allocation, unwritten extents and filesystem behavior. **Open question:** what conservative range translation meets Windows reference observations on each target filesystem? Do not promise exact physical extent equivalence.

## Differential gates to implement later (not run)

- On Windows reference volumes: query sparse support; attempt creation with sparse attribute alone, then `FSCTL_SET_SPARSE`; record attributes and error codes.
- For a sparse file with nonzero sentinels, zero unaligned ranges (including partial blocks, beyond EOF and crossing EOF); record byte-for-byte readback, length, sparse attribute and allocated ranges.
- Query ranges with tiny/insufficient output buffers, unaligned offsets, empty files, and nonsparse files; capture returned counts, partial results and errors.
- Repeat on Windows NTFS and supported network filesystems separately; never generalize NTFS results to all Windows filesystems.
- On Linux ext4/XFS/Btrfs where available, separately probe punch-hole, zero-range and ordinary zero writes; record return codes, `stat` size, `st_blocks`, `SEEK_DATA`/`SEEK_HOLE` and readback. Treat `st_blocks` as a diagnostic, not a Windows oracle.
- Add races with mapped views, concurrent writes, truncation and duplicate handles only after basic semantic gates are established.

## Decision boundary

**Verified:** Windows exposes sparse attribute/zero/query contracts; Linux offers distinct allocation primitives with filesystem-dependent support. **Hypothesis:** an adapter with explicit guest metadata plus per-filesystem capability probing can meet useful sparse-file compatibility without kernel changes. **Unknown:** precise cross-filesystem range-query, sharing, error and mapped-view equivalence. No benchmark, compatibility percentage or Windows reference experiment is claimed.

## Related research

R49 (delete/sharing), R57 (byte-range locks), R61 (file identity), R64/R70 (mapped flush), R100 (file position). This note addresses sparse **allocation control**, not those separate contracts.
