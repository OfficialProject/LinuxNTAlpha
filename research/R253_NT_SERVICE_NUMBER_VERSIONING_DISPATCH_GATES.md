# R253 — NT service-number versioning and dispatch authority gates

Status: **DOCUMENTATION SYNTHESIS / HYPOTHESES; NO NEW EXPERIMENT**  
Date: 2026-11-22  
Scope: x86-64 Windows-origin native `syscall` instructions entering a Linux-hosted NT-compatible subsystem.

## Question

R26 proves that a specially built PE can issue one trapped `syscall` and receive a synthetic return value. R90, R234 and R236 examine interception, rearming, and security. **A different unresolved question is how a dispatcher identifies an NT operation without treating a numerical service index as a stable public ABI.**

## Verified upstream contracts

1. Microsoft documents that **Native System Services** are primarily used by operating-system components, and that their interfaces can change between Windows releases. Applications should generally use documented Win32 interfaces. This is a warning against assuming undocumented NT service implementation details or ordinals are permanently stable. [Microsoft: Windows Native System Services](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/windows-kernel-mode-native-system-services).
2. Microsoft documents that user-mode calls to `Nt` and `Zw` native service routines behave alike from user mode; their distinction matters to kernel-mode callers because of the previous-mode interpretation. Thus a Linux guest dispatcher cannot infer a trusted kernel caller merely from the function name or a guest-provided register. [Microsoft: Using Nt and Zw Versions of the Native System Services Routines](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-nt-and-zw-versions-of-the-native-system-services-routines).
3. The Linux Syscall User Dispatch interface is a **compatibility** facility to redirect syscalls from selected address regions, with a userspace selector and SIGSYS delivery. The kernel documentation explicitly cautions that it is not a security mechanism. This does not identify a Windows NT operation or validate its arguments. [Linux kernel: Syscall User Dispatch](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html).
4. The x64 Windows calling convention specifies integer/pointer register arguments and stack layout for normal calls, but that alone does **not** specify a universal, version-independent raw NT syscall number table. [Microsoft: x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention).

These are **documented interface observations**, not measurements of Windows syscall indices or evidence of compatibility with any particular Windows build.

## Architectural consequences — hypotheses, not implemented decisions

- **H1 — Symbol-first guest runtime:** Prefer independently implemented, named `ntdll` entry points whose semantic dispatch keys are internal stable identifiers. This is compatible with R28's narrow named export milestone and avoids assigning meaning to arbitrary raw numbers in the host API.
- **H2 — Versioned raw-service compatibility:** If direct Windows-origin `syscall` stubs must run, resolve the number using an explicitly selected, independently validated *guest Windows build and architecture profile*. Unknown numbers must fail closed as unsupported guest operations; they must never fall through to a Linux syscall merely because the numeric value matches.
- **H3 — Provenance as a separate axis:** An intercepted syscall's instruction address, guest thread identity, and current dispatch state must be checked separately from its numerical service identifier. A guest-controlled number is never authorization.
- **H4 — No universal table claim:** Neither a single snapshot of `ntdll` nor a third-party syscall-number list demonstrates that one mapping works across Windows versions, configurations or architecture variants.

## Proposed falsifiable tests (NOT RUN)

1. **Windows reference extraction:** For each specifically identified Windows build and x64 architecture, record executable and `ntdll` file versions, hashes, and observed stubs for a *small documented service set*. Distinguish a direct `syscall` stub from a jump, thunk, hook, or other implementation. Do not assume every export has an immediate service number.
2. **Version mismatch:** Feed the same synthetic raw service number into two deliberately different guest-version profiles; verify that dispatch uses the selected profile, not a hard-coded host-global table. Where mappings are not independently verified, return unsupported rather than guessing.
3. **Name-versus-number:** Call an independently implemented `ntdll` export through its PE import and separately through a version-pinned raw stub; compare return status and side effects for the same validated operation.
4. **Collision defense:** Deliberately use a number that names a legitimate Linux syscall but is absent from the guest profile. Confirm no host syscall executes. Run with SUD selector transitions and a tracing observer as designed in R234.
5. **Untrusted registers/pointers:** Fuzz invalid argument pointers, lengths, handles and service numbers in an isolated child; require bounded guest failure and no host privilege escalation.
6. **Concurrency and reload:** Change the selected guest profile only through a defined process-creation boundary; simultaneous threads must not see partially published dispatch maps.

## Open evidence requirements

- **OPEN:** A primary-source-supported, versioned method for obtaining actual Windows x64 raw service mappings on licensed reference installations. Microsoft documentation above does not publish a complete stable table.
- **OPEN:** Which Windows versions/builds the first native runtime will explicitly support.
- **OPEN:** The guest process identity and dispatcher ownership model, including crash recovery and concurrent threads (R232–R235).
- **OPEN:** A reproducible implementation of repeated traps with NTSTATUS, register preservation, rearming and host-syscall exclusion. R26 is only a single synthetic trap.
- **OPEN:** Whether raw stubs need support at all for the first milestone; named-import calls may be a narrower, better-evidenced initial target.

## Nonclaims

No Windows binary was inspected, no service index was verified, no syscall mapping was implemented, no test was run, and no security or performance improvement is asserted.

## Relationship to prior research

R26 establishes one synthetic raw PE trap. R90 covers guest/host syscall provenance. R234 covers transition/rearm invariants. R236 covers SUD mode/version drift. **This note isolates the additional *service identity and versioning* contract**, so those interception findings are not mistaken for an NT service-dispatch implementation.
