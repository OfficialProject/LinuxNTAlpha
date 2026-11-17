# R147 — NT Object Manager symbolic-link resolution and Linux path-boundary gates

Date: 2026-11-17. Method: primary documentation review only; **no code executed and no Windows reference experiment**. This study extends R89 (DOS-device namespace) and R60 (filesystem reparse points), without treating either as equivalent to Object Manager symbolic-link objects.

## Verified published contracts

1. Microsoft's `OBJECT_ATTRIBUTES` reference distinguishes `RootDirectory` (a filesystem directory or Object Manager directory handle), `OBJ_OPENLINK` (open the symbolic-link object itself rather than follow its target), and `OBJ_DONT_REPARSE` (refuse name parsing on an encountered reparse with `STATUS_REPARSE_POINT_ENCOUNTERED`). The latter wording does **not** establish whether Object Manager symbolic links and filesystem reparse points have identical flag behavior. [S1]
2. Microsoft documents `NtOpenSymbolicLinkObject` as opening an existing symbolic-link object, and `NtQuerySymbolicLinkObject` as returning its target in an initialized `UNICODE_STRING`; the query documents `STATUS_BUFFER_TOO_SMALL` and required-length reporting. These are separate object APIs, not Linux `readlink` aliases. Microsoft warns these functions may change. [S2, S3]
3. Linux `openat2` has explicit component-resolution restrictions, including `RESOLVE_NO_SYMLINKS`, `RESOLVE_BENEATH`, and `RESOLVE_IN_ROOT`. They constrain Linux VFS pathname traversal, **not** guest Object Manager namespace lookup or guest security descriptors. [S4]

## Architectural inference — not implemented

**HYPOTHESIS:** Implement a guest namespace resolver with typed entries (directory, symbolic link, device, file gateway), process/session-specific DOS-device lookup, root-handle identity and granted-access checks. Keep guest link-following decisions separate from the host's `openat2` confinement policy. Only after guest resolution reaches a filesystem-backed object should the host open a confined path; a guest link target must not become an unchecked host absolute path.

**HYPOTHESIS:** A resolver needs a finite traversal budget, stable reference ownership for the link object during lookup, and explicit distinction between opening a link object and following it. These are proposed safety properties, **not** asserted Windows limits or observed Windows behavior.

## Documentation gap requiring Windows reference tests

The generic `OBJECT_ATTRIBUTES` documentation defines `OBJ_DONT_REPARSE`, but the Microsoft user-mode `NtCreateFile` reference describes its `Attributes` values more narrowly. Neither reference establishes an unconditional cross-version outcome for `OBJ_DONT_REPARSE` applied to a DOS-device path that traverses an Object Manager symbolic link. Do **not** infer a universal result for `\\??\\C:` or conflate `OBJ_OPENLINK`, `OBJ_DONT_REPARSE`, and `FILE_OPEN_REPARSE_POINT`. [S1, S5]

## Proposed conformance and security gates (NOT RUN)

- On pinned Windows builds, create/query/open a permitted Object Manager symbolic link; record granted rights, target-string length, short-buffer status and whether opening the link differs from following it.
- Test absolute and `RootDirectory`-relative names, link chains, missing targets, name collisions, link deletion while handles remain, and access-denied targets. Capture NTSTATUS and object type, not just success/failure.
- Independently compare `OBJ_OPENLINK`, `OBJ_DONT_REPARSE`, and file `FILE_OPEN_REPARSE_POINT` for object namespace links versus filesystem junctions. Version and filesystem must be recorded; unresolved combinations stay OPEN.
- On Linux, construct malicious host symlink/magic-link paths and directory rename races; verify that the proposed guest resolver cannot escape an `openat2`-confined root. Record feature availability and exact errno; no security guarantee is claimed.
- Reject attempts to use guest namespace entries as host paths before rights and confinement are checked; add concurrent lookup/close and link-retargeting tests.

## Decision gate

**OPEN:** Whether the first runtime implements a dedicated guest Object Manager namespace broker or a smaller in-process resolver. Choose only after Windows-reference link/rights tests, process/session namespace isolation requirements, and host filesystem race tests. Neither a Linux symlink nor an `openat2` flag alone satisfies the NT object namespace contract.

## Primary sources (reviewed 2026-11-17)

- [S1] Microsoft, `OBJECT_ATTRIBUTES` (WDK): https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wudfwdm/ns-wudfwdm-_object_attributes
- [S2] Microsoft, `NtOpenSymbolicLinkObject`: https://learn.microsoft.com/en-us/windows/win32/devnotes/ntopensymboliclinkobject
- [S3] Microsoft, `NtQuerySymbolicLinkObject`: https://learn.microsoft.com/en-us/windows/win32/devnotes/ntquerysymboliclinkobject
- [S4] Linux man-pages, `openat2(2)`: https://man7.org/linux/man-pages/man2/openat2.2.html
- [S5] Microsoft, `NtCreateFile`: https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile
