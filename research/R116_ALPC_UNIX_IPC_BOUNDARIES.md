# R116 — ALPC port semantics versus Linux Unix-domain IPC

Research date: 2026-11-16. Status: **documentation/source review only**. No Windows differential test, Linux prototype, benchmark, or integration has been run.

## Scope and evidence limits

This note isolates the *native NT ALPC port* boundary from R37/R78 named pipes and R15/R66 IOCP. The Microsoft-published ALPC ETW provider explicitly distinguishes send, receive, wait-for-reply, wait-for-new-message, and stop-waiting events. This verifies that ALPC is a distinct Windows tracing surface; **ETW event names do not establish complete syscall semantics**. [M1]

The System Informer project's `phnt/include/ntlpcapi.h` is a publicly inspectable *third-party native-header reconstruction*, **not an official Microsoft stability contract**. It declares `NtAlpcCreatePort`, `NtAlpcConnectPort`, `NtAlpcSendWaitReceivePort`, port/message attributes and handle/security/view/context attribute structures. These declarations support investigation of candidate ABI surfaces, but structure details and flags must be checked against a pinned upstream revision and target Windows builds before implementation. [P1]

Linux `unix(7)` documents AF_UNIX ancillary descriptor transfer (`SCM_RIGHTS`), credential messages (`SCM_CREDENTIALS`), and peer credentials (`SO_PEERCRED`). It explicitly says peer credentials reflect connect/socketpair time; pathname and abstract sockets have different permission behavior. This is transport functionality, **not an NT port namespace, NT handle table, or NT token impersonation implementation**. [L1]

## Architectural consequences (inferences, not experimentally verified)

1. **Port identity and authority.** Model guest ALPC port objects and connection endpoints in the guest object/handle manager; do not equate a Unix socket pathname or fd with an NT handle. R87 and R92's object lifetime and guest DACL decisions apply. *Hypothesis*: a broker can map guest endpoint IDs onto private AF_UNIX channels, with explicit access checks at connection and per-message operations.
2. **Message and reply correlation.** Keep request IDs, pending replies, cancellation/timeout state, disconnect transitions, and reply ownership in a guest state machine. A `sendmsg`/`recvmsg` pair does not by itself prove NT send-and-wait atomicity or reply semantics. Do not assert a particular NT ordering until Windows-reference traces establish it.
3. **Handle transfer.** `SCM_RIGHTS` transfers host file descriptors, whereas guest NT handles can denote objects without corresponding host fds and carry guest-granted rights. Translate via the guest object table and validate transfer permissions; never expose arbitrary host fds solely because a guest message carries a handle attribute.
4. **Security context.** `SO_PEERCRED` is not a Windows token/SID, and cannot supply arbitrary thread impersonation context (R81/R92). Treat peer credentials as a *host channel authentication input*, never as the guest authorization decision.
5. **Shared sections/views.** A future zero-copy data-view path must preserve guest section object rights, mapping lifetime and address-space ownership. A Linux shared-memory fd is at most a backend; see R59/R94/R105. Avoid claiming performance benefit without a benchmark.
6. **Version/compatibility boundary.** The native ALPC interface should initially be considered a restricted compatibility target, not a stable Win32 application-facing promise. Keep unverified attributes fail-closed and record observed Windows version/build for each differential result.

## Proposed falsifiable conformance gates (NOT executed)

- **A1: port lifetime.** Windows-reference: create a named port, connect, duplicate/close client/server handles in varied orders; record endpoint closure and name reuse. Compare a pure guest state machine before Linux transport.
- **A2: request/reply.** Two clients send interleaved requests; vary server reply order, client timeout, server termination and client disconnect. Capture NTSTATUS, message IDs, and observable completion ordering.
- **A3: rights.** Try connection with different guest security descriptors and tokens; separately test handle-attribute transfer of a guest-only object, invalid handle and insufficient-rights handle. No host fd should bypass guest policy.
- **A4: views.** Exercise view attribute lifecycle, close/unmap/reuse races and malformed lengths on Windows reference; parse-only validation precedes any mapped guest memory.
- **A5: adversarial Linux backend.** Peer credential changes, ancillary truncation (`MSG_CTRUNC`), unexpected `SCM_RIGHTS`, abstract-namespace access, fd exhaustion, broker crash, and stale endpoint IDs. Verify fail-closed behavior.
- **A6: performance.** Only after semantic gates: measure message round-trip latency, tail latency, allocations and context switches across payload sizes, with Windows and Linux baselines and stated hardware. No speed claim today.

## Open questions / evidence frontier

- Which ALPC native structures and flags are stable for the project's **specific Windows target build**? Pin a System Informer revision and compare with Windows SDK/WDK material where published.
- Which request/reply and completion-list semantics are actually observable and required by first target applications?
- What is the minimal broker interface that preserves guest object rights without introducing cross-process host-fd privilege escalation?
- Which existing applications depend on ALPC directly versus through RPC or other Windows libraries? Determine by trace, not assumption.

## Sources

- [M1] Microsoft Learn, *ALPC class*, ETW provider and event types (page last updated 2021-01-07): https://learn.microsoft.com/en-us/windows/desktop/ETW/alpc (reviewed 2026-11-16).
- [P1] System Informer, `phnt/include/ntlpcapi.h`, upstream native declarations: https://github.com/winsiderss/systeminformer/blob/master/phnt/include/ntlpcapi.h (reviewed 2026-11-16; **moving branch, revision not pinned**).
- [L1] Linux `unix(7)`, AF_UNIX ancillary messages and credentials: https://man7.org/linux/man-pages/man7/unix.7.html (reviewed 2026-11-16).

**Evidence classification:** source-verifiable interface facts above; architecture points are explicitly hypotheses/inferences; all proposed tests are unexecuted.
