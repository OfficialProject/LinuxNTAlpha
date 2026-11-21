# R216 — PE64 CFG metadata parsing and loader enforcement boundary

Research date: 2026-11-21. Scope: x86-64 PE32+ images in the proposed Linux-hosted NT-compatible user-space runtime. **Documentation analysis only: no implementation, Windows-reference run, Linux experiment, or benchmark was performed.**

## Why this is a separate gate

R180 treats CFG call-target policy at the process level; R194 treats image bootstrap publication. This note isolates the **binary-format trust boundary** between parsing a PE load-configuration directory and granting indirect-branch validity. A successful PE parser is not evidence of a working CFG implementation.

## Primary-source findings (VERIFIED as documented, not experimentally validated)

1. Microsoft's *PE metadata* specification states that the GuardCFFunctionTable (GFIDS) is a **sorted** array of valid indirect-call target RVAs. The entry width is four bytes plus the extra-byte count encoded in `(GuardFlags & IMAGE_GUARD_CF_FUNCTION_TABLE_SIZE_MASK) >> IMAGE_GUARD_CF_FUNCTION_TABLE_SIZE_SHIFT`. A malformed ordering can prevent image loading. Optional GFIDS metadata defines suppressed and export-suppressed targets; future metadata sizes/flags are not a license to silently reinterpret unknown values. [Microsoft PE metadata](https://learn.microsoft.com/en-us/windows/win32/secbp/pe-metadata).
2. The same specification describes `GuardCFCheckFunctionPointer` and AMD64 `GuardCFDispatchFunctionPointer` as loader-installed function pointers, and states the pointer locations should be read-only for effective protection; the loader temporarily reprotects them to install the implementation. A parse-only or no-op replacement cannot legitimately be advertised as CFG enforcement. [Microsoft PE metadata](https://learn.microsoft.com/en-us/windows/win32/secbp/pe-metadata).
3. Address-taken IAT entries are represented in a separate `GuardAddressTakenIatEntryTable`, with their own count; it uses the GFIDS stride encoding but reserved metadata bytes must be zero. Thus import resolution alone does not establish indirect-call-target policy. [Microsoft PE metadata](https://learn.microsoft.com/en-us/windows/win32/secbp/pe-metadata).
4. Microsoft's PE format specification defines the load-configuration directory and `GuardFlags`, including `IMAGE_GUARD_CF_INSTRUMENTED`, `IMAGE_GUARD_CF_FUNCTION_TABLE_PRESENT`, and export-suppression flags. A runtime must interpret the flags together with versioned directory size and actual mapped ranges rather than assume all fields exist in every image. [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

## Candidate architecture (HYPOTHESIS, not a design decision)

Introduce a bounded **parse/validate/publish** transaction: (a) validate load-config directory size before accessing any field; (b) check every table pointer, count, stride, and multiplication for overflow and mapped-image containment; (c) enforce sorted GFIDS and validate reserved metadata; (d) construct immutable per-image target metadata with explicit export-suppression state; (e) publish that metadata atomically with the image's executable mappings and import state; (f) only then install check/dispatch helpers and make code reachable. Maintain an image generation ID so unloading/reloading cannot revive stale targets.

A process-level guest CFG policy must distinguish **metadata understood**, **policy enforced**, and **policy unsupported**. If instrumentation expects real CFG enforcement but the runtime cannot supply it, reject or explicitly classify the image as unsupported rather than silently substituting permissive helpers. The exact Windows rejection and mitigation behavior remains an OPEN Windows-reference question.

## Conformance and security gates (PROPOSED; none executed)

- Generate small PE64 fixtures with: valid sorted GFIDS; descending or duplicate entries; truncated directory; count-times-stride overflow; target RVA outside image; reserved nonzero IAT metadata; suppressed and export-suppressed targets; and differing load-config structure sizes. Determine exact Windows outcomes on a specified Windows build before choosing rejection behavior.
- On Windows, compile CFG-instrumented indirect-call and address-taken export fixtures; compare direct call, indirect call, `GetProcAddress`, and dynamically changed target validity under documented process mitigations. Capture OS build, toolchain flags and observed exceptions.
- On Linux, test whether guest loader publication prevents a second thread from reaching an image before check/dispatch helpers and target tables are ready; exercise unload/reload, pointer tampering and malformed-image rejection. Keep parser fuzzing independent of execution tests.
- Security acceptance requires a malicious image not to gain arbitrary host code execution by forging a GFIDS RVA, rewriting helper pointers, racing unload, or exploiting count/stride arithmetic. Host page permissions alone are not a substitute for guest indirect-target policy.

## Open questions and boundaries

- OPEN: Windows build-specific handling of duplicate entries, unknown future metadata, and exact load failures; Microsoft prose alone does not establish all error codes.
- OPEN: whether a software CFG enforcement path can cover every compiler-generated indirect branch without instrumenting guest binaries; do not assume it can.
- OPEN: interactions with dynamic call-target APIs, JIT function tables (R188), image-load signature policy (R182), and process mitigation policy (R180).
- OUT OF SCOPE: CET shadow-stack/IBT equivalence, anti-cheat acceptance, and kernel CFG. No performance claim.

## Sources (primary)

- Microsoft, *PE metadata*, https://learn.microsoft.com/en-us/windows/win32/secbp/pe-metadata (consulted 2026-11-21).
- Microsoft, *PE Format*, https://learn.microsoft.com/en-us/windows/win32/debug/pe-format (consulted 2026-11-21).

Cross-references: R180_CFG_CALL_TARGET_POLICY_GATES.md, R182_IMAGE_LOAD_PROVENANCE_SIGNATURE_POLICY_GATES.md, R188_DYNAMIC_X64_FUNCTION_TABLE_UNWIND_GATES.md, R194_PE64_BOOTSTRAP_PUBLICATION_GATES.md.
