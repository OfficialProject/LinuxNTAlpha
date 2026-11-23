# R271 — PE64 raw-syscall fixture: source-level evidence and next-test contract

Reviewed: 2026-11-23. **Evidence class: repository source inspection plus primary API contracts.** No compilation, execution, Windows reference measurement, or benchmark was performed during this review.

## Why this note exists

[R26](R26_PE64_RAW_SYSCALL_SUD.md) reports a successful single-interception fixture. [R234](R234_SUD_REARM_TRANSITION_INVARIANTS.md) defines a broader transition state machine; [R258](R258_SUD_SIGNAL_ALTSTACK_REENTRANCY_GATES.md) covers signal-stack and reentrancy hazards. This note audits the **actual guest source** `experiments/pe64-image/sud_fixture.c` and narrows the immediate acceptance test to prevent a misleading PASS when a second syscall escapes into Linux.

## VERIFIED — inspected guest source (repository main branch, 2026-11-23)

The checked-in fixture contains one `win_entry` function, a volatile `data = 37`, and a volatile pointer to that data. Its inline x86-64 assembly is:

```c
__asm__ volatile("movq %%rcx, %%r10\n\tmovl $39, %%eax\n\tsyscall"
                 : "=a"(result) : : "rcx", "r10", "r11", "memory");
```

The function returns 42 only when `result == 0x12345L` and `*pdata == 37`; otherwise it returns -1. This is a synthetic Windows-target fixture: **39 is deliberately Linux x86-64 getpid, not an identified Windows service**. The `mov rcx,r10` instruction resembles the x64 NT syscall-stub register transfer, but the fixture does not validate an actual Windows NTDLL stub or service table.

The source has **exactly one** `syscall` instruction in this function. Therefore the reported one-trap result cannot establish rearming, callback reentry, repeated service dispatch, cross-thread behavior, or interception after an imported host call. Those are untested, not failed.

The inline-assembly declaration explicitly lists `rcx`, `r10`, `r11` and memory as clobbered. This is a compiler interface observation, **not** proof that an interrupted guest's complete architectural context (including vector state and flags) is restored by the Linux host signal path.

## VERIFIED — relevant upstream contracts

- Linux [Syscall User Dispatch](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html) describes SIGSYS-based compatibility interception, a per-thread selector and allowed instruction-pointer region. It expressly does **not** provide a security sandbox.
- Linux [sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html) describes SA_SIGINFO/ucontext delivery; [signal-safety(7)](https://man7.org/linux/man-pages/man7/signal-safety.7.html) constrains work inside handlers.
- Microsoft [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention) specifies Windows x64 call ABI requirements; Linux host function entry and Windows guest function entry are not interchangeable merely because both are x86-64.
- Microsoft [PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) specifies image structures and relocation records, but says nothing that would make Linux syscall number 39 an NT service.

These links identify primary contracts. Their live page revisions were **not freshly fetched** in this run; do not treat this review as a new upstream-version audit.

## HYPOTHESIS — the shortest decisive extension

A second guest `syscall` immediately after the first synthetic return may execute directly as Linux getpid if the host signal handler left a selector in ALLOW. This is a **risk inferred from R26's described handler sequence**, not an observed escape. The test must make that escape observable independently of the PE's arithmetic return value.

## Proposed next experiment — not run

1. Add a **separate** fixture, preserving the existing single-trap fixture as a regression baseline. Emit three distinct raw `syscall` instructions, all using a Linux-colliding number, with explicit compiler clobbers and independently checked synthetic return values. Disassemble the built PE to confirm three syscall sites and record their instruction addresses.
2. Instrument host trap count and trap-site order using preallocated, thread-local state; assert **exactly three ordered SIGSYS captures**, not just a final return of 42. The host must also record that no guest-origin colliding instruction completed as a Linux getpid. Do not use an accidental PID-sized value as proof of interception.
3. Put a known Linux host syscall between two guest calls through a trusted imported bridge; verify it completes while all guest-origin calls remain trapped. Test both selector-driven and permitted-IP-region approaches **only where the running kernel supports the exact mode**.
4. Run the fixture in a child process with a watchdog and a negative control that intentionally leaves the selector ALLOW; verify the test detects the escape. The negative control must never be presented as a successful NT compatibility test.
5. Run concurrency, asynchronous signals, nested callbacks and alternate-stack cases as **separate** gates after the deterministic single-thread test. Record kernel version, headers, compiler/linker versions, exact disassembly, trap metadata, and PASS/FAIL/SKIP reasons.
6. A PASS requires interception **and** host-syscall liveness, with no unexpected Linux syscall entry. An unsupported SUD mode is SKIP. A second or third guest syscall entering Linux is FAIL even if the PE happens to return the expected value.

## Architectural consequence

**OPEN.** R26 proves only one synthetic trap. Before expanding the NT service table, require continuous provenance-preserving dispatch under repeated guest/host transitions. Neither SUD nor an NT-looking register sequence supplies guest authorization or containment. No change to ADR-003 is justified.

## Repository provenance

- Inspected directly: [experiments/pe64-image/sud_fixture.c](../experiments/pe64-image/sud_fixture.c), fetched from the repository's default branch on 2026-11-23.
- Cross-checked against existing research: [R26](R26_PE64_RAW_SYSCALL_SUD.md), [R234](R234_SUD_REARM_TRANSITION_INVARIANTS.md), [R258](R258_SUD_SIGNAL_ALTSTACK_REENTRANCY_GATES.md).
