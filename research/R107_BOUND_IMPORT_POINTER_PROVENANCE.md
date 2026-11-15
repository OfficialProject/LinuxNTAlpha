# R107 — Bound-import metadata is not a trusted host function pointer

Date: 2026-11-15. Status: **DOCUMENTATION REVIEW ONLY**; no code or Windows-reference experiments were run.

## Question

Can a native PE32+ loader on Linux safely reuse prebound import-address-table (IAT) values, and what must be validated before exposing imports to executable guest code?

This note extends [R20](R20_PE64_IAT_BRIDGE.md), [R44](R44_PE64_IMPORT_PARSER_BOUNDARIES.md), [R51](R51_PE_EXPORT_FORWARDERS_RESOLUTION.md), and [R29](R29_API_SET_DELAY_IMPORT_GATES.md). It specifically addresses **trust and provenance of bound pointers**, not general import descriptor parsing, export forwarding, or delay-load behavior.

## Verified from Microsoft's PE/COFF specification

1. Each ordinary import-directory entry has an import lookup table RVA, timestamp, forwarder-chain field, module-name RVA, and IAT RVA. The timestamp is zero before binding and is set to the referenced DLL's timestamp after binding.
2. PE32+ import lookup entries are 64-bit. The top bit selects ordinal versus name lookup; the lookup table ends in a zero entry. The IAT starts with the same values as the lookup table **until the image is bound**. Thus treating a bound IAT as an import-name table is unsound.
3. A separate **Bound Import Directory** exists. Its descriptors and forwarder references encode binding metadata. This is different from the ordinary import directory and from the delay-load import directory.
4. Delay-load imports have separate name, IAT, optional bound IAT and optional unload IAT tables. Their resolution timing and restoration semantics are distinct from ordinary startup imports.

Primary source: Microsoft, [PE Format — Import Directory Table, Import Lookup Table, Import Address Table, Bound Import Directory, Delay-Load Import Tables](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) (consulted 2026-11-15).

## Architectural conclusion (inference, not measured)

**Never execute or publish a PE file's prebound IAT addresses as Linux host call targets merely because binding metadata appears valid.** A guest image is untrusted input, while a Linux-hosted NT implementation owns its callable stubs. Even a timestamp match does not establish that an address belongs to the correct mapped module, export, ABI bridge, or permitted host service.

A conservative first implementation can **ignore the prebound address optimization** and reconstruct every ordinary import from validated lookup/name-or-ordinal metadata against the guest module graph, then write only resolved, authorized guest-callable entry points to the IAT. This is a proposed policy, not a claim about how Windows behaves in every version.

For images where the original lookup table is absent or insufficient to recover the intended imports, the initial loader should fail closed with a distinct unsupported-binding error instead of interpreting arbitrary IAT values as valid host pointers. Whether Windows accepts particular such images is **OPEN** and requires differential testing.

## Proposed conformance and security gates — NOT EXECUTED

- Parse-only fixtures: unbound import, bound ordinary import, bound import with forwarded dependency, absent lookup table, ordinal imports, mismatched timestamps, truncated bound descriptors, invalid module-name offsets, and integer-overflowing descriptor counts. Check bounded iteration and deterministic rejection.
- Hostile fixture: IAT entry points into the Linux host executable, into an unrelated mapped guest image, and into nonexecutable memory. Confirm no address is invoked during parsing or resolution.
- Multiple-DLL fixture: imports resolve by guest module identity and export symbol rather than incidental host symbol spelling; verify that the published IAT contains only bridge targets with the correct Windows x64 calling convention.
- Windows-reference differential tests: compare load success/failure and resolved module identity for valid linker-produced bound and unbound fixtures across explicitly recorded Windows builds. Do **not** assume malformed-image error codes are stable without evidence.
- Delay-load fixture: confirm initial load does not eagerly resolve delay imports under the chosen helper contract; test explicit unload/reset separately. Do not conflate ordinary binding with delay binding.
- Record image SHA-256, compiler/linker versions, Windows build, host kernel, loader commit, and PASS/FAIL/SKIP for each test.

## Remaining open questions

1. Which legitimate modern linker-produced images omit the original lookup table after binding, and how should the MVP report them?
2. How should API-set redirection, SxS activation contexts, export forwarders and module unload invalidate or refresh resolved IAT entries?
3. What is the correct synchronization and rollback protocol if a dependency fails during resolution, before any guest entry point is called?
4. Can an explicit guest-address ownership table provide a practical invariant: every published call target is either an executable guest export or a verified ABI bridge, never an arbitrary host pointer?

No performance benefit, compatibility gain, or exploit prevention is claimed without implementation and testing.
