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

- [R115 — minimum NT runtime integration gates](R115_MINIMUM_NT_RUNTIME_INTEGRATION_GATES.md): evidence-based dependency order and falsifiable integrated PE/ABI/thread/dispatch lifecycle milestone. Architecture synthesis only; no new experiment.

- [R116 — ALPC port semantics versus Linux Unix-domain IPC](R116_ALPC_UNIX_IPC_BOUNDARIES.md): Microsoft ALPC ETW, third-party native declarations and Linux unix(7) distinguish guest port identity, reply correlation, handle transfer and impersonation from AF_UNIX transport; proposed conformance/security gates only, no experiment.

- [R117 — PE section alignment, host-page protection and discardable-section gates](R117_PE_SECTION_PAGE_PROTECTION_GATES.md): Microsoft PE and Linux mmap/mprotect primary contracts; distinguishes page-level permission conflicts and discard lifecycle from R19's narrow successful fixture. Parse-only and Windows-reference tests proposed; no experiment.

- [R118 — Windows large-page section versus Linux HugeTLB](R118_LARGE_PAGE_SECTION_HUGETLB_GATES.md): Microsoft and Linux primary contracts distinguish section-level large-page eligibility, Windows 10 1703+ view opt-in, guest privilege checks and Linux unswappable HugeTLB pool constraints. Versioned differential and capability gates proposed; no experiment.

- [R119 — Linux memfd sealing versus mutable NT section views](R119_MEMFD_SEALS_SECTION_VIEW_GATES.md): Linux memfd/seal and Microsoft mapping contracts distinguish irreversible inode seals from per-view NT access, COW and protection transitions. Proposed Windows/Linux differential and security gates; no experiment.

- [R120 — NT context continuation versus Linux signal-frame restoration](R120_NT_CONTEXT_CONTINUATION_SIGNAL_RETURN.md): Microsoft RtlRestoreContext/RtlUnwindEx and Linux signal-frame primary contracts; separates guest CONTEXT, host ucontext and unwind semantics. Isolated Linux and Windows-reference gates proposed; no experiment.

- [R121 — Windows heap identity and allocator semantic gates](R121_WINDOWS_HEAP_IDENTITY_ALLOCATOR_GATES.md): Microsoft heap API contracts distinguish process/private heap handles, allocation ownership, realloc failure and heap policy from host malloc. Windows-reference and backend differential gates proposed; no experiment.

- [R122 — Windows child handle inheritance versus Linux fd inheritance](R122_CHILD_HANDLE_INHERITANCE_GATES.md): Microsoft child handle selection, stable numeric guest handles and access rights contrasted with Linux close-on-exec/clone descriptor semantics; proposes leak-resistant guest handle-table bootstrap and differential race tests. No experiment.

- [R123 — general PAGE_GUARD one-shot fault gates](R123_GENERAL_GUARD_PAGE_ONESHOT_GATES.md): Microsoft and Linux primary contracts distinguish one-shot guard clearing, underlying protection and service-call failures from Linux mprotect/userfaultfd. Proposed Windows-reference and concurrency gates; no experiment.

- [R124 — Windows interrupt-time domains versus Linux clocks](R124_WINDOWS_INTERRUPT_TIME_LINUX_CLOCK_DOMAINS.md): Microsoft/Linux primary contracts distinguish suspend-inclusive and suspend-exclusive elapsed clocks, precise versus coarse time readings, and time-namespace effects. Proposed differential gates only; no experiment.

- [R125 — NT-compatible thread bootstrap publication and startup gates](R125_THREAD_BOOTSTRAP_PUBLICATION_GATES.md): Microsoft CreateThread/CRT and Linux clone/clear-child-TID primary contracts; proposes parked-host-thread publication barriers, rollback ownership and Windows differential tests. No experiment.

- [R126 — x64 stack-probe helper and guest stack-growth gates](R126_X64_STACK_PROBE_HELPER_GATES.md): Microsoft compiler/ABI documentation distinguishes compiler-inserted `__chkstk` linkage and custom calling convention from R65 stack-growth handling. Proposes pinned-toolchain PE and Windows-reference gates; no experiment.

