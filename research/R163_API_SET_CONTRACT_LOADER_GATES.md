# R163 — API-set contract resolution and host-module identity gates

Date: 2026-11-18. Status: **VERIFIED FROM SOURCE** for the documented Microsoft API-set contracts; **HYPOTHESIS** for LinuxNTAlpha design; **OPEN** for runtime behavior and version-specific conformance. No code was executed.

## Why this matters

The existing PE64 experiments bind an explicitly allowlisted synthetic import, not the Windows loader's API-set namespace. Treating every import descriptor's DLL name as a host filesystem path would reject legitimate Windows binaries or invite a malicious file to impersonate a virtual contract. This note isolates the *contract-to-implementation* step from PE parsing, export binding, DLL lifetime and guest filesystem lookup. It does not establish a functioning loader.

## Verified primary-source findings

1. Microsoft defines an API set as a **contract**, not an on-disk DLL. Windows 10+ loader support redirects an imported `api-` or `ext-` contract name through an OS-composed API-set schema to a host module. The `.dll` suffix is conventional and is not part of the contract identity. The same contract may map differently on different Windows products. [Microsoft: Windows API sets](https://learn.microsoft.com/en-us/windows/win32/apiindex/windows-apisets); [API set loader operation](https://learn.microsoft.com/en-us/windows/win32/apiindex/api-set-loader-operation).
2. A static import of an unresolved contract prevents process startup, whereas an optional delay-loaded or dynamically resolved dependency can fail at use time. A successful schema mapping does **not** guarantee a requested export or capability is available. Legacy module names may be handled through *reverse forwarders* on products where their implementation was refactored. [Microsoft: API set loader operation](https://learn.microsoft.com/en-us/windows/win32/apiindex/api-set-loader-operation).
3. `IsApiSetImplemented` checks contract availability on the running device; a positive answer is not a guarantee that every export or call succeeds. Named-group availability is distinct from contract-level binding. Neither the `api-` nor `ext-` prefix alone proves availability. [Microsoft: IsApiSetImplemented](https://learn.microsoft.com/en-us/windows/win32/api/apiquery2/nf-apiquery2-isapisetimplemented); [Detect API set availability](https://learn.microsoft.com/en-us/windows/win32/apiindex/detect-api-set-availability).
4. `GetApiSetModuleBaseName` reports the schema's host-module base name without searching for the file, loading it or inspecting exports. Microsoft documents dynamic binding of the API-query contract as a compatibility pattern. [Microsoft: GetApiSetModuleBaseName](https://learn.microsoft.com/en-us/windows/win32/api/apiquery2/nf-apiquery2-getapisetmodulebasename).
5. The Microsoft SDK's umbrella libraries influence which API-set contracts appear in import tables. The WindowsApp.lib reference lists individual contracts and Windows build introduction/movement details; it is **not** a universal host mapping table. [Microsoft: APIs present on all Windows devices](https://learn.microsoft.com/en-us/uwp/win32-and-com/win32-apis).

These links are public documentation consulted 2026-11-18, not pinned Windows binaries or a claim that all documented behavior was measured on a particular build.

## Architectural hypothesis (not selected)

Maintain a **versioned guest API-set namespace** separate from guest DLL path lookup:

- Parse and preserve the original import-descriptor name; classify only after bounded, case-insensitive contract-name validation. Never synthesize a host path from a contract string.
- Resolve `contract -> guest implementation-module identity` through an explicit target-profile table, then `module + export name/ordinal -> callable guest entry point`. Maintain separate errors for missing contract, host and export.
- Represent absent/unmapped contracts explicitly. A host-module mapping is not a promise of a specific exported function or feature group.
- Keep module identity, loader references, forwarder chains and unload decisions separate from contract aliases; multiple contracts may resolve to one module.
- Treat API-set schema selection as part of the *emulated Windows target profile*, not as an automatic reflection of the Linux host or a universal static table.
- Any future schema imported from Windows requires licensing/provenance review. This note does not authorize copying proprietary schema data.

## Falsifiable conformance gates

1. **Windows reference, multiple builds/editions:** collect OS build and architecture, then compare `IsApiSetImplemented` and (where available) `GetApiSetModuleBaseName` for a documented present contract, a deliberately nonexistent contract, versioned names, and names with/without `.dll`. Record unsupported query API as SKIP, not FALSE.
2. **Static versus dynamic:** construct signed-off benign PE fixtures importing one available and one absent contract. Compare process-start failure against `LoadLibrary` failure and delay-load behavior. Capture exact Windows error/exception and build; do not assume identical outcomes across all versions.
3. **Binding and alias identity:** use a fixture with two contracts resolving to the same implementation and verify export resolution, module identity and reference lifetime; distinguish schema-host reporting from actual host loading.
4. **Parser negative controls:** truncated descriptor strings, unterminated names, non-ASCII names, excessive length, fake `api-` prefix, path separators, traversal and cyclic forwarder chains must fail safely. Parse-only is the default for untrusted inputs.
5. **Security:** place a user-writable file named like an API-set contract in a guest-visible search directory. Confirm the contract-resolution path does not load that file merely because its name matches a virtual contract.
6. **Profile drift:** compare at least two explicitly recorded Windows versions. Do not hard-code one observed contract-to-host mapping as universal.

## Boundaries and next action

**OPEN:** actual schema structure and loader implementation for pinned Windows builds; forwarder cycle/error semantics; how to model contract aliases and groups precisely; authoritative redistribution/licensing of mapping data; performance of lookup and caching. No Windows reference run, Linux test, binary inspection or performance measurement was performed in this research pass.

**Next smallest safe experiment:** add a *parse-only* PE import-name classifier and synthetic, explicitly authored contract mapping table with malformed-name tests. Keep it separate from real PE execution until the above reference gates are collected. Existing R19/R20/R23/R28 fixture results must not be relabeled as API-set compatibility.
