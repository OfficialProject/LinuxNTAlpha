# R67 — Cross-process NT handle duplication and Linux descriptor-transfer boundaries

Research date: 2026-11-13. **Primary-documentation review only. No Windows reference tests, Linux tests, implementation, or performance measurements were run.**

This deepens R10's typed-handle model and R34's child inheritance discussion. The specific question is whether a Windows-compatible `NtDuplicateObject` operation can be faithfully implemented using Linux `dup`, `pidfd_getfd`, or descriptor passing.

## VERIFIED — Windows documented contracts

1. **Process-relative handles and authorization.** `DuplicateHandle` resolves the source handle in the specified source process, creates the result in the target process, and requires `PROCESS_DUP_HANDLE` rights on both process handles. The resulting numeric handle is valid in the target process, not automatically in the caller. [Microsoft DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle); [process rights](https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights).
2. **Same object, independently managed handle.** A duplicate references the same underlying object; for a file, the documented file-position state is shared. The new handle can have its own inheritance flag and requested access, subject to object-specific restrictions. `DUPLICATE_SAME_ACCESS` ignores requested access. Microsoft warns that access amplification behavior varies by object type: it is not safe to assume either unrestricted elevation or universal attenuation. [Microsoft DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle).
3. **Close-source failure semantics are exceptional.** `DUPLICATE_CLOSE_SOURCE` closes the source even when the duplication operation returns an error. This cannot be implemented by an unconditional 'commit target then close source' rule without reproducing the observed failure cases. [Microsoft DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle).
4. **Native interface carries handle attributes separately.** Microsoft's `ZwDuplicateObject` contract documents `DesiredAccess`, `HandleAttributes`, and `DUPLICATE_SAME_ATTRIBUTES` in addition to `DUPLICATE_SAME_ACCESS`; its documentation directs user-mode callers to the `NtDuplicateObject` name. Do not assume the Win32 Boolean inheritance argument is the complete native attribute model. [Microsoft ZwDuplicateObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-zwduplicateobject).
5. **Inheritance is a different operation.** Inherited handles preserve numeric value and access rights in the child, whereas cross-process duplication creates a handle in an already designated target process. An explicit process-creation handle list is also documented. [Microsoft inheritance](https://learn.microsoft.com/en-us/windows/win32/procthread/inheritance).
6. **Object-specific restrictions matter.** Microsoft's `DuplicateHandle` documentation explicitly discourages its use for I/O completion ports and sockets; socket transfer has a separate Winsock mechanism. A universal 'all NT handles are duplicable' policy is unsupported. [Microsoft DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle).

## VERIFIED — Linux mechanism is not the NT contract

Linux `pidfd_getfd` duplicates another process's file descriptor into the *calling* process, requires a `PTRACE_MODE_ATTACH_REALCREDS` access check, and creates a close-on-exec descriptor. Its permission model and destination are not the Windows `PROCESS_DUP_HANDLE`/target-process semantics. [Linux pidfd_getfd(2)](https://man7.org/linux/man-pages/man2/pidfd_getfd.2.html). This is a kernel facility for obtaining an fd, not a complete Windows object handle manager.

## HYPOTHESIS — implementation boundary

A compatibility handle broker should maintain a **per-NT-process** table mapping opaque handle values to typed object references, granted-access masks, and handle attributes. Duplication should be modeled as a rights-checked operation between two such tables; a Linux fd is an optional backing resource, not the externally visible handle. For file-backed objects, preserve shared file-position identity where required; for purely virtual NT objects, no Linux fd may exist. This design has **not** been validated against Windows behavior.

Avoid using Linux `pidfd_getfd` as a blanket duplication primitive: it changes the receiver and authorization model and does not cover non-fd NT objects. If a broker transfers an underlying fd, its NT handle-table transaction must still separately enforce access, lifetime, inheritance and object-type rules.

## OPEN questions and proposed differential gates — NOT executed

| Probe | Windows reference observation to capture | Required design decision |
| --- | --- | --- |
| Same-process duplicate, same access vs requested reduced access | Return status, granted access, shared object state | Access-mask calculation per type |
| Cross-process source/target with and without `PROCESS_DUP_HANDLE` | Status, destination table visibility, source persistence | Authorization at both endpoints |
| `DUPLICATE_CLOSE_SOURCE` with invalid target, invalid requested access and success | Whether source remains usable after each return | Failure/close ordering and error mapping |
| Native `DUPLICATE_SAME_ATTRIBUTES`, inheritable and noninheritable cases | Resulting flags; child inheritance after duplication | Native handle-attribute representation |
| Duplicate file and move file pointer through either handle | Shared pointer behavior and close lifetime | Shared backing-open identity |
| Duplicate event, section, process, token, IOCP and socket | Per-type success/failure and supported transfer method | Typed policy, not fd-only policy |
| Handle reuse under concurrent close/duplicate and target process exit | Observed statuses, stale-handle safety and leak counts | Table locking, generation protection and rollback |

Use a dedicated Windows-reference harness recording OS build, process architecture, tokens, rights, object type, return status, and source/target postconditions. Add a separate isolated Linux broker model test with injected failures **after** the Windows observations establish expected semantics. Never label proposed tests as PASS.

## Security and architecture gate

The critical invariant is **no authority amplification from a forged handle, cross-process lookup, stale table slot or backend fd transfer**. Treat this as a threat-model requirement, not a proven property. Do not select a Linux-kernel modification or promise zero-copy/performance benefits from these documents alone. No Windows compatibility milestone is advanced by this research note.
