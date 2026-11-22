# R242 — NTDLL loader DLL-notification registration and callback lifetime gates

Status: PRIMARY-SOURCE DOCUMENTATION REVIEW ONLY (2026-11-22). No Windows reference execution, Linux prototype, or benchmark performed.

## Scope and distinction from existing work

R45 establishes DllMain serialization and loader-lock restrictions; R27 covers TLS callbacks; R32 and R238 cover module resolution. This study addresses a separate observable interface: **LdrRegisterDllNotification / LdrDllNotification / LdrUnregisterDllNotification**, including callback publication, module identity and teardown. An ordinary DllMain lifecycle implementation does not automatically implement this registration interface.

## VERIFIED — published Microsoft contracts

1. `LdrRegisterDllNotification(Flags, NotificationFunction, Context, Cookie)` registers a callback; `Flags` **must be zero**; successful registration returns `STATUS_SUCCESS`; the cookie is used for unregistering. Microsoft documents availability starting with Windows Vista / Server 2008, and warns the interface may change or disappear without notice. [S1]
2. The callback receives a reason, a pointer to notification data and the registration context. Documented reasons are `LDR_DLL_NOTIFICATION_REASON_LOADED` (1) and `LDR_DLL_NOTIFICATION_REASON_UNLOADED` (2). The loaded/unloaded data contain reserved flags, full/base DLL names as pointers to `UNICODE_STRING`, the module base and image size. Microsoft states the callback runs **before dynamic linking takes place**, and explicitly warns that calling functions in *any other module* from it is unsafe. [S2]
3. `LdrUnregisterDllNotification(Cookie)` cancels a previous registration. Microsoft documents `STATUS_SUCCESS` on success and `STATUS_DLL_NOT_FOUND` if the callback is not found. [S3]
4. Microsoft separately documents that `DllMain` runs with the loader lock held and warns of lock-order inversion. That is a **related constraint**, not a direct statement of the exact locking implementation used for every DLL notification callback. [S4]
5. Microsoft lists no associated public header for the register/unregister routines; it identifies `Ntdll.lib` in the WDK or dynamic resolution via Ntdll.dll. Consequently, a prototype relying on an undocumented structure layout copied from an arbitrary SDK version is not a stable ABI proof. [S1][S3]

These statements are documentation facts, **not** findings from a Windows trace.

## HYPOTHESES / design consequences (NOT VERIFIED)

- Guest process loader state likely needs a registration table whose entries contain callback address, context, cookie identity and lifetime, independent of Linux `dlopen` hooks. A host ELF loader callback is not a substitute for guest PE module notifications.
- Loader publication should distinguish *mapped*, *notified*, *imports resolved*, *initialized*, *visible to callers* and *unloaded*. The documentation's pre-linking language is insufficient by itself to determine the complete event order relative to import binding, TLS callbacks, DllMain and concurrent queries.
- Callback dispatch must not outlive the PE module containing callback code. This implies module-lifetime and unload-race coordination, but exact unregister/reentrancy behavior requires Windows-reference evidence.
- A guest callback may inspect names/base/size but should not be assumed safe to invoke arbitrary guest APIs during notification; documentation specifically warns against cross-module calls.

## Proposed Windows-reference differential tests — NOT EXECUTED

Use a disposable Windows x64 process and a minimal EXE + DLL A + DLL B, with callback code entirely in the registering module and preallocated event buffers. Record Windows build, compiler, PE hashes and all return statuses.

1. Register callback with zero flags; load/unload A, log reason, thread identity, context pointer, full/base names, base and size. Verify the documented data shape and that loaded/unloaded notifications occur; do not assume exact global order.
2. Register two callbacks and load B; record order on each tested Windows build, but treat ordering as empirical/version-sensitive unless separately specified.
3. Unregister one cookie, then load A; verify the absence/presence of callbacks against a reference run. Probe repeated unregister, invalid cookie and nonzero flags in isolated cases; do not predict undocumented error codes.
4. Have one thread repeatedly load/unload A while another registers/unregisters callbacks. Use watchdogs, bounded iterations and a preallocated lock-free trace; investigate whether any callback executes after a successful unregister returns.
5. Load A with dependencies and a TLS callback; record notification relative to TLS and DllMain using separate minimal event logs. Avoid calls to other modules *from the notification callback*.
6. Register a callback residing in a DLL, then test its unload only after unregistering. A deliberately unregistered-unload variant belongs in a crash-isolated process and is a safety characterization, not a normal compatibility test.
7. Verify registration visibility for initial process dependencies versus subsequent explicit loads; do not assume callbacks retroactively report modules loaded before registration.

## Linux implementation and security gates

- Validate callback pointers and writable cookie destinations as guest addresses; reject malformed guest pointers without letting a guest overwrite host memory.
- Keep registration/cookie identity process-scoped, generation-safe and synchronized with guest module lifetime. Never expose host function pointers or ELF loader internals through guest callback data.
- Ensure notification data strings are guest-accessible with lifetime bounded to the callback; test mutation, recursive load attempts and cancellation races against reference behavior before choosing implementation details.
- Do not call PE callbacks from Linux signal handlers or while holding unrelated host locks; the actual guest loader locking protocol must be specified and audited.
- Distinguish Windows-reference contract tests from Linux-only safety tests. No compatibility or speed claim follows from implementing the table.

## Unresolved research frontier

- Ordering and locking guarantees among callbacks, TLS callbacks, import resolution, DllMain and recursive loads on target Windows versions.
- Concurrent unregister versus in-flight callback semantics; cookie reuse and invalid-cookie behavior.
- Loader shutdown and failed-attach notification behavior.
- Exact module-name normalization and event visibility under API-set/SxS redirection.
- Whether version-specific loader notification behavior should be an explicit compatibility profile.

## Primary sources (checked 2026-11-22)

- [S1] Microsoft, *LdrRegisterDllNotification function*, https://learn.microsoft.com/en-us/windows/win32/devnotes/ldrregisterdllnotification
- [S2] Microsoft, *LdrDllNotification callback function*, https://learn.microsoft.com/en-us/windows/win32/devnotes/ldrdllnotification
- [S3] Microsoft, *LdrUnregisterDllNotification function*, https://learn.microsoft.com/en-us/windows/win32/devnotes/ldrunregisterdllnotification
- [S4] Microsoft, *Dynamic-Link Library Best Practices*, https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices

No code reused; no experiments claimed.
