# R9 — Exception unwind and ntsync source verification

Date: 2026-11-12. Verified documentation research; no unwind execution test.

## Windows x64 exception unwinding (documented)
Microsoft specifies image-relative RUNTIME_FUNCTION entries in PE .pdata, referring to UNWIND_INFO (typically .xdata). On an exception, the dispatcher locates the function for the control PC, recognizes prolog/epilog regions, processes unwind operations and invokes language-specific handlers where applicable. Leaf functions without entries may be unwound as a simple return. Dynamically generated code may register function tables via RtlAddFunctionTable / RtlInstallFunctionTableCallback.

Sources:
- https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64
- https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention

## Implications (hypotheses pending conformance)
- Linux signal interception alone cannot supply full Windows SEH/C++ unwinding.
- Cross-ABI frames must either have compatible unwind behavior or explicitly delimit supported unwind domains.
- A first-stage exception engine may validate metadata and walk frames without language-specific dispatch; a real app requires more.
- Fuzz malformed function tables, chained unwind metadata, stack limits and alignment.

## ntsync source correction
An earlier version of this note suggested an ntsync UAPI revision hazard from differing documentation names. **That inference is withdrawn.** Direct checks of upstream Linux v6.18, v6.19 and master revealed **identical UAPI header blob SHA 6d06793512b150a80018618b698537a0e3793e54**, including the authoritative names NTSYNC_IOC_MUTEX_KILL and NTSYNC_IOC_EVENT_READ. At the same revisions, the driver implementation uses these names. The v6.18/v6.19 documentation mistakenly uses NTSYNC_IOC_KILL_OWNER / NTSYNC_IOC_READ_EVENT in section headings despite also mentioning the correct mutex name in introductory prose. This is currently a **documentation error**, not an established ABI change. See [R8](R8_NTSYNC_VERSION_DRIFT.md) for source blob SHAs and evidence.

Do not add unsupported ioctl-name shims for this discrepancy. Pin actual headers, compare numeric ioctl values and structs, compile with versioned toolchains, and test behavior against installed kernels before inferring differences.

## Decision
Windows exception support stays a separate correctness gate; ntsync compile-time and runtime conformance are required, but no header ABI drift was established in audited upstream v6.18, v6.19 or master.
