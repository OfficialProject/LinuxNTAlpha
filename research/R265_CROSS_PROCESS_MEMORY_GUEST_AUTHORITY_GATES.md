# R265 — Cross-process memory operations: NT process-handle authority versus Linux process_vm_* access

Research date: 2026-11-23. **Status: documentation analysis only. No Windows/Linux differential test, benchmark, or implementation was run.**

## Scope and relationship to earlier work
R67/R246 cover cross-process *handle duplication*, R233 covers process topology, R247 covers *virtual-memory query* provenance, and R69/R241 cover executable-memory mitigation. This note addresses a different boundary: **reading and writing another guest process's address space**, with a guest process handle and its granted rights, when the Linux backend may use `process_vm_readv`/`process_vm_writev`.

## VERIFIED — documented contracts

1. Microsoft's process access-rights table distinguishes `PROCESS_VM_READ` (0x0010), `PROCESS_VM_WRITE` (0x0020), and `PROCESS_VM_OPERATION` (0x0008). `OpenProcess` checks requested access against the process security descriptor; a process identifier or host PID alone is not the authority represented by a granted guest handle. [M1]
2. `ReadProcessMemory` requires a handle granting `PROCESS_VM_READ`; Microsoft documents failure when the requested range crosses inaccessible memory. It returns a BOOL and exposes transferred-byte count and Win32 last error separately. [M2]
3. `WriteProcessMemory` documents both `PROCESS_VM_WRITE` and `PROCESS_VM_OPERATION` requirements, and failure when the range includes inaccessible memory. [M3] **Do not weaken this documented contract based on anecdotes about particular Windows builds.** A November 2026 Microsoft Q&A discussion reports conflicting observations about a write-only handle; these are not an authoritative change to the API specification. [M5]
4. Linux `process_vm_readv`/`process_vm_writev` operate on a numeric target PID and arrays of local/remote iovecs, returning a *byte count* on success, including potentially short transfers. Remote access is subject to `PTRACE_MODE_ATTACH_REALCREDS` checks; partial transfers and errno do not directly implement Windows BOOL/last-error behavior. [L1]
5. Linux ptrace access checks incorporate credentials, dumpability, capabilities and LSM policy; they are not checks of a Windows guest process handle's granted access mask. [L2]

## HYPOTHESIS — proposed architecture (not selected)

A guest process-object registry should resolve a **caller-owned guest handle** to a stable target guest-process identity and granted-rights mask *before* any host operation. A privileged host broker could then authorize an operation and perform `process_vm_*` on the correct host worker, but only if the broker also enforces target generation, guest allocation/protection policy, host isolation and race handling. The Linux syscall is a transport mechanism, **not** the Windows authorization model. This is particularly important if multiple guest processes share one host PID/address space (R233): `process_vm_*` cannot by itself distinguish them.

The documented all-accessible-range Win32 contract suggests validating the guest range before transfer. **OPEN:** concurrent target unmap/protect changes mean a prior check cannot guarantee an atomic copy; do not claim that a simple preflight check makes the operation atomic. For failure paths, retain actual copied-byte observations internally and determine Windows-visible output count and error from Windows-reference measurements rather than inventing a mapping.

## Proposed unrun conformance/security gates

| Gate | Windows-reference oracle | Linux/backend adversarial check |
| --- | --- | --- |
| Rights | Open target with read-only, write-only, operation-only, and combined rights; attempt read/write, capture BOOL, bytes, last error | Reject unauthorized guest handles even when host credentials would permit access |
| Boundary | Read/write zero bytes, valid page, unmapped page, and range straddling valid/invalid pages | Detect short host copies; never misreport short copy as full success |
| Identity | Duplicate/revoke/close guest handles; exit and replace target; verify stale-handle behavior | PID reuse and target-worker restart cannot redirect to a different guest process |
| Isolation | Attempt access between unrelated guest security principals and to host supervisor mappings | No raw host PID, raw pointer, or broker authority exposed to untrusted PE code |
| Concurrency | Race protect/unmap/exit against copy, repeat under Windows builds | No use-after-free, guest-map ledger divergence, or unauthorized partial writes |
| Executable pages | Write to image/read-only/executable pages with explicitly varied access masks | Do not silently bypass guest protection or mitigation policy |

For each Windows result record OS build, architecture, process integrity/token, granted access mask, page state/protection, byte count, return value, and GetLastError immediately after failure. For each Linux result record kernel release, LSM/Yama settings, namespace and PID topology, privileges, iovec layout and return/errno. None of these measurements exists in this note.

## Architecture decision impact

**OPEN:** Is the guest's cross-process address-space authority implemented as a brokered operation, an isolated worker operation, or a kernel extension? Before ADR-003 selection, require rights isolation, stable process identity, partial-copy conformance and a performance comparison that includes broker overhead. Do not treat a successful host `process_vm_writev` as evidence of Windows compatibility.

## Primary sources (reviewed 2026-11-23)

- [M1] Microsoft, *Process Security and Access Rights*: https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights
- [M2] Microsoft, *ReadProcessMemory*: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory
- [M3] Microsoft, *WriteProcessMemory*: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-writeprocessmemory
- [L1] Linux man-pages, *process_vm_readv(2)*: https://man7.org/linux/man-pages/man2/process_vm_readv.2.html
- [L2] Linux man-pages, *ptrace(2), access mode checking*: https://man7.org/linux/man-pages/man2/ptrace.2.html
- [M5] **Non-normative, conflicting community observations**, Microsoft Q&A (November 2026): https://learn.microsoft.com/en-us/answers/questions/6057504/permissions-needed-for-writeprocessmemory-appear-t
