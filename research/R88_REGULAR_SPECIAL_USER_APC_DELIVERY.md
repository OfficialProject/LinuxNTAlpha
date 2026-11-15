# R88 — Regular versus special user-mode APC delivery boundaries

Status: RESEARCH ONLY — no implementation, Windows-reference run, or Linux experiment.
Evidence checked: 2026-11-14 UTC. Scope: documented Win32 API contracts, not undocumented NT internals.

## Verified from Microsoft primary documentation

1. `QueueUserAPC` requires a target thread handle with `THREAD_SET_CONTEXT`. Each thread owns an APC queue; ordinary user APCs run when the target enters an alertable wait, pending ordinary APCs are dispatched FIFO, and the wait returns `WAIT_IO_COMPLETION`. An APC queued before thread startup may execute at startup. APC callbacks may themselves enter alertable waits, recursively dispatching more APCs. On thread termination queued APCs are discarded. [M1]
2. The documented alertable wait APIs include `SleepEx`, `WaitForSingleObjectEx`, `WaitForMultipleObjectsEx`, `SignalObjectAndWait`, and `MsgWaitForMultipleObjectsEx`. If a wait completes before an ordinary APC arrives, the APC remains queued until a later alertable opportunity. [M2]
3. `QueueUserAPC2` supports **special user-mode APCs** on Windows 11 build 22000+ and Windows Server 2022 build 20348+. These can interrupt a target executing user-mode code without an alertable wait; when the target is in a syscall or non-alertable wait, delivery occurs after that operation completes, rather than interrupting the wait. The documentation warns about reentrancy, including a special APC interrupting another special APC. It says special APCs are not supported under WoW. [M3]
4. `QUEUE_USER_APC_FLAGS` distinguishes `NONE`, `SPECIAL_USER_APC`, and `CALLBACK_DATA_CONTEXT`; the last changes the callback parameter to an `APC_CALLBACK_DATA` containing the original parameter and an interrupted-context pointer. [M4]
5. Completion routines for `ReadFileEx`, `WriteFileEx`, and waitable timers are APC-based. These are **thread-targeted callback semantics**, not interchangeable with IOCP packets. [M2]

## Architecture implications (hypotheses, not verified implementations)

- **H1: Separate queue policy from delivery mechanism.** A prospective guest thread record should distinguish regular and special APC requests and keep callback payloads, target-thread identity, termination state and guest ABI context. A Linux eventfd/futex can notify a scheduler, but neither by itself executes a callback on the correct guest thread.
- **H2: Ordinary APC dispatch at guest alertable boundaries** can be implemented in the user-space NT wait layer, provided atomicity of wait registration, object readiness, APC enqueue, and return-status selection is independently verified. A simple check-then-sleep is vulnerable to a lost notification.
- **H3: Special APC delivery** likely needs an explicit guest execution interruption mechanism and safe restoration of guest registers/stack state. Linux signal delivery is a *candidate*, not a semantic match: signal masks, syscall restart, host-runtime critical sections, nested delivery and cross-ABI unwinding all require separate proofs. Never invoke arbitrary guest APC callbacks inside a host signal handler without an audited safety model.
- **H4: Callback identity and lifetime** must be tied to guest thread/process lifetimes. A host worker invoking callbacks on its own thread would violate the documented target-thread contract.

## Proposed falsifiable conformance matrix (NOT RUN)

1. Ordinary APC queued while a target performs non-alertable wait: verify no callback until a subsequent alertable wait; record wait return and callback thread ID.
2. Enqueue A/B/C before alertable entry: record FIFO order, callback argument, return code, and nested alertable wait behavior.
3. Race a signaled object with APC enqueue around `WaitForSingleObjectEx`: record permitted outcome distributions and ensure no dropped APC. Do not presume a deterministic winner without Windows evidence.
4. Queue before thread startup and queue during thread shutdown: record execution and documented failure/discard cases.
5. On supported Windows builds only, compare `QueueUserAPC2` regular versus special while target runs user code, blocks in non-alertable wait, or executes a syscall; measure callback thread ID and interruption timing. Gate WoW cases separately.
6. For `CALLBACK_DATA_CONTEXT`, verify original argument and context layout against installed Windows SDK headers, including x64 ABI alignment and register restoration.
7. Reentrant special APC with a lock held by interrupted code: demonstrate deadlock hazard in a disposable reference process, never in the main runtime.
8. Completion-routine APC versus IOCP completion: verify independent consumption paths and that an IOCP dequeue does not stand in for an alertable callback.

## Integration and decision gate

This is adjacent to R66 (IOCP alertable dequeue), R74 (thread lifetime), R75 (multi-object waits), and R55 (cross-ABI register preservation), but concerns the **delivery contract and interruption safety** of APC callbacks themselves. Do not claim special APC compatibility from regular APC tests. Before choosing signal-based special delivery, obtain Windows reference traces and isolated Linux tests proving nested callback, syscall, TLS/TEB, register and unwind invariants. Record OS build and SDK version for every test.

## Primary sources

- [M1] Microsoft, QueueUserAPC: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-queueuserapc
- [M2] Microsoft, Asynchronous Procedure Calls: https://learn.microsoft.com/en-us/windows/win32/sync/asynchronous-procedure-calls
- [M3] Microsoft, QueueUserAPC2: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-queueuserapc2
- [M4] Microsoft, QUEUE_USER_APC_FLAGS: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ne-processthreadsapi-queue_user_apc_flags

Source caveat: These describe supported Win32 behavior, not a complete specification of native `NtQueueApcThread` internals or Linux implementation feasibility.
