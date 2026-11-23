# R267 — Open-by-file-ID versus Linux export handles: authority and identity gates

Research date: 2026-11-23. Status: **primary-documentation review; no experiment run**.

## Question and nonduplication

R61 treats *querying* identity from an existing file handle, and R259 treats creating hard links. This note addresses the distinct inverse operation: **reopening an object from a previously acquired identifier**, especially when host filesystem capabilities and mount namespaces differ. It does not claim that a Linux inode number is a usable NT file ID.

## VERIFIED FROM SOURCE (documented contracts, not tested behavior)

1. Microsoft `OpenFileById` accepts a volume/share hint handle, a `FILE_ID_DESCRIPTOR`, desired access, sharing mode and flags. The requested access must coexist with other open handles' sharing constraints. This is not merely a pathname lookup: [Microsoft OpenFileById](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-openfilebyid).
2. Microsoft documents `NtCreateFile`'s `FILE_OPEN_BY_FILE_ID` as a filesystem-specific binary 8- or 16-byte reference or object ID, **not necessarily a null-terminated Unicode filename**. NTFS and ReFS differ; FAT, exFAT, UDFS and CDFS are listed as unsupported. In particular, ReFS 8-byte and 16-byte reference numbers must not be treated as interchangeable: [Microsoft NtCreateFile (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntcreatefile).
3. Linux `name_to_handle_at` produces a filesystem-specific variable-size `file_handle` and a mount ID; `open_by_handle_at` uses a mount descriptor and that opaque handle. The documented `open_by_handle_at` failure cases include `EPERM` without `CAP_DAC_READ_SEARCH`, `EOPNOTSUPP` for unsupported filesystem handle encoding, and `ESTALE` for handles that cannot be reopened. Mount IDs are not durable volume identifiers across unmount/remount: [Linux man-pages open_by_handle_at(2)](https://man7.org/linux/man-pages/man2/open_by_handle_at.2.html).
4. Microsoft documents that a file opened relative to a root directory which was itself opened by file ID may produce a **relative** `FileNameInformation` result, rather than the same full-path result as a name-opened root. Reopen provenance therefore affects at least one observable query: [Microsoft NtQueryInformationFile](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/content/ntifs/nf-ntifs-ntqueryinformationfile).

## HYPOTHESES (architecture, not verified)

- Keep a guest-visible **volume-scoped ID registry** distinct from both Linux `st_ino` and opaque export handles. Store the original Windows-style ID bytes and width, guest volume identity, backing object generation/lifetime, and opening provenance. Never expose a raw Linux export handle as a portable Windows ID.
- An unprivileged NT runtime should **not gain host `CAP_DAC_READ_SEARCH` merely to implement `OpenFileById`**. A scoped, explicitly authorized broker or an existing-descriptor/registry approach is a candidate, but neither is yet proven race-free or semantically complete.
- Resolve guest access tokens, NT ACLs, share modes, delete-pending state, virtual-volume scope and object lifetime **before** returning a new guest handle. Host `open_by_handle_at` success alone cannot establish any of these NT conditions.
- A missing, stale, unsupported or ambiguous ID must fail closed with a separately justified NTSTATUS mapping. Do not map every `ESTALE` to a single Windows status without reference tests.

## OPEN questions and falsification tests (NOT RUN)

1. Windows differential test: create an NTFS file and hard link; obtain legacy and 128-bit IDs; open by each documented supported descriptor form; rename, unlink and recreate while holding original handles. Record exact OS build, filesystem and statuses. Repeat ReFS if available; do not assume both widths refer to the same object.
2. Windows authorization test: change desired access and sharing combinations while another handle is open; test delete-pending, cross-volume hint and file-ID-opened directory relative opens; record `FileNameInformation` behavior.
3. Linux unprivileged test: on ext4, XFS and other available filesystems, probe `name_to_handle_at` and `open_by_handle_at` separately. Capture kernel release, mount namespace, filesystem, capabilities, mount ID, returned handle type/length and exact errno. Mark unsupported and denied as SKIP/expected rejection, not success.
4. Security negative test: demonstrate that an identifier originating outside the guest's virtual volume cannot reopen a host object through a privileged broker; test mount replacement, handle reuse, namespace changes and stale IDs.
5. Lifetime test: ensure that a guest handle already opened by ID remains attached to the original object across name changes, while later opens follow documented ID lifetime rules. Test failures after backend object removal and after runtime restart.

## Architectural gate

Do not select Linux `open_by_handle_at` as the universal implementation of Windows open-by-ID. It is a **conditional backend primitive** with a distinct authorization model, filesystem support matrix and identifier format. No compatibility or performance claim follows from API similarity. Any privileged broker proposal requires a separate threat model and an experimentally validated scope restriction.

## Evidence limits

Primary documentation URLs checked 2026-11-23. No Windows-reference machine, Linux capability test, code change, benchmark or syscall trace was executed in this research note. See R61 for identity queries, R60/R243 for path/object namespace resolution, R92/R131 for guest access checks, and R259 for hard-link creation.
