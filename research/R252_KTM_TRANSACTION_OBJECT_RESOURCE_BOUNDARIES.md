# R252 — KTM transaction-object lifetime and transacted-resource boundaries

Date: 2026-11-22. Status: **documentation review only**; no Windows reference execution, Linux experiment, or implementation. Scope: x64 NT-on-Linux compatibility research; **not** a proposal to replicate Windows kernel drivers.

## Verified primary-source contracts

1. **Transaction objects are distinct NT objects.** Microsoft's `ZwCreateTransaction` contract creates a handle and accepts a unit-of-work GUID, transaction-manager handle, timeout, and access mask. Closing the last transaction handle before commit causes rollback. `ZwOpenTransaction` can obtain another handle by unit-of-work identity. Transaction-object deletion follows both handle closure **and** release of KTM internal references; last-handle close is not synonymous with immediate object destruction. [S1][S2][S3]
2. **Participation is not a filesystem flag.** KTM transactional clients pass a transaction identity to resource managers; an RM opens the transaction and enlists. A single transaction can coordinate multiple resource managers. Commit/rollback may be synchronous or asynchronous, and RMs must be able to recover relevant in-flight state from logs after a crash. [S4][S5]
3. **Registry handles carry transactional association.** `ZwCreateKeyTransacted`/`ZwOpenKeyTransacted` bind a registry key handle to a transaction. Subsequent operations on that key handle participate without each operation supplying the transaction handle. [S6][S7]
4. **TxF is a separate, discouraged consumer.** `CreateFileTransacted` associates a file handle with a transaction; ordinary handle-based I/O thereafter uses that association. Microsoft warns TxF may be removed and recommends alternatives. After a transaction ends, operations other than close on associated transacted file handles fail with `ERROR_HANDLE_NO_LONGER_VALID`. This does **not** imply that all KTM transaction APIs have been removed. [S8][S9]

## Architectural consequences (inference, not demonstrated)

**HYPOTHESIS H1:** A guest-owned transaction broker is required for any supported KTM subset: transaction identity, granted handle rights, state transitions, enlistments, per-resource associations, and terminal-state publication cannot be inferred from Linux file descriptors or a filesystem rename. This follows from S1–S7 but does not establish feasibility or performance.

**HYPOTHESIS H2:** Separate capability tiers should be explicit: (A) transaction object lifecycle and information APIs; (B) registry transactional semantics; (C) TxF file handles; (D) custom RM enlistment and crash recovery. Supporting A must not silently claim B–D. If a tier is unsupported, return a verified guest error rather than report success and perform nontransactional writes.

**OPEN O1:** Exact Windows-version behavior for close-last-handle versus outstanding enlistments, timeout, recovery and async completion; collect Windows reference traces before selecting an internal state machine.

**OPEN O2:** Whether contemporary target Windows builds expose/enable each TxF operation and what precise errors result on unsupported filesystems. Deprecation is not evidence of universal runtime unavailability.

**OPEN O3:** Crash-consistent cross-resource commit requires a durable protocol; Linux `rename`, `fsync`, and an ordinary database transaction do not alone prove equivalence to KTM's multi-RM protocol. No host implementation has been validated.

## Falsifiable acceptance gates (proposed, not executed)

- **T1 — handles and identity:** create a transaction, open it by UOW, duplicate/close handles in differing orders, query state; compare last-handle rollback and object lifetime on Windows.
- **T2 — rights:** open handles with distinct access masks; test commit, rollback, query, duplicate and cross-process inheritance; record exact NTSTATUS/Win32 error mapping.
- **T3 — registry:** open transacted and nontransacted handles to the same key, write through both, commit/rollback, inspect visibility and invalid-handle behavior; repeat with child processes.
- **T4 — file:** test supported and unsupported TxF targets, I/O on transacted handles before/after terminal state, and Microsoft-documented invalidation.
- **T5 — fault injection:** terminate participants before prepare, after prepare, and during commit; reboot Windows reference VM and compare recovery. This requires a controlled VM and is **not performed**.
- **T6 — security:** deny unauthorized cross-process access and transaction-handle substitution; ensure guest rollback cannot mutate host files outside the intended guest namespace.

## Placement and prioritization

This is a **compatibility boundary**, not a reason to select a modified Linux kernel now. Implementing a broad KTM resource-manager framework before the ordinary PE loader, NT handle table, registry, and file semantics are correct would be premature. First establish actual application demand and Windows-version reference behavior; maintain an explicit unsupported capability record.

## Primary sources (retrieved 2026-11-22)

- [S1] Microsoft, ZwCreateTransaction: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwcreatetransaction
- [S2] Microsoft, ZwOpenTransaction: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwopentransaction
- [S3] Microsoft, Transaction Objects: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/transaction-objects
- [S4] Microsoft, Creating a Transactional Client: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/creating-a-transactional-client
- [S5] Microsoft, Understanding TPS Components: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/understanding-tps-components
- [S6] Microsoft, ZwCreateKeyTransacted: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwcreatekeytransacted
- [S7] Microsoft, ZwOpenKeyTransacted: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwopenkeytransacted
- [S8] Microsoft, How to Use Transactional NTFS: https://learn.microsoft.com/en-us/windows/win32/fileio/how-to-use-transactional-ntfs
- [S9] Microsoft, CreateFileTransactedA: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfiletransacteda
