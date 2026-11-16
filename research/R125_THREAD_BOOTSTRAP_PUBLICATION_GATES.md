# R125 — NT-compatible thread bootstrap and publication barrier

Research date: 2026-11-16. Status: **documentation-backed analysis; no experiment executed**.

## Question and boundary

Can a Linux thread be exposed as a successfully created Windows thread before its guest TEB, loader notifications, TLS and exception/dispatch context are ready? **Hypothesis:** doing so risks guest code observing an incompletely initialized thread, and a host-thread creation success must not be equated with completed guest initialization. This note specifies a bootstrap/publication boundary, not a selected kernel/runtime design.

Related work: [R24](R24_GS_TEB_CONTEXT.md) and [R25](R25_PE64_GS_TIB_EXECUTION.md) establish synthetic GS/TIB probes; [R45](R45_DLL_LOADER_LOCK_LIFECYCLE.md) and [R91](R91_DLL_LOADER_LOCK_NOTIFICATION_ROLLBACK.md) cover DLL notification ordering; [R74](R74_THREAD_OBJECT_EXIT_LIFETIME.md) covers thread-object lifetime; [R42](R42_THREAD_SUSPEND_CONTEXT_GATES.md) covers suspend counts. **New focus here:** the cross-subsystem startup publication barrier and failure ownership.

## Verified source contracts

1. Microsoft's CreateThread documentation defines a new thread's start routine, initial stack-size/reservation flags, CREATE_SUSPENDED behavior, handle and ID outputs. It explicitly warns that creation may succeed even with an invalid start address, with failure arising when execution begins. Therefore, a loader must not silently convert all future start-routine failures into synchronous creation errors. [Microsoft CreateThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread).
2. Microsoft states that during process startup and DLL initialization, newly created threads do not begin executing application code until DLL initialization completes; DLL initialization/detach is serialized per process. This is a concrete startup ordering constraint, **not** evidence that all internal kernel thread creation is blocked. [Microsoft CreateThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread); [Microsoft CreateRemoteThreadEx](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createremotethreadex).
3. Microsoft distinguishes CRT-managed thread startup via _beginthreadex from raw CreateThread, and documents different cleanup/handle ownership. A compatible subsystem must not assume it can implement the CRT's per-thread resources merely by allocating a kernel thread. [Microsoft _beginthreadex](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/beginthread-beginthreadex).
4. Linux clone/clone3 supports creation-time thread and TLS-related controls, but is a host task creation mechanism rather than a Windows guest TEB/loader lifecycle implementation. [Linux clone(2)](https://man7.org/linux/man-pages/man2/clone.2.html).
5. Linux clear_child_tid clears a host memory word and futex-wakes on termination; this is **not** a durable Windows thread-object completion record. [Linux set_tid_address(2)](https://man7.org/linux/man-pages/man2/set_tid_address.2.html). See also R74.

## Proposed internal state machine (HYPOTHESIS, not observed Windows internals)

- **ALLOCATING:** validate guest creation flags/rights and reserve guest thread identity, stack, TEB/TLS bookkeeping and a lifetime-owned thread object; failures here return creation errors.
- **HOST_CREATED_PARKED:** host thread exists but cannot enter untrusted guest code; its Linux TLS, signal/dispatch ownership and guest GS selection are initialized. Parent and child communicate through an explicit barrier with well-defined release/acquire publication.
- **GUEST_READY:** guest thread context, initial TEB/TLS, loader notification plan, and handle/ID visibility are coherent. For CREATE_SUSPENDED, hold the start gate while allowing the creation API to return.
- **LOADER_GATE:** serialize applicable DLL_THREAD_ATTACH notifications and process initialization constraints before calling the guest start routine. Avoid waiting for a worker while holding the loader serialization lock if the worker needs that lock.
- **RUNNING / EXITING / TERMINATED:** execute guest entry under the correct ABI; on normal exit, perform applicable teardown and publish durable guest exit status and signaling separately from Linux task reaping.

This is an **implementation proposal**, not a claim that Windows uses these exact states. In particular, the relative ordering of guest handle publication, loader notifications, and the CreateThread return is a **Windows-reference differential question**. Do not encode the proposed ordering as verified behavior.

## Failure and security gates

- An early child initialization failure must not leak a runnable thread with host FS/guest GS or signal-dispatch state half installed. Define rollback ownership for stack, TEB, TLS slots, handle-table entry, host task, and exit-status record.
- Guest code must never run before the guest address-space/module lifetime references it uses are acquired. A concurrently unloading DLL or terminating process must not leave a start address dangling without the appropriate guest-visible failure semantics.
- Keep guest TEB pointer selection separate from host libc TLS. R24/R25 are narrow synthetic evidence, not a complete bootstrap.
- Linux host thread IDs are implementation identifiers; expose guest thread IDs and guest handles through a dedicated table. Preserve lifetime across handle closure and completion (R74).
- Host pthread/clone cancellation, signals and async termination cannot be substituted blindly for Windows TerminateThread, ExitThread, APC or exception behavior.

## Falsifiable next experiments (NOT RUN)

1. **Windows reference, version recorded:** CreateThread from DllMain during DLL_PROCESS_ATTACH, with bounded watchdog and atomic event trace; compare the time of API return, DLL_THREAD_ATTACH and guest start-routine entry. Repeat from outside loader initialization and with DisableThreadLibraryCalls.
2. **Windows reference:** CREATE_SUSPENDED thread; verify whether DLL_THREAD_ATTACH occurs before or after ResumeThread and whether the thread start routine is ever entered while suspended. Repeat multiple ResumeThread calls and record return values.
3. **Windows reference:** create a thread with an invalid but non-null start address and record creation result, eventual exception/process exit, and thread-object signaling; isolate each run in a child process.
4. **Linux isolated prototype:** host-thread parked bootstrap, per-thread GS, Linux FS, signal dispatcher, and start gate; stress concurrent create/exit, rollback injection at each stage and sanitizer configurations. Report PASS/SKIP and actual kernel/compiler versions.
5. **Integrated PE fixture:** guest entry creates another guest thread via a deliberately implemented imported API, reads synthetic TEB and per-thread TLS, then exits with an exact DWORD; verify thread-object status after host thread teardown. This would be a *new* integration result, not covered by R24/R25.

## Decision

**OPEN.** Prefer a parked-host-thread bootstrap with explicit guest publication and teardown barriers as the first falsifiable design. Do not commit to direct clone3, pthreads, kernel changes, or a precise Windows internal sequencing model until reference tests and fault-injection runs exist. No runtime compatibility or performance result is claimed.
