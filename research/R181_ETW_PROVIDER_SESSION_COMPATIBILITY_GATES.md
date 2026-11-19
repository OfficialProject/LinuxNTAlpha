# R181 — ETW provider/session semantics versus Linux tracing

Status: **VERIFIED FROM PRIMARY DOCUMENTATION** for stated Windows contracts; **HYPOTHESIS** for LinuxNTAlpha design. Research only; **no experiment performed**. Reviewed 2026-11-19.

## Architectural question

Can Windows binaries using Event Tracing for Windows (ETW) register providers, enable sessions, and emit/consume events while running directly on Linux, without incorrectly treating Linux tracing facilities as an interchangeable Windows API?

## Verified source contracts

1. Microsoft's [ETW architecture and instrumentation guide](https://learn.microsoft.com/en-us/windows-hardware/test/weg/instrumenting-your-code-with-etw) distinguishes **provider**, **session**, **controller**, and **consumer**. A provider may be a user process, kernel component, or Windows kernel. Providers emit event headers and typed payloads; sessions broker enabled events to consumers. Controllers can dynamically enable and disable tracing, and provider callbacks receive enable/disable notifications. The guide describes buffering and dynamic control. These are Windows ETW contracts, not Linux tracing capabilities.
2. Microsoft's [EVENT_FILTER_DESCRIPTOR](https://learn.microsoft.com/en-us/windows/win32/api/evntprov/ns-evntprov-event_filter_descriptor) defines filter type, size, and pointer; documented filter categories include process ID, event ID, and stackwalk. Thus an implementation that merely accepts provider registration and drops filters cannot claim full session behavior.
3. Microsoft's [ETW overview](https://learn.microsoft.com/en-us/windows-hardware/test/weg/instrumenting-your-code-with-etw) identifies kernel-mode and user-mode providers. Guest-visible kernel provider events must not be fabricated from Linux tracepoints without a verified schema and semantic correspondence. Linux host events and Windows guest events have different identities and access boundaries.

## Architecture implications — hypotheses, not findings

**HYPOTHESIS:** Keep a guest ETW control plane in the NT subsystem: provider registration keyed by provider GUID and process identity; per-session enable state, filters, event schemas, callback lifetime, buffering, timestamps, and consumer delivery. Explicitly classify guest user-mode provider events separately from host Linux instrumentation. A Linux backend could be useful for internal diagnostics, but it is not itself an ETW compatibility implementation.

**Security boundary:** Provider-supplied event payloads and filter buffers are untrusted. Validate lengths, alignment, pointer lifetimes, session access rights, and event delivery ownership. Do not leak host tracepoint data or other guest processes' events simply because the host tracer can observe them. Preserve cancellation and teardown ordering when a provider unregisters while callbacks or event delivery are in flight.

**OPEN:** Establish exact Windows reference behavior for TraceLogging, manifest-based providers, classic providers, real-time consumers, event loss counters, ETL file formats, security descriptors, and kernel logger sessions. These are separate compatibility milestones, not consequences of implementing EventRegister/EventWrite alone.

## Proposed falsifiable gates (not executed)

1. On a pinned Windows build, create a minimal provider/controller/consumer trio; record compiler version, API calls, provider GUID, session configuration and resulting event sequence. Test disabled provider, enable callback, filter changes, session stop/restart, provider unregister and multiple sessions.
2. Validate event identity, descriptor and payload decoding, timestamps, activity IDs, and ordering across multiple producer threads. Record buffer overflow/lost-event behavior rather than assuming reliable delivery.
3. Test negative rights, malformed payload sizes, invalid filter descriptors, stale registration handles and concurrent unregister/emit. Isolate crashing tests in separate processes.
4. Implement an independent Linux-side guest-state model and compare traces with the Windows reference. Passing a Linux test only proves the model's tested behavior, not full ETW compatibility.
5. Benchmark separately after correctness: event throughput, callback latency, buffer pressure and overhead with tracing disabled versus enabled. No performance result is claimed here.

## Scope and limitations

This is a **source-contract review**, not a verified LinuxNTAlpha runtime feature. The cited Microsoft documentation is not pinned to a particular Windows build; version-specific behavior remains open. The repository's earlier process, thread, and loader research does not establish ETW provider/session compatibility. No source code was copied, and no Windows/Linux experiment was run.
