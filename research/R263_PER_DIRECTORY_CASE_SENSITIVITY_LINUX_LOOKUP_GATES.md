# R263 — Per-directory Windows case sensitivity versus Linux pathname lookup

Date: 2026-11-23. Status: **documentation-derived findings and proposed tests only**. No Windows-reference run, Linux backend test, implementation, or benchmark was performed.

## Scope and nonduplication

R50 covers rename replacement, R60 reparse/openat2, R61 file identity, R230 DOS-device namespaces, and R261 volume aliases. This note isolates **per-directory filename case matching**, its mutation/inheritance, and its interaction with a guest namespace. It does not assert that the project has selected a filesystem implementation.

## Verified primary-source findings

1. Microsoft's WSL case-sensitivity documentation says ordinary Windows filesystem lookup is generally case-insensitive, whereas Linux lookup is generally case-sensitive. Windows supports per-directory case-sensitivity flags; the feature appeared in Windows 10 build 17107. Thus neither a global lowercasing pass nor unmodified Linux lookup represents all guest directory states. [MS-Windows-WSL-case]
2. The same Microsoft document describes `fsutil.exe file queryCaseSensitiveInfo` and `setCaseSensitiveInfo ... enable|disable`, the NTFS restriction, permissions required for changes, and an empty-directory restriction for the documented operation. It also describes inheritance for newly created directories and an exception in WSL 1. These are **documented interface-specific behaviors**, not proof that every NT native API, filesystem, or Windows build has identical restrictions. [MS-Windows-WSL-case]
3. Microsoft documents the `system.wsl_case_sensitive` attribute for WSL integration and warns that applications assuming insensitive lookup can fail in sensitive directories. It warns that changing a directory flag while WSL retains references can yield stale effective behavior. This is not evidence that a native Linux xattr of the same name is a portable implementation of Windows semantics. [MS-Windows-WSL-case]
4. Microsoft's WSL release notes distinguish DrvFs `case=off`, `case=dir`, and `case=force` policies and explicitly tie them to WSL's mount behavior. These modes are useful comparison cases but must not be assumed to be the NT filesystem contract for arbitrary applications. [MS-WSL-release]

## Architectural inference (not experimentally verified)

A Linux-backed NT namespace probably needs a **guest directory metadata authority** for case policy, and a resolver that chooses matching rules at each directory component rather than applying one rule to the entire path. Preserve original spelling and stable object identity separately from case-folded lookup keys. If a Linux directory contains two names that compare equal under the guest's selected insensitive rule, do not silently choose the first `readdir` result: define a deterministic conflict/error policy and validate it against a Windows reference.

A simple scan-and-compare followed by `openat` is subject to replacement and rename races. A defensible candidate design combines directory-relative handles, authoritative metadata, collision checks on create/rename, and identity validation after lookup; whether it can be made atomic on each supported host filesystem remains an **open design question**. Avoid assuming that ASCII-only folding, Unicode normalization, locale-dependent lowercasing, or host ext4 casefold matches the NTFS/Windows matching table. The exact comparison tables and their versioning require independent primary-source research and Windows differential tests.

The case-sensitivity state is not a permission substitute: enforce guest ACL and share rules independently, and ensure a guest path cannot escape the authorized root by racing case-variant entries or changing parent directories. Metadata changes should invalidate resolver caches; an open handle should continue to identify the object it opened even if names or directory policy change, subject to Windows-reference results.

## Unrun conformance gates

| Gate | Windows-reference observation required | Linux candidate observation required |
| --- | --- | --- |
| C1 | In default and flagged NTFS directories, open `Alpha` as `alpha`; record status and returned file identity | Compare both resolver modes; never infer correctness from Linux default alone |
| C2 | Create `Alpha` and `alpha` in each mode; capture exact errors and resulting enumeration | Verify collision rejection versus distinct entries without choosing an arbitrary alias |
| C3 | Create a nested directory under sensitive and insensitive parents using Win32 and, separately, native APIs; pin OS build | Verify per-component policy and inherited metadata; do not generalize WSL1 exceptions |
| C4 | Query/change directory flag with empty/nonempty directories, live handles and varying access rights | Check atomicity, cache invalidation, rollback and authority; capture exact statuses |
| C5 | Rename a file with case-only change; replace target while another handle is open | Verify no stale alias, identity confusion or host TOCTOU escape; coordinate with R50/R61 |
| C6 | Exercise non-ASCII pairs, normalization variants, and names differing only by Unicode case mappings | Record Windows build/filesystem behavior before choosing a comparison table |
| C7 | Race creates, renames, directory flag changes and opens across threads/processes | Reject ambiguous results and ensure root confinement and stable guest identity |
| C8 | Compare NTFS with a network share and another supported Windows filesystem | Identify filesystem-dependent capability/error differences instead of assuming NTFS rules universally |

All gates are **proposals**. No pass/fail values have been measured.

## Decision impact and unresolved questions

Do not select “direct host path lookup” as a universal implementation without first proving C1–C8. A guest namespace service with explicit per-directory case policy is a candidate, not a selected architecture. Prioritize exact Windows comparison-table semantics, the native FSCTL/query/set contracts, case-only rename rules, and network filesystem capability differences in a subsequent source audit.

## Primary sources (consulted 2026-11-23)

- [MS-Windows-WSL-case] Microsoft Learn, *Case Sensitivity*: https://learn.microsoft.com/en-us/windows/wsl/case-sensitivity
- [MS-WSL-release] Microsoft Learn, *Release Notes for WSL* (DrvFs case policies): https://learn.microsoft.com/en-us/windows/wsl/release-notes

**Evidence boundary:** Microsoft WSL guidance is primary evidence for the specifically described Windows/WSL behavior, but is not a substitute for version-pinned NTFS driver/WDK specifications or independent Windows-native experiments. No claim of an NT-wide invariant is made.
