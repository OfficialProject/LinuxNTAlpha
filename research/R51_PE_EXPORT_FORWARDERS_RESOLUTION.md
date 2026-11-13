# R51 — PE export forwarders, ordinals, and resolution boundaries

Status: **DOCUMENTATION REVIEW / PROPOSED CONFORMANCE TESTS ONLY**. No Windows-reference execution, Linux prototype, or benchmark was performed for this note. Reviewed 2026-11-13.

## Why this is a separate gate

R20 establishes a synthetic *import* binding to a Linux MS-ABI function. R28 binds one real NTDLL export name to an independent implementation. Neither establishes correct parsing of an arbitrary DLL's **export directory**, resolving an exported forwarder, or matching `GetProcAddress` ordinal behavior. R29 covers API-set contract names and delay imports; those are adjacent but different problems.

## Primary-source contracts

**VERIFIED — PE/COFF specification (documentation).** The export directory includes an export address table (EAT), an export name pointer table, and an export ordinal table. The ordinal table contains *unbiased indexes* into the EAT; the export directory's `OrdinalBase` is added to produce the public ordinal. The name pointer table is sorted lexically, enabling binary search. The number of named exports need not equal the number of EAT entries.

**VERIFIED — PE/COFF specification (documentation).** An EAT entry can be an export RVA pointing to code/data or a *forwarder RVA* pointing to a null-terminated ASCII string within the export section. The specification illustrates forwarders in forms such as `NTDLL.RtlAllocateHeap` and `OTHERDLL.#27`. An export RVA must not be treated as executable code simply because it appears in the EAT.

**VERIFIED — Microsoft GetProcAddress documentation.** The caller may request an exported function by name or ordinal. For ordinal lookup, the ordinal must be in the exported range, but Microsoft explicitly warns that nonconsecutive ordinals can produce an invalid non-NULL address even when a number lies in the range. Thus a compatibility implementation must not silently invent a guarantee that every ordinal in the interval is valid.

**VERIFIED — Microsoft GetProcAddress documentation.** An exact exported name's spelling and case matter. The documentation instructs using the spelling/case from the exporting module's definition or declaration.

Sources:
- Microsoft, *PE Format*, section “The .edata Section (Image Only)”, including “Export Directory Table”, “Export Address Table”, “Export Name Pointer Table”, and “Export Ordinal Table”: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- Microsoft, *GetProcAddress function (libloaderapi.h)*: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress

## Architectural implications (INFERENCE, not measured)

1. Keep **module discovery**, **export parsing**, and **forwarder target resolution** separate. A PE parser can recognize a forwarder without loading any DLL or providing any Windows API.
2. Resolve name -> name-pointer index -> unbiased EAT index, or public ordinal -> subtract `OrdinalBase` -> EAT index, with overflow and bounds checks at each step. Never equate name-table index with EAT index.
3. Classify the EAT target using the PE export-directory address range before interpreting it as a callable pointer. Validate that a forwarder string is terminated within the mapped image and the permitted export range; malformed inputs must fail closed.
4. Forwarders imply a *dependency graph*, not just a symbol table. Cycles, excessive chain depth, absent modules, and absent target exports need explicit failure behavior and resource limits. The limits are proposed implementation safeguards, **not** claimed Windows-compatible limits.
5. A forwarder target that resembles an API-set contract still needs the separate R29 API-set mapping policy; blindly treating the string as a Linux pathname or physical DLL name is incorrect.
6. The Windows export lookup behavior and Linux host symbol resolution should remain isolated: a host MS-ABI thunk is not evidence that the corresponding Windows module, handle, or loader state exists.

## Proposed parse-only test matrix (NOT EXECUTED)

| Fixture | Expected local parser behavior (proposed) |
| --- | --- |
| Two named exports whose name-pointer order differs from EAT order | Resolve each via its ordinal-table index |
| Export with ordinal but no name | Ordinal resolution only |
| Nonzero `OrdinalBase` | Correct public ordinal translation |
| Forwarder to `MOD.Func` | Return typed forwarder record, never raw code pointer |
| Forwarder to `MOD.#123` | Preserve ordinal target as ordinal, not name |
| Forwarder string without in-range NUL | Reject |
| EAT/name/ordinal table arithmetic overflow or out-of-image range | Reject |
| Repeated forwarders A -> B -> A | Terminate under explicit cycle/depth policy |
| Duplicate or unsorted export names | Flag malformed/noncanonical input; verify Windows behavior separately before making a compatibility claim |
| Ordinal hole | Do not assume valid callable target; differential-test against Windows |

## Windows-reference gates (NOT EXECUTED)

Build minimal Windows DLL fixtures with a .def file containing named, ordinal-only, and forwarded exports. Record results from `LoadLibraryExW` / `GetProcAddress` on specified Windows build(s), including `GetLastError` where documented and stable. Test forwarder target load behavior, failure cases, exact-case lookup, and ordinal holes separately. Capture DLL versions and OS build. Only then decide which malformed-image behaviors require Windows compatibility and which can remain stricter parser policy.

## Decision

**OPEN:** export-forwarder traversal policy, module loading integration, and compatibility handling of malformed export directories. **Do not claim:** full DLL loading, API-set support, safe untrusted PE execution, or Windows `GetProcAddress` equivalence from this documentation review.
