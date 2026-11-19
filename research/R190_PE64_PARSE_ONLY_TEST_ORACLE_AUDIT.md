# R190 — PE64 parser negative-test coverage audit and preflight acceptance gates

Date: 2026-11-19. Status: **VERIFIED FROM REPOSITORY SOURCE** for observations; **PROPOSED / NOT RUN** for all tests. No compilation, fuzzing, or execution was performed.

## Scope and evidence

Repository source inspected at Git tree `17d4fe618f43eefad52a7affdbecd68571fbb0c0` (tree returned by GitHub for HEAD during this review). Relevant paths:

- [test.py](https://github.com/OfficialProject/LinuxNTAlpha/blob/main/experiments/pe64-image/test.py)
- [pe64_probe.c](https://github.com/OfficialProject/LinuxNTAlpha/blob/main/experiments/pe64-image/pe64_probe.c)
- [Makefile](https://github.com/OfficialProject/LinuxNTAlpha/blob/main/experiments/pe64-image/Makefile)
- [R115 integration gates](R115_MINIMUM_NT_RUNTIME_INTEGRATION_GATES.md)
- [R181 source audit](R181_PE_HARNESS_INTEGRATION_SOURCE_AUDIT.md)

The URLs above track the default branch and are **not pinned code URLs**; the tree SHA records the inspected snapshot. This note makes no claim that the tree SHA is a commit SHA. Before implementation, fetch and pin the exact commit and blobs.

Primary format contract: Microsoft PE/COFF, [PE Format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format), especially optional-header data directories, section mapping, base relocations and import lookup. The Microsoft page is living documentation, not a version-pinned implementation.

## Verified test-harness observations

1. The harness builds five self-authored PE fixtures and uses a subprocess for each invocation of `pe64_probe`. The Python helper chooses `--parse-only` by default; execution requires an explicit fixture execution mode.
2. `test_corruptions_rejected` enumerates individual malformed headers/sections/relocations, but each case mutates a single field from the baseline fixture. The test does not exhaustively cover pairwise interactions among otherwise individually valid fields.
3. `test_unapproved_imports_rejected` checks seven malformed/scope-unsupported import variants. The production resolver is intentionally singleton: one 40-byte descriptor directory and one 16-byte IAT. Therefore these tests **do not** establish safety for a future generalized multi-descriptor resolver.
4. `test_deterministic_mutations_are_parse_only` performs 200 seeded byte-level mutations over five fixtures, asserting only exit status 0 or 1 and absence of the entry-return marker. A zero exit status is allowed for a mutation; this is a **smoke test**, not a parser correctness oracle or coverage-guided fuzz test.
5. The parser's `execute` function maps and relocates the image, patches a supported import, and applies page protections **even in parse-only mode**; it skips the guest entry call. Thus parse-only means **no intentional entry-point execution**, not zero mapping side effects or an isolated sandbox.
6. `run` sets a 10-second subprocess timeout. The Python tests do not currently collect coverage, sanitizer diagnostics, kernel crash metadata, maximum RSS, or a structured rejection reason per mutation.
7. The loader intentionally rejects unsupported directories and requires a nonempty DIR64 relocation directory, 4096-byte pages and constrained section layouts. Those are fixture-scope limitations, not general PE/COFF validity requirements.

These are code-reading observations, **not findings of a runtime vulnerability**.

## Highest-value proposed acceptance improvements (NOT IMPLEMENTED)

### A. Structural mutation oracle

Create deterministic table-driven cases with a named expected result and a named rejection class, not merely `returncode in (0,1)`. Add a positive control for each newly supported format feature. For current strict fixture scope, distinguish `valid PE but unsupported` from `malformed PE`; both may reject, but the cause must be recorded.

Candidate boundary cases: truncated DOS e_lfanew field; optional-header size exactly at required directory boundary; section virtual length versus raw length; section alignment and file alignment combinations; directory range straddling a section; relocation block at final image byte; relocation entries crossing section boundaries; duplicate relocation targets; import-name NUL terminator beyond mapped data; lookup thunk and IAT aliasing; descriptor/thunk termination at the final permitted byte; malformed import hint/name; section raw data overlaps with headers; overflow near 32-bit RVA limits. Each must be generated from a verified fixture layout, not assumed offsets.

### B. Deterministic corpus + fault accounting

For each mutated file record SHA-256, mutation seed, fixture build identity, invocation mode, exit status, stdout/stderr, timeout, signal termination, and sanitizer diagnostics. Unexpected signal termination or timeout must be **FAIL**, not an acceptable rejection. Keep all mutations parse-only and run under resource limits in disposable subprocesses. Separate parser robustness from Windows compatibility conformance.

### C. Multi-import preflight gate before R115 M1

Do not broaden the allowlist and executable resolver simultaneously. First implement a *read-only import-plan validator* producing a bounded list of descriptors, thunk entries and destination RVAs. Validate all entries and resolve allowlisted symbols into a temporary plan before any IAT writes; commit patches only after whole-plan acceptance. Inject a bad second descriptor and assert no partial patch or guest entry execution. Add tests for duplicate imports, shared name records, cycles/forwarders (reject until supported), zero and overflowing counts, and aliasing IAT ranges.

### D. Safety boundaries

A parser test harness is not a security boundary. Keep execution restricted to independently built trusted fixtures; avoid describing fuzzed/mutated PE files as safe to execute. Run any new sanitizer tests in a controlled CI job with explicit time/memory ceilings and retain artifacts for failures. Do not infer compatibility, isolation or performance from a passing parser corpus.

## Proposed verification protocol (NOT RUN)

1. Record default-branch commit SHA, compiler versions, Linux kernel, architecture and fixture SHA-256.
2. Run existing `make -C experiments/pe64-image test` unchanged and record PASS/FAIL/SKIP by test.
3. Add read-only structural corpus and rejection-oracle checks. Run ASan/UBSan builds separately; do not claim they ran unless logs are retained.
4. Add multi-import validation without executing any new guest binary; preserve all existing trusted-fixture execution tests.
5. Only after parser tests pass, attempt R115's integrated lifecycle experiment with its own independent acceptance criteria.

## Decision and nonduplication

**OPEN:** Whether the current parser's bounded mapping/relocation phase has failure modes not exercised by the existing corpus; code inspection alone cannot answer this. **HYPOTHESIS:** A structural oracle and all-or-nothing import plan will reduce integration regression risk; this has not been demonstrated.

R23 establishes parse-only execution separation, R44 studies import parsing, R115 specifies integrated runtime acceptance, and R181 audits thread/dispatch ownership. **This note specifically audits the current Python test oracle, corpus accounting, and preflight mutation coverage** rather than repeating those architecture discussions.
