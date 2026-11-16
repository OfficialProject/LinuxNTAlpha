# R117 — Pinned ALPC native-header evidence and compatibility boundary

Date: 2026-11-16. Status: **source inspection only**; no Windows reference run, Linux implementation, or benchmark.

## Why this follow-up exists

[R116](R116_ALPC_UNIX_IPC_BOUNDARIES.md) correctly identifies the distinction between ALPC guest semantics and Unix transport, but cites a moving System Informer branch and explicitly leaves the native-header revision unpinned. This note pins that evidence and narrows what it can establish; it does **not** convert reverse-engineered declarations into a Microsoft ABI guarantee.

## Verified from inspected upstream source

At System Informer commit [`9281222e16a736e6c45c8c068ad576289f9675a4`](https://github.com/winsiderss/systeminformer/commit/9281222e16a736e6c45c8c068ad576289f9675a4), the file [`phnt/include/ntlpcapi.h`](https://github.com/winsiderss/systeminformer/blob/9281222e16a736e6c45c8c068ad576289f9675a4/phnt/include/ntlpcapi.h) was fetched directly; its Git blob SHA was `9aea30a9f5ee921b6130d418f38c9c2cd0d3c2c2`.

The inspected file declares `NtAlpcCreatePort`, `NtAlpcSendWaitReceivePort`, `NtAlpcCancelMessage`, and `NtAlpcImpersonateClientOfPort`. It defines distinct ALPC message attribute masks for security (`0x80000000`), view (`0x40000000`), context (`0x20000000`), and handle (`0x10000000`). Its `ALPC_CANCEL_FLAGS` enum includes `TRY_CANCEL`, `NO_CONTEXT_CHECK`, and `FLUSH` flags. **These are verified declarations in a third-party header, not verified kernel behavior or stable Windows contracts.** In particular, source comments describing internal paths and effects must not be treated as experimental proof.

The header's separation of cancellation, impersonation, and handle/view attributes provides an explicit **research inventory**, rather than evidence that a Unix-domain socket can emulate those operations directly. Linux AF_UNIX descriptor passing is a host-fd transfer mechanism, not an NT guest-handle transfer or token impersonation operation (see R116).

## Implementation implications (hypotheses, not results)

1. Model message cancellation as a distinct guest operation with observable ownership, race, and completion states. Do not implement `NtAlpcCancelMessage` as an unconditional Linux socket close.
2. Separate guest security and impersonation context from host socket peer credentials. Do not equate a received host UID with an NT impersonation token.
3. Treat the handle, view, context, and security attribute masks as **different parsing and authorization surfaces**; unsupported attributes should fail explicitly rather than be silently ignored.
4. Keep native structure layouts and flag support behind a target-build/version gate until independently verified. The pinned header alone cannot establish alignment, field layout, status codes, or compatibility across Windows releases.

## Falsification plan — NOT EXECUTED

- **C1 cancellation:** On a controlled Windows reference build, submit a request, race cancel against reply, disconnect, and timeout, and record NTSTATUS plus whether a reply remains receivable. Repeat with documented flag combinations and reject unknown bits.
- **C2 impersonation:** Compare a server thread's effective token before, during, and after ALPC client impersonation with differing client impersonation levels and denied rights; record token restoration even after errors.
- **C3 attributes:** Independently vary security, context, handle, and view attributes; include malformed lengths and invalid handles. Record exact target build, structures, statuses, and observed ownership transitions.
- **C4 Linux candidate:** Only after C1–C3, prototype a guest object-table state machine with AF_UNIX transport; deliberately inject truncated ancillary data, unexpected fds, and broker termination. Assert guest authorization is not bypassed.
- **C5 differential:** Compare traces, not only successful return codes. Any unsupported Windows case remains OPEN, not PASS.

## Remaining evidence gaps

- The project's chosen Windows reference build and a pinned Microsoft WDK/SDK baseline have not been established here.
- No Windows kernel trace or reference program validates the third-party header's undocumented flags, structures, or comments.
- No claim is made about ALPC performance, prevalence among games, or suitability for a kernel implementation.

**Source provenance:** System Informer commit and blob above, inspected 2026-11-16; [R116](R116_ALPC_UNIX_IPC_BOUNDARIES.md) for separately documented Microsoft ETW and Linux AF_UNIX sources. This is a source-pinning and cancellation/impersonation follow-up, not a replacement for R116.
