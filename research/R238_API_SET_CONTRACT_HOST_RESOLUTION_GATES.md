# R238 — Windows API-set contracts versus PE import host resolution

Date: 2026-11-22. Status: **documentation and architecture analysis only**. No Windows reference run, Linux fixture, performance measurement, or compatibility result is claimed.

## Scope and relationship to existing work

R32 addresses DLL search and activation contexts; R194 addresses PE64 bootstrap publication; R221 and R222 address delay and bound imports. This note isolates an additional loader boundary: **an imported API-set contract name is not necessarily the physical DLL whose image should be opened**. Treating all import descriptor strings as filesystem paths is not a valid general loader architecture.

## Primary sources and evidence levels

1. **VERIFIED (documented Windows design):** Microsoft, *Windows API sets*, https://learn.microsoft.com/en-us/windows/win32/apiindex/windows-apisets — API sets are virtual aliases for DLL exports, decoupling API contracts from implementation DLLs. The implementation DLL can differ by Windows device family and version; API-set names should not be assumed to identify on-disk files.
2. **VERIFIED (PE representation):** Microsoft, *PE Format*, import directory / import lookup table / import address table sections, https://learn.microsoft.com/en-us/windows/win32/debug/pe-format — the image records DLL-name imports and symbol lookup data, and the loader resolves IAT entries. The PE import record alone does not define the host DLL for an API-set contract.
3. **VERIFIED (Windows loader interface):** Microsoft, *LoadLibraryExW*, https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw ; *GetProcAddress*, https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress — module loading and export lookup have observable success/failure contracts, and are not merely PE string rewriting.
4. **VERIFIED (Windows loader guidance):** Microsoft, *Dynamic-link library search order*, https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order — API-set redirection appears as a distinct loader-resolution step. Search order, redirection, and loaded-module identity must be modeled separately.
5. **VERIFIED (host boundary):** Linux man-pages project, *dlopen(3)*, https://man7.org/linux/man-pages/man3/dlopen.3.html — ELF dynamic-loader path and symbol resolution are Linux contracts, not a native implementation of Windows API-set redirection.

Sources above identify normative documentation pages; this review did not record a specific upstream page revision or independently run the referenced APIs. Any exact Windows build-specific mapping is **OPEN** until independently captured.

## Architecture implications (HYPOTHESIS / proposed design)

A guest loader should maintain separate identities for (a) the requested import contract, (b) the selected guest-visible implementation module, (c) the physical backing image or independently implemented runtime module, and (d) the exported symbol/ordinal ultimately bound. A mapping table should be selected by a declared **guest Windows compatibility profile**, not inferred from the Linux distribution or the physical presence of a DLL-shaped file. A resolver must not silently fall back to Linux `dlopen` for an API-set name.

Suggested resolution pipeline: parse bounded import descriptors → classify contract versus ordinary module name → resolve a versioned guest API-set namespace → apply guest loader policy and module lifetime rules → resolve forwarders and exports → publish IAT only after dependencies validate. This is a **candidate**, not an experimentally established Windows loader ordering. Delay-load imports must use the same guest namespace when first resolved, while keeping R221's helper and unload rules.

Security concern: an attacker-controlled file named like an API-set contract must not automatically override the guest contract mapping. Conversely, a compatibility layer must not claim the Microsoft API-set namespace is a security boundary in its own right; module trust and code integrity are separate checks (see R182).

## Falsifiable conformance gates (NOT EXECUTED)

- **Windows reference:** for pinned Windows builds and architectures, compile minimal PE64 fixtures importing representative API-set contract exports; record actual module-load behavior, resolved function address/module identity, errors, and whether a same-named file can alter resolution. Distinguish documented behavior from observed implementation details.
- **Import parser:** reject truncated names, invalid RVA ranges, cyclic export forwarders, and arithmetic overflow before publishing IAT entries; retain requested contract string in diagnostics.
- **Versioning:** vary the declared guest compatibility profile and verify deterministic mappings and explicit unsupported-contract failures. Do not manufacture an undocumented mapping from current host files.
- **Interaction:** cover ordinary imports, forwarded exports, `LoadLibraryExW`/`GetProcAddress`, delay imports (R221), bound-import fallback (R222), SxS policy (R32), and failure rollback (R194).
- **Isolation:** supply malicious DLL files with contract-like names, concurrent module unload/reload, and competing import resolution; check that guest mapping and module generation ownership cannot be hijacked.
- **Performance:** after correctness, compare warm/cold import resolution latency, allocation count, and module cache hit rate against controlled baselines; no result is available.

## Unresolved evidence

Exact per-build API-set schema and host mappings; behavior of undocumented native loader entry points; package/activation-context interactions; and whether a schema snapshot or a curated, tested mapping is a more maintainable independent implementation. These are **OPEN**, not established compatibility guarantees. Avoid copying Microsoft binary mapping data or third-party implementation code without provenance and license review.

## Decision impact

ADR-003 remains **OPEN**. API-set resolution belongs to guest loader policy and namespace ownership regardless of whether the implementation ultimately uses a user-space service or targeted Linux extensions. This note does not justify a kernel fork.
