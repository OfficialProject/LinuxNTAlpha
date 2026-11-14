# R82 — NT keyed-event rendezvous versus Linux futex wake

Research date: 2026-11-14. **Source audit only; no experiment executed.** Scope: the keyed-event NT service family, distinct from R48 SRW/condition variables, R54 WaitOnAddress, and R75 wait-multiple.

## Evidence

**VERIFIED (ReactOS implementation, not Windows conformance):** At ReactOS commit `0db2415442da18342be8a834c3b97a2ea4be1740`, `ntoskrnl/ex/keyedevt.c` implements `NtCreateKeyedEvent`, `NtOpenKeyedEvent`, `NtWaitForKeyedEvent` and `NtReleaseKeyedEvent`. An event object contains 23 hash buckets with separate wait and release lists. Matching compares both the calling process identity and the pointer-sized key, within the keyed-event object. A release with no matching waiter itself queues and blocks until paired; a wait with no matching release likewise blocks. Both paths support an alertable flag and optional timeout. Each path checks two-byte key alignment and requires different object access rights (`KEYEDEVENT_WAIT` or `KEYEDEVENT_WAKE`). A null handle selects a default object named `\\KernelObjects\\CritSecOutOfMemoryEvent` in this implementation. Source: [ReactOS keyed-event implementation, pinned revision](https://github.com/reactos/reactos/blob/0db2415442da18342be8a834c3b97a2ea4be1740/ntoskrnl/ex/keyedevt.c).

**VERIFIED (Linux interface):** `FUTEX_WAIT` checks a **32-bit user-memory value** against an expected value and then atomically sleeps if equal. `FUTEX_WAKE` wakes up to a requested number of waiters at the futex address; it does **not** block until a matching waiter arrives. A futex operation may return on interruption or spuriously, requiring a surrounding state protocol. Source: [Linux futex(2) manual](https://man7.org/linux/man-pages/man2/futex.2.html), [FUTEX_WAIT manual](https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html).

**INFERENCE:** A direct one-to-one mapping of keyed-event release to `FUTEX_WAKE` is not sufficient: release-first rendezvous would be lost, and arbitrary pointer-sized keys need not refer to mapped, readable 32-bit futex words. A guest-owned table keyed by (keyed-event object identity, guest process identity, key value) plus per-waiter wake words could provide the missing rendezvous bookkeeping. This is an architecture candidate, not an implemented or validated design.

## Implementation boundaries and risks

1. **Identity and rights:** Resolve the guest handle to a typed object and validate wait versus wake rights before lookup. Distinct keyed-event objects with the same key must not cross-match; guest process identity must be stable across host threads.
2. **Linearization:** Under a per-bucket lock, atomically find/remove an opposite-side waiter or enqueue the caller. Each waiter needs a generation/state transition that cannot lose a wake between queue insertion and entering the Linux futex wait.
3. **Timeout and alertability:** Cancellation must race safely with a matching partner. Define exactly which side owns success when timeout and pairing overlap; integrate guest APC/alert semantics from R13 rather than assuming Linux EINTR equals NT alertable behavior.
4. **Lifecycle:** Object handle close, guest-thread exit, guest-process teardown and duplicate handles must not cause use-after-free or orphaned waiters. Closing one handle does not by itself prove the object has no remaining references.
5. **Security:** Never dereference the key as a host pointer merely to hash or compare it. Keep guest handle and process namespaces separate; account for memory/resource exhaustion through unbounded queued rendezvous attempts.
6. **Licensing:** ReactOS source is behavioral reference material only. No code is copied into LinuxNTAlpha; consult R21 before any proposed reuse.

## Falsifiable reference gates (NOT RUN)

- Windows reference: wait-first and release-first pairing, one-to-one completion, several identical keys, distinct keys, distinct keyed-event handles, and cross-process same-key cases.
- Validate odd-valued keys, null/default handle, invalid/wrong-access handles, zero and finite timeouts, alertable APC interruption, and status codes on a pinned Windows build.
- Stress concurrent timeout-versus-pair races and thread exit; require exactly one terminal outcome per operation and no stranded partner.
- Linux isolated prototype: randomized schedules with a modeled rendezvous oracle, ThreadSanitizer where applicable, and forced preemption at enqueue/pair/futex boundaries.
- Compare correctness before latency; collect distributions on matched hardware only after conformance.

## Open questions

- **OPEN:** Which of the ReactOS behaviors above precisely match supported Windows builds? ReactOS is not a normative Microsoft specification. Obtain Windows-reference traces before freezing semantics or exact NTSTATUS mapping.
- **OPEN:** Whether an existing synchronization backend (including ntsync) can implement this *two-sided* keyed rendezvous without additional guest-managed state. Do not infer coverage from support for ordinary NT events.
- **OPEN:** Which user-space NT runtime components actually require this API family, and whether higher-level compatibility can avoid exposing it in the first milestone.

**Decision:** Retain keyed events as an explicit NT-runtime synchronization gate, not as a simple futex alias. No claim of implementation, benchmark, or Windows compatibility.
