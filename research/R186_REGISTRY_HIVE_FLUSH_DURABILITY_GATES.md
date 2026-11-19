# R186 — Registry hive flush durability and Linux persistence gates

Status: SOURCE-VERIFIED contracts; HYPOTHESIS implementation; OPEN conformance. Reviewed 2026-11-19. No experiments performed.

## Why this is a distinct gate

R38 addresses registry WOW64/notifications and R110 addresses notification lifetime. This note instead addresses the **durability acknowledgement boundary** between visible in-memory registry writes and persisted hive state. It does not claim that a Linux file format is a Windows hive format.

## VERIFIED FROM PRIMARY DOCUMENTATION

1. Microsoft states that registry modifications are visible to other processes **without** calling RegFlushKey. Windows uses periodic lazy flush and shutdown flush; an explicit RegFlushKey is expensive, blocks modifications to keys in the affected hive during flush, and returns after the hive's data has been written to its registry store. It can flush other dirty keys in that hive, not just the supplied key. Source: [Microsoft RegFlushKey](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regflushkey) (documentation reviewed 2026-11-19; page lists update 2024-02-22).
2. Microsoft documents that RegSaveKeyEx saves a key and descendants to a **new** file; an existing destination causes failure, the caller requires enabled SE_BACKUP_NAME, and an interrupted/failed save can leave a corrupt file. It is not equivalent to a normal flush and is not a replacement for system-state VSS backup. Source: [Microsoft RegSaveKeyExA](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsavekeyexa) (reviewed 2026-11-19).
3. Microsoft states RegSaveKey saves nonvolatile keys, not volatile keys. Source: [Microsoft RegSaveKeyW](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsavekeyw) (reviewed 2026-11-19).
4. Linux rename(2) atomically replaces an existing pathname in the namespace, but an existing open descriptor still refers to its prior file; namespace atomicity is **not** an NT registry hive transaction or proof of crash durability. Source: [Linux rename(2)](https://man7.org/linux/man-pages/man2/rename.2.html) (reviewed 2026-11-19).
5. Linux fsync(2) flushes file data and associated metadata, but syncing a file does not necessarily ensure its directory entry is persisted; fsync the containing directory for that guarantee. Source: [Linux fsync(2)](https://man7.org/linux/man-pages/man2/fsync.2.html) (reviewed 2026-11-19).

## HYPOTHESIS — possible subsystem design, not selected architecture

Maintain an authoritative in-memory, access-controlled guest registry namespace and per-hive dirty-generation ledger. Guest writes become visible at publication, while durable generation advances only after a successful host persistence barrier. For a snapshot strategy, write a new complete snapshot into a confined same-filesystem temporary file, fsync it, atomically rename, and fsync the parent directory before acknowledging explicit flush. A write-ahead log is an alternative, but would require its own replay, checksumming, truncation and ordering proof. Either strategy must serialize a flush against concurrent writes and prevent a later snapshot from superseding an earlier durable generation out of order.

This is **not** a claim that all filesystems, storage controllers or power-failure models guarantee identical persistence; nor that the Microsoft flush API exposes a numeric generation. The generation ledger is an internal proposal. Host permissions and Linux uid are not substitutes for Windows key ACL/token checks. A privileged registry service must confine paths and reject symlink/path traversal.

## OPEN conformance and failure gates

- Windows reference: two processes mutate distinct keys in one hive; verify immediate visibility without flush, then explicit flush, while recording whether concurrent writes block and when. Avoid asserting exact latency.
- Windows reference: volatile versus nonvolatile key behavior across reboot; RegSaveKeyEx existing destination, insufficient privilege and partial-failure behavior. Use disposable test hives and a controlled VM; do not deliberately crash a production host.
- Linux prototype: inject failure at each write, fsync, rename and directory-fsync boundary; verify restart recovery and never acknowledge a generation not recovered. Record filesystem, mount options, kernel, hardware and fault-injection mechanism.
- Race: writer A, writer B, concurrent flushes, process exit and registry-service crash; distinguish in-memory visibility, durable state and notification ordering.
- Resource/security: bound snapshot size and write amplification; enforce guest hive ownership, per-key access, and isolation from host registry-equivalent configuration.
- Measure: p50/p95/p99 flush and ordinary write latency, write amplification and cross-process blocking under identical controlled workloads. No performance result is asserted here.

## Architecture implication

Do **not** implement RegFlushKey as a no-op or equate ordinary RegSetValueEx success with stable-storage persistence. Conversely, fsync on every guest write would impose a stronger and potentially costly policy than Microsoft's documented lazy-flush behavior. A registry service with explicit persistence generations is a candidate pending Windows-reference and Linux crash-recovery tests.
