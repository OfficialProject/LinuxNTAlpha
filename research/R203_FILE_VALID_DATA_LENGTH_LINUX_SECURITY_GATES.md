# R203 — NT valid-data length versus Linux file extent and zero-fill semantics

Status: DOCUMENTATION REVIEW ONLY (2026-11-20). No Windows reference run, Linux backend test, implementation, or performance measurement. This is distinct from R101 sparse-file extent operations, R201 unbuffered I/O and R202 mapped-view flushing.

## Verified primary-source contracts

1. **Three distinct lengths.** Microsoft documents file size (EOF), allocation size, and valid data length (VDL) as separate concepts. SetEndOfFile changes file size; SetFileValidData changes VDL. Extending EOF does not itself establish initialized data. [Microsoft SetEndOfFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setendoffile).
2. **Privilege and exposure.** SetFileValidData requires a write-capable handle and enabled SeManageVolumePrivilege at open; Microsoft explicitly warns that skipping zero initialization can disclose previous disk contents, including after a crash or subsequent copy to a less restricted destination. The Win32 documentation excludes network, compressed, sparse and transacted files. [Microsoft SetFileValidData](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilevaliddata).
3. **Native information class.** FILE_VALID_DATA_LENGTH_INFORMATION supplies an eight-byte signed length to ZwSetInformationFile; the WDK requires positive growth beyond current VDL and no larger than file size. [Microsoft WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-_file_valid_data_length_information). MS-FSCC specifies privilege, invalid-parameter and buffer-length statuses; MS-FSA additionally specifies an oplock-break check before state update. [MS-FSCC](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/5c9f9d50-f0e0-40b1-9b84-0b78f59158b1), [MS-FSA](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fsa/bb684ef2-1d58-407f-9e5f-e503e339064f).
4. **Linux mismatch.** Linux fallocate ZERO_RANGE promises that subsequent reads of the range return zero; it can implement this with unwritten extents. Punch-hole likewise returns zeros. Neither operation is a documented analogue of privileged exposure of uninitialized allocated disk contents. [Linux fallocate(2)](https://man7.org/linux/man-pages/man2/fallocate.2.html).

**Documentation nuance:** The Win32 SetFileValidData page says new length is *less than* file size, while WDK/MS-FSCC allow *less than or equal to* file size. Do not silently reconcile this discrepancy: test exact-EOF input through both documented entry points on a pinned Windows build.

## Candidate subsystem boundary (HYPOTHESIS, not selected)

Track guest EOF, allocation length and VDL independently in the guest file-object/stream metadata layer. Preserve Linux's zero-return guarantee for unwritten extents rather than trying to expose host storage. The safe initial implementation may reject privileged VDL advancement as unsupported rather than falsely report successful NT semantics. A zero-filled implementation could avoid disclosure but would not faithfully reproduce the documented privileged no-zeroing operation or its performance; measure and label this as an explicit compatibility deviation.

**Security requirement:** Never translate guest SeManageVolumePrivilege into raw host block-device access or uninitialized cross-tenant bytes. Validate rights on the guest handle and token, check object identity and filesystem capabilities, and keep the host filesystem as the ultimate data-isolation boundary. Guest metadata alone must not make stale host data readable.

## Differential and adversarial gates (PROPOSED; not executed)

- On a disposable Windows test volume, capture exact Win32/NTSTATUS behavior for VDL=0, unchanged VDL, VDL+1, exactly EOF, greater than EOF, and malformed information buffer lengths. Compare privileged and nonprivileged tokens and handle access masks.
- Repeat for ordinary, sparse, compressed, remote and directory handles; record filesystem and Windows build. Check oplock-break sequencing and shared-handle effects where observable.
- Extend EOF, perform partial writes, advance VDL, read holes through a second handle, close/reopen and crash-recovery **only on isolated throwaway storage**. Never expose uninitialized data from a real disk or shared environment.
- Linux backend: test ftruncate, fallocate default/KEEP_SIZE/ZERO_RANGE and hole punching on each supported filesystem; verify visible bytes, allocated blocks, metadata persistence and behavior across file aliases. Distinguish test success from Windows conformance.
- Security gate: no path may return previously allocated host bytes from another file or tenant, including after guest privilege changes, remount, rename, copy, truncation or crash.
- Benchmark only after semantics are specified: report time to extend, first random write, disk space and write amplification; do not claim a speedup from documentation.

## Open questions

- What exact Win32/native differences explain the documented EOF equality discrepancy, and are they build/filesystem dependent?
- Which filesystem-specific allocation/VDL behaviors can be represented without guest-managed storage or a custom filesystem?
- What should the compatibility policy do for privileged applications that explicitly rely on uninitialized data visibility? Security must take precedence over pretending full equivalence.

Source review date: 2026-11-20. All findings above are sourced contracts or explicitly labeled hypotheses/proposed tests; no experiments were run.
