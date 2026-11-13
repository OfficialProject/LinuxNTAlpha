# R44 — PE32+ import-table parser: bounded structural acceptance before resolution

Date: 2026-11-12. Status: **DOCUMENTATION-VERIFIED DESIGN GATES; NO NEW EXPERIMENT**.

## Scope and nonduplication

R20 proves only a single permitted `probe.dll!add_five` IAT binding. R29 covers API-set contracts, delay imports and forwarders. This note specifies the **ordinary PE32+ import-directory parse-only boundary** needed before lifting R20's single-descriptor restriction. It does not implement an import parser or claim Windows loader equivalence.

## Primary evidence

Microsoft's [PE format specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format), sections **Import Directory Table**, **Import Lookup Table**, **Hint/Name Table**, and **Import Address Table**, specifies:

- **VERIFIED:** The import directory is an array of **20-byte descriptors**, one per referenced DLL, terminated by a descriptor whose fields are all zero. The fields are import lookup table RVA, timestamp, forwarder-chain index, DLL-name RVA and import address table RVA.
- **VERIFIED:** A PE32+ lookup entry is **64 bits**. A zero entry terminates the table. The high bit distinguishes ordinal from name lookup; an ordinal uses the low 16 bits, with the intervening bits reserved as zero. A by-name entry points to a two-byte hint followed by a null-terminated case-sensitive ASCII export name.
- **VERIFIED:** Before binding, the IAT ordinarily has the same layout/content as the lookup table; binding replaces its slots with resolved addresses. Bound images complicate the assumption that IAT contents can always be parsed as original lookup entries.
- **VERIFIED:** A DLL name in a descriptor is an ASCII null-terminated string addressed by RVA. Its contents do not themselves establish that a corresponding physical library exists.

These are **format contracts**, not proof that Windows rejects every malformed image in the same way.

## Proposed parser contract — DESIGN, NOT OBSERVED WINDOWS BEHAVIOR

1. **Parse-only by default.** Separate file-backed byte access, mapped-image RVA access, descriptor parsing, thunk parsing, and eventual symbol resolution. No callback or guest entry execution during parsing.
2. **Validate directory span with overflow-safe arithmetic.** Confirm `RVA <= image_size` and `size <= image_size - RVA` before adding. Use an explicit finite descriptor count bounded by the available directory bytes; require an all-zero terminator *within the accepted span* for the first strict-parser profile. Distinguish this conservative policy from Windows' potentially more permissive behavior.
3. **Validate every referenced span independently.** A descriptor being in bounds does not validate its DLL-name string, lookup table, hint/name record, or writable IAT destination. Scan strings and zero-terminated thunk arrays with remaining-span limits, never an unbounded `strlen`.
4. **Parse lookup and patch destinations separately.** For each nonzero lookup thunk, validate its name/ordinal representation and a corresponding 8-byte IAT slot. Do not assume a nonzero IAT slot can be interpreted as a lookup thunk in a bound image. Reject unsupported bound-import layouts explicitly until reference behavior is characterized.
5. **Retain ordered descriptors and imports.** Do not collapse duplicate module descriptors or duplicate symbol imports at parse time. Preserve descriptor order and slot identity so later Windows-reference testing can determine which behavior matters.
6. **Keep a typed outcome.** Return one of `well_formed_supported`, `well_formed_unsupported`, or `malformed`, with a stable reason code and location. An ordinal import is not inherently malformed just because R20 cannot resolve it.
7. **Do not trust mapped pages as file bounds.** PE section virtual size, raw data size, and zero-filled tails can differ. Define whether each read is from validated file bytes or a reconstructed image, and test both semantics explicitly before treating the parser as a security boundary.

## Falsifiable test matrix — PROPOSED, NOT EXECUTED

| Case | Strict parse-only expected result | Windows-reference question |
| --- | --- | --- |
| Two descriptors, multiple named imports, proper null descriptors/thunks | structurally accepted | Does reference loader resolve all entries and in what order? |
| Ordinal import with valid reserved bits | structurally accepted, resolver unsupported | Does the target DLL export that ordinal? |
| Ordinal flag with reserved bits set | malformed in strict profile | Is Windows more permissive? |
| Descriptor exactly truncated at last byte | malformed | Is the image rejected at load? |
| Missing all-zero descriptor within declared span | malformed in strict profile | Does Windows inspect beyond declared directory size? |
| Unterminated DLL or export name at validated image boundary | malformed | Is load rejected or another boundary used? |
| Lookup table zero terminator missing | malformed | What is the actual Windows search bound? |
| IAT slot crosses validated image end or lacks writable mapping | malformed/unsupported as appropriate | What load status and protections occur? |
| Nonzero timestamp and modified IAT | accepted only under separately defined bound-import profile | How does Windows validate and fall back from stale bindings? |
| Duplicate descriptors or duplicate symbols | structurally accepted | Are duplicates preserved, coalesced, or resolved separately? |

Each fixture should record its SHA-256, PE generator/toolchain version, target architecture, parser result and failure location. Run parsing under sanitizers and deterministic mutation tests; run **only intentionally trusted fixtures** in an execution path. A parse-only PASS is not evidence of Windows conformance or hostile-code isolation.

## Open questions / evidence gaps

- Whether Windows loader accepts directory tables or thunk terminators outside the declared data-directory size; the strict profile above intentionally does not guess.
- How `OriginalFirstThunk == 0` and bound import tables should be handled for different Windows builds. The specification alone does not establish a complete compatibility policy.
- Whether zero-filled virtual section tails may legally contain import metadata that is absent from file-backed bytes in supported Windows builds.
- Precise loader failure codes and cleanup order for malformed descriptors, invalid ordinal exports, or partially bound dependencies.

## Next evidence-producing action

Implement a **standalone parse-only** ordinary-import iterator and fixtures first, without modifying R20's executable allowlist. Capture differential results against a documented Windows build before relaxing strict structural policies. Only then connect parsed imports to the R29 module resolver and the R20/R28 ABI stubs.

**Decision impact:** None. No code, Windows run, benchmark, or conformance claim is made in this note.
