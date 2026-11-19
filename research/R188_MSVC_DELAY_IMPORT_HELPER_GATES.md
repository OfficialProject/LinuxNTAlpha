# R188 — MSVC delay-load helper, IAT mutation, and guest exception gates

Date: 2026-11-19. Status: **VERIFIED FROM PRIMARY MICROSOFT DOCUMENTATION** for documented MSVC linker/helper contracts; **HYPOTHESIS** for LinuxNTAlpha design; **OPEN** for executable conformance. No code was compiled or run and no performance measurements were made.

## Scope and nonduplication

R19/R20/R28 establish narrow PE fixture execution and imports; R64/R91 cover ordinary DLL dependency and lifecycle; R107/R162 cover bound imports; R173 covers DLL search policy. This note isolates **MSVC linker-generated delay imports**, which are not equivalent to ordinary eager PE import resolution. In particular, a loader that eagerly patches all import tables may accidentally erase guest-visible delay-load behavior and hook/exception semantics.

## VERIFIED — MSVC linker and helper contract

1. The MSVC linker option `/DELAYLOAD:dllname` creates a delayed import path: the target DLL is loaded on first call through a generated thunk using a helper, rather than necessarily at process image startup. The default desktop helper is supplied through `delayimp.lib`; an application can substitute its own helper. [Microsoft: Linker support for delay-loaded DLLs](https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls?view=msvc-170).
2. The default `__delayLoadHelper2(PCImgDelayDescr, FARPROC *)` checks a cached module handle, calls `LoadLibrary` and `GetProcAddress` as necessary, returns the resolved procedure address, and writes that address to the specified delay IAT slot. `ImgDelayDescr` uses RVAs and includes DLL-name, module-handle, IAT, INT, optional bound-IAT and unload-IAT fields. This is a **toolchain runtime contract**, not proof that the Windows kernel itself performs the helper's work. [Microsoft: Understand the delay load helper function](https://learn.microsoft.com/en-us/cpp/build/reference/understanding-the-helper-function?view=msvc-170).
3. Guest-defined `__pfnDliNotifyHook2` and `__pfnDliFailureHook2` can intercept helper phases. Depending on the phase, a hook may supply an alternate module handle, procedure pointer, or even a replacement helper. A compatible runtime must not silently bypass these guest calls by resolving all delay imports at load time. [Microsoft: Error handling and notification](https://learn.microsoft.com/en-us/cpp/build/reference/error-handling-and-notification?view=msvc-170).
4. Default failure handling can raise Visual C++ structured exceptions, including `VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)` and `VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND)`, with `DelayLoadInfo` carried through exception information. The guest SEH mechanism therefore intersects delay-load behavior; replacing failure with a Linux signal or a process abort is not equivalent. [Microsoft: Error handling and notification](https://learn.microsoft.com/en-us/cpp/build/reference/error-handling-and-notification?view=msvc-170).
5. With `/delay:unload`, `__FUnloadDelayLoadedDLL2` supports explicit unload and resets the delay IAT from its saved original form; without this option, resolved entries ordinarily remain in the IAT. The documented unload-name comparison is case-sensitive. The linker also supports bound delay imports and `__HrLoadAllImportsForDll`, with its own case-sensitive name comparison. [Microsoft: Linker support for delay-loaded DLLs](https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls?view=msvc-170).
6. Microsoft documents limitations: delay-load data imports are unsupported, delay-loading Kernel32.dll is unsupported, forwarded-entry binding is unsupported, and delay loading can change DLL initialization/static-TLS behavior compared with startup loading. These are constraints on the documented MSVC mechanism, not necessarily all possible third-party delay-loader implementations. [Microsoft: Linker support for delay-loaded DLLs](https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls?view=msvc-170).

## HYPOTHESIS — minimal compatibility boundary

- Parse delay-import descriptors separately from ordinary import descriptors; validate every RVA, descriptor terminator, thunk extent, writable IAT target, and optional unload/bound table before guest execution. Never trust a descriptor merely because its image has a PE signature.
- Preserve guest-generated thunks and the guest's linked helper when present. The compatibility layer should provide correct guest `LoadLibrary`, `GetProcAddress`, module identity/refcounts, and exception dispatch rather than unconditionally rewriting delay imports as eager imports.
- Track module ownership and lifetime independently of the host ELF loader. Restoring an unload IAT while another guest thread is executing a previously resolved function presents a race requiring explicit Windows-reference conformance; neither host `dlclose` nor a single atomic pointer write proves safe semantics.
- Treat helper hooks as guest code with full ABI, callback, loader-lock, and exception implications. Do not execute unknown PE fixtures as part of parser fuzzing.
- Avoid claiming that a particular delay-import descriptor must be accepted by every MSVC version; the documentation reviewed is a living MSVC 17.x view, not a pinned compiler/linker binary.

## OPEN — falsifiable acceptance matrix (NOT RUN)

1. Compile a controlled x64 MSVC fixture with `/DELAYLOAD`, a custom notification/failure hook, and one ordinary eager import. Capture linker version, `dumpbin /imports`, actual descriptors, IAT before/after first call, and exact hook order on a pinned Windows build.
2. Missing DLL and missing symbol: catch guest SEH and record exception code and `DelayLoadInfo`; contrast hook recovery with unhandled failure. Do not infer an exact status code from host `dlerror`.
3. Build with and without `/delay:unload`; verify IAT reset and second-call re-resolution, case-sensitive unload-name behavior, and concurrent callers. Use watchdogs and disposable processes.
4. Build bound/unbound variants and exercise stale timestamps, changed DLL base, and forwarded exports. Record when the helper falls back to `GetProcAddress`.
5. Parse-only malformed descriptors: out-of-image RVAs, unterminated INT/IAT, IAT overlapping read-only image data, malformed unload-IAT and arithmetic overflow. No malformed image execution.
6. Only after guest SEH and ordinary loader identity are implemented, test delay-load hooks under guest thread concurrency and compare with Windows. No latency benefit is asserted.

## Architecture consequence

**OPEN:** Delay-load support should be an explicit loader/guest-CRT integration milestone, not a side effect assumed from eager import success. Preserve the documented distinction between MSVC-generated user-mode helper behavior and OS loader responsibilities. No kernel extension is justified by these sources alone.

## Provenance

Microsoft Learn primary pages inspected 2026-11-19:
- https://learn.microsoft.com/en-us/cpp/build/reference/understanding-the-helper-function?view=msvc-170
- https://learn.microsoft.com/en-us/cpp/build/reference/error-handling-and-notification?view=msvc-170
- https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls?view=msvc-170

These are documentation contracts, not pinned MSVC source commits or experimental results. No third-party code was copied.
