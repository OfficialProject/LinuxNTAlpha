# R229 — Windows process/thread error modes versus host error presentation

Status: **VERIFIED FROM SOURCE** for the linked Microsoft API contracts; **HYPOTHESIS** for the guest implementation boundary; **OPEN** for Windows-reference conformance. Reviewed 2026-11-21. **No experiment executed.**

## Scope and non-duplication

R227 covers Windows Error Reporting and Linux core-dump boundaries; R108 covers signal/exception ingress. This note isolates **serious-error dialog suppression and process/thread mode inheritance**, not crash-report generation or exception dispatch. A Linux errno or signal alone cannot establish whether Windows would display a critical-error dialog.

## Verified primary-source contracts

1. Microsoft documents a **process error mode**, accessible through `GetErrorMode` and set through `SetErrorMode`. `SetThreadErrorMode` supplies a thread-specific override; `GetThreadErrorMode` retrieves the effective thread mode. Microsoft states that a thread inherits its process's error mode. Source: [SetThreadErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setthreaderrormode).
2. Documented `SetThreadErrorMode` flags include `SEM_FAILCRITICALERRORS` (0x0001), `SEM_NOGPFAULTERRORBOX` (0x0002), and `SEM_NOOPENFILEERRORBOX` (0x8000). The first suppresses critical-error-handler message boxes; the second suppresses the Windows Error Reporting dialog; the third concerns legacy `OpenFile` prompts. These flags must **not** be collapsed into a single generic “disable errors” switch. Source: [SetThreadErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setthreaderrormode).
3. `SetThreadErrorMode` returns success/failure and optionally writes the prior mode; failure requires `GetLastError` to inspect details. It is therefore observable API state, not merely a UI preference. Source: [SetThreadErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setthreaderrormode).
4. At the **Windows kernel-driver boundary**, `IoSetThreadHardErrorMode` enables/disables hard-error reporting for the current thread and returns the previous enabled state. Microsoft documents that disabling it suppresses `IoRaiseHardError` user notification, with the IRP completed without copying data into user buffers; `IoRaiseInformationalHardError` then returns FALSE. This is kernel DDI evidence of a separate thread-specific hard-error policy; it does **not** establish that arbitrary Windows applications may call this driver DDI. Sources: [IoSetThreadHardErrorMode](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-iosetthreadharderrormode), [IoRaiseHardError](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-ioraiseharderror).
5. `IoRaiseHardError` uses a normal kernel APC for user-dialog delivery; Microsoft warns of deadlock when normal kernel APC delivery is disabled while an I/O path waits for that dialog. This is a specific kernel-driver hazard, **not** proof that the Linux guest runtime needs to reproduce a Windows kernel APC to implement the initial user-mode API. Source: [IoRaiseHardError](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-ioraiseharderror).

## Architectural hypothesis — not selected

Keep **guest-owned process and per-thread error-mode state** separate from Linux signal disposition, host stderr, GUI notifications, and the WER subsystem. Route eligible guest errors through an explicit policy decision before optional UI presentation. Preserve error return values and per-thread last-error state regardless of whether a dialog is suppressed. Never use `SEM_NOGPFAULTERRORBOX` as a blanket instruction to swallow access violations or other guest exceptions.

The runtime must define whether thread creation snapshots the process mode or observes later process-mode changes. The cited documentation alone does not settle all interaction/race cases; do not silently encode an assumption. In a headless runtime, “no GUI” must not be confused with Windows-compatible success or error semantics.

## Falsifiable acceptance gates — NOT RUN

- On a pinned Windows build, read process/thread modes; set process mode, create threads, set and reset thread overrides, and observe previous-mode outputs and `GetLastError` on invalid flags. Include concurrent process-mode changes.
- Compare process inheritance across child creation and independent threads; record exactly which bits survive, with Windows version and executable fixture.
- Exercise a controlled missing-media or legacy `OpenFile` error without destructive operations; separately record API return/last-error and whether a dialog is presented. Do not infer UI behavior from return codes alone.
- For WER, distinguish the crash/exception path from `SEM_NOGPFAULTERRORBOX` presentation policy, using R227's reporting boundary.
- Verify guest thread-mode isolation, no host-global signal-disposition mutations, no host modal dialog deadlocks, and deterministic behavior in a headless environment.

## Open questions

1. Exact process/thread override precedence and snapshot semantics on the chosen reference Windows builds.
2. Which native NT hard-error service behavior is required for the initial user-mode milestone, beyond documented Win32 error-mode APIs.
3. Whether a desktop shell or separate UI broker should ever present hard errors; no UX decision is justified by this note.
4. No performance result, Linux test, or Windows-reference differential result exists for this note.

**Boundary:** Error-mode API compatibility is independent of Linux errno translation, exception delivery, and WER integration. No kernel modification is warranted on this evidence.
