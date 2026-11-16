# R122 — Windows child-handle inheritance versus Linux descriptor inheritance

Date: 2026-11-16. Status: **VERIFIED FROM PRIMARY API DOCUMENTATION** for public contracts; **HYPOTHESIS** for architecture; **OPEN** for differential conformance. **No experiment was run.**

## Distinct architectural gap

R34 addresses process parameters/environment inheritance, R67 cross-process handle duplication, and R68 pseudohandles. This note isolates **child process creation's inheritable-handle selection, stable guest handle values, rights and lifetime**. An NT handle table is not a Linux file-descriptor table, and the host's fork/exec descriptor behavior cannot stand in for guest object-manager inheritance.

## VERIFIED — Windows documented behavior

1. `CreateProcessW` with `bInheritHandles=TRUE` inherits inheritable handles; with FALSE it does not. Inherited handles retain **the same numeric value and access rights** as the parent handles. Microsoft documents restrictions across Terminal Services sessions and PPL protection boundaries. [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).
2. Handle inheritance requires both an inheritable handle (creation security attributes, duplication, or `SetHandleInformation`) **and** process creation configured to inherit. `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` permits an explicit child-specific selection. Inherited handles reference the **same object**, not a newly opened path. Pseudohandles, DLL module handles, and GDI/USER handles are not general inheritable object handles. [Microsoft, Inheritance](https://learn.microsoft.com/en-us/windows/win32/procthread/inheritance).
3. Microsoft warns that blanket inheritance is hazardous when multiple parent threads spawn children with different intended handle sets; explicit handle lists are recommended. `STARTF_USESTDHANDLES` and startup standard-handle fields need separate validation: the documented fields are copied without validation, so a valid numeric value is not guaranteed. [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw); [Creating Processes](https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes).
4. `lpProcessAttributes`/`lpThreadAttributes` govern inheritability of the **newly returned process/thread handles**, not the same setting as `bInheritHandles` for existing parent handles. [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).

## VERIFIED — Linux documented behavior and mismatch

1. `execve` normally retains open file descriptors unless `FD_CLOEXEC` is set; this is a host descriptor flag, not a Windows per-handle inheritance policy or access mask. [Linux execve(2)](https://man7.org/linux/man-pages/man2/execve.2.html); [Linux F_GETFD/F_SETFD](https://man7.org/linux/man-pages/man2/F_GETFD.2const.html).
2. Without `CLONE_FILES`, `clone` copies the descriptor table but references the same underlying open-file descriptions; with `CLONE_FILES`, descriptor-table changes are shared until `execve` unshares the table. Neither mode models arbitrary guest NT objects or guarantees preservation of guest handle numbers. [Linux clone(2)](https://man7.org/linux/man-pages/man2/clone.2.html).
3. Linux `FD_CLOEXEC` is insufficient to represent inherited event, semaphore, section, process, thread, or other guest handles that have no corresponding host descriptor. Conversely, allowing every host descriptor to survive `execve` risks leaking runtime-internal resources. This mismatch follows from the documented API types; no leak experiment has been performed.

## HYPOTHESIS — fail-closed design candidate

- Give each guest process a separately owned handle table. Each entry has a guest-visible integer, object reference, granted access, inheritance flag and lifecycle metadata. Preserve selected **guest handle values** at child creation and increment the underlying guest object references; do not equate guest handle integers with host fd integers.
- Snapshot and validate the child inheritance selection under a defined process-creation transaction. Resolve explicit handle lists against parent handles and reject stale/closed handles according to Windows-reference results, not guessed error codes. Test concurrent parent close/duplication and simultaneous spawns.
- Mark **all runtime-owned host descriptors close-on-exec by default**. Explicitly transfer only required host capabilities through a private child bootstrap protocol, with independent guest handle-table reconstruction. Avoid exporting host fds directly as NT handle identities.
- Treat standard handles as separate guest process-parameter fields pointing to guest handles, not automatically fd 0/1/2. Validate semantics against Windows versions and console/pipe cases before implementing.
- Preserve access checks and cross-session/security boundaries independently of Linux fd possession; the guest security token and object-manager policy remain authoritative in this candidate.

## Proposed falsification gates — NOT RUN

1. Windows x64 oracle: create inheritable and non-inheritable event, mutex, pipe, file and section handles; spawn children with `bInheritHandles` false/true; query handle values, granted access, object identity and shared signaling.
2. Repeat with `STARTUPINFOEX` handle lists (including handles not marked inheritable, duplicates, closed handles and malformed list entries); capture actual errors, not presumed statuses.
3. Spawn two children concurrently with disjoint handle lists while a third parent thread duplicates/closes handles; record unwanted inheritance and race behavior under a watchdog.
4. Test `STARTF_USESTDHANDLES`, invalid standard-handle values, console versus detached children and Windows version differences. Distinguish documented behavior from observed behavior.
5. Linux prototype: instrument child `/proc/self/fd` before and after bootstrap; assert no unrelated host descriptor leaks and verify guest object handles survive even without corresponding Linux descriptors.
6. Cross-process teardown: parent closes inherited handles immediately after spawn; verify child remains able to use the shared guest object until its own last close. Include access-denied and stale-handle negative controls.

## OPEN

- Native `NtCreateUserProcess` inheritance and attribute-list behavior, including Windows build-dependent errors and creation ordering, need independently inspected primary declarations and oracle tests.
- Whether snapshot semantics are atomic relative to concurrent handle close/duplication is not established by these public API descriptions.
- Host bootstrap and descriptor-passing transport, cross-session guest policy, and object-manager integration are not implemented or benchmarked.
- No claim is made that `fork`, `clone`, `posix_spawn` or `execve` alone implements NT process creation.

## Architectural disposition

Keep inheritance inside the guest object manager/process-creation transaction. Do not expose host descriptor inheritance as a compatibility guarantee or change ADR-003 on documentation alone.
