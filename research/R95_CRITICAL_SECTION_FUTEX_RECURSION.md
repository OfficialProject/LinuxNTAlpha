# R95 — Windows critical sections versus Linux futex ownership and recursion

Status: **primary-source research only** (2026-11-15). No implementation, Windows differential run, Linux benchmark, or PE execution was performed for this note.

## Scope and separation from existing work

R48 covers SRW locks/condition variables; R54 covers WaitOnAddress; R75 covers multi-object waits. This note isolates **Win32 CRITICAL_SECTION**, which is neither an NT dispatcher mutex nor an SRW lock. Its recursive owner semantics and process-local lifetime require a separate compatibility contract.

## VERIFIED — documented Windows contract

1. A critical section synchronizes threads **within one process**, not across processes. The application allocates and initializes the CRITICAL_SECTION storage. A mutex object is the documented cross-process alternative. [Microsoft: Critical Section Objects](https://learn.microsoft.com/en-us/windows/win32/sync/critical-section-objects).
2. **Recursive acquisition is supported**: the owning thread may enter repeatedly, including via TryEnterCriticalSection, and must leave once per successful entry. TryEnterCriticalSection does not block; EnterCriticalSection blocks while another thread owns the object. There is no guaranteed waiter acquisition order. [Microsoft: EnterCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-entercriticalsection).
3. Unlike an NT mutex, a critical section does **not** report abandoned ownership. If its owner terminates, its state is undefined; deleting while owned also leaves waiting threads in an undefined state. Do not invent recovery or abandonment behavior. [Microsoft: Critical Section Objects](https://learn.microsoft.com/en-us/windows/win32/sync/critical-section-objects); [Microsoft: EnterCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-entercriticalsection).
4. InitializeCriticalSectionAndSpinCount accepts a spin count, ignored on a single-processor system; on multiprocessor systems it may spin before waiting. Its documented return behavior differs historically: on Windows Vista and later it always returns nonzero. A critical section must not be copied or moved, nor reinitialized before deletion. [Microsoft: InitializeCriticalSectionAndSpinCount](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializecriticalsectionandspincount).
5. Deletion requires an unowned object with no concurrent users; further use other than reinitialization is invalid. [Microsoft: DeleteCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-deletecriticalsection).
6. EnterCriticalSection can raise STATUS_POSSIBLE_DEADLOCK under the documented system-configured critical-section timeout. During process exit, an attempt to enter that would block instead terminates the process. These are special lifecycle cases, not ordinary timed-lock APIs. [Microsoft: EnterCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-entercriticalsection).

## VERIFIED — Linux primitive boundary

Linux FUTEX_WAIT operates on a **32-bit futex word** and atomically checks an expected value before sleeping, preventing a particular lost-wakeup race. FUTEX_WAKE wakes waiters; the futex interface itself does not supply a Windows CRITICAL_SECTION recursion counter, Windows guest thread identity, spin-count policy, or the Win32 API lifecycle. [Linux futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html); [Linux FUTEX_WAIT(2const)](https://man7.org/linux/man-pages/man2/futex_wait.2const.html).

## HYPOTHESIS — implementation candidate, not selected

A process-local userspace structure with atomic lock state, **guest-thread owner identity**, recursion depth, and a futex-based contended slow path could reproduce the ordinary enter/try/leave behavior. The implementation must preserve the guest-visible CRITICAL_SECTION ABI and its address identity; a host pthread mutex substitution is not automatically ABI- or semantics-equivalent. Spin tuning is a performance question, not a compatibility shortcut. Avoid exposing an internal Linux futex word as the entire Windows structure without a proven layout contract.

## OPEN — conformance gates

1. **Windows reference:** initialize; enter twice on thread A; try-enter from A succeeds; try-enter from B fails; leave three times; B can acquire. Record results and timeouts without assuming fairness.
2. **Contention:** N workers repeatedly increment a protected counter with a barrier; compare spin counts 0 and nonzero, report correctness separately from latency distributions. Repeat on a single logical CPU and multiple CPUs.
3. **ABI:** compile a Windows-target PE fixture against authoritative target headers, verify size/alignment and calls across the MS-ABI boundary; do not hardcode a structure layout from recollection.
4. **Lifecycle:** test only documented legal delete/reinitialize sequences in-process. Run owner termination, deletion while owned, and exit-time contention **only in isolated disposable Windows processes**; their undefined behavior must not be turned into a conformance assertion.
5. **Identity:** verify guest thread IDs remain stable across the chosen Linux thread mapping; evaluate migration/fibers separately (R35). Ensure recursion checks cannot confuse a recycled guest ID with a live owner.
6. **Lost wakeups:** instrument the transition between failed acquisition, waiter registration, FUTEX_WAIT expected-value check, unlock, and wake. Prove state transitions before claiming correctness.
7. **Exceptions:** record STATUS_POSSIBLE_DEADLOCK behavior only on a controlled Windows reference, without altering machine-wide registry policy in an unattended test.

## Architectural implication

Treat critical sections as a **process-local synchronization subsystem**, not as a kernel dispatcher object or an ntsync requirement. First deliver recursive correctness, guest ABI, and lifecycle tests; then compare futex-backed implementations against native Windows on equivalent hardware. This is a candidate boundary, **not** evidence that a Linux-backed implementation is faster.

## Evidence notes

Sources above are primary Microsoft API documentation and Linux man-pages documentation consulted 2026-11-15. No upstream source revision was pinned and no experiment was executed. Windows-version-specific behavior must be validated against the target support matrix before coding.
