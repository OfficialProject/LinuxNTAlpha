# R32 — DLL search policy and activation-context resolution gates

Date: 2026-11-12. Status: **primary-documentation review only; no experiment executed**.

## Scope and relationship to existing work

R20 demonstrates a synthetic single-import binding; R29 covers API-set contracts and delay imports. Neither establishes Windows-compatible DLL *location selection* or activation-context redirection. This note isolates those unresolved loader policies rather than repeating import-table mechanics.

## Verified from Microsoft primary documentation

1. **Name resolution is not a simple directory walk.** Windows DLL lookup can first consider DLL redirection, API sets, side-by-side (SxS) manifest redirection, already loaded modules and KnownDLLs before filesystem directories. The actual order depends on packaged/unpackaged application type and flags. [Microsoft DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
2. **Dependencies are resolved separately.** Even when a top-level DLL is loaded by absolute path, its dependencies are searched as module names; copying the top-level directory as a universal dependency root would be incorrect. [Microsoft DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
3. **Unpackaged safe search is conditional.** With SafeDllSearchMode enabled, the current directory appears after the Windows folder; disabling it moves the current directory earlier. `SetDllDirectory` and `LoadLibraryEx` can alter policy. `LOAD_LIBRARY_SEARCH_*` flags define another search strategy, and multiple user-added directories have unspecified relative order. [Microsoft DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
4. **Search policy is version-sensitive.** Microsoft's documented unpackaged order includes the package dependency graph beginning with Windows 11 21H2; it must not be assumed for every Windows target. [Microsoft DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
5. **Activation contexts are more than DLL paths.** Microsoft's activation-context documentation identifies version-specific redirection for DLLs, COM servers, window classes, type libraries and interfaces. Application manifests may populate activation contexts during process creation. [Microsoft activation contexts](https://learn.microsoft.com/en-us/windows/win32/sbscs/activation-contexts).

## Architectural implications (reasoned hypotheses, not measured results)

- **HYPOTHESIS:** A loader resolver should accept an explicit target-Windows-version profile, caller/load flags, module identity, active activation context, loaded-module registry and a virtualized Windows path namespace. Keeping these inputs explicit may prevent hard-coding a Linux `LD_LIBRARY_PATH`-style algorithm.
- **HYPOTHESIS:** Windows-style case-insensitive module identity and path canonicalization need their own compatibility layer; blindly using Linux pathname lookup could produce both wrong bindings and unsafe path traversal. Precise edge-case semantics remain OPEN.
- **HYPOTHESIS:** A read-only deterministic resolution engine, separated from actual mapping/execution, is the safest first implementation and permits Windows-reference comparisons.
- **OPEN:** KnownDLLs object/section identity, activation-context stack lifetime, DLL redirection details, reparse-point/path normalization, loader-lock interactions, package graph modeling and error/status equivalence need separate primary-source reviews and tests.

## Proposed reproducible gates (NOT performed)

1. Create a pure resolver with a fake filesystem, explicit process/module tables and a selectable version profile. Test precedence collisions: API-set versus physical name, loaded module versus directory, SxS redirect versus app directory, and KnownDLL versus attacker-controlled current directory.
2. Use a Windows reference VM with documented OS build and architecture to record `LoadLibraryEx` outcomes for duplicate DLL names in application/system/current/added directories, safe-mode variants and flag combinations. Record loaded *absolute module paths*, errors, and all process settings. Avoid assuming unspecified ordering of multiple user directories.
3. Test transitive dependency lookup when the parent DLL is loaded via absolute path; verify that the dependency is not automatically resolved relative to that path absent relevant flags.
4. Add manifest-based activation-context tests, including switching contexts in one thread; verify DLL redirection and, separately, non-DLL versioned objects. Distinguish expected behavior from Windows-version-dependent observations.
5. Reject ambiguous or escaping virtual paths before mapping PE pages; compare loader behavior and security outcomes independently.

## Decision impact

**Do not** select a permanent DLL resolution algorithm or assert application compatibility from R20/R29 alone. First implement parse-only policy tests and Windows-reference conformance cases. This work does not justify kernel modifications, Windows compatibility claims, or performance estimates.

## Sources (accessed 2026-11-12)

- Microsoft Learn, *Dynamic-link library search order*, https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order (page shows last updated 2023-02-09).
- Microsoft Learn, *Activation Contexts*, https://learn.microsoft.com/en-us/windows/win32/sbscs/activation-contexts (page shows last updated 2021-01-07).
