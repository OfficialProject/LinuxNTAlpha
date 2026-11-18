# R166 — NT keyed-event rendezvous versus Linux futex wake semantics

Status: **documentation/source review only (2026-11-18)**. No Windows reference probe, Linux prototype, benchmark, or application compatibility test was executed. This note is independent of R54 (WaitOnAddress) and R161 (critical sections).

## Evidence and confidence

1. **Source-verified, implementation evidence:** Wine 10.0's NTDLL Unix implementation uses separate `SELECT_KEYED_EVENT_WAIT` and `SELECT_KEYED_EVENT_RELEASE` operations, routes them through `server_wait`, substitutes its default `keyed_event` when the supplied handle is null, rejects odd keys with `STATUS_INVALID_PARAMETER_1`, and forwards alertable/timeout arguments. This establishes *Wine's implementation*, not an independently verified Windows kernel contract. [W1]
2. **Secondary interface evidence, not Microsoft normative documentation:** System Informer's phnt-derived NtDoc describes keyed events as matching one waiter and one releaser by object/key, with the release side able to block until a counterpart arrives; the key is an opaque pointer-sized identifier, not dereferenced. Its claim of same-process matching and exact error behavior requires Windows reference testing. [N1, N2]
3. **Linux primary interface evidence:** `FUTEX_WAIT` atomically compares a **32-bit word at a user address** with an expected value before sleeping; `FUTEX_WAKE` wakes up to a requested count and does not guarantee which waiter is selected. Futex waits may require rechecking a predicate after wake. [L1, L2, L3]
4. **Microsoft primary interface evidence (contrast, not keyed-event documentation):** Windows auto-reset events can retain a signaled state when no thread is waiting. This is not evidence that keyed-event releases retain a credit. [M1]

## Architectural distinction

**Inference:** Mapping a keyed event directly to `futex(key)` is not a faithful general design: the guest key is an opaque identity, potentially not mapped or readable, while Linux futexes require a valid aligned futex-word address and expected 32-bit value. A keyed release may itself wait for a matching waiter, whereas `FUTEX_WAKE` simply returns the count awakened. Furthermore, a guest handle and process namespace must participate in matching, not merely the numeric guest key. Never dereference a guest key as a host pointer merely to construct the backend.

**Candidate design, not validated:** Maintain a per-keyed-object, per-guest-process rendezvous table indexed by `(object identity, process identity, pointer-sized key)`. Queue wait-side and release-side operations separately, match them under one lock, transfer completion ownership exactly once, then wake each matched operation through dedicated host wait primitives (possibly futexes on runtime-owned state words). Track request generation, timeout, APC/alertable interruption, cancellation, handle closure, and object lifetime. A queue and runtime-owned futex words are a proposed mechanism, not a verified replacement for Windows scheduling or return-status behavior.

**Security boundary:** Validate guest handle access rights and object type before queue admission. Treat guest keys as untrusted integers; isolate guest processes even if host memory or numeric keys overlap. Bound queue allocations; ensure teardown cannot reuse a request node before all wakeups retire. Do not call a raw Linux futex with an arbitrary guest pointer.

## Windows-reference and Linux acceptance gates (all unexecuted)

- Create/open named and unnamed keyed objects; compare object identity and handle-right enforcement; verify null-handle fallback on each target Windows build.
- For each key, test wait-first, release-first, concurrent many-to-one, one-to-many, two distinct keys, two objects with identical keys, and two processes with identical keys. Capture which operations block and exactly how many complete.
- Test zero key, odd key, arbitrary aligned but unmapped key, reused virtual address, and key values above 32 bits on x64. Record NTSTATUS and parameter precedence; do not infer status solely from Wine.
- Test relative and absolute deadlines, zero timeout, alertable APC interruption on either half, nonalertable waits, handle close during an in-flight operation, and process/thread termination. Record return statuses and whether counterpart operations remain pending.
- In a separate Linux model test, inject wake-before-sleep, timeout-vs-match, cancel-vs-match, object-destroy-vs-wake, and key-reuse races. Assert no lost match, double completion, dangling waiter, or cross-process wake.
- Only after matching Windows reference behavior, profile uncontended and contended paths. Compare with a runtime-owned futex backend and any alternative; do not claim a performance advantage beforehand.

## Open questions

- Exact Windows-version-specific pairing order, fairness, status precedence, and APC behavior remain **unknown**.
- Whether a keyed event is needed for the first minimal NT runtime milestone is **undecided**; prioritize only if target imports or runtime dependencies demonstrate demand.
- Wine's source may differ across versions. Pin implementation version for future comparisons; do not transplant LGPL code without the R21 license decision.

## Sources

- [W1] Wine 10.0 source, `dlls/ntdll/unix/sync.c`, Debian packaged source: https://sources.debian.org/src/wine/10.0~repack-6/dlls/ntdll/unix/sync.c/
- [N1] System Informer/phnt-derived NtDoc, NtWaitForKeyedEvent: https://ntdoc.m417z.com/ntwaitforkeyedevent
- [N2] System Informer/phnt-derived NtDoc, NtReleaseKeyedEvent: https://ntdoc.m417z.com/ntreleasekeyedevent
- [L1] Linux futex(2): https://man7.org/linux/man-pages/man2/futex.2.html
- [L2] Linux FUTEX_WAIT(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html
- [L3] Linux FUTEX_WAKE(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAKE.2const.html
- [M1] Microsoft, Event Objects (Synchronization): https://learn.microsoft.com/en-us/windows/win32/sync/event-objects
