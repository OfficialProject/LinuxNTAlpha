# R50 — NT rename replacement, POSIX opt-in, and Linux rename backend gates

Evidence date: 2026-11-13 UTC. **Primary Microsoft documentation/protocol review only. No Windows or Linux experiment was run.** Extends [R10](R10_OBJECT_MANAGER_FILESYSTEM.md) and [R49](R49_FILE_DELETE_DISPOSITION_SHARING.md); focuses on *rename* rather than delete disposition.

## VERIFIED FROM SOURCE

1. The Win32 `FILE_RENAME_INFO` structure is supplied to `SetFileInformationByHandle`; `FileRenameInfo` uses `ReplaceIfExists`, while `FileRenameInfoEx` uses `Flags`. `RootDirectory` can identify the directory against which a relative destination name is resolved. `FileNameLength` is in **bytes**, and a trailing NUL is not required. The documentation also describes absolute paths, current-directory-relative paths, and NTFS stream renames. [Microsoft FILE_RENAME_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info).
2. Microsoft documents `FILE_RENAME_REPLACE_IF_EXISTS = 0x1` and `FILE_RENAME_POSIX_SEMANTICS = 0x2`. With both set, an existing destination may be replaced **despite open handles**; existing handles to the replaced file remain usable, while new opens of the destination name refer to the renamed source. This is an *opt-in* contract; it does not establish that legacy replacement has the same behavior. [Microsoft FILE_RENAME_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information).
3. The MS-FSCC `FileRenameInformationEx` wire structure has a 32-bit flags field, reserved alignment, a 64-bit `RootDirectory` field, a byte-counted UTF-16 destination name, and padding. The protocol states that `RootDirectory` **must be zero for network operations**; do not blindly apply that network restriction to local native calls. Its flags include storage-reserve and pin-state options beyond the two basic replacement flags. [MS-FSCC §2.4.43](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/4217551b-d2c0-42cb-9dc1-69a716cf6d0c).
4. The SMB2-specific legacy rename representation uses `ReplaceIfExists` rather than the EX flags field; with replacement disabled, the protocol says an already-existing target **must** cause failure. This is a distinct protocol representation and should not be confused with the native ABI layout. [MS-FSCC §2.4.42.2](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/52aa0b70-8094-4971-862d-79793f41e6a8).
5. Windows file opens' access/share compatibility includes DELETE access for rename operations; therefore a successful host `renameat2` is not evidence that NT share-access checks were met. [Microsoft CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew).

## HYPOTHESIS — implementation boundary, not validated

An NT-facing rename broker should resolve source **handle identity** and target directory/name under a guest namespace, perform access/share and target-replacement checks, and commit the namespace transition as one serialized operation. The Linux `renameat2` family is a potential final filesystem primitive, **not** a substitute for the checks. In particular, a normal Linux rename-over-existing target may preserve old open file descriptors, resembling the *explicit* POSIX replacement option but not necessarily the default NT behavior.

Use directory-descriptor-relative operations with a defined confinement policy. Any proposed path walk must address symlinks, mount crossings, hard links, case-insensitive aliases, external host writers, and the race between checking target identity and renaming it. The project has not demonstrated a race-free implementation; an advisory lock alone does not constrain unrelated host processes.

Keep namespace state and handle/object lifetime separate: replacing the target spelling does not imply that the displaced file's existing handles become the new file. R49's delete-pending model cannot be reused without independently validating rename-specific transitions.

## Proposed falsifiable conformance matrix — NOT RUN

Use a pinned Windows build and local NTFS first. Capture exact Win32 errors and, where available, NTSTATUS. Repeat on ReFS/SMB only as separate filesystem-specific cases.

1. Rename source into absent target with `FileRenameInfo` and `FileRenameInfoEx`; test absolute and directory-handle-relative names and byte lengths without terminators.
2. Rename into an existing target with replacement disabled, legacy replacement enabled, and EX `REPLACE_IF_EXISTS | POSIX_SEMANTICS`; compare name visibility and source/target open-handle reads after the operation.
3. Hold target open with and without `FILE_SHARE_DELETE`; hold source open with different share modes. Record which conflicts block rename and whether failure changes either name.
4. Duplicate or inherit source/target handles before rename; verify all surviving handles' identity and content, not merely whether a pathname exists.
5. Rename across directories, case-only rename, hard-linked target, read-only destination, and an open image section. Record volume and build-specific outcomes.
6. Pass malformed `FileNameLength`, unsupported flag combinations, and a relative path with invalid `RootDirectory`; verify rejection without host filesystem side effects.
7. Compare a candidate Linux broker under concurrent rename/open/close against the reference trace. Inject a host-side actor outside the broker to establish whether isolation assumptions actually hold.

## OPEN

- Precise legacy replacement and open-target failure behavior on each supported Windows build and filesystem.
- Mapping from guest NT namespace and security context to Linux path operations without TOCTOU escapes.
- Whether a broker plus a constrained filesystem namespace suffices when host processes can still mutate the same tree.
- Compatibility implications of advanced EX flags and NTFS alternate stream rename.
- Performance: **no measured cost** for brokered rename or any proposed optimization.

**Status:** Microsoft-documented API/protocol contracts VERIFIED FROM SOURCE; Linux broker mapping HYPOTHESIS; reference and backend tests OPEN. No general filesystem compatibility claim.
