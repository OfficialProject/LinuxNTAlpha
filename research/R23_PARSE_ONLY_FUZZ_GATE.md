# R23 — Separating PE validation from executable code, plus deterministic mutation smoke test

Date: 2026-11-12. Status: **REPRODUCED** on a Linux x86-64 research container. This is a limited parser smoke test, **not a security audit or sandbox**.

## Motivation

Earlier R19/R20 executable probes interpreted PE headers and then entered the image. Their negative tests intentionally rejected malformed inputs before execution, but they had no safe way to inspect an arbitrary newly generated parser test case without risking execution of an image that happened to survive the checks.

A parser fuzzing effort must not execute mutationally modified instruction bytes. A parse-only option is a necessary separation of concerns, even though the parser and memory mapper still process untrusted data within the address space and may contain vulnerabilities.

## Implementation

At [experiments/pe64-image](../experiments/pe64-image/), the launcher now requires an explicit mode:

- **--parse-only file.exe**: validates headers, section layout, relocation blocks and (where present) the single allowlisted import, copies/relocates/maps sections, applies final protections and exits **without invoking the PE entry**. Success prints PARSE_OK; invalid inputs print REJECT.
- **--execute-trusted-fixture file.exe**: explicitly invokes the PE entry after all prior checks. The only intended executable inputs are the two self-generated, trusted fixtures; this remains **unsafe for downloaded/third-party binaries**.

The Python tests preserve five real execution attempts for the no-import image and five for the synthetic-import image. All 19 header/relocation and 7 import corruption cases now run in parse-only mode and require REJECT. Two additional acceptance cases verify parse-only succeeds without a pe64_entry_return message.

A new deterministic mutation sweep uses Python random.Random(20261112) to generate **200 variants** across both fixture images, each with 1–5 random single-bit corruptions, with periodic random truncations. Every case is run with --parse-only in a child process. The test requires exit status 0 (valid under this restricted parser) or 1 (explicit REJECT), disallows signal-terminated crashes and checks that no PE entry result is ever printed.

The suite now contains **six unittest methods** and ran to completion with GCC -O0, -O2 and -O3 and Clang -O1 ASan+UBSan on the Linux kernel 6.18.57 Debian 13 research environment. No crashes were observed in those 200 seeded variants.

## Limits

- This is 200 random mutations of two tiny fixtures, **not coverage-guided fuzzing, not comprehensive validation, not proof of a safe parser**.
- Parse-only still reads an untrusted PE into the same host process and calls native memory mapping/protection functions; a logic bug can still crash it or worse. It is not a privilege boundary.
- There is no process sandbox, seccomp policy, fuzz input minimization, libFuzzer/AFL++ coverage, instruction decoding, import-map generalization, TLS handling, or production exception handling.
- In execution mode the loader calls arbitrary PE machine code. Its exact synthetic import allowlist does not secure that execution path.
- "PARSE_OK" means the image fits this **deliberately narrow** format and survived checks; it does not mean Windows would accept the image or that the code is safe to run.

## Further acceptance gates

1. Refactor mapping/validation into a pure parser that never mprotects or invokes executable pages, and feed AFL++/libFuzzer using in-memory byte arrays to increase feedback and reduce process overhead.
2. Add structured cross-field mutation cases: overlapping RVA ranges, malformed relocation blocks and duplicate targets, IAT aliasing relocation data, high-pointer overflow, section/header table bounds, zero/raw-padding ambiguities.
3. Compare parse outcomes to a reference tool on Windows, with minimized false positives and false negatives, while keeping different loader policies explicit.
4. Establish Linux sandboxing for any later attempt to handle third-party PE files; do not conflate sandboxing with NT compatibility.
5. Maintain a strict parse-only default and explicit execution consent as features expand.

No performance claim, broad compatibility claim or security guarantee.
