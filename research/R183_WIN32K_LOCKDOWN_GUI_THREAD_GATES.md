# R183 — Win32k system-call lockdown and GUI-thread transition gates

Status: **VERIFIED FROM PRIMARY MICROSOFT DOCUMENTATION** for the documented mitigation contract; **HYPOTHESIS** for LinuxNTAlpha implementation. Reviewed 2026-11-19. **No experiments performed.**

## Architectural question and nonduplication

How can a Linux-hosted NT personality preserve a Windows process mitigation that blocks access to the Win32k GUI kernel service surface, without mistaking it for a general ban on Linux syscalls or a prohibition on every user-mode USER32 operation? This is separate from R174 (dynamic executable-code policy), R179 (child creation), R180 (CFG), and R182 (image provenance). The core unresolved issue is **guest service-domain authorization at the GUI-thread transition**, rather than PE mapping or image admission.

## Verified primary-source contracts

1. Microsoft's [Exploit protection reference](https://learn.microsoft.com/en-us/defender-endpoint/exploit-protection-reference) describes the **Disable Win32k system calls** mitigation: it prevents a thread from converting from non-GUI to GUI-thread status, which otherwise occurs on its first Win32k call or via IsGuiThread. The documentation explicitly warns that a single-process GUI application is affected; this mitigation is intended for dedicated non-UI processes.
2. [GetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocessmitigationpolicy) exposes ProcessSystemCallDisablePolicy and returns PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY. The documented description is disabling NTUser/GDI functions at the lowest layer. The API requires PROCESS_QUERY_INFORMATION on the queried process handle.
3. [SetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy) exposes ProcessSystemCallDisablePolicy for the calling process. Microsoft recommends applying mitigations before or during process initialization; the general API documentation alone does not establish all legal transitions for this specific policy.
4. The [Exploit protection reference](https://learn.microsoft.com/en-us/defender-endpoint/exploit-protection-reference) documents an audit-only configuration for this mitigation. The exact emitted events, supported builds and differences between audit and enforcement must be verified on Windows; documentation alone does not prove a Linux implementation's event equivalence.

## Proposed LinuxNTAlpha boundary — hypothesis, not implementation

Represent the guest process's Win32k lockdown policy in a trusted process-policy object. Route all guest NTUser/GDI kernel-service entry points through a common authorization decision **before** creating a guest GUI-thread identity or entering the guest window-manager/GDI service. A Linux Wayland/X11 compositor connection or an ordinary Linux syscall is not itself a Windows Win32k system call; blocking all such host operations would be an incorrect translation of the documented guest contract. Conversely, merely hiding a USER32 export is not sufficient if another guest entry path can reach the same protected service.

Guest GUI-thread state and host OS thread state must remain separate. A process-wide mitigation must apply to all guest threads, including threads created later, and guest code must not bypass the gate through alternate DLL exports, direct syscall interception, callbacks, RPC to an untrusted helper, or an unchecked broker endpoint. A trusted broker may implement GUI services for other processes, but must authenticate caller process identity and policy at every request. This is a **security design hypothesis**, not a statement about Microsoft's exact internal implementation.

Avoid prematurely treating all USER32 calls as forbidden: the Microsoft source defines the restriction in terms of NTUser/GDI/Win32k and GUI-thread conversion. Which higher-level calls are entirely user-mode, which invoke the protected service, and which fail with which errors are **OPEN** and require build-specific reference tests.

## Differential and security gates — proposed, not executed

1. On a pinned Windows version, capture initial policy using GetProcessMitigationPolicy. Apply lockdown at process creation or the earliest documented supported point. Record the API result, error, and thread GUI status before and after IsGuiThread and selected benign USER32/GDI calls. Include a control process without the mitigation.
2. Test a child thread created after lockdown and a thread already present before any permitted policy transition. Record whether failed GUI conversion is sticky, the exact failure status and whether the process survives. Do not infer behavior from documentation where it is silent.
3. Independently test audit mode on a build supporting it; record actual event identifiers and enforcement outcomes rather than assuming audit parity.
4. In a future Linux prototype, attempt each registered guest NTUser/GDI entry, direct guest syscall path, callback and broker route. Verify that no denied request mutates guest GUI-thread state or compositor state; separately confirm allowed non-GUI NT services still work.
5. Stress simultaneous first GUI requests on multiple guest threads and policy installation races. Verify the policy check and GUI-state transition are atomic with respect to guest policy updates; reject forged caller identities at broker boundaries.
6. Record Windows build, architecture, mitigation configuration, test binaries, Linux kernel/compositor versions and traces. Report Windows observations, Linux observations and design predictions separately.

## Unresolved decisions and limitations

**OPEN:** precise NTSTATUS/Win32 errors; IsGuiThread behavior with its conversion parameter on each Windows build; the relationship between GUI-thread conversion and already initialized GUI state; the complete protected service inventory; audit-event fidelity; mitigation inheritance and legal late updates; behavior with WOW64 and cross-process GUI brokers. No runtime, conformance result, latency improvement or security equivalence is claimed. No code was copied.

## Sources (retrieved 2026-11-19)

- Microsoft, [Exploit protection reference](https://learn.microsoft.com/en-us/defender-endpoint/exploit-protection-reference), sections *Disable Win32k system calls* and configuration options.
- Microsoft, [GetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocessmitigationpolicy).
- Microsoft, [SetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy).
