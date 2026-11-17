# R150 — Windows image-signature mitigation versus Linux file-integrity enforcement

Status: SOURCE-VERIFIED boundary analysis; implementation HYPOTHESIS; no experiment executed. Reviewed 2026-11-17.

## Scope and nonduplication

R69 addresses dynamic-code/executable-memory restrictions and R144 addresses CFG loader targets. This note isolates a different contract: **publisher-signature policy at Windows image-load time**, and why Linux fs-verity/IPE are not drop-in implementations. No claim of Windows Authenticode validation or security equivalence is made.

## Verified from primary documentation

1. Microsoft documents `ProcessSignaturePolicy` as a process mitigation restricting images loaded into a process, with Microsoft, Store, and WHQL-related options. `PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY` defines `MicrosoftSignedOnly`, `StoreSignedOnly`, `MitigationOptIn`, and audit bits. This is **publisher classification**, not merely byte integrity. [MS1, MS2]
2. `SetProcessMitigationPolicy` applies to the calling process; `GetProcessMitigationPolicy` queries a process with appropriate access. The API surface and application timing must be considered independently from Linux file permission checks. [MS1, MS3]
3. Linux fs-verity provides Merkle-tree-backed integrity of read-only files, including verification of mmap-backed reads. The Linux kernel documentation explicitly states that fs-verity alone does not authenticate a publisher and does not mandate a particular authentication scheme. [L1]
4. The same Linux documentation cautions that built-in fs-verity signatures **do not require arbitrary files to use fs-verity**; a complete policy requires trusted userspace checks or another enforcement layer. Its built-in signature trust keyring is shared system-wide, not naturally a per-Windows-process publisher trust store. [L1]
5. Linux IPE can make kernel-enforced decisions using `fsverity_digest` and `fsverity_signature` for operations including `EXECUTE`; these are Linux integrity-policy properties, not evidence that a PE's Windows Authenticode publisher is Microsoft or the Store. [L2]

## Consequence: three separate decisions

- **Windows-visible policy state**: retain the exact per-process mitigation flags, query/set access semantics, creation-time inheritance/initialization, and audit-versus-block behavior. Source-verified API existence; full state-transition semantics remain OPEN.
- **PE publisher authentication**: establish a trustworthy verification path for the *actual mapped image* and its certificate chain/publisher classification. HYPOTHESIS: a dedicated trusted loader/broker can perform this for PE images before admitting them to the guest namespace. Windows catalog signatures, embedded signatures, trust roots, revocation, timestamping, policy-version behavior, and PE hashing rules are OPEN.
- **Host integrity and enforcement**: fs-verity plus a suitable Linux enforcement policy may protect immutable host files, but is neither necessary nor sufficient to establish Windows publisher identity. The Linux enforcement scope and trust-root ownership must not silently override guest per-process policies.

## Security boundary and falsifiable design gates

**HYPOTHESIS:** require a trusted image-load broker to authorize each PE mapping under the *target process's* effective signature policy, binding authorization to an immutable file identity/content digest and preventing replacement between verification and mapping. Never infer trust from pathname, extension, Linux uid, or fs-verity presence alone.

1. Verify a corpus of PE fixtures with known publisher classes, unsigned fixtures, deliberately modified signatures, and a catalog-signed fixture against a pinned Windows reference environment. Record policy flags and expected load result; no such tests have been run.
2. Test image-load paths separately: initial EXE, ordinary DLL, delayed import, explicit LoadLibrary, and section-backed image mapping. Determine which operations the documented mitigation actually covers before emulating it.
3. Test race controls: rename/replacement, hard links, writable file descriptors, file mutation after signature check, and mapping from unlinked handles. Reject a design where checking one file permits executing different bytes.
4. Test per-process isolation: one process with signature restriction, another without; verify host-wide IPE policy does not accidentally substitute for guest-visible semantics.
5. Distinguish *audit* from *enforcement*, and record the precise NTSTATUS/Win32 errors and event behavior on reference Windows before implementation.
6. Threat model the broker and trust-store update mechanism, including privilege separation, hostile PE parsing, compromised guest process, and policy downgrades.

## Architecture decision

**OPEN:** no kernel extension is justified by these documents alone. First evaluate a trusted userspace PE verifier/load gate and a narrowly scoped immutable-file integrity mechanism; compare against Linux IPE only when a concrete host-enforcement requirement exists. This is not a claim of equivalence, compatibility, or performance.

## Sources (primary; accessed 2026-11-17)

- [MS1] Microsoft, SetProcessMitigationPolicy: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy
- [MS2] Microsoft, PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY: https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_binary_signature_policy
- [MS3] Microsoft, GetProcessMitigationPolicy: https://learn.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-getprocessmitigationpolicy
- [L1] Linux kernel documentation, fs-verity (living documentation; not pinned to a kernel release): https://docs.kernel.org/filesystems/fsverity.html
- [L2] Linux kernel documentation, IPE, kernel 6.16 documentation snapshot: https://docs.kernel.org/6.16/admin-guide/LSM/ipe.html

Source-version limitation: Microsoft Learn and the unversioned fs-verity page are living references. This note does not claim exact Windows-build or Linux-kernel-version conformance. No implementation, benchmark, or runtime observation is represented as reproduced.
