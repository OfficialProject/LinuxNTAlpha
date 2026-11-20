# R — Mandatory integrity control as a separate NT access-check gate

Status: **VERIFIED FROM MICROSOFT PRIMARY DOCUMENTATION; DESIGN HYPOTHESES ONLY** (2026-11-20). No Windows reference execution, Linux experiment, benchmark, or implementation is claimed.

## Why this is a distinct gap

[R92](R92_NT_DACL_ACCESS_CHECK_HOST_BOUNDARY.md) covers discretionary ACE ordering, token selection, granted masks and the host credential boundary, but explicitly leaves mandatory integrity behavior open. A correct DACL evaluator alone cannot authorize an NT object open: mandatory integrity control (MIC) may restrict access **before** DACL evaluation. This note is limited to that missing access-check layer; it does not duplicate the DACL algorithm.

## Verified Microsoft contracts

1. Microsoft's [Mandatory Integrity Control](https://learn.microsoft.com/en-us/windows/win32/secauthz/mandatory-integrity-control) documentation states that MIC is evaluated before DACL checks. A lower-integrity caller can be prevented from writing a higher-integrity object even when the DACL permits writing. An unlabeled object is treated as medium integrity. The object's mandatory integrity label resides in a **SYSTEM_MANDATORY_LABEL_ACE in the SACL**; the principal's integrity level resides in the access token.
2. The [SYSTEM_MANDATORY_LABEL_ACE definition](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_mandatory_label_ace) defines policy bits **NO_WRITE_UP = 0x1**, **NO_READ_UP = 0x2**, **NO_EXECUTE_UP = 0x4**. These are policy flags, not ordinary file permissions or a DACL allow/deny ACE. Microsoft's [MS-DTYP §2.4.4.13](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/25fa6565-6cb0-46ab-a30a-016b32c4939a) additionally enumerates integrity SID RIDs: untrusted 0x0000, low 0x1000, medium 0x2000, high 0x3000, system 0x4000 and protected process 0x5000. The Win32 structure page's abbreviated RID table must not be mistaken for the complete protocol enumeration.
3. [MS-DTYP §2.5.3.3, MandatoryIntegrityCheck pseudocode](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtyp/ae69a089-473d-4c23-bf3d-7a12a9d11123) compares token and object integrity SIDs and derives an allowed-access set; it explicitly considers the token's mandatory policy, the ACE's INHERIT_ONLY flag, and SeRelabelPrivilege. A simple numeric comparison followed by unconditional 'deny all writes upward' is therefore not a complete transcription of the specified algorithm.
4. Microsoft's [Mandatory Integrity Control](https://learn.microsoft.com/en-us/windows/win32/secauthz/mandatory-integrity-control) page also states that a newly launched process receives the minimum of the user's and executable file's integrity levels, and identifies TOKEN_MANDATORY_POLICY as independently queryable through GetTokenInformation(TokenMandatoryPolicy). Process-token creation and object-open authorization are separate integration gates.

These are documented contracts, **not** observations of a particular Windows build. No claim is made here that every object type applies identical generic-right mappings or that these protocol pseudocode branches exhaust all modern Windows security restrictions.

## Proposed architecture — HYPOTHESIS, not implementation

Keep the following logically separate in a guest NT security service:

- Guest token: integrity SID, mandatory policy, SID/privilege attributes and impersonation identity.
- Guest object security descriptor: SACL mandatory label and flags, plus DACL evaluated through the R92 path.
- Object-specific generic-right mapping: map the requested rights before deriving the final granted mask, and preserve the actual object type.
- Host enforcement: Linux UID/GID, mount/namespace boundaries and broker authority remain independently restrictive. **Guest MIC is not a Linux sandbox.**

A proposed access-check pipeline is: validate token/descriptor and ACE structure; determine effective object label and mandatory policy; compute the MIC-constrained rights; evaluate the mapped DACL request within those constraints; store only the resulting guest granted mask on a successfully opened handle; finally enforce host resource permissions separately. The precise handling of MAXIMUM_ALLOWED, privileges, generic mappings and owner rights requires Windows-reference differential tests before treating this ordering as a faithful implementation.

## Falsifiable differential test matrix — NOT RUN

Record exact Windows build, object type, token integrity SID, token mandatory policy, object SACL bytes, DACL bytes, desired access, generic mapping, granted mask, and NTSTATUS or Win32 error.

1. Low token / medium-labeled object / DACL explicitly permits write: request read, write, execute separately with NO_WRITE_UP; verify that an allowed DACL write does not override MIC.
2. Same labels with NO_READ_UP and NO_EXECUTE_UP separately and in combination; ensure each requested access category is distinguished.
3. Equal integrity versus higher token integrity with identical DACLs; verify the label-comparison branch rather than assuming every high-integrity token has every discretionary right.
4. Missing mandatory label versus explicit medium label; test a low-integrity token and record whether the documented default holds for the chosen object type.
5. INHERIT_ONLY mandatory ACE, inherited child label, and token mandatory policy variants; verify behavior against the MS-DTYP pseudocode rather than inferring from the ordinary case.
6. MAXIMUM_ALLOWED with mixed MIC and DACL constraints; compare the **granted mask**, not only the open result.
7. Change the object's SACL after opening an existing handle; distinguish existing handle grants from a new open. Include an impersonating thread with a different integrity token.
8. Attempt an operation guest policy allows but host permissions deny; verify that the broker cannot turn guest authorization into expanded host authority.

**PASS gate:** a pure evaluator with deterministic cases must agree with recorded Windows-reference results for the supported object types and token policies, including negative cases. Unsupported combinations must fail explicitly, not silently bypass MIC. **SKIP** applies when no Windows reference environment is available.

## Open questions

- Which NT object types should the first milestone support, and their exact generic mappings and privilege interactions?
- How does the reference build handle mandatory policy variants, inherited labels and absent SACLs for each chosen object?
- Which process-creation path can preserve executable-label lowering and thread impersonation without conflating guest and host credentials?
- What trusted component owns guest security descriptor persistence and validates untrusted SACL bytes?

**Source review date:** 2026-11-20. Microsoft Learn pages above were consulted as live primary documentation; no Windows binaries or kernel sources were executed or pinned. This note does not change ADR-003's open kernel boundary.
