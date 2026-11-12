# R14 — NT security tokens, access checks, and Linux credentials

Date: 2026-11-12. Status: architecture research, not implemented or tested.

## Primary documentation
- https://learn.microsoft.com/en-us/windows/win32/secauthz/access-tokens
- https://learn.microsoft.com/en-us/windows/win32/secauthz/access-control-model
- https://learn.microsoft.com/en-us/windows/win32/secauthz/how-dacls-control-access-to-an-object
- https://learn.microsoft.com/en-us/windows/win32/secauthz/ace-inheritance-rules
- https://learn.microsoft.com/en-us/windows/win32/secauthz/mandatory-integrity-control
- https://learn.microsoft.com/en-us/windows/win32/secauthz/privileges
- https://man7.org/linux/man-pages/man7/credentials.7.html
- https://man7.org/linux/man-pages/man7/capabilities.7.html

## Documented Windows model
Windows access tokens carry user and group SIDs, privileges and other security attributes; processes have primary tokens and threads may impersonate. A securable object's security descriptor can include owner/group, DACL and SACL. DACL evaluation uses ordered access-control entries and requested access masks; a null DACL and an empty DACL have materially different meaning. Integrity labels constrain certain operations even when discretionary access might otherwise permit them. Privileges are separate from ordinary DACL-granted access.

## Critical compatibility gaps
Linux UIDs, GIDs, supplementary groups and capabilities are not a drop-in Windows token representation. Linux DAC and ACL semantics cannot be assumed to match SID-based access checks, Windows privilege evaluation, deny ACE ordering, inheritance or integrity levels. Windows impersonation may change the effective access context of one thread without changing the whole process identity. Mapping every Windows principal to the same Linux UID would silently defeat isolation between NT users.

## Candidate design (HYPOTHESIS)
- Represent Windows identities and token objects in a trusted broker with stable SID mappings and explicit impersonation scopes.
- Apply Windows access checks before granting compatibility-layer object handles; store granted-access masks on handles.
- Constrain backing Linux resources with host OS sandboxing and least privilege as defense in depth. Broker checks must not be bypassable through direct host path/fd access.
- Treat token duplication, restricted tokens, privilege enabling, inherited ACEs, generic-rights mapping, and mandatory integrity as distinct conformance milestones.

## Test gates
1. Empty DACL vs null DACL vs absent DACL, and allow/deny ACE order.
2. Generic access rights mapped to object-specific rights, MAXIMUM_ALLOWED, owner rights and privilege overrides.
3. Thread impersonation and revert, including concurrent threads.
4. Inherited ACEs on created objects and explicit protection of a child DACL.
5. Cross-process handle duplication and access rights attenuation.
6. Integrity-level write restrictions and restricted tokens.
7. Host escape attempts bypassing broker, forged handles and symlink/race attacks.

## Security decision
Security cannot be deferred until after application compatibility. A prototype may use a deliberately restricted single-user sandbox, but must label unsupported access semantics explicitly and never present itself as safe multiuser NT isolation.
