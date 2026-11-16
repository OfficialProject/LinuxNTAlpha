# R123 — General PAGE_GUARD one-shot faults versus Linux page protections

Research date: 2026-11-16. Status: **documentation-verified contracts; implementation hypothesis only; no experiment**.

## Scope and nonduplication

[R65](R65_STACK_GUARD_GROWTH_GATES.md) covers *stack growth and guard faults*; [R108](R108_SIGNAL_OWNERSHIP_NT_EXCEPTION_INGRESS.md) covers *signal/exception ingress*. This note instead isolates **ordinary committed application pages with PAGE_GUARD**, including one-shot clearing, subsequent access, and the memory-protection state exposed to callers. It does not claim to solve stack growth or exception unwinding.

## Verified Windows contracts

1. Microsoft describes PAGE_GUARD (0x100) as a **modifier** to an underlying page protection, not a standalone access mode. It cannot be combined with PAGE_NOACCESS. A first access raises STATUS_GUARD_PAGE_VIOLATION (0x80000001) and **clears the guard modifier**; subsequent access uses the underlying protection unless the guard is rearmed. [Microsoft: memory protection constants](https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants); [Microsoft: Creating Guard Pages](https://learn.microsoft.com/en-us/windows/win32/memory/creating-guard-pages).
2. Microsoft says a guard fault during a system service generally causes that service to return failure; the guard state is still cleared. Do not model all faults as an ordinary user-mode instruction retry. [Microsoft: Creating Guard Pages](https://learn.microsoft.com/en-us/windows/win32/memory/creating-guard-pages).
3. VirtualProtect changes protection of committed pages, fails if a page in the requested range is not committed, and exposes the previous protection through its output parameter. This requires a guest protection model separate from a Linux VMA's current access bits. [Microsoft: VirtualProtect](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect).
4. PAGE_GUARD is not supported by CreateFileMapping as a mapping creation protection flag. This does **not** by itself establish every behavior of guard protection on mapped views: test those cases separately. [Microsoft: memory protection constants](https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants).

## Verified Linux boundary

Linux mprotect changes access permissions (PROT_NONE/READ/WRITE/EXEC) and does **not** define a Windows-style one-shot guard modifier or NTSTATUS exception dispatch. A PROT_NONE fault is therefore only a potential **trap mechanism**, not an implementation of PAGE_GUARD semantics. [Linux mprotect(2)](https://man7.org/linux/man-pages/man2/mprotect.2.html).

Linux userfaultfd supports missing-page and, subject to feature and memory-type negotiation, write-protect fault modes. Write-protect tracking is **not equivalent to trapping all reads, writes and instruction fetches**; do not select it as a general PAGE_GUARD backend without capability and semantic proof. [Linux userfaultfd(2)](https://man7.org/linux/man-pages/man2/userfaultfd.2.html).

## Candidate design — HYPOTHESIS, not a chosen architecture

Keep per-guest-page metadata: base access protection, guard-armed bit, committed state, mapping identity, generation and transition lock. For a private guest mapping, a Linux PROT_NONE trap might allow the runtime to atomically clear guest guard state, restore base Linux protection and inject the guest STATUS_GUARD_PAGE_VIOLATION into the NT exception path. The design must distinguish a real guest access from host-runtime access and other SIGSEGV causes. This is a proposal only.

**Unresolved correctness and security hazards:** two guest threads racing to touch the same guard page; a thread rearming the guard while another fault is pending; guest VirtualQuery/VirtualProtect observing inconsistent state; page protection and mapping changes racing with fault handling; host runtime accessing guest buffers during a system service; Linux signals delivered on alternate stacks; execution faults; shared/mapped pages; and pages whose Windows granularity does not match host page size. Do not treat a signal handler's arbitrary locking, memory allocation or mprotect calls as safe without a separately reviewed design.

## Falsifiable acceptance matrix — proposed, not run

1. On a Windows reference host, allocate committed read/write pages, arm PAGE_GUARD, record VirtualQuery and VirtualProtect old-protection observations, access once under an exception handler, then access again. Record fault code, exception address, continuation behavior and guard clearing.
2. Repeat with read-only and executable underlying protections, and invalid PAGE_GUARD|PAGE_NOACCESS; record API failures rather than presuming all combinations succeed.
3. Run two concurrent touching threads, repeated rearm loops, and a competing VirtualProtect thread. Measure **observed** exception counts and state; do not assume deterministic winner ordering.
4. Trigger guard access from inside a Windows system-service buffer operation, then retry. Distinguish returned failure from user-mode exception delivery.
5. Compare private allocation with mapped views where supported; identify protection constraints and mapping lifetime behavior empirically.
6. Only then build an isolated Linux prototype and test against the captured Windows results, including guest/host fault provenance and non-guard SIGSEGV regression cases.

## Decision gate

**OPEN:** No general PAGE_GUARD backend is selected. Require Windows-reference evidence, an explicit signal-ownership architecture, per-page synchronization proof, and adversarial concurrent tests before integrating this into the PE loader or NT memory manager. This note reports no code execution, benchmarks, or Windows reference measurements.

Sources reviewed 2026-11-16: the Microsoft Learn and Linux man-pages URLs linked above. Version-sensitive behavior beyond those documented contracts remains unverified.
