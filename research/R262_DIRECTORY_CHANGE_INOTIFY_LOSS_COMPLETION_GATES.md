# R262 — Directory change notifications versus Linux inotify: loss, rename and completion authority

Status: RESEARCH ONLY (2026-11-23). No experiment performed. ADR-003 remains open.

## Scope and verified contracts

**VERIFIED — Windows API:** `ReadDirectoryChangesW` monitors a directory opened with `FILE_LIST_DIRECTORY`; `bWatchSubtree` selects subtree monitoring, and `dwNotifyFilter` selects change classes. A directory handle is opened with `FILE_FLAG_BACKUP_SEMANTICS`; `FILE_FLAG_OVERLAPPED` enables asynchronous calls. The first call allocates a change buffer associated with the directory handle, whose size does not change for the lifetime of that handle. On overflow, a successful call can return zero bytes and discard accumulated change information. `ERROR_NOTIFY_ENUM_DIR` is a distinct inability-to-record condition requiring enumeration. Network monitoring has a documented >64-KB buffer restriction. [Microsoft ReadDirectoryChangesW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw).

**VERIFIED — Windows record ABI:** `FILE_NOTIFY_INFORMATION` consists of `NextEntryOffset`, `Action`, `FileNameLength` in bytes and a variable-length, non-NUL-terminated UTF-16 name relative to the watched directory. Actions include add, remove, modify, old rename and new rename. The short versus long filename spelling is not guaranteed. [Microsoft FILE_NOTIFY_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-file_notify_information). The [MS-FSCC protocol specification](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/634043d7-7b39-47e9-9e26-bda64685e4c9) additionally specifies 4-byte alignment of record offsets and consecutive old/new entries for multiple renames **within that protocol response**; do not automatically extrapolate protocol-specific guarantees to every local filesystem/provider.

**VERIFIED — Linux:** `inotify` exposes per-watch descriptors and event masks; rename `IN_MOVED_FROM`/`IN_MOVED_TO` events carry a correlation cookie. `IN_Q_OVERFLOW` is an event with watch descriptor -1. Monitoring is inode-based, not recursive, and directory watch semantics and rename pairing have documented limitations. [Linux man-pages inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html).

**VERIFIED — Windows extended API:** `ReadDirectoryChangesExW` offers extended information classes and the same handle-bound buffering and alertable completion-routine distinction. [Microsoft ReadDirectoryChangesExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesexw).

## Architectural consequence (HYPOTHESIS, not an implemented design)

A guest notification manager must own the **Windows directory handle, notification request and completion state**. An inotify fd is only a host event source, not a Windows directory handle or an authoritative Windows notification buffer. Candidate design: per-guest-handle queues with filter/subtree policy; a host-watch graph that grows as subdirectories appear; an explicit loss state on `IN_Q_OVERFLOW`, watch removal or watch-graph gaps; and a guest completion adapter shared with the NT asynchronous I/O/IOCP/APC machinery. Do not fabricate a complete rename pair when a cookie partner is absent, or report loss as an empty successful change set without a version-pinned Windows reference.

Host queue overflow can affect unrelated watches sharing an inotify instance. The mapping from global host loss to affected guest handles is an **OPEN** policy problem; conservative invalidation is preferable to silently claiming completeness. Recursion and renames across watched/unwatched directories create race windows. Mapping Linux byte names to Windows UTF-16 and NT namespace aliases also needs explicit encoding and security policy; the source contracts do not establish universal lossless conversion.

## Proposed conformance gates — NOT RUN

1. **Windows-reference matrix:** Windows build, local NTFS/ReFS versus SMB where available; filter bit, subtree flag, synchronous/OVERLAPPED completion, IOCP versus completion routine, buffer size, handle close and cancellation. Record exact return values, byte counts and `GetLastError`, never assume uniform filesystem behavior.
2. **Rename stress:** old/new pairs within and across watched trees; move-in/move-out; nested directory renames; simultaneous unrelated renames; missing inotify cookie partner; distinguish Windows-observed guarantees from [MS-FSCC] protocol-specific layout.
3. **Loss and recovery:** force host inotify queue overflow and guest buffer overflow separately, then observe what Windows actually returns. Require enumeration/resynchronization rather than asserting no changes.
4. **Recursive watch race:** create a directory and immediately populate descendants before watch registration; report whether a complete event history is achievable or explicitly unsupported.
5. **Security:** verify guest access rights and per-process namespace visibility before publishing events; a shared host inotify fd must not leak other guests' paths.
6. **Lifetime:** close/cancel directory handles while host events and completion packets are pending; compare exact Windows completion and cancellation results against host behavior.
7. **Resource limits:** test `max_user_watches`, `max_queued_events`, memory accounting, network paths, and unsupported filesystems. Failure must be observable and not silently degrade into false completeness.

## Relationship to earlier research

Complements existing asynchronous I/O, IOCP, APC, object-handle, path-namespace and filesystem identity research; this note isolates *notification loss and recursive watch fidelity*, not a new general filesystem architecture. No benchmark, prototype or Windows differential run is claimed.

## Open decisions

- Does a version-pinned Windows reference expose distinguishable overflow outcomes for each completion mechanism?
- What guest-facing error or rescan signal is defensible after an inotify global queue overflow?
- Can recursive completeness be supported without a kernel event source with stronger subtree semantics?
- How do remote SMB notifications and local filesystem rename records diverge in actual Windows tests?
