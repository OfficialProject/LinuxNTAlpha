# R256 — PE32+ base-relocation block validation and image-publication gates

Date: 2026-11-23. Status: **DOCUMENTATION REVIEW / PROPOSED TESTS ONLY**. No new PE fixture, Windows oracle, fuzz run, benchmark, or code change was executed for this note.

## Scope and prior-work boundary

[R19](R19_PE64_EXECUTION_EXPERIMENT.md) already demonstrates one Windows-target x64 PE fixture whose `IMAGE_REL_BASED_DIR64` fixup succeeds after relocation, plus a restricted set of malformed-image rejections. [R23](R23_PARSE_ONLY_FUZZ_GATE.md) separates parsing from execution. This note does **not** repeat those results. It isolates a missing generalization gate: multiple relocation blocks, target-span validation, repeated targets, and permission-safe publication of the relocated image. The existing R19 success is not evidence for these cases.

## VERIFIED — specification and host contracts

1. Microsoft's PE format defines a base-relocation directory consisting of blocks with a page RVA and block size followed by 16-bit type/offset entries. The offset is relative to the block's page RVA. A base relocation adjusts addresses when the image is not loaded at its preferred base. For x64, `IMAGE_REL_BASED_DIR64` (type 10) applies the base difference to a 64-bit field; `IMAGE_REL_BASED_ABSOLUTE` (type 0) is skipped and can serve as padding. These are **image base relocations**, not COFF object-file relocation records. [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
2. PE section characteristics distinguish execute/read/write permissions and `IMAGE_SCN_MEM_DISCARDABLE`. The relocation table can be carried in a discardable section; that flag does not justify freeing bytes before all required relocations have been processed. [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
3. Linux `mmap` and `mprotect` manage host virtual mappings and page protections, but do not parse or validate PE relocation records. Guest relocation safety and final guest-visible section semantics remain loader responsibilities. [Linux mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html); [Linux mprotect(2)](https://man7.org/linux/man-pages/man2/mprotect.2.html).

## HYPOTHESIS — strict x64-only parser and publisher

Use a bounded **parse/validate/commit** pipeline, initially for PE32+ AMD64 and types ABSOLUTE and DIR64 only. This is a proposed *project acceptance policy*, not a claim that Windows rejects every excluded file:

- Validate the relocation directory RVA/size against the mapped image and checked arithmetic. Walk blocks by checked `SizeOfBlock`, rejecting zero progress, sizes smaller than the 8-byte header, odd trailing entry bytes, or a block extending beyond the declared directory.
- For each non-ABSOLUTE entry compute `target_rva = page_rva + (entry & 0x0fff)` with overflow checks. Validate the **entire eight-byte target**, not just its first byte, against mapped image bounds. A target may cross a 4-KiB boundary; do not assume the block's page limits the 8-byte write.
- Reject unsupported relocation types for the x64 profile rather than silently treating them as DIR64. Distinguish architecture-specific relocation types from COFF relocation constants.
- Define explicit policy for duplicate/overlapping DIR64 targets and relocations into headers, zero-fill tails, or non-writable final sections. The PE specification alone does not establish a universal rejection rule for all these cases; record Windows reference behavior before claiming compatibility.
- Parse and collect fixups without executing guest code. Apply them only to a private staging image, then transition to final protections and publish executable entry points. If a later import/TLS/unwind phase fails, avoid exposing a partially initialized module to other guest threads.
- Preserve the difference between a *file-backed RVA*, a *mapped zero-filled image byte*, and an RVA not mapped by any accepted section. Treat these separately in parser diagnostics.

## OPEN — falsifiable conformance and safety gates

**P1, isolated parse-only fixture matrix:** generate multiple blocks, padding entries, page-RVA near image end, block-size truncation, odd payload lengths, directory-size truncation, integer wrap, eight-byte target crossing a page boundary, and targets crossing mapped-image end. Assert deterministic accept/reject without calling the PE entry point. Keep a corpus manifest with exact bytes, toolchain versions and expected policy.

**P2, semantic relocation fixture:** build a PE64 fixture with several independent 64-bit pointers in distinct sections/pages and verify each at two nonpreferred bases. Assert that applying the delta once yields correct values and that no fixup is applied twice. Include a zero-delta control. This is **not yet executed**.

**P3, Windows differential:** run the same well-formed and borderline fixture corpus under a pinned Windows build with a documented loader-observation method. Record loader acceptance and observable pointer values separately. Do not infer Windows behavior from Microsoft prose alone or from Linux acceptance.

**P4, permission/lifecycle:** check mappings before/after fixups and after failure rollback; assert no writable-and-executable page is published by this test loader. Include concurrent observer threads only after the staging/publish design is explicit. This is a project security invariant, not proof that a PE execution host is sandboxed.

**P5, integration:** once imports and TLS are enabled, verify that relocation of absolute addresses used by TLS and other directories precedes any callback that consumes them; verify rollback after a subsequent loader-stage failure. Coordinate with R27, R45, R64 and R221.

## Decision consequence

**OPEN:** R19's single relocated pointer is sufficient to establish *narrow native PE execution*, not broad relocation correctness. Before admitting general PE32+ images, require bounded multi-block validation, staging/publication tests and Windows differential evidence. No performance or compatibility claim follows from this design note.

## Sources and evidence hygiene

Primary specification: Microsoft PE/COFF format, https://learn.microsoft.com/en-us/windows/win32/debug/pe-format (reviewed 2026-11-23). Host API contracts: Linux man-pages `mmap(2)`, https://man7.org/linux/man-pages/man2/mmap.2.html and `mprotect(2)`, https://man7.org/linux/man-pages/man2/mprotect.2.html (reviewed 2026-11-23). Repository evidence: R19 and R23 above. No source code was imported; no new experiment was run.
