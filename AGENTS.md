# LinuxNTAlpha — autonomous research and development instructions

## Project requirements

Build and evaluate a performance-first Linux-based platform that may eventually execute x86-64 Windows programs without depending on Wine or Proton as *runtime dependencies*. Study Wine, ReactOS, Linux and other implementations as evidence; never assume an ABI/NT implementation is unnecessary. Retain Linux drivers and hardware support where possible. Begin with a compatibility subsystem on upstream Linux; do not prematurely build a distribution, a console fork, an anti-cheat bypass, or a Windows kernel driver loader.

Read README.md, research/PLAN.md, research/DECISIONS.md and research/INDEX.md before changing code or choosing an architecture. The kernel-vs-userspace boundary remains undecided. Judge candidates by correctness, reproducibility, security, Linux maintenance burden and *measured* performance, not slogans.

## Continuous task workflow

A running agent should independently:
1. Inspect the actual repository head, prior experiments, decision log and remaining unknowns.
2. Select the most consequential open question or false assumption; verify with official specs and pinned upstream source, including version/tag and blob/commit SHA where possible.
3. Where safe and useful, implement the smallest executable experiment that could **falsify** the hypothesis. Run it, compare expected vs observed outcomes, and account for failures and skips.
4. Commit complete evidence and source in the repository. Record exact environment, commands, source links, outcome, limitations and next tests.
5. Reassess and proceed to the next substantive question in the same session instead of ending after adding one research note, subject to execution/resource limits and any human approval requirements.

The user requested **no routine progress reports**. Keep findings in GitHub; provide a consolidated summary only when the user explicitly says stop or requests one. If access or an irreversible action requires authorization, ask only for that decision. Do not promise indefinite execution: a separate persistent runner is needed to restart Codex tasks after a task environment ends.

## Evidence and verification rules

- **VERIFIED FROM SOURCE**: quote or describe a directly inspected primary source with a resolvable URL and, for code, pin release/tag/blob SHA. Do not treat unpinned documentation as a versioned ABI.
- **REPRODUCED**: a test was actually executed and its environment, command, result and negative controls are recorded. Never represent a compiled-only or skipped test as passing.
- **HYPOTHESIS**: plausible engineering design without conformance and performance validation.
- **OPEN**: important unknown, explicitly tracked.
- When documentation conflicts with actual UAPI headers or source, inspect and correct the record, not merely create another conflicting note. For example, R8/R9 were corrected for misleading documentation-name claims, and R18 shows an actual pre-v6.14 ntsync ioctl ABI change.
- Prefer primary Microsoft docs, upstream Linux headers and code, Wine/ReactOS original repositories, authoritative licenses and measured tests. Avoid secondhand summaries as decisive evidence.
- No unsupported claims of lower latency, broad game compatibility, direct anti-cheat compatibility, Windows kernel driver support or universal security.
- Treat exposed host filesystem, namespace and shared IPC boundaries as security-sensitive. Never run untrusted Windows binaries with elevated host permissions.

## Experiment discipline

- Favor isolated userspace programs first. Never require root, change host kernel configuration or load a kernel module just to demonstrate a claim without appropriate approval and a controlled test host.
- Run **make -C experiments test** for the existing smoke tests; review pass vs skip individually. These do not constitute full NT compatibility.
- Compiler-generated Microsoft x64 ABI calls in Linux ELF (R17) prove only a narrow call bridge. A real PE fixture (R19) executes natively; an imported fixture (R20) calls one allowlisted synthetic function through an IAT. R23 added parse-only mode for hostile test mutations; require --execute-trusted-fixture explicitly for any code execution. Both still lack Windows operating-system services, TLS, exceptions and genuine DLL loading.
- A Linux syscall intercepted with SUD (R16) proves only interception mechanics, not NT syscall execution or isolation; R22's ~1-microsecond signal cost is a microbenchmark result on one container, not an application slowdown estimate.
- An old installed ntsync UAPI header or absent /dev/ntsync (R18) is **SKIP**, not a valid kernel synchronization benchmark.
- When adding tests, exercise boundary conditions and negative controls; add a README and a reproducible invocation.

## Current high-value research questions

1. Build on tested R19/R20 PE64 and one synthetic IAT import: comprehensive parse-only import test corpus, actual independently implemented Windows API semantics, then DLL loading, TLS and startup. Do not run untrusted binaries; a parser/executor is not a sandbox.
2. System call dispatch and x64 ABI transitions: actual Windows-origin raw syscall fixture, selector rearming, thread isolation, asynchronous signals and correct NT service mapping.
3. NT object manager, ACL/token access checks, Windows file-sharing and case-insensitive path semantics without unsafe host escapes.
4. NTSYNC runtime validation on a kernel exposing /dev/ntsync, including wait-any/all atomicity, abandoned mutexes and APC/alertable wait limitations.
5. Error/exception delivery: SEH, PE unwind data, ABI-crossing unwind, stack and guard pages, stack probes.
6. Licensing audit before importing third-party code; distinguish Linux UAPI with Linux-syscall-note from reusable licensed code.
7. Game scope: DirectX graphics/API translation, input, audio, launchers, GPU drivers and anti-cheat vendor cooperation are separate hard problems. No claim of supported games without tests.

Do not modify architecture choices in research/DECISIONS.md unless supported by an explicit, testable ADR rationale and evidence.
