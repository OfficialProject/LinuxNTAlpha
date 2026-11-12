# R10 — Windows object handles, file sharing and pathname confinement

Date: 2026-11-12. Research classification: documented semantics and design hypotheses. No benchmarks or runtime tests.

## Primary sources
- Microsoft Object Manager: https://learn.microsoft.com/en-us/windows/win32/sysinfo/object-manager
- Microsoft handles and objects: https://learn.microsoft.com/en-us/windows/win32/sysinfo/about-handles-and-objects
- Windows kernel object manager: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/windows-kernel-mode-object-manager
- NT namespaces: https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
- Session/global named objects: https://learn.microsoft.com/en-us/windows/win32/termserv/kernel-object-namespaces
- CreateFileW sharing: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
- Windows file streams: https://learn.microsoft.com/en-us/windows/win32/fileio/file-streams
- Linux openat2: https://man7.org/linux/man-pages/man2/openat2.2.html

## VERIFIED — Windows object manager is not just a file-descriptor table
Windows Object Manager creates, names, authorizes, duplicates and closes object handles, tracks quotas and object lifetimes, and exposes typed objects including processes, threads, files, sections, events, registry keys and access tokens. An object can have a security descriptor; opening a handle involves access checks. The namespace contains symbolic links and device paths; sessions have separate named-object namespaces, with Global\\ conventions for certain named objects.

### Design implication (HYPOTHESIS)
Model handles as process-scoped opaque identifiers referencing typed shared objects with granted-access masks, reference counts, namespace scope and security attributes. Linux fds can be backing resources but cannot alone reproduce every NT object and handle contract. Separate handle duplication from object duplication and model cross-process inheritance explicitly.

## VERIFIED — Windows file sharing has symmetric access constraints
CreateFileW uses desired access and FILE_SHARE_READ/WRITE/DELETE. Existing opens constrain a new open, and a new share mode must also permit existing granted access. FILE_SHARE_DELETE affects rename and delete, including delete-on-close behavior. Windows stream documentation notes share modes are maintained per stream, while delete checks consider all streams.

### Design implication (HYPOTHESIS)
A userspace file-broker/open-state registry may be required to enforce Windows sharing across all NT processes. Linux flock/advisory locks alone do not implement these semantics, and a registry keyed only by pathname can fail for rename, hard links and file replacement. Need robust identity, stream scope and cross-process synchronization.

## VERIFIED — Linux openat2 supports confinement primitives
openat2 RESOLVE_BENEATH and RESOLVE_IN_ROOT can constrain pathname traversal relative to a directory fd; RESOLVE_NO_SYMLINKS and RESOLVE_NO_MAGICLINKS add controls. EAGAIN can require retry if safety cannot be established under concurrent rename activity.

### Design implication (HYPOTHESIS)
Treat Windows path normalization/namespace resolution and Linux confined filesystem access as two separate phases. Avoid naive concatenation of untrusted paths with a host directory. Evaluate case folding, DOS devices, drive letters, reparse points, reserved names and path aliases before promising file compatibility.

## Proposed test matrix
1. Handles: close, duplicate, inherit, cross-process reference, invalid handle, access-mask enforcement, named object collisions, session isolation.
2. Files: all read/write/delete desired-access and share-mode pairs in both open orders; delete-on-close; rename while open; hard links; multi-stream opens.
3. Paths: \\??\\ and \\Device\\ mappings; case variants; symlink escapes; ../; nested reparse-like indirections; concurrent rename races; Unicode edge cases.
4. Security: attempted path escape, forged handle, stale handle reuse, unauthorized duplicate, denial-of-service through handle growth.

## Architecture gate
Do not claim that NT objects can be represented directly by Linux fds, or that Linux pathname operations match Windows, until conformance tests pass. Prioritize correctness of access and share modes before optimizing the broker.
