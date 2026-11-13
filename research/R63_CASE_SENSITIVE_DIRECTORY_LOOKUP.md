# R63 — Case-sensitive directory lookup versus Linux pathname resolution

Evidence class: **primary-documentation review only**. Reviewed 2026-11-13. **No Windows reference run, Linux prototype, or performance measurement.**

## Verified contracts

1. Windows file-name lookup is normally case-insensitive while retaining spelling, but supported Windows/NTFS directories may be marked case-sensitive. Microsoft's WSL case-sensitivity guidance documents per-directory configuration, inheritance (with a WSL 1 exception), and the danger of applications assuming case-insensitivity. This is not a blanket claim about every Windows filesystem. [Microsoft WSL case sensitivity](https://learn.microsoft.com/en-us/windows/wsl/case-sensitivity).
2. The documented `FILE_CASE_SENSITIVE_INFORMATION` structure exposes `FILE_CS_FLAG_CASE_SENSITIVE_DIR = 0x1`. `NtQueryInformationFile` documents `FileCaseSensitiveInformation` (information class 71) with a read-attributes access requirement and availability from Windows 10 version 1803. [Microsoft structure](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_case_sensitive_information); [Microsoft query API](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/content/ntifs/nf-ntifs-ntqueryinformationfile).
3. Linux pathname lookup normally distinguishes differently cased byte strings; `openat2` provides scoped path-resolution restrictions, **not** a Windows-style per-directory case-insensitive lookup option. Its `RESOLVE_BENEATH` and `RESOLVE_IN_ROOT` facilities address traversal boundaries, not case folding. [Linux openat2(2)](https://man7.org/linux/man-pages/man2/openat2.2.html); [Microsoft WSL case sensitivity](https://learn.microsoft.com/en-us/windows/wsl/case-sensitivity).

## Architectural consequence (inference, not implemented)

A naïve NT-to-Linux adapter that passes an NT pathname straight to `openat` will fail ordinary Windows-style case-insensitive opens on case-sensitive host filesystems. Conversely, lowercasing every component is incorrect: it loses preserved spelling, does not model case-sensitive directories, and can select an incorrect object when the host contains case-colliding names.

**Candidate**: implement NT-visible directory policy and component resolution above Linux VFS; keep opened directory handles and use fd-relative operations. The NT namespace policy must remain separate from Linux host-path security policy. The resolver must never silently select an arbitrary member of a case-collision set. This is a proposed design, **not** evidence that a race-free or fully compatible implementation exists.

**Security gate**: a userspace directory scan followed by a separate open is subject to rename/replacement races. A successful `openat2` containment check on a chosen spelling does not prove that the spelling was the correct NT case-fold match. Any resolver must bind selection and use to validated identities or reject ambiguous/racy cases; the precise strategy remains OPEN.

## Windows-reference differential matrix (proposed; not run)

- In an ordinary NTFS directory, create `Alpha.txt`, then query/open `alpha.txt`, `ALPHA.TXT` and the original; record returned spelling, identity and status.
- Repeat under a directory whose case-sensitive flag is enabled; test creation and lookup of two names differing only in case, including mixed-case parent components.
- Query the directory flag via the documented information class, with and without sufficient access rights.
- Probe rename and replacement while resolving a nested path; record identity rather than merely successful open.
- Probe non-ASCII case pairs under a recorded Windows build and filesystem; **do not assume** Unicode default casefold or Linux ext4 casefold matches Windows upcase tables.
- Repeat on a Linux host with a case-sensitive filesystem; test hostile symlinks, mount boundaries, concurrent renames and duplicate-case entries. Distinguish semantic failures from containment failures.

## Decision gate

Do not commit to host filesystem casefold, directory scanning, or an NT virtual directory index until a Windows-reference oracle establishes the desired lookup/ambiguity behavior and a Linux adversarial test demonstrates identity-preserving, confinement-safe resolution. This affects R10, R49–R50 and R60–R61, but does not replace their share-mode, rename, reparse or identity models.

**Open questions**: exact NT `OBJ_CASE_INSENSITIVE` interaction with filesystem directory flags; case-fold tables across filesystems and Windows versions; collision status and behavior; race-free backend selection; cost under large directories. None is resolved by the cited sources alone.
