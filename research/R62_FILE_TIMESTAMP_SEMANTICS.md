# R62 — Windows file timestamps versus Linux inode clocks

Research date: 2026-11-13. **Source-reviewed; no experiment executed.**

## Architectural question

Can Linux `statx` and `utimensat` directly back the four timestamps exposed by Windows `FILE_BASIC_INFO`? This is separate from R61's file *identity* and R49/R50's delete/rename *authorization*.

## Verified primary-source facts

1. Microsoft defines `FILE_BASIC_INFO` with `CreationTime`, `LastAccessTime`, `LastWriteTime`, `ChangeTime`, and `FileAttributes`. Its time values are FILETIME units: 100-ns intervals since 1601-01-01 UTC. Microsoft distinguishes data-stream writes from metadata changes such as renames. [Microsoft FILE_BASIC_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_basic_info).
2. Linux `stat`/`statx` expose access, modification, and status-change timestamps; `statx` can also report birth time when supported. Linux **ctime means last inode status change, not creation time**. Availability and precision vary with filesystem. [Linux inode(7)](https://man7.org/linux/man-pages/man7/inode.7.html); [Linux statx(2)](https://man7.org/linux/man-pages/man2/statx.2.html).
3. Linux `utimensat`/`futimens` set **atime and mtime**, not arbitrary creation time or ctime. `UTIME_OMIT` leaves a selected timestamp unchanged; precision is limited by the filesystem. [Linux utimensat(2)](https://man7.org/linux/man-pages/man2/utimensat.2.html).
4. Windows `GetFileInformationByHandleEx` and `SetFileInformationByHandle` provide handle-oriented query/update APIs; this does not establish identical behavior across all filesystems. [Microsoft GetFileInformationByHandleEx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex); [Microsoft SetFileInformationByHandle](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle).

## Implications — hypotheses, NOT implemented

- **HYPOTHESIS:** atime/mtime may form a partial backend for LastAccessTime/LastWriteTime after explicit epoch, range, and precision conversion. Mapping Linux `stx_btime` to CreationTime is conditional on filesystem support and does not solve explicit Windows creation-time updates.
- **OPEN:** Do not equate Linux ctime with Windows ChangeTime in all circumstances: triggers, explicit setting, and persistence need differential tests. Linux ctime cannot be independently set with `utimensat`.
- **HYPOTHESIS:** a virtual-volume metadata layer may be needed for writable CreationTime/ChangeTime, but durability, hard-link alias consistency, crash recovery, permissions, external Linux writes, and host mount behavior are unresolved. Any sidecar keyed only by pathname would conflict with R61 identity and R49/R50 rename/delete behavior.
- **OPEN:** Determine exact zero/unchanged semantics of Windows basic-info setters and the behavior of timestamp update suppression on handles; do not infer from structure definitions.
- **OPEN:** Filesystem-specific time granularity, mount `noatime`/`relatime`, network filesystems, and time range limitations may make an apparently exact mapping observably wrong.

## Proposed differential conformance tests — NOT RUN

Use a documented Windows build/filesystem and Linux kernel/filesystem/mount options. Record raw integer timestamps, not just formatted local times.

1. Create a file, query all four Windows times through an open handle, then write bytes, rename, change attributes, create a hard link, and remove one link. Observe which fields change and whether two aliases agree.
2. Set each time separately through `SetFileInformationByHandle(FileBasicInfo)`; test unchanged fields and explicit values, reporting errors and privileges. Compare with `GetFileInformationByHandleEx(FileBasicInfo)`.
3. Repeat queries through the original open handle after rename and deletion, separating handle identity from pathname lookup.
4. On Linux collect `statx` mask, `stx_btime`, atime, mtime, ctime and filesystem/mount information. Test `futimens` with `UTIME_OMIT`, plus writes, renames and chmod; record truncation/rounding and unsupported birth-time reports.
5. Exercise times before 1970, near representable limits, and non-100-ns-aligned values. Validate conversion with checked arithmetic and explicit failure paths.
6. Repeat on NTFS/ReFS and ext4/XFS where available; unsupported environments are **SKIP**, not evidence of compatibility.

## Acceptance gate

Do not claim Windows timestamp conformance until all four fields, explicit setter behavior, hard-link aliasing, handle lifetime, filesystem precision, and error translation pass a Windows-reference matrix. No Linux host or Windows reference experiment, implementation, or benchmark was performed in this research entry.

Primary-source URLs reviewed 2026-11-13. Documentation is unpinned and is not evidence of an immutable ABI or of tested runtime behavior.
