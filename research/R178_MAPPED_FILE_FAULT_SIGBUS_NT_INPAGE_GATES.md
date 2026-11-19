# R178 — mapped-file fault provenance: Linux SIGBUS versus NT in-page exceptions

Research date: 2026-11-19. **Status: primary-source documentation review only; no Windows or Linux fault experiment performed.** This note extends R108 (signal ownership), R120 (context restoration), R117 (section protections), and R70 (flush durability); it does not claim those topics are solved.

## Verified contracts

1. **Windows:** Microsoft documents that reading or writing a mapped view of a non-pagefile-backed file can raise `EXCEPTION_IN_PAGE_ERROR` when backing I/O cannot supply the page (examples include network disconnection, disk/device failure, disk full, or memory allocation failure). Applications may catch the exception through SEH and should check whether the fault address belongs to their mapping. [Microsoft, Reading and Writing From a File View](https://learn.microsoft.com/en-us/windows/win32/memory/reading-and-writing-from-a-file-view).
2. **Windows exception record:** For `EXCEPTION_IN_PAGE_ERROR`, `EXCEPTION_RECORD.ExceptionInformation[0]` denotes read (0), write (1), or execute (8); element [1] is the inaccessible virtual address; element [2] is the underlying NTSTATUS. This is more information than the mere exception code. [Microsoft, EXCEPTION_RECORD](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record).
3. **Linux:** `mmap(2)` documents SIGBUS for access to a mapped page beyond the end of the file, versus SIGSEGV for writes to read-only mapped memory. POSIX `ftruncate` specifies SIGBUS on subsequent references to discarded whole pages after shrink. [Linux mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html); [POSIX ftruncate](https://man7.org/linux/man-pages/man3/ftruncate.3p.html).
4. **Linux edge:** The partial page at the end of a non-page-aligned file is zero-filled; modifications past EOF within that page are not written to the file. The Linux mmap manual also warns that such modifications may remain in page cache and be visible to later mappings. [Linux mmap(2), NOTES](https://man7.org/linux/man-pages/man2/mmap.2.html).

## Architectural implication (inference, not verified implementation)

A blanket `SIGBUS -> STATUS_IN_PAGE_ERROR` translation is **not justified**: Linux EOF/truncation SIGBUS and backing I/O failure are distinguishable causes, and Windows behavior for every corresponding guest scenario is not established by these documents. A guest VM ledger should retain section identity, file-backed versus anonymous provenance, current view bounds and generation, plus any reliably available host fault metadata. A signal ingress layer must distinguish host-runtime faults from guest faults before constructing a guest `EXCEPTION_RECORD`. It must not manufacture an underlying NTSTATUS for an unknown host failure.

Mapping guest faults into SEH requires correct guest instruction/context capture and unwind/continuation (R9/R108/R120), not simply returning from a Linux signal handler. The design must also account for a racing file shrink and mapping teardown without dereferencing freed view metadata in the signal path.

## Falsifiable gates (proposed; NOT executed)

- **Windows reference matrix:** Windows build/filesystem documented; create mapped views of a local file, truncate or resize through permitted handles where allowed, and separately inject reproducible backing I/O failure on a controlled test volume. Capture whether mapping/truncation is permitted, exception code, parameter count and all defined parameters, address, access type, and recovery outcome. Do not infer Windows behavior from Linux results.
- **Linux baseline matrix:** pinned kernel/filesystem; mmap regular file with page-aligned and partial-page lengths; read/write in last partial page, beyond last full page, and after ftruncate shrink. Record signal, `siginfo_t` code/address and exact file size; test each in disposable child processes.
- **Guest ingress:** fault only inside a known guest view; verify host faults are not rewritten; stress unmap/truncate/fault races under ASan where compatible; verify nested signal handling, guest exception dispatch and continuation without deadlocks.
- **Failure fidelity:** where Linux does not expose a trustworthy backing-error-to-NTSTATUS mapping, record the unknown and fail conservatively; do not assert identical third exception parameter. Explicitly test injected I/O errors separately from EOF.
- **Security:** reject fault addresses outside the registered guest VM ledger; never permit an untrusted guest exception handler to consume or alter host signal frames or runtime bookkeeping.

## Open questions

- Which NTSTATUS does each Windows build/filesystem return for EOF-after-truncation versus genuine backing I/O failure? Requires Windows reference experiments.
- Which Linux `siginfo_t` fields reliably distinguish truncation, filesystem failure and hardware memory errors across kernels/filesystems? Requires pinned kernel-source audit and experiments.
- Can guest file-sharing and section lifetime semantics prevent some Linux-truncation scenarios entirely? Depends on R10/R49/R50 and Windows reference observations.

**No code, benchmark, compatibility claim or experiment is introduced in R178.**
