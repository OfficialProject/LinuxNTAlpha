# R197 — ALPC port/message identity versus Linux local IPC (2026-11-20)

**Status:** primary-source documentation review; **no code or Windows/Linux experiment executed**. This is a compatibility/security design gate, not a selected architecture.

## Why this is a separate gate

The project has researched named pipes (R37/R78), Object Manager naming (R87/R147), NT handles (R67/R146) and async request ownership (R195). None establishes that a Unix-domain socket is a drop-in replacement for Windows **Advanced Local Procedure Call (ALPC)** ports. This note isolates the missing port/message state machine and a minimum falsifiable test plan.

## Verified primary-source observations

1. Microsoft documents ALPC as a distinct Windows kernel event class with send, receive, wait-for-reply, wait-for-new-message and unwait events. This is evidence of separately observable ALPC operations, **not** a complete public wire/API specification. [Microsoft ALPC ETW class](https://learn.microsoft.com/en-us/windows/win32/etw/alpc).
2. Microsoft's Windows Performance Recorder stack-event reference separately lists `AlpcConnectRequest`, `AlpcConnectSuccess`, `AlpcConnectFail`, `AlpcClosePort`, `AlpcSendMessage`, `AlpcReceiveMessage`, `AlpcWaitForReply` and `AlpcUnwait`. Thus a single undifferentiated socket read/write abstraction is not justified by the available evidence. [Microsoft WPR Stack](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/stack-wpa).
3. Microsoft's debugger documentation explicitly says legacy LPC is emulated in ALPC and recommends `!alpc` rather than `!lpc`. Do not infer that every historical LPC structure or edge case is identical to ALPC. [Microsoft !lpc debugger documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/-lpc).
4. Linux AF_UNIX supports local sockets and ancillary descriptor passing (`SCM_RIGHTS`) and credential-related mechanisms; those are **Linux descriptor and credential** mechanisms, not automatically NT process-relative handle rights, ALPC message attributes or guest security tokens. [Linux unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html). Linux `sendmsg(2)` and `recvmsg(2)` define ancillary-data transfer and truncation handling. [sendmsg(2)](https://man7.org/linux/man-pages/man2/sendmsg.2.html), [recvmsg(2)](https://man7.org/linux/man-pages/man2/recvmsg.2.html).

**Evidence boundary:** Microsoft's event documentation is observational, not a normative description of `NtAlpc*` signatures, attribute validation, message ownership, queue limits, cancellation, or security semantics. No undocumented behavior is asserted here as verified.

## Candidate model — HYPOTHESIS, not implementation

Maintain a broker-owned **guest ALPC port object** independent of any host AF_UNIX fd. Track named connection-port identity, accepted communication endpoint identity, process-relative guest handles and granted rights, message IDs and correlation to outstanding reply waits, per-message payload/attribute ownership, peer guest-token identity, and disconnect/close generation. A Linux socket may carry bytes; it must not define guest access checks or handle identity. Any handle-transfer feature must pass through the existing guest handle table and rights-validation boundary (R67/R92/R146), not expose raw `SCM_RIGHTS` as NT handles.

This is deliberately only a candidate: the exact Windows message ordering, port creation/connect/accept sequencing, cancellation statuses, timeout units, impersonation and attribute behavior must be learned from a **pinned Windows reference build** before coding a purported compatible state machine.

## Prioritized conformance and security gates — PROPOSED, NOT RUN

| Gate | Reference evidence required | Candidate Linux-side check |
| --- | --- | --- |
| Connection | Create/connect/accept/reject and disconnect/close ordering; status codes | Explicit endpoint state transitions and generation-safe close |
| Request/reply | Multiple concurrent clients, message-ID association, server replies out of order | No cross-client or stale-ID reply delivery |
| Wait/cancel | Timeout, thread termination, port close during blocked receive/reply wait | No stranded waiters, double completion or use-after-free |
| Rights | Named-port DACL, impersonation/token changes, cross-process handles | Guest ACL checked before host IPC operation; Linux UID never substituted for guest token |
| Attributes | Message lengths, malformed/unknown attribute masks, resource/handle transfer | Bounds-check all buffers; prevent host-fd injection and guest-handle forgery |
| Namespace | Same name in differing guest namespace/session contexts | Guest Object Manager identity does not leak host pathname |
| Backpressure | Full queue, peer death, broker restart, retry behavior | Bounded memory and deterministic teardown without claiming Windows-equivalent limits |
| Performance | Same-hardware Windows reference vs Linux prototype under matched concurrency and payloads | Report latency distributions and CPU/memory, not blanket ALPC speed claims |

**Reference harness prerequisite:** explicitly identify the Windows build, target architecture, availability and calling convention of each native entry point under test; use documented ETW/WinDbg observability where permitted. Do not infer semantics solely from tracing event names. Keep guest fuzz tests isolated from the host broker. Verify license and provenance before using ReactOS/Wine implementations as reference material.

## Decision

**OPEN:** choose neither AF_UNIX, shared-memory rings nor kernel extension as the definitive ALPC backend until message/handle/security semantics are characterized. The initial low-risk milestone is a nonexecuting port/message state-machine model plus Windows-reference probes, not a claim of Windows service compatibility.

**Source retrieval date:** 2026-11-20. **Experiments:** none. **Upstream source revision:** not pinned (documentation pages, not source-code audit).
