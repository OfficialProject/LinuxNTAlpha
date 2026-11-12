# R30 — x64 unwind metadata: static image tables versus dynamic registration

Date: 2026-11-12. Status: **PRIMARY-DOCUMENTATION REVIEW ONLY**. No fixture, Windows reference run, or LinuxNTAlpha unwind implementation was executed for this note.

## Scope and distinction from R9

[R9](R9_EXCEPTION_UNWIND_NTSYNC_VERSION.md) establishes that Windows x64 exception dispatch uses image-relative `RUNTIME_FUNCTION` records and unwind metadata. This note narrows a separate implementation boundary: **discovering and validating a PE image's static exception directory is not the same as implementing the Windows runtime's function-table lookup, dynamically registered tables, or language-handler dispatch**.

## VERIFIED FROM SOURCE (Microsoft documentation)

1. Microsoft's PE/COFF format defines the **Exception Table** data-directory entry (index 3). On x64, exception directory entries are `IMAGE_RUNTIME_FUNCTION_ENTRY` records describing function start/end and unwind information; the table uses image-relative addresses. The directory's RVA and size describe image data, not host-native ELF unwind tables. [PE format, Exception Data](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
2. Microsoft's x64 exception-handling specification describes `RUNTIME_FUNCTION` entries as `BeginAddress`, `EndAddress`, and `UnwindInfoAddress`, and defines `UNWIND_INFO` version/flags, prolog size, unwind-code slots and optional handler or chained information. An exception dispatcher must interpret control-PC position, prolog/epilog state, nonvolatile register restoration and stack adjustments; copying a `.pdata` table does not implement this behavior. [x64 exception handling](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64).
3. Microsoft documents `RtlAddFunctionTable` for adding a dynamic function table and `RtlDeleteFunctionTable` for removing one; `RtlInstallFunctionTableCallback` supports callback-based lookup for dynamically generated code. Therefore an image-only `.pdata` index cannot cover every documented unwind lookup case. [RtlAddFunctionTable](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtladdfunctiontable); [RtlDeleteFunctionTable](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtldeletefunctiontable); [RtlInstallFunctionTableCallback](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlinstallfunctiontablecallback).
4. Microsoft's x64 calling convention requires caller-reserved shadow space and documents nonvolatile general-purpose and XMM registers; exception unwinding must respect the guest's Windows ABI, not assume that Linux SysV frame conventions or ELF `.eh_frame` apply to PE code. [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention).

These are *documented contracts*, not observations of a particular Windows build. Retrieval date: 2026-11-12. No upstream code was copied.

## HYPOTHESIS: staged implementation boundary

A first safe milestone is a **parse-only static exception-directory inspector** attached to the existing R23 untrusted-input gate. It can report structurally acceptable x64 `RUNTIME_FUNCTION` records without attempting to unwind a thread. A second milestone is a deliberately constrained *trusted fixture* that registers an explicit guest function-table lookup service and checks a synthetic stack-walk oracle. A third milestone requires real Windows reference traces for exception dispatch and C++/SEH interactions. None of these milestones is complete.

The runtime likely needs a registry keyed by guest code address and module lifetime, supporting static image tables and dynamic registration. This is a design proposal, not a measured necessity for the current single-function fixtures.

## Proposed falsifiable acceptance gates — NOT EXECUTED

1. **Directory validation:** bounds-check exception-directory RVA/size against the mapped image, require a multiple of the x64 entry width (12 bytes), reject wraparound, and verify each begin/end range is ordered and within executable guest image regions for the initial restricted profile. Label unsupported layouts separately from malformed ones.
2. **Unwind-info validation:** bound every referenced header and unwind-code array, account for 2-byte slot alignment and optional 4-byte-aligned handler/chained fields; validate opcode-specific extra slots, version and flags. Never follow a chained record without a recursion/visited-set limit.
3. **Negative controls:** zero-length entries, overlapping or unsorted ranges, truncated unwind-code arrays, invalid chained targets, cyclic references, malformed offsets, and out-of-image metadata. Compare results against a separately implemented parser oracle or Windows loader behavior where appropriate; not every unsupported record is necessarily invalid PE.
4. **Dynamic registry:** on trusted fixtures, add/delete tables and confirm that lookup stops returning removed records; test overlapping registrations and callback reentrancy against a version-pinned Windows reference. Do not assume undocumented precedence.
5. **Real dispatch:** build a Windows-target PE fixture with nonleaf functions, stack allocation, saved nonvolatile GPR/XMM state, and controlled exception points. Record Windows OS build, toolchain, PE metadata, guest register/stack snapshots and handler trace. Compare with LinuxNTAlpha, including failures.
6. **Cross-ABI boundary:** explicitly test whether a Windows exception traversing a Linux host callback is supported; until demonstrated, treat that transition as an unsupported boundary rather than pretending that ELF unwinding is compatible.

## OPEN

- Which Windows exception entry points and lookup interfaces are the minimum coherent API surface for first-party guest code?
- How are guest dynamic function tables synchronized with image unload, thread concurrency and signal-delivered faults without invoking unsafe host signal-handler operations?
- How are guard-page faults, stack growth, C++ personality routines and nested exceptions ordered on real Windows builds?
- What is the security policy for guest-controlled callback addresses and malformed metadata? The current PE executor is **not a sandbox**.

## Decision

Keep R9's exception gate open. Do **not** infer SEH support from existing GS/TIB, SUD, PE execution or ABI-bridge passes. Implement static unwind-directory **inspection before dispatch**, then use Windows reference tests to establish behavior. No performance or compatibility claim follows from this review.
