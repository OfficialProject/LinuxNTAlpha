# R86 — GetModuleHandleEx module pinning and lifetime gates

Date: 2026-11-14. Status: **VERIFIED FROM SOURCE** for documented Win32 API contracts; **HYPOTHESIS** for proposed LinuxNTAlpha design; **OPEN** for Windows differential behavior. No experiment was run.

## Why this is distinct

R45 covers DLL entry-point serialization; R64 covers dependency graphs and rollback; R51 covers export lookup. This note isolates the *public module-handle acquisition and lifetime contract*, especially a module obtained by code address, uncounted handles, and irrevocable pinning. A PE image mapped by R19/R20 is not evidence that these contracts work.

## Source-backed contracts

1. **Counted lookup.** `GetModuleHandleExW` normally increments the reference count of a module already loaded in the calling process; the caller balances this with `FreeLibrary`. The API does not load an absent module. Source: [Microsoft GetModuleHandleExW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw).
2. **Uncounted lookup.** `GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT` (0x2) does not increment the count and is explicitly unsafe to feed to `FreeLibrary`. The documentation warns that concurrent unloading can invalidate it before use. Source: same.
3. **Pinning.** `GET_MODULE_HANDLE_EX_FLAG_PIN` (0x1) keeps a module loaded until process termination regardless of subsequent `FreeLibrary` calls. PIN and UNCHANGED_REFCOUNT are mutually exclusive. Source: same.
4. **Address-based lookup.** `GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS` (0x4) interprets the nominal name argument as an address within a module. Name lookup without a path is ambiguous when multiple loaded modules share a basename; a null name selects the process executable. The API excludes modules loaded as `LOAD_LIBRARY_AS_DATAFILE`. Returned module handles are neither inheritable nor cross-process duplicable. Source: same.
5. **Unload and execution race.** `FreeLibrary` decrements the module reference count and unloads at zero; it must not be called from `DllMain`. Microsoft prescribes `FreeLibraryAndExitThread` when a thread must unload its own DLL and terminate, rather than two separate calls that can race. Source: [Microsoft FreeLibrary](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-freelibrary).

These are documented API promises, **not** proof of Windows' undocumented internal refcount representation or a specific order of internal locks.

## Architectural consequence — HYPOTHESIS, not an implementation

A guest-process module registry should associate each loaded image with its guest image address interval, canonical identity, load state, pin state and counted references. An address lookup must consult the guest module registry rather than Linux `dladdr`: the latter describes ELF dynamic-loader objects and cannot be assumed to identify manually mapped PE images. Guest HMODULE values must never be treated as host file descriptors or transferable kernel handles.

For an ordinary counted lookup, resolve the module and acquire the reference as one serialized operation relative to guest unloading. For UNCHANGED_REFCOUNT, return a borrowed handle without promising continued validity. For PIN, make a monotonic state transition that cannot be undone by `FreeLibrary`; preserve it through reference-count decrements. This state machine requires integration with R45/R64's loader callback lifecycle and graph references, not a standalone numeric counter.

Potential implementation hazard: mapping intervals can be reused after unload. A stale borrowed HMODULE might then denote another image at the same base. The documented race warning means a blanket lifetime guarantee would be wrong; whether and how Windows differentiates stale handles requires reference testing. Do not infer an undocumented generation-number ABI.

## Proposed conformance matrix — NOT EXECUTED

Run on a controlled Windows reference host with an independently compiled test DLL exposing a function address, plus a second DLL with the same basename in another directory where supported:

- Load once, obtain counted handle by name, release original load, verify the counted lookup retains the module; release last counted reference and observe detach.
- Obtain uncounted handle, verify lookup itself does not retain the module. Do not deliberately call `FreeLibrary` on a borrowed handle as a normal use case; isolate any negative test.
- Obtain PIN by name and by address; release ordinary references and verify module remains mapped until process exit. Check PIN|UNCHANGED_REFCOUNT rejection and `GetLastError`.
- Check FROM_ADDRESS for code, mapped data, a nonmodule pointer and a pointer just beyond image bounds. Record observed results without assuming undocumented edge behavior.
- Test null module name, extension normalization, full path, basename ambiguity, and datafile-only loading.
- Exercise concurrent counted acquisition versus unload with synchronization barriers; use a bounded stress test, record all outcomes, and never treat a crash as proof of an intended contract.
- Exercise a DLL-owned worker that uses `FreeLibraryAndExitThread`, observing unload/termination only after the documented combined operation.

Capture Windows build, architecture, compiler, test DLL hash, API return values, last-error values, module mapping observations and repetitions. Separately run an isolated LinuxNTAlpha registry state-machine test with deliberate unload/reload address reuse before executing guest PE code.

## Open questions

- Precise behavior of invalid flag combinations and failure last-error values beyond documented constraints.
- Reference-count contributions from implicit dependencies versus explicit loads; integrate with R64 before claiming graph correctness.
- Whether address-based lookup during unload callback is accepted and how loader reentrancy is serialized; avoid DllMain-hostile tests in production.
- Datafile/resource mappings and module enumeration need separate representation rather than silently entering the executable module registry.

**No runtime compatibility, security isolation, performance or correctness claim is made by this document.**
