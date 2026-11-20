# R194 — PE64 process bootstrap ordering and publication boundary

Date: 2026-11-19. Status: **architecture synthesis / primary-documentation review only**. No implementation, Windows reference run, Linux experiment, or benchmark performed.

## Distinct question

R19–R28 establish isolated native PE execution, imports, a synthetic GS thread block and one NTDLL export. They do **not** establish the ordering needed to publish a newly created Windows-compatible process to its first user entry point. This note defines a *bootstrap transaction* across existing loader, process, TLS and exception research, rather than another parser or loader-lock specification.

## Source-grounded contracts

1. **VERIFIED (Microsoft PE specification):** The PE optional header identifies the image entry point, image base, data directories and section layout; import, base relocation, TLS, load configuration and exception metadata are distinct structures. Merely transferring control to AddressOfEntryPoint does not implement their semantics. [PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
2. **VERIFIED (Microsoft TLS documentation):** PE TLS directory fields describe initialization data, index storage and callbacks; the loader supplies per-thread TLS initialization and calls callbacks. A GS-based synthetic thread block is not a substitute for static TLS. [PE TLS section](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#the-tls-section).
3. **VERIFIED (Microsoft DllMain documentation):** DLL initialization and thread notifications are serialized by the loader; DLL entry points run under loader-lock constraints. Calling arbitrary application callbacks during this phase can deadlock. [Dynamic-link library entry-point function](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function).
4. **VERIFIED (Microsoft process creation documentation):** CreateProcess creates a process and primary thread, with creation flags including CREATE_SUSPENDED. Child startup visibility and execution are separate lifecycle concepts. [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw); [Process creation flags](https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags).
5. **VERIFIED (Microsoft x64 exception documentation):** Nonleaf x64 functions rely on unwind metadata to support exception dispatch and stack unwinding. Mapping executable code without a valid exception/unwind strategy is not general Windows execution. [x64 exception handling](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64).
6. **VERIFIED (project evidence, limited):** R19, R20, R25, R26 and R28 independently demonstrate narrow PE fixtures. R27, R30, R45, R64, R91 and R185 document separate untested lifecycle requirements. None reports a complete bootstrap transaction.

## Candidate dependency graph (HYPOTHESIS; NOT Windows's asserted internal algorithm)

The following is an implementation **partial order** to validate, not a claim about undocumented Windows loader internals:

- **B0: reserve identity and authority.** Create a guest process object and primary-thread record, provisional handle table and security context. Keep externally visible guest state marked *initializing*. Host PID/TID are not sufficient guest identities.
- **B1: validate before execution.** Parse and bounds-check the image, imports, relocations, TLS, exception directory, load configuration and dependency graph. Enforce resource and mapping limits; reject malformed input without invoking guest code.
- **B2: establish address space and module graph.** Reserve/commit guest image and stack mappings; relocate images; resolve import dependencies and export forwarders with guest namespace/search rules. Distinguish a graph node's *allocated*, *linked*, *initializing*, *initialized* and *failed* states.
- **B3: establish thread-local and process-visible runtime structures.** Initialize per-thread guest TEB/TLS and process parameters/PEB-facing structures before callbacks that can inspect them. Set up guest x64 ABI stack, unwind registration and controlled guest/host syscall transitions before arbitrary guest entry.
- **B4: initialize modules and TLS.** Execute appropriate TLS callbacks and DLL process-attach notifications in a documented, version-profiled order, respecting loader serialization and rollback obligations. Do not infer a universal callback order from one fixture.
- **B5: publish runnable state.** Only after required initialization succeeds, make the primary guest thread eligible to run at its executable entry point; honor requested suspended startup and prevent premature host scheduler execution from becoming guest execution.
- **B6: failure rollback.** On parsing, dependency, mapping, TLS or attach failure, prevent entry-point execution, unwind completed initialization where required, close guest references, and release mappings. Exact detach and error behavior is a **Windows-reference question**, not presumed here.

This model intentionally avoids claiming that every Windows API observes one atomic creation event. Publication must be defined per observable API and tested.

## Highest-risk unresolved edges

**OPEN — callback reentrancy:** TLS/DllMain code can query modules, load dependencies, create threads or invoke runtime services. A simplistic graph lock held across all guest callbacks may deadlock or expose partially initialized objects. See R45/R64/R91.

**OPEN — initial-thread TLS:** Static TLS index/template allocation, guest GS state and per-thread initialization must be coherent before *any* guest callback, not merely before EXE entry. See R24/R25/R27.

**OPEN — unwind and failure:** Exceptions in callbacks or entry transitions require x64 unwind metadata and version-specific failure behavior; an ELF unwinder cannot be assumed to interpret guest PE metadata. See R9/R30/R188.

**OPEN — security:** No untrusted PE may execute before all intended host-syscall containment is active. SUD alone is not a sandbox; see R90/R164/R184.

**OPEN — observability:** Windows reference behavior for suspended startup, child-process visibility, attach failures and callbacks must be collected independently. A conceptual ordering diagram is not empirical evidence.

## Falsifiable acceptance gates (NOT RUN)

1. **Nonexecuting state-machine test:** create a synthetic graph with shared DLL dependencies and injected failure at every stage. Assert no executable entry, no leaked module reference, no double-detach and deterministic rollback of *owned* resources.
2. **Windows reference fixture:** record process-attach/TLS callback and EXE entry events using a small signed-by-project test image, with a second DLL and failure-injection variant. Compare cold launch, explicit LoadLibrary, suspended launch and process creation failures. Record Windows build/architecture; do not generalize one version.
3. **Linux PE fixture:** extend the existing loader with two synthetic DLLs, TLS and import dependencies, logging only guest-visible event order. Require reproducible events under repeated runs and failure injection; avoid claiming compatibility until matched to Windows.
4. **Reentrancy stress:** callback performs a permitted module query and an intentionally dangerous nested load; test timeout/rollback in isolated children, with no loader-global deadlock.
5. **Entry safety:** verify GS/TLS, stack alignment, nonvolatile registers, unwind registration and guest syscall interception before the first callback, including additional threads.
6. **Security/negative tests:** reject malformed directories and overlapping image mappings without guest execution, and verify that no callback can reach host syscalls outside the selected trust model.

Each future result must report PASS/FAIL/SKIP, fixture hashes, compiler/linker, Windows build or Linux kernel, architecture and precise stage traces. No such results are claimed here.

## Decision

**OPEN.** Adopt a *testable bootstrap state machine* as the next integration artifact before attempting a general application launch. It should compose existing validated fixture capabilities while preserving strict failure/rollback boundaries. This is an architecture proposal, not an accepted ADR or proof of Windows equivalence.

**Nonduplication:** R27 covers TLS structures; R45/R91 cover loader-lock callbacks; R64 covers dependency graph lifecycle; R130 covers child-process creation; R194 focuses on their cross-component *first-entry publication and failure ordering*, which none of those isolated notes establishes.
