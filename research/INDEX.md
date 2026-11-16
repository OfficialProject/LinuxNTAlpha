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

- [R62 — Windows file timestamps versus Linux inode clocks](R62_FILE_TIMESTAMP_SEMANTICS.md): Microsoft/Linux primary-source analysis of four Windows file times, Linux birth/status-change clocks, setter gaps and proposed differential conformance tests. No experiment.

- [R63 — Windows case-sensitive directory lookup versus Linux pathname resolution](R63_CASE_SENSITIVE_DIRECTORY_LOOKUP.md): Microsoft and Linux primary documentation; separates Windows per-directory case policy from Linux path confinement, with collision/race and reference conformance gates. No experiment.

- [R64 — DLL dependency graph identity, publication and rollback](R64_DLL_DEPENDENCY_GRAPH_LIFECYCLE.md): Microsoft primary-source analysis of loaded-module identity, shared dependencies, attach failures, and proposed nonexecuting graph plus Windows differential gates. No experiment.

- [R65 — Windows stack growth, guard faults, and Linux signal delivery](R65_STACK_GUARD_GROWTH_GATES.md): primary Microsoft/Linux documentation distinguishes reserved/committed stack pages, one-shot guard faults, stack-overflow recovery and Linux alternate signal stacks. Proposes Windows-reference and isolated Linux conformance gates; no experiment.

- [R66 — IOCP batched dequeue and alertable-wait gates](R66_IOCP_BATCHED_ALERTABLE_DEQUEUE.md): Microsoft/Linux primary documentation distinguishes batched port packets, APC interruption and io_uring CQEs; proposes Windows-reference and isolated state-machine tests. No experiment.

- [R67 — cross-process NT handle duplication and Linux fd boundaries](R67_CROSS_PROCESS_HANDLE_DUPLICATION.md): Microsoft/Linux primary documentation distinguishes process-relative NT handle rights, close-source failure semantics, object-type restrictions and pidfd_getfd limitations. Proposed Windows differential gates only; no experiment.

- [R68 — process/thread pseudo-handles and caller-relative identity](R68_PSEUDOHANDLE_CALLER_IDENTITY.md): Microsoft API contract review distinguishes caller-relative special handles from real transferable handles; no tests run.

- [R69 — dynamic-code mitigation and executable-memory trust boundary](R69_DYNAMIC_CODE_MITIGATION_EXEC_MEMORY.md): Microsoft/Linux primary documentation distinguishes Windows process/thread mitigation policy from Linux mapping permissions and identifies direct-syscall bypass as an untested security gate. No experiment.

- [R70 — mapped-view flush versus durable file synchronization](R70_MAPPED_VIEW_FLUSH_DURABILITY.md): Microsoft/Linux primary documentation distinguishes view writeback, metadata persistence, handle-type behavior and Linux msync/fsync guarantees. Proposes differential tests; no experiment.

- [R71 — Windows write-watch and Linux dirty-page tracking](R71_WRITE_WATCH_DIRTY_PAGE_TRACKING.md): Microsoft GetWriteWatch/ResetWriteWatch and Linux userfaultfd primary documentation; identifies reset races, missing-page writes and feature negotiation as conformance gates. No experiment.

- [R72 — NT ZeroBits and address-constrained VM placement](R72_NT_ADDRESS_CONSTRAINTS_ZEROBITS.md): separates Native NT high-bit restrictions, Win32 extended address requirements and top-down allocation from Linux mmap hints; reference conformance proposed, no experiment.

- [R73 — process wait objects, exit-status persistence and pidfd boundaries](R73_PROCESS_WAIT_PIDFD_EXIT_STATUS.md): Microsoft and Linux primary documentation; host pidfd/reaping is not an NT process object, and Linux wait status truncates normal exit codes. Proposed conformance tests only; no experiment.

- [R74 — thread-object exit status and Linux clear-child-TID mismatch](R74_THREAD_OBJECT_EXIT_LIFETIME.md): Microsoft thread termination/handle lifetime and Linux set_tid_address documentation; durable guest-thread-object hypothesis and falsifiable gates. No experiment.

