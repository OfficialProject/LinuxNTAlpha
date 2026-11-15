# R87 — NT object name removal, reference lifetime, and process-exit cleanup

Research date: 2026-11-14. Classification: **VERIFIED FROM PRIMARY DOCUMENTATION** and **HYPOTHESIS**. No Windows-reference tests, Linux experiments, benchmarks, or implementation were performed.

## Question and nonduplication boundary

R10 covers object-manager overview; R67 covers cross-process handle duplication; R68 covers pseudo-handles; R73/R74 cover process/thread termination and waitability. This note isolates a different failure mode: **a temporary NT object's namespace entry can disappear while the underlying object remains alive**, and a terminated process's handles do not necessarily imply that the object it referenced has been destroyed. A broker that treats a name, a Linux fd, and an object as the same lifetime unit will get this wrong.

## VERIFIED — primary Microsoft contracts

1. [Life Cycle of an Object](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/life-cycle-of-an-object) explicitly distinguishes **object reference count** from **open handle count**. Opening a handle increments both; a kernel pointer reference increments the object reference count without necessarily opening a handle. A temporary object's name is removed when its handle count reaches zero, but pointer references can keep the unnamed object alive. A permanent object retains an Object Manager reference and can remain named with no open handles.
2. [ZwMakeTemporaryObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwmaketemporaryobject) documents conversion of permanent objects to temporary ones and the name-removal rule. The distinction is not equivalent to POSIX unlink of an arbitrary filesystem path.
3. [Windows kernel-mode Object Manager](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/windows-kernel-mode-object-manager) identifies creation/destruction, namespace, process resources, access rights, and lifetime management as separate Object Manager responsibilities.
4. [Terminating a Process](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process) documents closure of a terminating process's handles and signaling of its process object; objects with other outstanding handles remain valid. [Process Handles and Identifiers](https://learn.microsoft.com/en-us/windows/win32/procthread/process-handles-and-identifiers) states that a handle to a process/thread can remain valid after the represented process/thread has terminated.
5. [Kernel object namespaces](https://learn.microsoft.com/en-us/windows/win32/termserv/kernel-object-namespaces) documents global and session-local names for selected named synchronization and mapping objects. Thus a string alone is not sufficient object identity without the namespace context.
6. [Windows security model for driver developers](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/windows-security-model) documents object security descriptors, access tokens and the granted rights stored with handles. A name lookup and a preexisting authorized handle are different security paths.

**Scope caution:** Microsoft's driver-facing pointer-reference rules describe kernel objects. They do not establish that ordinary user-mode clients can manufacture arbitrary kernel pointer references. The Win32 statement that an object survives while handles remain is compatible with, but not a complete substitute for, the deeper driver lifetime rules.

## HYPOTHESIS — minimum broker model

Represent three independent entities:

- **Namespace entry:** (namespace/session identity, name, object type, target object ID), removed under the guest object manager's rules.
- **Object record:** immutable unique identity, type-specific state, security descriptor, permanent/temporary status, reference accounting, and waitable state if applicable.
- **Per-process handle entry:** opaque process-relative number, object ID, granted-access mask, inheritance/protection attributes and a counted reference.

A broker should perform close and name-removal transitions under a serialization rule that prevents an open-by-name operation from binding to a half-deleted entry. A newly created object with the same name after the old temporary entry disappears must receive a **new object identity**, even if an internal reference still keeps the old object alive. Do not key identity only by string, host inode, Linux fd number, or a reusable slot without a stale-handle defense.

**Security hypothesis:** a stale numeric handle must never gain access to a new object occupying a recycled table slot. Reopen-by-name must perform the guest token/descriptor access check, whereas use of an existing handle must enforce its already-granted access mask. Linux UID or fd possession is not sufficient to prove either NT authorization rule. The exact NT handle-generation and reuse policy is **OPEN**; this is an invariant to test, not a claim that Windows exposes a generation counter.

## Proposed Windows reference conformance (NOT EXECUTED)

| Case | Required observations |
| --- | --- |
| Create named auto-reset event in process A; open same name in B; close A's handle | B's existing handle remains usable; reopening by name while B's handle remains succeeds subject to access |
| Close the final user handle to a temporary named event; create an event with the same name | New create/open result and state, name reuse, no accidental linkage to old identity |
| Duplicate a named event into B, terminate A, then operate via B | B's handle and event state after A exits |
| Create an event with same name in two sessions and with Global prefix | Distinct/session-shared identity, token rights, privilege-dependent creation outcomes |
| Create event with restrictive security descriptor; compare existing authorized handle versus unauthorized open-by-name | Access-check boundary and precise failure codes |
| Terminate a process while another process retains its process handle | Signaled state, exit-code query, handle lifetime and eventual close |
| Kernel-only pointer-reference/name-removal distinction | Only on an authorized controlled Windows kernel test host; **not** a user-mode claim |

Capture exact Windows build, token/session, object type, desired/granted access, create/open dispositions, return status and postconditions. Test cases must not assume user-mode access to permanent-object creation or kernel pointers. For name-reuse cases where object identity is not directly observable, use independent event state and carefully controlled handles; do not infer internal pointer equality.

## Linux prototype gate (NOT EXECUTED)

A model-only state machine can assert:
1. Two handles share one object ID while carrying distinct granted rights.
2. Closing one handle cannot destroy an object with another live handle.
3. The last temporary handle removes the name; any internal reference keeps the unnamed object alive.
4. Recreate-after-unlink gets a fresh ID; an old internal reference cannot alias it.
5. Process exit closes *that process's* handle entries only.
6. Session namespaces and access checks precede name-to-object resolution.
7. Concurrent open/close/recreate cannot produce stale-handle privilege escalation.

These are **proposed acceptance tests**, not evidence that a Linux implementation has passed them.

## Decision impact

Keep the Linux kernel-vs-userspace boundary OPEN. A broker-backed object graph is a candidate correctness baseline, not a performance result. Before choosing fd-backed fast paths or ntsync integration, prove that name removal, object lifetime, process exit, and guest authorization remain correct when the host backing resource's lifetime differs.

## Sources and retrieval scope

Primary Microsoft Learn documentation inspected on 2026-11-14; pages are living documents, **not pinned Windows kernel source or a version-specific binary ABI**. No Windows machine or kernel debugger was used. The behavior of a specific Windows build and any LinuxNTAlpha implementation remains unverified.
