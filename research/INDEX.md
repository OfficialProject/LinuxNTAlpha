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

## Legal and reuse gates
- [R21 — source-level license audit](R21_CODE_REUSE_LICENSE_AUDIT.md): Linux GPL-2.0 with syscall exception, ReactOS GPL plus mixed files, Wine LGPL, LLVM Apache with exception; project license remains a user decision. No implementation code imported.

## Executed prototypes
- [R16 — Syscall User Dispatch](R16_SYSCALL_USER_DISPATCH_PROTOTYPE.md): Linux x86-64 native syscall interception **PASS** at GCC -O0/-O2/-O3. A Windows-target PE raw syscall test is now in R26; still no real NT service mapping.
- [R17 — bidirectional ABI bridge](R17_ABI_BRIDGE_EXPERIMENT.md): GCC-generated MS/SysV call signatures **PASS** at -O0/-O2/-O3. PE execution is now separately exercised in R19–R28.
- [R18 — ntsync capability probe](R18_NTSYNC_ACTUAL_UAPI_TRANSITION.md): installed header detected as old, device absent, runtime test **SKIP**. Compiler-path tests verified; no ntsync benchmark.
- [R19 — PE32+ native execution](R19_PE64_EXECUTION_EXPERIMENT.md): a Clang/LLD-built freestanding PE64 image runs in Linux at a relocated base and returns 42, **PASS** (five positive iterations, 19 negative malformed-image tests).
- [R20 — a real PE IAT import bound to host function](R20_PE64_IAT_BRIDGE.md): a second PE64 image invokes its sole imported probe.dll!add_five via a Microsoft-ABI Linux host stub, **PASS** (five additional iterations, seven additional import rejection tests). Real Windows OS services remain unimplemented.

- [R22 — exploratory SUD trap latency](R22_SYSCALL_DISPATCH_LATENCY.md): 7-batch microbenchmark, pinned CPU in one container; 20000-iteration median batch averages ~72 ns allowed vs ~1032 ns SIGSYS interception. These are NOT real-app slowdowns or p99 values.
- [R23 — PE parse-only separation and mutation smoke test](R23_PARSE_ONLY_FUZZ_GATE.md): execution requires an explicit trusted-fixture flag; 26 known malformed cases plus 200 seeded mutations process without executing PE code. **Not a security guarantee.**

- [R24 — GS-based synthetic TIB concurrency](R24_GS_TEB_CONTEXT.md): four pthreads read distinct GS:0x30 and signal context while native Linux FS TLS stays intact (128 signals, GCC optimizations and sanitizer local tests). No real Windows TEB initialized.

- [R25 — real Windows-target PE reads synthetic GS TIB](R25_PE64_GS_TIB_EXECUTION.md): 5 successful PE entry calls that read GS:0x30 and call one imported Linux MS-ABI stub; no actual Windows TEB/PEB or kernel services.

- [R26 — Windows-target PE raw syscall intercepted by SUD](R26_PE64_RAW_SYSCALL_SUD.md): PE machine code issues an NT-style syscall deliberately colliding with Linux getpid; host returns one synthetic SIGSYS result (five passes). This is NOT a working NT service table or sandbox.

- [R28 — first real NTDLL export implementation](R28_NTDLL_RTL_COMPARE_MEMORY.md): independent MS-ABI RtlCompareMemory implementation verified by 8 host calls, 5 Windows-target PE imported calls, and strict import-name checks; no Windows process semantics implied.

Executable sources and makefiles: [experiments](../experiments/). Command: **make -C experiments test** (check each PASS/SKIP separately).

## Additional loader research
- [R27 — PE32+ static TLS directory and loader lifecycle gates](R27_PE64_TLS_LOADER_GATES.md): primary-specification review only; distinguishes TLS templates/callbacks from R24/R25 synthetic GS/TIB and R20 imports. No new experiment.

- [R29 — API-set contract resolution and delay-import gates](R29_API_SET_DELAY_IMPORT_GATES.md): Microsoft primary documentation; no experiment. Contract names are not physical DLL paths, and delay imports have separate descriptors and failure timing.

