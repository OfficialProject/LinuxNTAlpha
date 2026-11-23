# R259 — NT hard-link creation versus Linux linkat authority and alias safety

Research date: 2026-11-23. Status: **primary-source documentation synthesis only; no experiment executed**.

## Question and relation to prior work

R61 treats hard-link **identity** and inode reuse; R49/R50 treat delete/rename; R60 treats host path resolution. This note isolates the **creation operation**: when can a guest NT file service safely create a new directory entry for an existing guest file using Linux `linkat`, without bypassing guest sharing, volume, path, or authorization rules?

## VERIFIED FROM SOURCE

1. Microsoft documents `CreateHardLinkW(new, existing, NULL)` for files rather than directories, with all links on the same volume. Its documented API supports NTFS; filesystem/provider support is not universal. The new hard link does **not** receive an independent security descriptor; the underlying file's security descriptor applies to every alias. The API also documents a 1023-link limit for this interface. [Microsoft CreateHardLinkW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createhardlinkw); [Hard Links and Junctions](https://learn.microsoft.com/en-us/windows/win32/fileio/hard-links-and-junctions).
2. Microsoft explicitly states that file access and sharing policy applies **per file**, not per name: an incompatible existing open cannot be evaded by creating another hard link. It also documents the unusual case that a symbolic-link source can result in a hard link to the symbolic link itself; this is not license to treat every reparse-point kind identically. [Microsoft CreateHardLinkW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createhardlinkw).
3. Linux `linkat(2)` uses independently supplied source and destination directory descriptors and pathnames. By default it does **not** follow a symbolic link in the source; `AT_SYMLINK_FOLLOW` changes that behavior. `AT_EMPTY_PATH` can link an object referenced by a descriptor, but normally requires `CAP_DAC_READ_SEARCH`; it is **not** an unprivileged general-purpose replacement for guest-handle-based hard-link creation. [Linux link(2)/linkat(2)](https://man7.org/linux/man-pages/man2/link.2.html).
4. Linux reports `EXDEV` when paths are not on the same **mounted filesystem**, including some cases where the underlying filesystem is mounted at two locations. The Linux kernel's protected-hardlinks policy may reject links independently of guest NT rights; Linux also reports errors for read-only filesystems, immutable files, and filesystem limits. [Linux link(2)](https://man7.org/linux/man-pages/man2/link.2.html); [Linux proc_sys_fs(5)](https://man7.org/linux/man-pages/man5/proc_sys_fs.5.html).

## HYPOTHESIS: backend contract

A guest hard-link operation should first resolve both guest paths under the virtual NT namespace and determine their guest volume identities; authorize the requested operation using guest token/security and the guest file-object/share ledger; then attempt a descriptor-relative host operation under a restricted filesystem root. It should publish the new guest name only on confirmed host success. Host success alone must **never** grant guest authorization.

The host's `EXDEV` boundary and guest virtual-volume boundary need not coincide. If one virtual guest volume spans multiple host mounts, direct `linkat` cannot always implement guest semantics; do not silently copy bytes, since copying changes file identity and shared content semantics. Conversely, host paths on one mount may belong to different guest virtual volumes and must not be linked merely because Linux permits it.

A resolver should not turn a checked guest path into an unchecked absolute host pathname between authorization and mutation. Directory-fd anchoring reduces but does not automatically eliminate rename, mount, source-replacement, or symlink races. An operation that requires linking the *exact previously authorized object* needs an explicit identity/race strategy; `AT_EMPTY_PATH` has privilege and lifecycle restrictions. No such strategy has been demonstrated.

## OPEN questions and proposed falsification gates (NOT RUN)

1. **Windows reference**: on a pinned Windows build and NTFS volume, test two names for one file, sharing-denied opens, deletion of one alias, inherited security descriptor visibility, and per-name directory-entry metadata. Record return values and `GetLastError`; distinguish denied creation from denied subsequent opens.
2. **Windows reparse semantics**: separately test ordinary symlink, junction, and other available reparse types as *source*; do not infer from the ordinary symlink remark that all reparse types behave alike.
3. **Guest namespace**: attempt a hard link across guest drives mapped to the same host mount and within one guest drive mapped across different host mounts; compare with the Windows same-volume rule and report unsupported configurations.
4. **Linux host**: in a disposable unprivileged directory, test `linkat` with and without `AT_SYMLINK_FOLLOW`, `AT_EMPTY_PATH` (expect environment-dependent permission results), same/different mounts where available, existing destination, and concurrent rename of source. Record kernel, filesystem, mount namespace, credentials, sysctl and exact errno. No test result is claimed here.
5. **Security/race**: adversarially replace a source path with a symlink or alternate inode after authorization; prove either the intended inode is linked or the operation fails closed. Test host protected-hardlinks restrictions separately from guest ACL denial.
6. **Lifecycle**: with guest handles open across aliases, test that sharing, delete-pending state, identity, and last-link removal remain attached to the guest file object, not whichever pathname was used last.

## Decision gate

Do not equate `CreateHardLinkW` with a bare `linkat` wrapper until guest-volume policy, source identity, share/ACL authority, reparse handling, host restrictions and race behavior have passing differential tests. This note makes **no** claim of conformance, performance advantage, or successful implementation.

Sources inspected 2026-11-23; Microsoft Learn pages are living documentation and are not pinned to a Windows build; Linux man-pages describe API behavior rather than pinning a particular kernel implementation. Future experiments must record exact versions.