- [R75 — wait-for-multiple-objects atomicity and Linux futex_waitv limits](R75_WAIT_MULTIPLE_OBJECTS_ATOMICITY.md): Microsoft/Linux primary API contracts; wait-all deferred state transitions, wait-any index selection, abandonment and alertable wait gates. Windows-reference conformance tests proposed; no experiment.

- [R76 — Windows job objects versus Linux cgroup v2](R76_JOB_OBJECTS_CGROUP_V2_GATES.md): primary Microsoft and Linux v6.12 contracts distinguish NT job identity, nested membership, kill-on-close and IOCP notification semantics from cgroup resource enforcement. Reference tests proposed; no experiment.

- [R77 — Windows process/thread priority versus Linux scheduling](R77_WINDOWS_PRIORITY_LINUX_SCHEDULER_GATES.md): Microsoft/Linux primary API references distinguish process classes, relative thread priority, background modes, dynamic boosts, and host scheduler privileges; proposes differential conformance gates. No experiment.

- [R78 — Windows named-pipe instances versus Linux local IPC](R78_NAMED_PIPES_UNIX_IPC_GATES.md): Microsoft named-pipe and Linux Unix-socket primary contracts; separates per-name instances, message-read modes, connection races, overlapped completion and guest ACLs from host IPC transport. Proposed conformance gates only; no experiment.

- [R79 — NTFS alternate data streams versus Linux xattrs](R79_ALTERNATE_DATA_STREAMS_XATTR_GATES.md): primary Microsoft/Linux documentation distinguishes independently addressable named data streams from bounded whole-value xattrs; proposes backend and Windows-reference conformance gates. No experiment.

- [R80 — Windows processor groups versus Linux CPU affinity](R80_PROCESSOR_GROUP_AFFINITY_GATES.md): Microsoft/Linux primary-source contracts distinguish Windows version-dependent group/primary-group identity from Linux per-thread cpumasks and cpuset intersections; reference tests proposed, not executed.

- [R81 — NT thread impersonation versus Linux credentials](R81_THREAD_IMPERSONATION_HOST_CREDENTIALS.md): Microsoft/Linux primary documentation distinguishes guest effective-token selection from host UID and filesystem credential enforcement; proposes security and Windows-reference conformance gates. No experiment.

- [R82 — NT keyed-event two-sided rendezvous versus Linux futex](R82_KEYED_EVENT_RENDEZVOUS.md): pinned ReactOS source audit and Linux futex contract identify release-first pairing, key identity, timeout and APC gates. Reference conformance proposed; no experiment.

- [R83 — Windows oplock breaks versus Linux file leases](R83_OPLOCK_LINUX_LEASE_SEMANTIC_GATES.md): Microsoft/Linux primary documentation distinguishes oplock break acknowledgment, asynchronous completion and directory support from Linux signal-driven, timeout-bounded regular-file leases. Conformance gates proposed; no experiment.

- [R84 — Win32 atom table namespace and lifetime](R84_WIN32_ATOM_TABLE_NAMESPACE_LIFETIME.md): Microsoft primary API contracts distinguish local and global atom identity, case-insensitive lookup, reference counting, process-exit persistence and integer atom boundaries. Proposed Windows-reference gates only; no experiment.

- [R85 — InitOnce initialization retry and publication gates](R85_INIT_ONCE_INITIALIZATION_GATES.md): Microsoft primary API contracts distinguish retryable failure, synchronous/async completion, guest ABI layout and context publication; proposed Windows-reference tests only, no experiment.

- [R86 — module-handle counted lookup, pinning and lifetime](R86_MODULE_HANDLE_PINNING_LIFETIME.md): Microsoft GetModuleHandleEx/FreeLibrary contracts; distinguishes borrowed handles, permanent pinning, address lookup and concurrent unload. Proposed Windows-reference tests only; no experiment.

- [R87 — NT object name removal versus reference lifetime](R87_OBJECT_NAME_REFERENCE_LIFETIME.md): Microsoft Object Manager primary contracts distinguish temporary namespace-entry removal from object reference lifetime, handle closure on process exit, session naming and granted-rights checks. Proposed broker-state and Windows-reference gates; no experiment.

- [R88 — regular and special user-mode APC delivery boundaries](R88_REGULAR_SPECIAL_USER_APC_DELIVERY.md): Microsoft primary Win32 contracts distinguish alertable FIFO delivery from special APC interruption, callback context and thread lifetime; proposes differential Windows/Linux gates. No experiment.

