# R217 — Dynamic CFG target mutation and Linux CET non-equivalence gates

Research date: 2026-11-21. Scope: Windows x64 guest CFG policy under a Linux-hosted NT runtime. **Documentation analysis only; no implementation, Windows reference test, Linux experiment, or performance measurement was performed.**

## Distinct question and relation to earlier work

R216 establishes the PE GFIDS parser-to-loader trust boundary. R180 addresses process-level CFG policy. This note addresses a separate **runtime mutation** boundary: dynamically allocated executable pages, JIT target changes, export suppression and cross-process authorization. It also tests the tempting but invalid assumption that Linux x86 CET support supplies Windows CFG.

## VERIFIED — documented primary-source contracts (not reproduced observations)

1. Microsoft documents that CFG is jointly compiler- and runtime-enforced: instrumented indirect calls consult a valid-target policy maintained by the Windows runtime/kernel. CFG-aware and unaware modules may coexist, with protection gaps. Windows normally treats committed executable memory allocated through VirtualAlloc or changed through VirtualProtect as valid indirect-call targets unless callers use the relevant target-policy protection flags. Source: [Microsoft Control Flow Guard](https://learn.microsoft.com/en-us/windows/win32/secbp/control-flow-guard).
2. `SetProcessValidCallTargets` takes a **target process handle**, executable virtual-address range and ordered 16-byte-aligned target offsets, with per-offset flags; it fails when CFG is not enabled for the target process. Consequently, a guest process's authorization to modify another guest process's target set is not implied by host process access or `mprotect` success. Source: [Microsoft SetProcessValidCallTargets](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-setprocessvalidcalltargets).
3. Microsoft's process mitigation structure distinguishes `EnableControlFlowGuard`, `EnableExportSuppression`, `StrictMode`, and XFG-related policy bits. Export suppression initially invalidates exports and makes them valid when dynamically resolved through `GetProcAddress`; strict mode can reject DLLs without CFG and cannot be disabled once enabled. These are **process policy** semantics, not simply properties of a PE image's GFIDS. Source: [Microsoft PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_control_flow_guard_policy).
4. Linux's upstream x86 CET documentation explicitly distinguishes **shadow stack**, which checks return-address integrity, from **indirect branch tracking (IBT)**, which checks ENDBR-marked indirect branch destinations. The documented 64-bit Linux support is **userspace shadow stack and kernel IBT**, not userspace IBT. Even a future userspace IBT facility would not by itself reproduce Windows CFG's mutable, per-process allowed-target set or export-suppression behavior. Source: [Linux kernel x86 shadow stack documentation](https://docs.kernel.org/next/x86/shstk.html).
5. The Linux kernel documentation says userspace shadow stack requires hardware, kernel configuration and loader/runtime enablement; the kernel does not automatically interpret ELF CET feature notes. Therefore neither the presence of CET-capable hardware nor a PE's CFG load-config bits establishes that guest protection is active. Source: [Linux kernel x86 shadow stack documentation](https://docs.kernel.org/next/x86/shstk.html).

## HYPOTHESIS — candidate separation of responsibilities

Maintain a guest **process-owned call-target registry** distinct from the host's executable mappings and the R216 per-image immutable GFIDS. An allocation/mapping event creates a generation-tagged range with a guest target-validity policy; executable protection transitions, JIT updates and image unload/reload mutate or retire it under synchronization. A target-change API should resolve and authorize the **guest** process handle and validate the range and offsets before committing any changes. The helper used by compiler-instrumented guest indirect calls must consult the same published policy. These are design candidates, **not verified Windows internal implementation details**.

Model export suppression as a separate state transition: the export's image metadata and code mapping can exist while its indirect-call validity is withheld until a qualifying guest resolution. Whether the transition is per-process, per-address, per-image or affected by loader flags requires Windows reference tests; do not infer exact granularity from prose.

Treat CET shadow stack as a potentially complementary return-integrity mitigation, **not** a substitute for CFG. Enabling it for arbitrary guest PE code also requires separate signal, exception, fiber and unwind compatibility work (see R215); no such compatibility is established here.

## PROPOSED conformance and adversarial gates — NOT RUN

- Windows baseline: compile /guard:cf x64 fixtures and record Windows build, linker flags and process mitigation policy. Exercise executable VirtualAlloc with default target validity, PAGE_TARGETS_INVALID, VirtualProtect with and without PAGE_TARGETS_NO_UPDATE, then SetProcessValidCallTargets on aligned/unaligned, ordered/unordered and nonexecutable regions. Capture return values, GetLastError and fault behavior without guessing expected undocumented error codes.
- Export suppression: on a Windows build with the mitigation enabled, compare direct calls, indirect calls before/after GetProcAddress, repeated resolutions, module unload/reload and cross-thread races. Separately exercise strict-mode loading of CFG-aware/unaware DLLs. Log mitigation state, not just executable success.
- Cross-process rights: compare authorized and unauthorized target-process handles; confirm guest access checks cannot be bypassed by direct host pointer access or shared mappings.
- Linux prototype, if later implemented: exercise concurrent JIT writes and executable publication, address reuse, stale generations, unload during indirect call, and target invalidation under active threads. Verify fail-closed behavior where policy requires it; independently test host signal delivery and fault translation.
- Capability matrix: independently record CPU CET, Linux kernel userspace shadow-stack configuration/runtime status, presence or absence of userspace IBT, PE CFG metadata, guest CFG helper installation, and actual invalid-target rejection. **Never** collapse these into one “CFG supported” checkbox.
- Security review: evaluate whether uninstrumented guest code, direct host calls, malicious load-config metadata or a permissive fallback helper bypasses policy. If comprehensive enforcement cannot be demonstrated, report the precise coverage limit.

## OPEN — unresolved architecture decisions

- Exact Windows behavior for default-valid executable pages across image mapping, copy-on-write, executable permission transitions and target revocation; needs build-specific differential tests.
- How to enforce policy for guest modules that lack compiler-inserted CFG checks, and whether coverage claims should be scoped to instrumented call sites.
- Whether the NT runtime can safely enable Linux shadow stack for arbitrary PE guest threads given Windows SEH, context restore, fibers and mixed host/guest call paths.
- Cost of target-registry lookups and mutation synchronization versus Windows reference on equivalent hardware; no performance claim until measured.

## Primary sources (consulted 2026-11-21)

- Microsoft, *Control Flow Guard for platform security*: https://learn.microsoft.com/en-us/windows/win32/secbp/control-flow-guard
- Microsoft, *SetProcessValidCallTargets*: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-setprocessvalidcalltargets
- Microsoft, *PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY*: https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_control_flow_guard_policy
- Linux kernel documentation, *Control-flow Enforcement Technology (CET) Shadow Stack*: https://docs.kernel.org/next/x86/shstk.html

Cross-references: R180_CFG_CALL_TARGET_POLICY_GATES.md; R215_FIBER_CONTEXT_SWITCH_LINUX_GATES.md; R216_PE64_CFG_METADATA_PARSER_ENFORCEMENT_GATES.md.
