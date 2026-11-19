# R173 — DLL search policy, module identity and host-path containment gates

Status: **VERIFIED FROM SOURCE (Microsoft documentation reviewed 2026-11-18); HYPOTHESIS for LinuxNTAlpha architecture; no experiment executed**.

## Architectural question

A PE loader that can resolve import descriptors still cannot safely or correctly choose *which* module supplies an import. Should LinuxNTAlpha map DLL names directly onto Linux directories, or implement a versioned guest loader search policy and separate guest module identity from host file descriptors?

**Conclusion (provisional):** implement a policy-driven guest resolver, not a generic host PATH/dlopen lookup. This is a design hypothesis, not a proven implementation.

## Verified primary-source findings

1. Microsoft documents that DLL resolution is influenced *before ordinary filesystem search* by DLL redirection, API sets, SxS manifest redirection, the already-loaded module list and Known DLLs. For unpackaged applications with safe DLL search mode, the executable directory precedes system directories; current directory and PATH occur later. Windows 11 21H2 and later add the package dependency graph in the documented order. The exact policy depends on app packaging, Windows version and search-mode configuration. [Microsoft: DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
2. The same documentation says dependencies are searched by their *module names* even when the initial DLL was loaded by a full path. Therefore, propagating the parent's full-path directory to every transitive import is not the default rule. [Microsoft: DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
3. `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` requires a fully qualified target path and temporarily adds its containing directory for dependencies; `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` excludes ordinary standard-path directories. `LOAD_WITH_ALTERED_SEARCH_PATH` has distinct semantics and cannot be combined with the documented `LOAD_LIBRARY_SEARCH_*` flags. [Microsoft: LoadLibraryExW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw).
4. `LoadLibraryExW` documents that a bare name matching multiple already-loaded modules with the same base name and extension returns the module loaded first; explicit full paths and relative paths follow different rules. A missing dependency causes the load to fail. [Microsoft: LoadLibraryExW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw).
5. Data-file and image-resource loading flags do not perform normal executable-module initialization and have different API eligibility from executable DLL loads. A resolver must not equate every successful mapping with an initialized callable module. [Microsoft: LoadLibraryExW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw).
6. Microsoft describes application-specific `.local` redirection and manifest/SxS alternatives, including differences between packaged and unpackaged apps. It is not safe to model all redirection as a single executable-directory override. [Microsoft: DLL redirection](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-redirection).

Sources are living Microsoft Learn pages, **not pinned Windows implementation revisions**. Their documentation describes version-dependent behavior, not a claim of identical results across every Windows build.

## Implications and proposed design (HYPOTHESIS)

Define a **guest resolution request** with: caller process identity, target Windows version/profile, package status, requested name/path, load flags, activation context, guest application directory, process DLL-directory policy and current module graph. Return either a specific guest module identity and a *confined* host file reference, or a deterministic guest error.

- Resolve virtual contracts (API sets), SxS/redirection, loaded modules and Known DLL policy *before* ordinary candidate directory traversal, using explicit precedence tables for each supported profile.
- Keep guest module identity, normalized guest path, canonical host backing identity, load mode, reference ownership and initialization state distinct. Do not infer module equality from Linux inode alone or from case-folded basename alone.
- For filesystem candidates, resolve only within explicitly configured guest roots, with fd-relative traversal and host escape prevention. A string prefix check or direct `dlopen` is not a sufficient boundary. R60 and R147 already cover reparse/path escape mechanics; this note concerns **loader candidate precedence and identity**, not their replacement.
- Preserve transaction rollback across dependency failures and do not publish partially initialized modules as successful results. R64/R91 cover dependency graph and loader-lock lifetime separately.
- Treat Linux `LD_LIBRARY_PATH`, current working directory and host `PATH` as **non-authoritative** for guest DLL resolution. Guest PATH is a separately modeled input, not an implicit host search path.
- Reject unsupported version/profile/search flags explicitly rather than silently guessing a Windows policy.

## Falsifiable tests (NOT RUN)

1. **Windows-reference precedence matrix:** build self-authored DLLs with distinct exported constants in executable, current, user-added and system-equivalent directories. For each supported Windows build/profile, record chosen module path for bare-name import, explicit full-path load, altered search path, `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR`, and `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`. Avoid writing into protected Windows system directories; use controlled directory candidates and documented API flags.
2. **Transitive dependency case:** place A.dll at an absolute path, B.dll in its directory and another B.dll in the app directory. Compare default loading with explicit DLL-load-directory flags. Verify actual loaded paths rather than merely checking success.
3. **Identity collision:** load two distinct same-basename DLLs by absolute paths; query bare-name resolution and record which module is returned, plus unload/reload effects.
4. **Failure rollback:** inject missing dependency, invalid PE and failing initialization; assert no callable half-loaded module remains and reference counts/handles are balanced.
5. **Host containment:** construct symlink and path traversal candidates under guest roots; verify candidate selection never opens arbitrary host paths, even under rename races. Run only with disposable fixtures and no elevated permissions.
6. **Profile drift:** run the same test matrix on multiple Windows builds and document any version-sensitive differences rather than treating the current Learn page as timeless.

## Decision gate

**OPEN:** No DLL search policy has been implemented or benchmarked here. A minimum integration milestone may initially support a *strict, documented subset* (e.g., explicit allowlisted runtime modules), but must not advertise general Windows DLL search compatibility. Prioritize an executable test of import resolution with a synthetic two-directory collision before expanding API breadth.

## Nonduplication

R29/R163 concern API-set and delay-load semantics; R45/R64/R91 concern loader graph, locking and teardown; R60/R147 concern guest path traversal; R115 concerns integrated runtime milestones. This note isolates **versioned DLL search precedence, transitive dependency search and guest module identity**, which those topics do not settle.

No LinuxNTAlpha experiment was executed; no Windows-reference behavior was observed directly.