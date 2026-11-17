# R140 — Windows per-file-handle I/O priority hints versus Linux task I/O priority

Date: 2026-11-17. Status: **DOCUMENTATION REVIEW ONLY**. No Windows reference runs, Linux scheduler tests, or performance measurements were performed.

## Primary-source observations (VERIFIED as documented)

1. Microsoft documents `FILE_IO_PRIORITY_HINT_INFO` as a `PRIORITY_HINT` carried by `SetFileInformationByHandle`, associated with I/O **on a file-handle basis**. The documented Win32 enum has `IoPriorityHintVeryLow=0`, `IoPriorityHintLow=1`, `IoPriorityHintNormal=2`; `MaximumIoPriorityHintType` is a validation sentinel. Microsoft explicitly cautions that honoring a hint depends on underlying drivers. The structure requires 8-byte alignment. [Microsoft: FILE_IO_PRIORITY_HINT_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_io_priority_hint_info); [Microsoft: PRIORITY_HINT](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ne-winbase-priority_hint).
2. Microsoft's kernel-side `NtSetInformationFile` documentation identifies `FileIoPriorityHintInformation (43)` as changing the default IRP priority hint **for the file handle**, supplied in an 8-byte-aligned `FILE_IO_PRIORITY_HINT_INFORMATION`. This supports a guest handle-scoped contract, but does not establish a guarantee that storage will reorder I/O. [Microsoft: NtSetInformationFile](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/content/ntifs/nf-ntifs-ntsetinformationfile).
3. Linux `ioprio_set(2)` sets I/O scheduling class/priority for selected **threads/processes**, not a Windows-like guest file handle. The documented classes include real-time, best-effort and idle; effect depends on scheduler and I/O type. The manual specifically excludes asynchronous writes issued outside the dirtying task context from program-specific I/O-priority guarantees. [Linux man-pages: ioprio_set(2)](https://man7.org/linux/man-pages/man2/ioprio_set.2.html).

## Architectural mismatch (DERIVED, not measured)

A naïve translation of a guest file-handle hint to `ioprio_set` on the host worker thread would also change priority of unrelated I/O submitted by that worker. Conversely, one host task can submit I/O for multiple guest handles with different hints. Thus Linux task-level I/O priority is **not an identity-preserving replacement** for the guest handle-level contract. Linux scheduling policy also cannot be assumed to honor the hint on all backends. This is a semantic scope mismatch, not proof of a performance regression.

## Candidate approach (HYPOTHESIS)

Maintain a priority-hint field in the NT file-handle/open-instance model, validating supported guest enum values and alignment at the API boundary. On each submission, snapshot the relevant guest hint alongside the request, without mutating shared host worker priority globally. Initially preserve observable setter status and handle isolation while treating backend prioritization as explicitly best-effort. Investigate per-request host facilities or separate priority-isolated submission queues only after confirming actual supported kernel/filesystem/block paths. Do **not** assert a stable one-to-one mapping between the three Windows hints and Linux scheduling classes.

## Falsifiable conformance gates (OPEN; not executed)

- On a Windows reference VM, create two independently opened handles to the same file; assign different hints; issue reads through both; determine whether setting one handle affects the other, and record return statuses on each tested Windows build.
- Duplicate a handle and repeat the setter test; distinguish duplicated handle behavior from a second independent open. Do not assume the ownership boundary without measurement.
- Test invalid enum, misaligned structure, invalid handle, unsupported filesystem/device, and concurrent close/set races; capture exact Win32 and NTSTATUS results where available.
- On Linux, run a controlled I/O contention test with documented kernel, scheduler, filesystem, storage and submission mode; compare task-level `ioprio_set` to any per-request candidate while tracking unrelated guest handles.
- Include buffered reads, direct reads, synchronous writes and asynchronous buffered writes; do not infer that identical priority behavior is guaranteed.
- Verify worker reuse and concurrent guest handles do not leak priority between requests. Treat privilege failures and unsupported backend features as capability outcomes, not successful prioritization.

## Integration and decision gate

Related work: [R43](R43_ASYNC_IO_CANCELLATION_COMPLETION.md), [R100](R100_FILE_POINTER_EXPLICIT_OFFSET_GATES.md), [R137](R137_IO_URING_CANCEL_TARGET_CQE_LIFETIME.md). These cover completion and offset/lifetime questions, not this handle-vs-task priority identity mismatch.

**OPEN:** whether a specific Linux per-request I/O facility can preserve a meaningful priority ordering for LinuxNTAlpha's chosen backend, and which Windows-visible outcomes must be reproduced beyond accepted setter status. Do not select a kernel extension or claim equivalence until Windows-reference and Linux backend experiments exist.
