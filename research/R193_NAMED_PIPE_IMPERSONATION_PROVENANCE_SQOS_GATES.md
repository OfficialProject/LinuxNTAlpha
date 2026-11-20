# R193 — Named-pipe impersonation: message provenance and SQOS gates

**Research date:** 2026-11-19. **Status:** VERIFIED for cited Microsoft API contracts; HYPOTHESIS for proposed broker model; OPEN for Windows differential tests. No code executed, Windows reference trace collected, or performance measurement made.

## Distinct scope

R37/R78 cover named-pipe transport; R81 covers guest thread tokens versus Linux host credentials; R192 covers peek and transactions. This note addresses a different coupling: **which client security context is eligible for a server's impersonation after a particular read**, and how the client's security quality-of-service (SQOS) choice constrains it. Transport peer UID alone is insufficient evidence of the Windows client impersonation contract.

## VERIFIED — primary Microsoft contracts

1. `ImpersonateNamedPipeClient` is callable only by the **server end**. Microsoft says the server thread impersonates the security context of the **last message read from the pipe**, rather than simply the identity of whichever process currently owns the connected socket. On failure the client is not impersonated; Microsoft warns that subsequent requests then run in the server process security context. This is a potentially dangerous fail-open *application pattern*, not authorization for our runtime to silently proceed. [M1]
2. The client can use `SECURITY_SQOS_PRESENT` when opening the pipe through `CreateFile` to control the server's impersonation level. The documented levels are Anonymous (cannot identify/impersonate), Identification (identify, not impersonate), Impersonation (local impersonation), and Delegation (remote impersonation). Microsoft documents SecurityImpersonation as the default for named-pipe servers, and notes a special rule for remote connections: client `CreateFile` flags for the level are ignored and the server's account configuration determines the remote level. Do not extrapolate local behavior to remote pipes. [M2][M3]
3. `RevertToSelf` ends client impersonation. Microsoft warns that a failed revert leaves the process operating in the client's context and recommends shutting down rather than continuing. [M4]
4. Named-pipe security descriptors govern access to server and client ends. Microsoft documents an access check when a thread calls `CreateNamedPipe` to open the server end of an existing pipe. Authorization at connection/open is **not** equivalent to permission to impersonate at a particular level. [M5]

## HYPOTHESIS — provenance-preserving guest pipe broker

For each guest pipe instance, keep an immutable connection generation and negotiated guest SQOS; associate read-visible message/byte-range provenance with the guest client's token identity at the relevant operation boundary. Only a successful eligible **server read** may update the server endpoint's last-read impersonation candidate; `PeekNamedPipe` must not be presumed to update it. An impersonation request must atomically validate server endpoint identity, candidate lifetime, and SQOS level, then install a **guest thread** token. It must never call host `seteuid` to grant access or assume `SO_PEERCRED` is a Windows token.

This model is a candidate, **not** a verified description of the internal Windows NPFS implementation. In particular, Microsoft documentation's phrase “last message read” does not settle byte-mode partial-read attribution, failed/zero-byte reads, or cross-thread read races. A host AF_UNIX socket can supply transport but cannot independently enforce the guest ACL, impersonation level, token privileges, or read provenance. Keep host isolation separate from guest access checks.

## OPEN — falsifiable Windows reference matrix

| Test | Required observation |
|---|---|
| Before any read; after `PeekNamedPipe` only | Impersonation result, error, thread token state |
| After successful server read | Token identity, impersonation level, `OpenThreadToken` outcome |
| Anonymous and Identification SQOS | Exact impersonation result and identity-query capability; do not assume identical error codes |
| Impersonation and Delegation SQOS | Local outcomes and limits; remote path separately scoped |
| Two server threads, one pipe | Which read establishes candidate; whether both can impersonate |
| Partial byte reads, short message reads, zero-byte reads | Whether and when last-read candidate changes |
| Client disconnect/reconnect, server handle duplication | Candidate lifetime and stale generation rejection |
| Failed impersonation followed by privileged operation | Ensure explicit error path does not use server privileges as though client-authorized |
| Successful impersonation then `RevertToSelf` | Guest thread token transitions and failure handling |

Record Windows build, client/server token identities, local versus remote connection, explicit `CreateFile` SQOS flags, pipe type/read mode, exact bytes/read status, `GetLastError`, `OpenThreadToken` results, and independent server thread IDs. For Linux candidate testing, separately attempt direct host syscalls and inherited-descriptor access under a denied guest token: passing guest ACL tests alone does **not** establish containment.

## Architecture gate

Do not equate named-pipe connection authentication with per-read impersonation authorization. Preserve explicit client SQOS and read provenance in the design until differential evidence can simplify the model. No claim of NT impersonation compatibility or safe execution of untrusted PE binaries is made.

## Primary sources (inspected 2026-11-19)

- [M1] Microsoft, ImpersonateNamedPipeClient: https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-impersonatenamedpipeclient
- [M2] Microsoft, Impersonating a Named Pipe Client: https://learn.microsoft.com/en-us/windows/win32/ipc/impersonating-a-named-pipe-client
- [M3] Microsoft, Impersonation Levels: https://learn.microsoft.com/en-us/windows/win32/secauthz/impersonation-levels
- [M4] Microsoft, RevertToSelf: https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-reverttoself
- [M5] Microsoft, Named Pipe Security and Access Rights: https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights

No source code copied. No experiments performed.
