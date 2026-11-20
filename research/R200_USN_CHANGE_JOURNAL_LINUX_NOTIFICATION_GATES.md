# R200 — NTFS USN change-journal semantics versus Linux notification APIs

Status: **VERIFIED FROM SOURCE** for the cited API contracts; **HYPOTHESIS** for the proposed implementation; **OPEN** for conformance and performance. Research date: 2026-11-20. No experiments were run.

## Architectural question

Can a Linux-hosted NT compatibility subsystem expose Windows change-journal semantics using inotify or fanotify, without inventing persistent history or leaking host filesystem activity?

**Finding:** A direct event-for-event translation is not sufficient. Windows' USN journal exposes a volume-scoped, sequenced, queryable history and journal identity, whereas Linux notification queues can coalesce, overflow, or omit events. This is an interface-contract mismatch, not a measured performance result.

## Verified primary-source contracts

1. Microsoft documents `FSCTL_READ_USN_JOURNAL` and `FSCTL_ENUM_USN_DATA` returning a leading continuation USN and variable-length `USN_RECORD_V2`/`V3` entries. The record's `RecordLength` and `FileNameLength` must govern parsing; a filename need not be NUL terminated. The documented target filesystem is NTFS 3.0+ or ReFS. Source: [Microsoft, Walking a Buffer of Change Journal Records](https://learn.microsoft.com/en-us/windows/win32/fileio/walking-a-buffer-of-change-journal-records) (retrieved 2026-11-20).
2. Microsoft `READ_USN_JOURNAL_DATA_V0` defines `StartUsn`, `ReasonMask`, `ReturnOnlyOnClose`, `Timeout`, `BytesToWaitFor` and `UsnJournalID`. A journal stop/recreation can change the ID; a mismatched ID fails. With zero `BytesToWaitFor`, the call returns at end-of-journal with a continuation USN rather than waiting. Source: [Microsoft, READ_USN_JOURNAL_DATA_V0](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-read_usn_journal_data_v0) (retrieved 2026-11-20).
3. Linux `inotify(7)` documents bounded queues and `IN_Q_OVERFLOW`, identical-event coalescing, nonrecursive watch behavior, and rename pairing with cookies that is not guaranteed atomic. These properties preclude treating the queue as a durable replayable history. Source: [inotify(7), Linux man-pages project](https://man7.org/linux/man-pages/man7/inotify.7.html) (retrieved 2026-11-20; online manual is not pinned to a kernel release).
4. Linux `fanotify(7)` documents queue overflow, incomplete coverage of remote filesystem events, and missing mmap/msync/munmap modification notifications. Mount/filesystem monitoring changes coverage but does not turn its queue into a persistent USN journal. Source: [fanotify(7), Linux man-pages project](https://man7.org/linux/man-pages/man7/fanotify.7.html) (retrieved 2026-11-20; not a pinned kernel ABI revision).

## Hypothesis: a confined guest-volume journal

For **guest-controlled writes only**, place a durable journal in the virtual volume service, assigning a monotonically advancing guest USN at a defined metadata commit point, retaining journal generation identity, and recording guest file identity, parent identity, reason and rename transitions. This is a *design candidate*, not an assertion of NTFS-equivalent behavior.

The journal must not claim complete change capture for a host directory that is concurrently mutable outside the broker. Either (a) restrict supported journaling to an exclusively mediated guest-owned volume or (b) explicitly report unsupported journal capability on uncontrolled host mounts. Notifications may be useful for cache invalidation but cannot repair historical gaps after overflow.

## Open semantic and security gates

- **Windows reference:** create/query/delete journal; record USN continuity, ID changes, journal truncation, reason masks, close coalescing, rename old/new records, hardlinks, file IDs, volume permissions, buffer-too-small behavior and async cancellation. Record Windows build and filesystem.
- **Parser:** test truncated leading USN, zero-length records, overflow in record-length arithmetic, filename lengths and V2/V3 layout/version handling without executing untrusted input.
- **Backend:** compare journal replay after process restart and abrupt power loss; demonstrate exactly which commit boundary is durable. Do not claim durability without crash tests.
- **External mutation:** change a guest-exposed tree via an unmediated host process, mmap writes, rename and cross-mount operations; show a detectable unsupported/gap state rather than silently fabricating complete history.
- **Authorization:** deny journal operations across guest volume and security boundaries, including guessed file IDs and handles; ensure host absolute paths and unowned volume events are never exposed.
- **Performance:** separately measure journal append overhead, flush latency, storage growth and high-rate replay; no throughput claim is presently supported.

## Scope and nonduplication

This is distinct from [R58 directory-change notifications](R58_DIRECTORY_CHANGE_NOTIFICATIONS.md): a directory watcher is not a persistent, volume-scoped USN journal. It also complements [R61 file identity](R61_FILE_IDENTITY_HARDLINK_INODE_GATES.md) and [R186 registry hive durability](R186_REGISTRY_HIVE_FLUSH_DURABILITY_GATES.md), without changing any architecture decision.

**Decision gate:** Do not implement `FSCTL_READ_USN_JOURNAL` as a thin wrapper around inotify/fanotify. First define guest-volume ownership, durable event generation, journal generation/retention and Windows-reference error behavior.
