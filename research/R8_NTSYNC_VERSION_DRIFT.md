# R8 — Corrected ntsync documentation/header discrepancy

Date: 2026-11-12. **Correction to initial R8 claim** after inspecting pinned upstream source files.

## What was previously claimed
The initial note inferred that ioctl macro names in ntsync varied by published kernel revision and that a versioned ioctl adapter might be required. That was **not established** by documentation alone. In fact, even the v6.18 source documentation contains internally inconsistent names.

## Verified upstream header and source audit (2026-11-12)

Retrieved directly from torvalds/linux through the GitHub repository API, pinning both path and ref.

| File | v6.18 blob SHA | v6.19 blob SHA | master blob SHA |
|---|---|---|---|
| include/uapi/linux/ntsync.h | 6d06793512b150a80018618b698537a0e3793e54 | 6d06793512b150a80018618b698537a0e3793e54 | 6d06793512b150a80018618b698537a0e3793e54 |
| drivers/misc/ntsync.c | 999026a1ae0485763da7dbe8f263f83ca2fd01e5 | 9087f045e362737f81bc21f050306ddff35f54be | 2857ae37d3c81c67f36080e182b12e66bd08b3fc |

**CONFIRMED:** the entire ntsync UAPI header is byte-identical across the three inspected revisions (equal git blob SHA). It defines NTSYNC_IOC_MUTEX_KILL (ioctl number 0x86) and NTSYNC_IOC_EVENT_READ (0x8d), not NTSYNC_IOC_KILL_OWNER or NTSYNC_IOC_READ_EVENT. The corresponding ioctl switch in driver source at all three revisions uses the header's names.

The v6.18 and v6.19 Documentation/userspace-api/ntsync.rst files share blob SHA 25e7c4aef96813c297515a6a3d10a583bab849c7. Within that document, an introductory paragraph says NTSYNC_IOC_MUTEX_KILL, while ioctl-reference macro headings use NTSYNC_IOC_KILL_OWNER and NTSYNC_IOC_READ_EVENT. These headings conflict with their **same-version UAPI header and implementation**. This appears to be a documentation naming error, **not** ABI drift.

References:
- https://github.com/torvalds/linux/blob/v6.18/include/uapi/linux/ntsync.h
- https://github.com/torvalds/linux/blob/v6.19/include/uapi/linux/ntsync.h
- https://github.com/torvalds/linux/blob/master/include/uapi/linux/ntsync.h
- https://github.com/torvalds/linux/blob/v6.18/drivers/misc/ntsync.c
- https://github.com/torvalds/linux/blob/v6.18/Documentation/userspace-api/ntsync.rst

## Corrected engineering guidance
- Use pinned **UAPI headers and driver source**, not reStructuredText headings, as authority for ioctl macro names, numbers and structure declarations.
- Do not add a versioned compatibility adapter for these two names based on the documentation discrepancy; no relevant ABI variation was found in the three audited refs.
- Still verify runtime kernel/module support, ioctl return behavior and distro backports. Identical headers do not imply identical internal behavior or module availability.
- Consider submitting a narrowly scoped upstream documentation fix; do not change the LinuxNTAlpha ioctl code to reflect incorrect documentation names.
- Add compile-time offset/size assertions and runtime tests before using ntsync. **No runtime ntsync test was run here.**

## Remaining uncertainty
Tag refs may not expose out-of-tree distro patches or future changes. This is a pinned source comparison, not an assertion that any version ever released is ABI-identical.
