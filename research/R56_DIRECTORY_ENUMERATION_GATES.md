# R56 — NT directory enumeration: query state, restart and Linux getdents boundaries

Date: 2026-11-13. Classification: **primary-documentation analysis only**. No Windows differential test, Linux prototype, or performance measurement was executed for this note.

## Scope and relation to earlier work

R10 covers object handles, sharing and path confinement; R49/R50 cover deletion and rename. This note isolates **directory enumeration**: the per-open search cursor, filter semantics, information classes, output-buffer behavior and mutation races. It is not a claim that Linux directory entry APIs reproduce NT behavior.

## Primary references

1. Microsoft, **ZwQueryDirectoryFile** (documented kernel interface; see remarks for first-call search expression and restart): https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwquerydirectoryfile
2. Microsoft, **FILE_BOTH_DIR_INFORMATION** (entry layout and NextEntryOffset): https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_both_dir_information
3. Microsoft, **FindFirstFileW**: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew
4. Microsoft, **FindNextFileW**: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findnextfilew
5. Microsoft, **FindFirstFileExW** (information levels/search options): https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfileexw
6. Linux man-pages, **getdents(2)** (Linux-specific directory records and d_off semantics): https://man7.org/linux/man-pages/man2/getdents.2.html
7. Linux man-pages, **readdir(3)** (directory stream and nonportable ordering/offset assumptions): https://man7.org/linux/man-pages/man3/readdir.3.html

Source status: these are canonical upstream documentation URLs; this run did not retrieve and pin page revisions. Treat details beyond the cited API contracts as open until independently checked against a Windows reference.

## VERIFIED — API contract distinctions from primary documentation

- ZwQueryDirectoryFile is issued against a **directory file handle**, takes a FileInformationClass, an output buffer and length, optional FileName search expression, ReturnSingleEntry and RestartScan controls. Its documented behavior maintains query state across calls; a new search expression supplied after the first query is not a general-purpose way to replace the original expression on the same enumeration.
- NT directory information is not a POSIX `struct dirent` array. For example, FILE_BOTH_DIR_INFORMATION contains a variable-length FileName and an offset to the next record, together with Windows-specific metadata. Different information classes imply different structures and fields.
- FindFirstFileW/FindNextFileW expose a distinct Win32 enumeration contract; Microsoft warns that results are **not sorted**. Callers should not rely on alphabetical order. Win32 wildcard matching and DOS path handling must not be assumed equivalent to libc glob or Linux filesystem lookup.
- Linux getdents64/readdir provide directory records and iteration state, but not the NT information-class structures, Win32 attributes, short names or NT status/error contract. The `d_off` field is an opaque directory position, not a portable byte offset or stable file identifier.

## HYPOTHESIS — architecture boundary

A candidate NT directory-object layer should own a per-open enumeration context (search expression, initialized flag, restart state, information class conversion and cursor). A Linux directory fd or DIR* may be a backing resource, but **must not be exposed as the NT enumeration state model**. Separate (a) NT path and wildcard normalization, (b) host directory scanning, (c) NT metadata acquisition, (d) record packing and (e) status translation.

This is a design hypothesis, not a proven need for a dedicated broker or a selected implementation. R10's shared namespace/security decisions may affect the design.

## OPEN — differential behavior that documentation alone does not settle

1. First call with a tiny output buffer: exact returned status, bytes written, and whether a partial first record is permitted for each information class.
2. RestartScan after entries were removed/renamed; whether a reopened directory handle differs from a restarted scan on the original handle.
3. Case-insensitive search, DOS wildcard edge cases, short-name matching, and filesystem-specific behavior.
4. Reparse points, junctions, alternate data streams, remote filesystem entries and synthetic NT attributes.
5. Directory mutation during enumeration: duplicate, omitted and reordered entries; do not promise snapshot consistency.
6. Error translation and end-of-enumeration distinctions (for example, initial no-match versus later exhaustion) across the native NT and Win32 wrappers.
7. Concurrent calls on one directory handle, cancellation and asynchronous completion.

## Proposed conformance gates (NOT EXECUTED)

On a **licensed Windows reference**, create an isolated directory with ASCII and Unicode names, mixed case, long names, subdirectories, hidden files and controlled reparse points. Record OS build, filesystem, API path, search expression, information class, buffer size, flags, status, bytes returned and returned entry metadata. Vary buffer sizes around record boundaries, restart flags, ReturnSingleEntry and interleaved mutation. Run native ZwQueryDirectoryFile probes separately from Win32 FindFirstFileW/FindNextFileW tests; do not conflate their errors or contracts.

On Linux, first implement **parse-only** packing/validation tests for NT record alignment, NextEntryOffset chains, overflow-safe filename lengths and zero-progress handling. Only then compare a candidate enumeration backend with the Windows traces. Use independent directories for each test; never treat Linux getdents ordering as a Windows golden result.

## Decision gate

**Do not** implement NT directory enumeration as a thin getdents64 passthrough, claim stable ordering, or use a path string as a substitute for a live directory handle. No performance or compatibility claim is supported by this note.
