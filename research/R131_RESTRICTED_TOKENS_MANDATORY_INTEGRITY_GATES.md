# R131 — Restricted tokens, mandatory integrity and Linux credential boundaries

Status: **PRIMARY-SOURCE REVIEW; NO EXPERIMENT** (2026-11-17 UTC). Complements R92 (DACL/access checks) and R81 (impersonation); neither substitutes for this token-policy layer.

## Verified from primary sources

1. Microsoft documents that `CreateRestrictedToken` can remove privileges, mark token SIDs deny-only and add restricting SIDs. A restricted-token access check must succeed both against enabled SIDs and against restricting SIDs; a single Linux UID/GID check does not implement that conjunction. [Microsoft, Restricted Tokens](https://learn.microsoft.com/en-us/windows/win32/secauthz/restricted-tokens).
2. `AdjustTokenPrivileges` cannot add privileges absent from the token. Disabled, removed and enabled privileges must not collapse into one host-capability bit. [Microsoft, Changing Privileges in a Token](https://learn.microsoft.com/en-us/windows/win32/secbp/changing-privileges-in-a-token).
3. Mandatory Integrity Control (MIC) is evaluated *before* discretionary access checks. The object integrity SID resides in a mandatory-label ACE in its SACL, not in the ordinary DACL. The documented default policy is no-write-up; an unlabeled object is treated as medium integrity. [Microsoft, Mandatory Integrity Control](https://learn.microsoft.com/en-us/windows/win32/secauthz/mandatory-integrity-control).
4. The mandatory-label ACE defines independent no-write-up, no-read-up and no-execute-up bits (0x1, 0x2 and 0x4). Do not model MIC as a universal deny for all access whenever caller integrity is lower. [Microsoft MS-DTYP, SYSTEM_MANDATORY_LABEL_ACE](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/25fa6565-6cb0-46ab-a30a-016b32c4939a); [Microsoft MS-DTYP, MandatoryIntegrityCheck](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/ae69a089-473d-4c23-bf3d-7a12a9d11123).
5. Linux user namespaces scope capabilities and UID/GID mappings to namespace relationships; they do not define Windows restricting-SID intersection or object mandatory-label checks. Joining a user namespace via `setns` has specific capability and multithreading restrictions. [Linux man-pages, user_namespaces(7)](https://man7.org/linux/man-pages/man7/user_namespaces.7.html); [setns(2)](https://man7.org/linux/man-pages/man2/setns.2.html).

These are **documented contracts**, not proof that LinuxNTAlpha implements them. Microsoft pages are living documentation; no Windows build-specific behavior has been established by this review.

## Architecture hypothesis (NOT verified)

Maintain guest token state independently of host credentials: enabled/deny-only/restricting SIDs, privilege presence and enablement, integrity SID and mandatory policy. A guest object access decision would first apply mandatory policy and then the appropriate discretionary/restricted-token checks, with generic access mapping and object-specific rights. Host kernel enforcement must **also** prevent guest code from bypassing the broker through direct Linux syscalls, inherited file descriptors, mappings, or shared IPC. A successful guest ACL check alone is not host isolation.

Avoid mapping a low-integrity guest token directly to an unprivileged Linux UID and declaring MIC implemented: guest token changes, existing host descriptors and Linux file ownership have different lifetime and enforcement rules. Likewise, a user namespace is an isolation primitive to evaluate, not an NT token emulator.

## Falsifiable conformance gates (proposed, not run)

- On a controlled Windows reference host, create a security descriptor granting write to a normal enabled SID but not to a dedicated restricting SID. Compare unrestricted and restricted token access; then add the restricting SID's grant and repeat. Verify returned token's restricting SID list; record OS build and token attributes.
- Repeat with a deny-only group SID and explicit allow/deny ACEs. Distinguish denial due to deny-only semantics from restricting-SID intersection.
- For low/medium integrity tokens and objects, vary mandatory-label mask among no-write-up, no-read-up and no-execute-up, while keeping DACL grants constant. Include unlabeled-object behavior and object-specific rights.
- Test impersonation versus primary-token identity, token duplication, privilege disable/removal, handle duplication and preexisting open handles. Capture whether checks occur at open, use or both for each object operation; do not infer one universal rule.
- For a Linux backend, run equivalent guest-access decisions under different host UID/GID, namespace and FD-inheritance configurations. Negative controls must attempt direct host syscalls and inherited-descriptor bypasses. No untrusted guest execution before isolation review.

## Open questions

- Which subset of NT token information classes and object types is necessary for the first real PE process milestone?
- How should access checks at handle creation be distinguished from per-operation privilege checks, and how are existing granted-access masks preserved?
- What isolation mechanism ensures the guest cannot bypass broker decisions through host syscalls, shared memory or passed FDs?
- Which Windows build(s) are the compatibility reference, and which access-check edge cases are undocumented or version-dependent?

**Decision:** No kernel/userspace boundary change justified; keep ADR-003 open. No performance, security or conformance experiment was executed for this note.
