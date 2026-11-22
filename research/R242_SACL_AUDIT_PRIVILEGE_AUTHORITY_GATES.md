# R242 — SACL audit authority is not DACL authorization or Linux filesystem auditing

Date: 2026-11-22. Status: **primary-documentation research only; NOT TESTED**.

## Distinct question

Existing [R14](R14_SECURITY_TOKENS_ACL.md), [R92](R92_NT_DACL_ACCESS_CHECK_HOST_BOUNDARY.md), [R131](R131_RESTRICTED_TOKENS_MANDATORY_INTEGRITY_GATES.md), and [R224](R224_OBJECT_TYPE_GENERIC_ACCESS_MAPPING_GATES.md) cover token, DACL and generic-rights gates. This note isolates a separate security boundary: reading or changing an NT object's **system access control list (SACL)**, which is neither ordinary DACL authorization nor automatically equivalent to a Linux filesystem audit rule.

## VERIFIED FROM SOURCE (Microsoft documentation; no executable validation)

1. Microsoft's *Access control lists* distinguishes DACLs (allow/deny authorization) from SACLs (auditing of specified success and/or failure access attempts). It explicitly distinguishes an **absent DACL** (full access) from a **present empty DACL** (denies all access). Source: https://learn.microsoft.com/en-us/windows/win32/secauthz/access-control-lists (inspected 2026-11-22; page last updated 2025-07-10).
2. Microsoft's *SACL Access Right* specifies that `ACCESS_SYSTEM_SECURITY` controls getting/setting the SACL; `SE_SECURITY_NAME` must be **enabled in the requesting thread's access token** for this right. It says the right is **not valid in a DACL** and documents the privilege + handle-access workflow. Source: https://learn.microsoft.com/en-us/windows/win32/secauthz/sacl-access-right (inspected 2026-11-22; page last updated 2021-01-07).
3. The same SACL page distinguishes `GetSecurityInfo` / `SetSecurityInfo` on an opened handle from `GetNamedSecurityInfo` / `SetNamedSecurityInfo`, which internally request the access right when the privilege is enabled. This is an observable access-path difference, not proof that LinuxNTAlpha implements either path.

These are **documentation-level verified statements**, not pinned Windows build behavior or an audit of Linux kernel source. No claim is made that all object classes share identical SACL storage or event generation.

## HYPOTHESIS — NT authority boundary

Maintain guest security descriptors (including SACLs), token privileges, and the decision to generate guest audit records in a single trusted NT object/security authority. Linux uid/gid/mode, POSIX ACLs, and Linux audit facilities may support host containment or telemetry but must not silently replace NT SACL semantics. In particular:

- A caller with DACL-granted ordinary object access but without enabled `SeSecurityPrivilege` must not acquire SACL read/write authority merely because a Linux fd is readable or writable.
- A caller with the relevant enabled guest privilege must still use the documented security-information access path and object-specific validation; guest privilege does **not** imply Linux root or `CAP_AUDIT_CONTROL`.
- Guest audit events, if implemented, must carry guest object/token/request identity and successful versus failed authorization outcome; host audit events alone may have different identities and triggers.
- A failed or unavailable guest audit sink must have a specified behavior rather than silently implying that an audit event was durably recorded.

This is a candidate boundary, **not** a demonstrated design or a proposal to expose privileged Linux audit controls to guest programs.

## OPEN — Windows differential acceptance matrix (NOT RUN)

| Case | Required evidence |
| --- | --- |
| Null DACL vs present empty DACL | Separate access results, with SACL unchanged |
| Ordinary DACL read vs SACL read | `READ_CONTROL`-only handle cannot be treated as sufficient for SACL retrieval |
| Privilege present but disabled vs enabled | Observe `ACCESS_SYSTEM_SECURITY` open and SACL get/set results with exact token state |
| Thread impersonation token vs process primary token | Confirm which effective token supplies the privilege and when |
| Success-only vs failure-only audit ACE | Observe security event generation with audit policy and privileges explicitly configured |
| Handle-based vs named-object SACL APIs | Compare rights/privilege enforcement, object identity and errors |
| Cross-process duplicated handle | Verify granted access mask and privilege checks are not incorrectly recomputed or bypassed |
| Guest object teardown and audit queue failure | Define event ownership, ordering, and failure policy without inventing Windows outcomes |

Use an isolated Windows test VM with auditing policy and event-log access configured, record Windows build, token privilege attributes, DACL/SACL contents, exact desired-access masks, API return codes, `GetLastError`, and observed security events. A Linux prototype must independently record host kernel, namespace, capabilities and audit configuration. **No tests, event traces, or performance measurements were produced in this run.**

## Architecture consequence

The DACL decision engine and SACL/audit authority are related but not interchangeable. An implementation that maps NT SACL operations directly onto host file permissions or lets a guest control host audit configuration crosses an unjustified privilege boundary. Keep this as an explicit conformance and isolation gate under ADR-003; no kernel-vs-userspace decision follows from the cited documentation.

## Source and evidence limitations

Only the two Microsoft pages above were freshly inspected on 2026-11-22. The documentation is not pinned to a Windows binary revision; actual Windows build differences, event-log audit-policy requirements, privilege behavior across object classes, and Linux host backend options remain OPEN. No source code was copied.
