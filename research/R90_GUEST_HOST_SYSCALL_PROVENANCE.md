# R90 — Guest/host syscall provenance and the SUD security boundary

Status: **architecture synthesis / documentation-only**, 2026-11-15 UTC. No code, Windows reference run, Linux experiment, or benchmark was performed for this note.

## Question and scope

A PE32+ guest and an ELF-hosted runtime can execute in the same Linux address space. How can the runtime distinguish a Windows-origin raw `syscall` from its own Linux syscalls, and what *security* guarantees, if any, follow? This is a cross-cutting decision gate rather than a second demonstration of R26.

R16/R26 establish narrow Linux Syscall User Dispatch (SUD) interception feasibility. R69 separately shows why checking only the NT-facing memory API does not enforce a security mitigation against direct Linux syscalls. This note specifies the provenance and trust requirements that must be decided before generalizing the prototype.

## Source-grounded facts

1. **VERIFIED (Linux upstream documentation):** SUD is designed for compatibility layers needing to redirect selected syscall instructions; the kernel documentation explicitly states it is **not a security mechanism** and is not intended as a sandbox. It uses per-thread configuration, a selector and optionally an allowed instruction-pointer region. [Linux kernel: Syscall User Dispatch](https://docs.kernel.org/admin-guide/syscall-user-dispatch.html).
2. **VERIFIED (Linux upstream documentation):** Linux seccomp filters constrain syscalls at kernel entry and are designed for reducing exposed kernel attack surface, but seccomp filtering is not by itself a complete sandbox. The available actions, filter evaluation, and restrictions differ from SUD's compatibility dispatch. [Linux kernel: seccomp filter](https://docs.kernel.org/userspace-api/seccomp_filter.html).
3. **VERIFIED (project experiment, narrow scope):** R26's PE fixture executes one synthetic Windows-style raw syscall and observes one SIGSYS interception. Its handler temporarily permits Linux syscalls for signal return; it neither implements a real NT service table nor proves containment. [R26](R26_PE64_RAW_SYSCALL_SUD.md).
4. **VERIFIED (project analysis):** R69 identifies direct `mmap`/`mprotect` entry as a bypass candidate for an NT-only dynamic-code policy. No adversarial bypass experiment was run. [R69](R69_DYNAMIC_CODE_MITIGATION_EXEC_MEMORY.md).
5. **VERIFIED (project experiment):** R22's measured signal-interception cost belongs to a narrow, single-container microbenchmark; it cannot establish production NT-call overhead. [R22](R22_SYSCALL_DISPATCH_LATENCY.md).

## Architecture invariants to test (HYPOTHESES, not implemented)

**P1 — Provenance is a transition property.** A raw syscall's number alone cannot classify it as NT or Linux: identical register values and instruction encodings may occur on both sides. The candidate runtime must define guest→host and host→guest transitions, including callbacks, signal handlers, exceptions, nested entries and threads created after initialization. An instruction-pointer allowlist or a selector is an implementation candidate, **not** proof of provenance under adversarial control.

**P2 — Compatibility and containment are separate acceptance tracks.** A compatibility-only runtime may reject or translate unsupported guest services while documenting that arbitrary PE machine code can still exercise Linux capabilities. A security claim requires independent enforcement of allowed host syscalls and memory operations. SUD alone cannot supply that enforcement.

**P3 — Do not silently fall back to Linux.** If the guest issues an unrecognized NT service, the implementation should return a documented guest failure or terminate the test fixture. Passing its service number into the Linux syscall table risks an unrelated host operation.

**P4 — Signal-handler reentrancy is a correctness gate.** Any design which temporarily enables host syscall entry must prove that guest code cannot run during that window, including nested signal/callback paths. This is not established by the one-trap R26 fixture.

**P5 — A seccomp policy must be specified independently.** If containment is required, document which syscalls the trusted host must retain, which may be issued by guest-origin code, and whether the proposed policy can actually distinguish them. Do not assume that adding seccomp to SUD automatically creates a secure distinction.

## Falsifiable experiment sequence (NOT RUN)

1. **Mixed transition fixture:** PE guest issues two distinct synthetic service numbers, calls a host MS-ABI import between them, and repeats. Trace each entry's origin, saved registers, selector state and returned guest status. Require that host Linux calls succeed and guest raw calls never enter Linux under colliding numbers.
2. **Concurrent fixture:** launch multiple guest threads; interleave guest raw calls, host imports, signal delivery and thread exit. Assert per-thread dispatch initialization and cleanup, no cross-thread selector corruption, and no untrapped guest call.
3. **Nested entry fixture:** from a host callback re-enter guest code which issues a raw syscall; verify that provenance is correctly restored at every depth. Run with isolated subprocess timeouts and record all unexpected SIGSYS and Linux syscall outcomes.
4. **Adversarial fixture:** have guest code attempt to alter selector state or jump to any allowed host syscall instruction; test direct memory-protection changes. A successful bypass invalidates **containment** claims even if ordinary API compatibility still works.
5. **Separate policy test:** prototype a minimal seccomp filter in a disposable child process, record Linux kernel version and available filter features, and test allowed/denied host calls. Do not treat successful seccomp installation as proof that NT provenance is enforceable.
6. **Cost model:** only after correctness, measure per-transition latency, number of crossings per representative workload, tail latency and signal/CPU cost. Compare a direct imported NT stub path, SUD trap path and any brokered path under identical conditions.

Record source revision, Linux kernel configuration, compiler versions, CPU, process/thread count, signal masks, instruction pointers, raw syscall numbers, filter configuration and each PASS/FAIL/SKIP. Do not run adversarial fixtures in the main development process.

## Decision gate

**OPEN:** Is LinuxNTAlpha's first deliverable an explicitly *non-sandboxed* compatibility runtime, or must arbitrary Windows-origin code be contained? This choice controls the permissible syscall dispatch architecture. A SUD-based compatibility prototype is not evidence of a secure isolation boundary; a sandbox claim remains blocked until the adversarial tests and an independently enforced host policy pass.

**Nonduplication:** R26 documents the one-trap PE experiment; R69 documents dynamic-code mitigation; this note defines the shared guest/host provenance, nested-transition and security acceptance criteria without repeating their experiments or results.
