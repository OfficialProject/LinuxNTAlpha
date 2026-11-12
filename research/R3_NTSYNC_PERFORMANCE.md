# R3 — NTSYNC semantics and performance experiment plan
Date: 2026-11-12. No performance measurements yet.

## Primary source
https://docs.kernel.org/userspace-api/ntsync.html

## Verified
NTSYNC exposes /dev/ntsync and ioctl operations for semaphores, recursive mutexes and auto/manual-reset events. Its wait-any/wait-all semantics include atomic acquisition, ownership and abandoned mutex handling. Each NT instance and its objects are represented by file descriptions. It is intended as an NT compatibility mechanism, not a replacement for general Linux synchronization.

## Performance claims requiring measurement
- Kernel-level atomic multi-object waits could avoid repeated userspace round trips compared with a userspace service.
- ioctl and file descriptor management can have costs; uncontended and highly contended workloads may behave differently.
- A semantic mismatch makes any speed comparison invalid even if the benchmark is faster.

## Benchmark matrix
- uncontended mutex acquire/release; contended 2/4/8/16 threads
- manual/auto-reset event wakeups; semaphore release/acquire
- wait-any and wait-all with 1/2/8/32/64 objects, where supported
- thread wake latency p50/p95/p99/p99.9; CPU cycles, voluntary/involuntary switches
- throughput, RSS/PSS, kernel CPU, lock contention and scheduler delay
- compare only semantically equivalent behavior against userspace service, eventfd/futex implementations and Wine baseline where applicable

## Experimental controls
Pin CPU topology and governor; record kernel/config, CPU, microcode, compiler, workload, memory pressure, thermal state and mitigations. Warm up, randomize test order, run multiple independent repetitions, publish raw results and uncertainty intervals. Never compare different synchronization semantics as though equivalent.

## Risks
NTSYNC availability and permissions vary by kernel and distribution; feature-detect rather than assume. Device interface is Linux-specific. Security audit must include object ownership, fd lifetime, malformed ioctl inputs, denial-of-service and process isolation.

## Decision gate
Choose NTSYNC only for operations where compatibility correctness and benchmark results justify it. Kernel patching is not automatically faster.
