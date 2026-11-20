# R98 — KUSER_SHARED_DATA fixed-address guest mapping and time publication gates

Date: 2026-11-20. Status: **documentation research only**; no Linux or Windows experiment performed.

## Scope and evidence

**VERIFIED (Microsoft primary sources):** Microsoft documents `KUSER_SHARED_DATA` as an OS-maintained page containing time, version and other shared state. Microsoft Security Response Center (2022-04-05) describes the 4096-byte page mapped at fixed addresses on the user and kernel sides, and its mitigation that keeps the legacy kernel virtual address read-only while using a separate randomized writable kernel alias. This is **not** evidence that arbitrary applications may write the user mapping. The published WDK structure describes `KSYSTEM_TIME`-typed InterruptTime, SystemTime and TimeZoneBias and includes version-dependent fields. Microsoft documents `NtQuerySystemTime` returning 100-ns intervals since 1601-01-01 UTC; its documentation cautions that the API may change.

**CORROBORATION, not a Microsoft contract:** independent Windows layout analyses identify the x86/x64 user-mode mapping at `0x7FFE0000`. Treat that address and field offsets as compatibility targets requiring target-version validation, not as a stable public user-mode API for arbitrary future Windows releases.

Primary references:
- Microsoft WDK, KUSER_SHARED_DATA: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-kuser_shared_data
- Microsoft MSRC, *Randomizing the KUSER_SHARED_DATA Structure on Windows*, 2022-04-05: https://www.microsoft.com/en-us/msrc/blog/2022/04/randomizing-the-kuser_shared_data-structure-on-windows/
- Microsoft, NtQuerySystemTime: https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntquerysystemtime

Secondary layout cross-check (not normative):
- https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/ntexapi_x/kuser_shared_data/index.htm

## NT-on-Linux implications

**HYPOTHESIS:** For guest code containing direct absolute loads, LinuxNTAlpha may need a read-only guest-visible page at the expected Windows address rather than merely exposing time through imported functions. The host Linux vDSO is not a binary-layout replacement for this page.

**HYPOTHESIS:** A runtime-owned writable backing mapping plus guest read-only alias could update shared fields without allowing guest writes. A single immutable copy initialized at launch would not faithfully model continuously changing time. The Linux host must still own the guest address-space reservation and prevent unrelated mappings from taking it.

**OPEN:** Exact target Windows build layout and which fields are consumed by actual test binaries. The WDK definition alone does not establish all user-mode offsets for every release. Likewise, direct reading of time fields requires a validated consistency protocol: do not assume an unsynchronized 64-bit load or simple host clock memcpy is sufficient.

**OPEN:** Which clocks map to interrupt time, system time and unbiased time across suspend/resume, wall-clock adjustment, and virtualization; how to preserve time monotonicity and avoid cross-thread torn observations; whether fixed-address reservation conflicts with loader allocations, ASLR or host memory policies.

**SECURITY GATE:** The guest mapping must not become a host-controlled code execution surface or allow guest writes to runtime-maintained state. Never use a guest-writable page as the authoritative clock/security/version state.

## Proposed falsifiable acceptance tests (NOT EXECUTED)

1. On separately provisioned Windows reference builds, read the documented APIs and the candidate shared page under a controlled harness; record OS build, architecture, address, protection, relevant offsets and observed update behavior. Do not claim undocumented fields are stable across builds.
2. In a Linux isolated harness, reserve the candidate address with nonclobbering mapping semantics; reject collisions rather than using unsafe `MAP_FIXED` over an existing mapping. Confirm read-only guest access and fault on guest write.
3. Compare repeated reads against `NtQuerySystemTime`/documented Windows time APIs under normal operation and wall-clock changes, including concurrent readers; explicitly record error tolerances and clock definitions.
4. Add a freestanding PE64 fixture that directly loads a verified field from the shared page; verify actual guest machine code rather than a host-only unit test. Keep this separate from R24/R25 GS/TIB tests.
5. Exercise process startup/teardown and concurrent guest threads; ensure backing lifetime and address-space ownership cannot be invalidated by guest unmapping/remapping.

## Decision gate

Do not declare fixed-address page support complete until a version-scoped Windows reference and a Linux guest PE fixture both pass. This note proposes an architectural compatibility surface, **not** a working implementation, benchmark or security proof.
