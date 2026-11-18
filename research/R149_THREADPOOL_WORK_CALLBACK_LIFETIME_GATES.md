# R149 — Windows thread-pool work callback and teardown gates

Date: 2026-11-17. **Status: primary Microsoft API documentation reviewed; no Windows reference execution, Linux implementation, or benchmark.**

## Scope and prior-work boundary

R74 treats thread-object exit; R91 covers DLL loader teardown; R111 and R137 address I/O completion lifetime. This note addresses a separate user-mode contract: the Vista-and-later Windows thread-pool **work object** can have multiple concurrent callback invocations, and closing the work object is not equivalent to synchronously draining callbacks. It does not claim that implementing work items implements thread-pool waits, timers, or I/O.

## VERIFIED FROM SOURCE (published Microsoft API contracts, not pinned implementation binaries)

1. [Thread Pool API](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-pool-api) defines per-process default pools, optional separate pools, cleanup groups and callback-generating work objects. A work object may have multiple posts outstanding. This is a distinct abstraction from Linux pthread creation or a single queue entry.
2. [CreateThreadpoolWork](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolwork) binds a callback, context and optional callback environment to a work object. A NULL environment selects the default environment.
3. [SubmitThreadpoolWork](https://learn.microsoft.com/en-us/windows/desktop/api/threadpoolapiset/nf-threadpoolapiset-submitthreadpoolwork) allows repeated submissions, up to MAXULONG, without waiting for previous callbacks; callbacks may execute in parallel. Therefore one boolean pending flag per work object is insufficient.
4. [WaitForThreadpoolWorkCallbacks](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-waitforthreadpoolworkcallbacks) waits for outstanding callbacks and optionally cancels **pending, not-yet-started** callbacks. It does not promise interruption of already executing guest callbacks.
5. [CloseThreadpoolWork](https://learn.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-closethreadpoolwork) releases a work object immediately if no callbacks remain; otherwise it is freed asynchronously after outstanding callbacks complete. Cleanup-group member closure can replace individual work-object closure.
6. [InitializeThreadpoolEnvironment](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-initializethreadpoolenvironment) documents default DLL-lifetime responsibility and environment controls including callback library, cleanup group, pool and priority.
7. [Thread Pools](https://learn.microsoft.com/windows/desktop/procthread/thread-pools) cautions that worker-thread TLS, COM registration, security context and priority must be restored after callbacks; the new pool does not automatically impersonate the submitter. It also warns to drain callbacks before unloading their DLL.

These are documented interface guarantees and cautions, **not** measured Windows scheduler ordering, fairness or latency guarantees.

## Architectural hypotheses (NOT VERIFIED)

- **H1:** A guest work object needs a submission count/queue plus distinct queued, executing, completed and canceled invocation identities, protected by lifetime references. A host pthread pool may provide execution capacity, but its queue and cancellation APIs do not themselves establish Windows callback semantics.
- **H2:** Closing a guest work handle should prevent new guest submissions while allowing already accepted callbacks to finish safely; actual object reclamation must wait for all relevant references. Verify close-versus-submit races against Windows rather than inventing a deterministic order.
- **H3:** Callback entry must establish a valid guest TEB/PEB, Microsoft x64 ABI and guest process identity. Thread-local guest error state, impersonation, activation context and DLL lifetime must not leak between unrelated callbacks. See R24/R25, R81, R91 and R125.
- **H4:** A cleanup group should coordinate member lifetime and callback cancellation; it must not be implemented as merely freeing a collection of host pointers.

## Falsifiable gates — PROPOSED, NOT RUN

| Case | Windows reference observation | LinuxNTAlpha acceptance condition |
| --- | --- | --- |
| Submit same work object N times with callbacks blocked on an event | Count started, concurrently running, completed and pending | Every accepted post accounted for exactly once; compare Windows concurrency behavior without demanding a fixed schedule |
| Cancel pending callbacks while one is executing | Which callbacks begin/finish; return of wait | No executing callback silently killed; cancellation and wait conform to reference |
| Close with callbacks outstanding | Whether callback still runs, object lifetime and crash safety | No premature free or callback into unloaded module |
| Two pools and cleanup groups | Callback membership, close/wait ordering | No cross-pool/group misattribution |
| Callback changes TLS, impersonation or thread priority | State visible to next callback on reused worker | Guest state follows documented thread semantics; application-owned cleanup remains application responsibility |
| Concurrent submit/close and callback-initiated cleanup | Error/failure behavior, hangs, lifetime | Define supported/undefined reference behavior first; prevent internal use-after-free |
| DLL unload during queued callbacks | Reference behavior and documented hazards | Do not claim universal safety; test library-pinning policy explicitly |

Record Windows build, compiler, exact callback timestamps/IDs, synchronization barriers, and outcomes. Use a deterministic gate for counts and lifetime, not scheduler ordering. Separate a reference test from a Linux prototype and from any performance comparison.

## Open questions

- Precisely which cleanup-group callback and disassociation transitions affect a work object's wait count? Inspect the respective Microsoft APIs and implement separate reference tests.
- What are the observable threadpool-worker TEB/TLS and activation-context invariants for native PE callbacks?
- Can a shared Linux worker pool satisfy guest priority, long-running-callback and per-pool isolation requirements without additional dedicated threads?
- What minimum subset of Vista+ threadpool APIs is required by the first trusted PE test cohort? Import inventory is needed before prioritizing implementation.

## Source verification scope

Microsoft Learn URLs above were inspected on 2026-11-17. Their contents are living documentation; no Windows binary version, Linux kernel revision, Wine source snapshot or runtime experiment was audited in this note.
