# R193 — NT extended attributes versus Linux xattr backend gates

Status: DOCUMENTATION REVIEW ONLY (2026-11-19). No Windows-reference, Linux filesystem, or PE runtime test was executed. This is distinct from R79 alternate data streams: EAs are metadata name/value entries, not separately opened named file streams.

## Verified primary-source contracts

1. Microsoft defines `FILE_FULL_EA_INFORMATION` as a chain of entries containing `NextEntryOffset`, `Flags`, byte-length name and value fields; `FILE_NEED_EA` is a defined flag. Each subsequent entry is 4-byte aligned, with a zero next offset for the last. A name terminator precedes the value. [Microsoft WDM structure](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_file_full_ea_information); [MS-FSCC 2.4.16](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/0eb94f48-6aac-41df-a878-79f4dcfd8989).
2. `ZwQueryEaFile` accepts an optional explicit EA-name list, index, `ReturnSingleEntry`, and `RestartScan`; it can resume enumeration from a prior call. `ZwSetEaFile` accepts a chain of full EA entries and reports an NTSTATUS and I/O status. These are documented kernel interfaces, not proof of the exact user-mode syscall export or every filesystem's behavior. [Microsoft query contract](https://learn.microsoft.com/en-us/previous-versions/ff625894%28v%3Dvs.85%29); [Microsoft set contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-zwseteafile).
3. Linux xattrs are namespaced inode-associated name/value pairs. `fsetxattr` operates on an open fd, and `flistxattr` returns names rather than an NT-style cursor/record chain. Linux VFS documents a 255-byte attribute-name limit, 64-KiB value limit, and a 64-KiB returned-name-list limit; filesystem-specific aggregate limits may be lower. [Linux xattr(7)](https://man7.org/linux/man-pages/man7/xattr.7.html); [setxattr(2)](https://man7.org/linux/man-pages/man2/setxattr.2.html); [listxattr(2)](https://man7.org/linux/man-pages/man2/listxattr.2.html).

## Architectural implications (HYPOTHESES, not demonstrated compatibility)

- An NT EA adapter must validate guest record boundaries and 4-byte next-entry offsets *before* any host xattr calls; it must not pass guest offsets or names directly into host APIs.
- A one-to-one `user.*` xattr mapping is not yet justified: NT names/flags, Linux namespace permissions, enumeration order, limits and guest access rights differ. A per-file metadata representation or guest broker may be needed for unsupported hosts.
- Maintain an explicit guest enumeration snapshot/cursor policy, rather than assuming `flistxattr` ordering and repeated calls match `ZwQueryEaFile`.
- Reconcile EA operations with R49/R50/R61 file identity, deletion, rename and hard-link behavior. The host inode can be an implementation aid, not a sufficient NT access-control decision.

## Proposed falsifiable gates (NOT RUN)

1. **Parse-only:** reject truncated headers, overlong names/values, missing name terminators, overflowing offsets, misaligned nonfinal entries, overlapping chains, illegal flag bits and excessive total buffer sizes; verify precise failure-offset reporting where the target API documents it.
2. **Windows reference:** on a specified Windows build and EA-capable filesystem, set multiple EAs, update/remove entries, query by explicit names, restart, enumerate one-at-a-time, test tiny buffers, duplicate names, casing, empty values and `FILE_NEED_EA`; record exact NTSTATUS, `IoStatusBlock`, byte counts and order. Repeat on a filesystem lacking EA support.
3. **Linux backend:** run equivalent open-fd operations on ext4, XFS and a filesystem without usable xattrs; record mount configuration, namespace privilege, capacity failures, list-size limits and hard-link persistence.
4. **Concurrency/security:** race rename/unlink and EA updates with enumeration; verify guest handle rights, no leakage of host `security.*` or `trusted.*` namespaces, no path traversal, and isolation of guest metadata from unrelated host processes.

## Open decisions

- Which guest EA names, casing rules and flags are observable on supported Windows versions/filesystems?
- What is the exact NT user-mode entrypoint and error behavior to target, validated on a Windows reference machine?
- Can a secure, crash-consistent metadata store preserve EAs when the host filesystem cannot, without violating file identity/backup semantics?

No implementation or performance claim follows from this review.
