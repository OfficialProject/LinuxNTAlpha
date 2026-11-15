# R97 — WaitOnAddress versus Linux futex: address identity, wake semantics and teardown gates

**Status (2026-11-15):** documentation-based architectural analysis; **no experiment executed**. This is distinct from R48 (SRW locks and condition variables), R75 (wait-multiple object semantics), and R95 (recursive critical sections). It examines the lower-level address wait/wake primitive itself.

## Primary-source contracts

1. Microsoft documents `WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds)` as waiting while the bytes at an address match the supplied comparison value. Supported sizes are **1, 2, 4, or 8 bytes**. It returns nonzero on success and zero on failure; timeout is `ERROR_TIMEOUT`. The documentation explicitly warns that a thread may return **before the address changes**, including low-memory, abandoned wake, or checked-value scenarios, and callers must recheck the predicate. [WaitOnAddress](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress)
2. `WakeByAddressSingle` wakes **one** waiter on an address, if any; `WakeByAddressAll` wakes all. These APIs take an address rather than an NT dispatcher-object handle. [WakeByAddressSingle](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddresssingle), [WakeByAddressAll](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddressall)
3. Linux `futex(2)` describes a futex word as an aligned **four-byte integer**; `FUTEX_WAIT` atomically compares that word to an expected value and sleeps only if equal, with `EAGAIN` if the value differs. Futex wake targets the futex key selected by the address plus sharing mode. [futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html), [futex(7)](https://man7.org/linux/man-pages/man7/futex.7.html)
4. Linux `FUTEX_PRIVATE_FLAG` is for synchronization private to one process; shared futexes require the correct shared-key behavior. This distinction matters even when two guest mappings have identical numeric addresses. [futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html)

## Verified deductions from those contracts

- **A raw futex call cannot directly represent the full Windows API:** Linux's basic futex compare operates on a 32-bit aligned word, whereas Windows accepts 1/2/4/8-byte comparisons. An implementation needs a guest-aware wait registry or another carefully proven strategy for 1-, 2-, and 8-byte values, including unaligned-but-valid Windows addresses where permitted by the Windows contract.
- **No persistent event state is implied by a wake.** A wake without a waiter need not be remembered. The caller's predicate loop, not the wake call, decides whether progress is possible.
- **Check-and-park must be race-safe.** Reading guest memory, then separately registering/sleeping without a protocol permits a lost wake. A compatibility layer must establish a linearization rule across the final comparison, waiter registration, writer's update, and wake.
- **Guest address identity must not be confused with a host futex key.** Remapping, unmapping, and cross-process guest memory can invalidate naïve address-only registries. A design must define whether the Windows primitive is process-local and reject or separately model unsupported cross-process use rather than silently reusing host keys.
- **Spurious/early return is part of the caller contract.** The implementation may allow early successful wakeups; it must not promise that the compared bytes changed.

## Architecture hypothesis — not validated

For the first native PE runtime, use a **process-scoped address-wait service** with explicit waiter records keyed by guest address plus a mapping-generation/lifetime identity, and a protected compare/register/park protocol. An optimized direct futex path for aligned 4-byte cases may follow *only after* differential tests prove no lost wakes under concurrent stores and address reuse. The registry itself does not automatically solve atomicity: a writer can store outside its lock; the wake and recheck protocol must be proven with a defined memory-ordering contract.

Do **not** treat this as a claim that Windows uses a particular internal hash table, or that the proposed design outperforms futex. No Windows internal implementation has been inspected.

## Required conformance gates (proposals, not test results)

1. **Windows reference:** compare-equal versus unequal at each of 1/2/4/8 bytes; immediate timeout, finite timeout, infinite wait; inspect return and `GetLastError` only on failure.
2. **Wake behavior:** zero, one, and many waiters; single/all wake; wake before wait; repeated wake; loop around unchanged predicate and tolerate early return.
3. **Race stress:** barrier-controlled store-before-wake, wake-before-registration, compare-changing-during-registration, rapid timeout/wake contention, and high iteration counts with a watchdog. Record hangs separately from assertion failures.
4. **Memory and identity:** adjacent byte/word addresses, aligned versus unaligned values, mapped/unmapped/remapped address reuse, multiple private allocations, and protection failures; do not assume behavior not stated by documentation.
5. **Cross-process scope:** explicitly probe shared mapped pages on Windows to establish whether wake matching is cross-process; test Linux shared/private futex behavior independently. **Do not infer Windows cross-process support from Linux futex capabilities.**
6. **PE/ABI integration:** call the independently implemented exports from a Windows-target PE fixture, preserve x64 ABI, ensure Linux-host threads cannot accidentally join guest wait queues, and verify cleanup when the owning guest thread exits.

## Decision gate

Do not select a futex-only implementation until 1/2/8-byte waits, guest mapping lifetime, and wake-registration races have an independently reproducible correctness argument. Prioritize a Windows reference oracle over performance microbenchmarks. A benchmark must distinguish uncontended fast paths, contended wake latency, timeout behavior, and CPU consumption, with explicit hardware/kernel versions.

## Source provenance

Primary contracts: Microsoft Learn Win32 API references linked above; Linux futex(2)/(7) manual pages linked above. This note is a synthesis of published API contracts, not a source-code audit, Windows execution trace, or completed compatibility test.
