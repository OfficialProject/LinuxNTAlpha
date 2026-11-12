# LinuxNTAlpha research index — 2026-11-12

Read with [research plan](PLAN.md) and [architecture decisions](DECISIONS.md). Findings labeled VERIFIED in older notes may reflect documentation review rather than a run of code; the concrete experiments below explicitly distinguish PASS from SKIP.

## Scope and architecture
- [R1 — PE64 + ntsync initial audit](R1_PE64_NT_SYNC_AUDIT.md)
- [R2 — Windows x64 ABI](R2_WINDOWS_ABI.md)
- [R4 — native execution boundaries](R4_NATIVE_EXECUTION_SCOPE.md)
- [R5 — benchmark protocol](R5_BENCHMARK_PROTOCOL.md)
- [R10 — object manager and file sharing](R10_OBJECT_MANAGER_FILESYSTEM.md)
- [R11 — NT memory/sections](R11_NT_MEMORY_SECTIONS.md)
- [R12 — process creation](R12_PROCESS_CREATION_LIFECYCLE.md)
- [R13 — APC and alertable waits](R13_APC_ALERTABLE_WAITS.md)
- [R14 — security tokens and ACLs](R14_SECURITY_TOKENS_ACL.md)
- [R15 — IOCP vs io_uring](R15_IOCP_IO_URING.md)

## Synchronization
- [R3 — ntsync performance hypothesis](R3_NTSYNC_PERFORMANCE.md)
- [R6 — ntsync API semantics](R6_NTSYNC_API_VERIFICATION.md)
- [R7 — ntsync source-level questions](R7_NTSYNC_SOURCE_AUDIT.md)
- [R8 — corrected ntsync documentation-name discrepancy](R8_NTSYNC_VERSION_DRIFT.md)
- [R9 — x64 exception unwind and ntsync correction](R9_EXCEPTION_UNWIND_NTSYNC_VERSION.md)
- [R18 — **actual v6.13→v6.14 ntsync UAPI change**, plus stale installed header](R18_NTSYNC_ACTUAL_UAPI_TRANSITION.md)

## Executed prototypes
- [R16 — Syscall User Dispatch](R16_SYSCALL_USER_DISPATCH_PROTOTYPE.md): Linux x86-64 native syscall interception **PASS** at GCC -O0/-O2/-O3. No Windows raw syscall test yet.
- [R17 — bidirectional ABI bridge](R17_ABI_BRIDGE_EXPERIMENT.md): GCC-generated MS/SysV call signatures **PASS** at -O0/-O2/-O3. No PE execution yet.
- [R18 — ntsync capability probe](R18_NTSYNC_ACTUAL_UAPI_TRANSITION.md): installed header detected as old, device absent, runtime test **SKIP**. Compiler-path tests verified; no ntsync benchmark.

Executable sources and makefiles: [experiments](../experiments/). Command: **make -C experiments test** (check each PASS/SKIP separately).

## Highest-priority untested gates
1. Real PE64 binary creation, parsing, image-section mapping and entry-point invocation with isolated fixture.
2. Real Windows-origin raw syscall interception and correct NT-status dispatch, without accidentally entering Linux syscall services.
3. Cross-ABI unwinding, nonvolatile vector registers, TEB GS behavior and per-thread initialization.
4. Full ntsync runtime tests on a host with a suitable device/header/module combination.
5. License inventory and security threat model before substantial code reuse.

**Not claimed:** Windows app compatibility, working NT subsystem, performance superiority, anti-cheat support, Windows driver support or a functioning operating system. Those require separate conformance evidence.
