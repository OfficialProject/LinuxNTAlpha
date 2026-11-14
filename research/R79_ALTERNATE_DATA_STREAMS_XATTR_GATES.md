# R79 — NTFS alternate data streams versus Linux extended attributes

Status: **VERIFIED FROM PRIMARY DOCUMENTATION** for stated API contracts; **HYPOTHESIS** for backend design; **NOT REPRODUCED**. Reviewed 2026-11-14. No Windows or Linux experiment was executed for this note.

## Question

Can a Linux-hosted NT file layer represent Windows named `$DATA` streams simply by storing each stream in a Linux extended attribute (xattr), while preserving stream enumeration, file identity, open-handle behavior and safe path handling?

**Finding:** No general equivalence follows from the published interfaces. Linux xattrs are bounded, whole-value metadata operations; NTFS data streams are separately sized byte streams. A compatibility layer must implement stream identity, independent byte I/O and lifecycle semantics rather than expose xattrs as streams without qualification.

## Verified contracts

1. Microsoft's *File Streams (Local File Systems)* documents `filename:streamname:streamtype` syntax, an unnamed default `::$DATA` stream, and per-stream allocation size, actual size and valid-data length. It also describes per-stream compression, encryption and sparseness state. This is NTFS-oriented documentation, not a guarantee that all Windows filesystems support ADS. [Microsoft File Streams](https://learn.microsoft.com/en-us/windows/win32/fileio/file-streams).
2. `FindFirstStreamW` enumerates `$DATA` streams. For files, the first stream is the default unnamed stream; directories do not have an unnamed `$DATA` stream by default. A filesystem lacking streams can return `ERROR_INVALID_PARAMETER`; no streams can return `ERROR_HANDLE_EOF`. [Microsoft FindFirstStreamW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirststreamw).
3. The kernel-facing `FILE_STREAM_INFORMATION` records `NextEntryOffset`, a **byte** length for the UTF-16 stream name, stream size and allocation size. Therefore a guest NT enumeration implementation cannot substitute `listxattr` output directly. [Microsoft FILE_STREAM_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_stream_information).
4. Linux `xattr(7)` describes namespace-qualified name/value pairs with whole-value `getxattr` and `setxattr` operations, a VFS value limit of 64 KiB, and filesystem-dependent tighter aggregate constraints. `listxattr(2)` has a 64 KiB returned-name-list limit. These are not a byte-stream API. [Linux xattr(7)](https://man7.org/linux/man-pages/man7/xattr.7.html); [Linux listxattr(2)](https://man7.org/linux/man-pages/man2/listxattr.2.html).
5. Linux `setxattr(2)` replaces the attribute value (or creates it), with `XATTR_CREATE`/`XATTR_REPLACE` existence controls; it does not offer stream offsets, independently seekable descriptors, or an NT stream handle. [Linux setxattr(2)](https://man7.org/linux/man-pages/man2/setxattr.2.html).

## Architectural implications (hypotheses, not measured)

- **Reject xattr-only as the general ADS backend.** A 1 MiB named data stream already exceeds Linux's documented VFS per-value limit. Rewriting a whole xattr for a small stream update would also impose avoidable copy/write amplification; magnitude is unmeasured.
- **Candidate:** maintain a guest file-object identity mapping to independently stored named-stream payloads, with a transactional metadata index. Preserve guest file identity across hard links and renames; serialize metadata transitions and stream opens through the NT object/file service. This design has not been validated for crashes, concurrency, or performance.
- **Host escape gate:** never translate untrusted guest `:` stream syntax into arbitrary host paths. Keep stream-name parsing within the guest namespace and resolve underlying host objects with a confined, handle-relative path policy (see R60).
- **Access and sharing gate:** guest per-stream opens still need NT access checks, share-mode compatibility, delete/rename rules, and lifetime management (R10, R49, R50, R61). Linux file descriptors or inode xattrs alone do not prove those contracts.
- **Backend alternatives:** a dedicated virtual filesystem, sidecar object store, or a filesystem with native streams might reduce semantic translation. Evaluate actual durability, atomicity, inode association, quota and migration behavior before selection. Do not assume native NTFS support or universal filesystem behavior.

## Falsifiable conformance plan — not executed

On a disposable Windows NTFS volume, using documented Win32 APIs:

1. Create a regular file, write distinct bytes to default and named streams, enumerate with `FindFirstStreamW`/`FindNextStreamW`; record exact names and lengths.
2. Seek/write/extend/truncate the named stream without changing default stream content; compare file sizes and stream enumeration.
3. Open the same named stream twice with conflicting sharing flags; compare failures to opening a different stream of the same base file. Test deletion while open, with explicit share-delete variants.
4. Create a hard link and rename the base file; check whether named streams remain reachable by each alias, and whether handle identity persists.
5. Test directory named streams, missing stream names, and unsupported backing filesystems separately; do not generalize NTFS observations.
6. Test large (>64 KiB) stream content and multiple stream names, including UTF-16 names and malformed syntax; collect Win32 error codes.

On Linux, in an isolated temporary directory, compare a candidate sidecar implementation to those captured Windows results. Add crash-injection at index/payload commit boundaries, hard-link alias races, symlink escape attempts, namespace collisions, and concurrent deletion. Report kernel/filesystem versions and PASS/FAIL/SKIP separately. Do **not** use a host xattr as an NT stream without an explicit size/semantics limitation.

## Decision gate

Keep ADS storage undecided. Require Windows-reference traces plus Linux implementation tests before adopting a backend. No compatibility, security or performance improvement is claimed.
