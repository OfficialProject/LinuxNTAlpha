# R142 — ReadFileEx/WriteFileEx completion routines: initiating-thread affinity and alertable delivery

Date reviewed: 2026-11-22 UTC. Status: **DOCUMENTATION VERIFIED; IMPLEMENTATION HYPOTHESIS; NO EXPERIMENT**.

## Why this is a separate gate

R13/R88 discuss APCs, R43/R137 cancellation and completion, and R15/R66/R111 IOCP. This note isolates the **ReadFileEx/WriteFileEx completion-routine path**: an I/O backend completing a request is not equivalent to executing its Windows completion callback. No working NT I/O runtime is claimed.

## Verified Windows contracts

1. `ReadFileEx` and `WriteFileEx` specify an `LPOVERLAPPED_COMPLETION_ROUTINE`; the routine executes when the operation completes **or is canceled**, and the **initiating thread** enters an alertable wait. If completion happens before the alertable wait, the callback remains queued until that thread enters one. The `OVERLAPPED` object must remain valid for the operation's lifetime. `ReadFileEx` ignores `OVERLAPPED.hEvent`, but uses `Internal` and `InternalHigh`. [Microsoft ReadFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfileex), [Microsoft WriteFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefileex).
2. The callback receives error code, transferred byte count, and the original `OVERLAPPED*`. The Microsoft callback contract says all waiting completion routines are called before the alertable wait finishes with `WAIT_IO_COMPLETION`; callback ordering is **not guaranteed**. Callback reentrancy into further alertable waits can grow the thread stack. [Microsoft completion routine](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/nc-minwinbase-lpoverlapped_completion_routine).
3. `GetOverlappedResultEx` has a distinct optional alertable-wait path; the documentation explicitly requires the initiating thread for a queued ReadFileEx/WriteFileEx completion routine. An I/O completion routine must not be treated as an arbitrary worker callback or an IOCP packet. [Microsoft GetOverlappedResultEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresultex), [Microsoft synchronous/overlapped pipe I/O](https://learn.microsoft.com/en-us/windows/win32/ipc/synchronous-and-overlapped-input-and-output).
4. Linux `io_uring` reports request completions through CQEs, correlated using `user_data`; requests can complete out of order. Neither the CQE mechanism nor `io_uring_enter` provides a Windows initiating-thread guest callback or Windows alertable-wait return value. [Linux io_uring(7)](https://man7.org/linux/man-pages/man7/io_uring.7.html), [Linux io_uring_enter(2)](https://man7.org/linux/man-pages/man2/io_uring_enter.2.html).

## Architecture hypothesis (NOT validated)

Maintain an I/O request record with a stable generation-tagged identity, initiating **guest** thread, guest `OVERLAPPED*`, callback pointer, buffer lifetime state and terminal result. A Linux backend completion should transition this record exactly once into a guest-thread-specific pending completion-routine queue; it should **not call the guest callback from the io_uring CQ consumer**. Alertable-wait entry drains the eligible queue on the initiating guest thread using the Windows x64 calling convention, preserving guest exception/TEB context. Cancellation requests and terminal results must remain separate (R43/R137). Distinguish completion-routine delivery from IOCP/event paths; do not assume the same operation should produce every completion mechanism.

This is a proposed model, not proof of Windows-internal implementation or ordering. Windows callback exception behavior, callback reentrancy and thread-exit teardown require reference tests.

## Falsifiable conformance gates (NOT RUN)

- **Thread affinity:** Thread A submits ReadFileEx; thread B enters alertable wait first. Verify B does not execute A's callback; then A waits and verify callback thread identity.
- **Deferred delivery:** Complete I/O while A runs ordinary non-alertable work. Verify no callback before A's first alertable wait; verify callback then runs and wait returns the documented status.
- **Multiple completions:** Submit multiple requests with distinct `OVERLAPPED` pointers; check exact-once correlation and bytes/error values without asserting callback FIFO order.
- **Cancellation race:** Exercise cancel-before-complete, complete-before-cancel and concurrent cancellation. Distinguish terminal completion from cancellation request and preserve the original buffer/`OVERLAPPED` until terminal completion.
- **Reentrancy:** From a completion callback, submit another request and enter a nested alertable wait. Record nesting and return behavior without assuming callback ordering.
- **Lifetime:** Thread exit with outstanding operations, handle closure and failed submission must be compared against a pinned Windows version; do not invent expected results where documentation is silent.
- **Backend:** Repeat on Linux `io_uring` and a fallback worker I/O backend; check queue handoff, no duplicate callback, thread identity, no use-after-free and wakeup races. These Linux tests alone do not establish Windows compatibility.

## Unresolved

Windows-version-dependent edge cases, APC interaction/order, terminal cancellation status mapping, and resource teardown after initiating-thread exit remain **OPEN**. Pin Windows build, filesystem/device class and Linux kernel/toolchain before measuring. No benchmark, Windows-reference run or Linux runtime run was performed for R142.
