# R31 — PE load configuration, CFG metadata, and process mitigation gates

Date: 2026-11-12. Status: **documentation research only**. No Windows-reference conformance run, PE fixture, Linux CFG enforcement, or performance measurement was performed for R31.

## Scope and evidence

**VERIFIED (Microsoft documentation):** PE32+ has a load-configuration directory with version/size-dependent fields. The load configuration can carry a security cookie pointer, Guard CF function pointers, a Guard CF function table/count, and GuardFlags. A parser must not assume that every field in the current specification exists in every image. The PE specification defines `IMAGE_GUARD_CF_INSTRUMENTED` and `IMAGE_GUARD_CF_FUNCTION_TABLE_PRESENT`, among other flags. Primary source: [Microsoft PE format, load configuration directory](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

**VERIFIED (Microsoft documentation):** The GFIDS table records indirect-call targets; Microsoft specifies sorted RVAs and describes optional per-entry metadata encoded using the high GuardFlags bits. The metadata documentation states that a malformed ordering can prevent image loading. Primary source: [Microsoft PE metadata](https://learn.microsoft.com/en-us/windows/win32/secbp/pe-metadata).

**VERIFIED (Microsoft documentation):** Windows exposes a per-process CFG policy with EnableControlFlowGuard, EnableExportSuppression, StrictMode, and XFG-related bits. StrictMode requires subsequently loaded DLLs to support CFG and cannot be disabled after enabling. Export suppression changes which exported functions count as valid indirect-call targets until dynamically resolved. Source: [PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_control_flow_guard_policy).

**VERIFIED (Microsoft documentation):** Process mitigation is broader than CFG. Windows documents DEP, ASLR, dynamic-code restrictions, signature/image-load restrictions, and user shadow-stack policy, among others. Some policies are established at process creation; applying ASLR policy after loading images cannot retroactively provide the intended protection. Sources: [mitigation policy enumeration](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ne-winnt-process_mitigation_policy), [SetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy), [UpdateProcThreadAttribute](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute).

**VERIFIED (Microsoft documentation):** Dynamic-code policy includes ProhibitDynamicCode and thread opt-out/remote downgrade controls. This is not equivalent to a single Linux `mprotect` restriction. Source: [PROCESS_MITIGATION_DYNAMIC_CODE_POLICY](https://learn.microsoft.com/en-us/windows/desktop/api/winnt/ns-winnt-process_mitigation_dynamic_code_policy).

## Consequences for LinuxNTAlpha

**HYPOTHESIS:** A host that merely maps executable PE sections and binds IAT entries, as in R19/R20, may execute uninstrumented test images while failing to preserve the security properties and observable behavior of CFG-enabled applications. A valid CFG implementation needs a coherent *process-wide* view of loaded-image target metadata, indirect-call checking support, dynamically allocated executable targets, export resolution, and DLL load policy. Whether existing compiler-generated guard stubs can be serviced by independently implemented user-space dispatch needs experimentation.

**HYPOTHESIS:** Initial CFG support may be implementable in user space without Linux kernel changes, but robust enforcement across untrusted code, code generation, and concurrent image unloading must be threat-modeled separately. A userspace bitmap alone should not be advertised as a security boundary.

**OPEN:** Exact Windows-version-specific behavior for XFG, guard dispatch/check pointers, EH continuation metadata, and interactions with CET/shadow stacks. The documents above establish fields and policies, not an implementation-level compatibility guarantee.

## Proposed ordered gates (not executed)

1. **Parse-only:** Add strict bounded load-config parsing to the existing PE parser. Check directory RVA/size, structure `Size`, pointer/count arithmetic overflow, image-range validation, and GuardFlags-dependent table stride. Reject or explicitly classify unsupported configurations; do not silently treat protected images as unprotected.
2. **Fixture inspection:** Build PE64 fixtures with and without linker CFG instrumentation, inspect the emitted load configuration and guard references, and retain exact compiler/linker versions. Do not assume Clang/LLD emits Windows-equivalent CFG metadata without verifying the binaries.
3. **Windows reference:** On a licensed Windows test host, compare load acceptance, sorted/unsorted GFIDS, export suppression, dynamic target registration, and strict DLL loading. Record Windows build, mitigation configuration, return codes, and exceptions.
4. **Linux functional prototype:** Only after reference observations, test whether imported guard check/dispatch functions and target registration can reproduce the observed behavior. Verify concurrent DLL load/unload and indirect calls.
5. **Security review:** Distinguish application compatibility from mitigation strength; test code-page permissions, dynamic-code policy, failure-closed behavior, and adversarial mutation separately.

## Decision impact

Do **not** select a kernel fork or claim secure CFG compatibility on the basis of this review. Add load configuration to the loader acceptance checklist alongside R27 TLS, R29 API-set/delay import, and R30 unwind metadata. This is a new, separate gate: correctly parsing PE sections and resolving imports is insufficient for modern Windows loader and mitigation semantics.

Sources reviewed 2026-11-12; all are Microsoft primary documentation. No implementation code copied.
