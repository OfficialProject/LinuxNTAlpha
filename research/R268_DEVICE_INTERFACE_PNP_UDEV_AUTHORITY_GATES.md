# R268 — Windows device-interface PnP notifications versus Linux udev events

Date: 2026-11-23. **Status: primary-source documentation synthesis only. No Windows differential test, Linux device experiment, or implementation performed.** This note addresses user-mode device discovery and event delivery; it does **not** imply support for Windows kernel drivers.

## Verified contracts (documentation)

1. Microsoft specifies `CM_Register_Notification` for callback registration on Windows 8+, with filters for device interfaces, device handles, and device instances. The function **does not enumerate interfaces already present**. Microsoft's documented race-safe discovery order is to register, then call `CM_Get_Device_Interface_List` (or SetupDi enumeration); an arrival between those operations can legitimately appear both in the callback and enumeration. [M1][M2]
2. Interface-arrival and opened-device-handle removal notifications are distinct registrations. For device-handle `DEVICEQUERYREMOVE`, a callback may return `ERROR_CANCELLED` to veto removal, or close the handle to allow removal; `DEVICEQUERYREMOVEFAILED` may follow and requires recovery. The documentation also permits removal-pending without a preceding query-remove. [M1][M2][M3]
3. `RegisterDeviceNotificationW` provides a different Win32 message/service notification route, with `WM_DEVICECHANGE` for windows or `SERVICE_CONTROL_DEVICEEVENT` for service handlers. A generic Linux event subscription does not itself implement either guest delivery mechanism. [M4]
4. Linux `udev` receives kernel uevents for device add/remove/change, manages device-node permissions and symlinks, and exposes event and database access via libudev. Its device properties and Linux names are **host discovery metadata**, not Windows interface-class GUIDs, symbolic-link paths, guest handles, or veto semantics. [L1][L2] The latter non-equivalence is an architectural inference from the separately documented interfaces, not an experimental finding.
5. The Linux kernel's kobject uevent facility emits notifications to userspace; `udevadm monitor` distinguishes kernel events from post-rule udev events. That gives a plausible host observation backend, not a guarantee of Windows callback ordering or query-remove negotiation. [L3][L4]

## Proposed architectural boundary (hypothesis, not selected)

A privileged, policy-constrained **host device observer** could translate permitted udev/sysfs inventory and events into a **guest-owned device-interface registry** keyed by Windows interface class and stable guest instance identity. A guest PnP broker would own registration filters, enumeration snapshots, duplicate-event handling, per-process callback/message routing, registration teardown and device-handle state. Device access would remain a separately authorized host operation; receiving an arrival event must never grant arbitrary access to host `/dev` nodes.

**Crucial limitation:** host Linux hot-unplug is not equivalent to Windows query-remove. A guest veto cannot generally stop physical removal or override host device-manager policy. If Windows-reference applications depend on successful veto, the project must either provide an explicitly limited virtual-device policy or mark that compatibility surface unsupported. Do not silently report veto success.

## Unrun conformance/security gates

- **G1: inventory/subscribe race.** On a version-pinned Windows reference, register then enumerate while attaching a device; capture duplicate arrival, identity and callback ordering. Reproduce the observable contract in a synthetic guest-device test.
- **G2: handle removal state machine.** Open a device interface; record query-remove, veto, query-remove-failed, pending/complete and handle validity. Distinguish orderly eject from surprise removal. Never assume all events occur in every sequence.
- **G3: delivery surfaces.** Compare `CM_Register_Notification` callback delivery with `RegisterDeviceNotificationW` window/service routing, filters, lifetime, and unregister races.
- **G4: guest identity and security.** Two guest processes with different authorization enumerate the same synthetic interface; verify that enumeration visibility and opening rights are independently enforced, and that host `/dev` paths and credentials are not exposed.
- **G5: Linux event reconciliation.** Exercise add/remove/change and rapid replug using a disposable virtual device; reconcile monitor events with a fresh inventory snapshot. Record loss, duplicate and identity behavior without assuming udev event delivery is a durable log.
- **G6: version and environment.** Pin Windows build, host kernel, systemd/libudev version, device class, physical versus virtual device, permissions and timing instrumentation before asserting conformance.

## Decision impact

**Verified:** Windows interface discovery and notification have guest-visible registration, enumeration, identity and removal contracts absent from raw Linux uevents. **Hypothesis:** a guest-owned broker over a constrained host observer is a viable compatibility architecture. **Unresolved:** exact per-device veto support, event-loss recovery, security policy, performance and cross-version fidelity. No architecture decision or benchmark is made here.

## Primary sources

- [M1] Microsoft, `CM_Register_Notification`: https://learn.microsoft.com/en-us/windows/win32/api/cfgmgr32/nf-cfgmgr32-cm_register_notification
- [M2] Microsoft, *Registering for Notification of Device Interface Arrival and Device Removal*: https://learn.microsoft.com/en-us/windows-hardware/drivers/install/registering-for-notification-of-device-interface-arrival-and-device-removal
- [M3] Microsoft, `CM_NOTIFY_ACTION`: https://learn.microsoft.com/en-us/windows/win32/api/cfgmgr32/ne-cfgmgr32-cm_notify_action
- [M4] Microsoft, `RegisterDeviceNotificationW`: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerdevicenotificationw
- [L1] systemd project, `udev(7)`: https://man7.org/linux/man-pages/man7/udev.7.html
- [L2] systemd/libudev, `udev_monitor_new_from_netlink(3)`: https://man7.org/linux/man-pages/man3/udev_monitor_unref.3.html
- [L3] Linux kernel documentation, *Fallback mechanisms* (kobject uevents): https://docs.kernel.org/driver-api/firmware/fallback-mechanisms.html
- [L4] systemd project, `udevadm(8)`: https://man7.org/linux/man-pages/man8/udevadm.8.html

Related research: R10 (object namespace), R14/R92 (guest security), R81 (host credential boundaries), R264 (Win32 session/message boundary), R267 (file-ID/host authority). This note adds PnP-specific discovery and removal semantics rather than repeating those contracts.
