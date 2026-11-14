# R84 — Win32 atom tables: namespace, identity and lifetime gates

Research date: 2026-11-14. **Documentation review only; no Windows reference tests or Linux prototype were executed.** This is a distinct namespace/lifetime topic, not a claim that LinuxNTAlpha already implements atoms.

## VERIFIED — Microsoft API contracts

1. Windows distinguishes **local** atom tables (associated with an application) from the **global** atom table. `AddAtom`/`FindAtom`/`DeleteAtom` and `GlobalAddAtom`/`GlobalFindAtom`/`GlobalDeleteAtom` operate on different scopes. Local tables are destroyed when their application terminates; global string atoms can survive the originating application's termination until their reference counts reach zero. [S1]
2. A string atom is a 16-bit value in **0xC000–0xFFFF**; integer atoms use **0x0001–0xBFFF**, without string-table allocation or reference counts. A decimal `#dddd` input can denote an integer atom. String atom names have a documented 255-byte maximum. [S1, S2]
3. String lookup is case-insensitive while the spelling of the first insertion is retained for name retrieval. Adding an existing string increments its reference count; deleting decrements it and removes the entry at zero. Finding an atom is not an add/reference increment. [S1, S2, S3]
4. `GlobalDeleteAtom` returns zero even for an integer atom; Microsoft documents clearing last error first to distinguish failure from success. Do not infer failure from return value alone. [S4]
5. `InitAtomTable` controls the initial local-table bucket count if invoked before atom use; calling it after initialization does not resize the table. It does not resize the global table. [S5]

## Architectural implications — HYPOTHESES, not selected implementation

- A **per-guest-process local table** plus a **shared global table in an NT service** is a candidate representation. Merely using a Linux process-local hash map for both would fail cross-process visibility and global lifetime requirements. Whether global atom scope must be partitioned by Windows session/window station, and how Native NT `NtAddAtom` relates to the Win32 surface, remain **OPEN**; the Win32 documentation above does not establish those boundaries.
- Allocate 16-bit guest atom identities independently of host pointers/fds. Keep string identity, original spelling, reference count, and lifecycle under a shared synchronization policy. This is a design proposal, not measured performance evidence.
- Atom exhaustion, case-folding behavior outside ASCII, Unicode/ANSI conversion, malformed `#` strings, and atom-ID reuse are **OPEN** until reference tests. Do not assume Linux locale-sensitive case folding matches Windows.

## Proposed falsifiable Windows-reference gates (NOT RUN)

1. Two independent processes add differently cased versions of a unique global name; record identity, preserved spelling, and whether `GlobalFindAtom` changes its lifetime.
2. Add the same name twice, delete once, verify it remains; delete again, verify disappearance. Repeat with process A exiting without deletion and process B querying the global table.
3. Add a local atom in process A and search for it in process B; compare with global behavior. Repeat after A exits.
4. Probe `MAKEINTATOM` boundaries (0, 1, 0xBFFF, 0xC000), `#` decimal strings, overlength names, and empty strings; capture both return value and last error.
5. Compare ANSI and Unicode paths with non-ASCII case variants under a recorded Windows build and locale; verify returned names and lengths.
6. Stress repeated add/delete under threads and processes; verify reference counts indirectly and record identity reuse only as observed, not presumed.

## Security and isolation gates

A shared atom namespace is a cross-process communication surface. Before implementation, define guest isolation, quota/denial-of-service behavior, and the scope of visibility. Avoid exposing host addresses or treating an atom as an authorization token. These are **security hypotheses** requiring an explicit threat model.

## Sources (primary, retrieved 2026-11-14)

- [S1] Microsoft, *About Atom Tables*: https://learn.microsoft.com/en-us/windows/win32/dataxchg/about-atom-tables
- [S2] Microsoft, *GlobalAddAtomW*: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globaladdatomw
- [S3] Microsoft, *GlobalFindAtomW*: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalfindatomw
- [S4] Microsoft, *GlobalDeleteAtom*: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globaldeleteatom
- [S5] Microsoft, *InitAtomTable*: https://learn.microsoft.com/en-us/windows/desktop/api/Winbase/nf-winbase-initatomtable

**Status:** primary-documentation VERIFIED contracts; backend and tests OPEN. No experiment or compatibility claim.
