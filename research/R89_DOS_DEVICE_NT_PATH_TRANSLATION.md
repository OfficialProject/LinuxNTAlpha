# R89 — DOS device namespaces, NT path translation and Linux path-resolution gates

Status: **DOCUMENTATION REVIEW ONLY** (2026-11-15 UTC). No Windows reference tests, Linux experiments, or runtime implementation were performed.

## Why this is a distinct gate

R60 covers filesystem reparse points and Linux `openat2` confinement; R63 covers directory case policy; R34 covers drive-relative process state. This note isolates the *earlier* boundary between Win32 DOS-style paths, NT object-manager names, per-logon DOS device mappings and a host filesystem open. A string rewrite from `C:\\...` to a Linux pathname is not sufficient.

## Verified primary-source contracts

1. Microsoft documents separate Win32 path classes: drive-rooted `C:\\dir`, drive-relative `C:dir`, UNC `\\\\server\\share`, rooted-on-current-drive `\\dir`, and device/extended-length prefixes. Drive-relative paths depend on per-drive current-directory state; they are **not** synonymous with drive-rooted paths. [Microsoft: Naming Files, Paths, and Namespaces](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file).
2. The native `NtCreateFile` API accepts `OBJECT_ATTRIBUTES` with a `UNICODE_STRING` object name and an optional `RootDirectory` directory handle. With a root handle, the name can be relative to that directory; without it, a fully qualified name is required. Microsoft's examples include `\\Device\\Floppy1\\myfile.dat` and `\\??\\B:\\myfile.dat`. [Microsoft: NtCreateFile](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile).
3. DOS device names are mappings in a DOS device namespace, with global and local visibility distinctions. This is a namespace policy, not simply a host mount table. [Microsoft: Defining an MS-DOS Device Name](https://learn.microsoft.com/en-us/windows/win32/fileio/defining-an-ms-dos-device-name).
4. Linux `openat2` can constrain an individual host pathname resolution with `RESOLVE_IN_ROOT`, `RESOLVE_BENEATH` and `RESOLVE_NO_MAGICLINKS`; these flags **do not implement** Windows DOS device resolution, object-manager symbolic links, or Win32 path normalization. [Linux man-pages: openat2(2)](https://man7.org/linux/man-pages/man2/openat2.2.html).

## Architectural hypothesis (not selected)

A candidate user-mode guest namespace broker would (a) classify Win32 paths before normalizing, (b) maintain guest drive-relative state and per-session device mappings, (c) resolve NT object names and root-handle-relative names under guest object identity/permissions, and only then (d) delegate filesystem-backed objects to a host directory-fd-based backend with `openat2` restrictions. Device and pipe targets must be routed to guest device/IPC handlers rather than forced through Linux `open`. This is a **design hypothesis**, not evidence that the model is correct or secure.

The host backend must not assume that `RESOLVE_IN_ROOT` provides a complete sandbox: guest processes may still issue Linux syscalls directly unless a separately validated mediation boundary prevents it (see R26 and R69). A namespace broker in the same trust domain as untrusted PE code does not by itself enforce isolation.

## Proposed falsifiable tests (NOT RUN)

**Windows reference matrix:** Create distinct local/global DOS device aliases in permitted test contexts; compare `QueryDosDevice` visibility and actual open results across logon sessions. Compare `C:child`, `C:\\child`, `\\child`, `\\\\server\\share\\child`, `\\?\\C:\\child` and native `\\??\\C:\\child` using appropriately matched Win32/native calls. Test reserved device names and trailing-dot/space handling without assuming extended-length prefixes make all spellings equivalent. Exercise `NtCreateFile` with a directory `RootDirectory`, a relative name and an invalid root handle; record NTSTATUS and object identity. Use disposable volumes/aliases and explicit cleanup.

**Linux isolated backend matrix:** Open a temporary root directory fd, construct symlink and `..` escape attempts, and compare `openat2` results under `RESOLVE_BENEATH`, `RESOLVE_IN_ROOT` and explicit `RESOLVE_NO_MAGICLINKS`. Test rename races and mount boundaries separately. Record kernel version, mount topology, errno and whether the guest namespace mapping was already resolved before the host call.

**Acceptance criteria:** A guest path classification table and object-resolution trace agree with a pinned Windows reference environment on the tested cases; all host paths stay within the selected backend root for adversarial inputs; device names never silently resolve to unrelated Linux files. Unsupported semantics must fail explicitly, not be normalized away.

## Open questions

- Exact local/global DOS device namespace visibility and shadowing under different tokens, sessions and Windows versions.
- NT object-manager symbolic-link traversal, access checks, and path parsing for non-filesystem object types.
- Consistent namespace lookup under concurrent alias updates and filesystem renames.
- Whether any guest path classification can safely be done outside a trusted broker.

**Evidence limit:** The cited documents establish API contracts and Linux primitive behavior, not a working NT-on-Linux implementation. No performance, compatibility or security result is claimed.
