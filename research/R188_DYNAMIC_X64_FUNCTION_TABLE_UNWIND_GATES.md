# R188 — Dynamic x64 function-table registration and unwind ownership gates

Status: **primary-source documentation analysis only** (2026-11-19). No Windows or Linux experiment was performed. This note complements existing x64 exception/SEH work; it does not claim a working unwinder.

## Verified contracts

1. The Microsoft PE/COFF specification describes the x64 image exception directory as sorted `RUNTIME_FUNCTION` records (begin RVA, end RVA, unwind-information RVA). The x64 exception-handling specification explains that unwinding searches for a matching function entry, treats an absent entry as a leaf-function case, interprets prolog/epilog position, and follows chained unwind information. Therefore an implementation cannot assume that frame-pointer walking or DWARF metadata alone reproduces the PE exception mechanism. [S1, S2]
2. `RtlAddFunctionTable(FunctionTable, EntryCount, BaseAddress)` registers unwind metadata for dynamically generated code; Microsoft specifically distinguishes relatively stable generated code from callback-based dynamic generation. `RtlDeleteFunctionTable` removes a table previously registered by add or callback registration. [S3, S4]
3. `RtlInstallFunctionTableCallback` registers an address range and callback that resolves a runtime-function entry on demand. Its table identifier must have both low bits set; its documentation explicitly warns of deadlocks between the callback and code generator. It also supports an optional out-of-process callback DLL for debugger unwinding. [S5]
4. `RtlAddGrowableFunctionTable` accepts a partially populated sorted array, current count, maximum count and address range, and returns an opaque registration handle. It supports subsequent growth as code is generated; Microsoft documents availability starting with Windows 8. [S6]
5. The dynamic-function-table debugger inspection interface is explicitly marked subject to change/removal; it is not a stable internal ABI for the proposed runtime. [S7]

## Architectural implications (inferences, not demonstrated results)

- The guest must own a **process-scoped registry** of image and dynamic unwind ranges, including registration kind, base address, entry count, generation, and teardown state. Linux ELF unwinding is not a drop-in substitute for PE x64 unwind semantics.
- An unwind lookup should retain a safe reference to metadata while code is being unloaded or a registration is removed. The exact Windows race behavior is **unknown** and requires reference testing; do not invent a guarantee that deletion waits for active unwinders.
- Callback-based lookup must not hold the loader/code-generator lock while invoking guest callbacks. This is a proposed deadlock-avoidance design, not a verified Windows locking rule.
- Growable tables need a publication protocol so readers never observe a new entry count before fully initialized sorted entries. The precise guest memory-ordering and synchronization rules remain to be measured.
- Debugger/out-of-process callback support is a separate capability gate; successful in-process unwinding does not imply debugger parity.
- All metadata RVAs, lengths and range arithmetic must be checked against mapped guest memory before dereference. Reject malformed entries and cyclic chained unwind references without permitting arbitrary host-memory reads. This is a proposed security policy.

## Priority conformance gates (not executed)

1. **Windows reference:** compile MSVC x64 fixtures for static `.pdata`, nested nonleaf frames, prolog/epilog exceptions, and chained unwind records; capture `RtlLookupFunctionEntry`, `RtlVirtualUnwind`, exception dispatch and stack traces.
2. **Dynamic registration:** register one fixed table, query its address, delete it and query again; compare errors/results and avoid assuming post-delete behavior beyond documentation.
3. **Callback path:** register two generated functions in a callback-managed range; log callback invocations, recursive callback behavior, and code-generation lock interactions with watchdog protection.
4. **Growable path:** start with zero entries, grow sorted records, check boundary addresses and lookup visibility, then test concurrent readers/writers and table deletion. Record OS build and API availability.
5. **Lifetime/security:** test invalid count/range, duplicate/overlapping registrations, registration while unloading, metadata mutation, and concurrent deletion. Establish observed Windows behavior before specifying compatibility.
6. **Linux fixture:** implement a minimal guest registry and PE x64 unwind decoder with sanitizers; compare decoded caller contexts to Windows fixtures. Do not label host backtraces as guest SEH conformance.

## Open questions

- Precedence among overlapping image, fixed, growable and callback registrations is not established here.
- Exact deletion versus in-flight callback/unwind synchronization is not established here.
- Callback invocation reentrancy, debugger behavior and interaction with guest exception handlers require separate reference experiments.
- Any measurable overhead or performance benefit remains unknown.

## Primary sources

- [S1] Microsoft, *PE Format*, `.pdata` and exception directory: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- [S2] Microsoft, *x64 exception handling*: https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64
- [S3] Microsoft, *RtlAddFunctionTable*: https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtladdfunctiontable
- [S4] Microsoft, *RtlDeleteFunctionTable*: https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtldeletefunctiontable
- [S5] Microsoft, *RtlInstallFunctionTableCallback*: https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlinstallfunctiontablecallback
- [S6] Microsoft, *RtlAddGrowableFunctionTable*: https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtladdgrowablefunctiontable
- [S7] Microsoft, *RtlGetFunctionTableListHead*: https://learn.microsoft.com/en-us/windows/win32/devnotes/rtlgetfunctiontablelisthead