- [R30 — x64 static unwind metadata versus dynamic function-table registration](R30_X64_UNWIND_TABLE_REGISTRATION.md): Microsoft primary documentation; parse-only validation and Windows-reference conformance gates proposed, no experiment.\n\n- [R31 — PE load configuration, CFG metadata and process mitigation gates](R31_PE_LOAD_CONFIG_CFG_MITIGATIONS.md): Microsoft primary documentation; parse-only and Windows-reference acceptance gates proposed, no experiment.\n\n- [R32 — DLL search policy and activation-context resolution](R32_DLL_SEARCH_ACTIVATION_CONTEXTS.md): Microsoft primary documentation; Windows-version-sensitive module lookup, dependency resolution, and SxS context conformance gates. No experiment.\n\n- [R33 — waitable timer semantics and Linux backend gates](R33_WAITABLE_TIMER_SEMANTICS.md): Microsoft primary documentation; notification/synchronization timers, APC delivery, object naming, resolution policy and conformance proposals. No experiment.\n\n- [R34 — process parameters, environment and handle inheritance](R34_PROCESS_PARAMETERS_ENVIRONMENT_INHERITANCE.md): Microsoft primary documentation; command-line identity, environment blocks, drive directories and child handle semantics; conformance matrix proposed, no experiment.\n\n- [R35 — fibers, FLS and thread identity](R35_FIBERS_FLS_THREAD_IDENTITY.md): Microsoft primary documentation; cooperative fiber switching, per-fiber storage and cleanup are separate from GS/TIB and PE TLS. Reference conformance matrix proposed; no experiment.\n\n- [R36 — Windows job objects versus Linux cgroup v2](R36_JOB_OBJECT_CGROUP_V2_GATES.md): primary Microsoft/Linux documentation; identifies non-equivalent membership, commit accounting, nesting and lifecycle semantics; proposes Windows-reference conformance gates. No experiment.

- [R37 — named pipe semantics versus AF_UNIX](R37_NAMED_PIPES_AF_UNIX_SEMANTIC_GATES.md): Windows message/byte modes, instance lifecycle, ACLs and overlapped completion are not supplied automatically by Linux sockets. Primary documentation and proposed conformance gates only; no experiment.\n\n- [R38 — registry namespace, WOW64 views and notifications](R38_REGISTRY_WOW64_NOTIFICATION_GATES.md): Microsoft primary documentation; distinguishes logical registry views, shared-key exceptions, key security, and change notification lifecycles. Conformance proposals only; no experiment.

- [R39 — Windows thread-pool callback lifetime and cleanup groups](R39_THREADPOOL_CALLBACK_LIFETIME.md): primary Microsoft API documentation; queued/running callback distinction, wait rearming, cleanup-group lifetime, and proposed Windows-reference conformance tests. No experiment.\n\n- [R40 — Windows console event buffers versus Linux PTYs](R40_WINDOWS_CONSOLE_PTY_GATES.md): Microsoft primary documentation; distinguishes classic console input records, modes, ConPTY streams and resize lifecycle. Windows-reference conformance gates proposed; no experiment.

## Highest-priority untested gates
1. Expand R20 to parse and inspect multiple IAT descriptors safely without execution; then independently implement and conformance-test real Win32/NT APIs, DLL loading and TLS.
2. R26 traps one Windows-origin syscall; implement repeated NT-origin call dispatch, correct NTSTATUS handling and selector rearming across transitions without allowing unintended Linux syscalls.
3. Cross-ABI unwinding, nonvolatile vector registers, TEB GS behavior and per-thread initialization.
4. Full ntsync runtime tests on a host with a suitable device/header/module combination.
5. License inventory and security threat model before substantial code reuse.

**Not claimed:** Windows app compatibility, working NT subsystem, performance superiority, anti-cheat support, Windows driver support or a functioning operating system. Those require separate conformance evidence.
