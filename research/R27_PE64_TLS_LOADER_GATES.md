# R27 — PE32+ TLS directory and loader initialization: specification gates

Date: 2026-11-12. Status: **DOCUMENTATION REVIEW ONLY**. No new fixture was built or executed for this note. This is a distinct gate from R24/R25 (synthetic GS/TIB reads) and R20 (one imported function); none of those tests implements PE TLS initialization.

## Verified from primary documentation

1. The PE/COFF specification defines a **Thread-Local Storage Directory** (data-directory index 9). In PE32+ its structure includes `StartAddressOfRawData`, `EndAddressOfRawData`, `AddressOfIndex`, `AddressOfCallBacks` (all 64-bit virtual addresses), followed by 32-bit `SizeOfZeroFill` and `Characteristics`. **These fields are VAs, not RVAs**; after relocation they must be interpreted as addresses in the mapped image. See Microsoft PE format, “The .tls Section”: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
2. The loader assigns a TLS index and writes it at `AddressOfIndex`; TLS template bytes from `[StartAddressOfRawData, EndAddressOfRawData)` plus `SizeOfZeroFill` form per-thread initialized TLS data. The callback pointer array at `AddressOfCallBacks` is null-terminated. Each callback uses the DLL entry-point signature and receives a reason code. See the same PE/COFF specification.
3. Windows documents **static TLS** through `__declspec(thread)` and the TLS APIs `TlsAlloc`, `TlsGetValue`, `TlsSetValue`, and `TlsFree`. These mechanisms must not be treated as interchangeable implementations: compiler-generated static TLS uses loader-managed module indices and template data, whereas the TLS API exposes dynamically allocated slots. Sources: https://learn.microsoft.com/en-us/windows/win32/dlls/thread-local-storage and https://learn.microsoft.com/en-us/windows/win32/procthread/using-thread-local-storage
4. Microsoft's dynamic-link-library guidance says the system invokes `DllMain` for process/thread attach and detach notifications, with restrictions on work performed while the loader lock is held. The documented thread-notification behavior has exceptions (for example, threads already present when a DLL is loaded do not receive retroactive `DLL_THREAD_ATTACH`). Source: https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain and https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices
5. Microsoft describes TLS callbacks as invoked by the loader during process and thread initialization; the PE format's callback array is a loader contract, not an ordinary IAT import. Source: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

## Architectural implications (reasoned, not demonstrated)

**HYPOTHESIS:** A minimal loader can initially support static TLS in a restricted set of synthetic PE32+ fixtures by validating the directory, mapping its template, allocating per-thread blocks, writing a loader-assigned index, and invoking callbacks at the correct lifecycle points. This would require a real per-thread Windows-compatible TLS/TEB strategy; R24/R25's synthetic GS:0x30 is not enough.

**HYPOTHESIS:** Separating *parse-only structural validation* from *trusted-fixture execution* remains the safest development progression. Parsing a valid TLS directory is not proof of correct callback ordering, thread lifecycle semantics, loader-lock reentrancy or Windows compatibility.

**OPEN:** Exact callback ordering relative to dependent DLL initialization, C/C++ CRT startup, EXE entry, process shutdown and recursive loads needs version-scoped Windows conformance fixtures; the cited overview documents do not establish a complete total ordering for every case. Do not encode an assumed global ordering into the loader based solely on this note.

## Proposed reproducible acceptance gates (not executed)

1. **Parse-only:** validate TLS directory bounds, PE32+ field sizes, VA-to-image translations, template end >= start, bounded zero-fill allocation, index write target, callback array termination, pointer alignment, and relocation consistency. Reject integer overflow and pointers outside the permitted image mapping. A pointer outside the image may be legal in some relocated/linked cases; classify support restrictions separately from malformed PE claims.
2. **Single thread:** compile a freestanding Windows-target PE with nonzero initialized TLS and zero-fill; confirm independent template copy and index placement before any callback or entry call.
3. **Multiple threads:** run distinct Windows-target PE worker threads with independent TLS values and verify that Linux host FS/TLS is unchanged. Preserve/restore guest GS and host signal contexts as in R24.
4. **Lifecycle:** capture callback and DLL entry notifications in a Windows reference run and compare event traces to LinuxNTAlpha for process attach, newly created threads, thread detach, process detach and a dynamically loaded module. Record OS build, compiler/linker versions and complete fixture sources.
5. **Failure paths:** inject callback exceptions, invalid index pointers, recursive loads and thread creation during initialization in isolated subprocesses; record exit codes and traces. Do not execute untrusted images in the experimental host.

## Design decision

**No loader architecture selected.** Do not conflate synthetic GS/TIB support with compiler static TLS. Prioritize a TLS-directory *parse-only* test before executable callbacks, and require a Windows reference trace before claiming loader lifecycle correctness. No experiment or benchmark is claimed by this research note.