- [R127 — PE resource tree and language-selection gates](R127_PE_RESOURCE_TREE_AND_LANGUAGE_GATES.md): Microsoft PE/Win32 primary contracts distinguish bounded resource-directory parsing from type/name/language lookup and MUI selection; proposes malformed-image and Windows-reference gates. No experiment.

- [R128 — NT service identity and versioned dispatch gates](R128_NT_SERVICE_DISPATCH_VERSIONING_GATES.md): Microsoft primary API contracts and R26/R90 synthesis distinguish named NTDLL services from build-specific raw syscall identifiers; proposes synthetic profile, negative dispatch and Windows-reference gates. No experiment.

- [R129 — seccomp user-notification broker limits](R129_SECCOMP_USER_NOTIFICATION_BROKER_GATES.md): Linux seccomp primary contracts distinguish listener-mediated Linux syscall emulation from authenticated NT syscall provenance; identifies continuation races, listener liveness, collision and performance gates. Documentation only; no experiment.

- [R130 — Windows child-process creation transaction versus Linux exec](R130_CHILD_PROCESS_CREATION_TRANSACTION_GATES.md): directly inspected Microsoft CreateProcessW/creation-flags and Linux execve documentation; distinguishes guest child/primary-thread publication, suspended startup, token and inherited-handle boundaries from Linux image replacement. Proposed differential/rollback gates only; no experiment.

- [R131 — restricted tokens and mandatory integrity gates](R131_RESTRICTED_TOKENS_MANDATORY_INTEGRITY_GATES.md): Microsoft restricted-token two-pass SID checks and mandatory integrity policy contrasted with Linux user namespaces/credentials; proposed Windows-reference and host-bypass tests. Primary-source review only; no experiment.

- [R132 — job completion packet loss, PID reuse and drain observability](R132_JOB_COMPLETION_PID_REUSE_DRAIN_GATES.md): Microsoft job completion-port and Linux cgroup/pidfd primary contracts; separates lossy guest notifications, stable process identity, nested forwarding, last-handle kill and authoritative membership reconciliation. Proposed differential/security gates only; no experiment.

- [R133 — Windows job CPU rate versus Linux cgroup scheduling](R133_JOB_CPU_RATE_CONTROL_CGROUP_GATES.md): Microsoft and Linux primary contracts distinguish cycle-based job hard caps, 1–9 weights and min/max reservations from cgroup time quotas, 1–10000 weights and utilization clamps; identifies hierarchy, scheduler-class bypass and differential enforcement gates. Documentation only; no experiment.

- [R134 — job UI restrictions versus Linux desktop authority](R134_JOB_UI_RESTRICTIONS_DESKTOP_AUTHORITY_GATES.md): Microsoft job UI restriction flags and Linux cgroup documentation isolate USER-handle, clipboard, global-atom, desktop and display policy from resource enforcement; proposes reference/security gates. Documentation only; no experiment.

- [R135 — Windows opportunistic locks versus Linux file leases](R135_OPLOCK_LINUX_LEASE_GATES.md): primary Microsoft/Linux contracts distinguish oplock grants, asynchronous break completion, acknowledgments and directory oplocks from Linux signal-driven file leases. Differential/security gates proposed; no experiment.

- [R136 — NTFS USN change journal versus Linux event watches](R136_USN_CHANGE_JOURNAL_LINUX_EVENT_GATES.md): Microsoft/Linux primary contracts distinguish persistent volume replay, journal generation and file-reference enumeration from inotify/fanotify queues; proposes Windows-reference and Linux gap tests. Documentation only; no experiment.

- [R137 — io_uring cancellation CQE versus target completion lifetime](R137_IO_URING_CANCEL_TARGET_CQE_LIFETIME.md): Microsoft and liburing primary documentation distinguish cancellation requests from terminal I/O completion; proposes generation-tagged request identity, buffer-lifetime and dual-CQE race gates. No experiment.

- [R98 — NT service identity and Nt/Zw caller-mode gates](R98_NT_SERVICE_IDENTITY_CALLER_MODE.md): Microsoft primary contracts distinguish named ntdll exports, user-mode Nt/Zw equivalence, build-sensitive native interfaces, and raw syscall dispatch. Proposes Windows-reference and Linux fixture gates; no experiment.

