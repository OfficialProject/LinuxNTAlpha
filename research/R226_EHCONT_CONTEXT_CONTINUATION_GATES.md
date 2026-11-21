# R226 — PE64 EH continuation metadata and NT context-resume validation gates

Research date: 2026-11-21. Status: **documentation audit only**; no Windows reference run, PE fixture, Linux CET test, or implementation.

## Scope and relationship to earlier work

R30 addresses x64 unwind-function metadata; R31/R216/R217 cover load configuration, CFG and dynamic call-target policy; R42 covers thread context/suspension. This note isolates a distinct trust boundary: **where an exception or externally modified thread context may resume execution**. EH continuation metadata is not the same as the unwind function table or the CFG indirect-call target table.

## Primary-source findings

1. **VERIFIED (Microsoft documentation):** MSVC `/guard:ehcont` generates a **sorted list of RVAs** of valid exception-handler continuation destinations; compiler and linker both require the option. Microsoft documents availability from Visual Studio 2019 16.7, for 64-bit processes on 64-bit OS. Source: [MSVC /guard:ehcont](https://learn.microsoft.com/en-us/cpp/build/reference/guard-enable-eh-continuation-metadata?view=msvc-170).
2. **VERIFIED (Microsoft documentation):** Microsoft describes validation for `NtContinue` and `SetThreadContext`; the `NtContinue` explanation first checks the CET shadow stack for the supplied instruction pointer, then checks the containing image's EH continuation table if not found. A **legacy image with no EHCONT data** permits continuation within the image for compatibility, whereas an **EHCONT-enabled image with zero valid entries** permits none via that table. These states must not be collapsed. Same source.
3. **VERIFIED (Microsoft SDK interface):** `IMAGE_LOAD_CONFIG_DIRECTORY64` declares `GuardEHContinuationTable` and `GuardEHContinuationCount`. This establishes versioned load-config fields, not an unconditional guarantee that all images contain them. Source: [IMAGE_LOAD_CONFIG_DIRECTORY64](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_load_config_directory64).
4. **VERIFIED (Microsoft documentation):** The PE load-configuration directory is a versioned data directory; a loader must check directory and structure extents before reading later fields. Source: [PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

## Architecture implications (HYPOTHESIS, not implemented)

A candidate guest image registry would store module generation, validated load-config bounds, whether EHCONT metadata is absent versus present-but-empty, and a sorted RVA set. Guest `NtContinue` / context-modification pathways would consult the applicable guest mitigation policy and image ownership. This cannot be delegated to Linux signal return, `ptrace`, CFG target registration, or generic CET shadow-stack support merely because those mechanisms can change or constrain host control flow.

**OPEN:** Exact supported Windows build/mitigation combinations, interaction with non-image executable memory, dynamically generated code, cross-process `SetThreadContext`, CET-disabled systems, and exact rejection statuses require Windows differential testing. The Microsoft compiler page describes a particular `NtContinue` path; it does not by itself establish every runtime enforcement condition. Do not infer that the Linux kernel enforces Windows EHCONT.

## Proposed acceptance gates (NOT RUN)

- **Parser:** Build PE64 fixtures with no load-config, truncated load-config before EH fields, absent EH metadata, explicitly empty EH table, one/multiple sorted entries, duplicate/unsorted entries, out-of-image pointers, overflowed count and unmapped table. Reject malformed extents without executing guest code; determine Windows tolerance for ordering before deciding strictness.
- **Reference Windows:** On recorded Windows build, hardware CET state, process mitigation state and toolchain version, compare legitimate exception continuation, invalid image-local continuation, and valid/invalid externally set instruction pointers. Capture observed status, exception/termination, and context effects without guessing outcomes.
- **Linux-host prototype:** Separate parse/publish from enforcement. Probe transitions through a synthetic guest continuation validator before any host context mutation; record module unload/reload race behavior and verify that an absent table is distinct from an empty one.
- **Security:** Test that untrusted images cannot supply pointers into another image, that integer overflow cannot bypass table bounds, and that stale module generations cannot authorize a resumed instruction pointer.
- **Performance:** Only after semantics are validated, compare lookup cost on representative table sizes; no performance estimate is made here.

## Decision gate

Do not claim CET, CFG, unwind parsing, or a generic Linux context API provides EHCONT equivalence. Defer enforcement architecture until Windows-reference results and an explicit guest process-mitigation model exist. No code was copied from upstream.
