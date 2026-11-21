# R227 — Windows Error Reporting crash ownership versus Linux core-dump boundary

Research date: 2026-11-21. Status: documentation only; **no experiment run**. Scope: user-mode native Windows-compatible execution on Linux; not a claim of WER service compatibility.

## Question

Where should a Linux-hosted NT subsystem own guest crash classification, WER registrations, report contents, debugger handoff and consent, when the Linux kernel already has signal termination and core-dump facilities?

## Verified primary-source contracts

1. **VERIFIED — WER runtime exception module registration is a guest API with process-scoped state.** Microsoft documents `WerRegisterRuntimeExceptionModule` as registering an *out-of-process callback DLL*, with optional opaque context; multiple handlers are consulted until one claims a crash, otherwise native WER reporting proceeds. Registration is bounded by `WER_MAX_REGISTERED_RUNTIME_EXCEPTION_MODULES`; unregister before exit. Microsoft specifies Windows 7 / Server 2008 R2 minimum. Source: [Microsoft, WerRegisterRuntimeExceptionModule](https://learn.microsoft.com/en-us/windows/win32/api/werapi/nf-werapi-werregisterruntimeexceptionmodule), accessed 2026-11-21.
2. **VERIFIED — registration removal has observable errors.** `WerUnregisterRuntimeExceptionModule` identifies a registration by callback DLL name and context and documents `WER_E_NOT_FOUND` and `WER_E_INVALID_STATE`. Source: [Microsoft, WerUnregisterRuntimeExceptionModule](https://learn.microsoft.com/en-us/windows/desktop/api/werapi/nf-werapi-werunregisterruntimeexceptionmodule), accessed 2026-11-21.
3. **VERIFIED — WER exposes report/dump/attachment APIs, not merely an exception callback.** The Microsoft WER API reference enumerates `WerReportCreate`, `WerReportAddDump`, `WerReportAddFile`, `WerRegisterFile`, `WerRegisterMemoryBlock`, excluded memory blocks and report metadata. These are separate observable guest contracts. Source: [Microsoft WER API reference](https://learn.microsoft.com/en-us/windows/win32/api/_wer/), accessed 2026-11-21.
4. **VERIFIED — Linux core dumps are conditional host termination artifacts.** The Linux `core(5)` interface describes signal-triggered dumps, `RLIMIT_CORE`, permissions, dumpability, `core_pattern` and optional piping to a userspace collector. A pipe handler runs in the initial namespaces, not automatically in the crashed process's namespaces; it may receive a stream and metadata, and its lifetime can affect process reaping. Source: [Linux man-pages core(5)](https://man7.org/linux/man-pages/man5/core.5.html), accessed 2026-11-21.

## Architecture implications (reasoned, **not verified by implementation**)

- **HYPOTHESIS — guest crash coordinator:** retain guest exception classification and unhandled-exception disposition inside a guest runtime/service, before allowing host signal termination. Preserve guest exception record, guest thread/context, process identity and debugger state; Linux signals/core files alone cannot establish WER API results or callback selection.
- **HYPOTHESIS — report broker:** store WER module registrations and attachment/memory exclusion metadata under guest process identity; an isolated broker may collect reports and invoke authorized callbacks. A host `core_pattern` pipe is a potential fallback diagnostic source, **not** a substitute for WER and should not require global kernel configuration changes.
- **SECURITY GATE:** registered files/memory blocks and custom callback DLL paths are untrusted guest inputs. Enforce guest process rights, path resolution, memory-range validation, report size limits, privacy/consent policy and broker isolation. Never hand a host-privileged core collector unrestricted guest-controlled paths.
- **OPEN:** exact ordering among debugger first/second chance, vectored handlers, unhandled exception filters, WER custom modules and host fatal-signal delivery; Windows-version-dependent reporting policy; callback execution trust model; whether Linux-hosted PE execution can reproduce native Windows crash artifacts. The cited API pages do **not** establish this entire ordering.

## Proposed falsifiable gates (not performed)

1. **Windows reference:** on specified Windows builds, register two runtime exception modules and record registration/unregistration status, claim selection and fallback behavior; isolate test processes and use vendor-documented callback DLL exports. Verify callback DLL trust and supported OS versions.
2. **Windows reference:** separately exercise report create/add file/add dump/close and metadata/excluded-memory APIs, collecting exact HRESULT, artifacts and privacy behavior. Do not infer behavior from names alone.
3. **Linux isolation:** in an unprivileged disposable environment, record `core_pattern`, `RLIMIT_CORE`, dumpability and namespace configuration; compare a signal crash with and without core generation. Do not modify system-wide `core_pattern` on shared hosts.
4. **Guest-model security:** test registration identity reuse, callback removal races, invalid/oversized memory ranges, symlink/path traversal, duplicate attachments and concurrent crash handling. Distinguish guest-visible HRESULT from host errno.
5. **Differential decision:** accept a backend only if crash ordering, registration semantics, error values, privacy controls and teardown are reproducible. Measure overhead separately from functional correctness.

## Non-equivalences and decision

A Linux ELF core dump is not a Windows minidump, a WER report, or a registered WER callback invocation. Linux core collection must remain an optional host diagnostic facility. **No implementation architecture selected**; first establish a Windows crash/reference matrix and a guest report-ownership interface.

## Related existing research

R41 (vectored exception/continue), R189 (debug event ownership), R226 (EH continuation), R178 (fault provenance). This note addresses the distinct *post-unhandled-crash report and external callback ownership* boundary rather than exception dispatch itself.
