# R230 — DOS-device logon namespaces versus Linux mount namespaces

Status: Documentation research only, 2026-11-21. No Windows reference test, Linux experiment, implementation, or performance result.

## Scope and verified primary-source findings

R10 addresses general object-manager/filesystem boundaries. This note isolates logon-context-specific DOS-device aliases and the risk of mapping them directly to Linux mounts.

1. Windows maintains one global DOS-device directory and multiple local directories. On Windows XP and later, local DOS-device contexts are associated with logon sessions. A thread's current DOS-device context can change during its lifetime. Lookup checks local before global; local names shadow global names. [Microsoft WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/local-and-global-ms-dos-device-names).
2. Microsoft identifies a logon session using the authentication-ID LUID in the access token. Local namespace lifetime ends after the last reference to that authentication ID. LocalSystem has special global-context behavior. QueryDosDevice enumeration and lookup are context-sensitive. [Microsoft, Defining an MS-DOS Device Name](https://learn.microsoft.com/en-us/windows/win32/fileio/defining-an-ms-dos-device-name).
3. DefineDosDevice supports raw target strings, removal, exact-match-on-remove, and suppression of WM_SETTINGCHANGE broadcasts. These are observable mutation semantics, not simply Unix symlink creation. [Microsoft, DefineDosDeviceA](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-definedosdevicea).
4. DOS-device names are object-manager symbolic links (for example, a COM1 alias), not necessarily filesystem symlinks or mounts. Modern WDM drivers generally prefer device-interface registration over well-known DOS names. [Microsoft WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/introduction-to-ms-dos-device-names).
5. Linux mount namespaces isolate mount-point lists, copied at creation and subject to mount-propagation rules. They do not implement Windows authentication-ID-based alias selection or thread-specific DOS-device contexts. [Linux man-pages, mount_namespaces(7)](https://man7.org/linux/man-pages/man7/mount_namespaces.7.html).

## Candidate architecture (inference, NOT experimentally verified)

Maintain a guest-owned global alias map and local maps keyed by stable guest logon authentication identity, not Linux UID, PID, or mount namespace. Select the effective guest thread context at lookup time; resolve local-first, then global; authorize the resulting guest object separately before any Linux backend operation. Host mount namespaces may provide isolation, but cannot be the authoritative guest DOS-device namespace.

Treat guest alias strings, object references, and host filesystem paths as different types. An alias must not grant arbitrary host-path access. Define a generation-safe snapshot/reference protocol for concurrent alias mutation and impersonation. Whether Windows caches resolution across impersonation remains an open question.

## Proposed falsifiable gates (NONE executed)

- On a Windows reference host, create two distinct logon sessions for the same user when authorized; record token AuthenticationId and compare alias visibility with a LocalSystem context.
- Create conflicting local/global aliases and verify shadowing, enumeration, exact-match removal, raw-target behavior, and precise Win32 errors across supported Windows versions.
- Impersonate on one thread while another thread retains its original token; compare before/during/after QueryDosDevice results. Repeat with concurrent redefinition/removal. Do not assume the answer from Linux behavior.
- Distinguish alias changes from shell broadcasts using DDD_NO_BROADCAST_SYSTEM.
- In an isolated Linux model, share one host UID and mount namespace across two guest logon identities; demonstrate isolation, safe concurrent lookup/removal, and denial of alias-based host-path escape.

## Open questions and decision gate

Precise alias mutation ordering, impersonation transitions, creation permissions by Windows version, and open-handle lifetime require Windows differential evidence. No arbitrary Windows driver support is implied. Until conformance tests exist, do not equate the Windows DOS-device namespace with Linux bind mounts or mount namespaces.
