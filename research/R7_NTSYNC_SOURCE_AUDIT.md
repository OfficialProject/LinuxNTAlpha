# R7 — Linux ntsync source audit
Date: 2026-11-12
Primary source: https://github.com/torvalds/linux/blob/master/drivers/misc/ntsync.c
Documentation: https://www.kernel.org/doc/html/latest/userspace-api/ntsync.html
Status: source-level investigation, not benchmarked.

## Findings from upstream source
- Wait-all synchronization is serialized using an instance-wide wait_all_lock mutex, alongside per-object locks.
- An all_hint atomic indicator allows a fast path to avoid checking wait-all wakeups when no wait-all waiters are present.
- A wait queue uses an atomic signaled state and per-object queue entries; the code explicitly protects atomic multi-object acquisition.
- These mechanisms are engineered tradeoffs: wait-all correctness requires coordination, while minimizing lock contention for the common wait-any case.

## Performance implications (hypotheses)
- Workloads with frequent multi-object waits may encounter instance-level lock contention; measure under concurrent wait-all.
- The hint fast path likely limits overhead for workloads that rarely use wait-all.
- Device and ioctl overhead should be compared with an equivalent user-space NT service; a futex-only benchmark is not semantically equivalent to wait-all.

## Correctness and security investigations
- Trace lock acquisition order, deadlock avoidance and lifetime of object descriptors.
- Inspect ownership/abandonment and process exit interactions.
- Audit wait-all array validation, duplicate handling, and object instance separation.
- Verify whether implementation details differ across tagged kernel versions; do not assume master is stable ABI.

## Decision
Do not fork ntsync or propose a rewrite without reproducible evidence. Use upstream API as initial candidate.
