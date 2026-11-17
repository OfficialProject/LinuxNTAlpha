# R130 — Windows child-process creation versus Linux exec boundaries

Status: **primary-documentation review only** (2026-11-17 UTC). No code execution, Windows-reference experiment, performance measurement, or NT process-creation implementation is claimed.

## Question and scope

Can the LinuxNTAlpha PE executor use ordinary Linux `execve` as the complete implementation of Windows `CreateProcessW`? **No: the documented entry, identity, startup and security contracts differ.** This is a design constraint, not evidence that Linux cannot host an NT-compatible process. This note concentrates on the **creation transaction and initial publication**, complementing R99 (command line/environment), R122 (handle inheritance), R125 (thread startup), R73 (wait/exit), R92 (security), and R115 (integrated runtime).

## Directly inspected primary contracts (VERIFIED FROM SOURCE)

1. Microsoft `CreateProcessW` creates a **new process and primary thread** and documents separate application-name and mutable command-line parameters, process/thread security attributes, inheritance, flags, environment, working directory, startup information and returned process/thread handles. The child's security context normally derives from the **calling process token**, not a thread impersonation token. Source: [Microsoft CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw), inspected 2026-11-17 UTC.
2. Microsoft specifies that inherited handles preserve **numeric handle values and access rights**, when inheritance is enabled; it also documents conditions and restrictions. This does not imply that Linux fd numbers can be exposed directly as guest NT handles. Same source.
3. Microsoft documents `CREATE_SUSPENDED`: the primary thread does not run until resumed. It also documents creation flags for console, process group, debug relationships, environment encoding, and job breakaway with restrictions. Source: [Microsoft Process Creation Flags](https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags), inspected 2026-11-17 UTC.
4. Linux `execve` **replaces the calling process image**, passing an argv vector and environment vector. On successful exec it does not return; it resets or removes numerous process attributes and destroys all threads except the calling thread. Open descriptors survive unless close-on-exec. Source: [Linux man-pages execve(2)](https://man7.org/linux/man-pages/man2/execve.2.html), inspected 2026-11-17 UTC. The online man page is not a pinned kernel implementation; exact kernel-version behavior remains to be checked when testing.

**Consequence:** A bare `execve(PE_path, argv, envp)` neither loads a Windows PE32+ image through Linux's ordinary ELF interpreter nor satisfies guest creation semantics. An ELF runtime launcher *could* be started through Linux process creation, then load a PE in the child, but guest state and process/thread handles must be independently modeled. This last sentence is an **architectural inference**, not a tested implementation.

## Candidate creation transaction (HYPOTHESIS; NOT IMPLEMENTED)

- **Validate** the guest application name, command line, flags, working directory, image type, guest token rights, environment and inheritance list before any guest code runs. Fail unsupported creation flags explicitly.
- **Allocate** a new host child in a parked bootstrap state with a private guest process record and distinct guest process/thread handles. Linux PID/TID/fd values remain backend identifiers, not guest ABI values.
- **Construct** the child's PE image, PEB/TEB and process parameters; resolve imports and initialize required runtime state before releasing the primary guest thread. A minimal PE fixture does not prove these structures exist.
- **Publish** guest process/thread objects and the exact permitted inherited guest handles only after the child is ready; distinguish externally visible creation success from later image-initialization failure. This ordering needs Windows-reference verification, not assumption.
- **Resume** only according to guest suspension state; `CREATE_SUSPENDED` must not allow guest entry code to execute prematurely.
- **Rollback** failed launches without leaked host fds, mappings, guest handles, namespace references, credentials or zombie children. A failed child must not inherit ambient privileged host resources.

**Security warning:** A host child is not automatically isolated. Host credentials, environment, fd inheritance, filesystem visibility and Linux namespace access require explicit threat modeling. SUD is not a sandbox (R16/R90). Avoid executing untrusted PE images in the current experimental executor.

## Falsifiable conformance gates (OPEN; NOT RUN)

1. **Creation output:** On a version-recorded Windows reference, test `CreateProcessW` with distinct `lpApplicationName` and `lpCommandLine`, with quoted spaces, empty/explicit environment, current directory, and `CREATE_UNICODE_ENVIRONMENT`; capture child-visible command line and environment, returned process/thread IDs, and error results. Separate from R99 by recording parent-side success/failure and handle publication.
2. **Suspension:** Launch a deterministic child with `CREATE_SUSPENDED` and an observable first instruction/entry marker; verify no guest marker before `ResumeThread`. Test resume count and termination before resume. Do not infer correctness from a stopped Linux process alone.
3. **Token boundary:** Compare caller process-token versus impersonated-thread behavior on Windows using only disposable local test identities; compare with a host child created under restricted Linux credentials. Do not assume UID equivalence to Windows token/DACL rules.
4. **Inheritance and failure:** Test inheritable and non-inheritable guest handles, inherited numeric values, invalid creation flags, missing image, malformed PE and deliberately failed bootstrap. Inventory host fds and child processes after every failure. R122 covers handle transfer; this gate covers **atomic creation and rollback**.
5. **Concurrency:** Concurrent creators, parent exit during bootstrap, child exit before publication, and cancellation/timeout must produce deterministic guest handles and no orphaned resources.
6. **Host implementation comparison:** Compare a separate ELF launcher process and a parked `clone`/fork-based bootstrap on identical fixtures. Record kernel version, libc, launch mode, trace, resource usage and repeated latency distribution; no performance superiority is asserted here.

## Decision impact

**OPEN:** Choose the first supported subset of Windows creation flags and guest process semantics, and determine the precise observable creation-success point using Windows-reference traces. **Recommendation, not ADR:** defer general `CreateProcessW` compatibility until R115's single-process integration milestone works, but design the process object and guest handle table so a child bootstrap can be added without exposing Linux PID/fd identities.

## Nonduplication and limitations

R99 concerns child-visible strings; R122 inherited handle values; R125 guest thread bootstrap; R73 wait/exit. This note instead identifies the **cross-process creation transaction**, its security context, suspended initial thread, publication boundary and failure rollback. Microsoft documentation was directly inspected but is not tied to a frozen Windows build; no ReactOS/Wine source was audited and no native `NtCreateUserProcess` contract is claimed. Next step: build a Windows-reference differential harness before implementing or claiming these semantics.
