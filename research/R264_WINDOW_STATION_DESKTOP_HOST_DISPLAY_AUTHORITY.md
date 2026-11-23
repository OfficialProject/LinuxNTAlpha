# R264 — Window-station and desktop object authority versus Linux display sessions

Date: 2026-11-23  
Status: **DOCUMENTATION SYNTHESIS; NO EXPERIMENT**  
Scope: guest Win32 GUI object security and lifecycle, not rendering implementation.

## Question

Can a Linux Wayland/X11 session stand in for Windows window stations and desktops when LinuxNTAlpha eventually supports GUI programs?

**Finding (primary API contracts): no direct equivalence is established.** Windows exposes window-station and desktop objects with handles, access masks, security descriptors and process/thread attachment rules. A Linux display connection is a candidate rendering/input transport, not sufficient evidence of equivalent guest object identity or access control.

## Verified documented contracts

1. Microsoft documents window stations as securable objects containing a clipboard, global atom table and desktop objects. The interactive station is named `WinSta0`; noninteractive stations have different interaction properties. [Window Stations](https://learn.microsoft.com/en-us/windows/win32/winstation/window-stations).
2. A desktop is a securable object associated with a window station; desktop objects contain windows, menus and hooks. Windows API desktop identity therefore cannot be reduced to a host Wayland surface. [Desktops](https://learn.microsoft.com/en-us/windows/win32/winstation/desktops).
3. `SetProcessWindowStation` selects the calling process's window station; `SetThreadDesktop` associates a desktop with a thread and has documented failure restrictions when the thread already has windows or hooks (subject to the documented current-desktop exception). [SetProcessWindowStation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setprocesswindowstation), [SetThreadDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddesktop).
4. `OpenWindowStation`, `CreateWindowStation`, `OpenDesktop` and `CreateDesktop` take desired access and/or security attributes. Object security checks and handle-granted rights are guest semantics, not automatically provided by a Linux compositor connection. [Window Station Security and Access Rights](https://learn.microsoft.com/en-us/windows/win32/winstation/window-station-security-and-access-rights), [Desktop Security and Access Rights](https://learn.microsoft.com/en-us/windows/win32/winstation/desktop-security-and-access-rights).
5. `SwitchDesktop` changes which desktop is visible on the interactive window station, subject to access and documented limitations. This is not the same operation as selecting a Linux virtual workspace. [SwitchDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-switchdesktop).
6. Linux Wayland's protocol describes client/server object communication and compositor-mediated surfaces/input; its protocol alone does not define NT window-station or desktop handles, Windows access masks, or Win32 thread desktop attachment. [Wayland core protocol](https://wayland.freedesktop.org/docs/html/apa.html).

These are documented contracts, **not** observed Windows test outcomes, claims of complete Windows behavior, or proof that any particular Linux compositor is insecure.

## Architecture hypotheses (unverified)

- **H1:** Implement window-station and desktop identities as guest broker objects with guest security descriptors and handle tables, while delegating only authorized rendering and input transport to Wayland/X11.
- **H2:** A guest desktop-to-host-surface mapping needs explicit visibility, focus, clipboard and input ownership policy. Directly granting each guest process a host display socket risks bypassing guest desktop isolation; whether and how this occurs depends on the compositor and deployment.
- **H3:** Guest object identity and authorization must survive host display reconnects, and must not be inferred from host process IDs or host surface IDs.

## Proposed conformance gates — NOT RUN

**Windows-reference tests:** On an explicitly recorded Windows build and logon/session configuration, create two desktop objects with distinguishable names and access descriptors; exercise Create/Open/Close, SetThreadDesktop before and after creating a window, GetThreadDesktop, SetProcessWindowStation, and SwitchDesktop. Record return values, GetLastError, effective token, granted rights, thread state, and cleanup. Avoid assuming a service session is interactive. Include denied-access and invalid-handle cases.

**Linux prototype tests:** In a disposable isolated guest, prove that a guest without DESKTOP_SWITCHDESKTOP or relevant access cannot invoke the corresponding guest operation even if it can create host display resources. Test concurrent guest desktop transitions, clipboard isolation, compositor disconnect/reconnect, process death, and guest handle reuse. Instrument all guest-to-host display requests and confirm no direct unmediated display socket escapes the broker policy.

**Security gate:** Treat host display server access as a separate host capability. A guest security descriptor is not a Linux credential; do not map a successful host socket connection to an NT access grant.

## Relationship to existing research

R10/R14/R224 cover generic object and access-mask foundations; R134 covers job UI restrictions; R167 covers delayed clipboard rendering. **This note isolates the window-station/desktop identity, process/thread attachment and display-backend authority boundary.** It neither replaces those notes nor asserts a working GUI subsystem.

## Open questions

- Which documented and version-specific Win32k/user32 operations require kernel-side state, and which can be brokered without breaking thread affinity?
- How should secure desktops, session isolation and remote interactive sessions map to host compositor instances?
- Which Wayland compositor security interfaces, if any, can enforce the necessary separation without requiring a dedicated compositor per guest trust domain?
- How will input capture, hooks and desktop-switch rights be tested without compromising the host session?

## Evidence limitations

Primary documentation linked above; no source checkout or Windows reference machine was available for this note. Links identify contracts to verify during implementation. No code, benchmark, compatibility result or architecture decision is claimed.
