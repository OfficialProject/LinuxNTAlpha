# R94 — NT section-view inheritance versus handle inheritance (2026-11-15)

**Status:** primary-source contract review only. **No Windows-reference run, Linux experiment, or implementation.** Related: R11 (sections), R12/R34 (process creation), R59 (image/COW), R67 (handle duplication), R73 (process lifetime).

## Verified contracts (documentation, not experimentally reproduced)

1. Microsoft's `ZwMapViewOfSection` (user-mode name `NtMapViewOfSection`) takes a `SECTION_INHERIT InheritDisposition` parameter. The documented `ViewShare` value maps the view into future child processes; `ViewUnmap` does not. The routine also accepts a target process handle and allows concurrent views of a section in one or more processes. [Microsoft, ZwMapViewOfSection](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwmapviewofsection).
2. File-mapping **handle inheritance is a separate contract**: `CreateFileMapping` security attributes control whether the returned handle can be inherited. [Microsoft, CreateFileMappingA](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga). General Win32 inheritance requires an inheritable handle and an appropriate child-creation inheritance policy; inherited handles refer to the same underlying object. [Microsoft, Handle Inheritance](https://learn.microsoft.com/en-us/windows/win32/sysinfo/handle-inheritance); [Inheritance](https://learn.microsoft.com/en-us/windows/win32/procthread/inheritance).
3. Section-object lifetime is not equivalent to the lifetime of the creating handle: Microsoft's section guidance explicitly permits closing a section handle after establishing a view, while the view and section object persist until the view is unmapped (assuming no other references). [Microsoft, Managing Memory Sections](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/managing-memory-sections).

**Important scope caveat:** The `ViewShare` statement is the documented native API contract. It is **not evidence** that every ordinary Win32 `CreateProcess` path reproduces a parent's address-space mappings, nor that Linux `fork` provides equivalent semantics. Those questions require reference tests with precisely specified process-creation paths.

## Architectural consequences — hypotheses, not proven design decisions

- **H1:** Model three independent entities: section object (backing and security), process-relative section handle (granted rights and inheritance flag), and process-relative mapped view (base, size, protection, offset, inheritance disposition, section reference). A view can outlive the handle. Conflating these would incorrectly tie memory lifetime to handle closure.
- **H2:** A Linux `mmap` backend can provide some page-sharing behavior, but it cannot by itself decide guest NT child-creation view inheritance. The guest process manager must explicitly choose mappings for a new guest address space; native Linux process spawning is not an NT conformance oracle.
- **H3:** A child mapping's address identity, protection and backing visibility need to be checked against Windows; do not assume fixed-address recreation is always possible or safe. Linux `MAP_FIXED` is not a substitute for collision-safe guest address reservation.

## Windows-reference differential test plan (NOT RUN)

On a supported Windows x64 reference system, record OS build, creation API, process bitness, return statuses, allocation granularity, and `VirtualQuery` / `NtQueryVirtualMemory` observations. Use isolated test binaries and fresh child processes:

1. Create a pagefile-backed section; map a view using `NtMapViewOfSection` with `ViewShare`; write a sentinel; close the section handle but retain the view. Launch a child via a specifically recorded native process-creation path; query child mapping presence, base, protection, contents and lifetime.
2. Repeat with `ViewUnmap`, all else unchanged. The critical outcome is the **difference**, not an assumed result.
3. Repeat with an inheritable section handle and with a noninheritable handle, independently of the view-disposition toggle. Inspect whether the child holds a usable handle and whether the view exists; do not treat one as proof of the other.
4. Compare `CreateProcessW` and any tested native process-creation path **separately**, rather than extrapolating from one to the other.
5. Map the same section twice with different offsets, unmap one view, close all handles, and verify that the other view's access and section lifetime follow observed reference behavior.
6. Test conflicting requested base addresses and incompatible protections, recording NTSTATUS and whether existing mappings remain intact. Do not perform destructive fixed-address mapping inside the test controller.

## Proposed isolated model checks (NOT RUN)

- Guest section-handle closure cannot silently unmap live views.
- Child handle-table inheritance is independent of the mapped-view selection policy.
- View teardown decrements the correct section reference exactly once.
- Failed child mapping setup rolls back all newly installed views and references without changing parent mappings.
- No guest mapping operation may overwrite the host runtime's executable, TLS, signal trampoline or other protected regions.

## Open questions / decision gates

1. Which exact native process-creation paths honor `ViewShare`, and how does this interact with modern Win32 process creation? Microsoft documentation alone does not resolve every path.
2. How are inherited views relocated or rejected on address collision, and which statuses are observable?
3. How do copy-on-write views, image sections and pagefile-backed sections differ at inheritance time?
4. Which guest view-state records must be authoritative across process creation, section closure and unmapping?

**Recommendation (provisional):** Do not implement view inheritance by assuming `fork` equivalence. First establish Windows reference traces, then specify guest view-state and reference ownership. This is a research gate, not a chosen implementation architecture.

**Sources checked:** Microsoft Learn pages linked above, accessed 2026-11-15. No upstream source revision or benchmark is claimed.
