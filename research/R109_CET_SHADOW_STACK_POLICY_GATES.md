# R109 — Windows CET shadow-stack policy versus Linux x86 user shadow stack

Status: **primary-source documentation review; no experiment executed** (2026-11-15). Scope: x86-64 native PE execution on a Linux host, not kernel-mode CET. This note is not a claim that Windows CET policy is implemented.

## Why this is a distinct architectural gate

R9/R30 cover x64 unwind metadata; R31/R69 cover general mitigation and executable-memory policy; R55 covers cross-ABI register preservation; R90/R108 cover syscall/signal provenance. None establishes that Windows **process-wide CET compatibility and image admission policy** can be implemented by toggling Linux **per-thread** shadow-stack controls. A native Windows-code loader that runs in a Linux ELF process has two policy authorities: Windows-visible process/image rules and the Linux thread's actual hardware enforcement.

## Verified primary-source contracts

1. Microsoft's `PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY` exposes `EnableUserShadowStack`, `EnableUserShadowStackStrictMode`, `BlockNonCetBinaries`, `BlockNonCetBinariesNonEhcont`, `SetContextIpValidation`, and relaxed-mode policy. Microsoft's documentation describes compatibility mode as making violations fatal for CETCOMPAT modules, whereas strict mode makes all shadow-stack violations fatal. It also describes restrictions on changing these flags at runtime. **Source:** [Microsoft policy structure](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_user_shadow_stack_policy).
2. Windows process-creation mitigation attributes can select CET shadow-stack modes, context-IP validation and non-CET image blocking, with version qualifications. **Source:** [Microsoft UpdateProcThreadAttribute](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute); [Microsoft SetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy).
3. The Linux x86 kernel documentation states that currently its 64-bit kernel supports **userspace shadow stack and kernel IBT**, not userspace IBT through the same mechanism. A loader/runtime, not the kernel, interprets ELF CET notes; `arch_prctl(ARCH_SHSTK_ENABLE/DISABLE/LOCK/STATUS)` controls the **calling thread**, with state inherited on clone. Support requires CPU, kernel configuration, and userspace integration. **Source:** [Linux x86 shadow stack](https://docs.kernel.org/arch/x86/shstk.html).
4. Linux documents that a signal saves shadow-stack-specific state and `sigreturn` verifies/restores an SSP token; it also documents 32-bit-mode incompatibility when shadow stack is enabled. **Source:** [Linux x86 shadow stack — signal and 32-bit sections](https://docs.kernel.org/arch/x86/shstk.html).

## Deductions (not demonstrated)

- **Policy mismatch:** Linux `ARCH_SHSTK_ENABLE` is not an implementation of Windows CETCOMPAT module classification, compatibility-mode violation disposition, Windows image-load blocking, or Windows context-IP validation. A Linux-hosted loader needs a guest process policy model, per-module classification and a separate host-enforcement capability decision. This is an architectural inference from the above contracts.
- **Thread transition hazard:** Because Linux enforcement is per-thread and can be locked, choosing the host state too early can make later guest image admission, host callbacks, and compatibility transitions impossible. This is a design risk, not a demonstrated failure.
- **Exception bridge hazard:** A control-protection fault and its Linux signal context must not automatically be treated as an ordinary Windows access violation. The Windows exception mapping, handler ordering, continuation validation and Linux signal-restorer path require reference behavior and a tested policy. No specific NTSTATUS mapping is asserted here.
- **Cross-ABI boundary:** Host-to-guest calls, callbacks, unwind paths, fibers and context changes can interact with shadow-stack return-address integrity. Ordinary MS/SysV ABI bridge success (R17) does not prove CET-safe continuation.

## Proposed conformance and implementation gates — not run

1. **Capability inventory:** On each Linux test host record CPU features, kernel version/config, `/proc/cpuinfo` `user_shstk`, `/proc/self/status` thread-feature fields, `ARCH_SHSTK_STATUS`, glibc/loader behavior, and exact compiler/linker flags. Report SKIP rather than PASS where unsupported.
2. **Windows reference matrix:** On documented Windows 10/11 builds and CET-capable hardware, record policy query results and process creation with compatibility/strict mode; attempt CETCOMPAT and non-CET DLL loads; capture supported error/exception outcomes and any version-specific differences. Never infer those outcomes from Linux.
3. **Host-only isolated probes:** In disposable child processes, exercise `ARCH_SHSTK_ENABLE`, thread inheritance, lock semantics, signal delivery/return, and normal cross-ABI calls. Do not deliberately corrupt a production process's shadow stack. Record whether the ELF loader already enabled or locked the feature.
4. **PE loader parse-only gates:** Identify and bounds-check relevant load-configuration CET/EH continuation metadata before interpreting it. Reject malformed ranges, integer overflow, and pointers outside the image. Distinguish image metadata from process policy and host enforcement. Do not claim that presence of metadata alone proves a module is safe.
5. **Guest/host integration gates:** Exercise guest-to-host-to-guest callbacks, exception/unwind continuation, thread startup/exit, and host signal restoration with both supported and unsupported host CET states. Require Windows-reference parity before exposing policy as implemented.
6. **Security invariant:** Never silently report Windows hardware enforcement as active when Linux lacks the requisite host enforcement or when a required Windows-specific admission/continuation rule is unimplemented. Define a clear unsupported-policy result and versioned compatibility policy before enabling untrusted PE execution.

## Open questions

- What exact Windows exception status and debugger first/second-chance sequence is observed for each CET violation under compatibility versus strict mode?
- How does Windows classify mixed CETCOMPAT and non-CET modules and dynamically generated code on each target build?
- Which guest fiber/context and nonlocal-unwind operations require explicit shadow-stack repair, and which are handled by supported compiler/runtime sequences?
- Can a Linux ELF host process reliably preserve its own CET security while loading guest modules with different Windows-visible policies? This is **not established** by the Linux API documentation.

## Evidence boundaries

Sources above are Microsoft Learn and Linux kernel documentation, not test logs. No Windows reference machine, CET-capable Linux host, PE loader test, benchmark, or exploit experiment was run for R109. The Linux documentation describes an upstream interface; deployment support must be checked against the actual host kernel and runtime.
