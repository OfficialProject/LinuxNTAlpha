# R260 — Resource-only PE mappings versus executable DLL module identity

Date: 2026-11-23. Status: **PRIMARY-DOCUMENTATION VERIFIED; DESIGN HYPOTHESES ONLY; NO EXPERIMENT**.

## Scope and why this is distinct

R127 covers resource-tree parsing and language selection; R64/R86 cover executable module dependency identity and reference-count/pinning semantics; R173 covers DLL search policy. This note isolates a different loader boundary: **LoadLibraryEx resource/data mappings are not necessarily executable DLL module instances**, and their returned HMODULE-like values cannot be treated uniformly as an executable module base or a Linux dlopen handle. This is especially relevant before generalizing R19/R20's executable PE fixtures.

## Verified primary contracts

Source A: Microsoft, *LoadLibraryExW*, https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw (retrieved 2026-11-23; documentation page last updated 2024-07-15).

- **VERIFIED (documented):** LOAD_LIBRARY_AS_DATAFILE (0x2) maps the file for data/resources, without preparing it for execution. LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE (0x40) additionally prevents other processes from opening it for write while mapped. LOAD_LIBRARY_AS_IMAGE_RESOURCE (0x20) uses image layout but skips static imports and normal initialization. Microsoft advises combining image-resource mode with a datafile mode unless image layout is specifically needed.
- **VERIFIED (documented):** If the DLL is *already loaded as an executable DLL*, resource-mode LoadLibraryEx can return that executable module and increment its DLL reference count. Otherwise a separate resource/data mapping is created without incrementing the executable DLL's reference count or publishing it in the ordinary module enumeration. Repeated resource-mode loads of the same not-executable-loaded file create separate mappings.
- **VERIFIED (documented):** FreeLibrary releases either kind of LoadLibraryEx result, but resource mappings are unmapped rather than decrementing the executable DLL reference count. Resource-mode handles can be distinguished using LDR_IS_DATAFILE, LDR_IS_IMAGEMAPPING, and LDR_IS_RESOURCE macros, which inspect low bits of the handle value. These macros must not be confused with generic NT object-handle tagging.
- **VERIFIED (documented):** Resource-only mappings do not load normal static imports or invoke normal DLL initialization. DONT_RESOLVE_DLL_REFERENCES is a different legacy flag explicitly discouraged by Microsoft.
- **VERIFIED (documented):** Plain DATAFILE mapping does not prevent another process modifying the file; EXCLUSIVE and IMAGE_RESOURCE modes have documented stronger modification constraints. Host mmap alone is not proof of matching Windows file-share and image-section semantics.
- **VERIFIED (documented):** Loading an EXE via LoadLibraryEx does not load its static imports as a normally initialized DLL.

Source B: Microsoft, *GetProcAddress*, https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress (retrieved 2026-11-23; page last updated 2024-02-06).

- **VERIFIED (documented):** GetProcAddress does not retrieve exported addresses from modules loaded with LOAD_LIBRARY_AS_DATAFILE. The documentation does not establish that every combination of resource flags is interchangeable; test exact combinations against a pinned Windows build.

## Proposed runtime boundary — HYPOTHESIS / NOT IMPLEMENTED

Use a **tagged guest module-reference record** with separate `ExecutableModule`, `DataFileMapping`, and `ImageResourceMapping` variants. Preserve the documented already-executable-loaded fast path. Keep resource mapping identities, underlying file identity, mapped bytes, guest-visible tagged handle, file-share state, and cleanup generation separate. Never feed a resource-only handle into an executable IAT resolver merely because its underlying bytes have a PE export directory.

A Linux backend may use read-only file mappings or reconstructed PE image layouts, but this is only a **candidate**. The guest loader must enforce Windows-style sharing, section lifetime, and mutation policy independently; host fd/inode semantics alone do not establish equivalence. Handle tags are guest ABI values and must not leak into host pointers without validation.

## Falsifiable Windows-reference matrix — PROPOSED, NOT RUN

1. Load the same fixture twice with each resource flag independently; record handle equality, mapping-base identity, LDR_IS_* classification and FreeLibrary order.
2. Load an executable DLL first, then resource-load that same path; reverse the order in a fresh process. Record handle identity, executable reference count indirectly through safe unload behavior, and module enumeration visibility.
3. Attempt GetProcAddress, GetModuleHandle and GetModuleFileName on each returned handle, recording exact result/GetLastError, without calling any pointer obtained from an uninitialized image.
4. Use a fixture with an intentionally missing static import and a benign DllMain side-effect marker; determine whether resource loads succeed and whether DllMain was bypassed.
5. Compare DATAFILE, DATAFILE_EXCLUSIVE, IMAGE_RESOURCE and documented combinations under a second process attempting write-open/rename; record exact share flags, OS build and filesystem.
6. Verify resource RVA lookup for file layout versus image-layout expansion, including malformed/truncated resource directories, without executing untrusted bytes.
7. Exercise stale/forged low-bit tagged handles and concurrent FreeLibrary/resource lookup in an isolated test process; check guest broker generation safety.

For each case record Windows build, architecture, fixture hash, compiler/linker versions, exact flags, return values, last error and mapping/module state. Do not infer Windows behavior from a Linux-only mock. Parse-only fuzzing must remain separate from trusted-fixture execution.

## Architecture impact and open questions

- **HYPOTHESIS:** A split executable-module registry plus resource-mapping registry prevents accidental dependency initialization and avoids conflating resource mappings with executable loader reference counts.
- **OPEN:** Exact flag-combination precedence, handle tagging and error behavior across supported Windows builds; interaction with preexisting executable loads and simultaneous unloads.
- **OPEN:** Whether a confined Linux backend can match Windows write-sharing and section behavior without an additional broker, especially with mutable host paths.
- **Decision:** No ADR-003 architecture selection, no code imported, no benchmark or Windows experiment performed.

## Next evidence-producing action

Add a trusted, nonexecuting PE resource fixture to the existing parse-only test suite; then perform the Windows-reference matrix before connecting any resource-only mode to the R20 import resolver. This is distinct from R127 resource directory parsing and from R86 executable module pinning.
