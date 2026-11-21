# R223 — AppContainer/LowBox identity and Linux isolation are not interchangeable

Research date: 2026-11-21. Status: documentation-only architecture investigation; **no Windows or Linux experiment was run**.

## Scope and evidence

**VERIFIED (Microsoft documentation):** `CreateAppContainerProfile` creates a *per-user, per-application* AppContainer profile and returns its package SID. Microsoft's AppContainer launch procedure creates a profile first; for an existing profile it derives the SID from the name. It also documents redirected `LOCALAPPDATA`, `TEMP`, and `TMP` profile paths. These are guest-visible identity and storage contracts, not merely host process restrictions. [M1, M2]

**VERIFIED (Microsoft documentation):** `NtCreateLowBoxToken` takes an existing token, a package SID, a list of capability SIDs, and optional handles for named object directories; it returns an access-token handle. Its documented caller restriction is medium integrity level or higher. This is a distinct security-token operation, not a request to create a Linux namespace. [M3]

**VERIFIED (Linux documentation):** Linux user namespaces scope Linux credentials/capabilities and permit namespace-specific ID mappings; they do not define Windows package SIDs or Windows ACL evaluation. Linux seccomp filters system calls, and Landlock restricts specified filesystem/network operations according to its ABI; these mechanisms do not themselves implement Windows access tokens, capability SID grants, or guest object-manager namespace authorization. [L1–L3]

## Architecture implications (HYPOTHESES, not measured facts)

1. Maintain **guest token identity** (user/group/package/capability SIDs, integrity and relevant policy) in the NT-compatible security authority. Host UID/GID and namespaces may *enforce* a conservative subset, but cannot serve as the sole guest identity representation.
2. Model AppContainer profile creation, existence, naming, per-user lifetime and directory/registry redirection separately from process launch. A deterministic package SID must not be treated as proof that a profile exists.
3. Authorize every guest object-manager and brokered resource operation against the guest token and requested access. Host syscall confinement should be a second boundary, not the only gate; especially avoid an unmediated inherited host descriptor bypassing guest capabilities.
4. For least privilege, assess user/mount/network namespaces, Landlock, seccomp and descriptor filtering *as defense-in-depth*. Their availability and policies are host/kernel/configuration dependent; do not promise one-to-one Windows semantics.
5. The documented `NtCreateLowBoxToken` handle-array argument suggests a specific named-object-directory bootstrap contract; investigate exact handle ownership and rights on Windows before implementing it.

## Proposed falsifiable gates (NOT executed)

- **Windows reference:** Create an AppContainer profile, repeat creation (already exists), derive its SID, launch using `PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES`, and record token package/capability identity and environment paths; test profile cleanup and cross-user behavior.
- **Token/access:** Compare access to ordinary user-granted objects, package-SID-granted objects and capability-SID-granted objects; vary impersonation, inherited handles and explicit grants. Record API, build, access mask and exact error.
- **LowBox:** Under an authorized Windows test account, exercise documented `NtCreateLowBoxToken` preconditions and named-directory handles, recording NTSTATUS and handle lifetime. Do not infer behavior for unsupported undocumented combinations.
- **Linux backend:** Test user namespace availability, Landlock ABI, seccomp filters, namespace/descriptor escape paths and broker authorization in isolated processes. Separately prove that guest authorization rejects operations even where a host fd remains valid.
- **Security:** Negative tests for profile SID spoofing, capability forgery, cross-package directory collisions, token impersonation, stale/recycled handles, host-path traversal and cross-process fd transfer.
- **Decision gate:** Do not select a Linux isolation backend until a guest token/ACL reference matrix, threat model, feature-detection matrix and failure/rollback behavior are specified.

## Open questions

- Windows build/version differences in package SID, capability SID, LPAC policy and token query APIs.
- Which guest resources require package/capability access checks versus a broker-mediated explicit grant?
- Whether Linux confinement can remain per-guest-process without breaking NT shared handles and IPC.
- What AppContainer/LPAC functionality is needed for an initial desktop milestone, and what must be explicitly unsupported?

## Primary sources

- [M1] Microsoft, *Launch an AppContainer*: https://learn.microsoft.com/en-us/windows/win32/secauthz/implementing-an-appcontainer (accessed 2026-11-21).
- [M2] Microsoft, *CreateAppContainerProfile*: https://learn.microsoft.com/en-us/windows/win32/api/userenv/nf-userenv-createappcontainerprofile (accessed 2026-11-21).
- [M3] Microsoft, *NtCreateLowBoxToken*: https://learn.microsoft.com/en-us/windows/win32/secauthz/ntcreatelowboxtoken (accessed 2026-11-21).
- [L1] Linux man-pages, *user_namespaces(7)*: https://man7.org/linux/man-pages/man7/user_namespaces.7.html (reference URL; version-specific behavior requires verification).
- [L2] Linux man-pages, *seccomp(2)*: https://man7.org/linux/man-pages/man2/seccomp.2.html (reference URL; version-specific behavior requires verification).
- [L3] Linux kernel documentation, *Landlock*: https://docs.kernel.org/userspace-api/landlock.html (reference URL; ABI-specific behavior requires verification).

Source caution: The Microsoft Q&A discussion about unregistered, derived-only LPAC SIDs is **not** used as a normative API contract. This research does not assert such a mode is supported.