- [R138 — upstream SUD inclusive/exclusive mode drift and capability gates](R138_SUD_INCLUSIVE_EXCLUSIVE_VERSION_GATES.md): pinned upstream-master documentation contrasts with versioned v6.12; no runtime test, release availability or speedup asserted.

- [R139 — Windows write-watch versus Linux soft-dirty reset scope](R139_WRITE_WATCH_SOFT_DIRTY_RESET_GATES.md): Microsoft region-scoped dirty-page query/reset contracts contrasted with Linux process-wide soft-dirty clear_refs; identifies independent-allocation and concurrent-writer conformance gates. Documentation only; no experiment.

- [R140 — Windows per-file-handle I/O priority hints versus Linux task I/O priority](R140_FILE_IO_PRIORITY_HINT_SCOPE_GATES.md): Microsoft and Linux primary contracts distinguish guest file-handle-scoped hints from host task-scoped ioprio; proposes isolated submission and Windows/Linux differential gates. Documentation only; no experiment.

- [R141 — x64 extended-state context and feature negotiation](R141_XSTATE_CONTEXT_FEATURE_NEGOTIATION_GATES.md): Microsoft context/XSTATE, Intel CPUID/XSAVE and Linux signal-state primary contracts; separates ordinary ABI preservation from full guest context capture and feature policy. Proposed reference and Linux gates only; no experiment.

- [R142 — Windows NUMA topology and Linux placement scope](R142_NUMA_TOPOLOGY_PLACEMENT_GATES.md): Microsoft NUMA topology/allocation and Linux memory-policy primary contracts distinguish guest node identity, preferred placement, per-thread policy, VMA and shared-mapping scope, and cpuset restrictions. Proposed Windows/Linux differential and isolation gates; no experiment.

- [R143 — orderly versus forced process termination transaction](R143_PROCESS_TERMINATION_TRANSACTION_GATES.md): Microsoft ExitProcess/TerminateProcess and Linux exit_group primary contracts distinguish DLL detach, asynchronous remote termination, I/O draining and durable guest process identity. Differential/security gates proposed; no experiment.

- [R144 — PE CFG loader and valid-call-target gates](R144_PE_CFG_LOADER_TARGET_GATES.md): Microsoft primary documentation distinguishes load-config GFIDS and patched check/dispatch pointers, per-process valid-target state, export suppression and strict-mode DLL policy from Linux page protection; proposes Windows-reference, parser, security and performance gates. Documentation only; no experiment.

- [R145 — Windows synchronous I/O cancellation versus Linux request cancellation](R145_SYNC_IO_CANCELLATION_THREAD_SCOPE_GATES.md): Microsoft thread-scoped CancelSynchronousIo and file-scoped CancelIo/CancelIoEx contracts contrasted with Linux io_uring cancellation; identifies request identity, rights, completion lifetime and differential race gates. Primary-source research only; no experiment.

- [R146 — NT handle reuse, close/wait races and object-reference ownership](R146_HANDLE_REUSE_WAIT_RACE_GATES.md): Microsoft and Linux primary API contracts distinguish undefined concurrent close/wait guest behavior from required internal object-reference safety, guest slot ABA protection, and host fd reuse. Proposed differential and stress gates only; no experiment.

- [R147 — Object Manager symbolic-link resolution and Linux path boundaries](R147_OBJECT_MANAGER_SYMBOLIC_LINK_RESOLUTION_GATES.md): Microsoft Object Manager link APIs and Linux openat2 contracts distinguish guest link identity, root-relative lookup, OBJ_OPENLINK and unresolved OBJ_DONT_REPARSE applicability from host filesystem traversal. Windows-reference and security gates proposed; no experiment.

- [R148 — Object Manager directory enumeration versus filesystem enumeration](R148_OBJECT_DIRECTORY_ENUMERATION_GATES.md): Microsoft NtOpenDirectoryObject/NtQueryDirectoryObject contracts distinguish typed guest namespace entries, directory query rights and continuation buffers from NtQueryDirectoryFile; proposes Windows-reference buffer, access, concurrency and isolation gates. Documentation only; no experiment.

