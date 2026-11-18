# R159 — Activation-context stack, callback propagation and side-by-side binding gates

Status: PRIMARY-SOURCE DOCUMENTATION REVIEW (2026-11-18). No Windows-reference run, Linux experiment, or implementation is claimed.

## Scope and relationship to earlier work

This investigates a distinct missing dependency between PE manifest loading, thread creation, asynchronous callbacks, and guest DLL/COM resolution. It does **not** supersede the existing PE loader, thread bootstrap, or threadpool lifetime notes: those address other layers. The key risk is that a loader which resolves every module by a single process-wide name map may execute x86-64 PE code successfully while selecting the wrong assembly when a guest activation context changes.

## VERIFIED — published Microsoft contracts

1. `CreateActCtx` creates an activation context from an `ACTCTX` description; undefined `ACTCTX.dwFlags` bits must be zero. A successful handle starts with one reference. `ActivateActCtx` pushes the context onto the **calling thread's** stack and returns a cookie for pairing with `DeactivateActCtx`. The top frame determines side-by-side redirection for relevant DLL, window-class, COM, type-library and mutex bindings. A pushed null context selects original-manifest defaults over lower stack frames. [CreateActCtxA](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createactctxa); [ActivateActCtx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-activateactctx).
2. Normal deactivation requires reverse activation order. A valid cookie in a lower frame with flags zero raises `STATUS_SXS_EARLY_DEACTIVATION`; a missing cookie raises `STATUS_SXS_INVALID_DEACTIVATION`. With `DEACTIVATE_ACTCTX_FLAG_FORCE_EARLY_DEACTIVATION`, a lower-frame cookie removes intervening frames too, while a top-frame cookie returns `ERROR_INVALID_PARAMETER`. Thus a generic host stack pop cannot reproduce the documented failure paths. [DeactivateActCtx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-deactivateactctx).
3. Activation contexts are reference counted; activation/deactivation themselves adjust references, and callers release their creation reference with `ReleaseActCtx`. Releasing an application-owned reference is not equivalent to forcibly removing every active frame. [Reference counting activation contexts](https://learn.microsoft.com/en-us/windows/win32/sbscs/reference-counting-activation-contexts).
4. Microsoft documents **per-thread activation stacks**, automatic propagation of the current activation context to a new `CreateThread` thread, propagation to asynchronous callbacks and intra-process window messages, and cleanup of callback activation state after invocation. These are published behavioral requirements, not measurements of any particular Windows build. [Threads, asynchronous procedures and window messages](https://learn.microsoft.com/en-us/windows/win32/sbscs/using-threads--asynchronous-procedures--and-window-messages).
5. The public activation-context API includes `FindActCtxSectionString`, `FindActCtxSectionGuid`, `QueryActCtxW`, `QueryActCtxSettingsW`, `GetCurrentActCtx`, `AddRefActCtx`, `ReleaseActCtx` and `ZombifyActCtx`. Implementing only the push/pop pair does not cover its observable query and resolution surfaces. [Activation context reference](https://learn.microsoft.com/en-us/windows/win32/sbscs/activation-context-reference).

## HYPOTHESIS — candidate Linux-hosted architecture (not selected)

Keep a guest-owned, refcounted activation-context object and a per-guest-thread activation stack. Store a unique generation-tagged cookie per activation frame, not merely the activation-context handle. Capture an explicitly retained activation-context reference when guest work is queued; restore the captured context around callback dispatch and release it on callback completion or cancellation. Keep assembly-binding decisions in a guest resolver that receives the active context; do not conflate host ELF dynamic-linker state or Linux thread-local state with guest SxS resolution. Separate context identity from individual module mappings: the same logical module name may have different context-dependent bindings.

**Unresolved:** Microsoft documentation does not by itself establish the precise propagation behavior of every individual threadpool API, callback cancellation race, fiber switch, exception unwind, or manifest edge case. Do not generalize the broad SxS overview into an unconditional rule for every callback mechanism. Likewise, a proposed generation-tagged cookie is an internal safety design, not a Windows ABI requirement.

## Falsifiable gates before implementation claims

- **Windows reference:** create two side-by-side contexts with distinguishable section lookup results; activate A, then B; verify top-of-stack lookup, null activation, normal reverse deactivation, wrong-order exception and forced deactivation. Record exact Windows build and exception codes.
- **Thread propagation:** activate A and create a child thread; compare child and parent current contexts; independently test queued callback and window-message propagation, including nested callbacks and cleanup after exceptions. Keep each callback API as a separate result.
- **Lifetime:** release creation handle while activated, then query/deactivate; test cancellation before callback starts, concurrent release and reuse of guest handle slots. No use-after-free or accidental cross-thread stack mutation is acceptable.
- **Loader integration:** construct two manifest-scoped bindings with the same apparent DLL name and different exports. Resolve under A, B and default contexts; compare with a Windows baseline before asserting correct selection.
- **Negative/security:** malformed manifests, invalid cookies, cross-thread cookie use, recursion, oversized sections and hostile lookup strings must not escape the guest namespace or corrupt the host process.
- **Performance:** only after semantic conformance, benchmark activation/lookup/callback propagation against equivalent Windows workloads; report distribution and environment, not unmeasured speedup.

## Decision impact

**OPEN:** activation-context semantics should be included in the minimal integrated runtime's loader/thread/callback interface contract before claiming general Win32 compatibility. This does not justify a Linux kernel modification. The next concrete artifact should be a pinned Windows-reference test specification or runnable fixture, not an architectural performance claim.
