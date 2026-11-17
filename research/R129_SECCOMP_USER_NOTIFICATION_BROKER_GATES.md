# R129 — seccomp user notification as an NT syscall broker: limits and decision gates

Status: **primary-source documentation analysis; no experiment**, 2026-11-16. Scope: Linux x86-64 host, direct PE guest execution. This note evaluates a specific alternative to the R90 SUD compatibility path; it does **not** claim a working NT broker or sandbox.

## Why this question matters

R26 traps one Windows-origin syscall with Syscall User Dispatch (SUD); R90 separates syscall provenance from security containment. A proposed alternative is Linux seccomp user notification: filter a syscall, send a notification to a supervisor, and let that supervisor emulate or selectively continue it. Is this an appropriate mechanism for NT service dispatch, and can it protect the host?

## VERIFIED — upstream Linux contracts

1. A seccomp filter may return `SECCOMP_RET_USER_NOTIF`. A listener obtained with `SECCOMP_FILTER_FLAG_NEW_LISTENER` receives requests through `SECCOMP_IOCTL_NOTIF_RECV` and sends responses through `SECCOMP_IOCTL_NOTIF_SEND`. The filter sees Linux syscall number, architecture and arguments, not an NT service name or authenticated PE origin. [seccomp_unotify(2)](https://man7.org/linux/man-pages/man2/seccomp_unotify.2.html); [seccomp(2)](https://man7.org/linux/man-pages/man2/seccomp.2.html); [Linux kernel seccomp filter documentation](https://docs.kernel.org/userspace-api/seccomp_filter.html).
2. The Linux seccomp user-notification manual explicitly warns that the facility is **not intended as a security-policy enforcement mechanism**. Notification-time argument inspection is vulnerable to races with concurrent changes in tracee memory. It is principally useful for privileged supervision/emulation of selected calls. [seccomp_unotify(2)](https://man7.org/linux/man-pages/man2/seccomp_unotify.2.html).
3. The notification ID must be validated when acting on delayed requests; the API provides `SECCOMP_IOCTL_NOTIF_ID_VALID`. The listener must handle cancellation, tracee exit and stale notifications rather than treating the ID as permanently valid. [seccomp_unotify(2)](https://man7.org/linux/man-pages/man2/seccomp_unotify.2.html); [Linux seccomp user notification documentation](https://docs.kernel.org/userspace-api/seccomp_filter.html).
4. `SECCOMP_USER_NOTIF_FLAG_CONTINUE` asks the kernel to execute the intercepted Linux syscall. Kernel documentation warns that continuing after checking mutable tracee arguments creates time-of-check/time-of-use hazards. For an NT number that collides with a Linux number, continuing is especially inappropriate: it may execute an unrelated Linux operation. [seccomp_unotify(2)](https://man7.org/linux/man-pages/man2/seccomp_unotify.2.html).
5. A seccomp filter is attached to tasks, with thread-synchronization behavior controlled by installation flags; filters are inherited across `fork`/`clone` and persist across `execve`. Installation requires the documented privilege or `no_new_privs` prerequisites. Seccomp cannot be assumed to apply only to PE instruction addresses. [seccomp(2)](https://man7.org/linux/man-pages/man2/seccomp.2.html); [Linux seccomp filter documentation](https://docs.kernel.org/userspace-api/seccomp_filter.html).
6. Seccomp user notification can provide a supervised mechanism for selected syscall emulation, but it does not supply Windows TEB/PEB, the PE loader, Windows ABI adaptation, handle/object semantics, APCs, exceptions, or NTSTATUS conversion. Those remain LinuxNTAlpha's own compatibility obligations (see R11–R15, R19–R28 and R115).

## HYPOTHESIS — architecture consequences

**H1: A blanket notification filter will intercept trusted host runtime calls too.** Seccomp sees host Linux syscall entries, not a reliable guest/host trust label. An in-process runtime needs an independently justified route for its own syscalls. Allowlisting by syscall number alone cannot separate NT and Linux calls when numbers collide.

**H2: A separate supervisor process may simplify privilege separation but increases complexity.** It needs a protocol for request identity, guest memory access, object-handle rights, cancellation and process death. A successful notification round-trip is not evidence of correct NT dispatch, nor is it a general replacement for in-process imported NTDLL functions.

**H3: The notification listener is a liveness dependency.** Supervisor crashes, stalled replies and concurrent guest termination need explicit failure behavior. A compatibility design should fail closed on unknown NT services and should not accidentally fall through to the Linux syscall table.

**H4: User notification should be considered a *targeted host syscall mediation* tool, not the default NT call ABI.** The existing PE import-to-MS-ABI host-stub path (R20/R28) is more direct for exported functions. Raw guest syscalls still require explicit provenance and number mapping (R26/R90/R128). A broker may be appropriate for a separately isolated guest, but only after threat modeling and cost measurements.

## Proposed experiments — NOT RUN

1. **Capability probe:** in a disposable child, record kernel release, `SECCOMP_GET_ACTION_AVAIL` result, listener installation outcome, privilege/no_new_privs state and per-thread filter behavior. Distinguish SKIP (unavailable) from FAIL.
2. **Collision test:** issue a synthetic guest-origin syscall whose number equals a harmless Linux syscall, while a trusted host thread uses that Linux syscall. Verify whether the filter can actually separate the two without relying on attacker-controlled data. An inability to distinguish them rejects a blanket dispatch architecture.
3. **Broker lifecycle:** deliberately exit the supervisor, terminate a tracee while notification is pending, and reuse memory backing a pointer argument while notification is outstanding. Validate IDs and record exact kernel responses. Use isolated children and timeouts.
4. **Continuation race:** compare explicit emulation with `CONTINUE` under concurrent mutation of a pointer argument. Never treat a pre-continuation argument check as an authorization proof.
5. **Cost comparison:** after correctness, measure median and tail latency for (a) direct imported host stub, (b) R26-style SUD interception and (c) seccomp notification with an external broker. Pin versions and environment, include throughput and supervisor CPU. No performance result is asserted here.

## Decision

**OPEN:** Do not choose seccomp user notification as the universal NT syscall translation mechanism on documentation alone. It cannot authenticate guest provenance by itself, and the upstream manual explicitly disclaims security-policy enforcement. Retain it as a research candidate for narrow supervised host operations; require the collision, lifecycle, security and latency gates above before any architecture commitment.

**Nonduplication:** R90 establishes the general provenance/security distinction; R129 evaluates the concrete seccomp user-notification listener protocol, continuation hazards and broker-liveness tradeoffs. No code or experiment was added.
