# R182 — Image-load origin, integrity and signature mitigation gates

Status: **VERIFIED FROM PRIMARY MICROSOFT API DOCUMENTATION** for the enumerated Windows contracts; **HYPOTHESIS** for LinuxNTAlpha implementation. Reviewed 2026-11-19. **No experiments performed.**

## Question and relation to existing work

How can a Linux-hosted NT-compatible loader preserve Windows per-process restrictions on the *origin*, *integrity label* and *signer* of executable images? This is distinct from R173 (DLL search order/path containment), R174 (dynamic code generation), and R180 (CFG indirect targets). Finding a DLL, permitting executable memory, and authorizing its image provenance are different decisions.

## Verified Windows contracts

1. Microsoft's [PROCESS_MITIGATION_IMAGE_LOAD_POLICY](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_image_load_policy) documents **NoRemoteImages** (reject images from remote devices such as UNC shares), **NoLowMandatoryLabelImages** (reject images carrying a Low mandatory label), and **PreferSystem32Images** (search System32 before the application directory in the standard DLL search order). It also exposes **AuditNoRemoteImages** and **AuditNoLowMandatoryLabelImages** fields. Documentation does not establish that a Linux mount, Linux uid, or Unix pathname alone identifies Windows remote-device or mandatory-integrity semantics.
2. Microsoft's [PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process_mitigation_binary_signature_policy) separately exposes **MicrosoftSignedOnly**, **StoreSignedOnly**, **MitigationOptIn** (documented Microsoft/Store/WHQL signer restriction), and audit fields. These are signer/trust policy flags, not merely a check that a PE contains any signature.
3. [GetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocessmitigationpolicy) documents querying both policy classes and requires PROCESS_QUERY_INFORMATION on the process handle. [SetProcessMitigationPolicy](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessmitigationpolicy) documents setting process mitigation policy and recommends early application; some policy transitions are restricted. These sources do not specify every cross-version transition for the two classes.
4. Microsoft's [VirtualProtect](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect) documents changing page protections. This alone does not supply image provenance, signature-trust or mandatory-integrity checks.

## Architecture hypothesis (not implemented)

Maintain guest process mitigation state independent of Linux mmap permissions. At the guest loader's image-admission boundary, resolve the guest module identity and namespace; establish whether the image originated from a guest remote device; obtain guest mandatory-integrity metadata; and, when signature restrictions apply, validate the appropriate guest trust-chain/signer policy **before** mapping executable image pages. PreferSystem32Images belongs in the name-resolution phase, while the other image-load flags belong in admission. Do not treat Linux ownership, executable bits, xattrs, mount types, or host CA trust as automatically equivalent to Windows integrity labels or Microsoft/Store/WHQL trust.

**Security hypothesis:** A precheck followed by pathname reopen can create a time-of-check/time-of-use substitution. Keep a stable, validated file identity and content snapshot or equivalent trustworthy handle through admission and mapping. Ensure guest load paths cannot bypass admission through aliases, alternate paths, already-open file descriptors or direct executable mappings; the exact Windows boundary for each route remains to be measured. Host filesystem and process isolation must remain independently enforced.

**OPEN:** Version-specific behavior of audit flags, image-load failure codes, preloaded modules, mapped image sections, policy updates after load, Windows catalog signatures, certificate revocation and timestamp handling, redirected System32 and WOW64, remote-device reparse/DFS paths, and mandatory-label inheritance. No claim of compatibility for any of these is made.

## Falsifiable reference and Linux gates (not executed)

1. On a recorded Windows build, query initial policies; enable each documented flag independently at the earliest supported point. Record API return codes and GetLastError; separately test later policy transitions. Use a controlled signer matrix: unsigned, self-signed, Microsoft/Store/WHQL signed **only when legitimately available**. Do not fabricate trusted signatures.
2. Load the same harmless test DLL by explicit path, ordinary search, and alternate alias from local and UNC paths; compare denial, audit observations and PreferSystem32Images precedence. Include guest low-integrity-labeled fixtures made using supported Windows security APIs, recording ACL/label and token integrity.
3. Probe mapped-image sections, existing modules, and concurrent replacement or rename races. Isolate crashes and record whether rejection occurs at search, open, image-section creation, or load.
4. For the Linux prototype, test that the policy engine rejects guest-disallowed images even when Linux filesystem permissions allow them, and that host access denial cannot be overridden by guest policy. Test stable-file identity across rename and symlink changes; record exact kernel and filesystem versions.
5. Keep source-documentation checks, Windows-reference observations and Linux prototype results in separate evidence columns. No result or performance advantage is asserted until these gates are run.

## Research limitations

This is an API-contract study, not a claim of Windows signature validation or image mitigation support. Microsoft Learn pages are not pinned to a specific Windows binary build; build-dependent details remain OPEN. No source code was copied and no experiments were executed.