- [R89 — DOS device namespace and NT path translation](R89_DOS_DEVICE_NT_PATH_TRANSLATION.md): Microsoft Win32/NT and Linux openat2 primary contracts; distinguishes DOS device aliases, drive-relative path parsing, NT RootDirectory resolution and host path confinement. Proposed reference and adversarial tests only; no experiment.

- [R90 — guest/host syscall provenance and SUD security boundary](R90_GUEST_HOST_SYSCALL_PROVENANCE.md): Linux upstream SUD/seccomp documentation and R22/R26/R69 synthesis; distinguishes compatibility dispatch from containment, with nested-transition, concurrency and adversarial gates. No new experiment.

- [R91 — DLL loader-lock serialization and failure rollback](R91_DLL_LOADER_LOCK_NOTIFICATION_ROLLBACK.md): Microsoft primary loader entry-point contracts distinguish process/thread notification eligibility, serialized callbacks, FALSE versus exception failure paths, and dynamic versus static load. Proposed differential conformance only; no experiment.

- [R92 — NT discretionary access checks and Linux credential boundary](R92_NT_DACL_ACCESS_CHECK_HOST_BOUNDARY.md): Microsoft MS-DTYP and Win32 primary contracts distinguish ordered ACE evaluation, NULL versus empty DACL, generic mapping, MAXIMUM_ALLOWED and host authorization. Proposed Windows differential/security gates; no experiment.

- [R93 — debugger first-/second-chance exception dispatch](R93_DEBUGGER_EXCEPTION_DISPATCH_GATES.md): Microsoft debug-event contracts and Linux signal/ptrace boundaries; proposes Windows-reference dispatch ordering and debugger disposition gates. No experiment.

- [R94 — NT section-view inheritance versus handle inheritance](R94_SECTION_VIEW_INHERITANCE_GATES.md): Microsoft primary contracts separate section object lifetime, process-relative handles and mapped-view inheritance; native/Win32 child-creation differences remain an explicit Windows-reference gate. No experiment.

- [R95 — Windows critical-section recursion versus Linux futex](R95_CRITICAL_SECTION_FUTEX_RECURSION.md): Microsoft/Linux primary documentation; isolates process-local recursive ownership, spin policy, guest thread identity and futex wakeup correctness from SRW locks, WaitOnAddress and NT mutexes. Proposed differential tests only; no experiment.

- [R96 — fiber-local storage callbacks and module lifetime](R96_FLS_CALLBACK_LIFETIME_GATES.md): Microsoft FLS and POSIX pthread-key primary contracts; distinguishes process-wide FLS-index free callbacks from thread-only pthread destructors and documents unload-time callback hazards. Windows-reference gates proposed; no experiment.

- [R97 — WaitOnAddress versus Linux futex conformance gates](R97_WAIT_ON_ADDRESS_FUTEX_GATES.md): Microsoft/Linux primary API contracts distinguish 1/2/4/8-byte guest address comparisons from Linux four-byte futex words, and isolate wake-registration, address lifetime, and cross-process-scope questions. Proposed differential tests only; no experiment.

- [R98 — remote process memory access versus Linux process_vm_*](R98_REMOTE_PROCESS_MEMORY_GATES.md): Microsoft process-handle rights and full-range read/write contracts compared with Linux ptrace authorization and partial-copy returns; proposes target-lifetime, rights, and race conformance gates. No experiment.

- [R99 — process command-line and environment fidelity](R99_PROCESS_COMMAND_LINE_ENVIRONMENT_GATES.md): Microsoft primary API contracts distinguish raw UTF-16 command lines and Windows environment blocks from Linux argv/envp; proposed Windows-reference differential gates, no experiment.

- [R100 — synchronous file pointer versus explicit-offset I/O](R100_FILE_POINTER_EXPLICIT_OFFSET_GATES.md): Microsoft ReadFile/WriteFile/SetFilePointerEx and Linux pread/pwrite primary contracts; distinguishes guest current-position state from per-request offsets and proposes Windows-reference aliasing/concurrency tests. No experiment.

