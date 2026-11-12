# R9 — Exception unwind and ntsync revision hazards
Date: 2026-11-12. Evidence: official Microsoft and Linux documentation. No benchmarks executed.

## Windows x64 exception unwinding (VERIFIED)
Microsoft specifies sorted image-relative RUNTIME_FUNCTION entries in PE .pdata, referring to UNWIND_INFO (typically .xdata). On exception, the dispatcher finds the function for RIP, recognizes prolog/epilog, calls language-specific handlers when present, and reconstructs caller context using unwind operations. Leaf functions without entries are treated as return-address-on-stack. Dynamically generated code may register function tables via RtlAddFunctionTable / RtlInstallFunctionTableCallback.

Sources:
https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64
https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention

## Implications
- Signal interception alone cannot supply correct SEH or C++ unwinding.
- Windows ABI bridge frames must be unwind-aware or explicitly terminate supported unwind domains.
- A first-stage exception engine can validate unwind metadata and walk frames without attempting language-specific handlers; executing a real program with exception handling requires more.
- Fuzz malformed function-table entries and chained unwind info; check bounds, cycles, alignment and stack limits.

## NTSYNC UAPI revision warning (VERIFIED)
Official docs at /userspace-api/ntsync.html and /7.0/userspace-api/ntsync.html show differing ioctl macro names and struct member ordering. The docs at /next/ show yet another layout. This is a serious source-version hazard for any adapter.

Sources:
https://docs.kernel.org/userspace-api/ntsync.html
https://docs.kernel.org/7.0/userspace-api/ntsync.html
https://docs.kernel.org/next/userspace-api/ntsync.html

## Required follow-up
1. Diff pinned include/uapi/linux/ntsync.h versions, not only documentation.
2. Determine if changed names are aliases and whether binary struct layout actually changed.
3. Compile ABI assertions for sizeof and offsetof on each supported kernel-header revision.
4. Test ioctl compatibility against real kernels; don't infer binary breakage from doc drift alone.
5. Capture source commit hashes and benchmark environments before drawing performance conclusions.

## Decision
Keep exception support and ntsync adapter versioning as explicit acceptance gates. No performance or binary compatibility claims are established by this note.
