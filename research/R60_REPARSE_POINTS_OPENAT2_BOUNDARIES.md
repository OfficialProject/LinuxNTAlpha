# R60 — Reparse points, NT path traversal and Linux openat2 boundaries

Date: 2026-11-13. Status: **primary-documentation review only; no code executed or Windows/Linux differential test performed**.

## Scope and relationship to prior research

R10 discusses object-manager/filesystem separation; R32 addresses DLL search; R49/R50 cover deletion and rename; R56/R58 cover enumeration and notifications. This note isolates a different issue: **a Windows reparse point is not equivalent to a Linux symbolic link**, and host path traversal must not accidentally grant access outside the guest's permitted filesystem namespace.

## Verified from primary documentation

1. Microsoft documents file-system reparse-point operations through `DeviceIoControl`: `FSCTL_SET_REPARSE_POINT`, `FSCTL_GET_REPARSE_POINT`, and `FSCTL_DELETE_REPARSE_POINT`. `FILE_ATTRIBUTE_REPARSE_POINT` indicates a reparse point; modification/deletion require matching tags and, where applicable, GUIDs. [Microsoft: Reparse Point Operations](https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-point-operations) (reviewed 2026-11-13).
2. Microsoft documents `FILE_FLAG_OPEN_REPARSE_POINT` for opening a reparse point itself rather than simply treating every open as ordinary target traversal. Microsoft also documents `OpenFileById` behavior depending on this flag. [Microsoft: Reparse Points and File Operations](https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-points-and-file-operations) (reviewed 2026-11-13). **Do not infer** that this flag disables traversal of every intermediate component.
3. Microsoft `FSCTL_GET_REPARSE_POINT` retrieves reparse data by handle. The kernel-facing documentation distinguishes buffer-too-small, buffer-overflow, invalid-data and not-a-reparse-point statuses; these are not a single generic failure. [Microsoft: FSCTL_GET_REPARSE_POINT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/fsctl-get-reparse-point) (reviewed 2026-11-13).
4. Linux `openat2(2)` exposes per-open resolution controls: `RESOLVE_BENEATH` restricts traversal beneath a directory fd, `RESOLVE_IN_ROOT` treats that directory as a temporary resolution root, `RESOLVE_NO_MAGICLINKS` rejects magic links, and `RESOLVE_NO_SYMLINKS` rejects symlink traversal in **all** path components. `O_NOFOLLOW` alone only affects the final component. [Linux openat2(2) manual](https://man7.org/linux/man-pages/man2/openat2.2.html) (reviewed 2026-11-13).
5. The same Linux manual documents `EAGAIN` when a confinement check cannot be assured due to races, `EXDEV` on detected escape, and `ELOOP` for prohibited symlink/magic-link resolution. `RESOLVE_BENEATH` currently also blocks magic links, but the manual warns this could change; specify `RESOLVE_NO_MAGICLINKS` explicitly. [Linux openat2(2)](https://man7.org/linux/man-pages/man2/openat2.2.html).

These are **API contracts**, not observed behavior of this repository or proof of Windows NT compatibility.

## Architectural deductions — hypotheses, not established implementation

- **Separate namespaces.** Guest NT object paths, DOS-device aliases, volume mappings, Windows reparse tags and Linux host pathnames need distinct representations. Passing a guest path directly to `open()` or assuming all reparse data are Linux symlinks would collapse meaningful Windows semantics.
- **Confinement must survive traversal.** A candidate backend may open a trusted guest-volume root directory fd and use `openat2` with a deliberate resolution policy. This can help contain *host* traversal, but it does **not** implement arbitrary Windows reparse-point interpretation, ACLs, Windows share modes or NT object-manager lookup.
- **Two policies, not one flag.** Security policy may forbid host symlink/magic-link traversal even when Windows-compatible guest reparse processing is required. The guest resolver can interpret allowed reparse records and then submit an explicitly bounded host lookup. The exact representation of Windows reparse data is undecided.
- **Do not claim a sandbox.** Path-constrained opening alone does not isolate already-open descriptors, mount operations, other syscalls, process credentials, device nodes or race-prone operations outside the resolver.
- **Treat unsupported reparse tags explicitly.** An unknown tag must not silently become a host symlink or an ordinary file; the exact Windows status and fallback behavior require Windows-reference tests.

## Proposed differential conformance matrix — NOT RUN

On a version-recorded Windows test host with a disposable directory tree, test ordinary file, file symlink, directory symlink, junction/mount-point and unsupported-tag cases where setup is legally/technically permitted. For each, record:

1. Open target normally versus `FILE_FLAG_OPEN_REPARSE_POINT`; compare handle target, attributes and file identity. Test final versus intermediate reparse components separately.
2. Query reparse data with adequate, undersized and malformed buffers; record returned byte counts, native status (where directly tested) and Win32 error independently.
3. Test relative versus absolute targets, cycles, missing targets and links crossing a guest-volume boundary. Record exact Windows build, privileges and filesystem.
4. Test open-by-ID behavior, rename/delete of link versus target, and interaction with already-open handles; cross-reference R49/R50 rather than duplicate their general sharing tests.
5. On Linux, construct an isolated temporary tree with symlinks, `..` traversal and (if present) magic links; exercise `openat2` combinations under unprivileged credentials. Record kernel release, filesystem, errno, and distinguish expected policy denial from unavailable syscall support. Do **not** assume Linux errno equals Windows NTSTATUS.

## Acceptance gates

- Guest resolver must preserve reparse tag/type and return differentiated errors; unsupported tags remain explicit.
- Host path resolution must be confined by a documented, testable policy, including intermediate components, magic links and concurrent renames.
- Avoid string-prefix checks, pre-open `realpath` authorization, or follow-then-check as a sole defense; these do not establish atomic confinement.
- No Windows compatibility, security guarantee or performance claim without Windows oracle tests and Linux adversarial negative controls.

## Open questions

- Which reparse tags are required by the first PE application milestone (symlink, mount point, application execution alias, cloud placeholders, others)? Determine from actual app traces, not speculation.
- How should guest volume identities, per-drive current directories and NT object links map to host directory descriptors while preserving R10 security semantics?
- What should be the precise policy for Linux symlinks encountered inside guest volumes: expose as Windows reparse objects, reject, or normalize under a trusted filesystem adapter?
- Can the resolver preserve Windows case-insensitive lookup without unsafe lookup-then-open races? Needs a separately measured prototype.

**Evidence boundary:** No new experiment, conformance result, kernel source audit or performance measurement is claimed here. The Linux manual and Microsoft API pages are living documents, not pinned kernel/Windows builds.
