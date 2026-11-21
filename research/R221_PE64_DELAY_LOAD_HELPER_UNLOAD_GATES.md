# R221 — PE64 delay-load helper, import mutation and unload gates

Status: RESEARCH ONLY — no implementation or experiment.
Reviewed: 2026-11-21.
Scope: x86-64 PE image delay imports; deliberately separate from ordinary eager import resolution and from R173 DLL search policy.

## Verified primary-source contracts

1. The Microsoft PE/COFF format defines the Delay Import Descriptor data-directory entry and an eight-DWORD descriptor: Attributes, Name, Module Handle, Delay IAT, Delay INT, Bound Delay IAT, Unload Delay IAT, and Time Stamp. Bit 0 of Attributes selects RVA-based fields; other bits are reserved. The specification also describes an older pointer-based representation when the bit is clear. A PE64 parser must not blindly treat all fields as 64-bit pointers or assume that an image is safe merely because the descriptor fits. [P1]
2. The delay IAT initially points through thunks; the delay-load helper patches the selected slot with the resolved function. The optional unload IAT is a saved copy of the initial thunk table, intended to restore deferred resolution after explicit unload. The bound delay IAT and timestamp are optional and not proof that a referenced DLL is currently mapped at a fixed address. [P1]
3. Microsoft's MSVC helper contract names `__delayLoadHelper2(PCImgDelayDescr, FARPROC*)`. The helper checks a stored module handle, invokes LoadLibrary and GetProcAddress as needed, and writes the resulting function address to the IAT slot. Notification and failure hooks can change resolution behavior; helper failures can raise structured exceptions. This is linker/runtime-library functionality, **not** a guarantee that the OS loader eagerly resolves delay imports. [P2]
4. Microsoft's documented explicit unload helper `__FUnloadDelayLoadedDLL2` restores the original IAT from the unload table, frees the DLL, and clears helper state when the corresponding unload record exists. The documented DLL-name matching for this helper is case-sensitive. [P2]
5. `QueryOptionalDelayLoadedAPI` is an additional documented feature-probing surface. Its availability and results cannot be inferred merely from the existence of a descriptor. [P3]

## Architectural consequence (reasoned inference, not verified implementation)

A compatible guest runtime should initially **parse and validate** delay descriptors but leave their call thunks intact. The guest's MSVC-linked helper (or an independently compatible implementation when needed) must reach guest-compatible LoadLibrary/GetProcAddress/FreeLibrary, module identities, SEH and hook callbacks. This crosses the PE parser, guest loader, export resolver, exception dispatcher and module lifetime manager. It is incorrect to label delay-import support complete when only the descriptor parser works.

### Proposed state model (hypothesis)

Track each delay descriptor and slot with original thunk, current target, owning guest module generation and unload eligibility. Distinguish descriptor parse validity from helper-owned runtime state. In a concurrent guest, guard IAT mutation and unload against concurrent calls and stale module generations. **Do not claim that a particular locking algorithm reproduces Windows behavior** until Windows-reference concurrency tests establish the required guarantees. Host `dlopen` handles and ELF symbol addresses are not substitutes for guest HMODULE identities or guest export resolution.

### Security boundary

All RVA/VA conversions, pointer arithmetic, table lengths, thunk indexes and writable IAT destinations require checked bounds and mapping-permission validation before access. Do not accept a guest-controlled delay descriptor as authority to write arbitrary host memory. Treat hook-provided replacement targets as guest execution addresses subject to the guest's applicable control-flow policy (see R216/R217). Reject malformed descriptors deterministically in parse-only mode without executing guest code.

## Proposed, unrun conformance matrix

- **Parser fixtures:** RVA-based descriptor, older VA-based descriptor, reserved attribute bits, truncated directory, invalid string RVA, unterminated INT, IAT/INT count mismatch, overflow, missing optional BIAT/UIAT, out-of-image pointers. Record reject/accept policy separately from Windows' behavior.
- **Windows reference:** MSVC-built /DELAYLOAD fixture with absent DLL never called; first call; repeat call; name and ordinal imports; forwarded export; missing DLL and missing procedure; hooks substituting a module or target; exception recovery.
- **Unload:** optional UIAT present versus absent, repeated explicit unload, exact-case versus altered-case name, subsequent first call after unload, concurrent callers and unload. Capture helper return values, callbacks, IAT contents and module lifetimes.
- **Linux PE guest:** replicate each accepted Windows fixture through the guest module resolver and exception path. Compare observable return values, hook order, IAT transitions, failure/exception categories and unload effects. Separate correctness from performance.
- **Performance (only after correctness):** cold and warm first-call latency, steady-state patched-call overhead, contention and memory footprint, with toolchain, Windows build, Linux kernel and CPU recorded.

## Open questions

1. Exact Windows behavior for malformed legacy VA descriptors, concurrent first-call/unload races and helper customization across MSVC versions remains **OPEN**.
2. Whether current project PE parser exposes enough mapped-image metadata to safely validate optional delay tables remains **OPEN** until code audit.
3. Interactions with guest CFG IAT protection and runtime export-target changes remain **OPEN** (R216/R217).
4. Choice of guest module-lock strategy remains **HYPOTHESIS**; no benchmark or Windows experiment was performed.

## Primary sources

- [P1] Microsoft, *PE Format*, “Delay-Load Import Tables (Image Only),” https://learn.microsoft.com/en-us/windows/win32/debug/pe-format (accessed 2026-11-21).
- [P2] Microsoft, *Understand the delay load helper function*, https://learn.microsoft.com/en-us/cpp/build/reference/calling-conventions-parameters-and-return-type (accessed 2026-11-21).
- [P3] Microsoft, *QueryOptionalDelayLoadedAPI*, https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi2/nf-libloaderapi2-queryoptionaldelayloadedapi (accessed 2026-11-21).

No Windows or Linux binaries were built or executed for this note.
