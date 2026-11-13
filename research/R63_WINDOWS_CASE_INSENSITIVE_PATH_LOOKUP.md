# R63 — Case-insensitive Windows path lookup on case-sensitive Linux hosts

Status: **SOURCE-VERIFIED contract observations; architecture HYPOTHESES; no experiments**  
Reviewed: 2026-11-13. Scope: file-path component lookup, not the already-covered reparse-point traversal in R60, file identity in R61, or sharing/deletion in R49.

## Verified upstream contracts

1. Microsoft's *Naming Files, Paths, and Namespaces* says Win32 callers generally must not assume case sensitivity: `OSCAR`, `Oscar`, and `oscar` normally refer to the same name; NTFS can support case-sensitive behavior but it is not the default. Drive-letter case is likewise insignificant. The document also distinguishes DOS drive paths, UNC names and namespace prefixes, and imposes reserved-name/character rules. Source: https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
2. Microsoft's *Case Sensitivity* documents that ordinary Windows file lookup is case-insensitive whereas ordinary Linux lookup is case-sensitive. Windows supports per-directory case-sensitive flags (Windows 10 build 17107 onward); the documented WSL integration exposes `system.wsl_case_sensitive` on supported Windows filesystem paths. Directory creation/inheritance has version-specific caveats, including WSL 1. Source: https://learn.microsoft.com/en-us/windows/wsl/case-sensitivity
3. The Linux `openat2(2)` interface exposes resolution restrictions such as `RESOLVE_BENEATH` and `RESOLVE_IN_ROOT`; these are path-containment mechanisms, **not** Windows-compatible case-insensitive name matching. Source: https://man7.org/linux/man-pages/man2/openat2.2.html

These sources do **not** establish that Linux ext4, btrfs, overlayfs, or a specific mount configuration offers the Windows matching algorithm; do not generalize from a single filesystem.

## Architectural consequence (inference)

A direct translation of `CreateFileW("C:\\Games\\Data\\ASSET.PAK")` to Linux `openat()` may fail when the actual directory entry is `asset.pak`, even when Windows would have opened it. Lowercasing the whole path is not a valid solution: it loses the original spelling, cannot select between existing case-colliding entries, and may use a Unicode case mapping different from the target Windows/filesystem behavior. Conversely, globally forcing insensitive matching would break explicitly case-sensitive directories.

**Candidate design, not selected:** represent a Windows volume namespace separately from host paths; resolve one component at a time under a retained directory handle, applying a directory-specific comparison policy, then open the selected entry by a race-resistant handle-relative operation. Preserve original names and keep an explicit collision policy. Treat Unicode upcase tables, normalization, 8.3 aliases, DOS device names, and trailing-dot/space handling as **separate unresolved compatibility requirements**. `openat2` containment can be one defense, but it cannot alone prevent lookup/open time-of-check/time-of-use races when a user-space resolver enumerates names first.

## Priority conformance matrix (proposed, NOT run)

On a real Windows reference VM and a Linux test filesystem, record exact status, returned canonical name and identity, and directory settings for:

- `asset.pak` opened as `ASSET.PAK` in a default directory.
- Case-sensitive directory containing both `a` and `A`; test exact and mixed-case opens.
- Nested directory traversal with differing case policy at different levels.
- Creation with a case-only name collision and creation-disposition variations; ensure no accidental overwrite.
- Concurrent rename/swap of a parent component during resolution; verify no escape from a confined root and no wrong-object open.
- Non-ASCII case pairs with documented Windows filesystem/version and code-point recording; **do not assume** generic Unicode casefold matches NTFS.
- DOS drive-letter casing, UNC syntax and reserved device names at the Win32-to-NT namespace boundary.

Run separately on each supported filesystem/mount configuration and record Windows build, filesystem, volume settings, Linux kernel, mount options and process privileges. Test a real Windows NT-path API separately from Win32 normalization; they need not have identical preprocessing.

## Decision gate

Do not choose a host-filesystem or implement a universal casefolding layer until reference tests define matching and collision behavior. A viable solution must preserve security boundaries during rename races and interoperate with R49/R50 sharing and rename rules. This note is research only: **no Windows conformance test, Linux runtime test, performance measurement, or implementation was performed.**
