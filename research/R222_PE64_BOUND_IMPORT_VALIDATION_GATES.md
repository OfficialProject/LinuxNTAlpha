# R222 — PE32+ bound imports: validation, fallback and forwarded dependencies

Status: RESEARCH ONLY (2026-11-21). No fixture, Windows reference run, Linux execution, benchmark or architecture decision is claimed.

## Scope and relation to existing work

R221 covers **delay-load** helper resolution and unload; this note instead covers the ordinary import directory's **pre-bound** IAT, the optional bound-import directory, and invalidation of precomputed addresses. R194 covers process bootstrap generally. This narrower issue matters even if the first native PE implementation resolves all ordinary imports by name/ordinal.

## Verified primary-source findings

1. The Microsoft PE/COFF specification defines each ordinary import-directory entry's import lookup table (ILT), time/date stamp, forwarder-chain field, DLL name RVA, and import address table (IAT). Before binding, IAT contents match ILT contents; after binding, IAT entries can contain bound addresses. The timestamp field is initially zero and is set to the imported DLL timestamp when bound. For PE32+ the lookup entries are 64-bit, with the high bit selecting ordinal import. [S1]
2. Microsoft's archived *Inside Windows* PE analysis describes the optional `IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT` list: `IMAGE_BOUND_IMPORT_DESCRIPTOR` holds a DLL timestamp, a module-name **offset relative to the first bound-import descriptor** (not an RVA), and a count of immediately following forwarder-reference records. A forwarded dependency can therefore require validation of more than the initially named DLL. This is Microsoft-hosted historical engineering documentation, **not** a guarantee that current Windows versions use precisely the same loader fast path. [S2]
3. Microsoft documents a distinct optional **delay-bound IAT** and timestamp in the *delay-import* directory. That mechanism belongs to R221 and must not be conflated with ordinary bound imports. [S1]

## Candidate implementation boundary (HYPOTHESIS; not selected)

- Parse and validate the ordinary import descriptors and ILT independently of any pre-bound IAT values. Treat bound-import metadata as advisory optimization data, never an authority to execute arbitrary embedded addresses.
- Resolve guest DLL identity, API-set/forwarder resolution and export lookup through the guest loader's own module namespace. Do **not** assume a timestamp match makes a host pointer valid: native Linux mappings, guest ASLR, version-specific export tables and runtime-generated stubs can differ from a pre-binding environment.
- For an initial correctness-first implementation, **ignore pre-bound addresses** and rebuild the guest IAT from verified name/ordinal lookup. A later fast path would require an explicit validity proof covering target module identity, load address, export and forwarded-dependency identity, and memory permissions. This is a proposed design, not a documented Windows rule.
- Keep parsing, module identity, IAT publication and rollback separate. Reject out-of-range module-name offsets, unterminated strings, truncated descriptor/forwarder sequences, arithmetic overflow and IAT/ILT spans outside validated mapped image regions. Bounds checks are security requirements proposed for this implementation, not claims about exact Windows malformed-image outcomes.

## Open questions (do not infer answers)

1. On specific supported Windows builds, does the loader consult the bound-import directory, the ordinary import-descriptor timestamp, both, or neither for each image class?
2. How do modern API-set redirection, KnownDLL selection, forwarded exports and rebasing affect the decision to accept a bound IAT?
3. Which malformed bound-import cases fail image loading versus trigger safe fallback? What are the exact error/status and loader-state rollback semantics?
4. How do timestamp collisions or reproducible-build timestamps interact with binding validity? Timestamp equality alone is not proven to be sufficient.
5. Are ordinary bound imports emitted by contemporary x64 toolchains under reproducible build configurations, and is the optimization worth implementing after correctness?

## Proposed gates (NOT RUN)

**Parse-only:** construct PE32+ fixtures with valid/invalid bound directory bounds, zero/large forwarder counts, module-name offset outside directory, missing terminator, ILT/IAT length mismatch and arithmetic overflow. Assert deterministic rejection or conservative fallback without dereferencing bound addresses.

**Windows reference:** build unsigned, locally controlled test DLL/EXE pairs with ordinary imports, an export forwarder, changed DLL timestamp, rebased DLL, and identical timestamp with changed export layout. Record exact Windows build, linker flags, PE headers, loaded module bases, resolved IAT values, failures and repeatability. Avoid assuming that a build tool actually emits binding: inspect the resulting image first.

**Linux candidate:** run the same validated images against a guest namespace and verify no IAT entry escapes authorized executable guest mappings; verify failure rollback and concurrent-load publication. Compare results with the Windows reference before claiming parity.

## Sources and provenance

- [S1] Microsoft, *PE Format*, especially “Import Directory Table,” “Import Lookup Table,” “Import Address Table,” and delay-load sections. https://learn.microsoft.com/en-us/windows/win32/debug/pe-format (consulted 2026-11-21).
- [S2] Matt Pietrek, *Inside Windows: An In-Depth Look into the Win32 Portable Executable File Format, Part 2*, Microsoft MSDN Magazine archive (March 2002), discussion of `IMAGE_BOUND_IMPORT_DESCRIPTOR` and `IMAGE_BOUND_FORWARDER_REF`. https://learn.microsoft.com/en-us/archive/msdn-magazine/2002/march/inside-windows-an-in-depth-look-into-the-win32-portable-executable-file-format-part-2 (consulted 2026-11-21).

**Evidence limit:** the cited material establishes format descriptions and historical binding rationale, not current Windows loader internals or measured Linux equivalence.