- [R149 — Windows thread-pool work callback and teardown gates](R149_THREADPOOL_WORK_CALLBACK_LIFETIME_GATES.md): Microsoft primary API contracts distinguish repeated concurrent work submissions, cancellation of pending callbacks, asynchronous close and DLL/callback-environment lifetime. Proposed reference and Linux gates; no experiment.

- [R150 — Threadpool wait registration, timeout and teardown gates](R150_THREADPOOL_WAIT_REGISTRATION_LIFETIME_GATES.md): Microsoft primary contracts distinguish one-shot handle registration, FILETIME timeout sign, disarm versus queued callback cancellation, and asynchronous wait-object closure. Proposed Windows-reference and Linux architecture gates only; no experiment.

- [R152 — threadpool timer period, coalescing window and teardown](R152_THREADPOOL_TIMER_PERIOD_SLACK_LIFETIME_GATES.md): Microsoft primary API contracts distinguish one-shot/periodic scheduling, UTC versus awake-time deadlines, SetThreadpoolTimerEx race return, and queued versus executing callback lifetime. Proposed differential gates only; no experiment.

- [R153 — threadpool cleanup-group membership and teardown gates](R153_THREADPOOL_CLEANUP_GROUP_TEARDOWN_GATES.md): Microsoft primary contracts establish aggregate member release, pending callback cancellation, running callback drain, group reuse and documented concurrent creation races; proposes lifetime and Windows differential gates. No experiment.

- [R154 — threadpool capacity, callback priority and asynchronous pool close](R154_THREADPOOL_POOL_CAPACITY_PRIORITY_GATES.md): Microsoft primary contracts distinguish per-pool thread limits, callback-environment routing, priority of pending callbacks and asynchronous pool closure. Proposed Windows-reference, race and lifetime gates only; no experiment.

- [R155 — threadpool callback disassociation versus execution and cleanup-group lifetime](R155_THREADPOOL_CALLBACK_DISASSOCIATION_GATES.md): Microsoft primary contracts distinguish object-specific callback wait release from actual callback return and group/module ownership; proposes Windows-reference and mock-scheduler tests. Documentation only; no experiment.

- [R157 — threadpool stack policy and persistent-worker gates](R157_THREADPOOL_STACK_PERSISTENT_WORKER_GATES.md): Microsoft primary contracts distinguish new-worker-only stack reserve/commit settings from live worker stacks, persistent-thread selection from long-running hints, and pool policy from worker identity. Windows-reference and Linux differential gates proposed; no experiment.

- [R158 — simple threadpool submission and in-callback long-running admission](R158_THREADPOOL_SIMPLE_SUBMISSION_LONG_RUNNING_GATES.md): Microsoft primary contracts distinguish handleless simple callback submission from reusable TP_WORK and distinguish CallbackMayRunLong's capacity BOOL from its unconditional long-running marker. Proposed reference and Linux ownership gates only; no experiment.

- [R159 — activation-context stack, callback propagation and SxS binding gates](R159_ACTIVATION_CONTEXT_STACK_CALLBACK_BINDING_GATES.md): Microsoft primary contracts distinguish thread-local activation stacks, cookie validation, reference ownership and context-dependent binding from host loader state; proposes Windows-reference and security gates only, no experiment.

- [R160 — threadpool I/O notification accounting and overlapped-request lifetime](R160_THREADPOOL_IO_NOTIFICATION_LIFETIME_GATES.md): Microsoft primary API contracts distinguish Start/Cancel notification credits, underlying I/O completion and queued/running callback lifetimes; proposed differential gates only, no experiment.

- [R161 — critical-section recursion, ownership, spin and lifetime gates](R161_CRITICAL_SECTION_RECURSION_LIFETIME_GATES.md): Microsoft primary API contracts distinguish recursive process-local critical sections from SRW locks, document invalid teardown and owner-death boundaries, and propose Windows-reference and Linux safety gates. Documentation only; no experiment.

- [R162 — bound-import metadata and rebinding gates](R162_BOUND_IMPORT_REBINDING_GATES.md): Microsoft PE-format and data-directory primary contracts distinguish ordinary prebound IAT state from original lookup and delay-bound metadata. Proposes conservative rebinding, parser-security and Windows-reference gates. Documentation review only; no experiment.

