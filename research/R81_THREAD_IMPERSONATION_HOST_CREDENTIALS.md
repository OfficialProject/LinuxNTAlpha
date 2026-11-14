# R81 — Thread impersonation tokens versus Linux credentials

Research date: 2026-11-14. Status: **documentation analysis only; no code or Windows conformance experiment executed**.

## Scope and distinction

R14 surveys security tokens/ACLs; R37/R78 cover named-pipe transport. This note isolates a distinct architectural boundary: **which security principal is used when a server thread performs an access check while impersonating a client**, and why simply changing the Linux host UID is not a faithful implementation.

## VERIFIED — documented contracts

1. Microsoft documents `RevertToSelf` as terminating client impersonation and lists `ImpersonateNamedPipeClient`, `ImpersonateLoggedOnUser`, `ImpersonateSelf`, and `SetThreadToken` among operations whose impersonation should be ended with `RevertToSelf`. Microsoft explicitly warns that failure to revert leaves the application running in the client's security context; a server must treat this as a security-critical failure, not a cosmetic API error. [MS RevertToSelf](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-reverttoself) (accessed 2026-11-14).
2. Linux maintains real/effective/saved and filesystem user/group IDs, supplementary groups, and capabilities with different purposes; filesystem IDs participate in filesystem access decisions. [Linux credentials(7)](https://man7.org/linux/man-pages/man7/credentials.7.html) (man-pages 6.19, accessed 2026-11-14).
3. Linux kernel credentials are technically per-thread, but the NPTL POSIX-thread implementation coordinates changes to process credentials across threads. Therefore, a naïve call to `seteuid` or `setfsuid` cannot be assumed to model an isolated Windows guest-thread impersonation state with correct Linux library behavior. [Linux credentials(7)](https://man7.org/linux/man-pages/man7/credentials.7.html), especially NOTES and user/group-ID sections.

## HYPOTHESIS — candidate runtime boundary

Maintain **guest process primary token** and **optional guest thread impersonation token** as independent runtime objects, with effective-token selection at each guest object-manager/ACL authorization operation. Guest handles carry granted-access state established at open/duplicate time; avoid rechecking merely because the calling thread later changes token. The host process remains subject to Linux credentials and sandbox policy independently. Guest impersonation must **not** grant additional host privileges.

This is an architectural proposal, **not** a claim that Linux user-space mediation is sufficient for untrusted Windows code. The current PE/SUD experiments do not establish containment of direct Linux syscalls, host file-descriptor access, shared mappings, or malicious guest control flow. A security boundary must be independently proven before running untrusted PE images.

## OPEN — conformance questions requiring Windows reference measurements

- On an impersonating server thread, test access to an object denied to the server primary token but permitted to the client token, and the inverse. Record exact requested access masks, impersonation level, privilege state, and observed errors.
- Test `OpenThreadToken` before, during and after impersonation, and whether explicit `SetThreadToken(NULL, ...)` / `RevertToSelf` changes only the current guest thread's effective security context.
- Test nested or repeated impersonation/revert sequences without presuming an implicit token stack; document actual Windows behavior.
- Test named-pipe client impersonation with different client impersonation levels, including denied impersonation; avoid assuming transport peer credentials equal an authorized guest token.
- Test handle granted-access persistence after token changes and duplicate/open operations under different tokens.
- In a separate hostile Linux harness, attempt direct host syscalls and inherited-fd access while guest token policy denies access. Expected *security acceptance*: neither bypass is possible; **not yet demonstrated**.

## Decision gate

**Do not** select host UID switching as the sole implementation of NT impersonation. Require a guest token/ACL authorization design and a separate, tested host containment model. Keep guest authorization semantics and host enforcement distinct in diagrams, code reviews and benchmarks. No Windows security guarantee is claimed.

## Sources

- Microsoft, `RevertToSelf` Win32 API contract, last-updated 2024-02-22: https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-reverttoself
- Linux man-pages project, `credentials(7)`, man-pages 6.19 (2026-07-02): https://man7.org/linux/man-pages/man7/credentials.7.html

No source code copied; no experiment performed.
