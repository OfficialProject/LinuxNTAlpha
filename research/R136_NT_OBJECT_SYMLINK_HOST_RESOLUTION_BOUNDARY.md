# R136 — NT object-symbolic-link traversal is not Linux filesystem symlink traversal

Reviewed: 2026-11-21. **Documentation-only source review; no Windows reference experiment or Linux test executed.** Complements R10 (object manager), R60 (filesystem reparse/openat2), R89 (DOS path translation), R92 (access checks) and R135 (oplocks). This note focuses on the **object namespace traversal boundary**, not filesystem path lookup.

## VERIFIED FROM PRIMARY DOCUMENTATION

1. Microsoft's `OBJECT_ATTRIBUTES` contract permits a fully qualified object name when `RootDirectory == NULL`, or a relative name when `RootDirectory` names an object-manager directory **or filesystem directory**. These two root-handle classes must not be silently conflated. `OBJ_CASE_INSENSITIVE` controls name matching, while `OBJ_OPENLINK` requests opening an object symbolic-link object itself rather than following its target. [M1]
2. `NtOpenSymbolicLinkObject` opens a symbolic-link **object**; it is a distinct native object operation, not a POSIX `readlink` operation on an inode. Microsoft's user-mode devnote cautions that this API may change or become unavailable. [M2] The corresponding kernel documentation describes closing the returned handle with `ZwClose`. [M3]
3. `OBJ_DONT_REPARSE` is documented on the general `OBJECT_ATTRIBUTES` page as prohibiting reparsing during object-name parsing, with `STATUS_REPARSE_POINT_ENCOUNTERED` on encounter. However, Microsoft's user-mode `NtCreateFile` parameter description lists `Attributes` as zero or `OBJ_CASE_INSENSITIVE`; the published contracts therefore do **not** establish all flag/function combinations. Do not assert that `OBJ_DONT_REPARSE` always applies identically to NT object symbolic links and filesystem reparse points. [M1, M4]
4. Linux `openat2` resolves paths against Linux directory file descriptors. `RESOLVE_BENEATH` forbids escapes above the supplied directory, `RESOLVE_IN_ROOT` temporarily treats that directory as root, `RESOLVE_NO_MAGICLINKS` forbids magic-link resolution, and `RESOLVE_NO_SYMLINKS` forbids symlink resolution. The documentation explicitly warns that the current incidental magic-link restriction of BENEATH/IN_ROOT might change; specify NO_MAGICLINKS explicitly when required. These flags protect **host filesystem resolution**, not the NT object namespace. [L1]

## HYPOTHESIS — minimal boundary design, not an architecture decision

Represent object-manager directories and symbolic links as typed guest objects with their own handles, names, access checks, reference counts and resolution rules. Resolve guest `\\??\\` and other object-manager links *before* deciding whether a resulting target should be handed to a filesystem backend. A guest object-link target is not automatically a host symlink path; opening a link object and opening its target are distinct operations. Keep a trusted mapping between guest filesystem volumes and preopened host directory descriptors; only then perform host path lookup using `openat2` confinement as defense in depth.

**Security invariant to test:** guest-supplied object names, symbolic-link targets, relative roots and case-insensitive lookups must not let an untrusted guest select arbitrary host pathnames or escape a designated host root. `openat2` is not an NT security descriptor or object-access-check engine; it cannot substitute for R92.

## OPEN: targeted falsification matrix (NOT EXECUTED)

- On a pinned Windows build, create a permitted private object directory and symbolic-link object; separately open the link with `NtOpenSymbolicLinkObject` and query its target, then compare ordinary object-open traversal with `OBJ_OPENLINK` and `OBJ_DONT_REPARSE`. Record privileges, security descriptors, exact status and target object type. Avoid system namespace mutations.
- Repeat relative-name lookup with an object-directory root and a filesystem-directory root; do not assume identical behavior or identical error precedence.
- Test dangling targets, cycles, multiple-link chains, case-only collisions, access-denied intermediate directories, link replacement during lookup and handle-close races. Record whether the link handle survives target deletion and whether errors identify the intermediate or final component.
- On Linux, separately exercise `openat2` with BENEATH, IN_ROOT, NO_MAGICLINKS and NO_SYMLINKS on a disposable tree with absolute symlinks, `..`, procfs magic links and mount crossings. Log kernel, mount type, flags and errno; expect failures/skips to be reported individually.
- In a proposed guest resolver, verify typed-root validation, loop limits, deterministic access-check order and that host path translation occurs only after guest namespace resolution. Run concurrency stress and negative controls before claiming correctness.

## Decision impact

Do **not** encode NT object-manager symbolic links directly as Linux symlinks or use `openat2` as the complete guest namespace resolver. Treat the boundary as an explicit design hypothesis awaiting differential Windows testing. No measured performance, compatibility or security-isolation claim follows from this note.

## Primary sources

- [M1] Microsoft, `OBJECT_ATTRIBUTES` (ntdef.h): https://learn.microsoft.com/en-us/windows/win32/api/ntdef/ns-ntdef-_object_attributes (accessed 2026-11-21; living documentation, not pinned to SDK build).
- [M2] Microsoft, `NtOpenSymbolicLinkObject`: https://learn.microsoft.com/en-us/windows/win32/devnotes/ntopensymboliclinkobject (accessed 2026-11-21).
- [M3] Microsoft, `ZwOpenSymbolicLinkObject`: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwopensymboliclinkobject (accessed 2026-11-21).
- [M4] Microsoft, `NtCreateFile`: https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile (accessed 2026-11-21; function-specific parameter contract).
- [L1] Linux man-pages project, `openat2(2)`: https://man7.org/linux/man-pages/man2/openat2.2.html (accessed 2026-11-21; living documentation, no pinned kernel source revision).

No third-party implementation code copied. No experiment was performed.
