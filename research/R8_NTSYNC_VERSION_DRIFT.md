# R8 — Version-sensitive ntsync ABI names
Date: 2026-11-12

## Primary evidence
- Latest docs: https://www.kernel.org/doc/html/latest/userspace-api/ntsync.html
- Linux v6.18 docs: https://linux.googlesource.com/linux/kernel/git/torvalds/linux/+/refs/tags/v6.18/Documentation/userspace-api/ntsync.rst
- Source: https://github.com/torvalds/linux/blob/master/drivers/misc/ntsync.c

## Finding
The names of certain documented ioctl operations differ across published kernel documentation revisions. For example, historical documentation uses NTSYNC_IOC_MUTEX_KILL and NTSYNC_IOC_EVENT_READ, while v6.18 documentation refers to NTSYNC_IOC_KILL_OWNER and NTSYNC_IOC_READ_EVENT. This means implementation must be keyed to the actual installed UAPI headers and tested against specific kernel versions, not inferred from an unpinned web page.

## Required compatibility practice
- Pin kernel git SHA and UAPI header version for experiments.
- Probe runtime feature availability and gracefully report unsupported operations.
- Keep a versioned adapter for ioctl naming/behavior where required.
- Compare numeric ioctl definitions and structs before assuming an ABI-breaking change; a rename in documentation alone does not prove binary incompatibility.
- Add compile-time tests against the actual kernel headers and runtime behavior tests.

## Status
A documentation discrepancy is verified; binary compatibility impact is OPEN pending source/header diff.
