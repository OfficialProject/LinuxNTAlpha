# R83 — Windows oplock breaks versus Linux file leases

Date: 2026-11-14. Status: **documentation review only**; no Windows reference run, Linux lease test, prototype, or benchmark was performed.

## Question and boundary

Can Linux `F_SETLEASE` serve as a transparent backend for NT/Win32 opportunistic locks (oplocks) on files and directories? **Answer: not as a direct semantic substitute.** This does not rule out selective use as one backend signal inside a separately implemented NT file-object and oplock state machine.

## VERIFIED — primary documented contracts

1. **Windows acquisition and completion.** Microsoft documents `FSCTL_REQUEST_OPLOCK` with `REQUEST_OPLOCK_INPUT_BUFFER`/`REQUEST_OPLOCK_OUTPUT_BUFFER`. A successful grant through an overlapped handle is represented by a pending control request whose completion reports the break, rather than an ordinary synchronous acquisition return. The documented grant conditions depend on requested level, open count, stream and other oplocks. [Microsoft, Requesting and Granting Oplocks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/granting-oplocks); [FSCTL_REQUEST_OPLOCK](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_request_oplock).
2. **Windows break/ack ordering.** For break types that require acknowledgment, the conflicting operation may be held until the holder acknowledges, or closes the handle. Microsoft documents no automatic timeout for that acknowledgment wait. Some read-only breaks are advisory and require no acknowledgment; behavior must be selected by oplock level and triggering operation. The `REQUEST_OPLOCK_OUTPUT_FLAG_ACK_REQUIRED` flag determines whether acknowledgment is needed for modern requests. [Microsoft, Breaking Oplocks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/breaking-oplocks); [Acknowledging Oplock Breaks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/acknowledging-oplock-breaks).
3. **Windows sharing-order interaction.** Microsoft documents that NTFS batch/filter breaks can precede a sharing-violation check, while RH/RWH breaks can occur after the sharing conflict is detected; the latter allows the holder to close and potentially avoid the sharing violation. `FILE_COMPLETE_IF_OPLOCKED` changes how a conflicting create waits and reports its result. [Microsoft, Breaking Oplocks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/breaking-oplocks).
4. **Windows directory support.** Microsoft documents modern R/RH directory oplocks (Windows 8 and later), including breaks caused by enumeration-changing operations and a distinct RH rename/delete transition. [Microsoft, FSCTL_REQUEST_OPLOCK](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_request_oplock).
5. **Linux lease model.** Linux `fcntl(F_SETLEASE)` supplies read/write leases on **regular files**; conflicting `open(2)` or `truncate(2)` sends a signal (normally SIGIO), and may block the breaker. The lease is associated with an **open file description** and thus shared by duplicated descriptors. A lease holder may release/downgrade it, but the kernel can **forcibly** break it after `/proc/sys/fs/lease-break-time`. Eligibility and filesystem support restrict acquisition. [Linux man-pages, F_SETLEASE(2const)](https://www.man7.org/linux/man-pages/man2/F_SETLEASE.2const.html).

## DERIVED — specific non-equivalences

- **Timeout mismatch:** Linux forced lease break cannot by itself preserve Windows's documented acknowledgment-dependent indefinite wait. Host lease expiration is not guest acknowledgment.
- **Object/type mismatch:** Linux regular-file leases do not directly implement modern Windows directory oplocks or the R/RH/RW/RWH caching-level state machine.
- **Completion mismatch:** A Linux signal is not an NT pending FSCTL completion, IO status, overlapped event, or IOCP packet. An explicit completion adapter would be necessary.
- **Namespace and enforcement mismatch:** Host operations that bypass the guest file broker can interact with Linux leases under host rules; guest `CreateFile` sharing checks and Windows-specific break ordering still require guest-aware mediation. This is an architecture inference, **not a verified bypass exploit**.
- **Ownership mismatch:** Linux lease open-file-description lifetime and NT handle/stream oplock association must be tested under duplicate, inherit, close and reopen sequences rather than assumed identical.

## HYPOTHESIS — possible architecture, not selected

Implement guest file-stream oplock state, handle identity, pending FSCTL requests, acknowledgments and conflicting-operation queues in a central NT file service. Evaluate `F_SETLEASE` only as an optional host-side notification/cooperation mechanism for regular files; never treat its timeout, signal or privilege policy as the NT contract. This is particularly important for multi-process guest consistency and host-file access. It is **not proven** that this design covers mmap, rename, network mounts, or operations outside the broker.

## OPEN — falsifiable conformance gates

1. **Windows reference:** For each supported modern R/RH/RW/RWH level, request on overlapped handles; record grant status, pending I/O completion, output level, ACK_REQUIRED and exact conflicting-operation completion ordering.
2. **Sharing and close:** Contrast a conflicting `CreateFile` with and without `FILE_COMPLETE_IF_OPLOCKED`, with owner acknowledgment, owner close, and an actual sharing violation; record whether the break occurred before the failed create.
3. **Directory:** Request R/RH on directories; separately create a child, rename/delete directory and modify metadata. Record break level and acknowledgment requirement. Gate by Windows version and filesystem.
4. **Host lease baseline:** On an isolated disposable Linux filesystem, probe `F_SETLEASE` support, `SIGIO`, breaker blocking, downgrade, duplicated-descriptor closure, `O_NONBLOCK`, and the configured lease-break timeout. Report kernel, filesystem, mount, UID/capabilities and effective `lease-break-time`.
5. **Interference/races:** Explicitly test host writers outside the NT broker, guest handles in different processes, mmap writers, rename/unlink and cancellation; do not claim isolation if any operation escapes the intended guest semantics.
6. **Correctness criterion:** No conflation of pending acquisition with successful synchronous completion; no premature release of a guest operation requiring acknowledgment; no loss or duplicate delivery of a break completion. Test failed grants and handle closure separately.

## Decision impact

**Do not map NT oplocks one-to-one onto Linux file leases.** Keep oplock behavior in the file-service design review alongside R10/R37/R49/R50/R57/R58/R78; only consider lease acceleration after Windows-reference and Linux-backend tests. No implementation decision or performance claim follows from this documentation audit.

## Evidence record

Sources reviewed 2026-11-14: Microsoft Learn driver and Win32 oplock contracts linked above, and Linux man-pages `F_SETLEASE(2const)`. These are interface documents, **not pinned source-code revisions**. No code was imported; no experiment was run.
