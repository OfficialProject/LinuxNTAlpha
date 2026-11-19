# R174 — Windows dynamic-code mitigation versus Linux memory-protection gates

Date: 2026-11-18. Status: **documentation research only**. No experiment was run; no Windows or Linux conformance result is claimed.

## Question

Can a native PE runtime enforce Windows process dynamic-code restrictions while using Linux memory mapping, JIT and page-protection primitives, without incorrectly equating Linux W^X hardening with Windows mitigation policy?

## Primary contracts (documentation-verified)

1. Microsoft documents `PROCESS_MITIGATION_DYNAMIC_CODE_POLICY` with `ProhibitDynamicCode`, `AllowThreadOptOut`, `AllowRemoteDowngrade`, and `AuditProhibitDynamicCode` fields. These are **distinct policy dimensions**; they cannot be represented faithfully by a single boolean 'no executable writable pages'. Source: [PROCESS_MITIGATION_DYNAMIC_CODE_POLICY](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_dynamic_code_policy).
2. Microsoft exposes process mitigation policy query and setting through `GetProcessMitigationPolicy` and `SetProcessMitigationPolicy`; process creation also has mitigation-policy attributes. The runtime must model inherited/creation-time state separately from subsequent requests. Sources: [GetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocessmitigationpolicy), [SetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy), [UpdateProcThreadAttribute](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute).
3. Linux `mmap(2)` and `mprotect(2)` provide mapping and protection operations, including `PROT_EXEC`; their API contracts do **not** by themselves define the Windows mitigation state, audit behavior or thread opt-out policy. Sources: [mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html), [mprotect(2)](https://man7.org/linux/man-pages/man2/mprotect.2.html).
4. Linux seccomp filtering operates on system-call requests and cannot be treated as an authoritative replacement for guest API-level mitigation semantics. Source: [seccomp(2)](https://man7.org/linux/man-pages/man2/seccomp.2.html).

These are source-derived API distinctions, **not** proof that any particular Windows build returns a specific error for every protection transition.

## Architectural consequences (hypotheses requiring tests)

- **H1 — guest policy ledger:** Maintain a per-guest-process mitigation-policy object and explicit per-guest-thread opt-out eligibility/state. Query APIs read this guest state rather than infer it from Linux VMAs.
- **H2 — mediation:** All guest-visible virtual-memory operations (including section mapping, image loading and protection changes) must consult the policy ledger. An isolated host `mprotect` check is insufficient if another runtime path can install executable pages.
- **H3 — provenance:** Distinguish loader-provided executable image mappings from guest attempts to generate or modify executable code; exact Windows acceptance rules are **OPEN** until Windows reference tests.
- **H4 — trust boundary:** A guest that can issue unmediated Linux syscalls may bypass a user-space-only guest policy. Whether syscall dispatch, seccomp, kernel facilities or another boundary is appropriate remains **OPEN**, and no security guarantee is claimed.

## Proposed differential gates (NOT EXECUTED)

1. On a pinned Windows version/build, query baseline policy; set each supported field separately and record return values and errors, including incompatible combinations.
2. Compare `VirtualAlloc` / `VirtualProtect` transitions between RW, RX, RWX and no-access, including executable image sections and attempts to modify executable mappings.
3. Compare opt-out behavior for two threads in one process, with explicit synchronization, and test whether policy changes affect already-established mappings.
4. Test creation-time mitigation attributes independently of runtime policy setting, recording unsupported or immutable-policy failures.
5. In Linux fixture processes, test `mmap`, `mprotect` and section-backed mappings under a prototype policy ledger; instrument **all** entry points and explicitly attempt bypass via raw syscalls.
6. Capture Windows error codes, effective page protections, kernel version, architecture, toolchain, mitigation flags and exact test binaries. Do not infer Windows semantics from Linux success/failure alone.

## Decision gate

Do **not** select a Linux enforcement mechanism or claim Windows ACG compatibility until Windows-reference behavior, complete guest VM operation coverage, syscall-bypass threat model and negative security tests have been recorded. A strict W^X default may be a useful independent hardening choice, but it is **not** evidence of Windows dynamic-code-policy compatibility.

## Related research

See [R144](R144_PE_CFG_LOADER_TARGET_GATES.md) for CFG target validity and loader patching, [R171](R171_PLACEHOLDER_VIRTUAL_ADDRESS_REPLACEMENT_GATES.md) for reservation/replace semantics, and [R164](R164_SUD_SIGSYS_ALTSTACK_CONTINUATION_GATES.md) for syscall-dispatch hazards. CFG, placeholder management and dynamic-code mitigation are separate contracts.
