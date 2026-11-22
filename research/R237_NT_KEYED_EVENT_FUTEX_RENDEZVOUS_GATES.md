# R237 — NT keyed-event rendezvous versus Linux futex wait queues

Date: 2026-11-22. Status: **documentation/source review only**; no Windows reference run, Linux prototype, benchmark, or architecture selection.

## Why this is distinct

R48 covers SRW locks and condition variables, and the existing futex/ntsync studies cover other synchronization surfaces. This note isolates the **native keyed-event object and key-matched wait/release pair**, including the fact that a Linux futex is not itself a keyed-event object. A successful RtlWaitOnAddress or ordinary futex test must not be reported as keyed-event conformance.

## Evidence ledger

1. **Verified in third-party NT interface declarations, not a Microsoft public behavioral specification:** the PHNT header declares `NtCreateKeyedEvent`, `NtOpenKeyedEvent`, `NtWaitForKeyedEvent`, and `NtReleaseKeyedEvent`, with a handle, pointer-valued key, alertable flag, and optional LARGE_INTEGER timeout for wait/release. It declares distinct `KEYEDEVENT_WAIT` and `KEYEDEVENT_WAKE` access masks. This verifies the published declaration surface, **not** exact Windows kernel behavior or stability guarantees. [PHNT declarations](https://github.com/winsiderss/phnt/blob/master/ntexapi.h) (also [independent PHNT mirror](https://github.com/lucasg/processhacker2/blob/master/phnt/include/ntexapi.h)).
2. **Verified in independent compatibility source, not proof of Windows behavior:** ReactOS's NTDLL export specification lists the wait/release/open calls; its RTL critical-section code can fall back to `NtWaitForKeyedEvent(NULL, CriticalSection, FALSE, timeout)` when the lock semaphore cannot be created. This is evidence of a real compatibility dependency and the NULL-handle call pattern, but does **not** prove the Windows global-keyed-event contract. [ReactOS export spec](https://github.com/reactos/reactos/blob/master/dll/ntdll/def/ntdll.spec); [ReactOS critical.c](https://github.com/reactos/reactos/blob/master/sdk/lib/rtl/critical.c).
3. **Verified Linux host contract:** `FUTEX_WAIT` compares a 32-bit aligned memory word against an expected value and atomically sleeps only on a match; `FUTEX_WAKE` wakes up to N waiters at a futex address, with no guarantee of waiter order. The Linux futex key is tied to a host memory location, not automatically to an NT object handle plus arbitrary pointer-valued key. [futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html); [FUTEX_WAIT](https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html); [FUTEX_WAKE](https://man7.org/linux/man-pages/man2/FUTEX_WAKE.2const.html).
4. **Verified Linux host limitation:** a successful futex wake is not sufficient evidence that a particular logical rendezvous completed; futex documentation requires callers to recheck their user-space condition after wakeups. [FUTEX_WAIT](https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html).
5. **Independent implementation evidence, not an authoritative specification:** Wine's NTDLL synchronization source implements its own synchronization structures and documents the separate RtlWaitOnAddress pathway. Do not infer that all NT keyed-event semantics are identical to WaitOnAddress. [Wine sync.c](https://github.com/wine-mirror/wine/blob/master/dlls/ntdll/sync.c).

## Proposed architectural contract (hypothesis, not implementation)

Represent a keyed-event object by a guest-owned identity and access rights. Represent each operation by a distinct pending request carrying (object generation, full-width key, operation direction, guest thread identity, deadline, alertable state, terminal status). Maintain per-key queues under a lock; pair complementary wait and release operations at one linearization point. Host futex/eventfd/condition variables may be *sleep/wakeup mechanisms*, never the sole source of guest-visible pairing or status.

A NULL handle, if confirmed on target Windows versions, needs an explicitly scoped guest-default object identity. Do not silently equate all NULL handles across independent guest processes. Keep handle close, object destruction, and thread termination separate from key reuse. A key is an opaque guest value; never dereference it solely because it is pointer-typed. These are **design proposals** subject to differential testing.

## Critical unknowns and Windows reference gates

- **Pairing direction and blocking:** Does release block until a waiter arrives, and does wait block until release arrives? Exercise wait-first, release-first, same-key many-to-many, distinct-key isolation, and repeated keys; capture NTSTATUS and actual completion order. Do not assume FIFO.
- **Handle and rights:** Probe invalid handles, wrong object type, WAIT-only versus WAKE-only rights, named/opened aliases, duplicate handles, NULL handle, cross-process same key, and close while pending. Record OS build and process context.
- **Alertable waits:** Probe queued user APC before and during both directions, timeout races, and whether interruption consumes or preserves a pairing; compare alertable true/false and capture returned statuses.
- **Timeout:** Probe zero, negative relative, positive absolute, NULL infinite, boundary values, and clock adjustment; capture exact NTSTATUS and pairing state after timeout.
- **Cancellation and lifetime:** Probe waiting/releasing thread exit, process exit, object handle close, key reuse and subsequent operations. A timed-out request must never match a later counterpart.
- **Contention:** Use barriers and a deterministic schedule recorder; verify exactly-once matching, no lost partners, no duplicate completion and no cross-key delivery under parallel operations.
- **Security:** A malicious guest must not turn an arbitrary key into a host pointer dereference, wake another guest's queue, or reuse a retired object generation. Host futex addresses and internal queue state must not be guest-controlled.
- **Performance:** Only after correctness, compare guest-pair latency and contention throughput against Windows on equivalent hardware, with host backend variants and p50/p95/p99. No performance result exists yet.

## Implementation acceptance and falsification

First establish a version-pinned Windows-reference matrix. Then implement a model checker or deterministic scheduler for wait-first/release-first, timeout-versus-pair, APC-versus-pair, and teardown-versus-pair. Compare a minimal Linux backend against the reference; report PASS/FAIL/SKIP for each case. Reject a direct futex-address mapping if it cannot preserve guest object identity, full-width keys, access checks, timeout cancellation and exactly-once pairing. The host primitive remains undecided.

## Provenance caveat

Microsoft does not provide a complete publicly documented contract for these native calls in the sources reviewed here. PHNT, Wine and ReactOS are independently maintained source artifacts, **not Microsoft normative specifications**. No proprietary Windows source was used, no implementation was copied, and no experiment was run for this note.
