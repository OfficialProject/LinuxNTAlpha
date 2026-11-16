# R115 — Minimum viable NT runtime: dependency-ordered integration gates

Status: **architecture synthesis, documentation-only** (2026-11-16). No new experiment, benchmark, Windows-reference observation, or code execution is claimed.

## Question

What is the smallest *integrated* demonstration that would materially advance LinuxNTAlpha beyond its existing independent PE/ABI/GS/syscall fixtures, without falsely claiming general Windows compatibility?

This note synthesizes existing project experiments and upstream interface contracts. It is **not** another PE loader, syscall, or process-semantics audit.

## Verified project baseline

- [R19](R19_PE64_EXECUTION_EXPERIMENT.md) executes a freestanding PE32+ fixture at a relocated base. This does not initialize an ordinary Windows process.
- [R20](R20_PE64_IAT_BRIDGE.md) binds one synthetic PE import to an MS-ABI host stub. This does not resolve general DLL dependencies.
- [R24](R24_GS_TEB_CONTEXT.md) and [R25](R25_PE64_GS_TIB_EXECUTION.md) demonstrate synthetic GS-based thread-local reads, not an authentic Windows TEB/PEB lifecycle.
- [R26](R26_PE64_RAW_SYSCALL_SUD.md) intercepts one synthetic guest raw syscall; [R90](R90_GUEST_HOST_SYSCALL_PROVENANCE.md) identifies nested transitions and containment as unresolved.
- [R28](R28_NTDLL_RTL_COMPARE_MEMORY.md) independently implements one documented NTDLL export, not a usable NTDLL.
- [R22](R22_SYSCALL_DISPATCH_LATENCY.md) is a narrow trap microbenchmark, not an application performance comparison.

**Inference:** The missing milestone is not another isolated primitive; it is a repeatable **single-process lifecycle** that composes the primitives and checks teardown/failure paths.

## Primary-source boundary

- Microsoft PE/COFF specification (image loading, relocations, import tables, TLS, exception directory): https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- Microsoft x64 calling convention (registers, shadow space, unwind rules): https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- Microsoft DLL entry point requirements: https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function
- Linux Syscall User Dispatch (compatibility, explicitly **not** a security mechanism): https://docs.kernel.org/admin-guide/syscall-user-dispatch.html
- Linux seccomp filtering (distinct enforcement layer): https://docs.kernel.org/userspace-api/seccomp_filter.html

These links are source identifiers for review; **their current revisions were not fetched or independently revalidated during this run**. Existing project results above are evidence only for their documented fixtures and environments.

## Proposed milestone M1 (HYPOTHESIS; NOT IMPLEMENTED)

**Definition:** Execute one deterministic, self-built x86-64 PE32+ program with at least two imports from independently implemented, named guest-facing runtime modules, with two guest threads and explicit startup/teardown, and verify all observable return values and lifecycle events. The program must be linked for Windows x64 and run as native machine code on Linux, without a Wine/Proton runtime.

The fixture should exercise:

1. **Loader:** parse-only preflight; bounded multi-descriptor import lookup; image relocation; explicit failure on unknown import, invalid RVA or unsupported directory. Keep untrusted PE execution out of the parse-only test process. See [R23](R23_PARSE_ONLY_FUZZ_GATE.md), [R44](R44_PE64_IMPORT_PARSER_BOUNDARIES.md), [R51](R51_PE_EXPORT_FORWARDERS_RESOLUTION.md).
2. **Thread initialization:** construct documented guest-visible TLS/GS state for both threads, with distinct thread identities; verify host Linux FS-based TLS remains intact. Do not equate a synthetic GS pointer with a complete TEB. See R24/R25, [R27](R27_PE64_TLS_LOADER_GATES.md).
3. **ABI:** call from PE into two host implementations and back, with nested callbacks and nonvolatile GPR/XMM checks; a returned scalar alone is insufficient. See [R17](R17_ABI_BRIDGE_EXPERIMENT.md), [R55](R55_CROSS_ABI_SIMD_PRESERVATION_GATES.md).
4. **Dispatch:** repeat two synthetic guest raw syscall numbers across both threads with an intervening imported host call; ensure no colliding guest number reaches the Linux syscall table. Preserve nested selector state. This is **compatibility correctness**, not containment. See R26/R90.
5. **Lifetime:** deterministic module initialization and thread shutdown, with injected failure at each phase. Verify no stale guest thread registration, no leaked host mappings or handles, and no invocation of callbacks from unloaded code. See [R45](R45_DLL_LOADER_LOCK_LIFECYCLE.md), [R64](R64_DLL_DEPENDENCY_GRAPH_LIFECYCLE.md), [R91](R91_DLL_LOADER_LOCK_NOTIFICATION_ROLLBACK.md).

### Required negative tests

- Reject malformed image and import metadata **without** executing guest entry code.
- Inject failure after mapping, relocation, import resolution, thread setup and first callback; compare resource inventory against baseline after each teardown.
- Re-enter guest code from a host callback and force a guest syscall; detect any selector leakage.
- Test parallel thread exits and callback ordering; record deadlock/timeouts rather than treating a hang as PASS.
- Test unsupported NT service numbers; reject deterministically instead of falling through to host Linux syscalls.

## Decision rules (proposed, not established)

**M1 PASS** requires all positive and negative cases with reproducible source, exact toolchain/kernel identifiers and recorded outputs. A missing capability must be **SKIP**, not PASS. A single positive execution is insufficient.

**M1 is not:** Windows app compatibility, a Windows-compatible process environment, full DLL semantics, exception/unwind conformance, sandboxing, a benchmark victory, driver support, or anti-cheat support.

**M2 after M1:** Windows-reference differential tests for the same APIs, PE TLS callbacks, exception/unwind, process handles and synchronization. Select the kernel boundary only after comparing a user-mode path with alternatives on equivalent workloads; [DECISIONS.md](DECISIONS.md) ADR-003 remains OPEN.

## Highest-leverage next implementation task

Build a **test-only integration harness** around existing R19–R28 sources, preserving their independent test entry points. Before writing code, inventory the actual current experiment interfaces and fixture build commands. Do not assume their APIs can be composed without changes. Implement the loader's bounded multi-import *parse-only* acceptance tests first, then add one integration feature at a time with regression checks.

## Nonduplication and remaining unknowns

R19–R28 document isolated positive experiments; R44, R55, R64 and R90 document individual contract boundaries. R115 adds an **explicit dependency order and a falsifiable integration acceptance milestone**. Unknowns include whether existing harness APIs permit clean composition, whether nested SUD transitions can be made correct, and which real Windows API should be the first differential conformance target. No outcome is predicted.
