# R106 — Side-by-side activation contexts and manifest-directed binding

Status: DOCUMENTATION REVIEW ONLY (2026-11-15). No Windows reference test, Linux implementation, or performance measurement was run.

## Scope and relationship to earlier work
R29/R64/R86/R91 address loader dependencies, identity, and lifetime; R106 isolates **thread-local activation-context selection and manifest-driven binding**. This is a separate input to DLL resolution, COM activation, window-class lookup, and some named-object operations. It is not equivalent to PE import-table parsing or ordinary Linux dynamic linking.

## VERIFIED — Microsoft-documented contracts
1. Windows activation contexts contain assembly redirection data, including DLL and COM information. Process creation can derive a default context from an application manifest. Contexts can redirect DLLs, COM servers, window classes, type libraries, and mutex names. [1]
2. `CreateActCtxW` creates an activation-context handle from an `ACTCTX` descriptor; unknown flag bits cause invalid-parameter failure. An `ACTCTX` source can identify a PE image with manifest resources or an associated manifest file. [2,3]
3. `ActivateActCtx` pushes a context onto the **calling thread's** activation stack, returning a cookie; `DeactivateActCtx` checks pairing and reverse-order deactivation. A pushed null context restores default-manifest behavior over lower stack entries. [4]
4. Embedded executable manifests take precedence over external application manifests for the default context. The application may otherwise begin with a system default context. [5]
5. Microsoft's DLL search-order documentation places SxS manifest redirection among the resolution stages, separately from API sets, the already-loaded module list, and KnownDLLs. Search policy varies with packaged/unpackaged application and Windows version. [6]
6. The public activation-context API also includes `FindActCtxSectionString`, `FindActCtxSectionGuid`, `QueryActCtxW`, `GetCurrentActCtx`, reference operations, and `ZombifyActCtx`. Their mere existence does **not** establish precise undocumented lifetime behavior. [7]

## Architectural implication — HYPOTHESIS, not implementation decision
A native Linux-hosted NT compatibility runtime likely needs a guest-owned activation-context representation and a per-guest-thread stack, consulted by a central guest binding resolver. The resolver would distinguish (a) manifest/SxS resolution, (b) API-set mapping, (c) already-loaded module identity, and (d) ordinary search policy. Using `dlopen` alone cannot be assumed to preserve these guest semantics. A host filesystem must not be treated as a trusted manifest source merely because it contains matching names.

Suggested logical boundaries (not a proven ABI): manifest/resource parser → assembly identity and policy resolver → immutable context record → guest thread activation stack → DLL/COM/window-class/named-object binding adapters. Keep stack cookies and handle references separate until Windows tests establish exact ownership rules.

## OPEN conformance gates (proposed; NOT EXECUTED)
- **Precedence:** identical embedded and external manifests with deliberately different assembly dependencies; compare default resolution on supported Windows versions.
- **Thread isolation:** two threads activate conflicting contexts while resolving the same symbolic dependency; verify per-thread selection and whether any process-wide loaded-module caching changes the result.
- **Stack discipline:** nested A/B contexts, wrong-order cookie deactivation, null-context push, and post-failure restoration; capture return values and `GetLastError`.
- **Lifetime:** release the context handle while it is active, then query/deactivate; distinguish reference ownership from stack membership. Avoid assuming behavior from the API list alone.
- **Resolution:** manifest-directed DLL, window-class, and COM selection with version collisions; test each API independently rather than assuming all consumers behave identically.
- **Security:** malformed/oversized XML, cyclic or missing dependencies, path traversal and symlink races in a Linux-backed assembly store; require bounded parsing and confined file resolution.
- **Versioning:** record exact Windows build, packaging model, manifest resources, architecture, and all observed results. Do not extrapolate Windows XP-era guidance to every current Windows release.

## Consequence for milestones
The current PE fixture suite demonstrates native machine-code execution and narrow import bridging, not activation-context compatibility. Treat manifest/SxS resolution as a separately testable loader gate before claiming ordinary desktop application support. No kernel modification is justified by this documentation review.

## Primary sources (retrieved 2026-11-15)
[1] Microsoft, *Activation Contexts*: https://learn.microsoft.com/en-us/windows/win32/sbscs/activation-contexts
[2] Microsoft, *CreateActCtxW*: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createactctxw
[3] Microsoft, *ACTCTXA*: https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-actctxa
[4] Microsoft, *ActivateActCtx*: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-activateactctx
[5] Microsoft, *Specifying a Default Activation Context*: https://learn.microsoft.com/en-us/windows/win32/sbscs/specifying-a-default-activation-context
[6] Microsoft, *Dynamic-link library search order*: https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order
[7] Microsoft, *Activation Context Reference*: https://learn.microsoft.com/en-us/windows/win32/sbscs/activation-context-reference