- [R101 — Windows sparse-file controls versus Linux extent operations](R101_SPARSE_FILE_EXTENT_GATES.md): Microsoft sparse/zero/allocated-range controls compared with Linux fallocate hole punching and zero-range behavior; distinguishes guest sparse metadata from host extents and proposes differential tests. No experiment.

- [R102 — SignalObjectAndWait handoff non-atomicity](R102_SIGNAL_OBJECT_AND_WAIT_HANDOFF.md): Microsoft explicitly disclaims atomic signal-then-wait behavior; distinguishes this API from R75 wait-all transactions and proposes Windows-reference gates. Documentation only; no experiment.

- [R103 — Windows CPU Sets versus Linux thread affinity](R103_CPU_SETS_AFFINITY_GATES.md): Microsoft CPU Set precedence, process-default versus thread-selected policy and processor groups contrasted with Linux per-thread affinity and cpuset restrictions. Differential conformance proposed; no experiment.

- [R104 — Windows timer-resolution policy versus Linux timer scheduling](R104_TIMER_RESOLUTION_WAITABLE_TIMER_GATES.md): Microsoft Windows 10/11 timeBeginPeriod policy, waitable timer signaling/APCs and Linux timer slack/timerfd contracts; proposes cross-version differential and wakeup/power gates. No experiment.

- [R105 — pagefile-backed section commitment versus Linux overcommit](R105_SECTION_COMMIT_OVERCOMMIT_GATES.md): Microsoft SEC_COMMIT/SEC_RESERVE accounting and section lifetime contrasted with Linux mmap/MAP_NORESERVE and host overcommit policy; cross-process and failure-path conformance proposed, no experiment.

- [R106 — SxS activation-context and manifest-directed binding](R106_SXS_ACTIVATION_CONTEXT_BINDING.md): Microsoft primary contracts distinguish per-thread activation stacks, manifest resolution and SxS redirects from ordinary DLL search; proposes Windows-reference, lifetime and security gates. No experiment.

- [R107 — bound-import pointer provenance and loader trust boundary](R107_BOUND_IMPORT_POINTER_PROVENANCE.md): Microsoft PE/COFF primary documentation distinguishes prebound IAT contents from import lookup metadata; proposes a fail-closed Linux-hosted loader policy and Windows-reference/security gates. No experiment.

- [R108 — Linux signal ownership and NT exception ingress](R108_SIGNAL_OWNERSHIP_NT_EXCEPTION_INGRESS.md): Linux process-wide signal dispositions versus thread masks, host/guest fault provenance, nested-fault and Windows SEH ordering gates. Primary-source review only; no experiment.

- [R109 — Windows CET shadow-stack policy versus Linux user shadow stack](R109_CET_SHADOW_STACK_POLICY_GATES.md): Microsoft and Linux primary contracts distinguish Windows process/image CET compatibility and strict-mode policy from Linux per-thread hardware enforcement and signal restoration. Proposes capability, Windows-reference and cross-ABI conformance gates; no experiment.

- [R111 — per-handle IOCP completion suppression](R111_IOCP_COMPLETION_SUPPRESSION_GATES.md): Microsoft primary contracts distinguish immediate-success packet suppression, OVERLAPPED event low-bit suppression, and file-object event signaling from host async completion. Proposed differential/race gates; no experiment.

- [R112 — Windows console objects versus Linux pseudoterminals](R112_WINDOWS_CONSOLE_PTY_BOUNDARIES.md): Microsoft console/ConPTY and Linux PTY primary documentation; distinguishes guest handle, input-record, Unicode and screen-buffer semantics from host terminal streams. Differential gates proposed; no experiment.

- [R113 — fixed-address KUSER_SHARED_DATA versus Linux vDSO](R113_KUSER_SHARED_DATA_VDSO_GATES.md): Microsoft/Linux primary-source review distinguishes direct guest loads at 0x7FFE0000 from Linux clock APIs; proposes mapping, coherence, versioning and Windows-reference gates. No experiment.

- [R114 — NT delete disposition versus Linux unlink](R114_DELETE_DISPOSITION_UNLINK_GATES.md): Microsoft file-disposition/Win32 sharing contracts compared with Linux unlink lifetime; distinguishes legacy delete-pending from POSIX namespace removal, with proposed Windows-reference and race gates. No experiment.
