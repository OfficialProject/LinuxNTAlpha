# R270 — Child-process mitigation and Linux process-creation authority gates

Date: 2026-11-23. Status: **primary-documentation research; no experiment, implementation, or Windows differential run**.

## Architectural question

Can an NT-compatible guest enforce Windows child-process creation mitigation when guest processes are implemented as Linux processes, and when process creation can be requested indirectly through another guest process or a privileged supervisor?

This is distinct from R12 (process creation lifecycle), R14/R223 (tokens and isolation), R232/R233 (supervisor/process topology), and R254 (invalid-handle mitigation). The new question is **which principal authorizes a requested child creation and where the restriction must be checked**.

## Verified public contracts

1. Microsoft's `UpdateProcThreadAttribute` documents `PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY` as a DWORD-valued creation attribute, supported on Windows 10 / Server 2016 and later. `PROCESS_CREATION_CHILD_PROCESS_RESTRICTED` (0x01) prevents the created process from creating children; `PROCESS_CREATION_CHILD_PROCESS_OVERRIDE` (0x02) can override a restriction only when the process creating the new process is not itself restricted. Microsoft explicitly warns that the restriction is effective only in a sandbox where privileged process handles are inaccessible; handles granting `PROCESS_CREATE_PROCESS` or `PROCESS_VM_WRITE` may permit bypass. **Source:** https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute

2. Microsoft's WDK `PROCESS_MITIGATION_CHILD_PROCESS_POLICY` defines `NoChildProcessCreation`, `AuditNoChildProcessCreation`, and `AllowSecureProcessCreation`. The documentation says deny takes precedence when deny and audit are both set, and describes a secure-process exception / creation on behalf of another process. The WDK page lists Windows 10 1709 as its minimum client version. **Source:** https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-_process_mitigation_child_process_policy

3. `SetProcessMitigationPolicy` provides a process mitigation API; the documented API is not proof that every policy can be relaxed after initialization. Treat specific child-policy mutability and return codes as **unverified** until Windows reference tests. **Source:** https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy

4. Linux seccomp user notification can stop a filtered syscall and let a listener respond instead of executing it, but it is a Linux syscall interception mechanism, **not** a Windows token or process-handle authorization system. The listener/target mechanics are documented by the Linux man-pages example; this does not establish a secure or sufficient NT implementation. **Source:** https://www.man7.org/tlpi/code/online/dist/seccomp/seccomp_user_notification.c.html

## Derived architecture constraints (reasoned proposals, not verified implementation facts)

- A guest process-creation request should carry an **initiating guest security principal**, a **requested parent-process handle with granted rights**, and the **target's proposed child-policy attributes**. Do not decide permission solely from the Linux parent PID, `fork` caller, or broker credentials.
- A privileged Linux supervisor spawning all workers can accidentally bypass a guest's restricted-child policy unless the broker checks the initiating guest's policy *before* host process creation and before publishing guest process handles.
- A guest process created with a restrictive policy must not gain an unrestricted alternate creation path via native `clone`/`fork`/`exec`, a second broker endpoint, or privileged guest process handles. This is an **architecture/security gate**, not a claim that existing prototypes expose such paths.
- Deny and audit are separate outcomes. An audit-only policy should not be silently treated as deny, and audit records should not be confused with Linux kernel audit logs.
- The documented bypass caveat means Windows-compatible policy emulation alone cannot promise containment. Host-level isolation and handle-access mediation must be independently evaluated.

## Unrun falsification and conformance matrix

| Gate | Windows reference observation needed | Linux guest/backend acceptance condition |
| --- | --- | --- |
| C1 creation restriction | Parent with restricted child policy attempts ordinary `CreateProcessW`; record success, Win32 error and child visibility | Denied guest request creates no host worker, no guest process object, and no inheritable handle |
| C2 override | Compare restricted versus unrestricted creator supplying `PROCESS_CREATION_CHILD_PROCESS_OVERRIDE` | Override decision uses initiating guest identity; privileged broker identity cannot silently override |
| C3 inherited / target policy | Create a child with restricted attribute; child attempts to create grandchild | Target's effective policy is recorded and applied to subsequent creation |
| C4 handle-based alternate parent | Exercise `PROC_THREAD_ATTRIBUTE_PARENT_PROCESS` with and without `PROCESS_CREATE_PROCESS`; compare restriction and failure | Guest handle rights are validated before any host spawn; no PID-only shortcut |
| C5 audit versus deny | Exercise audit-only, deny-only and both bits on supported Windows builds; collect observable results and audit channels | Audit/deny states remain distinct and denial precedence matches observed version |
| C6 secure-process exception | Probe only in an authorized Windows environment supporting secure-process creation; record unsupported cases | Do not claim support or silently allow an exception absent verified secure-process semantics |
| C7 native escape | From a guest process attempt all supported direct Linux process-creation and broker routes | No guest-controlled path creates an untracked host process or evades guest creation policy |
| C8 crash/restart race | Not a direct Windows equivalence claim; force supervisor failure during pending creation | No orphan host child is published as an authorized guest child; idempotent cleanup/rollback |

Record Windows build, process token/AppContainer context, bitfields, API return, `GetLastError`, observed child identity and audit availability. For Linux, record kernel, namespace/seccomp configuration, guest/host PID mapping and every spawn endpoint. Do not infer Windows return codes from documentation that does not specify them.

## Decision impact

**Hypothesis H270:** a single authoritative guest process-creation transaction, coupled with a separate host process-creation confinement layer, can preserve the documented child-policy decisions more faithfully than direct per-guest `fork` without mediation. This is a candidate design, not a selected architecture.

**Unresolved:** precise Windows build-specific behavior of policy updates, secure-process exceptions, handle-based bypasses, audit visibility, and whether an adequately confined Linux host design can preserve guest process identity across broker failure. These require Windows differential tests and Linux adversarial prototypes. No benchmark or compatibility claim follows from this note.
