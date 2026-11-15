# R100 — Synchronous file-pointer state versus explicit-offset I/O

Status: **primary-source documentation review only (2026-11-15)**. No Windows differential test, Linux experiment, benchmark, or implementation was run. This note addresses the *file-position ownership* gate, not the already documented R43 cancellation/IOCP gates or R49/R50 sharing/rename gates.

## Verified contracts (documentation)

1. Microsoft's **ReadFile** reference distinguishes a synchronous file handle (without `FILE_FLAG_OVERLAPPED`) from an asynchronous one. With a synchronous handle and NULL `lpOverlapped`, the operation uses the current file position and updates it before return. With a non-NULL `lpOverlapped` on a synchronous handle, the documented operation starts at its explicit offset and waits for completion. On an asynchronous handle, an `OVERLAPPED` pointer is required, and its offset is not automatically updated. A pending request may return `ERROR_IO_PENDING`. [Microsoft ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile).
2. **WriteFile** documents corresponding synchronous/asynchronous file-position rules and pending-completion behavior. [Microsoft WriteFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile).
3. **SetFilePointerEx** changes the file pointer for a seeking handle but explicitly does **not** determine offsets for overlapped reads/writes. Microsoft warns that a compound seek-then-read on a handle shared by threads needs application synchronization; `FILE_FLAG_NO_BUFFERING` adds alignment restrictions. [Microsoft SetFilePointerEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointerex).
4. Linux **pread/pwrite** take explicit offsets without changing the open file description's current offset; a successful call may transfer fewer bytes than requested. [Linux man-pages pread(2)](https://man7.org/linux/man-pages/man2/pread.2.html). These primitives do not themselves supply Windows completion events, NTSTATUS, or handle access checks.

## Architectural implication (hypothesis, not a proven implementation)

An NT-facing file-object/open-instance layer should own the guest's synchronous current-position state and distinguish it from explicitly offset requests. An explicit-offset backend such as `pread/pwrite` is a candidate for both paths, provided the compatibility layer serializes guest current-position operations where required, advances the guest position by the correct completed transfer, and never accidentally advances it for overlapped requests. **Do not** equate each Windows HANDLE with an independently opened Linux fd: duplication, inheritance, and reopening may have different position-sharing behavior, which must be established with Windows-reference probes before deciding the object model.

A plain `lseek + read` sequence is not a safe substitute for a single logical guest operation when other operations can intervene. Conversely, serializing *all* I/O through one global lock would be an unjustified performance design. The right scope and concurrency behavior remain unmeasured.

## Proposed falsifiable Windows-reference matrix (not executed)

- Open the same temporary regular file with and without `FILE_FLAG_OVERLAPPED`; for each, issue `ReadFile`/`WriteFile` with documented valid NULL or non-NULL `OVERLAPPED` combinations. Record success, `GetLastError`, bytes transferred, position from `SetFilePointerEx(FILE_CURRENT)`, and file contents.
- Compare (a) two calls on one handle, (b) `DuplicateHandle` aliases, (c) independently reopened handles, and (d) an inherited handle in a child. Do not assume their position-state identities in advance.
- On synchronous handles, test explicit `OVERLAPPED` offsets followed by NULL-offset operations. On asynchronous handles, submit concurrent disjoint explicit-offset requests and verify that completion and position are separate observations.
- Test EOF, short transfers, zero-length operations, seek past EOF, and invalid/nonseekable handle types. Record exact Windows version, filesystem, sharing/access flags, and API error/status.
- With two threads, deliberately race a seek-then-read compound sequence, then repeat with application synchronization; do not assert deterministic ordering for an unsynchronized race.
- Separately test no-buffering alignment and append-access behavior rather than extrapolating ordinary buffered-file results.

## Acceptance criteria and unresolved decisions

A future guest model passes only if observed bytes, error/status mapping, file position, offset behavior, aliasing, and completion lifetime match the recorded Windows baseline for each supported case. Keep per-request completion tracking distinct from file-position state. Determine native `NtReadFile/NtWriteFile` byte-offset sentinel and synchronous-file-object semantics from authoritative NT specifications or controlled Windows probes **before** extending this Win32-level analysis to raw NT entry points.

**Not established:** duplicate-handle pointer sharing, native NT sentinel handling, performance impact, compatibility with actual applications, or a working Windows file subsystem.
