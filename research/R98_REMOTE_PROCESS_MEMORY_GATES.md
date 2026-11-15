# R98 — Cross-process guest memory operations versus Linux process_vm_* semantics

**Status (2026-11-15): VERIFIED FROM PUBLISHED PRIMARY CONTRACTS; architecture hypotheses and proposed tests only. No experiment executed.**

## Scope and nonduplication

R53/R59/R71/R72 cover mappings, image COW, write-watch and address constraints; R67/R68 cover handles and identity; R92 covers NT discretionary access. This note isolates **remote memory copy**, where a valid Windows process handle does not imply that a Linux PID or ptrace permission is sufficient, and where Linux partial-transfer behavior is not a drop-in Win32 result.

## Verified primary contracts

1. Microsoft [ReadProcessMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory) requires `PROCESS_VM_READ` on the process handle and documents that the *entire* requested range must be readable; the operation fails when it crosses inaccessible memory. A nonzero return indicates success; zero indicates failure, with extended error from `GetLastError`. The optional transferred-byte count is not a guarantee of atomic copying.
2. Microsoft [WriteProcessMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-writeprocessmemory) documents `PROCESS_VM_WRITE` and `PROCESS_VM_OPERATION` handle rights and requires the *entire* requested range to be writable, otherwise failure. Do not infer that the documentation guarantees transactional rollback of bytes already copied during a concurrent mapping/protection change.
3. Microsoft [process object access rights](https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights) distinguishes `PROCESS_VM_READ`, `PROCESS_VM_WRITE` and `PROCESS_VM_OPERATION`; `OpenProcess` performs guest security-descriptor access checks. These are not equivalent to host UID or PID permission.
4. Linux [process_vm_readv(2)/process_vm_writev(2)](https://man7.org/linux/man-pages/man2/process_vm_readv.2.html) may return a positive **partial byte count**; remote iovec elements are not validated in advance and a later inaccessible page can stop the transfer. The documented permission check is `PTRACE_MODE_ATTACH_REALCREDS`. The calls are **not atomic**.
5. Linux [ptrace(2) access-mode checks](https://man7.org/linux/man-pages/man2/ptrace.2.html) incorporate host credentials, dumpability and Linux security modules. They are a host-enforced second gate, not an implementation of the guest process object's DACL.

## Architectural consequences (deduced from those contracts)

- A Windows-compatible service must resolve the **guest process handle and granted access mask first**, then choose a permitted host transport. Directly accepting a Linux PID bypasses NT handle lifetime, guest process identity and rights.
- A single `process_vm_readv/writev` call cannot be exposed unchanged as the Win32 API: a short positive transfer is a legitimate Linux result but not a successful full Win32 operation.
- Prechecking a guest address range does not create a transaction: the target can unmap, protect or exit after validation. **Do not promise all-or-nothing writes**, and distinguish API failure from an assertion that no bytes changed.
- Host ptrace denial can prevent an otherwise guest-authorized operation. The compatibility layer must fail safely or use a separately justified cooperative transport; it must not silently acquire broad host privileges.
- Guest PID reuse and process-object reference lifetime require a stable host-process association. R73/R74/R67 are related, but none establishes that a bare PID is a safe guest handle identity.

## Proposed implementation hypothesis — unvalidated

For trusted guest processes launched under one supervisor, maintain a broker-owned process-object table (stable guest identity, host pidfd/lifetime, granted-rights metadata). Perform guest access validation before remote-copy operations. Prefer a cooperative, narrowly scoped per-process memory service when host ptrace policy disallows direct copies. Evaluate `process_vm_*` as an optimization only after the guest/host authorization and partial-result contracts are implemented.

This does **not** establish that a broker can safely read arbitrary guest addresses, that a pidfd itself authorizes memory access, or that a cooperative transport is faster.

## Falsifiable conformance and security gates (not run)

1. **Windows reference:** read/write fully accessible buffers; crossing a no-access/guard/unmapped page; zero-length calls; partial final pages; source/destination pointer failures; capture return, last-error, transferred-byte count and destination bytes separately.
2. **Rights matrix:** open process handles with only read, only write, write+operation, query-only, denied access and duplicated reduced-rights handles; distinguish documented requirements from observed OS-version-specific behavior.
3. **Concurrency:** target thread repeatedly unmaps/remaps or changes protection while remote read/write runs; record partial changes and failures without asserting transactional semantics.
4. **Linux transport:** separately test process_vm_* on adjacent valid/invalid remote iovecs, one remote iovec spanning a protection boundary, terminated target, host ptrace denial and Yama/LSM restrictions; record kernel version and exact errno/byte count.
5. **Identity/security:** recycle host PIDs, close guest handles while an operation is in flight, terminate target, and verify that no operation can target a replacement process or cross an unapproved guest security boundary.
6. **Integration:** Windows-target PE fixture imports `ReadProcessMemory`/`WriteProcessMemory` through the loader bridge and tests exact Win64 calling convention, guest handle rights and status translation. Only run controlled fixtures.

## Decision gate

Do not implement remote guest memory access as a bare `process_vm_*` wrapper. Require an explicit guest handle-rights model, target-lifetime binding, short-transfer handling, and a documented host-denial policy before enabling cross-process operations. Performance comparisons come **after** conformance and isolation tests.

## Provenance

Microsoft Learn API references and Linux man-pages links above, inspected 2026-11-15. These are published API contracts, not pinned source-code audits or observed experiments. Windows implementation details, undocumented rights exceptions and precise failure-side byte counts remain **OPEN**.
