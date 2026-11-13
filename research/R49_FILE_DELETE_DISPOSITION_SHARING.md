# R49 — File delete disposition, sharing and POSIX unlink mismatch

Evidence date: 2026-11-13 UTC. **Primary Microsoft documentation/protocol review only. No Windows-reference or Linux experiment was executed.** Extends R10 (object manager/filesystem), R11 (sections), R14 (security), and R43 (asynchronous I/O). This note addresses a concrete incompatibility risk in any direct Linux VFS backend.

## VERIFIED FROM SOURCE

1. Windows `CreateFile` checks **both directions** of sharing compatibility: a new desired access must be permitted by existing opens' share modes, and existing granted access must be permitted by the new share mode. `FILE_SHARE_DELETE` permits subsequent opens requesting delete access; delete access covers rename as well. The documented sharing options remain in effect until the handle closes, regardless of process context. A conflicting open can yield `ERROR_SHARING_VIOLATION`. [Microsoft CreateFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew).
2. The `CreateFile` documentation states that a name pending deletion following `DeleteFile` cannot be reopened; deletion is delayed until all handles close, and that attempted open yields `ERROR_ACCESS_DENIED`. This is a documented **legacy delete-pending case**, not a universal rule for all disposition flags. [Microsoft CreateFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew).
3. `FILE_FLAG_DELETE_ON_CLOSE` requests deletion after all handles close, including duplicated handles, and existing opens must have allowed delete sharing. This must not be conflated with an immediate POSIX `unlink`. [Microsoft CreateFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew).
4. `FILE_DISPOSITION_INFORMATION_EX` has distinct `DELETE` (0x1), `POSIX_SEMANTICS` (0x2), `FORCE_IMAGE_SECTION_CHECK` (0x4), `ON_CLOSE` (0x8), and `IGNORE_READONLY_ATTRIBUTE` (0x10) flags. The Microsoft protocol specification says POSIX semantics remove the visible link when the POSIX delete handle closes, while other open handles can continue accessing streams. The `ON_CLOSE` flag requires a handle opened with `FILE_DELETE_ON_CLOSE`, otherwise `STATUS_NOT_SUPPORTED` is specified. [MS-FSCC §2.4.12](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/2e860264-018a-47b3-8555-565a13b35a45); [Microsoft driver structure documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-_file_disposition_information_ex).
5. `SetFileInformationByHandle` requires appropriate granted access (e.g. `DELETE` for disposition); it warns that information-class behavior can differ by OS release and underlying driver. [Microsoft SetFileInformationByHandle](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle).
6. `FILE_DISPOSITION_INFO.DeleteFile` has no effect when the handle was opened with `FILE_FLAG_DELETE_ON_CLOSE`. [Microsoft FILE_DISPOSITION_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_disposition_info).

## Architectural inference — HYPOTHESIS

A direct `openat`/`unlinkat` translation is insufficient. Linux inode lifetime after unlink resembles the **opt-in POSIX disposition** behavior, but does not by itself enforce Windows sharing compatibility, legacy delete-pending name rejection, granted DELETE access, image-section checks, or duplicated-handle close semantics.

Candidate model: retain a subsystem-wide file identity record with granted-access and share-mode counts per open, pending disposition, name/link state, and object/handle reference counts. Serialize conflicting open, rename, disposition and close transitions around that record. The precise identity key must account for hard links, rename, mount namespaces, remote filesystems and host processes outside the subsystem; an inode-number-only key is not proven sufficient. This is a design candidate, **not an implementation**.

A host-only advisory lock is not automatically sufficient: processes outside the compatibility subsystem may not honor it. Define the supported isolation boundary before claiming Windows-style exclusion. Never expose unrestricted host pathname traversal to a guest.

## Proposed falsifiable reference matrix — NOT RUN

Run on a pinned Windows build and local NTFS volume, then separately on SMB/ReFS where available. Record all `CreateFileW` desired-access/share-mode combinations and exact `GetLastError` values immediately after failure.

1. Open A with read access and no delete sharing; attempt B with DELETE access, then retry with A allowing `FILE_SHARE_DELETE`. Reverse the conflict: A granted DELETE and B omits delete sharing.
2. With multiple compatible opens, set legacy disposition, attempt reopen by name before and after closing the disposition handle, and after the last outstanding handle. Distinguish name lookup from reads on already-open handles.
3. Repeat using disposition EX `DELETE|POSIX_SEMANTICS`; observe namespace visibility **before** and **after** closing the POSIX delete handle and reads through the other open handle. Test documented unsupported/invalid flag combinations separately.
4. Duplicate a delete-on-close handle; close original while duplicate remains, then close duplicate. Measure name visibility and existing-handle behavior at each point.
5. Test read-only attributes, mapped data views and image sections separately, recording exact NTSTATUS (where native API accessible) and Win32 error conversion. Do not presume all file systems implement every flag.
6. Test hard-link aliasing and rename while other handles remain open; ensure share checks apply to file identity, not merely the original spelling. Include host namespace confinement negative tests.
7. Repeat a minimal Linux implementation under concurrent opens/close/rename with a bounded watchdog; compare traces rather than presuming Linux errno equals Windows last-error.

## OPEN

- Versioned Windows behavior for all EX combinations, remote filesystems, and interaction with image sections.
- Correct reference-count/identity model spanning duplicated and inherited handles, multiple names, guest processes, and host external writers.
- Whether a Linux backend can enforce sharing reliably without a broker or isolated mount/filesystem boundary; no benchmark exists.
- Cross-check protocol SHOULD/MUST wording against actual Windows conformance results before freezing an ABI.

**Status:** documented API/protocol behavior VERIFIED FROM SOURCE; proposed broker/state machine HYPOTHESIS; all experiments OPEN. No claim of Windows file compatibility or performance advantage.
