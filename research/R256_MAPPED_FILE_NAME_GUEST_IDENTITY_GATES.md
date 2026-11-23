# R256 — Mapped-file name queries must use guest section identity

Date: 2026-11-23. Status: **source-grounded design research only**. No Windows reference run, Linux prototype, benchmark, or implementation.

## Scope and relation to earlier work

R59 (image-section copy-on-write), R61 (file identity), R64/R70 (mapped-view flushing), and R114 (delete disposition) examine backing objects and lifetime. This note isolates a different observable contract: resolving a *guest address* to the *guest-visible name* of its mapped file, including when Linux backing differs from Windows naming. It does not claim an undocumented NT memory-information class is supported.

## VERIFIED FROM PRIMARY DOCUMENTATION

1. Microsoft's documented `GetMappedFileNameW` checks whether an address in a specified process lies within a memory-mapped file, and returns the mapped file's name. The process handle requires `PROCESS_QUERY_INFORMATION`; the output length is in characters, and the documented too-small-buffer behavior is truncation with `ERROR_INSUFFICIENT_BUFFER`. Source: https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmappedfilenamew (consulted 2026-11-23).
2. Microsoft's published kernel `NtQueryVirtualMemory` and `MEMORY_INFORMATION_CLASS` documentation currently identifies **only `MemoryBasicInformation`** as supported by that documented interface. Do **not** promote reverse-engineered `MemoryMappedFilenameInformation` enum values to a Microsoft-documented ABI or infer that the PSAPI implementation necessarily uses a particular native class. Sources: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntqueryvirtualmemory and https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ne-ntifs-_memory_information_class (consulted 2026-11-23).
3. Linux `/proc/pid/maps` reports VMAs with permissions, offset, device, inode and optional pathname; its read permission is subject to a ptrace access-mode check. This is host VM metadata, not an NT object namespace or Windows process-handle authorization model. Source: https://man7.org/linux/man-pages/man5/proc_pid_maps.5 (consulted 2026-11-23).
4. Linux `/proc/pid/exe` is a symlink to the executed host program and can show a ` (deleted)` suffix after unlink; access is ptrace-gated. A guest PE started by a Linux ELF loader therefore cannot safely infer its guest image name from this symlink. Source: https://man7.org/linux/man-pages/man5/proc_pid_exe.5 (consulted 2026-11-23).

## HYPOTHESIS — implementation boundary, not a decision

Maintain a per-guest-process mapping registry keyed by guest address intervals, with references to guest section/file objects, original namespace provenance, backing identity and view lifetime. Query the *guest* registry only after enforcing guest process-handle rights. Render a guest NT device path or other Windows-visible form from the retained guest file identity; do not expose Linux container paths or `/proc` names. Track split/unmap/remap and image-vs-data views, and retain backing identity across guest rename/delete where the reference Windows system requires it. A host `/proc` snapshot may assist diagnostics but must not be the authoritative guest result. All of these are proposed invariants awaiting Windows oracle validation.

## OPEN — discriminating conformance work (NOT RUN)

- On a version-pinned Windows host, map one file into two processes and query interior, boundary and unmapped addresses; record `GetMappedFileNameW` results, path format, access checks, zero-length buffer, exact-fit and undersized buffers.
- Rename and delete/disposition a file while a view remains; query before and after, distinguishing successful rename, denied rename, and deferred deletion. Repeat with image and data sections.
- Query across process boundaries with only `PROCESS_QUERY_LIMITED_INFORMATION`, `PROCESS_QUERY_INFORMATION`, and no rights; record errors instead of assuming that newer Windows rights substitute for the documented right.
- Compare hardlinks, case variants, device aliases, network-backed files and section views with no remaining file handle. Determine whether name reflects section creation, current file name, or some other reference-system identity.
- If native undocumented information classes are considered, pin their definitions to a specific upstream implementation and independently verify each on Windows; the Microsoft kernel documentation above does not establish them.
- Linux prototype: map/unmap/split views with `mmap`/`munmap`, compare host `/proc/self/maps` to an independently maintained guest registry; inject rename/unlink and check for host-path leakage. **No such test has been executed here.**

## Architectural consequence

A path-query adapter is not simply a wrapper around `/proc/pid/maps`. The existing guest object/section registry should own identity and authorization, while Linux VMA information is an implementation detail. This alone does not justify a kernel extension; keep ADR-003 open.
