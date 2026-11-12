# R38 — Registry namespace, WOW64 views, and notification gates (2026-11-12)

**Status:** primary-documentation research; no Windows reference execution, Linux prototype, or benchmark. **VERIFIED FROM SOURCE** means documented by Microsoft, not observed by LinuxNTAlpha. This is a new registry-specific gate, separate from R10's filesystem/object-name work and R34's environment handling.

## Verified Windows-visible constraints

1. **The registry is not a single host directory tree.** Microsoft documents predefined roots and registry key handles, with access permissions enforced on key operations. A Linux file path or SQLite row is at most a storage backend, not the Windows key/handle contract. [M1, M2]
2. **WOW64 offers logical views, not a blanket duplicated tree.** Selected keys are redirected, others shared; explicit `KEY_WOW64_32KEY` and `KEY_WOW64_64KEY` request alternate views. `Wow6432Node` is a reserved physical implementation location and should not be hardcoded by applications. [M3, M4]
3. **Do not implement obsolete reflection as a default modern behavior.** Microsoft says registry reflection ended with Windows 7 / Windows Server 2008 R2; its older synchronization behavior is historical, not a current cross-view invariant. [M4, M5]
4. **Change notifications have a handle and lifecycle contract.** `RegNotifyChangeKeyValue` observes selected classes of key/subtree changes, detects a single change per registration, and must be rearmed after notification. Closing the watched key signals the event. The function rejects remote handles; `REG_NOTIFY_THREAD_AGNOSTIC` was introduced in Windows 8 for thread-pool use. [M6]
5. **Key access is not equivalent to filesystem ownership.** Registry key security descriptors and requested access rights must be checked under Windows token semantics; Linux UID/mode bits alone cannot establish Windows authorization. [M2]

## Architectural consequences — hypotheses only

- **HYPOTHESIS:** represent a registry key as a securable NT-managed object with a stable logical identity, per-open handle rights, and separately versioned value/subkey state. Store durable data behind an adapter; avoid exposing raw host storage paths to guests.
- **HYPOTHESIS:** resolve predefined roots, current-user identity, WOW64 view, and per-key sharing/redirection *before* backend lookup. A hardcoded prefix replacement of all `HKLM\\SOFTWARE` accesses is incorrect because Microsoft's table includes shared descendants.
- **HYPOTHESIS:** deliver notifications through a registry service event queue integrated with the NT wait/event subsystem. Linux inotify on backend files would not inherently model subtree filters, single-change rearming, security checks, or close signaling.
- **OPEN:** precise `NtCreateKey` / `NtOpenKey` / `NtQueryValueKey` / `NtNotifyChangeKey` semantics, key deletion while handles remain open, symbolic-link keys, hive load/unload, persistence/flush ordering, HKCR merge precedence, transactions, virtualization, and remote-registry behavior. These require further source audits and Windows reference observations.
- **OPEN:** x86 WOW64 support is outside the currently executed x64 PE milestone; the logical view resolver should be designed/tested separately rather than claiming x86 process compatibility.

## Falsifiable conformance plan (NOT EXECUTED)

| Gate | Windows reference procedure | Candidate invariant |
|---|---|---|
| View selection | 32/64-bit clients open same redirected key; repeat with both KEY_WOW64 flags | View resolution precedes storage lookup |
| Shared exceptions | Read/write documented shared descendants from both views | No accidental duplicate state |
| Access check | Create key with restrictive DACL; open with allowed/denied masks and different tokens | Handle rights are enforced independently of host UID |
| Enumeration | Insert mixed-case subkeys/values; enumerate with small buffers | Case, ordering and buffer/error behavior match measured reference |
| Notification | Register key/subtree filters, mutate child/value, close watched handle, rearm | Event state and close behavior match reference |
| Lifecycle | Open two handles, delete key, query via old handle, reopen by name | Lifetime and visibility match reference |
| Persistence | Modify and flush, restart isolated service, inspect durable state | No silent loss beyond documented durability model |

Capture Windows version/build, process architecture, exact access masks, API return codes, buffer lengths, notification ordering, and repetitions. Compare the Linux candidate only after the reference results are captured. Never import a production user's registry hive for exploratory testing.

## Primary sources (reviewed 2026-11-12)

- [M1] Microsoft, *Predefined Keys*: https://learn.microsoft.com/en-us/windows/win32/sysinfo/predefined-keys
- [M2] Microsoft, *Registry Key Security and Access Rights*: https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-key-security-and-access-rights
- [M3] Microsoft, *32-bit and 64-bit Application Data in the Registry*: https://learn.microsoft.com/en-us/windows/desktop/sysinfo/32-bit-and-64-bit-application-data-in-the-registry
- [M4] Microsoft, *Registry Redirector*: https://learn.microsoft.com/en-us/windows/win32/winprog64/registry-redirector
- [M5] Microsoft, *Registry Keys Affected by WOW64*: https://learn.microsoft.com/en-us/windows/win32/winprog64/shared-registry-keys
- [M6] Microsoft, *RegNotifyChangeKeyValue*: https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue

**Boundary:** no registry implementation, measured performance, or Windows application compatibility is claimed.
