# R179 — Child-process restriction, token authority, and Linux broker bypass gates

Research date: 2026-11-19. Status: **documentation review only; no experiment**.

## Scope and distinction from existing work

R130 studies the child-creation transaction and R131 studies restricted tokens/integrity. This note isolates the **child-process creation restriction as a security policy**, especially its documented bypass conditions, and asks where a Linux-hosted NT process broker must enforce it. It does not claim that a Linux process hierarchy implements Windows policy.

## Verified primary-source contracts

1. **VERIFIED — creation-time child policy.** Microsoft's `UpdateProcThreadAttribute` documents `PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY` as a DWORD child-creation policy for a process being created (Windows 10 / Windows Server 2016 onward). `PROCESS_CREATION_CHILD_PROCESS_RESTRICTED` (0x01) prohibits that process from creating children; `PROCESS_CREATION_CHILD_PROCESS_OVERRIDE` (0x02) can be supplied only when the creating process is not restricted. The restriction becomes a property of the token under which the new process runs. Source: [Microsoft UpdateProcThreadAttribute](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute), Remarks, child-process policy values (accessed 2026-11-19).
2. **VERIFIED — the restriction is not an unconditional sandbox.** Microsoft explicitly cautions that it is effective only for sandboxed applications that prevent access to privileged process handles: access to another process with `PROCESS_CREATE_PROCESS` or `PROCESS_VM_WRITE` may enable bypass. This is a crucial security boundary, not an implementation detail. Same Microsoft source.
3. **VERIFIED — parent identity and creation authority are not identical.** Microsoft's `PROC_THREAD_ATTRIBUTE_PARENT_PROCESS` requires a process handle with `PROCESS_CREATE_PROCESS`; selected attributes (including token, handles, job, affinity) derive from that specified parent, while other attributes (for example debug port) can derive from the creator. Therefore the broker must model *requester*, *designated parent*, *token* and *handle rights* separately. Same Microsoft source, attribute table.
4. **VERIFIED — Linux `clone`/`clone3` are process-creation mechanisms, not Windows token-policy engines.** Linux documents process creation and control over shared VM, descriptor tables, signal handlers, namespaces and (with clone3) cgroup placement. These controls do not, by themselves, express the Microsoft token restriction above. Source: [Linux clone(2) manual](https://man7.org/linux/man-pages/man2/clone.2.html), DESCRIPTION (accessed 2026-11-19).
5. **VERIFIED — Windows process mitigations are policy-specific.** Microsoft's `SetProcessMitigationPolicy` sets policies for the calling process, but its documented enumeration should not be mistaken for a universal child-creation enforcement primitive. The creation-time child-policy attribute is a separate contract. Source: [Microsoft SetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy) (accessed 2026-11-19).

## Architecture hypothesis (not implemented)

**HYPOTHESIS:** A trusted NT process broker should authorize each guest child-creation request against the effective guest token's child-creation restriction and the supplied parent-process handle's rights, before allocating host resources. A single Linux parent PID check is insufficient: it neither represents Windows' designated-parent semantics nor protects against an alternate privileged broker path. Maintain a capability-based distinction between guest `PROCESS_CREATE_PROCESS` handles and raw host process creation.

**HYPOTHESIS:** If untrusted PE machine code can directly invoke unrestricted Linux `clone`, `fork`, `vfork` or another host spawn path, a broker-only guest policy can be bypassed. The containment design must account for direct syscalls, delegated creation through other processes, privileged handles, and broker authorization. This is a security threat model, not proof of a working confinement mechanism. Linux seccomp/user-notification limitations already investigated in R129 remain relevant.

**OPEN:** Exact Windows error codes, precedence when creation-time policy collides with restricted tokens, alternate-parent creation, inherited handles and job restrictions, and version-specific policy propagation require Windows reference measurements. Do not invent these results.

## Falsifiable acceptance gates

- **Windows differential:** In a pinned Windows build, construct a launcher, a restricted child and a separate helper. Compare unrestricted and restricted child creation; test `OVERRIDE` from both permitted and restricted creators; capture API return, `GetLastError`, token information where exposed, and resulting process lineage.
- **Alternate-parent authority:** Try valid/invalid `PROCESS_CREATE_PROCESS` handles and separate designated-parent and creator identities. Record inherited token, handles and jobs; do not infer semantics from Linux PPID.
- **Bypass/security:** Attempt creation via delegated helper and via process handles with distinct access rights. Only run in a disposable controlled environment. Document exactly which Windows sandbox preconditions are satisfied.
- **Linux mediation:** Under an explicitly pinned kernel and isolation setup, try direct spawn syscalls and broker-mediated spawn, with and without guest restrictions; assert that denial occurs at every available creation ingress and cannot be evaded through an unrestricted host path.
- **Rollback/lifetime:** A denied or failed creation must not publish a guest process ID, usable child handle, token or leaked host process. Test races between handle closure, parent termination and broker decision.
- **Reporting:** Separate Windows-observed behavior, Linux-observed behavior and proposed emulation; capture kernel/OS versions, source commit, policy bits, access masks, privileges, sandbox configuration and every PASS/SKIP.

## Decision gate

**OPEN:** Do not claim enforceable Windows child-process mitigation until all guest creation routes are mediated or otherwise isolated and a Windows reference matrix validates policy behavior. This note recommends no kernel fork and records no performance measurement.
