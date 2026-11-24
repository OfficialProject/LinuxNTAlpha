# R275 — SUD configuration inspection, restoration and task lifecycle

Date: 2026-11-24. Status: source audit only; no experiment executed. Extends R234, R272–R274 with the control-plane interface, not another syscall interception design.

## Verified upstream behavior (Linux v6.12)

1. The [SUD documentation](https://github.com/torvalds/linux/blob/v6.12/Documentation/admin-guide/syscall-user-dispatch.rst) explicitly describes per-thread configuration, ptrace inspection/modification for checkpoint/restart, and reset on fork or exec. It explicitly disclaims sandboxing.
2. [UAPI ptrace.h](https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/ptrace.h#L115-L143) defines GET and SET configuration requests and a four-u64 structure: mode, selector, offset, len. The selector is a tracee virtual address, not the selector byte or a transferable handle.
3. [SUD implementation](https://github.com/torvalds/linux/blob/v6.12/kernel/entry/syscall_user_dispatch.c) implements GET by reading the task syscall-work flag and configuration fields, then copying the structure to the tracer. SET requires exact structure size, copies from the tracer and calls the shared task configuration setter. The setter checks mode, interval arithmetic and selector address range; it does not establish that the selector mapping will remain accessible. At dispatch, a failed selector read causes fatal SIGSEGV and an invalid selector value causes fatal SIGSYS. The setter also clears on_dispatch. GET does not serialize that transient flag, guest register state or a pending SIGSYS frame.
4. [ptrace.c](https://github.com/torvalds/linux/blob/v6.12/kernel/ptrace.c#L1243-L1290) routes these requests through the ordinary ptrace request path; the native syscall checks attachment and stopped-state readiness before architecture dispatch. This is not an unrestricted remote configuration API.

Source identity: tag v6.12; inspected blobs respectively e3cfffef5a63364624136b6bc36a0e3856e244b9, 72c038fc71d093514e010fdc4a833cb95285053e, 5340c5aa89e7dd33f30a3543e4f73bfc08ea5a4e, d5f89f9ef29f65f137ef8a6978e6dbe75564056b. Later kernel modes must be separately qualified (R236).

## Architectural inference, not verified runtime behavior

A supervisor can potentially inspect a stopped worker's dispatch configuration as a diagnostic invariant, but configuration equality is not proof that the selector byte, mappings, register context or guest service transaction are correct. Do not restore a raw selector address into a different address space without establishing its mapping and ownership. Do not restore while a guest service or signal continuation is in flight merely because GET succeeds. Treat worker creation and exec as initialization boundaries; require explicit dispatch setup before publishing guest readiness. A fail-stop policy is preferable to speculative restoration until R232's recovery protocol is demonstrated. Keep ptrace authority outside guest debugger APIs; guest debug rights must not imply host ptrace access.

## Proposed falsification tests (all unrun)

- Stopped isolated child: compare self-installed configuration with tracer GET; verify exact size rejection and OFF/ON round trip. Record kernel version, headers, errno and child status; unsupported requests are SKIP, not PASS.
- Two workers with distinct selectors: inspect each stopped task independently, change one configuration, verify the other is unchanged and that each subsequently traps its own synthetic service.
- Fork/clone variants and exec: separately inspect resulting task configuration and test a harmless syscall before and after explicit reinitialization. Do not infer every clone variant from documentation wording.
- Selector lifetime: use isolated disposable children with unmapped selector and invalid byte, record terminal signals and distinguish configuration acceptance from dispatch-time validity.
- Quiescence: compare GET during a normal stopped guest boundary versus a pending SIGSYS delivery stop; demonstrate why identical configuration cannot establish continuation equivalence. Do not attempt production recovery until signal frame, registers, selector state and outstanding broker requests have independent ownership checks.
- Authorization: verify unrelated unprivileged workers cannot inspect or modify each other's configuration under the intended host policy; test actual deployment restrictions rather than assuming ptrace defaults.

## Decision impact

This is a potential diagnostic/checkpoint primitive, not a reason to select SUD or promise transparent restart. It adds a task-lifecycle acceptance gate to R235. No kernel execution, performance measurement, Windows conformance result or implementation change is claimed.
