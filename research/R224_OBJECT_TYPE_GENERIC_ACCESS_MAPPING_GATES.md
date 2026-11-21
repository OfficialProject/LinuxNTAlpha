# R224 — NT object-type generic access mapping and granted-handle rights

Research date: 2026-11-21. Evidence: **VERIFIED FROM PRIMARY MICROSOFT DOCUMENTATION**; design and tests below are **HYPOTHESES/PROPOSALS**. No Windows-reference test, Linux experiment, benchmark, or implementation was run.

## Question and nonduplication boundary

R14 covers tokens/ACLs, R67 cross-process handle duplication, R81 impersonation, R87 object lifetime and R223 AppContainer. This note isolates a prerequisite to all of them: a generic access bit does **not** grant a universal operation across NT object types. The requested mask must be translated through an object-type-specific mapping before access evaluation, and a handle's granted mask must remain separate from the request, object identity and Linux backing fd.

## VERIFIED — primary contracts

1. [Generic Access Rights](https://learn.microsoft.com/en-us/windows/win32/secauthz/generic-access-rights) states that each securable object type maps GENERIC_READ/WRITE/EXECUTE/ALL into different standard and object-specific rights. For example, generic file read includes FILE_READ_DATA, FILE_READ_EA, FILE_READ_ATTRIBUTES, READ_CONTROL and SYNCHRONIZE; registry-key generic read instead covers key-specific operations. **Consequence:** interpreting GENERIC_READ as a fixed broker-wide bit is incorrect.
2. [GENERIC_MAPPING](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-generic_mapping) specifies the four per-type expansion masks. [MapGenericMask](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-mapgenericmask) applies that mapping to a requested access mask. The API description does not establish the internal ordering of every NT create/open path.
3. [ACCESS_MASK](https://learn.microsoft.com/en-us/windows/win32/secauthz/access-mask) distinguishes bits 0–15 object-specific, 16–23 standard, bit 24 ACCESS_SYSTEM_SECURITY, bit 25 MAXIMUM_ALLOWED and bits 28–31 generic. MAXIMUM_ALLOWED is **not** a synonym for GENERIC_ALL; it requests calculation of the maximum rights allowed by the applicable security evaluation.
4. [AccessCheck](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-accesscheck) requires mapped desired access and accepts MAXIMUM_ALLOWED as a special case returning the maximum permitted granted mask. It evaluates a security descriptor and client impersonation token, not merely the existence of a Linux fd.
5. [ObReferenceObjectByHandle](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obreferenceobjectbyhandle) documents type-dependent desired access, rejection of generic bits at that kernel API boundary, optional expected-object-type validation, and a UserMode-versus-KernelMode access-mode distinction. This is driver-facing evidence, **not** a claim that a user-mode program may call ObReferenceObjectByHandle.
6. [IoGetFileObjectGenericMapping](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-iogetfileobjectgenericmapping) provides the kernel's file-object generic mapping. It is a useful reference for one object type, not evidence that the same mapping works for events, sections, tokens or registry keys.

## HYPOTHESIS — guest authority boundary

A candidate NT broker should maintain a versioned mapping table keyed by **guest object type** and a separate per-process handle entry containing an immutable granted-access mask. Proposed sequence: identify target type and guest caller token; normalize requested generic bits with that type's mapping; evaluate the security descriptor, privileges and MAXIMUM_ALLOWED semantics; publish the resulting granted mask with the handle; on subsequent operations, compare the operation's type-specific required rights against **that handle**, not against a fresh ambient host-UID check. Host permissions remain a second enforcement layer, never the source of NT rights.

**Critical unresolved cases:** whether a particular create/open API applies special access expansion or implicit rights; object-specific SYNCHRONIZE availability; owner and privilege overrides; mandatory integrity, restricted tokens, capability SIDs, protected-process policy and kernel-mode special cases. Do not assume a generic mapping plus DACL alone fully implements Windows authorization. Avoid copying mappings from undocumented reverse-engineered structures without revision/license checks.

## Proposed Windows differential matrix — NOT EXECUTED

| Probe | Observe |
| --- | --- |
| Create/open file, registry key, event, section and process using GENERIC_READ and then type-specific operations | Exact granted rights, successful/denied operations, status and object type |
| Request GENERIC_ALL versus MAXIMUM_ALLOWED under a restrictive DACL | Granted mask and success/failure differences |
| Request ACCESS_SYSTEM_SECURITY with/without the required enabled privilege | Status, granted mask and SACL access |
| Duplicate a reduced-rights handle, then change caller impersonation token | Previously granted rights versus attempted new rights; exact duplicate options |
| Reopen by name under a different token versus use an existing handle | New access check versus preserved existing handle authority |
| Pass a handle to a mismatched object-specific operation | Type rejection and status, not host-fd fallback |
| Recycle numeric handle slots after close under concurrent use | No stale handle gains rights to a new object |

Capture exact Windows build, process/token integrity, enabled privileges, DACL/SACL, object type, input desired mask, output granted mask where observable, NTSTATUS/Win32 error and every operation's outcome. A Win32 success alone does not reveal the kernel's entire granted mask.

## Proposed isolated model tests — NOT EXECUTED

Implement pure mapping and handle-table tests before host syscall mediation: (1) two object types with distinct GENERIC_READ expansions never cross-authorize; (2) MAXIMUM_ALLOWED is evaluated rather than bitwise expanded as GENERIC_ALL; (3) generic bits are eliminated before type-specific rights checks; (4) two handles to one object retain distinct granted masks; (5) impersonation affects a new open, not automatic enlargement of an old handle; (6) type mismatch and stale-generation handles fail closed; (7) all failures preserve object/handle reference counts. A model PASS would validate internal consistency, **not** Windows compatibility.

## Decision impact

This is an argument for keeping guest authorization state independent of Linux file descriptors and credentials, not evidence that a particular kernel/user-space split is optimal. A future fast path must preserve guest object type, per-handle granted rights, privilege checks and revocation/lifetime invariants before any speed claim.

## Source and verification limits

Microsoft Learn primary documentation reviewed 2026-11-21. No Windows build was tested and no upstream Windows kernel source was inspected. Specific Windows-version edge cases and actual LinuxNTAlpha implementation remain OPEN.
