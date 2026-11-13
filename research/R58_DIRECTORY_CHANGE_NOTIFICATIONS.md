# R58 — Directory change notifications versus Linux inotify

Evidence date: 2026-11-13 UTC. **Source review only. No experiment executed.** Complements R43 (asynchronous I/O), R49–R50 (namespace mutation) and R56 (directory enumeration). This note concerns change *notification*, not enumerating current directory contents.

## VERIFIED FROM SOURCE

1. Windows `ReadDirectoryChangesW` requires a directory handle with `FILE_LIST_DIRECTORY` access; `bWatchSubtree` selects the whole subtree or just the directory's immediate children. The API does **not** report changes to the watched directory itself. Its filter distinguishes file/directory names, attributes, size, last-write, last-access, creation time and security descriptor changes. Size and last-write detection can be delayed by filesystem caching. [Microsoft ReadDirectoryChangesW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw).
2. The first Windows request allocates a buffer **associated with the directory handle**; its size remains fixed until that handle closes, and changes between requests accumulate there. On overflow, the call can return success with **zero bytes**, discarding the accumulated detail; callers should re-enumerate. Documentation also specifies `ERROR_NOTIFY_ENUM_DIR` when not all changes could be recorded. A remote directory request with a buffer larger than 64 KiB can fail with `ERROR_INVALID_PARAMETER`. [Microsoft ReadDirectoryChangesW, Remarks](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw).
3. Windows asynchronous notification can use an overlapped event, an I/O completion port, or a completion routine delivered in an alertable wait. Completion routines also cover canceled operations. The asynchronous `lpBytesReturned` parameter is undefined; obtain the byte count through the chosen completion mechanism. [Microsoft ReadDirectoryChangesW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw).
4. `ReadDirectoryChangesExW` adds an information-class selection: ordinary `FILE_NOTIFY_INFORMATION` versus `FILE_NOTIFY_EXTENDED_INFORMATION`. Therefore a backend producing only the ordinary record layout is insufficient for the extended API. [Microsoft ReadDirectoryChangesExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesexw).
5. Linux inotify watches individual filesystem objects. Directory watches cover immediate children; **recursive watching is not built in**. Newly created or moved-in subdirectories require additional watches, creating a gap in which changes may occur before watch installation. `IN_MOVED_FROM` and `IN_MOVED_TO` can share a cookie, but matching is racy and the two events are not guaranteed to be inserted atomically. [Linux man-pages inotify(7), limitations and rename](https://man7.org/linux/man-pages/man7/inotify.7.html).
6. Linux inotify can coalesce identical unread events; queue overflow generates `IN_Q_OVERFLOW`, with lost events. Inotify does not reliably capture modifications via `mmap`/`msync`/`munmap`, nor remote changes on network filesystems. It does not identify the triggering process. [Linux man-pages inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html).

## HYPOTHESIS — candidate NT-on-Linux translation, not an architecture decision

An NT-facing notification broker could own a logical watch per Windows directory handle, with an explicit subtree-watch graph, Windows filter masks, buffered records and per-request asynchronous completion state. Inotify could be **one** event source, not the semantic contract. The broker would need to serialize guest-origin filesystem changes with notification bookkeeping where possible; external host changes, mmap writes and remote filesystems remain completeness gaps. Reconciliation by re-enumeration after detected loss is necessary but does not reconstruct a precise historical event sequence.

Do **not** map `IN_Q_OVERFLOW` directly to one hardcoded Win32 error without Windows reference tests. Do **not** assume an inotify rename cookie equals Windows extended file identity, or that a Linux inotify descriptor has Windows handle-scoped buffer and cancellation semantics.

## Falsifiable reference matrix — NOT RUN

Use a pinned Windows build and filesystem (local NTFS first), logging request mode, handle identity, filter, subtree flag, record bytes, completion channel, exact error, and ordered file operations:

1. Open watched directory with required rights; verify rejection with inadequate access. Mutate immediate children, grandchildren and the directory itself, separately. Test each filter with metadata writes and flush boundaries.
2. Queue an overlapped request; create/rename/delete entries; decode all `NextEntryOffset` chains and UTF-16 names, checking alignment and returned lengths. Repeat with `ReadDirectoryChangesExW` ordinary and extended classes.
3. Test IOCP versus event versus alertable completion-routine paths; close the handle and request cancellation during pending I/O. Record whether each request completes and its terminal status; link to R43 rather than guessing from Linux `close` or `EINTR`.
4. Generate more changes than a small first-request buffer can retain. Confirm loss indicators, subsequent request behavior, and re-enumeration recovery; vary buffer size on **later** calls to check the documented handle-lifetime allocation rule.
5. Rename a directory subtree into and out of the watched tree during rapid writes. Repeat cross-directory moves and hard-link aliases. Compare Windows output with Linux inotify traces; do not require one-to-one events without evidence.
6. Independently probe a Linux inotify implementation under overflow, moved-in directory races, mmap writes and host-external mutations; record kernel/filesystem and dropped-event indicators. This is backend characterization, **not** a Windows conformance result.
7. Only after correctness gates, benchmark watcher count, memory overhead, wake latency, burst throughput and rescan cost at controlled tree depths and event rates.

## OPEN

- Exact NT `NtNotifyChangeDirectoryFile`/Ex status and cancellation behavior; this note verifies the documented Win32 surface, **not** the native syscall ABI.
- Windows behavior for duplicate directory handles, multiple concurrent requests and filter changes on one handle; require differential tests.
- Which Linux backend, if any, can cover uncooperative host, mmap and remote filesystem changes sufficiently for a declared support scope.
- Buffer-overflow and asynchronous-completion details across local NTFS, ReFS and SMB; do not generalize across filesystems.

**Status:** primary-source contracts VERIFIED FROM SOURCE; translation HYPOTHESIS; experiments OPEN. No runtime implementation or compatibility claim.
