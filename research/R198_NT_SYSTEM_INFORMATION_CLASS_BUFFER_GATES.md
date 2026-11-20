# R198 — NtQuerySystemInformation class/version and buffer contract gates

Date: 2026-11-20. Status: **documentation review only; no experiment executed**.

## Scope and relationship to earlier work

R80 covers processor groups and affinity. R172 covers KUSER_SHARED_DATA, R176 clock domains, and R181 ETW. This note instead examines the **generic native system-information dispatch boundary**: a single NTDLL entry point whose output structure, authorization, length behavior and supported classes vary by information class and Windows version. It does not prescribe an NT kernel implementation.

## Verified primary-source contracts

1. Microsoft's `NtQuerySystemInformation` signature takes `SYSTEM_INFORMATION_CLASS`, an in/out buffer, a byte length, and an optional `ReturnLength`, returning `NTSTATUS`. Microsoft explicitly warns the API can change or disappear in future Windows versions and recommends documented alternatives where available. The enumerated classes have different result layouts and descriptions. **Verified from Microsoft API documentation**, not from a Windows test. [MS-NtQuery](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntquerysysteminformation).
2. Microsoft's `GetLogicalProcessorInformationEx` returns a *sequence of variable-size* `SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX` records. Its insufficient-buffer path returns `ERROR_INSUFFICIENT_BUFFER` and the required byte count. This is a **Win32 wrapper contract**, not evidence that every native `NtQuerySystemInformation` class returns the same status or required length. Microsoft documents a Windows Server 2022 change to NUMA-group reporting: `RelationNumaNodeEx` / `RelationAll` can include multiple group affinities while legacy `RelationNumaNode` reports a primary-group view. [MS-Topology](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformationex).
3. Linux `lscpu` collects topology from sysfs, `/proc/cpuinfo`, and architecture-specific sources. This establishes *possible host data inputs*, not a compatible NT layout, snapshot, or security policy. [util-linux lscpu manual](https://man7.org/linux/man-pages/man1/lscpu.1.html).

## Architectural implications (hypotheses, not verified implementations)

- **H1: class-dispatched serialization.** Maintain a Windows-build-profile-aware registry of explicitly supported information classes, each with an independent structure layout, authorization gate, length/status policy, and snapshot routine. Do not implement one global “insufficient buffer” rule.
- **H2: topology snapshot consistency.** Capture a coherent guest-visible CPU topology generation before computing output size and serializing variable-length records; reconcile with guest affinity policy and R80 rather than exposing unrestricted host CPU inventory. Linux hotplug/cpuset races require a defined retry or snapshot policy.
- **H3: information minimization.** Treat raw host kernel details, process inventory, kernel addresses and hardware identifiers as potentially sensitive. A guest query should not automatically disclose host-wide data simply because the Linux process can read it. Define per-class guest visibility and privilege semantics.
- **H4: bounded output.** Validate class and input size, use checked arithmetic for record counts/offsets, validate guest writable memory, and prevent partial host-data disclosure on failure. Actual Windows partial-write behavior must be measured class by class; zeroing is a proposed defensive default, **not** a Windows compatibility claim.

## Falsifiable gates

**Windows reference (not run):** For each *explicitly selected* supported class and OS build, record native status and `ReturnLength` with null/zero, exact-size, one-byte-short, oversized, invalid-class, and inaccessible-output cases. Capture exact structure bytes only where permissible; compare layouts across OS versions, privilege contexts, and 1/64/65+ CPU configurations where available. Separate native NTDLL results from Win32 wrapper results. Never extrapolate one class's length behavior to another.

**Linux prototype (not run):** Feed synthetic topology snapshots with sparse CPU IDs, cpuset restrictions, multi-group boundaries, concurrent generation changes and arithmetic-overflow-sized records into a pure serializer. Assert no out-of-bounds access, stable guest numbering, explicit class authorization, and no host-only data exposure. These are proposed tests, not measurements.

**Integration (not run):** Verify that R80 affinity masks, R172 shared data, R176 clock presentation and process/thread enumeration agree with the chosen guest-visible system snapshot. Failure to maintain consistency blocks publication of the affected class.

## Open questions

- Which classes and exact Windows build profiles are required by the first real target application? This should be established from imports/traces rather than an invented universal compatibility list.
- Which class-specific structures are fully publicly specified, and which require lawful Windows differential characterization?
- Which fields must be virtualized for per-container guest isolation, and which can be exposed without compromising host privacy?
- How are topology changes and required buffer lengths observed during concurrent hotplug?

## Source record

Primary sources consulted 2026-11-20:
- Microsoft, NtQuerySystemInformation (winternl.h): https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntquerysysteminformation
- Microsoft, GetLogicalProcessorInformationEx (sysinfoapi.h): https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformationex
- util-linux manual, lscpu(1): https://man7.org/linux/man-pages/man1/lscpu.1.html

No code was imported; no Windows reference run, Linux prototype run, benchmark, or security validation was performed.
