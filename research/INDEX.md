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

- [R41 — vectored exception/continue handler lifecycle](R41_VECTORED_EXCEPTION_CONTINUE_GATES.md): primary Microsoft documentation; separate process-wide callback ordering, removal, and continuation from x64 unwind tables. Windows-reference conformance gates proposed; no experiment.


- [R42 — thread suspension, resume counts and context APIs](R42_THREAD_SUSPEND_CONTEXT_GATES.md): Microsoft primary API contracts; separate logical suspend counts and x64 context fidelity from Linux thread signals. Proposed Windows-reference tests only; no experiment.

- [R43 — asynchronous I/O cancellation and completion suppression](R43_ASYNC_IO_CANCELLATION_COMPLETION.md): Microsoft primary API contracts distinguish cancellation request from terminal completion, IOCP packet suppression, and failed dequeue status. Proposed reference tests only; no experiment.

- [R44 — PE32+ ordinary import parser bounds and structural acceptance](R44_PE64_IMPORT_PARSER_BOUNDARIES.md): Microsoft PE-format-based strict parse-only contract, including multi-descriptor bounds, lookup/IAT separation, and proposed Windows differential gates. No new experiment.

- [R45 — DLL loader-lock serialization and entry-point lifecycle](R45_DLL_LOADER_LOCK_LIFECYCLE.md): Microsoft primary documentation; separates serialized DllMain callbacks, thread notifications, loader-lock inversion and failure rollback from PE import parsing and TLS structure. Proposed Windows-reference conformance gates only; no experiment.\n\n- [R46 — dynamic TLS slot APIs versus PE static TLS and FLS](R46_DYNAMIC_TLS_SLOT_API_GATES.md): Microsoft primary API contracts for TlsAlloc/TlsGetValue/TlsSetValue/TlsFree and version-gated TlsGetValue2, with Windows-reference test matrix. No experiment.\n\n- [R48 — SRW locks and condition-variable gates](R48_SRW_CONDITION_VARIABLE_GATES.md): Microsoft primary API contracts for process-local pointer-sized SRW locks, atomic unlock-and-sleep, shared/exclusive reacquisition, timeout and lost-wakeup conformance. No experiment.\n\n
- [R49 — file delete disposition, sharing and POSIX unlink mismatch](R49_FILE_DELETE_DISPOSITION_SHARING.md): Microsoft CreateFile and MS-FSCC primary contracts; distinguishes legacy delete-pending from opt-in POSIX semantics, share checks, duplicated handles and section constraints. Reference conformance matrix proposed; no experiment.

- [R50 — file rename replacement and POSIX opt-in semantics](R50_FILE_RENAME_REPLACEMENT_GATES.md): Microsoft primary Win32/driver/MS-FSCC contracts; separates legacy replacement from EX POSIX replacement, open-handle identity, and Linux backend race/isolation gates. No experiment.


- [R51 — PE export forwarders, ordinals and resolution boundaries](R51_PE_EXPORT_FORWARDERS_RESOLUTION.md): Microsoft PE-format and GetProcAddress documentation; distinguishes EAT/name/ordinal indexing, forwarder chains and API-set boundaries. Parse-only and Windows-reference gates proposed; no experiment.

- [R52 — NTSTATUS, Win32 last-error and HRESULT boundaries](R52_NTSTATUS_LAST_ERROR_HRESULT_BOUNDARIES.md): primary Microsoft API contracts, per-thread error state and explicit conversion boundaries; proposed Windows-reference tests only, no experiment.\n\n- [R53 — placeholder VM reservation, split/coalesce and mapped-view replacement](R53_PLACEHOLDER_VM_REPLACEMENT.md): Microsoft VirtualAlloc2/MapViewOfFile3/VirtualFree/UnmapViewOfFile2 contracts; identifies address-ownership and MAP_FIXED race hazards in a prospective Linux backend. Reference tests proposed; no experiment.


- [R54 — WaitOnAddress versus Linux futex conformance](R54_WAIT_ON_ADDRESS_FUTEX_GATES.md): Windows 1/2/4/8-byte address comparison versus Linux 32-bit futex, lost-wakeup races, wake targeting, timeout and error translation; proposed differential tests only, no experiment.

- [R55 — cross-ABI SIMD preservation and unwind gates](R55_CROSS_ABI_SIMD_PRESERVATION_GATES.md): Microsoft/GCC primary-source review of XMM6–XMM15 preservation, callbacks and unwind obligations; PE fixture and differential tests proposed, not executed.\n\n## Highest-priority untested gates
1. Expand R20 to parse and inspect multiple IAT descriptors safely without execution; then independently implement and conformance-test real Win32/NT APIs, DLL loading and TLS.
2. R26 traps one Windows-origin syscall; implement repeated NT-origin call dispatch, correct NTSTATUS handling and selector rearming across transitions without allowing unintended Linux syscalls.
3. Cross-ABI unwinding, nonvolatile vector registers, TEB GS behavior and per-thread initialization.
4. Full ntsync runtime tests on a host with a suitable device/header/module combination.
5. License inventory and security threat model before substantial code reuse.

**Not claimed:** Windows app compatibility, working NT subsystem, performance superiority, anti-cheat support, Windows driver support or a functioning operating system. Those require separate conformance evidence.

- [R56 — NT directory enumeration state and Linux getdents boundaries](R56_DIRECTORY_ENUMERATION_GATES.md): Microsoft/Linux primary documentation; query cursor, search expression, information-class packing and mutation conformance gates. No experiment.

- [R57 — Windows byte-range locks versus Linux OFD locks](R57_BYTE_RANGE_LOCKS_OFD_GATES.md): Microsoft and Linux primary-source review of lock ownership, same-handle overlap, mandatory I/O enforcement, asynchronous completion and proposed Windows/Linux differential gates. No experiment.

- [R58 — Windows directory change notifications versus inotify](R58_DIRECTORY_CHANGE_NOTIFICATIONS.md): primary Microsoft/Linux documentation; distinguishes Windows handle-scoped buffering, subtree coverage, overflow and asynchronous completion from Linux nonrecursive watches and event loss. Reference conformance gates proposed; no experiment.

- [R59 — image-section mapping and copy-on-write gates](R59_IMAGE_SECTION_COW_GATES.md): Microsoft/Linux primary-source contracts distinguish SEC_IMAGE protections, SEC_IMAGE_NO_EXECUTE, FILE_MAP_COPY commit accounting, and Linux MAP_PRIVATE. Differential conformance plan only; no experiment.

- [R60 — reparse points and Linux openat2 path confinement](R60_REPARSE_POINTS_OPENAT2_BOUNDARIES.md): primary Microsoft/Linux documentation distinguishes reparse tags and open-the-link semantics from Linux symlinks and host path resolution. Proposes differential Windows and adversarial Linux gates; no experiment.

- [R61 — Windows file identity, hard links and Linux inode mapping](R61_FILE_IDENTITY_HARDLINK_INODE_GATES.md): Microsoft/Linux primary documentation; distinguishes volume-scoped Windows 128-bit IDs from filesystem-scoped Linux inodes, with proposed cross-alias and lifetime conformance tests. No experiment.
