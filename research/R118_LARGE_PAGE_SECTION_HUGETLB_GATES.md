# R118 — Windows large-page section contracts versus Linux HugeTLB

Date reviewed: 2026-11-16. Status: **documentation research only; no experiment executed**.

## Scope and distinction

This note addresses the **large-page attribute and view lifecycle**, not the ordinary pagefile-backed commitment accounting already considered in [R105](R105_SECTION_COMMIT_OVERCOMMIT_GATES.md), the image-section permissions in [R117](R117_PE_SECTION_PAGE_PROTECTION_GATES.md), or generic section mapping in [R11](R11_NT_MEMORY_SECTIONS.md). A Linux HugeTLB mapping is a potential physical-memory backend, **not** an NT section object implementation.

## Verified contracts (primary platform documentation)

1. **Windows object eligibility.** Microsoft documents `SEC_LARGE_PAGES` for *pagefile-backed* mapping objects; it does **not** support that flag for executable-image-file or data-file-backed mapping objects. The maximum mapping size must be a multiple of `GetLargePageMinimum()`; the flag requires `SEC_COMMIT` and an enabled `SeLockMemoryPrivilege`. This is an explicit guest privilege and validation gate, not merely a performance hint. [Microsoft CreateFileMapping](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga).

2. **Windows view opt-in varies by OS release.** Beginning with Windows 10 version 1703, `MapViewOfFile` uses ordinary pages by default even when the mapping object was created with `SEC_LARGE_PAGES`; callers must supply `FILE_MAP_LARGE_PAGES` to request a large-page view. Before 1703 that flag is ignored and such views are mapped with large pages automatically. The large-page view's size and specified base address must be aligned to `GetLargePageMinimum()`. [Microsoft large-page mapping guide](https://learn.microsoft.com/en-us/windows/win32/memory/creating-a-file-mapping-using-large-pages); [MapViewOfFile](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile).

3. **Linux HugeTLB has independent resource policy.** Linux documents a persistent huge-page pool, reservations and surplus pages, configurable supported sizes and a HugeTLB filesystem. Huge pages are reserved from ordinary memory and are not swappable. A mapping may fail because the requested huge-page pool or permission is unavailable even when normal `mmap` memory would succeed. [Linux kernel HugeTLB administration](https://docs.kernel.org/6.2/admin-guide/mm/hugetlbpage.html).

4. **An anonymous shared-memory fd is not automatically an NT large-page section.** Linux `memfd_create` supports memory-backed file descriptors and optional sealing; its ordinary usage does not imply huge-page backing or Windows mapping-object rights/privileges. Any proposed `MFD_HUGETLB` use must separately negotiate Linux availability, resource limits and authorization. [Linux memfd_create(2)](https://man7.org/linux/man-pages/man2/memfd_create.2.html).

## Architectural implications (hypotheses, not implementation findings)

- **H1 — split guest policy from host allocation.** A guest section service could validate `SEC_LARGE_PAGES`, `SEC_COMMIT`, mapping size, view alignment, requested protections and the guest token's lock-memory privilege *before* choosing a host HugeTLB backing. Host Linux privilege checks remain a separate enforcement layer. The service must not grant Windows guest privileges solely because the Linux service process has host capabilities.
- **H2 — versioned view policy.** If targeting Windows 10 1703+ semantics, record the view-level `FILE_MAP_LARGE_PAGES` request independently of the section object's creation attribute. Do not assume all views must use HugeTLB because the section has the attribute.
- **H3 — fail rather than silently downgrade where observable.** When a caller explicitly requests large pages and Linux HugeTLB allocation fails, silently substituting transparent huge pages or ordinary `mmap` may misrepresent observable guest behavior. Determine the correct Windows failure and state transitions through reference tests; do not invent a fixed NTSTATUS mapping from Linux errno.
- **H4 — no default performance claim.** HugeTLB can trade page-table/TLB overhead against unswappable reserved memory and fragmentation; no improvement in game latency or application throughput is established by this document.

## Falsifiable conformance gates (NOT RUN)

| Gate | Windows-reference observation | Linux prototype observation |
| --- | --- | --- |
| Invalid object type | `SEC_LARGE_PAGES` with real file-backed mapping | Reject before host allocation |
| Privilege | Disabled versus enabled `SeLockMemoryPrivilege` | Guest token decision distinct from host capabilities |
| Alignment | Object size, view size and explicit base misalignment | Guest validation and resource cleanup |
| Version | Windows 10 1703+ view with/without `FILE_MAP_LARGE_PAGES` | View-level selection and fallback policy |
| Resource pressure | Large-page allocation when physical pool exhausted | HugeTLB pool, cgroup/rlimit and error provenance |
| Cross-process lifetime | Duplicate section handles, map/unmap and last-close | Shared backing lifetime and no premature release |
| Protection | Writable/executable view combinations | Reject unsupported guest policy before `mmap` |

For every reference case record Windows build, architecture, page-size minimum, token privilege state, object/view flags, exact error, and whether a view was actually created. For Linux record kernel release, HugeTLB sizes/pool counters, `/proc/meminfo`, limits, process credentials and mapping flags. Do not require privileged host changes in CI; report **SKIP** when no huge-page pool exists.

## Open questions

1. Which exact Windows NTSTATUS and Win32 last-error codes result from each alignment, privilege, and allocation failure on supported target builds?
2. How does the 1703+ ordinary-page default interact with explicit large-page section commitment and actual memory accounting?
3. Which guest section/view operations are legal after a successful large-page map (protection changes, partial unmap, cross-process views)?
4. Is HugeTLB fd-backed shared memory sufficiently expressive for these cases without privileged system-wide pool management?

**Decision:** Do not select HugeTLB as the default NT section backend or introduce kernel modifications on this evidence alone. First execute the versioned Windows-reference matrix and isolated Linux capability tests.
