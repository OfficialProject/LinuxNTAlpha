# R92 — NT discretionary access checks and Linux credential boundary

Status: RESEARCH ONLY (2026-11-15). No implementation, Windows reference run, or Linux experiment is claimed.

## Scope and relation to existing research

R81 studies thread impersonation versus Linux credentials; R87 studies NT object naming, handles and lifetime. This note isolates the *authorization decision* made when an NT object handle is opened, and the difference between a granted handle mask and later use of that handle. It is not a general Windows security implementation plan.

## Verified primary-source contracts

1. Microsoft [MS-DTYP §2.5.3.2](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/4f1bbcbb-814a-4c70-a11e-2a5b8779a6f9) specifies ordered DACL evaluation: matching allow ACEs satisfy requested rights, matching deny ACEs reject rights still requested, and remaining unsatisfied rights reject the request. **NULL/absent DACL** permits access; **present empty DACL** denies ordinary requests (subject to documented owner implications). These cases must not be conflated.
2. Microsoft's [ACE ordering guidance](https://learn.microsoft.com/en-us/windows/win32/secauthz/order-of-aces-in-a-dacl) explicitly says order can change the outcome; canonical deny-before-allow ordering is a recommendation, not permission to reorder arbitrary supplied ACLs before evaluating them.
3. Microsoft's [AccessCheck contract](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-accesscheck) requires generic rights to be mapped before the check, accepts MAXIMUM_ALLOWED with a different granted-mask result, uses an impersonation token with TOKEN_QUERY, and reports both API success/failure and access status. A successful API invocation is **not** the same as authorized access.
4. [MS-DTYP §2.5.3.2](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/4f1bbcbb-814a-4c70-a11e-2a5b8779a6f9) documents a distinct MAXIMUM_ALLOWED evaluation path; an implementation cannot treat it as an ordinary single requested-right bit.
5. Microsoft's [AccessCheckByTypeResultList](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-accesscheckbytyperesultlist) documents object-type-specific ACE evaluation and multiple access results for an object/property hierarchy. This is a separate feature gate from a basic NT kernel-object DACL check; do not claim it is needed for the first PE milestone.

## Architecture implications — hypotheses, not measurements

**HYPOTHESIS A:** An NT object broker can maintain guest security descriptors, token SID/privilege state and object-type generic mappings independently of Linux UID/GID. Linux credentials remain an additional host-enforcement boundary. Merely translating a guest SID to a Linux UID cannot reproduce ordered ACE semantics, guest granted-access masks or MAXIMUM_ALLOWED.

**HYPOTHESIS B:** At successful handle creation, store a stable granted-access mask with the guest handle-table entry and check required rights on subsequent operations. Do not silently re-run a mutable DACL check for every use or infer guest rights from a host file descriptor. Whether particular object operations require extra privilege or state checks remains object-specific and OPEN.

**SECURITY REQUIREMENT (proposed):** Guest authorization must never enlarge host authority. A broker opening Linux resources on behalf of guest objects needs a separately reviewed host privilege model; a guest access-allowed decision is not permission to bypass Linux filesystem or namespace restrictions.

## Falsifiable gates, not executed

- **DACL order:** Same token and rights, allow-then-deny versus deny-then-allow for the same SID; record actual granted masks and status on a pinned Windows build. Do not normalize the input ACL.
- **Absent versus empty DACL:** Construct valid descriptors with owner/group SIDs, compare absent/NULL and zero-ACE present DACL. Include owner access and explicitly record its effect.
- **Generic mapping:** For each object type under test, compare GENERIC_READ/WRITE/ALL with the equivalent mapped object-specific rights; ensure no unmapped generic bits reach AccessCheck.
- **MAXIMUM_ALLOWED:** Request it with overlapping allow and deny ACEs; compare granted masks rather than only Boolean success.
- **Token selection:** Compare primary token versus a thread impersonation token with different SID memberships, using the documented impersonation-token AccessCheck contract. Include an impersonation-level validity case.
- **Handle lifetime:** Open with a granted mask, change the DACL, then exercise that existing handle and attempt a new open. Separate guest handle rights from host fd permissions.
- **Host isolation:** Attempt guest-allowed access to a host-denied resource; verify no privilege escalation through broker or fd passing.

Record Windows version/build, token attributes, descriptor bytes, ACE order, desired mask, mapped mask, granted mask, status, and host kernel/credentials. No results are currently available.

## Open questions

- Exact object-type-specific generic mappings and privilege overrides for the first supported NT object types.
- Restricted-token, deny-only SID, mandatory integrity and owner-rights behavior; each requires a separately scoped primary-source audit and Windows differential cases.
- How to enforce a guest-authorized file operation without granting the runtime broad host filesystem authority.
- Which security descriptors belong to guest object-manager objects versus host-backed files, and how to persist them safely.

## Decision gate

Before exposing guest object opens across processes, implement and test a pure, deterministic ACL evaluation core against Windows-reference cases; keep host authorization independent. This is a proposed gate, not evidence that the architecture is feasible or secure.

Sources reviewed 2026-11-15: Microsoft MS-DTYP access check pseudocode, Microsoft AccessCheck API, Microsoft ACE ordering, Microsoft AccessCheckByTypeResultList (URLs above).
