# R59 — Image sections versus data mappings: SEC_IMAGE and copy-on-write

Date: 2026-11-13. Status: **documentation review only; no experiment executed**. Scope: Windows file-mapping APIs versus Linux mmap, not an implemented NT section manager.

## Verified primary-source contracts

1. Microsoft documents `SEC_IMAGE` for an executable-image-backed file mapping. The page protection supplied to `CreateFileMapping` does **not** determine protections of image views; those derive from the executable image. Microsoft also says `SEC_IMAGE` cannot be combined with other mapping attributes. Source: [CreateFileMappingA](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga), section *SEC_IMAGE* (retrieved 2026-11-13).
2. `SEC_IMAGE_NO_EXECUTE` is a distinct image-mapping attribute, requiring `PAGE_READONLY`; Microsoft documents different image-load notification and integrity-check behavior. It must not be silently treated as `SEC_IMAGE`. Same primary source.
3. `MapViewOfFile` documents `FILE_MAP_COPY`: writes privatize pages, are not written back to the original file, and the system/process commit charge covers the **entire view** because every page might become private. This is an accounting contract, not just a data-visibility contract. Source: [MapViewOfFile](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile), *FILE_MAP_COPY* (retrieved 2026-11-13).
4. Linux `mmap(MAP_PRIVATE)` provides private copy-on-write modifications not written to the underlying file; the effect of subsequent external file modifications on the mapped region is unspecified. Source: [Linux mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html), *MAP_PRIVATE* (retrieved 2026-11-13).
5. Linux `MAP_SHARED` propagates modifications to the underlying file; it is not an interchangeable implementation of Windows copy-on-write views. Same Linux source.

## Architectural implications (reasoned, **not experimentally verified**)

- A generic `mmap` of the PE file is **not sufficient evidence** of Windows image-section semantics: PE sections require image-layout interpretation and protections derived from image metadata, while ordinary Linux mappings use file offsets and explicit protection arguments. R19's fixture loader demonstrates execution of selected PE bytes, not conformance with `SEC_IMAGE` section objects.
- `MAP_PRIVATE` is a plausible **mechanism** for parts of Windows `FILE_MAP_COPY` data isolation, but Linux `mmap` alone does not establish Windows view-wide commit charging, handle/object access checks, image-section layout, or image-section lifecycle.
- Avoid equating `SEC_IMAGE_NO_EXECUTE` with merely clearing `PROT_EXEC`: documented Windows notification/integrity behavior crosses boundaries that a userspace mapping flag alone cannot reproduce.
- A Linux-hosted NT-compatible implementation should separate a **section object** (backing identity, type, maximum protection, naming/handles) from **per-process views** (base, offset, length, requested access, actual page protections, private-page accounting). This is a candidate design, **not** a selected architecture.

## Differential conformance plan (proposed, **not run**)

On a documented Windows reference version, record return status, `GetLastError` where defined, `VirtualQuery` region type/protection, mapped contents, cross-process visibility and commit metrics for:

1. Same file mapped as ordinary read-only data, `FILE_MAP_COPY`, and `SEC_IMAGE`; inspect image section layout and `VirtualQuery` classification.
2. Two copy-on-write views of one file: modify one page in one view, then compare both views and the backing file; separately record commit charge before/after mapping and writing.
3. Invalid `SEC_IMAGE` combinations, invalid PE images, and requested access incompatible with mapping protection. Preserve exact error codes and OS build.
4. Image with distinct executable/read-only/writable sections; compare per-region protections before and after permitted writes.
5. `SEC_IMAGE_NO_EXECUTE` behavior on supported Windows versions; do **not** attempt kernel image-notify validation without an authorized driver test environment.

On Linux, build analogous `MAP_PRIVATE`/`MAP_SHARED` fixtures, record `/proc/self/maps` and `smaps` plus page faults and RSS; **do not** equate Linux RSS/commit statistics with Windows commit-charge counters. Treat Windows outcomes as the oracle for compatibility, not the Linux implementation.

## Acceptance gates

- **Parser gate:** strict PE section bounds, alignment, image size and overflow validation before any executable mapping; reuse R23/R44 negative corpus.
- **Semantics gate:** explicit separation of image versus data sections; no blanket assertion that `MAP_PRIVATE` implements `SEC_IMAGE`.
- **Accounting gate:** reproduce documented `FILE_MAP_COPY` reservation/commit behavior or document a measured divergence; no performance claim before measurement.
- **Security gate:** do not map untrusted image bytes executable during parse-only tests; no claim of sandboxing.

## Open questions

- Which Windows NT section creation/mapping statuses and per-view protections should be exposed at the native `NtCreateSection`/`NtMapViewOfSection` boundary? Win32 documentation alone is insufficient to specify the complete native contract.
- What subset of Windows image-section sharing and backing-file mutation restrictions is required for the first application milestone? Requires versioned Windows differential tests.
- Can a userspace pager or a narrowly scoped Linux extension improve fidelity without imposing unacceptable maintenance/security costs? **Hypothesis**, pending tests.

No PE fixture, Windows oracle, Linux mapping benchmark or kernel patch was run for R59.
