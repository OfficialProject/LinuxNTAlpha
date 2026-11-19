# R181 — PE harness integration audit: SUD selector and host/guest transition ownership

Date: 2026-11-19. Status: **VERIFIED FROM PINNED REPOSITORY SOURCE** for the code observations below; **HYPOTHESIS** for proposed refactoring and tests. **No experiment was run.**

## Question

What concrete integration blockers in the *current* PE32+ experiment must be addressed before R115's two-thread, multi-import runtime milestone can be attempted without overstating its safety?

## Pinned evidence

Inspected repository HEAD `8e3f1c0b3a0d731d7c7e9bf59eba4d907e7f7d9a`:

- [pe64_probe.c](https://github.com/OfficialProject/LinuxNTAlpha/blob/8e3f1c0b3a0d731d7c7e9bf59eba4d907e7f7d9a/experiments/pe64-image/pe64_probe.c), particularly `patch_one_import`, `call_entry`, `on_sud_sigsys`, and `call_entry_sud`.
- [experiment README](https://github.com/OfficialProject/LinuxNTAlpha/blob/8e3f1c0b3a0d731d7c7e9bf59eba4d907e7f7d9a/experiments/pe64-image/README.md).
- [R115 integration acceptance gates](https://github.com/OfficialProject/LinuxNTAlpha/blob/8e3f1c0b3a0d731d7c7e9bf59eba4d907e7f7d9a/research/R115_MINIMUM_NT_RUNTIME_INTEGRATION_GATES.md).

### Verified observations, not runtime test results

1. **Import resolver is intentionally singleton.** `patch_one_import` requires a 40-byte import directory (one descriptor plus null descriptor), 16-byte IAT (one slot plus terminator), and chooses between exactly two allowlisted single-symbol imports. It cannot implement R115's simultaneous two-import milestone unchanged. A generalized resolver must validate descriptor termination, thunk termination, RVA ownership, and all writes before exposing executable entry.
2. **SUD selector and counters have process-global static storage.** `sud_selector`, `sud_traps`, and `sud_unexpected` are static globals; `call_entry_sud` passes the address of that selector to `PR_SET_SYSCALL_USER_DISPATCH`. This single-thread fixture cannot establish independent per-thread selector state or safe concurrent calls. It is not evidence of an observed race.
3. **SIGSYS handler does not validate a guest instruction-pointer range.** `on_sud_sigsys` checks signal provenance, `si_syscall == SYS_getpid`, then changes `REG_RAX` and sets the selector to ALLOW. The current code deliberately recognizes one synthetic service and does not implement a guest NT syscall namespace. A guest-vs-host ownership decision cannot be inferred from a service number alone.
4. **Rearming is not demonstrated.** The SIGSYS handler switches the selector to ALLOW to permit signal return. The fixture expects one trap; it does not demonstrate reblocking before a subsequent guest syscall, nor guest→host→guest nested transitions.
5. **GS state is scoped to a synchronous call.** `call_entry` stores old GS, installs a stack-local synthetic TIB, calls the guest, then restores GS. This does not supply a persistent guest thread lifecycle, nested callback state, or exception-safe restoration.
6. **The signal disposition is process-wide.** `call_entry_sud` installs and restores a SIGSYS action around one invocation. An integrated multi-thread runtime needs coordinated signal ownership; simply invoking the current helper concurrently would not preserve per-thread teardown semantics.

These are source-code facts about this pinned revision, not claims that Linux itself lacks the necessary facilities.

## Primary Linux contract to verify in implementation

- [Linux Syscall User Dispatch documentation](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html) describes compatibility-oriented syscall dispatch and explicitly says it is not a security mechanism. It describes selector-based dispatch and SIGSYS delivery; the project's service dispatcher must not mistake this for an NT security boundary.
- [prctl(2) PR_SET_SYSCALL_USER_DISPATCH](https://man7.org/linux/man-pages/man2/PR_SET_SYSCALL_USER_DISPATCH.2const.html) documents enabling/disabling the interface and selector pointer semantics.
- [sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html) defines process signal dispositions and signal masks; [signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html) distinguishes process-directed and thread-directed signals.

These external links identify the normative review surface; no pinned Linux kernel tree or live runtime was inspected in this audit. Version-specific Linux behavior remains to be checked against the selected test kernel.

## HYPOTHESIS — minimum refactor before R115 M1

Separate (a) immutable process-level guest image/import metadata, (b) thread-local dispatch selector and guest/host transition stack, and (c) process-level signal-handler installation and teardown. A guest-entry guard should own the transition, restore prior GS/dispatch state on *every* supported exit path, and reject nested transitions that the implementation cannot yet preserve. Guest syscall dispatch should check provenance and a strictly enumerated service table; unsupported services must fail closed **within the compatibility subsystem**, not be forwarded to Linux.

Do **not** attempt to fix the existing singleton fixture by merely making the counters atomic: that does not establish thread-local selector lifetime, handler ownership, or nested transition correctness. Likewise, a selector set to ALLOW for host code must not be treated as guest isolation.

## Falsifiable follow-up tests — NOT RUN

1. Preserve all existing single-fixture regression tests before refactoring; capture the exact compiler, kernel, and CI results.
2. Extend **parse-only** import tests to two descriptors, duplicate descriptors, unterminated thunk arrays, aliasing IATs, invalid name RVAs, overflow, and partial-resolution rollback. Never execute mutated images.
3. With two explicitly trusted PE fixtures, alternate guest and host calls and issue **two** guest syscalls per thread. Record every trap, guest instruction pointer, selector state before/after, and exact return value; require that no guest syscall silently reaches Linux.
4. Run two guest threads with distinct selectors and GS data. Deliberately interleave guest entry, host callback, guest reentry and thread exit; check cross-thread contamination.
5. Inject a controlled guest fault and a host-side fault separately; prove that signal dispatch does not misclassify host failures as guest NT exceptions. Run in disposable child processes with timeouts.
6. Inject failures at handler install, selector setup, guest entry and teardown; verify cleanup and host signal disposition after each failure. Treat missing kernel capability as SKIP.

## Decision

**OPEN:** Whether SUD remains the preferred dispatch path for integrated execution. The pinned code demonstrates a narrow synthetic fixture, not safe nested/multithread guest dispatch. No performance or security conclusion follows from this audit.

**Nonduplication:** R80/R89/R90/R164 analyze SUD architecture and signal boundaries; R115 defines the milestone. This note records *specific current-source integration blockers* and an ordered refactor/test contract tied to a reproducible repository commit.
