# R98 — Native NT service identity, Nt/Zw caller mode, and build-specific dispatch

Date: 2026-11-17 UTC. **Documentation-only architecture research.** No Windows reference host, kernel instrumentation, Linux execution, or performance experiment was run for this note.

## Distinct unresolved question

R26 proves interception of **one deliberately synthetic** PE-origin `syscall`; R90 discusses the **guest/host provenance and containment boundary**. Neither establishes how a real Windows user-mode caller identifies an NT service, what an `Nt*` versus `Zw*` export promises, or how an independently implemented subsystem should handle version-sensitive service stubs. This note defines that *service identity and caller-mode* boundary without treating a Linux syscall number as an NT service.

## Primary-source findings

1. **VERIFIED — Microsoft's documented Nt/Zw distinction is caller-mode dependent.** Microsoft states that calls to Nt and Zw versions of a native system service can differ when called from **kernel mode** because of how input parameters are interpreted using the previous processor mode; from **user mode** the Nt and Zw versions behave identically. This is not a license to ignore probing, access checks, or handle validity in a guest-facing implementation. Source: [Using Nt and Zw Versions of the Native System Services Routines](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-nt-and-zw-versions-of-the-native-system-services-routines) and [PreviousMode](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/previousmode).
2. **VERIFIED — NtQueryInformationProcess is explicitly a compatibility-sensitive internal interface.** Microsoft documents that it may be changed or unavailable in future Windows versions and recommends public alternatives where possible; its user-mode entry is in `Ntdll.dll`. A fixed cross-version native service surface must therefore not be inferred merely from the current documented prototype. Source: [NtQueryInformationProcess](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess).
3. **VERIFIED — NtQueryInformationThread has the same public-API preference and internal-change warning.** Microsoft documents its `Ntdll.dll` user-mode export and points to public replacements for supported information classes. Source: [NtQueryInformationThread](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationthread).
4. **VERIFIED — NTSTATUS is a typed result domain, not Linux errno.** Microsoft's NTSTATUS definitions distinguish severity and facility fields; `RtlNtStatusToDosError` converts NT status values to Win32 error codes. A native `Nt*` boundary must preserve the native status rather than unconditionally translating it to a Win32 `GetLastError` value. Sources: [MS-ERREF NTSTATUS](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/705fb797-2175-4a90-b5a3-3918024b10b8), [RtlNtStatusToDosError](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlntstatustodoserror).
5. **VERIFIED — Windows PE import resolution is by export name/ordinal, not by kernel service number.** The PE specification defines import lookup/name tables and import address table resolution. A guest importing `ntdll!NtQueryInformationProcess` presents a named DLL-export dependency to the loader; a separately compiled raw syscall fixture does not. Source: [Microsoft PE format, Import Directory](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#import-directory-table).

## Architecture implications (HYPOTHESES / proposed policy)

**H1 — stable semantic service keys.** Represent each implemented NT operation internally by a versioned semantic identifier (for example `NtQueryInformationProcess` plus information class and contract version), *not* by a raw x86-64 service-number integer. Resolve imported `ntdll` exports to ABI-correct entry points. If raw syscall compatibility is attempted, introduce an explicitly version-scoped translation table, derived and tested against an identified Windows build. This is a design proposal, not evidence that all Windows builds can be supported.

**H2 — guest user mode is the initial trust contract.** The first runtime executes user-mode PE code, not Windows kernel drivers. Its NT entry points should enforce user-mode pointer validation, handle rights, and object security. Kernel-mode Zw/PreviousMode behavior belongs to a separate, currently out-of-scope driver-compatibility design. Merely naming a host function `Zw*` must not bypass checks.

**H3 — no service-number fallback.** An unknown Windows-origin raw service number must not be forwarded into Linux's syscall table. Treat it as an unsupported guest operation with an explicit, documented failure path; preserve the distinction between unsupported service, unsupported information class, and access-denied outcomes after Windows-reference verification.

**H4 — exported stubs and raw syscall paths must converge.** If both a named ntdll import and a raw guest syscall are supported for the same Windows build, route both to one semantic implementation while recording their distinct entry provenance. Avoid claiming equivalence until pointer probing, buffer length, information-class behavior, and NTSTATUS results match Windows.

## Falsifiable gates — NOT RUN

1. On an isolated, licensed Windows x64 reference VM, record exact OS build and architecture. Resolve documented ntdll `NtQueryInformationProcess` and `NtQueryInformationThread` exports by name and invoke selected documented information classes with valid, undersized, null, and inaccessible buffers. Record return status, returned lengths, and observable output; do **not** assume stable behavior across builds.
2. Repeat against at least two Windows builds. Compare semantic results independently of raw syscall numbering. If raw service numbers are inspected, archive only observed, build-attributed evidence; do not construct a purported universal number table.
3. On Linux, create a freestanding PE fixture with two named ntdll imports and separately a synthetic raw-syscall fixture. Confirm loader binding, Microsoft ABI register preservation, thread-specific state, status translation boundaries, and rejection of unknown raw service numbers. Keep synthetic and Windows-derived service identifiers visibly distinct.
4. For any proposed Zw alias, test that an untrusted guest cannot bypass user-mode validation, security checks or handle rights by selecting a different export spelling.
5. Under nested callbacks and multiple threads, verify dispatch context is restored and host Linux syscalls never accidentally use guest NT service identifiers. The security/selector tests remain owned by R90.

Record PASS/FAIL/SKIP per case, reference OS build, CPU, binary hashes, compiler/linker versions, service export names, input structures and statuses. **No such results exist in this note.**

## Decision boundary

**OPEN:** Select an initial supported Windows-build/API contract and a minimal set of independently implemented ntdll services before any raw-number mapping. Named import binding is a separate, more controllable milestone than raw syscall compatibility. Neither path alone establishes Windows application compatibility.

**Nonduplication:** R26 covers the one-trap proof; R90 covers syscall provenance and sandbox limits; R52 covers generic NTSTATUS/Win32/HRESULT conversion; R44/R51 cover PE import/export mechanics. R98 adds the native **service identity, caller-mode, and build-scoped dispatch** acceptance criteria.
