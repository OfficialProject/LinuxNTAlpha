# R261 — Guest volume identity, mount aliases, and filesystem capability reporting

Date: 2026-11-23. Status: **VERIFIED FROM MICROSOFT DOCUMENTATION; DESIGN HYPOTHESIS; NO EXPERIMENT**.

## Distinct question

R60/R89 address path translation and reparse boundaries, R61 file identity, R63 case lookup, and R231 remote-file provider identity. None establishes the **volume-level namespace and capability contract** connecting drive letters, mounted folders, volume GUID paths, volume serial numbers and filesystem flags. A Linux mount, device number or inode cannot simply be published as a Windows volume identity.

## VERIFIED — Microsoft primary API documentation (retrieved 2026-11-23)

1. [GetVolumePathNameW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumepathnamew) returns the volume mount point containing the resolved path. Its documentation explicitly covers nested mounted folders and traversal through junctions; remote paths are treated differently. It also specifies non-obvious behavior for invalid trailing components and unqualified relative paths. This is a *path-to-mount-point* operation, not a host realpath call.
2. [GetVolumeNameForVolumeMountPointW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumenameforvolumemountpointw) returns a volume GUID path of the form `\\\\?\\Volume{GUID}\\` for a drive root or mounted folder; when more than one such name exists, the first in the mount manager cache is returned. The documentation explicitly notes filesystem/remote-provider support limitations.
3. [GetVolumePathNamesForVolumeNameW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumepathnamesforvolumenamew) enumerates drive letters and mounted-folder paths associated with a volume GUID path. Therefore a volume may have **multiple guest path aliases**; assuming one drive letter per volume is unsound.
4. [GetVolumeInformationW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationw) reports volume label, format-assigned volume serial number (not physical-drive manufacturer serial), filesystem name, maximum component length and filesystem capability flags. Its documentation requires a root path ending in a backslash. Capability flags are not safely inferred from a Linux filesystem name; in particular, filesystem compression and volume compression flags are mutually exclusive.
5. [GetVolumeInformationByHandleW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationbyhandlew) obtains information via a file handle, a separate authority/identity path from name-based lookup. This matters when a guest path changes while a handle remains valid.
6. [Creating Mounted Folders](https://learn.microsoft.com/en-us/windows/win32/fileio/mounting-and-dismounting-a-volume) documents multiple mounted-folder aliases and Windows reparse-point implementation. This does **not** imply that an arbitrary Linux bind mount is a Windows reparse point.

These are documented contracts, not measurements on a particular Windows build, and not claims that LinuxNTAlpha implements them.

## HYPOTHESIS — minimal authority model

Maintain an explicit **guest volume registry** with a stable guest volume ID, optional volume GUID path(s), root/mount aliases, provider identity, filesystem-capability record, and generation. Resolve names through the guest DOS-device/reparse namespace before consulting the host filesystem. Associate open guest file objects with a resolved volume identity rather than recomputing identity solely from their current path.

A Linux backend might use `statx` mount IDs, mountinfo, `statfs` and file descriptors as **host observations**, but none of those is the Windows volume GUID, format-assigned serial, guest drive-letter mapping, or Windows capability bitmask. Do not claim a stable host mount ID across remounts or namespace recreation without version-specific evidence. Host metadata should not confer guest access rights or reveal paths outside a confined guest root.

For initial synthetic volumes, expose only capabilities that the guest implementation actually enforces; do not advertise persistent ACLs, case-insensitive search, reparse support or hard links merely because the underlying Linux filesystem has vaguely related features. Keep the registry and file-object identity generation synchronized with guest mount changes.

## PROPOSED Windows differential test matrix — NOT RUN

1. Create two drive/mounted-folder aliases for one test volume. Compare `GetVolumePathNameW`, `GetVolumeNameForVolumeMountPointW`, `GetVolumePathNamesForVolumeNameW` and serial/flags through each alias.
2. Traverse a nested mounted folder and a junction into another volume; compare returned root and GUID path before/after rename or removal. Keep an open file handle and compare `GetVolumeInformationByHandleW` against path-based queries after the path changes.
3. Test root-path trailing slash, missing leaf, invalid intermediate component, relative path, UNC path, insufficient output buffers and NULL optional output pointers. Capture return values and `GetLastError` individually.
4. Capture the entire filesystem flags bitmask and maximum component length for each reference filesystem; separately test effective per-directory case behavior. Never treat one filesystem's reported values as universal.
5. Test mount removal and re-addition with open handles; verify alias lifetime, stale registry generation rejection and whether volume identity remains observable via the handle.
6. For Linux backend comparison, record kernel version, mount namespace, filesystem, host mount identifiers, underlying fd identity and guest registry revision. Run negative tests preventing traversal into unauthorized host mounts.

Windows-reference runs require a recorded OS build, privilege context, filesystem/provider type, exact mount configuration, fixture hashes and raw outputs. Host comparison requires kernel and toolchain versions. **No such runs were performed in this research note.**

## Architectural impact / OPEN

- **OPEN:** exact identity persistence and error codes across remounts, network shares, aliases and volume GUID changes on target Windows builds.
- **OPEN:** which guest filesystem capability flags can be truthfully implemented by an initial Linux-backed provider; advertised support must be tested independently from Linux host support.
- **OPEN:** whether existing R60/R89 guest namespace design can preserve handle-based volume identity across path mutation without introducing a separate authority.
- **Decision:** ADR-003 remains OPEN. No source code imported, benchmark run, compatibility claim or implementation result is asserted.

## Next falsifiable milestone

Build a tiny Windows-reference probe for alias enumeration, nested mount-point resolution, serial/flag consistency and handle-versus-path identity, then design a parse-only guest registry conformance fixture. Do not implement a Linux `st_dev` → Windows volume serial shortcut without the differential evidence above.
