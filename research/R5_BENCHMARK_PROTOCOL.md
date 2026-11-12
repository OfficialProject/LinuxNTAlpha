# R5 — Performance and correctness protocol
Date: 2026-11-12. No measured outcomes reported.

## Goals
Demonstrate performance rather than assume it: startup latency, memory, scheduling/wake-up latency, syscall/IPC overhead, input-to-photon latency, game frame-time distribution and compatibility.

## Comparators
1. Reference Windows 11 on identical hardware.
2. Upstream Linux with established Wine/Proton path when applicable.
3. LinuxNTAlpha prototype on same kernel/hardware.
4. Optional alternative kernel configurations, differing in one controlled factor.

## Required metadata
CPU, RAM, motherboard, firmware, GPU and driver, display mode and refresh, kernel build, OS version, compiler, power profile, CPU governor, thermal conditions, game/app version, test fixture hash, run order and raw traces.

## Metrics
- startup p50/p95 with cold/warm cache separated
- RSS/PSS/USS and page-fault counts; swap and memory pressure
- synchronization throughput and wake-up p50/p99/p99.9
- frame time distribution including 1% and 0.1% lows; input-to-photon where instrumentation exists
- CPU and GPU energy and clocks; security mitigations must be recorded
- correctness failures and crash rates

## Gates
- Correctness first: passing ABI, PE, TLS, exception and process tests.
- No claimed improvement without repeated measurements and uncertainty.
- Reject changes with significant regressions in security, stability, hardware support or compatibility.
- Publish negative findings and revert unproven optimizations.
