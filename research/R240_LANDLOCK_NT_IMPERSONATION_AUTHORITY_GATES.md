# R240 — Landlock confinement versus NT thread-token impersonation

Date: 2026-11-22. Status: **primary-documentation review; no experiment**. This is a host-security boundary note, not an assertion of NT compatibility or an architecture decision.

## Distinct question

R81 and R92 cover guest impersonation and DACL access checks; R223 covers AppContainer isolation; R232–R235 cover host supervisor trust and architecture gates. This note asks a narrower question: **can a Landlock-confined Linux worker safely implement dynamic NT thread impersonation and host file access, or must guest authorization remain an independent broker decision?** It addresses host *confinement* rather than attempting to restate NT token semantics.

## VERIFIED FROM SOURCE — external interface contracts

1. Microsoft documents that a process has a primary token; an impersonating thread also has an impersonation token, and access checks during impersonation use the latter. Reverting ends that use. Source: [Microsoft, Impersonation](https://learn.microsoft.com/en-us/windows/desktop/com/impersonation), accessed 2026-11-22. This does not specify a Linux implementation.
2. The versioned [Linux 6.17 Landlock userspace documentation](https://docs.kernel.org/6.17/userspace-api/landlock.html) specifies rulesets with handled filesystem rights and path-beneath rules. Rights not included in handled_access_fs are generally not denied by that ruleset, with the documented special case of REFER. Files opened before sandboxing are not subject to those restrictions. ABI-version-specific rights include REFER (ABI 2), TRUNCATE (ABI 3), and later device ioctl controls (ABI 5). These are host restrictions, not Windows ACLs.
3. The [Linux 6.9 Landlock LSM documentation](https://docs.kernel.org/6.9/security/landlock.html) describes open-time file-descriptor rights: the relevant Landlock rights remain attached when an fd is passed between processes. Therefore fd transfer is **not** a way to discard those already-attached restrictions. This does **not** establish that a receiver's guest NT access mask or token is validated.
4. Linux Landlock documentation explicitly lists file-related operations not fully restricted by its filesystem rules (including chmod/chown/setxattr and some other syscall families). Thus even an apparently narrow Landlock filesystem policy is not a complete general-purpose guest security monitor. The exact unsupported-operation list and ABI support are kernel-version-dependent; do not extrapolate across versions.

## HYPOTHESIS — layered authority, not token-to-Landlock equivalence

Use Landlock as a *monotonic host damage-reduction boundary* for an untrusted guest worker, not as the mutable NT security token or object DACL evaluator. A separate trusted service would evaluate guest process/thread effective token, object security descriptor, desired access, granted handle rights and impersonation lifetime **before** performing a host filesystem action. The service itself must enforce path/handle authority and avoid accepting raw guest pointers, unchecked fds, or host paths as authorization.

A Landlock domain is not a reversible per-request Windows impersonation switch. Treating a Linux thread's sandbox restrictions as the Windows thread's effective token risks both incorrect access decisions and confused-deputy bugs. The worker's permitted host resources should be a strict upper bound on all guest identities it serves, while each guest request is checked against its own NT authority. This is a candidate design, not a demonstrated implementation.

## OPEN — minimum falsifiable acceptance matrix (NOT RUN)

| Case | Expected invariant to test | Failure mode |
| --- | --- | --- |
| Guest thread A impersonates restricted identity while B remains primary-token | Same host worker must not leak A's guest authorization to B or vice versa | Thread-token cross-talk |
| Open guest file under impersonation, then revert | Existing guest handle retains documented granted rights; later opens use current effective token | Retroactive grant or inappropriate revocation |
| Transfer host fd to service | Landlock's attached fd rights still apply; service additionally checks guest handle capability | Confused-deputy access |
| Sandbox created after preopened host fd | Preopened fd is explicitly inventoried, denied or held by trusted service | Preopen bypass of intended policy |
| Guest rename across directory boundaries | Check NT path/share/security first; separately record Landlock REFER/MAKE/REMOVE outcomes | False equivalence between host errno and NTSTATUS |
| Unsupported Landlock right or old kernel ABI | Capability probe fails closed or uses documented restricted fallback | Silent allow due to unhandled access bit |
| Guest-supplied path with symlink/reparse escape | Service resolves against controlled namespace and authorization, not guest-provided host string | Host path escape |
| Host syscall not mediated by service | Independent host containment blocks forbidden actions | SUD mistaken for sandbox |

Windows-reference tests must record Windows build, token type, impersonation level, object ACL, handle desired/granted access and exact result. Linux tests must record kernel release, Landlock ABI query, ruleset rights, inherited/open fds, namespace setup and exact errno. **No such tests were performed in this note.**

## Architectural consequence

**OPEN:** Before implementing a guest impersonation API, specify which component owns NT access decisions and which process owns real host fds. Landlock can reduce the worker's host exposure but cannot substitute for NT tokens, DACLs, share-mode checks or handle lifetime. This is an additional concrete security gate for ADR-003, not evidence that any candidate has passed it.

## Evidence and limitations

Primary sources inspected on 2026-11-22: Linux kernel versioned documentation 6.17 and 6.9, Microsoft Impersonation documentation. These are published interface descriptions, **not pinned source-code audits**. No kernel build, fixture, sandbox, differential Windows run, performance measurement, or security penetration test was conducted. Future work must pin Linux source commits and confirm behavior on the actual execution kernel before calling this reproduced.
