# R161 — Win32 critical-section recursion, ownership, spin policy and lifetime gates

Date: 2026-11-18. Evidence: Microsoft primary API documentation inspected on this date. **Documentation research only: no Windows reference run, Linux prototype, benchmark or implementation is claimed.**

## Boundary and nonduplication

R48 covers SRW locks and condition variables; R54 covers address waits. A Win32 `CRITICAL_SECTION` is a separate, process-local, *recursive* ownership primitive with application-allocated storage, unlike an SRW lock or a named NT mutex. This note specifies the missing critical-section contract rather than reusing either primitive's assumptions.

## VERIFIED — documented API contracts

1. `EnterCriticalSection` blocks until ownership is acquired; `TryEnterCriticalSection` returns immediately and succeeds when the caller already owns the section. The owner can enter recursively, and must leave once per successful entry. No waiting-thread acquisition order is guaranteed. [Microsoft: EnterCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-entercriticalsection); [TryEnterCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-tryentercriticalsection).
2. `LeaveCriticalSection` by a nonowner is erroneous and may leave another waiter blocked indefinitely. This is **not** a documented recoverable error-code path. [Microsoft: LeaveCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-leavecriticalsection).
3. `InitializeCriticalSectionEx` returns BOOL, accepts zero or `CRITICAL_SECTION_NO_DEBUG_INFO` as flags, and sets a spin count; on a uniprocessor the spin count is ignored/zero. `SetCriticalSectionSpinCount` returns the *previous* spin count. These are policy knobs, not guarantees of busy-wait duration or a throughput improvement. [Microsoft: InitializeCriticalSectionEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializecriticalsectionex); [SetCriticalSectionSpinCount](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setcriticalsectionspincount).
4. A `CRITICAL_SECTION` cannot be copied or moved and must be treated as logically opaque. Initializing an already initialized object is undefined; delete before reinitializing. [Microsoft: InitializeCriticalSectionEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializecriticalsectionex).
5. `DeleteCriticalSection` requires the section to be unowned and not concurrently accessed. Using it after deletion, or deleting while owned, is outside the documented safe contract. Owner-thread termination likewise leaves section state undefined. [Microsoft: DeleteCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-deletecriticalsection); [EnterCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-entercriticalsection).
6. `EnterCriticalSection` documents a possible `STATUS_POSSIBLE_DEADLOCK` exception on a timed-out wait, controlled by a registry setting, and a special process-exit behavior: an entry that would block can terminate the exiting process. Neither should be silently modeled as an ordinary futex timeout. [Microsoft: EnterCriticalSection](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-entercriticalsection).

## HYPOTHESIS — candidate Linux-hosted implementation

A guest-compatible implementation may use an owner guest-thread identity, recursion depth, contention state and a futex-backed parking generation, with an uncontended user-space fast path. This is an **architecture proposal**, not a verified equivalence. Atomic state transitions must prevent a lost wake between observing contention and sleeping. Guest identity must not be confused with a reusable Linux TID. Host `pthread_mutex_t` must not be substituted blindly: its binary layout, recursion policy, teardown rules and error reporting are not a documented Win32 ABI.

The in-memory `CRITICAL_SECTION` ABI remains an **OPEN** issue: applications may allocate the structure directly, and Windows-target code may observe its layout even though Microsoft instructs callers to treat it as opaque. A side table keyed only by guest address risks stale-object aliasing after delete/reinitialize or memory reuse; generation and lifetime handling need a proof or targeted test. Do not infer undocumented internal field values from the API pages.

## Proposed conformance and safety gates — NOT RUN

- On a pinned Windows build, initialize, enter twice, try-enter as owner, verify a competing thread cannot enter, leave once (still blocked), then leave again (waiter proceeds). Record exact order only for controlled handshakes, not as a fairness guarantee.
- Verify uncontended `TryEnterCriticalSection`, contended immediate failure, and `SetCriticalSectionSpinCount` previous-value return. Run single-core and multi-core cases separately; record OS build and CPU topology.
- Test independent objects, repeated valid delete/reinitialize, and concurrent high-contention entry/leave with a bounded watchdog. **Do not** include use-after-delete, nonowner leave, double initialization or owner death as normative conformance expectations; run such cases only in disposable isolated processes for robustness and classify results as undefined-behavior observations.
- Exercise the candidate guest implementation with a deterministic scheduler/model checker around ownership publication, waiter enqueue, wake, generation rollover and thread identity reuse. Validate that no waiter sleeps past a completed release.
- Separate correctness from performance: compare fast-path cost, contended wake latency, CPU consumed by spin policy and p50/p99 distributions against a Windows baseline on matched hardware. No speedup is currently established.

## OPEN questions

- Exact guest `RTL_CRITICAL_SECTION` ABI layout and version-specific observable fields require a pinned Windows SDK/NTDLL source or differential evidence; do not fabricate field contracts.
- How should guest thread termination and process teardown interact with a stuck critical section without introducing host-service deadlock? The public contract leaves owner death undefined.
- Which exception/termination semantics must be implemented for the first compatibility target, and what is the precise OS-version policy?
- Does a userspace-only implementation suffice for intended workloads, or is any kernel intervention justified by reproducible contention measurements?

**Decision:** no Linux-kernel change justified by these sources. First establish reference behavior and safe user-space ownership/lifetime handling.