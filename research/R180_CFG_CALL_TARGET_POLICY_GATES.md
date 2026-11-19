# R180 — Control Flow Guard target validity and Linux enforcement boundary

Status: **VERIFIED FROM SOURCE** for documented API contracts; **HYPOTHESIS** for proposed LinuxNTAlpha implementation. Research-only; **no experiment run**. Reviewed 2026-11-19.

## Architectural question

Can LinuxNTAlpha preserve Windows Control Flow Guard (CFG) policy for native x86-64 PE execution without confusing Linux executable-page permission with the Windows set of permitted *indirect branch destinations*?

## Verified contracts (primary sources)

1. Microsoft's [Control Flow Guard overview](https://learn.microsoft.com/en-us/windows/win32/secbp/control-flow-guard) describes compiler-inserted indirect-call checks and a runtime-maintained set of valid call targets. The document explicitly permits mixtures of CFG-aware and CFG-unaware code and says failed CFG checks terminate the process. It also describes default executable committed pages as valid indirect-call targets, subject to `PAGE_TARGETS_INVALID` and `PAGE_TARGETS_NO_UPDATE` behavior. This is a documented Windows behavior, **not** a claim that Linux enforces it.
2. [SetProcessValidCallTargets](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-setprocessvalidcalltargets) changes CFG target validity within an executable memory region. Its target offsets must be 16-byte aligned and ascending; the API fails if CFG is not enabled for the process. A successful `mprotect(PROT_EXEC)` alone therefore cannot establish Windows CFG target-state equivalence.
3. Microsoft's [PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_control_flow_guard_policy) distinguishes `EnableControlFlowGuard` (not mutable through SetProcessMitigationPolicy), `EnableExportSuppression` (exports initially invalid until dynamically resolved), and `StrictMode` (reject DLLs without CFG; can be enabled but not disabled after start). It also exposes XFG-related flags; their enforcement semantics require a separate audit.
4. The Linux [mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html) and [mprotect(2)](https://man7.org/linux/man-pages/man2/mprotect.2.html) interfaces control virtual mappings and page permissions, not Windows CFG per-target metadata. This comparison is limited to those documented interfaces; it does **not** prove that every Linux architecture lacks other control-flow mechanisms.

## Consequences for the NT-on-Linux boundary

**HYPOTHESIS:** Track per-guest-process CFG policy, PE load-configuration metadata, image ownership, executable intervals and call-target validity in a dedicated guest memory-policy service. Module load/unload, relocation, export resolution, `VirtualAlloc`, `VirtualProtect`, `SetProcessValidCallTargets`, and process creation must use one coherent state transition protocol. The PE loader cannot be treated as an unrelated mapping utility.

**Security requirement:** A software-only target bitmap is *not* enforcement unless every relevant instrumented guest check actually consults it and the guest cannot substitute a writable check function or alter the bitmap. Host `mprotect`, seccomp and Landlock do not automatically implement Windows CFG. The uninstrumented-guest and mixed-module cases are separate threat-model questions. Never advertise CFG protection merely because policy APIs return success.

**OPEN:** Determine the actual PE load-config GuardFlags, guard-function pointers, FID tables, export suppression behavior, forwarders and delay imports for pinned Microsoft-toolchain-built fixtures. Determine whether a user-space implementation can faithfully expose Windows process mitigation status without allowing guest-controlled bypass. Hardware CET/IBT and XFG are **not** assumed equivalent to CFG.

## Falsifiable test plan (not performed)

1. On a controlled Windows reference machine, build signed-off test fixtures with CFG enabled and disabled, inspect their PE load configurations with documented tooling, and record Windows build, compiler/linker versions and exact command lines.
2. Compare indirect calls to valid and invalid targets; test executable allocation defaults, `PAGE_TARGETS_INVALID`, `PAGE_TARGETS_NO_UPDATE`, ordered and misaligned target-offset inputs, and CFG-disabled `SetProcessValidCallTargets` errors.
3. Compare export suppression before/after `GetProcAddress`, module unload/reload and strict-mode rejection of a non-CFG DLL. Record actual process termination versus API errors; run faulting cases in separate child processes.
4. On Linux, separately test only mapping/protection mechanics and the proposed guest bitmap state machine. A passing state-machine test does **not** establish CFG enforcement or Windows compatibility.
5. Add negative tests for races between dynamic target updates, unmap/reuse, thread dispatch and image teardown. Fail closed on stale target ownership, and document any incompatibility.

## Evidence limitations and relationship to prior work

This note isolates **indirect-call target validity and mitigation state**; it does not repeat R109 (CET shadow stacks), R174 (dynamic-code mitigation and page protections), R162 (bound imports), or R163 (API-set contracts). No Windows differential run, Linux host run, performance benchmark, or security proof was performed. The Microsoft documentation is live and not pinned to a Windows build; exact version-dependent behavior remains **OPEN**.
