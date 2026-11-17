# R148 — Object Manager directory enumeration versus filesystem enumeration

Date: 2026-11-17. Evidence: Microsoft primary API documentation inspected on this date. **No Windows or Linux experiments executed.** Scope: NT Object Manager *directory objects*, not filesystem directories. Complements R10 (namespace/handles), R89 (DOS devices), and R147 (symbolic-link lookup).

## VERIFIED FROM SOURCE — published API contracts

1. Microsoft documents `NtOpenDirectoryObject` as opening an existing *directory object* using `OBJECT_ATTRIBUTES` and a requested access mask. Distinct access bits include `DIRECTORY_QUERY=0x0001`, `DIRECTORY_TRAVERSE=0x0002`, `DIRECTORY_CREATE_OBJECT=0x0004`, and `DIRECTORY_CREATE_SUBDIRECTORY=0x0008`. Therefore query authorization cannot be assumed from successful name traversal alone. [S1]
2. Microsoft documents `NtQueryDirectoryObject(DirectoryHandle, Buffer, Length, ReturnSingleEntry, RestartScan, Context, ReturnLength)`. `Context` is in/out enumeration state; `RestartScan` restarts or continues the scan; `ReturnSingleEntry` limits the returned entries. The buffer contains `OBJECT_DIRECTORY_INFORMATION` records, terminated by a null record, followed by name strings. Each record contains two `UNICODE_STRING` members: `Name` and `TypeName`. [S2]
3. Microsoft documents `NtQueryDirectoryFile` separately, with a file handle, `FILE_INFORMATION_CLASS`, optional name filter, I/O status block and filesystem-dependent concurrent-mutation behavior. Its output format and enumeration semantics must not be substituted for those of `NtQueryDirectoryObject`. [S3]
4. Both Object Manager directory APIs are documented as Ntdll exports with a warning that the functions may change. Their published pages do not provide a complete version-specific table of short-buffer NTSTATUS results, mutation ordering, or concurrent-enumerator isolation. These details remain OPEN rather than inferred. [S1, S2]

## HYPOTHESIS — candidate runtime boundary

Maintain typed guest directory objects and per-handle granted access. Enumerate guest object namespace entries as (name, type-name) pairs, **not** host filesystem entries. A host `getdents64` adapter is not a valid general implementation: a guest directory can contain event, mutex, symbolic-link, section, and other named object types that have no host inode representation. If the runtime ultimately uses host storage for selected objects, the guest enumeration contract still belongs above that storage adapter.

Represent `Context` as an opaque guest-visible continuation value with broker-owned enumeration state. Do not assume a stable snapshot, monotonic ordering, or a direct index encoding until reference tests establish the behavior. Validate the caller's output length and pointer range, build records with correctly bounded UTF-16 strings and termination, and only publish continuation state after a defined result outcome. These are proposed implementation safeguards, not demonstrated Windows behavior.

## OPEN — discriminating Windows-reference tests (NOT RUN)

- On pinned Windows builds, create a permitted private Object Manager directory; open with `DIRECTORY_QUERY`, `DIRECTORY_TRAVERSE`, both, and neither. Record exact access outcomes and NTSTATUS; distinguish directory opening from querying.
- Populate entries with at least two object types and Unicode names. Verify `Name`, `TypeName`, buffer layout/alignment, terminator, `ReturnLength`, and zero/short/exact-fit buffers. Record whether partial entries or continuation changes occur on failure.
- Compare `ReturnSingleEntry` true/false, `RestartScan` true/false, zero and nonzero `Context`, and two independent handles to the same directory. Do not presume ordering or state scope.
- Mutate namespace membership between calls and concurrently with enumeration. Record duplicate/missing entries and behavior when a named object is unlinked but remains referenced by an open handle.
- Try invalid, stale, wrong-type and insufficient-rights handles, and invalid user buffers. Record statuses without translating them prematurely to Win32 errors.
- Security: test a directory hidden by access control and a cross-session name. Do not leak inaccessible names merely because host storage can enumerate them.

## Architectural decision gate

**OPEN:** Determine whether a dedicated namespace broker or process-local namespace authority is needed to provide consistent directory handles and continuation semantics across guest processes. Do not select a Linux filesystem enumeration mechanism as the NT namespace implementation before Windows reference conformance tests. This note does not establish performance superiority or complete Object Manager support.

## Primary sources (inspected 2026-11-17)

- [S1] Microsoft, `NtOpenDirectoryObject`: https://learn.microsoft.com/en-us/windows/win32/devnotes/ntopendirectoryobject (page last updated 2021-03-22; living documentation, not a pinned Windows build).
- [S2] Microsoft, `NtQueryDirectoryObject`: https://learn.microsoft.com/en-us/windows/win32/devnotes/ntquerydirectoryobject (living documentation).
- [S3] Microsoft, `NtQueryDirectoryFile` (WDK): https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntquerydirectoryfile (living documentation).
