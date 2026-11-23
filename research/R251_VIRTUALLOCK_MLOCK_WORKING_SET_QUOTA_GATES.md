# R251 — VirtualLock/VirtualUnlock and working-set quotas versus Linux mlock

Evidence review: 2026-11-22. Status: **documentation only; no experiments performed**. This extends R250 (working-set inspection), R247 (guest VM provenance), and R248 (large pages); it does not replace them.

## Verified primary contracts

1. Microsoft **VirtualLock** covers every page touched by the byte range, requires all pages committed, rejects PAGE_NOACCESS, and keeps successfully locked pages resident until unlock or process exit. The documented lockable-page ceiling is tied to the process **minimum working set minus overhead**; increasing the minimum/maximum via SetProcessWorkingSetSize may be necessary. VirtualLock does **not** increment a per-page lock count. Source: [VirtualLock](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtuallock).
2. Microsoft **VirtualUnlock** accepts a subrange different from the original VirtualLock range provided all covered pages are locked. If any covered page is not locked, it returns FALSE with ERROR_NOT_LOCKED and removes those pages from the working set; this failure side effect matters. Source: [VirtualUnlock](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualunlock).
3. **SetProcessWorkingSetSizeEx** operates on a process handle requiring PROCESS_SET_QUOTA; minimum/maximum sizes govern Windows working-set management, and hard-min/hard-max flags change enforcement. Raising sizes can require SeIncreaseWorkingSetPrivilege. An ordinary working-set limit is **not** a Linux locked-memory quota. Sources: [SetProcessWorkingSetSizeEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-setprocessworkingsetsizeex), [GetProcessWorkingSetSizeEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-getprocessworkingsetsizeex).
4. Linux **mlock/munlock** prevent swapping of mapped pages, round ranges to page boundaries on Linux, and locks do not stack. mlock2(MLOCK_ONFAULT) is intentionally *not* equivalent to VirtualLock's immediate residency guarantee. Linux lock limits depend on RLIMIT_MEMLOCK and CAP_IPC_LOCK; fork does not inherit memory locks, exec removes them, and unmapping removes the lock. Source: [mlock(2)](https://man7.org/linux/man-pages/man2/mlock.2.html).
5. Microsoft **QueryWorkingSetEx** reports a Locked field when the entry is valid; do not confuse this with Linux mincore residency or with the process-wide working-set policy. Sources: [PSAPI_WORKING_SET_EX_BLOCK](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-psapi_working_set_ex_block), [QueryWorkingSetEx](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-queryworkingsetex).

## Architectural implications (HYPOTHESES, not selected design)

- Maintain a **guest page-lock ledger** keyed by guest process and mapping generation, separate from host VMAs and Linux fd ownership. A successful guest VirtualLock could validate guest commitment/protection/quota first, then use host mlock with rollback on failure. The exact NT page-level locking rules under concurrent decommit/remap remain **OPEN**.
- Guest minimum/maximum working-set policy and privilege checks require an NT-facing authority layer. Blindly changing RLIMIT_MEMLOCK or granting CAP_IPC_LOCK would either misreport Windows semantics or enlarge host privilege.
- A Linux host may deny mlock under resource limits even when guest policy would permit the operation. **OPEN:** whether a restricted runtime should fail conservatively, reserve host capacity beforehand, or negotiate a capability; never silently claim a successful lock without satisfying the promised residency.
- For VirtualUnlock on partially unlocked ranges, Linux munlock alone cannot synthesize Windows ERROR_NOT_LOCKED and working-set trimming side effects. An explicit guest ledger and a separately tested eviction mechanism may be necessary. **OPEN:** whether any safe host primitive reproduces trimming exactly; no equivalence asserted.
- Host mlock may pin memory shared with other guest mappings. Guest lock ownership, quotas, and cleanup must not be inferred from aggregate host lock counters.

## Proposed conformance gates — **NOT RUN**

**W1 Windows reference:** Lock a committed read/write range spanning a page boundary, then unlock a strict subrange; capture success, GetLastError, QueryWorkingSetEx Locked and residency before/after.

**W2 Windows reference:** Probe PAGE_NOACCESS, reserved/uncommitted pages, zero length, partially invalid ranges and repeated lock/unlock. Record precise outcomes by Windows build, not by assumption.

**W3 Windows reference:** Probe mixed locked/unlocked VirtualUnlock and verify ERROR_NOT_LOCKED plus the documented working-set removal side effect; compare QueryWorkingSetEx snapshots.

**W4 Windows reference:** Change working-set minimum/maximum and hard flags with/without privileges; test lock ceiling and error behavior on a disposable test process. Do not exhaust system RAM.

**L1 Linux host:** Record kernel version, page size, RLIMIT_MEMLOCK, capability status, mlock/munlock results, /proc/self/status VmLck and mincore observations. Test mlock2(MLOCK_ONFAULT) separately, not as an interchangeable backend.

**A1 architecture/security:** Run concurrent map/unmap/decommit, worker crash and process termination with guest mapping generations; assert no stale lock ledger, cross-guest quota bypass, unauthorized privilege escalation or false successful residency.

## Decision impact

This is an additional **ADR-003 architecture-selection gate**: native PE execution and Linux page locking do not establish guest NT working-set compatibility. No benchmark, Windows differential result, Linux probe, kernel change, or compatibility claim is recorded here.
