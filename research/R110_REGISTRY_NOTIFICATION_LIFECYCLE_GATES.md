# R110 — Registry change notifications: guest key lifecycle versus host filesystem events

Status: **VERIFIED FROM PRIMARY DOCUMENTATION** for the API contracts below; **HYPOTHESIS** for implementation mapping; **NO EXPERIMENT**. Reviewed 2026-11-15.

## Architectural question

Can an NT-compatible registry service implement `RegNotifyChangeKeyValue` / `NtNotifyChangeKey` faithfully using Linux inotify on a hive backing file? **Not by simply forwarding inotify events.** The guest contract is expressed in registry key handles, access rights, subtree scope, change classes, thread lifetime and NT event/APC completion. Inotify is an inode/filesystem-event interface with different identity and queue semantics.

## Verified source contracts

1. Microsoft documents that `RegNotifyChangeKeyValue` requires an opened key with `KEY_NOTIFY`, offers key-only or subtree watching, and filters name, attributes, last-set and security changes. Synchronous mode blocks; asynchronous mode signals a caller-supplied event. A successful registration detects **one change**, after which the caller re-registers. Closing the key signals the event. Unless `REG_NOTIFY_THREAD_AGNOSTIC` is set (Windows 8+), caller-thread termination also signals the event. Reusing a key handle with different subtree/filter settings can succeed while ignoring the changed settings. Source: [Microsoft Win32 RegNotifyChangeKeyValue](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue).
2. Microsoft documents `ZwNotifyChangeKey` (called `NtNotifyChangeKey` from user mode) with `STATUS_PENDING` for asynchronous pending operations, optional event/APC completion, key-handle signaling when no event is provided, and first-registration filter/subtree behavior. It requires `KEY_NOTIFY` and ends the notification session on key closure. This is a kernel-driver DDI description; exact user-mode NT behavior still needs differential verification. Source: [Microsoft ZwNotifyChangeKey](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-zwnotifychangekey).
3. Linux `inotify(7)` documents that its queue can overflow (`IN_Q_OVERFLOW`), events may be lost, watch identities refer to filesystem objects, and watch/resource limits apply. It does not define Windows registry key access rights or NT event/APC completion. Source: [Linux inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html).
4. Microsoft separately documents `NtNotifyChangeMultipleKeys`; it must not be silently treated as identical to the single-key API without a distinct contract review. Source: [Microsoft NtNotifyChangeMultipleKeys](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntnotifychangemultiplekeys).

## Hypothesis: implementation boundary

A registry service should maintain **logical key objects and notification registrations** independent of hive-file storage. Registry mutations would publish notifications as part of a serialized logical mutation path; an inotify watcher, if used at all, would serve only as a hint that an *external host writer* changed backing storage. Treating every hive-file write as a matching guest key change would create false notifications; relying exclusively on filesystem events risks missing logical mutations or conflating distinct keys.

Proposed registration state: key-object identity; granted access; subtree and filter captured at initial registration; registering guest thread identity and thread-agnostic flag; event/APC/handle completion target; pending/completed/cancelled state; key-handle lifetime. The guest service must coordinate this state with the NT object/dispatcher service; it cannot assume a Linux file descriptor is a guest event handle.

This is a design candidate, **not** a demonstrated implementation or measured performance claim.

## Falsification / conformance plan (not executed)

On a controlled Windows reference VM, with a fresh disposable registry subtree:

1. Verify denied `KEY_NOTIFY` rights; distinguish invalid handle from insufficient rights.
2. Register each filter and subtree setting; mutate direct key, child key, descendant key, value and security descriptor; capture exact signals and statuses.
3. Register asynchronous notification, then close the key; separately terminate the registering thread with and without `REG_NOTIFY_THREAD_AGNOSTIC`. Observe event state and whether a second registration is required.
4. Exercise the same key handle with changed filter/subtree settings; compare with a fresh handle.
5. Test event reset and rapid consecutive mutations around re-registration; do not assume a count of one event per mutation.
6. For the native API, separately verify key-handle signaling, event completion and APC callback/alertable-wait behavior.
7. Stress host backing-file replacement, rename and inotify queue overflow against a prototype registry broker; verify the guest does not leak host path identities or silently lose authoritative in-process changes.
8. Record Windows build, registry view (32/64-bit), privileges, precise call traces, return codes and host kernel version before claiming reproduction.

## Open questions

- Native API cancellation and completion status on handle closure, thread exit and hive unload, including Win32/native translation.
- Notification semantics for registry virtualization, WOW64 registry views, symbolic links and transactional operations.
- Correct ordering with NT event signaling, APC delivery and concurrent registry writes.
- Cross-process security and isolation when multiple guest processes share a registry broker.

## Decision impact

Do **not** choose inotify as the authoritative registry notification engine. Keep logical registry mutation/notification ownership inside the NT registry service until Windows-reference tests can validate the contract. No kernel modification is justified by the evidence here.
