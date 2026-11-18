# R162 — Bound-import metadata and rebinding gates

Status: PRIMARY-SOURCE DOCUMENT REVIEW ONLY (2026-11-18). No Windows-reference execution, Linux fixture, benchmark, or implementation change was performed.

## Scope and relationship to prior work

R20 demonstrates a synthetic single-import IAT bridge; R44 covers ordinary import descriptor structural bounds; R51 covers export forwarders; R29 covers delay imports. This note isolates **prebound import-address state** and the failure modes of trusting it. It does not duplicate the ordinary import parser design or claim that current Windows versions optimize bound imports in any particular way.

## VERIFIED — format contracts

1. Microsoft documents an ordinary import directory entry containing an import lookup table RVA, Time/Date Stamp, Forwarder Chain, DLL Name RVA, and Import Address Table RVA. The stamp is initially zero and is set to the DLL timestamp when bound. The IAT and lookup table are initially identical; binding replaces IAT entries with imported symbol addresses. Source: [Microsoft PE format — Import Directory Table / Import Address Table](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
2. The PE optional-header data directories separately identify a **bound import table** and a **delay import descriptor**; they must not be conflated with the ordinary import table. Source: [Microsoft IMAGE_DATA_DIRECTORY](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_data_directory).
3. Microsoft documents an optional delay-bound IAT and timestamp in the *delay-import* descriptor. That is a different lifecycle from eager ordinary imports; the delay-load helper may patch thunks when invoked. Source: [Microsoft PE format — Delay-Load Import Tables](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
4. The PE format's import lookup entries encode names or ordinals and terminate with zero. A prebound IAT pointer is **not** by itself a validated lookup-table name/ordinal. Source: [Microsoft PE format — Import Lookup Table](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

## HYPOTHESIS — conservative LinuxNTAlpha loader design

For initial compatibility, treat prebound addresses as **untrusted hints**, never authoritative host function pointers. Resolve each dependency to a guest module identity, validate lookup entries and exported targets, then write guest-appropriate IAT pointers. This intentionally forgoes any historical bound-import fast path until Windows-version-specific evidence supports it. The choice is motivated by Linux host address-space layout, independent module implementations, and the risk of executing image-supplied pointers; it is **not** a claim about Windows loader internals.

Use distinct parser records for (a) ordinary imports, (b) bound-import metadata, and (c) delay-import metadata. Track whether a thunk is original lookup data, a prebound address, a delay thunk, or a resolved runtime pointer. Never infer a valid import lookup entry from a potentially prebound IAT value.

## OPEN — version and structural ambiguities

- Establish exact bound-import descriptor/forwarder-ref layouts against a pinned Windows SDK `winnt.h` revision, and reconcile them with PE-format documentation before implementing byte-level parsing.
- Determine with actual Windows-reference tests whether current supported Windows versions use, ignore, or validate historical bound-import optimization paths for controlled images. Do not generalize from a Windows 2000 loader article.
- Test loader behavior for missing/mismatched timestamps, forwarded exports, rebased dependencies, malformed directory sizes, overlapping metadata, absent original lookup tables, and conflicting ordinary/delay bound information.
- Decide policy for bound-only images with no recoverable symbol identity; do not fabricate a symbol mapping from raw address values.

## Proposed reproducible acceptance gates — NOT EXECUTED

1. Parse-only: generate small PE32+ fixtures with ordinary imports plus a separate bound-import directory, valid and invalid size/offset combinations; reject out-of-image reads, overflow, unterminated descriptors, and metadata overlap according to an explicit parser policy.
2. Resolution: deliberately place bogus prebound addresses in IAT slots while preserving valid import lookup entries. A Linux-hosted mock guest module resolver must overwrite the slots with the expected MS-ABI target; no call may jump to the bogus address.
3. Forwarders: construct a controlled export-forwarder chain and compare resolved symbol identity against R51's expected model; reject cycles or missing targets deterministically.
4. Differential: on a pinned Windows build, compare load outcomes and IAT contents for bound/unbound/rebased controlled images, recording toolchain, image hashes, timestamps, loader errors, and exact OS build. Record SKIP when Windows is unavailable.
5. Security: fuzz directory counts and RVA arithmetic in **parse-only** mode before executing any PE fixture. Run executable tests only on trusted generated images, per R23.

## Architectural consequence

Bound-import handling is a loader correctness and security gate, **not** evidence for a kernel change. Keep it in the guest image loader and module resolver until conformance results require a different boundary. No speedup or compatibility claim follows from this review.