- [R163 — API-set contract resolution and host-module identity gates](R163_API_SET_CONTRACT_LOADER_GATES.md): Microsoft primary documentation distinguishes virtual API-set contracts, OS-specific schema-to-host mapping, dynamic/late binding and capability queries from physical DLL paths and export availability. Proposes versioned namespace and Windows-reference/security gates. Documentation only; no experiment.

- [R164 — SUD SIGSYS alternate-stack and continuation lifetime gates](R164_SUD_SIGSYS_ALTSTACK_CONTINUATION_GATES.md): Linux sigaltstack/SS_AUTODISARM, sigreturn and signal-safety contracts expose suspended-handler-frame ownership and blocking NT service continuation hazards; proposes kernel capability, repeated-trap, per-thread and cancellation tests. Documentation only; no experiment.

- [R165 — Windows fiber switching, FLS and thread identity](R165_FIBER_SWITCH_FLS_THREAD_IDENTITY_GATES.md): Microsoft primary contracts distinguish cooperative fiber stacks and per-fiber FLS from executing-thread TLS and identity; proposes Windows-reference migration, lifetime and Linux ABI/CET/XSTATE gates. No experiment.

- [R166 — keyed-event rendezvous versus Linux futex wake](R166_KEYED_EVENT_RENDEZVOUS_FUTEX_GATES.md): Wine source and Linux futex contracts distinguish paired keyed wait/release from address-based wake; Windows-reference, security and concurrency gates proposed. No experiment.

- [R167 — Windows delayed clipboard rendering versus Wayland selection ownership](R167_CLIPBOARD_DELAYED_RENDER_WAYLAND_SELECTION_GATES.md): Microsoft clipboard ownership, render messages and window-station sequence contracts contrasted with Wayland focus-scoped offers; proposed lifecycle, access-control and differential gates. Documentation only; no experiment.

- [R168 — Windows condition-variable lock modes versus pthread waits](R168_CONDITION_VARIABLE_LOCK_MODE_GATES.md): Microsoft and POSIX primary contracts distinguish atomic guest critical-section/SRW shared-or-exclusive release-and-reacquire from pthread mutex-only waits, with timeout, cancellation and lost-wakeup gates. Documentation only; no experiment.

- [R169 — Windows INIT_ONCE retry and asynchronous publication versus pthread_once](R169_INIT_ONCE_RETRY_ASYNC_PTHREAD_ONCE_GATES.md): Microsoft and POSIX primary contracts distinguish Windows synchronous retry after failure, competing asynchronous completion, context publication and check-only semantics from pthread_once's void callback and one-time normal completion. Proposed Windows-reference and Linux state-machine gates; documentation only, no experiment.

- [R170 — explicit Windows large-page allocation versus Linux HugeTLB/THP](R170_LARGE_PAGE_ALLOCATION_HUGETLB_GATES.md): Microsoft and Linux primary documentation distinguishes privilege-gated, explicit large-page guarantees from opportunistic THP and Linux pool/quota constraints. Proposed differential/security gates only; no experiment.

- [R171 — Windows placeholder reservations and Linux fixed-address replacement](R171_PLACEHOLDER_VIRTUAL_ADDRESS_REPLACEMENT_GATES.md): Microsoft VirtualAlloc2/VirtualFree/MapViewOfFile3/UnmapViewOfFile2 placeholder split, exact-match replacement, coalescing and restoration contracts contrasted with Linux mmap/MAP_FIXED/MAP_FIXED_NOREPLACE. Proposes a guest interval ledger, collision/rollback security gates and Windows/Linux differential tests. Documentation only; no experiment.

- [R172 — KUSER_SHARED_DATA fixed guest mapping versus Linux vDSO](R172_KUSER_SHARED_DATA_VDSO_GATES.md): Microsoft shared-data, interrupt-time and Linux vDSO primary contracts distinguish fixed-address guest memory, versioned layout, clock epochs and read-only publication from host time fast paths. Proposed Windows-reference, mapping-security and clock-conformance gates only; no experiment.

