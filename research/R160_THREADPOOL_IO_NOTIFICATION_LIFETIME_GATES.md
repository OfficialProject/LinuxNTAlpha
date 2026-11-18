# R160 — Threadpool I/O notification accounting and overlapped-request lifetime

Research date: 2026-11-18. Status: **DOCUMENTATION REVIEW ONLY**. No Windows reference program, Linux backend, race test, or performance experiment was executed.

## Scope and relationship to prior work

R39 and R149–R158 cover other threadpool callbacks and teardown. R43/R111/R137 cover asynchronous I/O completion/cancellation. This note isolates the **TP_IO notification accounting** contract, which must not be collapsed into either a raw IOCP packet or the lifetime of the underlying overlapped operation.

## VERIFIED — Microsoft primary API contracts

1. `CreateThreadpoolIo` binds a file handle, callback, context, and optional callback environment to an opaque TP_IO object. Receiving callbacks requires `StartThreadpoolIo`. [CreateThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolio).
2. `StartThreadpoolIo` is required **before each** asynchronous I/O request; Microsoft explicitly warns that omitting it can cause ignored completions and memory corruption. If submission fails with an error other than `ERROR_IO_PENDING`, the caller must balance the notification with `CancelThreadpoolIo`. [StartThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-startthreadpoolio).
3. With `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS`, immediate successful completion generates no associated TP_IO callback and the notification must also be balanced with `CancelThreadpoolIo`. This is **not** equivalent to canceling the I/O request. [StartThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-startthreadpoolio); [CancelThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-cancelthreadpoolio).
4. `WaitForThreadpoolIoCallbacks(pio, TRUE)` cancels queued **callbacks**, not pending I/O operations. The documentation explicitly warns against freeing an `OVERLAPPED` while the operation remains pending, and suggests `GetOverlappedResult` or prior `CancelIoEx` where appropriate. [WaitForThreadpoolIoCallbacks](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-waitforthreadpooliocallbacks).
5. `CloseThreadpoolIo` may release the TP_IO asynchronously after outstanding callbacks complete. The documentation directs callers to close the associated file handle and wait for outstanding overlapped operations before closing TP_IO; no new overlapped I/O may be initiated afterward. [CloseThreadpoolIo](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolio).
6. The older `BindIoCompletionCallback` API is a distinct threadpool binding path and its documentation cautions about immediate submission errors, initiating-thread termination and callback DLL unload. Do not assume it shares the TP_IO Start/Cancel protocol. [BindIoCompletionCallback](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-bindiocompletioncallback).

## HYPOTHESIS — prospective LinuxNTAlpha design, not a measured implementation

Maintain separately: (a) guest TP_IO identity and callback-environment ownership; (b) a per-submission **notification credit** acquired before submitting; (c) a unique overlapped-request generation and its terminal completion; and (d) queued/running callback references. A failed submission or a successful no-packet completion under skip-on-success consumes the notification credit without fabricating a callback. Callback cancellation must not free request buffers before the backend establishes terminal completion. The exact matching algorithm for simultaneous requests is **OPEN**; do not assume that a simple aggregate counter is sufficient for correct correlation.

A possible backend can use io_uring for operations and a guest completion scheduler for TP_IO callbacks, but Linux CQEs do not implement the Win32 Start/Cancel notification contract, Windows callback environments, or guest object teardown automatically. This is an architecture candidate, not a claim of equivalence.

## Proposed falsifiable conformance gates — NOT RUN

- Windows reference: create a TP_IO on an overlapped-capable file; submit multiple operations with one Start each; record callback count, callback `OVERLAPPED` identity and completion status. Include immediate pending, immediate success and immediate failure; explicitly vary skip-on-success mode.
- Validate each documented no-callback path with a balancing Cancel; exercise missing/extra balancing only in isolated disposable processes because Microsoft warns about memory corruption.
- Queue callbacks behind a blocked worker, invoke `WaitForThreadpoolIoCallbacks(TRUE)`, and independently determine when the underlying I/O actually completes before reclaiming its `OVERLAPPED` and buffer.
- Race `CancelIoEx`, callback dequeue, file close, and TP_IO close while preserving request references; use repeated deterministic barriers and record OS build, file type and completion mode. No outcome is predicted.
- Confirm callback-environment and DLL lifetime on the Windows reference before mapping any host scheduler or module-unload policy.

## Architectural consequence

The first integrated runtime should model **notification credit**, **I/O request completion**, and **callback lifetime** as three distinct state transitions. Treat collapsing them as a correctness risk until Windows differential evidence disproves that concern. This does not select a kernel architecture or justify performance claims.

## Source audit

All six references above are Microsoft Learn primary API contract pages, inspected 2026-11-18. API minimum support for TP_IO functions is Windows Vista / Windows Server 2008 per those pages; exact modern build-specific behavior remains untested. No source code was imported.
