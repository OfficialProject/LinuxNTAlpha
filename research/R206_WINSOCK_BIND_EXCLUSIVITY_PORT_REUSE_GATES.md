# R206 — Winsock bind exclusivity versus Linux port reuse

Date: 2026-11-20. Status: **SOURCE REVIEW ONLY**; no Windows or Linux bind experiment executed.

## Architectural question

Can Windows `SO_REUSEADDR` and `SO_EXCLUSIVEADDRUSE` be mapped directly to Linux `SO_REUSEADDR` / `SO_REUSEPORT` without weakening port ownership or changing observable bind failures?

## VERIFIED FROM PRIMARY DOCUMENTATION

1. Microsoft distinguishes Windows `SO_REUSEADDR` (potentially forcing a second bind to an occupied port, with indeterminate packet ownership) from `SO_EXCLUSIVEADDRUSE` (preventing forced port takeover). Modern Windows bind rules vary with wildcard versus specific interface, same versus different account, IPv4 versus dual-stack IPv6, and option combinations. Microsoft's Windows Server 2003-and-later tables explicitly distinguish `WSAEACCES` from `WSAEADDRINUSE`. Source: [Microsoft, Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE](https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse) (living documentation; article indicates update 2022-06-13).
2. Microsoft documents that `SO_EXCLUSIVEADDRUSE` is set **before bind** and that the bound port can remain unavailable after closing a listening socket while accepted connections are active. This is a lifetime rule, not merely a one-time bind check. Source: [Microsoft, SO_EXCLUSIVEADDRUSE](https://learn.microsoft.com/en-us/windows/win32/winsock/so-exclusiveaddruse).
3. Linux `socket(7)` describes `SO_REUSEADDR` as relaxed local-address reuse, but specifically excludes binding over an active listening socket; wildcard listeners constrain other local-address binds. Linux `SO_REUSEPORT` allows multiple sockets to bind an identical address when **each** opts in before bind; the documented same-effective-UID restriction mitigates hijacking. These are not the same option contracts as Windows `SO_REUSEADDR`. Source: [Linux socket(7)](https://man7.org/linux/man-pages/man7/socket.7.html) (living manual, not pinned kernel source).

## HYPOTHESIS — implementation boundary

Do **not** equate guest `SO_REUSEADDR` with host `SO_REUSEPORT`, or guest exclusivity with simply leaving host reuse flags unset. A guest-facing port reservation/ownership layer may be required to preserve Windows' option-specific error outcomes, account-dependent rules, wildcard/specific conflicts, dual-stack interactions, and post-listener accepted-connection lifetime. The host kernel must still enforce a safe baseline against **non-guest host processes**: a guest-only reservation table cannot stop an unrelated host process from binding a host port. A global host-port coordination scheme would need a threat model, namespace policy, and explicit privileges; no such design is approved.

The strongest compatibility approach might instead intentionally expose a constrained subset and fail closed when Windows semantics cannot be honored. This is an **unverified candidate**, not evidence that complete Windows bind behavior can be implemented without a kernel change.

## OPEN — falsifiable gates (NOT RUN)

1. On a pinned Windows build, exercise TCP and UDP bind matrices: first/second socket default, reuse, exclusive; wildcard versus specific address; same versus different security principal; IPv4, IPv6-only, and dual-stack. Record each Winsock error and whether traffic reaches the intended socket.
2. Repeat the matrix on a pinned Linux kernel using default, `SO_REUSEADDR`, and `SO_REUSEPORT`, including different effective UIDs and network namespaces. Record errno and packet routing; do not infer Windows conformance from Linux success.
3. Keep accepted TCP connections alive after closing an exclusive listener; test rebind while connections are active, after orderly shutdown, and after reset. Include wildcard/specific collision cases.
4. Model concurrent guest and non-guest host bind attempts and TOCTOU races. Demonstrate whether proposed mediation actually prevents takeover rather than only tracking it.
5. Audit upstream Linux `inet_bind` / `inet6_bind` source at a specific release commit before implementing or claiming kernel-version-invariant behavior; documentation alone does not pin the algorithm.

## Decision impact

**OPEN:** Socket bind policy is a correctness **and security** boundary. It should not be treated as a mechanical socket-option translation. This note is separate from R204 (event notification) and R205 (socket duplication); it does not claim Windows or Linux runtime test results, port-hijack resistance, or a finished socket layer.

Sources reviewed 2026-11-20. No third-party code imported.
