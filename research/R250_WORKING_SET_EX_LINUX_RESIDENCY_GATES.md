# R250 — QueryWorkingSetEx versus Linux residency inspection

Date: 2026-11-22. Status: **documentation review only**. No code executed, Windows differential measurements, Linux probes, or compatibility claims.

## Scope and distinction

R247 concerns allocation provenance and VirtualQuery; R248 concerns explicit large-page allocation. This note addresses a different observable: **per-address extended working-set inspection**, including resident/valid state, shareability, sharing count, NUMA node, protection, lock and large-page attributes. A Linux page-residency bit alone cannot provide this contract.

## Verified primary-source contracts

1. Microsoft's `QueryWorkingSetEx` takes a process handle requiring `PROCESS_QUERY_INFORMATION`, an in/out array of `PSAPI_WORKING_SET_EX_INFORMATION` with input virtual addresses, and a byte count. The API can query addresses outside the ordinary working set, including AWE and large pages. Failure is reported through a zero return and `GetLastError`. PSAPI_VERSION changes import/export naming: `K32QueryWorkingSetEx` versus a `Psapi.dll` wrapper. [Microsoft: QueryWorkingSetEx](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-queryworkingsetex).
2. Microsoft's `PSAPI_WORKING_SET_EX_BLOCK` exposes a `Valid` discriminator. In the valid interpretation it includes `ShareCount` (capped at seven), `Win32Protection`, `Shared`, `Node`, `Locked`, `LargePage`, and `Bad`; invalid entries have a different field interpretation. Several fields are OS-version-dependent. Do **not** interpret all bitfields as valid when `Valid=0`. [Microsoft: block layout](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-psapi_working_set_ex_block).
3. Linux `mincore(2)` reports a snapshot of **calling-process** mapped-page residency, not the Windows extended fields; it requires a page-aligned start address, and unmapped ranges can fail with `ENOMEM`. Its snapshot can become stale immediately. [Linux man-pages: mincore(2)](https://man7.org/linux/man-pages/man2/mincore.2.html).
4. Linux `/proc/pid/pagemap` reports present/swapped and certain page-table bits; PFNs are zeroed for callers without `CAP_SYS_ADMIN` on supported kernels. The kernel documents subtleties of the exclusive bit for large allocations. Thus neither PFN visibility nor `exclusive` is a portable substitute for Windows `ShareCount` or `Shared`. [Linux kernel: pagemap](https://docs.kernel.org/6.17/admin-guide/mm/pagemap.html).

## Architectural implications (inference, not implemented)

- A guest process/handle authority layer must check Windows guest rights before any host inspection. Do not expose arbitrary host `/proc` data or grant `CAP_SYS_ADMIN` to emulate guest page queries.
- A guest VM ledger (R247) can preserve declared allocation protection, large-page intent (R248), mapping identity and explicit lock operations. It **cannot by itself** prove physical residency, sharing counts or NUMA placement.
- For a one-host-process-per-guest prototype, `mincore` could contribute a best-effort local residency signal. Cross-process queries would require a deliberately authorized worker/broker path; no Linux API equivalence has been demonstrated.
- Reported `Shared` is not interchangeable with Linux pagemap exclusive or file-backed flags. Host transparent huge pages must not automatically become guest explicit `LargePage` reports.
- The fallback policy for unavailable fields must be established against a version-pinned Windows reference. Fabricating `ShareCount=1`, `Node=0`, or `Locked=0` would be unjustified.

## Unrun conformance and security gates

1. **Reference matrix:** On specified Windows builds, allocate private, shared section, image, explicit large-page (when permitted), AWE (when permitted), locked and decommitted pages; query each address with `QueryWorkingSetEx` and record full flags, errors and OS version. Distinguish unavailable privileges from semantic failures.
2. **State transitions:** Capture before/after touch, decommit, protection changes, lock/unlock, share/copy-on-write, and view teardown; include races with concurrent unmap. Do not assume deterministic residency across samples.
3. **Rights:** Compare self and cross-process handles with varying granted rights, invalid handles, inaccessible buffers and invalid sizes. Confirm error translations separately from host `errno`.
4. **Linux capability survey:** On each target kernel and privilege configuration, compare `mincore`, `pagemap`, and permitted proc inspection against a known mapped test region. Log missing permissions and page-size/THP state rather than treating unavailable data as zero.
5. **Security isolation:** Attempt guest queries for unrelated host PIDs, privileged pagemap PFNs, and stale guest process IDs. Require no host information disclosure.
6. **Decision gate:** Only adopt a backend after a field-by-field coverage table identifies exact, approximate, unavailable and Windows-version-sensitive semantics. Preserve explicit uncertainty.

## Open questions

- What subset of extended fields is actually relied upon by the first target application set?
- Can cross-process residency be queried without unsafe privilege escalation while maintaining the Windows access-right boundary?
- Which Windows build-specific invalid-page layouts and share-count behavior must be supported?
- What latency/consistency tradeoff is acceptable for brokered per-page queries?

**No experiment performed.** No conclusion that `mincore`, `pagemap`, or a ledger implements `QueryWorkingSetEx`.
