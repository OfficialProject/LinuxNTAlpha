# R61 — Windows file identity, hard links and Linux inode mapping

Research date: 2026-11-13. Status: **documentation-grounded architectural analysis; no Windows/Linux differential experiment executed**.

## Question

Can a Linux-hosted NT file subsystem derive Windows file IDs, hard-link counts and open-handle identity directly from Linux inode metadata without violating observable Windows semantics? This complements R49/R50 (delete/rename), R56 (enumeration) and R60 (path resolution), rather than revisiting their operations.

## Verified primary-source contracts

1. **Windows modern file identity is a volume-scoped pair.** `GetFileInformationByHandleEx(FileIdInfo)` returns `FILE_ID_INFO` containing a volume serial number and a 128-bit file ID. Microsoft documents comparing **both** fields to determine whether open handles identify the same file. [Microsoft FILE_ID_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_id_info); [GetFileInformationByHandleEx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex).
2. **The older handle-information structure exposes a narrower ID and hard-link count.** `BY_HANDLE_FILE_INFORMATION` includes `nNumberOfLinks`, 32-bit volume serial, and two 32-bit file-index halves. Microsoft explicitly warns that IDs may be reused, can change on some filesystems, and that the 64-bit file index is **not guaranteed unique on ReFS**; the 128-bit interface exists for that reason. [Microsoft BY_HANDLE_FILE_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-by_handle_file_information).
3. **Linux inode identity is filesystem-scoped, not a Windows volume-serial contract.** `stat`/`statx` expose device identity, inode number and link count; inode numbers are unique only within a filesystem. Hard links refer to the same inode. [Linux inode(7)](https://man7.org/linux/man-pages/man7/inode.7.html); [Linux symlink(7)](https://man7.org/linux/man-pages/man7/symlink.7.html).
4. **A file's name and identity are different dimensions.** Linux hard links are peer names for the same inode, not redirects analogous to symbolic links. Windows file-ID queries operate on handles rather than requiring a name lookup. [Linux symlink(7)](https://man7.org/linux/man-pages/man7/symlink.7.html); [Microsoft FILE_ID_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_id_info).

## Derived design implications (not verified implementations)

- **HYPOTHESIS:** Within a single stable local Linux filesystem, an internal key based on mount/filesystem identity plus inode identity may be useful for deduplicating NT file objects opened through hard-link aliases. It is **not** by itself a portable Windows `FILE_ID_INFO` encoding. Bind that key to the underlying opened file descriptor, not a path string, and define lifecycle/eviction to avoid aliasing after inode reuse.
- **OPEN:** A Windows-compatible 128-bit file ID and volume serial may need a virtual-volume identity service and persistent or generation-aware mapping. No claim is made that Linux `st_dev` is stable across mounts, reboots, namespaces or filesystem configurations, or that `st_ino` alone is sufficient for persistent IDs.
- **OPEN:** Filesystem-specific behavior (NTFS, ReFS, FAT, network shares) cannot be inferred from one Windows behavior or a generic Linux inode. Windows documentation expressly qualifies ID stability and uniqueness by filesystem.
- **HYPOTHESIS:** File sharing, delete-pending and rename policy should consult one NT open-file identity ledger across aliases, while path lookup and reparse resolution remain separate. This must be reconciled with R49/R50/R60; no locking or race-free design is established here.

## Proposed conformance matrix (NOT RUN)

On a Windows reference system, record OS build, filesystem type and volume identifier, then independently collect `GetFileInformationByHandle` and `GetFileInformationByHandleEx(FileIdInfo)` results for:

1. Two handles opened via the same path, then via two hard-link names; compare modern IDs, legacy indices, volume serials and link counts.
2. Rename one hard-link name while keeping both handles open; compare identities before/after and enumerate remaining names.
3. Delete one of two links with appropriate sharing flags; query surviving handle(s) and surviving name, separating delete disposition from link-count changes.
4. Replace a destination using the documented replacement API, while holding handles to old source and destination; capture identity of each **handle** and each newly opened **path** separately.
5. Delete and recreate a pathname repeatedly; detect any observed ID reuse rather than assuming IDs are eternal.
6. Repeat on NTFS and ReFS if available, reporting unsupported cases as SKIP rather than extrapolating.

On Linux, independently record `fstat`/`statx` device, inode, link count, mount identity if available, and descriptor lifetime for analogous hard-link, rename and unlink cases. Compare semantics, **not numeric Windows and Linux IDs**.

## Acceptance gate

Do not claim correct Windows file identity until: (a) same-object alias tests pass, (b) path changes do not silently rebind already-open NT handles, (c) ID lifetime/reuse policy is documented and tested, (d) volume namespace boundaries are explicit, and (e) filesystem-specific differences and unsupported features are reported rather than silently normalized.

## Scope and evidence limits

No code added; no Windows machine, Linux filesystem probe or performance benchmark was run for R61. Source review is verified; proposed backend mapping and conformance outcomes are hypotheses/open questions. Source URLs reviewed 2026-11-13.
