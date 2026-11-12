# R29 — API-set contracts, forwarders and delay imports: loader resolution gates

Date: 2026-11-12. Status: **VERIFIED FROM MICROSOFT DOCUMENTATION; NO EXPERIMENT RUN**.

## Question

Does the R20/R28 one-DLL, one-name allowlist generalize by treating every PE import descriptor's DLL name as a physical library to open? **No.** Microsoft's documented API-set and delay-import contracts require a separate resolution model. This note does not implement one.

## Primary-source findings

1. **API sets are contracts, not DLL files.** Microsoft states that an import beginning with `api-` or `ext-` is resolved by the loader using the current device's API-set schema to a host implementation; a physical file with the contract name need not exist. The `.dll` suffix is conventional and is not part of the contract identity. Contract names are case-insensitive. A versioned contract and an unversioned alias are distinct forms; prefixes alone do not establish availability. [Windows API sets](https://learn.microsoft.com/en-us/windows/win32/apiindex/windows-apisets); [API set loader operation](https://learn.microsoft.com/en-us/windows/win32/apiindex/api-set-loader-operation).
2. **The mapping is environment-dependent.** Microsoft's loader-operation documentation describes direct contract-to-host resolution and separate legacy-module reverse-forwarding. Consequently a static hard-coded `api-ms-win-*` to `kernel32.dll` substitution is **not** a generally valid resolution algorithm. [API set loader operation](https://learn.microsoft.com/en-us/windows/win32/apiindex/api-set-loader-operation).
3. **Availability is not export presence.** Microsoft's `IsApiSetImplemented` documentation describes contract/group availability, not a guarantee that a specific export exists or that calls succeed. Microsoft's `GetApiSetModuleBaseName` documentation states that a schema host lookup does not check the filesystem or load a module. [Detect API set availability](https://learn.microsoft.com/en-us/windows/win32/apiindex/detect-api-set-availability); [GetApiSetModuleBaseName](https://learn.microsoft.com/en-us/windows/win32/api/apiquery2/nf-apiquery2-getapisetmodulebasename).
4. **Static and delayed imports have different failure timing.** Microsoft notes that an unavailable static import can fail process initialization before application code can query availability, while delay-loading defers resolution until the relevant call. [Detect API set availability](https://learn.microsoft.com/en-us/windows/win32/apiindex/detect-api-set-availability); [Windows API sets](https://learn.microsoft.com/en-us/windows/win32/apiindex/windows-apisets).
5. **Delay import descriptors are not ordinary import descriptors.** The PE specification describes a separate optional-header delay-import directory with 32-byte descriptors, including attributes, name, module-handle slot, delay IAT, delay name table, optional bound/unload tables and timestamp. Attribute bit 0 distinguishes RVA-based from older VA-based descriptor fields; other attribute bits are reserved. The unload table supports restoring thunk state. [Microsoft PE format, Delay-Load Import Tables](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
6. **Ordinary imports and export forwarders are additional gates.** The PE specification distinguishes import lookup/name/ordinal entries and an export-address entry that can be a forwarder RVA pointing to a string naming another DLL/export. Binding a single hard-coded symbol is not a substitute for parsing these cases. [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

## Architecture implications — HYPOTHESES, not implementation claims

- Separate **PE structure parsing**, **module/contract identity resolution**, **export lookup (including forwarders)**, and **host ABI/API implementation**. This avoids accidentally treating a contract as a filesystem path or treating a resolved host as proof that an export works.
- A deliberately small initial supported-contract registry may be acceptable for *trusted fixtures*, provided unsupported contracts fail explicitly and it is never described as Windows schema compatibility.
- Keep contract resolution independent of Linux pathname case-folding or arbitrary host DLL loading; guest-controlled import strings must not become unchecked host filesystem paths.

## Proposed falsifiable acceptance gates — NOT EXECUTED

1. Parse-only synthetic PE fixtures with ordinary named and ordinal imports, multiple descriptors, truncated/unterminated thunk arrays, invalid RVAs, and forwarder strings; record bounded rejection versus accepted-but-unsupported cases. No guest code execution.
2. Parse-only delay-import fixtures for both RVA-based and legacy VA-based descriptors, reserved attribute bits, missing terminators, invalid module-handle and IAT destinations, integer overflow, and absent optional bound/unload tables. Separate malformed input from unsupported legal format.
3. Construct Windows-target reference fixtures importing one documented API-set contract and one ordinary module, with an optional delayed import. Record Windows build/edition, contract name, schema host resolution (where documented APIs permit), loader outcome and actual export outcome. Do not assume one Windows edition's mapping is universal.
4. Build an explicit synthetic contract registry and compare only fixture-level resolution outcomes; later add real per-environment schema and forwarder semantics after Windows reference traces.
5. Run under an isolated, unprivileged host; retain R23's parse-only default. A parser success is neither loader correctness nor sandboxing.

## Open questions

- Which Windows release/edition should define the first conformance baseline? This affects API-set mappings and availability.
- What exact export-forwarder cycle and depth behavior should be emulated? Requires a version-scoped Windows reference test rather than guessing.
- How will delay-load helper state, concurrency, error paths and unload behavior interact with a future Windows-compatible loader and TLS lifecycle?
- Can a minimal supported-contract subset deliver useful real application startup without committing to a complete schema implementation? Needs measured dependency corpus, not intuition.

**Decision:** No architecture decision is changed. Before expanding R28's allowlist to arbitrary Windows imports, implement structural parsing and explicit unsupported cases; do not silently translate API-set names into presumed physical DLLs. No test or Windows conformance result is claimed here.
