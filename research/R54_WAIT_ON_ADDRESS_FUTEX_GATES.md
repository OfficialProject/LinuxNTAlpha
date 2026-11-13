# R54 — WaitOnAddress and Linux futex: address-wait conformance gates

Status: **DOCUMENTATION-BASED RESEARCH; NO EXPERIMENT EXECUTED**  
Date: 2026-11-13  
Scope: Windows 8+ `WaitOnAddress`, `WakeByAddressSingle`, `WakeByAddressAll` and a prospective Linux futex backend. This note does not claim that an NT runtime or compatible implementation exists.

## Why this is a separate gate

R48 addresses SRW locks and condition variables, while R3/R6/R7/R18 concern NT synchronization objects and ntsync. Address-based waits are **not** ordinary NT object handles: their public interface accepts a process-local address and comparison bytes. Therefore neither a handle-based ntsync implementation nor a Linux futex call is, by itself, evidence of Windows API conformance.

## Source-grounded contracts

**VERIFIED FROM PUBLISHED API CONTRACTS (not locally tested):**

1. `WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds)` compares the bytes at `Address` against `CompareAddress` and waits while equal; documented supported comparison sizes are **1, 2, 4 or 8 bytes**. The API can return because of a wake, a value change, or a spurious wake. Callers must recheck their predicate. It returns nonzero on success, zero on failure; `GetLastError` is the documented error channel. [Microsoft WaitOnAddress](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress).
2. `WakeByAddressSingle` and `WakeByAddressAll` target threads waiting on an address in the calling process. They do **not** take a kernel object handle and do not imply cross-process wakeups. [Microsoft WakeByAddressSingle](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddresssingle), [WakeByAddressAll](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddressall).
3. Linux `futex(2)` provides an atomic compare-and-block operation for a **32-bit aligned futex word**; its ordinary WAIT operation compares a 32-bit value, unlike the Windows API's 1/2/4/8-byte comparison sizes. Private futex operations can avoid interprocess sharing overhead. [Linux futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html), [Linux futex(7)](https://man7.org/linux/man-pages/man7/futex.7.html).
4. `FUTEX_WAIT` may return for reasons other than a matching wake, including signals; the user-space predicate must be checked again. Linux futex errno values are **not** Windows last-error codes. [Linux futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html).

**Important source qualification:** URLs identify primary Windows API contracts and Linux manual documentation; this research run did not execute a Windows-reference test or validate live page revisions. API behavior must be version-pinned before implementation.

## Architectural mismatch and proposed approach

**HYPOTHESIS:** A runtime-private wait registry keyed by process address, with a generation/sequence futex word per wait bucket, can accommodate 1-, 2-, 4- and 8-byte comparisons without incorrectly reading an arbitrary Windows application address as a Linux 32-bit futex. A waiter would compare the requested byte width, record a bucket sequence, recheck the application value, and then sleep on the sequence. A waker increments the sequence and wakes a subset/all waiters for the exact address.

**OPEN correctness risks:**

- **Lost wakeup:** A naive compare-then-enqueue-then-futex-wait sequence can miss a wake between its comparison and sleeping. A precise registration and sequence protocol is necessary; simply wrapping `FUTEX_WAIT` around the application's address is insufficient.
- **WakeSingle targeting:** Hash collisions must not wake the wrong address or permanently starve eligible waiters. A shared bucket wake-all is not equivalent to selecting one eligible waiter, even if it happens to be functionally safe for some clients.
- **Address lifetime and reuse:** Unmap/remap and address reuse can cause ABA-style aliasing. Documented caller lifetime requirements must be honored; a runtime cannot promise safe concurrent deallocation of a waited-on address.
- **Comparison width and alignment:** Windows-supported 1/2/8-byte sizes must not be silently narrowed to a 4-byte futex word. Unaligned and page-boundary accesses need a documented reference baseline rather than speculative support.
- **Timeout and clock behavior:** Translate millisecond timeout, zero timeout and `INFINITE` carefully; retry after interrupts using a deadline rather than resetting the entire relative timeout. Check which Linux clock/operation is actually used.
- **Ordering:** Wake operations are not a substitute for application-level atomic publication or memory ordering. A test that merely returns after a wake does not establish visibility guarantees.
- **Cancellation and signals:** Linux `EINTR` or `EAGAIN` must not leak as raw errno into the Windows ABI; the Windows-visible result must be established against reference behavior.

## Proposed differential tests — NOT EXECUTED

On a version-recorded Windows x64 reference host and a Linux x64 prototype, record return values, `GetLastError`, observed wait state, timeouts, and thread scheduling outcomes:

1. Sizes 1/2/4/8: initially equal; value changes before sleep, while sleeping, and after waking.
2. Mismatch on entry, zero timeout, finite timeout and `INFINITE`; repeat at least 1,000 times with controlled synchronization.
3. Two addresses deliberately sharing one registry bucket; `WakeByAddressSingle` and `WakeByAddressAll` with multiple waiters.
4. A wake racing waiter registration, using a deterministic test barrier rather than relying only on stress.
5. Repeated spurious wakeups, interrupted Linux waits and timeout near expiry.
6. Pointer and size validation, page-boundary values, and unsupported sizes (Windows reference first; avoid intentionally dereferencing inaccessible memory in-process without isolation).
7. High-contention throughput and wake latency against a Windows baseline, Linux native futex baseline, and a simple mutex/condition-variable implementation, with CPU, kernel, build and affinity recorded.

## Implementation decision gate

Do **not** select a futex-only or ntsync-only backend based on nominal API similarity. Require: (a) a proof or reproducible adversarial test for no lost wakeups; (b) exact-address wake targeting and size coverage; (c) Windows-reference error/timeout conformance; (d) memory-safety and address-lifetime review; and (e) performance measurements with variance and overhead attribution.

No Windows test, Linux prototype, benchmark, source import or architecture decision was performed in R54.
