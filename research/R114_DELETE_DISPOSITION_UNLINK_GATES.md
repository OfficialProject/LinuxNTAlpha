# R114 — NT delete disposition versus Linux unlink (documentation review)

Date: 2026-11-16. Status: **primary-source documentation review; no experiment performed**. This note is distinct from R79 (alternate streams), R83 (oplocks), R87 (object-name lifetime), R89 (path translation), and R100 (file-pointer state). It addresses *file-system deletion state and namespace visibility*, not general NT object deletion.

## Verified source contracts

1. Microsoft documents `FILE_DISPOSITION_INFORMATION_EX` flags: DELETE (0x1), POSIX_SEMANTICS (0x2), FORCE_IMAGE_SECTION_CHECK (0x4), ON_CLOSE (0x8), IGNORE_READONLY_ATTRIBUTE (0x10). The caller needs DELETE access. Without POSIX_SEMANTICS, the file marked for deletion is not actually removed until the open handles close and the link count is zero; with POSIX_SEMANTICS, the link disappears from the visible namespace when the POSIX-delete handle closes, while other already-open handles can continue using streams. The documentation lists STATUS_CANNOT_DELETE for read-only files or existing mapped views, subject to flags and filesystem behavior. [Microsoft WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-_file_disposition_information_ex).
2. Microsoft's [MS-FSCC FileDispositionInformationEx](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/2e860264-018a-47b3-8555-565a13b35a45) additionally specifies ON_CLOSE constraints and status cases; distinguish protocol requirements from what a particular Windows filesystem and build actually accepts.
3. Win32 [DeleteFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilea) documents legacy delete-pending behavior: open sharing must allow deletion, later opens of a pending file fail with ERROR_ACCESS_DENIED, whereas POSIX deletion can make later name-based opens fail with ERROR_FILE_NOT_FOUND. [CreateFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea) documents FILE_SHARE_DELETE and DELETE_ON_CLOSE; share permissions are associated with existing opens, not simply pathname permissions.
4. Linux [unlink(2)](https://man7.org/linux/man-pages/man2/unlink.2.html) removes a directory entry immediately and preserves an unlinked inode while open descriptors remain. Linux [close(2)](https://man7.org/linux/man-pages/man2/close.2.html) describes last-reference cleanup. Neither call provides NT-style share-access arbitration or a guest-visible delete-pending state.

## Architectural inference (not implemented)

A direct `unlinkat` translation of every NT delete request is **not semantically sufficient**: it removes the name too early for legacy deletion, permits host opens that should be blocked by guest sharing/delete-pending state, and cannot by itself enforce the guest DELETE access right. Conversely, delaying all unlinks would break POSIX-disposition visibility.

Candidate design: a broker-managed file identity with per-open granted access, share masks, disposition mode, pending-delete state, link/name tracking, and outstanding guest handle references. Serialize guest open/rename/delete decisions through this broker; defer or perform host unlink according to the guest state machine. This is a **hypothesis**, not a proof of complete semantics. Hard links, cross-process host accesses, file IDs, rename races, mmap/image sections, remote filesystems, and crashes make name-only bookkeeping unsafe. A userspace broker cannot control unrelated host processes without additional containment/mount policy.

## Falsifiable Windows-reference gates (not executed)

- Open A without FILE_SHARE_DELETE; attempt delete via B; record Win32 error and NTSTATUS independently. Repeat with A allowing delete.
- Legacy delete pending: retain A, request delete via B, close B, test name-based reopen and A read/write, then close A; distinguish access-denied from not-found at each stage.
- POSIX disposition: retain another sharing-compatible handle; mark with POSIX_SEMANTICS, close the delete handle, test namespace disappearance and surviving handle I/O.
- Repeat with FILE_DISPOSITION_ON_CLOSE, DO_NOT_DELETE, IGNORE_READONLY_ATTRIBUTE, read-only attribute, and image-mapped view; record exact Windows build, filesystem, privileges, and return statuses.
- Hard-link two names to one file, delete one name under both modes, and check link count/name visibility and open-handle behavior.
- Stress open/delete/rename races across processes; confirm that no successful guest open bypasses share arbitration. Repeat with unrelated host processes to expose the containment boundary.

## Open questions / research frontier

- Which exact state transitions apply to `FileDispositionInformation` versus `FileDispositionInformationEx` on NTFS, ReFS, and SMB, and across supported Windows versions?
- Can guest name identity be made stable under hard-link and rename races without a filesystem-specific host adapter?
- How should a crashed guest process, broker restart, or preexisting host mapping affect pending deletion?
- Which section-object/image-view restrictions are mandatory for each flag combination? The WDK and protocol descriptions must be reconciled by versioned Windows-reference tests.

**Evidence boundary:** This document reports published API/protocol contracts and design deductions only. No Windows conformance run, Linux prototype, benchmark, or implementation has been performed for R114.
