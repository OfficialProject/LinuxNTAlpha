# R190 — Per-handle I/O completion notification suppression and event signaling gates

Research date: 2026-11-19. Status: **documentation review only; no experiment executed**.

## Why this is a separate gate

R15/R43/R66 describe completion ports, cancellation and dequeue semantics. They do not establish the per-file-handle **notification mode** that determines whether a successful synchronous completion produces a port packet. An NT-compatible I/O layer that unconditionally posts every completion would violate documented Win32 behavior even if its IOCP queue and cancellation state machines were otherwise correct.

## Verified primary-source contracts

**V1 — flags are monotonic per handle.** Microsoft documents `SetFileCompletionNotificationModes(FileHandle, Flags)` with two flag values; once a notification mode is set for a handle it cannot be removed by subsequent calls. `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS` (0x1) suppresses a completion-port entry only if the handle is associated with a port, the file was opened for asynchronous I/O, and the request succeeds immediately without returning `ERROR_PENDING`. This is not a blanket rule for every successful request. Source: [Microsoft SetFileCompletionNotificationModes](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfilecompletionnotificationmodes).

**V2 — event signaling is a separate mode.** `FILE_SKIP_SET_EVENT_ON_HANDLE` (0x2) requests that the file handle not be signaled when I/O completes. Microsoft documents restrictions and exceptions; do not infer that this suppresses the separately supplied `OVERLAPPED.hEvent`, completion-port packets, or all other completion paths. Read the complete version-specific Microsoft contract before implementing its exceptions. Source: [same Microsoft API reference](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfilecompletionnotificationmodes).

**V3 — native completion-port association has distinct configuration.** `NtSetInformationFile` documents `FileReplaceCompletionInformation` as a way to change/remove a file's completion-port association, using `FILE_COMPLETION_INFORMATION` (port handle and completion key). This is distinct from the monotonic notification flags and must not be conflated with them. Sources: [NtSetInformationFile](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntsetinformationfile), [FILE_COMPLETION_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_completion_information).

## Hypothesized architecture (not validated)

**H1.** Keep guest-visible notification-mode bits in the NT file-handle/open-file state, and decide whether to enqueue a guest IOCP packet from **guest completion semantics**, not from whether Linux `io_uring` happened to produce a CQE. Linux backend completions may still be required internally to retire resources and finalize `IO_STATUS_BLOCK` state.

**H2.** Explicitly represent the return-path distinction: immediately completed success, pending then completed success, immediate failure, and asynchronous failure. A Linux CQE's arrival timing is not itself evidence of the Windows call's original return classification.

**H3.** Treat event signaling, IOCP posting, APC callbacks, `OVERLAPPED` status publication, and cancellation as separate outputs of one guest I/O state transition. Avoid a single generic 'notify on completion' boolean.

These are proposed design constraints, not proof that any particular backend satisfies Windows behavior.

## Proposed conformance matrix — NOT RUN

On a supported Windows reference version, create an overlapped file handle, associate an IOCP, then vary notification flags (0, 0x1, 0x2, 0x3), with controlled operations that (a) complete immediately successfully, (b) return `ERROR_IO_PENDING` then complete, and (c) fail. For each, record the **initial API return**, `GetLastError`, `OVERLAPPED.Internal/InternalHigh`, event state, file-handle wait state, and IOCP dequeue result. Use bounded waits and isolate test cases to prevent hangs. Repeat with and without explicit `OVERLAPPED.hEvent`, and with port association changed where the target Windows version permits. Verify flag monotonicity, invalid flags, synchronous-handle rejection, and socket-provider restrictions against Microsoft documentation before asserting expected results.

On Linux, separately exercise a mock guest-completion state machine with synthetic backend events. This cannot substitute for Windows differential results. Include interleavings for cancellation and close to test packet suppression without losing internal completion accounting.

## Open questions

1. Which exact native information-class and per-open-file-object fields correspond to each documented Win32 flag on the target Windows versions? Do not assume one-to-one mapping without source or differential evidence.
2. Does the target implementation model duplicated handles sharing the same open-file description and notification state? Confirm reference behavior, rather than equating NT handles with Linux file descriptors.
3. How do file-handle signal state and explicit `OVERLAPPED.hEvent` interact with `FILE_SKIP_SET_EVENT_ON_HANDLE` across supported Windows releases?
4. What happens to pending completions and port association during handle closure, duplication and reassociation?

## Evidence and limits

Primary sources checked 2026-11-19: Microsoft Win32 `SetFileCompletionNotificationModes`, Microsoft WDK `NtSetInformationFile`, and `FILE_COMPLETION_INFORMATION` pages linked above. No Windows reference system was used, no Linux code was executed, and no performance claim is made. The source pages are living documentation; implementation should pin Windows build and SDK versions in later differential work.
