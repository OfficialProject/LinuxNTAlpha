# R117 — PE image section alignment, shared-page protections and discardable sections

Date: 2026-11-16. Status: **DOCUMENTATION-VERIFIED contracts and OPEN implementation questions**. No experiment or Windows-reference test was performed for this note.

## Why this is a separate gate

R19 demonstrates a deliberately simple PE32+ image with three sections, relocation and a final protection transition. It does **not** establish a general mapping policy for arbitrary PE section alignment, shared host pages, section padding, or discardable sections. R59 covers section-object/COW semantics; this note focuses on the **loader's page ownership and protection calculation before first execution**.

## Verified primary-source constraints

1. The PE optional header contains `SectionAlignment` (virtual alignment) and `FileAlignment` (raw-data alignment). The PE specification describes the usual requirement that section alignment be at least file alignment and the special case when section alignment is below the architecture page size. `VirtualSize` can exceed `SizeOfRawData`; the remaining image bytes are zero-filled. Raw offsets and virtual addresses are **different coordinate systems**. [Microsoft PE Format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
2. PE section characteristics include `IMAGE_SCN_MEM_EXECUTE`, `IMAGE_SCN_MEM_READ`, `IMAGE_SCN_MEM_WRITE` and `IMAGE_SCN_MEM_DISCARDABLE`. The last is a property of a section, not an instruction to discard it before loader initialization. [Microsoft PE Format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
3. Linux `mprotect` operates on page-aligned addresses and changes protection on pages, not arbitrary byte subranges. A PE virtual section boundary need not imply a distinct host protection boundary. [Linux mprotect(2)](https://man7.org/linux/man-pages/man2/mprotect.2.html).
4. Linux `MAP_FIXED` replaces existing mappings; upstream manual explicitly warns about mapping races and describes `MAP_FIXED_NOREPLACE` for collision-safe acquisition of *previously unreserved* ranges. [Linux mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html).
5. Microsoft documents cache coherency responsibilities when code is generated or changed before execution. This is a separate contract from changing page permissions. [VirtualProtect](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect), [FlushInstructionCache](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-flushinstructioncache).

## Hypotheses, not established Windows behavior

- **H1 — explicit page ownership:** first reserve the complete `SizeOfImage` range, then construct the image while nonexecutable, and derive final host permissions from **all** PE section byte ranges touching each host page. Reject ambiguous or incompatible overlaps until Windows-reference behavior is known. This is a proposed fail-closed prototype policy, not proof of Windows loader equivalence.
- **H2 — shared-page conflict:** blindly calling `mprotect` once per section can leave a shared page with only the *last* section's rights, or grant execute/write to neighboring bytes. Compute page-granular protection once after validating every section. This follows from Linux page granularity; the exact Windows mapping outcome remains OPEN.
- **H3 — discardable lifecycle:** deferring reclamation until relocations, TLS, exception registration, imports and other consumers have completed is safer than freeing a section solely because its header says DISCARDABLE. Whether and when Windows reclaims particular image sections is not established by this note.
- **H4 — host-page-size portability:** a PE alignment accepted on one host page size might produce conflicting section protections on another. Treat host page size as an explicit loader capability and test parameter rather than assuming 4096-byte Linux pages.

## Proposed conformance and security gates (NOT RUN)

1. **Parse-only fixtures:** raw size smaller/larger than virtual size, nonzero zero-fill tail, section alignment below page size, mismatched file alignment, section overlap, overflow in `VirtualAddress + VirtualSize`, image extent beyond `SizeOfImage`, and conflicting permissions on one host page. Record precise acceptance/rejection reasons; never execute malformed fixtures.
2. **Isolated mapping fixtures:** for each accepted layout, inspect final `/proc/self/maps` and probe allowed/denied accesses in disposable child processes. Include two sections sharing one page and one section spanning multiple pages. A SIGSEGV is an observed Linux outcome, not automatically the correct Windows outcome.
3. **Windows-reference matrix:** build benign signed/unsigned self-owned PE images with controlled alignment and section flags; measure whether Windows accepts loading, what `VirtualQuery` reports, and whether code/data access behaves as expected. Test on named Windows versions, not an assumed universal loader.
4. **Lifecycle:** ensure relocation targets are writable only during controlled patching, executable mappings are not published before validation, and discarded pages are not used by delayed initialization or unwind paths. Test module unload/reload and concurrency separately.
5. **Address collision:** reserve a range with `MAP_FIXED_NOREPLACE` when appropriate, verify collision fails without replacing unrelated memory, and never derive a free-range guarantee from a prior `/proc/self/maps` scan.

## Decision impact

**OPEN:** whether a strict prototype rejects sub-page section alignment, implements Windows-like coalesced protections, or uses a more complex mapping backend. Do not silently broaden R19's accepted input set or assert W^X guarantees for all PE layouts. The smallest useful next step is a parse-only section-to-host-page permission planner with adversarial tests, followed by a Windows differential corpus.

## Evidence limits

No code was changed, no executable was run, no performance data was gathered, and no claim of Windows compatibility is made. Primary URLs reviewed 2026-11-16; no Windows build-specific behavior was independently measured.
