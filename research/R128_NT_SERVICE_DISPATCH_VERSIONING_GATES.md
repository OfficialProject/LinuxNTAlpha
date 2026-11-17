# R128 — NT service dispatch: identity, versioning and guest ABI gates

Status: **source-linked architectural analysis only** (2026-11-17 UTC). No new source checkout, Windows reference execution, Linux experiment, benchmark, or compatibility result is claimed.

## Question

After R26's one intercepted synthetic PE syscall, how should LinuxNTAlpha identify an NT service without assuming that a numeric syscall identifier is a portable public API? This note is about **service identity and ABI contracts**, not R90's guest/host syscall provenance or R52's error-code translation.

## Primary-source contracts and evidence limits

1. **VERIFIED from Microsoft published API contracts:** Microsoft documents callable Native API routines such as `NtQueryInformationProcess`, including their arguments, information classes and `NTSTATUS` return convention. The documentation also warns that this function and the structures it returns are internal to the operating system and subject to change; applications should use public alternatives where available. [Microsoft: NtQueryInformationProcess](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess).
2. **VERIFIED from Microsoft published driver documentation:** The `Nt` and `Zw` names can have different semantics when invoked by kernel-mode callers; the kernel-mode distinction is not a license to assume that user-mode raw syscall numbers, entry stubs or kernel caller-mode semantics are stable. [Microsoft: Using Nt and Zw Versions of the Native System Services Routines](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-nt-and-zw-versions-of-the-native-system-services-routines).
3. **VERIFIED from Microsoft published API contracts:** `RtlGetVersion` exposes Windows version information through a documented NTDLL routine, but its existence does not establish a documented mapping from OS version to syscall IDs. [Microsoft: RtlGetVersion](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlgetversion).
4. **VERIFIED project scope, not Windows compatibility:** [R26](R26_PE64_RAW_SYSCALL_SUD.md) intercepts one deliberately synthetic Windows-origin raw syscall. [R28](R28_NTDLL_RTL_COMPARE_MEMORY.md) implements one NTDLL export without a syscall. [R90](R90_GUEST_HOST_SYSCALL_PROVENANCE.md) separately identifies the security boundary. None establishes a Windows-version-specific service table.

These URLs identify upstream primary documentation, **not independently fetched or revision-pinned pages in this run**. Claims about exact undocumented Windows syscall-number tables, Windows builds or NTDLL binary layouts remain **OPEN**.

## Architectural inference (HYPOTHESIS)

A compatibility runtime should maintain a **named, typed service registry** independent of the mechanism used to reach it. For an ordinary imported `ntdll.dll` entry point, the guest-facing implementation can call that registry directly using a Windows x64 ABI adapter. A raw guest `syscall` requires an **explicitly selected and tested guest build profile** before a numeric ID can be translated to a named service. Unsupported profiles or IDs must fail closed rather than pass the number to Linux.

The public name and signature of an NTDLL function are not proof of the raw service number used by a particular Windows build. Likewise, `Nt`/`Zw` naming is not a blanket equivalence rule for all calling contexts.

## Proposed separation of responsibilities (NOT IMPLEMENTED)

- **Guest API surface:** explicit export name, signature, calling convention, input/output validation, and guest-visible `NTSTATUS` semantics.
- **Service registry:** stable project-internal symbolic identity and versioned information-class handling, without exposing Linux errno or fd values.
- **Dispatch adapters:** imported NTDLL stub calls and raw-syscall traps independently map to the same service registry, with provenance checks per R90.
- **Build profile:** an opt-in, evidence-backed mapping for numeric raw IDs, scoped to a specific guest architecture and Windows reference build. Never silently infer the profile from a single syscall.
- **Host backend:** Linux syscalls and IPC are implementation details, subject to rights checking and guest-object lifetime rules.

## Falsifiable test gates (NOT RUN)

1. **Synthetic registry:** build a freestanding PE fixture that invokes one named NTDLL import and one *synthetic* raw service ID, both mapped explicitly to a test-only registry entry. Verify equal guest-visible output, register preservation, and no unintended Linux syscall. This proves dispatch plumbing only.
2. **Unknown IDs:** invoke unknown, colliding, negative and boundary raw IDs. Require deterministic guest rejection and no fallthrough to host Linux services.
3. **Version mismatch:** configure two synthetic guest build profiles that assign different IDs to the same named service. Require correct selection under an explicit profile and rejection when no profile is selected.
4. **Signature mismatch:** exercise invalid pointers, invalid information-class lengths and unsupported information classes for an independently implemented named service. Assert no host pointer dereference before validation and compare outcomes against a version-pinned Windows reference.
5. **Windows reference gate:** capture the exact Windows architecture/build, signed module identity and reference outputs for each documented API contract. Any numeric raw-service mapping must be independently measured on that build; never promote an internet syscall table into a verified fact.
6. **Concurrency and nesting:** combine the R90 mixed guest/host transition fixture with parallel guest threads, callbacks and failure injection. Check the registry does not confuse a guest raw ID with a Linux syscall number.

## Decision consequences

**OPEN:** Whether the initial supported guest ABI should be limited to imported, independently implemented NTDLL exports, with raw numeric syscall compatibility deferred until build-specific evidence exists. This is a practical way to make progress without promising arbitrary Windows binary compatibility.

**Recommendation (not a selected ADR):** implement named NTDLL entry points first and keep raw numeric dispatch explicitly experimental. This reduces dependence on undocumented, build-specific service numbers while preserving the project's long-term direct-PE execution goal.

**Nonduplication:** R26 proves a synthetic trap, R90 discusses provenance, R52 discusses error conversion, and R115 specifies a composed runtime milestone. R128 isolates the **versioned service identity and ABI mapping** that those notes do not settle.
