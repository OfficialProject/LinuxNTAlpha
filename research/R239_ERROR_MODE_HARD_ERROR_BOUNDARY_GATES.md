# R239 — Process/thread error modes versus NT hard-error delivery

Research date: 2026-11-22. Status: **documentation review only; no Windows or Linux experiment**. This is a narrow extension of R52 (status conversion) and R227 (WER/crash reporting), not a claim to implement undocumented NtRaiseHardError behavior.

## Verified primary-source contracts

1. Microsoft documents process error mode through `SetErrorMode`/`GetErrorMode`. `SetErrorMode` returns the previous bitmask; child processes inherit the parent's error mode. Concurrent process-wide changes by threads are discouraged. `SEM_NOALIGNMENTFAULTEXCEPT` is architecture-dependent, and once set cannot subsequently be cleared where applicable. Source: [SetErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-seterrormode).
2. Microsoft documents thread-specific `SetThreadErrorMode`/`GetThreadErrorMode` from Windows 7/Server 2008 R2. `SetThreadErrorMode` optionally reports the previous mode and returns success/failure, unlike `SetErrorMode`'s previous-mask return. Microsoft says a thread inherits its process error mode and recommends the thread API to avoid process-wide interference. Sources: [SetThreadErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setthreaderrormode), [GetThreadErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getthreaderrormode).
3. The documented flags have distinct consequences: `SEM_FAILCRITICALERRORS` suppresses the critical-error dialog; `SEM_NOGPFAULTERRORBOX` affects Windows Error Reporting UI; `SEM_NOOPENFILEERRORBOX` affects the legacy `OpenFile` prompt; `SEM_NOALIGNMENTFAULTEXCEPT` belongs to the process API, not the listed thread flags. Sources: [Error Mode](https://learn.microsoft.com/en-us/windows/win32/debug/error-mode), [SetErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-seterrormode), [SetThreadErrorMode](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setthreaderrormode).
4. At the kernel-driver boundary, `IoSetThreadHardErrorMode` independently controls hard-error reporting for the current thread and returns its prior enabled state. Microsoft documents suppression of `IoRaiseHardError` dialogs and specific IRP completion consequences when disabled. `IoRaiseHardError` also has an APC-related deadlock caveat. These are **driver contracts**, not proof that a user-mode `NtRaiseHardError` call has identical behavior. Sources: [IoSetThreadHardErrorMode](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-iosetthreadharderrormode), [IoRaiseHardError](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-ioraiseharderror).
5. `IoRaiseInformationalHardError` may report success without a visible dialog in session 0 (Windows Vista onward); it may also return false for disabled dialogs or resource/duplicate-queue conditions. This further disproves a simplistic assumption that a hard-error request always implies UI. Source: [IoRaiseInformationalHardError](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-ioraiseinformationalharderror).

## Architectural implications (hypotheses, not findings)

- **H1:** Store process-default error mode in guest process state and thread-specific state in guest thread identity, not Linux task-global signal policy. The effective precedence/inheritance model needs a Windows-reference test, particularly when process mode changes after thread creation.
- **H2:** Model a hard-error request as a guest-owned policy/response transaction, separate from NTSTATUS-to-Win32 translation, WER crash capture and Linux signal/core handling. A headless implementation should have an explicit no-UI policy, not block indefinitely awaiting a Linux desktop dialog.
- **H3:** Keep kernel-driver hard-error behavior outside initial user-mode compatibility claims; no evidence yet establishes an equivalent Linux IRP/APC path.

## Differential acceptance gates (proposed, NOT run)

1. On a version-pinned Windows reference, record return values, previous-mode masks and `GetErrorMode`/`GetThreadErrorMode` values for each documented flag, zero mode, repeated calls and invalid bitmasks. Preserve `GetLastError` only where the API contract requires it.
2. In parent/child and two-thread tests, vary process mode before/after thread creation, then apply thread mode overrides and check actual effective behavior on an operation with a documented error-mode effect. **Do not assume** the documentation's inheritance wording resolves all override/precedence details.
3. Compare critical-error suppression, WER UI suppression and legacy OpenFile prompt suppression independently. Test in interactive and noninteractive sessions without relying on a visible dialog as the sole oracle; use bounded timeouts and disposable VMs.
4. Separate user-mode API results from driver `IoSetThreadHardErrorMode` and `IoRaiseHardError` tests; driver tests require a separately authorized, isolated Windows test environment and must not be inferred from user-mode results.
5. For Linux host implementation, assert that no guest can display host-global UI, signal another tenant or stall the supervisor through an unbounded hard-error wait. Check cleanup on process termination and worker crash.

## Open questions

- Exact `NtRaiseHardError` parameter/response semantics and privilege/session restrictions across supported Windows builds require version-pinned primary implementation evidence or controlled Windows differential tests; no assertion made here.
- Whether a newly created thread snapshots process mode or resolves a later process-mode change dynamically remains to be measured.
- Which error-mode flags a chosen target Windows build ignores or normalizes, and whether unsupported bits fail or round-trip, remain open.

## Relationship to existing research

R52 covers error-code domains, not process/thread error-mode policy. R227 covers WER and host crash reporting, not critical-error prompts or per-thread overrides. This note addresses the missing **policy and delivery boundary** without claiming a working NT hard-error subsystem.
