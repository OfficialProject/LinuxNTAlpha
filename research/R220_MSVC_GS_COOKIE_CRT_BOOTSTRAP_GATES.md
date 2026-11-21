# R220 — MSVC /GS security-cookie and CRT bootstrap boundary

Date reviewed: 2026-11-21. Status: **DOCUMENTATION VERIFIED / DESIGN HYPOTHESES / NO EXPERIMENT**.

## Why this is a separate gate

R194 covers PE process bootstrap, R185 stack probing, and R216/R217 CFG metadata and policy. None of those establishes that a Windows-target binary compiled with MSVC `/GS` can safely enter protected functions under a Linux-hosted NT compatibility runtime. Stack probing (`__chkstk`), stack-cookie initialization (`__security_init_cookie`), CFG indirect-target validation and PE relocation are different mechanisms and must not be treated as substitutes.

## Verified Microsoft contracts

1. MSVC `/GS` uses a global security cookie in protected functions. On protected-function entry the cookie is saved on the stack; on exit it is checked against the global value. A mismatch signals a buffer-overrun condition and terminates the program. The cookie is also used in exception-handling-related code. [S1, S2]
2. Normally the C runtime initializes this cookie. When a program bypasses CRT startup (for example with a custom `/ENTRY`), Microsoft explicitly requires calling `__security_init_cookie` before entering any overrun-protected function. Without initialization the known default weakens the protection; initialization after a protected frame has begun can cause a false failure. [S1]
3. `/GS` is a compiler-generated protection, not a guarantee against all memory corruption; it cannot be inferred merely from PE validity or successful execution of a freestanding fixture. [S2]
4. Microsoft documents the `__security_check_cookie` and `__report_gsfailure` paths in its Visual C++ stack-based buffer protection discussion. Their behavior and available symbols are toolchain/runtime-version-sensitive; the NT compatibility layer must not assume that every binary imports these helpers or that a single static implementation covers every compiler version. [S3]

## Architectural implications (hypotheses, not observed behavior)

- **Ownership separation:** the guest CRT (or a deliberately compatible substitute) should own compiler-runtime initialization for an ordinary CRT-linked image; the PE loader should establish relocation, imports, TLS, exception infrastructure and the correct entry contract, but should not blindly overwrite every image's `__security_cookie`. A custom-entry image may need its own explicit initialization. The exact loader-versus-CRT division across supported Windows/toolchain versions requires Windows reference tests.
- **Security boundary:** a freestanding PE that returns a value (R19/R20) does not exercise `/GS` startup, cookie entropy, corrupted-frame termination, or nested exception paths. Passing those fixtures is no evidence of stack-cookie compatibility.
- **Failure handling:** an integrity failure must not be converted into an ordinary recoverable guest exception or swallowed by a Linux signal handler. The exact guest-visible status, exception parameters, debugger behavior and process termination policy remain to be established on reference Windows builds.
- **Entropy:** never use the documented default cookie as a production initialization strategy. The precise seed recipe is an implementation/version question, not established by the public API contract; independently validate a security-equivalent policy rather than asserting bit-for-bit Windows reproduction.

## Proposed falsifiable gates (NOT RUN)

1. Build separately identifiable MSVC `/GS` PE64 fixtures: normal CRT entry, custom `/ENTRY` with early cookie init, and custom `/ENTRY` without init. Record compiler/linker versions, imports, relocations, load-config fields and disassembly. Do not presume the last case must crash; its documented weakness is security, not guaranteed immediate failure.
2. Windows reference matrix: capture cookie initialization ordering relative to TLS callbacks, DLL attach and first protected frame; distinguish documented requirements from version-dependent observed behavior. Avoid using intentionally vulnerable binaries outside isolated test environments.
3. Deliberately corrupt a test-only protected stack cookie under a controlled debugger and record failure status, exception/fast-fail details, debugger visibility and termination. Do not generalize from one Windows version.
4. Native Linux PE fixture: supply the actual compiler-runtime imports and verify correct early initialization, independent per-module behavior where applicable, and failure handling. Record PASS/SKIP/FAIL and make no security claims from a single successful return.
5. Cross-compile with Clang/LLD as a separate toolchain family; verify emitted guard patterns and helper dependencies rather than assuming MSVC-compatible startup semantics.
6. Negative regression: reject attempts to conflate `/GS` with `__chkstk`, CFG, or a Linux host compiler's stack protector. Explicitly check guest/host module boundary and ABI handling.

## Unresolved decisions

- Which Windows/MSVC and Clang versions define the first compatibility target?
- How will CRT/UCRT/VCRuntime dependencies be provided without a Wine/Proton runtime dependency, consistent with licensing and redistributable terms?
- Which fast-fail and debugger semantics are required at the first executable milestone?
- What security guarantees can be asserted only after actual randomized-cookie and fault-injection tests?

## Primary sources (accessed 2026-11-21)

- [S1] Microsoft, `__security_init_cookie`, current MSVC CRT documentation: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/security-init-cookie?view=msvc-170
- [S2] Microsoft, `/GS (Buffer Security Check)`: https://learn.microsoft.com/en-us/cpp/build/reference/gs-buffer-security-check?view=msvc-170
- [S3] Microsoft, *Visual C++ Support for Stack-Based Buffer Protection*: https://learn.microsoft.com/en-us/archive/msdn-magazine/2017/december/c-visual-c-support-for-stack-based-buffer-protection

No LinuxNTAlpha experiment was executed for R220. This note is a documentation synthesis and test plan, not an implementation or benchmark.
