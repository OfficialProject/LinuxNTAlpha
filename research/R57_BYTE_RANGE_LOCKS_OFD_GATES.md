# R57 — Windows byte-range locks versus Linux OFD locks

Evidence date: 2026-11-13 UTC. **Primary Microsoft API documentation and Linux man-pages review only; no Windows or Linux experiment executed.** Extends R10 (object/file identity), R43 (asynchronous completion/cancellation), R49–R50 (open-handle and namespace semantics). The focus is byte-range lock *ownership and enforcement*, not file-sharing flags.

## VERIFIED FROM SOURCE

1. Windows `LockFileEx` takes an `OVERLAPPED` start offset and 64-bit length; it supports shared or exclusive locking, and `LOCKFILE_FAIL_IMMEDIATELY` selects immediate failure instead of waiting. With an asynchronous handle, an unavailable lock may pend and later signal the event; successful completion must be determined through the overlapped completion mechanism. [Microsoft LockFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex).
2. Exclusive Windows byte-range locks deny other processes' read/write access to the locked range; shared locks deny writes, including by the lock owner, but permit reads. Locks can extend beyond EOF. Memory-mapped file views bypass byte-range lock enforcement. A second file handle opened by the locking process does not automatically gain access. [Microsoft LockFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex); [Microsoft byte-range locking overview](https://learn.microsoft.com/en-us/windows/win32/fileio/locking-and-unlocking-byte-ranges-in-files).
3. The Microsoft `LockFileEx` documentation explicitly describes an exceptional **same-handle** case: a shared lock can overlap an exclusive lock held via the same handle, and two unlocks are needed for the combined lock state. An inherited handle does not confer access to the parent's locked range on the child. Do **not** simplify Windows ownership to a generic POSIX process lock or one flat interval type. [Microsoft LockFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex).
4. Linux traditional `fcntl(F_SETLK/F_SETLKW)` record locks are process-associated. Closing **any** descriptor referring to the file releases all that process's traditional record locks on that file; threads within the process share those locks. [Linux man-pages fcntl_locking(2)](https://man7.org/linux/man-pages/man2/F_SETLK.2const.html).
5. Linux `F_OFD_SETLK/F_OFD_SETLKW` locks are associated with an **open file description**, persist across duplicated descriptors/fork, and release on the last close of that description. However, new OFD locks on the *same* open description are always compatible and may **convert, split or coalesce** the prior lock. The Linux documentation states no deadlock detection for OFD locks. [Linux man-pages fcntl_locking(2)](https://man7.org/linux/man-pages/man2/F_OFD_SETLK.2const.html).
6. Linux OFD and traditional record locks are ordinarily **advisory**; an uncooperative host process can ignore them. Linux mandatory locking is documented as unreliable and unsupported from Linux 5.15. Thus an OFD lock by itself does not enforce Windows-style denial of ordinary `read`/`write` calls. [Linux man-pages fcntl_locking(2)](https://man7.org/linux/man-pages/man2/F_OFD_SETLK.2const.html).

## Architectural inference — HYPOTHESIS, not a selected implementation

Use a guest-visible lock table keyed to stable file identity plus **Windows handle/open and owner context**, tracking range, shared/exclusive mode, nesting/overlap, pending requests and completion ownership. Route guest reads/writes and locks through one enforcement point. An OFD lock may be useful for cooperative host interoperability but cannot alone represent the Windows same-handle overlap stack, inherited-handle access distinction, mandatory I/O denial or asynchronous completion.

A broker-based design could serialize conflicting guest lock acquisitions and range I/O; it must also define how mapped views, duplicated/inherited handles, process exit, cancellation, hard links and nonparticipating host processes behave. Do not claim isolation if host processes can bypass the broker. In particular, **never** treat `F_OFD_SETLK` success as proof that a Windows guest write would be allowed.

The Linux manual describes OFD locking as Linux-specific (introduced in 3.15); this is an API availability fact, not evidence of NT equivalence or performance superiority.

## Falsifiable conformance matrix — NOT RUN

On a **pinned Windows version and filesystem** (local NTFS first; SMB separately), record each operation, handle identity, access rights, byte offset/length, pending state, completion method, exact `GetLastError`, and final file bytes:

1. Handle A takes exclusive range [100, 200). On independent handle B, attempt overlapping and disjoint synchronous reads/writes, plus reads through a mapped view; repeat with a shared lock. Include zero-length/overflow parameter checks as exploratory, **not** predicted outcomes.
2. Acquire shared then exclusive overlapping ranges via **the same handle** where documented; unlock once, probe remaining denial, unlock again, probe release. Repeat with two independent opens to distinguish ownership.
3. Duplicate handle A, then independently open B; close original A and check whether duplicate retains lock. Inherit A into a child and probe child's access, recording the documented child restriction separately from lock lifetime.
4. With an overlapped handle, queue a blocked `LockFileEx`, release the conflicting lock, observe event/completion and actual acquired state. Repeat with immediate-failure flag and with cancellation requests; correlate with R43's cancellation-versus-completion gates.
5. Acquire a range extending past EOF; then extend file and verify range enforcement. Check hard-link aliases and concurrent rename for identity consistency.
6. Run a Linux *separate* harness using `F_SETLK` and `F_OFD_SETLK`: demonstrate unrelated-descriptor close effects, same-description conversion, fork/dup lifetime, and a noncooperating `pread/pwrite` process. These are proposed tests, not results.
7. For a prospective broker, issue simultaneous conflicting lock requests and reads/writes under a bounded watchdog; compare event traces to Windows, including release on abnormal termination. Repeat on a remote filesystem only after explicitly scoping its guarantees.

## OPEN

- Exact Windows lock-owner identity and inherited/duplicated-handle corner cases beyond the documented cases; verify on a real Windows reference before choosing internal keys.
- Windows pending-lock cancellation and IOCP/event ordering for all handle modes; link to R43, do not infer from Linux `EINTR`.
- Whether a secure isolated file namespace plus broker can cover all guest I/O without expensive host interception; benchmark only after correctness gates pass.
- Filesystem-specific locking behavior on SMB/ReFS and mapped views; do not extrapolate local NTFS tests.

**Status:** API contracts VERIFIED FROM SOURCE; translation architecture HYPOTHESIS; reference experiments OPEN. No lock implementation or compatibility claim.
