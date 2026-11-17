# R144 — PE Control Flow Guard loader and target-state gates

Status: **VERIFIED FROM SOURCE** for the cited Microsoft contracts; **HYPOTHESIS** for proposed LinuxNTAlpha design; **OPEN** for Windows-version-specific conformance. Reviewed 2026-11-17. No experiment performed.

## Why this is an integration blocker

The existing PE64 fixture and IAT experiments establish narrow native execution, not correct handling of compiler-instrumented indirect calls. CFG is not just a PE parser flag or Linux executable-page permission: Windows combines image metadata, loader-patched check/dispatch pointers, per-process valid-target state, and process mitigation policy. Treating all executable addresses as permanently valid could make a sample run but silently discard a requested security property.

## Primary-source findings

1. **VERIFIED FROM SOURCE — image metadata and loader writes.** Microsoft's *PE metadata* documentation specifies a sorted GuardCFFunctionTable (GFIDS) of RVAs, with entry stride 4 plus the additional metadata-byte count encoded in GuardFlags. Defined optional flags include explicitly suppressed targets and export-suppressed targets. It describes GuardCFCheckFunctionPointer and, on AMD64, GuardCFDispatchFunctionPointer, which the OS loader updates while transiently changing memory protection. It also identifies IMAGE_GUARD_CF_INSTRUMENTED, IMAGE_GUARD_CF_FUNCTION_TABLE_PRESENT and IMAGE_DLLCHARACTERISTICS_GUARD_CF. Source: https://learn.microsoft.com/en-us/windows/win32/secbp/pe-metadata (consulted 2026-11-17). This is an evolving documentation contract, **not** a pinned Windows kernel implementation.

2. **VERIFIED FROM SOURCE — enforcement is a runtime property.** Microsoft's *Control Flow Guard for platform security* states that the compiler inserts indirect-call checks and identifies valid targets, while the Windows runtime maintains valid-target state and checks calls; failed checks terminate the process. It states executable committed pages are normally treated as valid targets by VirtualAlloc/VirtualProtect, with PAGE_TARGETS_INVALID / PAGE_TARGETS_NO_UPDATE affecting that behavior. Source: https://learn.microsoft.com/en-us/windows/win32/secbp/control-flow-guard (consulted 2026-11-17).

3. **VERIFIED FROM SOURCE — dynamic target API.** SetProcessValidCallTargets requires an executable region and ordered 16-byte-aligned target offsets; it fails when CFG is not enabled for the target process. Source: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-setprocessvalidcalltargets (consulted 2026-11-17).

4. **VERIFIED FROM SOURCE — mitigation modes are distinct.** PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY documents EnableControlFlowGuard, EnableExportSuppression, StrictMode, and XFG-related fields. Export suppression makes eligible exports invalid until dynamically resolved via GetProcAddress; strict mode refuses DLLs without CFG and can be enabled after process startup but not subsequently disabled. Source: https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_control_flow_guard_policy (consulted 2026-11-17). Do not infer uniform behavior across all Windows builds from this API documentation.

5. **VERIFIED FROM SOURCE — page-protection interface is not enough.** The documented PAGE_TARGETS_INVALID / PAGE_TARGETS_NO_UPDATE modifiers concern CFG target state, not merely readable/writable/executable permissions. Source: https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants (consulted 2026-11-17).

## Proposed architecture, not a decision

**HYPOTHESIS:** Maintain guest-process CFG policy and a versioned valid-target map separately from host mmap/mprotect state. The PE loader should validate bounded load-config metadata, construct target state, and install ABI-correct check/dispatch thunks before executing instrumented code. Guest VirtualAlloc/VirtualProtect and SetProcessValidCallTargets should update that map according to guest policy; GetProcAddress should cooperate with export suppression. Host NX/W^X remains an independent defense and cannot replace CFG. No kernel extension is justified by these sources alone.

**OPEN:** Exact loader initialization ordering, mixed instrumented/uninstrumented modules, XFG call-site signatures, Windows-build differences, races during concurrent target updates, thunk ABI preservation, and treatment of invalid or malformed GFIDS. In particular, a successful indirect call on Linux is not evidence of CFG enforcement.

## Falsifiable test gates

1. **Parse-only:** build a corpus with instrumented and non-instrumented PE images; inspect load-config size, flags, sortedness, table bounds, stride and metadata. Mutate one field at a time and reject malformed reads without executing guest bytes.
2. **Windows reference:** compile a minimal /guard:cf x64 DLL/EXE using a recorded MSVC version; record dumpbin /headers /loadconfig, Windows build, mitigation policy, valid and invalid indirect-call outcomes, and export-suppression behavior. Use disposable, isolated test processes.
3. **LinuxNTAlpha:** use the *same* trusted fixture to verify loader thunk patching, valid/invalid target behavior, explicit PAGE_TARGETS_INVALID and NO_UPDATE transitions, and dynamic target updates; compare statuses and termination semantics to the Windows reference.
4. **Concurrency/security:** test loader/unloader and JIT-style target updates racing with indirect calls; ensure no stale target is retained after guest image lifetime ends. Validate that guest policy cannot overwrite host process code or security-sensitive memory.
5. **Performance:** only after correctness, measure check/dispatch overhead with matching enabled/disabled controls and confidence intervals. Do not infer a speedup from native instruction execution.

## Relation to prior work

Complements R19/R20/R28 PE/import fixtures, R51 export resolution, R69 dynamic-code mitigation, R91 loader rollback, R109 CET shadow stacks and R117 PE page protection. CFG is a distinct software-enforced indirect-call target contract; CET does not automatically satisfy it. This note adds no test result and makes no compatibility or performance claim.
