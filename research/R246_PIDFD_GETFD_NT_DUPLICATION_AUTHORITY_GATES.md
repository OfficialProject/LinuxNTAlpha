# R246 — pidfd_getfd versus NT cross-process handle duplication authority

Reviewed: 2026-11-22. Status: **primary-source documentation review only**. No Windows or Linux experiment, benchmark, or implementation was performed.

## Question and boundary

R67 addresses cross-process duplication generally; R224 covers type-specific granted access; R244 covers close protection; R232/R233 address supervisor/process identity. This note isolates a narrower Linux backend choice: **can `pidfd_getfd` replace a guest NT handle-duplication operation, especially when a supervisor mediates multiple Linux processes?** Answer from the documented contracts: **not by itself**. This is a semantic gap assessment, not an architecture decision.

## VERIFIED — independently published primary contracts

1. Microsoft's [DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle) requires `PROCESS_DUP_HANDLE` access to both source and target process handles, creates the new guest handle **in the target process**, and returns a value valid in that target's handle table. `DUPLICATE_SAME_ACCESS` copies granted access; otherwise desired access is object-type-dependent. The documentation explicitly notes some object types/rights cannot be freely elevated and that console handles have same-process restrictions. It warns against duplicating sockets and IOCP handles by this API. This is not an arbitrary host-fd transfer contract.
2. Microsoft's [ZwDuplicateObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-zwduplicateobject) describes separate source and target process-handle rights, desired access, handle attributes, `DUPLICATE_SAME_ATTRIBUTES`, `DUPLICATE_SAME_ACCESS`, and `DUPLICATE_CLOSE_SOURCE`. Kernel-mode Zw behavior and user-mode Nt behavior must not be conflated.
3. Linux [pidfd_getfd(2)](https://man7.org/linux/man-pages/man2/pidfd_getfd.2.html) (introduced Linux 5.6) creates a new descriptor **in the calling process**, copying an existing descriptor from the process referenced by a pidfd. Both refer to the same open file description and share file status flags and offset. The returned descriptor has `FD_CLOEXEC` set; `flags` must currently be zero. Its permission gate is `PTRACE_MODE_ATTACH_REALCREDS`, not a Windows `PROCESS_DUP_HANDLE` guest-access check. The call can fail with `EPERM`, `EBADF`, `ESRCH`, `EMFILE`, or `ENFILE`, among others.
4. Linux [unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html) documents `SCM_RIGHTS` descriptor passing between cooperating processes using Unix sockets. The `pidfd_getfd` manual explicitly contrasts the cooperative `SCM_RIGHTS` mechanism with noncooperative `pidfd_getfd`. Neither operation, alone, allocates a Windows guest handle with NT access masks, object type, attributes, or namespace identity.
5. Microsoft's [Kernel Objects](https://learn.microsoft.com/en-us/windows/win32/sysinfo/kernel-objects) distinguishes a duplicated file handle sharing the same file object/file pointer from a separately opened file handle with a separate file object. This makes Linux open-file-description sharing a useful *partial* analogue for file handles, not for every NT object type.

## Derived incompatibilities (reasoned implications, not measured results)

- **Direction mismatch:** `pidfd_getfd` imports an fd into the *caller*. NT duplication publishes a handle into a *specified target*. A broker calling `pidfd_getfd` cannot thereby directly install an NT handle into an unrelated target's guest handle table.
- **Authority mismatch:** Linux ptrace credentials decide host fd extraction; guest process-handle rights and per-object granted masks decide NT duplication. Allowing a guest to select an arbitrary host pidfd/targetfd would bypass the proposed guest authority boundary.
- **Type mismatch:** NT events, registry keys, sections, process/thread handles and other object-manager references need not correspond one-to-one with a Linux fd. Duplication must retain guest object identity and independent per-handle metadata.
- **Lifetime mismatch:** a copied host fd keeps an open file description alive, but guest source/target handle generations, protection flags, inheritance and close-source ordering remain independent. `FD_CLOEXEC` does not express NT inheritance or close protection.
- **Status mismatch:** host `errno` cannot be blindly forwarded as NTSTATUS. The failing stage (guest access validation, source lookup, host transfer, target publication, source close) must be identified before status translation.

## HYPOTHESIS — candidate broker transaction (NOT IMPLEMENTED)

1. Authenticate guest caller and source/target **guest** process objects, validate `PROCESS_DUP_HANDLE` and any object-type-specific restrictions. Pin source handle generation and guest object reference.
2. Determine new guest granted mask and attributes under the requested NT duplication flags. Reserve a target guest-handle slot, without making it visible.
3. If the object requires a host fd in a distinct worker, use a deliberately authorized transfer path (possibly `SCM_RIGHTS`, or `pidfd_getfd` when its ptrace security policy is appropriate). Do not equate successful fd transfer with guest publication.
4. Atomically publish the target guest handle or roll back the reservation, transferred fd and references. Treat `DUPLICATE_CLOSE_SOURCE` as a separate, precisely ordered guest transition; R244 flags unresolved protected-close behavior.
5. On worker crash, stale slot reuse or transport failure, fail closed without exposing a newly transferred fd as an unapproved guest capability.

This sequence is a **candidate**, not a claim that all Windows edge cases can be reproduced by a user-space broker. Transaction boundaries and error ordering require Windows-reference observations.

## Unrun acceptance gates

| Gate | Windows reference | Isolated Linux/model probe |
| --- | --- | --- |
| Rights | Source/target `PROCESS_DUP_HANDLE` denied independently; reduced vs same-access duplicates | No guest duplication merely from host ptrace permission |
| Destination | Duplicated value works in target but not accidentally in source | Broker-imported fd remains unpublished until guest target slot commits |
| Sharing | Duplicated file pointer versus separately reopened file pointer | `pidfd_getfd` shares open file description/offset |
| Metadata | Inheritance, protected-close, `DUPLICATE_SAME_ATTRIBUTES` and close-source failure ordering | Separate per-handle attributes, generation and rollback |
| Types | Event, file, section, registry key, process, console, socket and IOCP restrictions | Non-fd objects duplicate without inventing host descriptors |
| Failures | Source/target termination and concurrent close during duplication; exact statuses | `EPERM`, `ESRCH`, `EBADF`, resource exhaustion, fd reuse, worker crash |

Record Windows build, exact API, caller token, source/target access masks, object type, handle flags, return/error/status and handle survival. Record Linux kernel, ptrace/Yama/LSM policy, namespaces, pidfd identity and fd ownership. **No rows have been executed.**

## Architecture decision gate

Do not choose `pidfd_getfd` as the NT handle-duplication primitive merely because it can import another process's fd. It may be a **backend transport optimization** for a subset of file-backed guest objects after explicit guest authorization and target-handle publication are implemented. Benchmark and security-test it against cooperative `SCM_RIGHTS` under the actual process topology before selecting a transfer mechanism. No claim of performance advantage is supported here.

## Source provenance and limits

Microsoft Learn and Linux man-pages primary contracts accessed 2026-11-22. Documentation is not Windows behavioral testing; `DuplicateHandle` versus `NtDuplicateObject` error and protected-close corner cases remain OPEN. Linux Yama/LSM policy and kernel-version differences require host-specific verification. No source code was imported.
