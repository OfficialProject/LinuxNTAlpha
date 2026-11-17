# R135 — Windows opportunistic locks versus Linux file leases (documentation-only gates)

Reviewed: 2026-11-17. Status: **primary-source contract review; no code executed and no Windows/Linux differential experiment performed**.

## Scope and distinction from prior research

R49/R50/R57/R58/R114 cover deletion, rename, byte-range locks and change notification. This note addresses **opportunistic locks (oplocks)**, a separate cache-coherency and conflicting-open protocol. An oplock is not a byte-range lock, nor is its break notification interchangeable with directory-change notification.

## VERIFIED from Microsoft primary documentation

1. Windows 7-style oplocks use `FSCTL_REQUEST_OPLOCK` with `REQUEST_OPLOCK_INPUT_BUFFER` and `REQUEST_OPLOCK_OUTPUT_BUFFER`. On a successful grant, user-mode `DeviceIoControl` reports `ERROR_IO_PENDING`; the overlapped operation completes on break, with break details in the output buffer. Synchronous file handles are not eligible. [M1, M2]
2. Legacy level-1 oplocks permit local read/write caching, and a conflicting operation requires a break acknowledgment before proceeding. Legacy level-2 read caching has no acknowledgment requirement on break. These are different state machines. [M3, M4]
3. The modern control code supports repeated level requests on the same handle, unlike legacy codes that may require close/reopen. Breaks can be advisory-only or require acknowledgment depending on the level/operation. **Do not model every break as blocking.** [M1]
4. Microsoft documents directory oplocks with Windows 8/Server 2012 support for read and read-handle levels; changes affecting directory enumeration can break them, while an RH directory oplock broken by rename/delete requires acknowledgment. [M1]
5. Remote-file oplocks are negotiated by the redirector, not directly requested by client applications with the local FSCTL. A local-file implementation does not establish SMB compatibility. [M1, M2]

## VERIFIED from Linux primary interface documentation

6. Linux `fcntl(F_SETLEASE)` establishes read/write leases on an **open file description**; conflicting `open` or `truncate` triggers a signal (normally SIGIO), with the conflicting operation blocked unless nonblocking behavior applies. A lease is shared by duplicated descriptors and removed when all references close, or explicitly released. [L1]
7. Linux permits read leases on read-only descriptors and write leases only when there are no other open descriptors; lease availability depends on regular-file and filesystem support, ownership/capability checks, and host policy. A kernel lease-break timeout can forcibly remove/downgrade a lease. [L1]
8. Linux lease state is not a direct substitute for the Windows overlapped `DeviceIoControl` completion, oplock levels/keys, per-handle output records, directory oplocks, or acknowledgment-specific state transitions. **This is a contract comparison, not an experimental assertion about every Linux filesystem.** [M1, M2, L1]

## HYPOTHESIS: architecture to evaluate, not select yet

Maintain a **guest-visible per-stream oplock state machine** in the NT file-object service, with request identity, level, key (where applicable), grant/deny, break target, pending completion and acknowledgment state. Enforce conflicting guest operations at the same serialized file-object coordination boundary as share access and delete/rename transitions. Treat `F_SETLEASE` as an optional host coordination mechanism for selected local regular files, not the source of guest-visible truth.

**Security boundary:** a user-space guest coordinator cannot automatically prevent arbitrary native Linux processes from opening or modifying the same backing file. Linux leases may help for supported operations, but cannot be assumed to provide complete Windows-style coherence. Any claim of isolation requires explicit threat model and host-side enforcement or a controlled storage namespace.

## Proposed falsifiable gates — NOT EXECUTED

- **Windows reference matrix:** open a temporary file with `FILE_FLAG_OVERLAPPED`; request R, RH, RW, RWH and supported legacy levels with documented FSCTLs; record grant error, `OVERLAPPED.Internal`, output break level and acknowledgment requirement. Include multiple handles, same-key/different-key cases, close during pending request, and cancellation.
- **Directory matrix:** on supported Windows versions, request R/RH directory oplocks and independently create, delete, rename child and rename/delete directory; record advisory versus acknowledgment-required breaks.
- **Ordering matrix:** request an oplock, issue a conflicting open from a second process, deliberately delay acknowledgment, record whether the second operation blocks, then acknowledge or close. Apply strict timeouts to avoid test hangs.
- **Linux capability matrix:** on named filesystem/mount/kernel combinations, test `F_SETLEASE` grants, duplicate-fd lifetime, signal delivery, forced timeout, nonblocking conflicting open, and native-process bypass scenarios. Capture mount type, kernel version, lease-break-time and permission errors; never assume success.
- **Guest service matrix:** model per-stream grant/break/ack transitions with deterministic concurrency tests, then compare exact externally visible results against the Windows reference; verify that no blocked operation can outlive guest handle teardown or lost completion.

## OPEN

- Exact modern oplock level/key transition table and error precedence across Windows builds and filesystems (requires a pinned Microsoft oplock overview plus Windows-reference results).
- Whether an eventual Linux backend needs a filesystem mediation layer for native-process interference.
- Correct interaction with SMB server/redirector paths, guest ACLs, shared mappings, and cross-process handle duplication.
- Quantitative cost of centralized stream coordination and pending-completion delivery; no performance claim is justified.

## Primary sources

- [M1] Microsoft, `FSCTL_REQUEST_OPLOCK` user-mode contract: https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_request_oplock
- [M2] Microsoft, *Requesting and granting oplocks*: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/granting-oplocks
- [M3] Microsoft, `FSCTL_REQUEST_OPLOCK_LEVEL_1`: https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_request_oplock_level_1
- [M4] Microsoft, `FSCTL_REQUEST_OPLOCK_LEVEL_2`: https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_request_oplock_level_2
- [L1] Linux man-pages project, `F_GETLEASE/F_SETLEASE(2const)`, man-pages 6.19 (2026-01-14): https://man7.org/linux/man-pages/man2/F_GETLEASE.2const.html

No copied implementation code, no benchmark and no assertion of working NT compatibility.
