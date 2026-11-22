# R233 — NT process isolation versus Linux clone resource-sharing flags

Date: 2026-11-21. **Evidence class: documentation review only. No experiments executed.** This note addresses the *host execution topology* choice, not R12 process bootstrap, R67 handle duplication, R73 exit-status persistence, or R232 supervisor-failure ownership.

## Verified primary-source constraints

1. Microsoft describes a user-mode Windows process as having a **private virtual address space and private handle table**. This is a Windows isolation contract, not a claim that each guest process must have a distinct Linux PID. Source: Microsoft WDK, *User Mode and Kernel Mode*, https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/user-mode-and-kernel-mode (consulted 2026-11-21).
2. Linux `clone(2)` documents that **CLONE_VM shares the entire memory space**, including the effect of mmap/munmap; without CLONE_VM, the child receives a separate copy of the calling process's memory space. **CLONE_FILES shares the descriptor table**, including subsequent closes and opens; without it, the child inherits a distinct descriptor table, but copied descriptors still refer to the same open file descriptions. Source: Linux man-pages, `clone(2)`, https://man7.org/linux/man-pages/man2/clone.2.html (consulted 2026-11-21).
3. Linux `clone(2)` documents that CLONE_THREAD implies CLONE_SIGHAND, which in turn requires CLONE_VM. Linux thread-group exit and wait semantics are therefore not a direct replacement for Windows process-object and thread-object lifetime semantics. Same source.
4. Linux `unshare(2)` documents that unsharing CLONE_VM/CLONE_THREAD/CLONE_SIGHAND from a multithreaded caller fails, limiting attempts to retrofit a separate guest-process address space into an already multithreaded host worker. Source: Linux man-pages, `unshare(2)`, https://man7.org/linux/man-pages/man2/unshare.2.html (consulted 2026-11-21).
5. Microsoft debugger documentation recognizes per-process handle tables and distinguishes process/thread objects from handle-table entries. Source: Microsoft WDK, `!handle`, https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/-handle (consulted 2026-11-21).

## Architectural consequence (reasoned, not measured)

**Candidate A — one Linux process per guest NT process:** independent host memory mappings and host descriptor tables provide a useful *starting boundary*. Guest handle rights, shared sections, NT process IDs, inherited handles, termination/wait objects, and debugger identity still require guest-owned semantics. Do not equate host PID or fd with guest PID or HANDLE.

**Candidate B — multiple guest NT processes inside one Linux CLONE_VM group:** may simplify shared runtime state but does **not** provide independent guest address spaces. It would require a separate proven isolation mechanism for guest pointers and per-process mappings. Merely assigning distinct guest handle-table objects does not restore memory isolation. Treat this as a high-risk hypothesis until isolation is demonstrated.

**Candidate C — separate Linux processes plus supervisor/broker:** combines A's address-space boundary with centralized guest object identity and authorization. It introduces IPC, broker restart and shared-object reference-lifetime costs. Those costs must be benchmarked; no performance claim is justified here.

**Provisional preference, not ADR:** use A/C as the initial experimental baseline because they preserve a native Linux memory-protection boundary without a new memory virtualization layer. This is not a final decision on the broker's placement, process model, or kernel modifications.

## Falsifiable conformance and security gates (proposed; NOT RUN)

- Spawn two guest processes with the same preferred PE image base; verify private writes and mapping changes in one cannot affect the other, except explicitly shared section views.
- Give each process the same numerical guest HANDLE value referring to different objects; verify access checks, duplicate/inherit/close, and reference lifetimes are independent of host fd numbering.
- Verify a duplicated Linux fd's shared open-file-description state does not accidentally substitute for Windows per-handle flags, guest access rights, or file-pointer rules.
- Attempt cross-process guest memory read/write with and without explicit NT-granted rights; enforce checks in the guest authority layer and host sandbox independently.
- Crash a worker and separately crash a supervisor; verify guest object terminal state, pending waits, and cleanup against the R232 failure policy.
- Record clone flags, kernel version, namespaces, seccomp policy, PE fixture SHA, Windows reference build, and PASS/FAIL/SKIP for each case. Use a Windows reference system for semantic comparisons.

## Unresolved

- Whether a dedicated broker can satisfy guest process creation, cross-process section mapping and handles without unacceptable IPC latency.
- Which NT memory operations must execute in a target worker versus through a privileged broker, and how to prevent TOCTOU between validation and mapping.
- How host process namespaces and security controls compose with guest token impersonation (R81/R223).
- Whether a shared-address-space topology offers any measured advantage after accounting for mandatory isolation.

**Scope limit:** This note establishes constraints and testable candidate topologies; it does not implement NT processes, validate a kernel configuration, or demonstrate Windows compatibility.
