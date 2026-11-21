# R216 — PE64 integration seam inventory and first refactor gate

Research date: 2026-11-21. **Status: repository-source inspection and design hypothesis only.** No build, test, benchmark, Windows reference run, or integration was executed in this review.

## Question and scope

R115 proposes a two-thread, two-import minimum NT runtime integration milestone. This note inventories the *actual current experiment build and execution interfaces* and identifies the smallest non-speculative code refactor required before that milestone. Unlike R115, this is an interface-level inventory, not a proposed Windows conformance specification.

## VERIFIED FROM REPOSITORY SOURCE

The following paths were inspected on the repository default branch on 2026-11-21:

- `experiments/pe64-image/Makefile`: the PE probe is built by linking **pe64_probe.c** and **ntdll_minimal.c** into a single executable. Its `test` target runs `ntdll_minimal_test` and `python3 test.py`. The fixture build uses Clang targeting `x86_64-pc-windows-msvc` and `lld-link` with freestanding, no-default-library PE entry points.
- `experiments/pe64-image/README.md`: the existing loader accepts **either no imports or exactly one descriptor with one named import**, allowlisted as `probe.dll!add_five` or `ntdll.dll!RtlCompareMemory`. It rejects TLS, exception, delay-import and other unsupported directories. Therefore an M1 fixture with two imports cannot currently be accepted unchanged.
- The same README defines four distinct execution modes: `--parse-only`, `--execute-trusted-fixture`, `--execute-synthetic-gs-fixture`, and `--execute-synthetic-sud-fixture`. The latter two are *separate* explicit entry paths; successful individual runs do not prove that their signal/GS setup can coexist.
- `experiments/syscall-user-dispatch/README.md`: the standalone SUD smoke test temporarily installs a SIGSYS handler, switches its selector and restores dispatch state. Exit 77 means SKIP, not PASS.
- `experiments/gs-teb-probe/README.md`: the GS probe saves/restores per-thread GS, checks Linux FS remains unchanged and tests four pthreads. It does not establish guest thread startup or TEB completeness.
- `experiments/abi-bridge/README.md`: the ABI bridge is an ELF-to-ELF compiler-generated ABI test, not a generic PE guest/host call interface.

These are claims about *checked-in interfaces and recorded experiment descriptions*, not independent reproduction of the historical PASS statements in those files.

## Integration dependency and proposed refactor (HYPOTHESIS)

**Gate A — parser separation, no guest execution.** Extract bounded PE metadata inspection into a callable parse-only interface without changing current CLI behavior. Preserve the existing single-import acceptance as a regression baseline. Extend import metadata validation to multiple descriptors and multiple thunks, but do not execute the resulting image. Include overflow, overlapping RVA, unterminated descriptor/thunk/name, duplicate module/name and unknown-symbol negative cases. Reject unsupported imports deterministically. The specific representation and ownership of parsed structures must be decided after inspecting `pe64_probe.c` itself.

**Gate B — explicit guest runtime context.** Define a test-only context that owns image mappings, import bindings, per-thread guest GS state, signal/dispatch registration and teardown status. This is an interface *proposal*, not a claim that existing code already exports these pieces. Teardown must be reverse-ordered and idempotence/reentry behavior must be specified.

**Gate C — compose existing single-thread mechanisms first.** After Gate A passes, run one trusted self-built fixture through an imported function, a guest GS read and a trapped synthetic syscall in one lifecycle. Ensure every guest syscall interception is attributed to the guest and never forwarded to the Linux table by default. Only then introduce a second guest thread.

**Gate D — two threads and two named imports.** This is the R115 M1 goal. Do not bypass Gate C by writing a larger standalone demonstration that cannot reuse existing regression tests.

## Primary-source contracts that constrain implementation

- Microsoft PE/COFF image format, import and relocation directories: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- Microsoft x64 calling convention and preserved register obligations: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- Linux Syscall User Dispatch, explicitly a compatibility mechanism rather than a sandbox: https://docs.kernel.org/admin-guide/syscall-user-dispatch.html
- Linux x86-64 FS/GS context and arch_prctl: https://docs.kernel.org/arch/x86/x86_64/fsgs.html

These live upstream documentation URLs identify normative/reference material; their versions were not pinned or source-code audited in this run.

## OPEN — inspection and test prerequisites

1. Read `pe64_probe.c` and `test.py` line-by-line to identify parser state, mapping ownership, signal handler installation, fixture dispatch and negative-test fixtures. The Makefile/README inspection alone cannot establish safe extraction boundaries.
2. Confirm how the SIGSYS selector and handler interact with callbacks that reenter guest code; design negative tests for nested transitions and interrupted teardown (R90).
3. Decide whether guest-thread creation belongs to the integration harness or a reusable runtime service; validate host FS/GS preservation and guest cleanup for each thread.
4. Preserve the existing `make -C experiments test` suite and classify SKIPs separately. Record exact toolchain/kernel/commit and real observed outputs when tests are eventually run.
5. Compare a Windows-reference fixture for the first actual NT API contract only after the composed synthetic fixture passes; synthetic `add_five` and service 39 are not Windows API conformance evidence.

## Decision

**No kernel/userspace boundary decision and no performance claim.** The immediately falsifiable next engineering task is a parser-only multi-import refactor with unchanged existing fixtures and negative controls. Existing R115 defines the milestone; R216 identifies the checked-in build and CLI seams that currently block it.