- [R173 — DLL search policy, module identity and host-path containment](R173_DLL_SEARCH_POLICY_MODULE_IDENTITY_GATES.md): Microsoft primary documentation establishes version/profile-dependent DLL search precedence, transitive dependency lookup, LoadLibraryEx flag distinctions and loaded-module identity; proposes confined guest resolution and differential/security gates. Documentation only; no experiment.

- [R174 — Windows dynamic-code mitigation versus Linux memory protections](R174_DYNAMIC_CODE_POLICY_LINUX_MEMORY_GATES.md): Microsoft mitigation-policy and Linux mmap/mprotect/seccomp primary contracts distinguish guest policy state, thread opt-out, audit and creation-time restrictions from host page permissions. Proposes differential, mediation and bypass-security gates. Documentation only; no experiment.

- [R175 — COM apartment identity, message pumping and marshaling gates](R175_COM_APARTMENT_MESSAGE_PUMP_GATES.md): Microsoft primary COM contracts identify per-thread initialization, STA dispatch, cross-apartment proxies and mode-change errors as a guest USER32/COM integration gate. Windows-reference tests proposed; no experiment.

- [R176 — QPC frequency and Linux clock-domain translation](R176_QPC_CLOCK_DOMAIN_TRANSLATION_GATES.md): Microsoft QPC/QPF and Linux clock_gettime contracts distinguish stable guest counter frequency, epoch/clock selection, host slewing, overflow and shared-data consistency. Proposes Windows-reference and Linux differential gates; documentation only, no experiment.

- [R177 — Windows working-set policy and Linux reclaim gates](R177_WORKING_SET_MEMORY_PRIORITY_LINUX_RECLAIM_GATES.md): Microsoft and Linux primary contracts distinguish process resident working-set bounds, hard flags, memory priority and trim from Linux madvise, mlock and cgroup memory controls. Differential/security gates proposed; no experiment.

- [R178 — mapped-file fault provenance and NT in-page exception gates](R178_MAPPED_FILE_FAULT_SIGBUS_NT_INPAGE_GATES.md): Microsoft EXCEPTION_IN_PAGE_ERROR and Linux SIGBUS contracts distinguish backing I/O failures from EOF/truncation and host signal provenance. Windows-reference, Linux fault and guest-ingress tests proposed; no experiment.

- [R179 — child-process restriction, token authority and Linux broker bypass gates](R179_CHILD_PROCESS_RESTRICTION_TOKEN_BROKER_GATES.md): Microsoft creation-time child-policy, privileged-handle bypass and alternate-parent contracts contrasted with Linux clone semantics; proposes security and Windows/Linux differential gates. Documentation only; no experiment.

- [R180 — CFG indirect-call target validity and Linux enforcement boundary](R180_CFG_CALL_TARGET_POLICY_GATES.md): Microsoft CFG and process-mitigation primary contracts distinguish per-target validity, export suppression and strict module policy from Linux page execution permissions. Proposes loader/state-machine and Windows-reference security gates; no experiment.

- [R181 — ETW provider/session compatibility and Linux tracing boundaries](R181_ETW_PROVIDER_SESSION_COMPATIBILITY_GATES.md): Microsoft primary ETW provider, session, controller, consumer and filter contracts distinguish guest tracing semantics from Linux host instrumentation; proposes Windows-reference, security, lifecycle and performance gates. Documentation only; no experiment.

- [R182 — image-load provenance, integrity and signature mitigation gates](R182_IMAGE_LOAD_PROVENANCE_SIGNATURE_POLICY_GATES.md): Microsoft image-load and binary-signature mitigation contracts distinguish remote-device, Low mandatory-integrity and signer restrictions from DLL search, executable-page permission and host filesystem trust; proposes reference and Linux security gates. Documentation only; no experiment.

- [R183 — Win32k lockdown and GUI-thread transition gates](R183_WIN32K_LOCKDOWN_GUI_THREAD_GATES.md): Microsoft primary mitigation contracts distinguish blocked guest NTUser/GDI service entry and GUI-thread conversion from generic Linux syscalls or user-mode API names. Proposes Windows-reference, broker authorization and race/security gates; documentation only, no experiment.
