# R69 — Dynamic-code mitigation and executable-memory authority

Status: **documentation-only architectural research**, 2026-11-14 UTC. No Windows reference experiment, Linux kernel test, or LinuxNTAlpha implementation was executed for this note.

## Question

Can an NT-compatible process enforce Windows dynamic-code restrictions while hosting Windows machine code in an ordinary Linux process, including when code invokes Linux syscalls directly rather than mediated NT APIs?

## Verified primary-source contracts

1. **Windows process policy is not merely W^X.** Microsoft's `PROCESS_MITIGATION_DYNAMIC_CODE_POLICY` describes `ProhibitDynamicCode` as preventing generation of dynamic code **or modification of existing executable code**. It separately exposes `AllowThreadOptOut`, `AllowRemoteDowngrade`, and `AuditProhibitDynamicCode`. Microsoft warns that the thread opt-out setting is not suitable for strong security. [Microsoft structure](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_dynamic_code_policy).
2. **Policy is observable and configurable through Windows APIs.** `GetProcessMitigationPolicy` exposes `ProcessDynamicCodePolicy`; `SetProcessMitigationPolicy` supports process policy configuration. Microsoft advises applying mitigations before or during process initialization. [Get](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocessmitigationpolicy), [Set](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy).
3. **Creation-time configuration is distinct from runtime calls.** `UpdateProcThreadAttribute` documents `PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_*` values, including an always-on variant permitting opt-out. [Microsoft creation attributes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute).
4. **Linux memory protections alone are not equivalent to a Windows mitigation policy.** The Linux `mprotect(2)` manual explicitly permits changing existing code mappings to writable and documents `READ_IMPLIES_EXEC` behavior; `mmap(2)` documents architecture-dependent read/execute implications. These are Linux mapping primitives, not process-wide NT dynamic-code policy enforcement. [mprotect(2)](https://man7.org/linux/man-pages/man2/mprotect.2.html), [mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html).

## Architectural consequence (reasoned, not measured)

**HYPOTHESIS:** Implementing `NtProtectVirtualMemory` / `VirtualProtect` restrictions solely inside LinuxNTAlpha's user-space API dispatcher is insufficient as a *security boundary* if untrusted Windows-origin code can directly issue Linux `mmap`, `mprotect`, `pkey_mprotect`, or equivalent operations. Such calls could bypass a policy enforced only at the NT API boundary. R16/R26 establish narrow syscall-interception feasibility, **not** comprehensive syscall mediation or sandboxing.

**HYPOTHESIS:** A defensible implementation needs a clearly specified trust boundary for executable-memory creation and mutation, potentially a constrained Linux execution context and/or kernel-mediated policy. A userspace shadow map can help reproduce NT-visible errors and state, but by itself cannot guarantee enforcement against direct Linux syscalls, alternate mappings, or cooperating processes. The correct mechanism remains **OPEN** pending threat-model and kernel-interface audits.

**Important distinction:** A compatibility-only implementation may emulate API failures without claiming adversarial enforcement; a security-oriented implementation must resist bypass from code executing in the same address space. Do not conflate these milestones.

## Proposed conformance and adversarial gates (NOT RUN)

| Gate | Windows reference | LinuxNTAlpha candidate |
|---|---|---|
| Baseline | Allocate writable memory, change to executable, attempt modification of executable pages with mitigation disabled | Record NT-visible protection transitions and returned status |
| Prohibit | Enable dynamic-code prohibition before test operations; exercise new executable allocation and modification of executable memory | Compare failure modes and resulting page access, not just return codes |
| Opt-out | Create process with documented allow-opt-out configuration; opt one thread out using supported thread policy; test another thread separately | Check thread-specific effects and synchronization, with Windows build recorded |
| Inheritance | Create child with and without creation mitigation attribute | Confirm process-specific policy initialization and queries |
| Native bypass | **Not a Windows-reference equivalent:** on Linux, attempt direct `mmap/mprotect/pkey_mprotect` from code outside NT API stubs | Determine whether security policy survives unmediated Linux entry paths |
| Alias/bypass | Create shared/aliased mappings and try to modify bytes underlying executable pages | Audit actual executable-code integrity rather than VMA labels alone |

Record Windows edition/build, CPU architecture, mitigation flags, error/status, mapping state, and all toolchain/kernel versions. Isolate intentionally faulting tests in subprocesses. No expected numeric results or NTSTATUS codes are asserted without reference execution.

## Decision gates

- **OPEN:** Is the initial milestone API-level compatibility or hostile-code containment? Specify separately in architecture decision records.
- **OPEN:** Which Linux kernel facilities can enforce restrictions across *all* executable-memory paths without preventing legitimate PE image loading, JIT use when policy is disabled, or NT process creation?
- **OPEN:** How will policy changes, thread opt-out and cross-process modifications be represented consistently with NT process/thread identity?
- **OPEN:** What is the interaction with CFG, image signing and executable section mappings? R31 and R59 cover adjacent but different requirements.

**Recommendation (provisional):** Keep dynamic-code policy enforcement as an explicit architectural gate. Do not label a user-space `VirtualProtect` check as an effective exploit mitigation until direct-syscall and alias-bypass tests pass. No kernel design is selected by this note.
