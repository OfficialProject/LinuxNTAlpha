# R136 — NTFS USN change-journal replay versus Linux filesystem notifications

Research date: 2026-11-17. **Status: documentation-based findings; no Windows/Linux experiment performed.**

## Question and scope

Can Linux inotify/fanotify notifications substitute for the Windows NTFS/ReFS update sequence number (USN) journal exposed through volume control codes? This is distinct from R58's directory-handle change notifications: R136 concerns **volume-wide replay, cursor identity, persistent change history, and record formats**.

## Verified primary-source contracts

1. Microsoft documents two distinct control paths: `FSCTL_READ_USN_JOURNAL` reads change records with filters, while `FSCTL_ENUM_USN_DATA` enumerates records/files constrained by USN bounds. Their output starts with a next-position USN followed by zero or more variable-length `USN_RECORD_V2` or `USN_RECORD_V3` records. A filename is length-delimited and need not be NUL-terminated. [Microsoft, Walking a Buffer of Change Journal Records](https://learn.microsoft.com/en-us/windows/win32/fileio/walking-a-buffer-of-change-journal-records).
2. `MFT_ENUM_DATA_V0` uses `StartFileReferenceNumber`, `LowUsn`, and `HighUsn`; the next file-reference starting point is returned in the first output field. This is **not** equivalent to advancing a sequential change-log cursor. [Microsoft, MFT_ENUM_DATA_V0](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-mft_enum_data_v0).
3. `READ_USN_JOURNAL_DATA_V0` includes `StartUsn`, reason mask, `ReturnOnlyOnClose`, `Timeout`, `BytesToWaitFor`, and `UsnJournalID`. A deleted/recreated journal receives a new ID; reads against an old ID fail rather than silently continuing. Final-close records and timeout behavior depend on request fields and synchronous/asynchronous handle mode. [Microsoft, READ_USN_JOURNAL_DATA_V0](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-read_usn_journal_data_v0).
4. Linux inotify is a file/directory monitoring API with finite queues and `IN_Q_OVERFLOW`; it does not itself expose an NTFS-style persistent volume change-log cursor. Linux fanotify reports filesystem events via notification groups and has queue-overflow semantics; it is not a documented USN replay API. [Linux inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html), [Linux fanotify(7)](https://man7.org/linux/man-pages/man7/fanotify.7.html).

## Architectural consequence (reasoned, not measured)

A direct `inotify -> USN_RECORD` adapter cannot honestly promise replay across daemon downtime, a stable volume journal identity, or gap-free history. A guest-visible USN subsystem would need an independently specified **volume-scoped journal service** with monotonically assigned positions, persistent records, a generation/journal ID, bounded retention, explicit gap detection, and file-reference identity. Host event feeds could be one input, **not** proof of complete coverage. Files modified outside the subsystem or during a monitoring outage create an unavoidable reconciliation problem unless the underlying filesystem offers an authoritative change source.

Do **not** fabricate historical USN events after a gap; report a discontinuity and require a new inventory/reconciliation. Do not infer exact Windows failure codes or NTFS record-version support from these general contracts; pin target Windows builds and filesystems first.

## Proposed conformance gates (not run)

- On a Windows reference volume, query journal ID/range, make create/write/rename/delete/close operations, then read from a saved `StartUsn`; capture exact reason flags, record boundaries, next cursor, file references, and errors.
- Compare `FSCTL_ENUM_USN_DATA` with `FSCTL_READ_USN_JOURNAL` on the same volume; ensure the first output field is interpreted according to each request rather than conflating cursors.
- Delete/recreate a journal in a disposable test volume and verify stale-ID behavior. Repeat after journal truncation/retention turnover, including expected error codes.
- Test synchronous versus overlapped reads, `BytesToWaitFor`, `Timeout`, cancellation and empty-buffer behavior; preserve exact target-build results.
- On Linux, independently measure inotify/fanotify event coverage, queue overflow, daemon restart, rename pairing, mount changes, and out-of-band writers. Treat observed coverage as backend capability evidence, **not** NT conformance.
- Fuzz length-delimited `USN_RECORD` decoding, alignment, unsupported versions, short buffers and filename offsets before any guest exposure.

## Decision gate

**Do not claim USN journal compatibility** from R58 directory notifications or from Linux event watches. Prefer deferring journal APIs over returning plausible but incomplete history. An eventual implementation requires a clearly defined supported-volume model, gap semantics, and Windows-reference differential evidence.

## Sources inspected

- Microsoft: [Walking a Buffer of Change Journal Records](https://learn.microsoft.com/en-us/windows/win32/fileio/walking-a-buffer-of-change-journal-records)
- Microsoft: [MFT_ENUM_DATA_V0](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-mft_enum_data_v0)
- Microsoft: [READ_USN_JOURNAL_DATA_V0](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-read_usn_journal_data_v0)
- Linux man-pages: [inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html), [fanotify(7)](https://man7.org/linux/man-pages/man7/fanotify.7.html)

No implementation, performance benchmark, or reference-system experiment is asserted.
