# R134 — Job UI restrictions are desktop security policy, not cgroup resource limits

Status: **VERIFIED FROM PRIMARY DOCUMENTATION** for explicitly cited API contracts; **HYPOTHESIS** for the proposed mediation design; **OPEN** for reference behavior and implementation. Retrieved 2026-11-17. No experiments executed.

## Gap and scope

R36/R76 model job membership and cgroup enforcement; R131 models process-token checks; R132/R133 cover completion/drain and CPU limits. This note isolates **JOBOBJECT_BASIC_UI_RESTRICTIONS**, a different boundary: Windows USER/desktop/clipboard/global-atom policy. These flags cannot be implemented by mapping job membership to Linux cgroup v2. This is a desktop-facing NT compatibility and cross-process isolation gate, not a CPU/memory scheduling gate.

## Verified Windows contract

Microsoft documents eight flags in `JOBOBJECT_BASIC_UI_RESTRICTIONS.UIRestrictionsClass`:

| Flag | Documented restriction | Required guest-facing mediation |
| --- | --- | --- |
| `JOB_OBJECT_UILIMIT_HANDLES` (0x1) | Disallow USER handles belonging to processes outside the job; restrict broadcast destinations and hook installation | USER-handle authorization, broadcasts, hooks |
| `JOB_OBJECT_UILIMIT_READCLIPBOARD` (0x2) | Disallow clipboard reads | Clipboard read path |
| `JOB_OBJECT_UILIMIT_WRITECLIPBOARD` (0x4) | Disallow clipboard writes | Clipboard write path |
| `JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS` (0x8) | Disallow changes through SystemParametersInfo | System parameter mutation |
| `JOB_OBJECT_UILIMIT_DISPLAYSETTINGS` (0x10) | Disallow ChangeDisplaySettings | Display configuration mutation |
| `JOB_OBJECT_UILIMIT_GLOBALATOMS` (0x20) | Give each restricted job a separate atom table | Atom namespace and lookup |
| `JOB_OBJECT_UILIMIT_DESKTOP` (0x40) | Disallow CreateDesktop and SwitchDesktop | Desktop creation/switch |
| `JOB_OBJECT_UILIMIT_EXITWINDOWS` (0x80) | Disallow ExitWindows/ExitWindowsEx | Session exit/shutdown request |

The source additionally documents `UserHandleGrantAccess` for selectively granting USER-handle access to a job subject to UI restrictions. The Windows job is a securable named object with separately checked rights; a restriction is not itself a Linux credential or cgroup setting.

Sources: [Microsoft JOBOBJECT_BASIC_UI_RESTRICTIONS](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_ui_restrictions), [Microsoft Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects), [Microsoft Job Object Security and Access Rights](https://learn.microsoft.com/en-us/windows/win32/procthread/job-object-security-and-access-rights).

## Verified Linux boundary

Linux cgroup v2 documents resource controls, membership, and hierarchical behavior; it does not specify Windows USER handle ownership, clipboard, desktop, global-atom or Windows hook semantics. Therefore a cgroup controller alone does **not** establish conformance for any UI flag. This is a conclusion about the documented interface, not a claim that all Linux desktop stacks lack analogous policies.

Source: [Linux kernel cgroup v2 documentation](https://docs.kernel.org/admin-guide/cgroup-v2.html).

## Architecture hypothesis — unimplemented

A guest desktop/USER authority should resolve guest process identity and job ancestry before allowing operations at the actual authority boundary. Guest-visible handles must be checked against guest job membership, not against host PID alone. A clipboard or display service must not accept an alternative unmediated path that bypasses a restriction. The global-atom namespace may require per-job identity and lifecycle distinct from process-local tables. `UserHandleGrantAccess` means an unconditional deny rule for all cross-job handles is potentially too strong.

**Open:** exact nested-job inheritance/combination rules for each UI flag, interaction with desktop/window-station ACLs, failure codes, and timing of updates require Windows-reference measurement. Do not infer those rules from the flag names. Host Wayland/X11 permissions, portal APIs and desktop integration have not been audited here.

## Falsifiable conformance and security gates (not executed)

1. On a version-recorded Windows reference, establish a control case for each flag, then set one flag at a time and record return values, last-error codes and observable side effects for representative documented API calls.
2. Create two processes in the same job and one outside it; exercise USER-handle lookup, window broadcasts, hook installation, and `UserHandleGrantAccess` with explicit positive and negative cases.
3. Compare global-atom creation/lookup across two jobs, and test cleanup when members exit but job handles remain; distinguish guest job lifetime from host process lifetime.
4. Test clipboard read versus write independently, including any operation forwarded to a host desktop service. Verify blocked guest operations cannot reach the host clipboard through a second path.
5. Repeat with nested jobs and separately recorded desktop/window-station access; log ambiguity rather than inventing inheritance semantics.
6. Audit whether the chosen desktop backend exposes host resources to untrusted guest code without the guest USER authority. If yes, the backend cannot claim isolation solely from API-level checks.

## Decision gate

Do not treat `SetInformationJobObject(JobObjectBasicUIRestrictions)` as a no-op merely because cgroup v2 has no matching controller. Before desktop integration, define an enforceable USER/desktop authority boundary, per-flag error/side-effect matrix, and a policy for unsupported features. **No tests, performance measurements, or Windows build-specific behavior are claimed in this note.**
