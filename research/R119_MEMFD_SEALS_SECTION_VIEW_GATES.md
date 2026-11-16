# R119 — Linux memfd sealing versus mutable NT section views

Date: 2026-11-16. Status: **VERIFIED FROM PUBLISHED API CONTRACTS; NO EXPERIMENT**.

## Scope and prior-work boundary

R11/R59/R94/R105 examine section identity, image mapping, inheritance and commitment. R118 covers large pages. This note isolates a separate implementation hazard: **irreversible Linux file seals versus NT section view permissions, writes and lifetime**. It does not claim that Windows exposes Linux-style sealing or that memfd is the chosen backing store.

## Verified API contracts

1. Linux [memfd_create(2)](https://man7.org/linux/man-pages/man2/memfd_create.2.html) creates an anonymous file usable with `ftruncate`, `mmap`, and descriptor passing. `MFD_ALLOW_SEALING` starts with no seals; without it, `F_SEAL_SEAL` initially prevents adding seals. This is a Linux file-descriptor/inode facility, not an NT section API.
2. Linux [F_ADD_SEALS(2const)](https://man7.org/linux/man-pages/man2/F_ADD_SEALS.2const.html) specifies that seals are shared by descriptors for the inode and **can only be added**, not removed. `F_SEAL_WRITE` cannot be added while a writable shared mapping exists (`EBUSY`). `F_SEAL_FUTURE_WRITE` prevents new writable mappings and `write(2)` but permits already-existing shared writable mappings to continue changing the contents. `F_SEAL_GROW` and `F_SEAL_SHRINK` restrict file size changes. `F_SEAL_EXEC` (Linux 6.3+) freezes execute mode and implies other seals; it is not an NT per-view execute-permission policy.
3. Microsoft [CreateFileMappingW](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-createfilemappingw) distinguishes section-creation protection from view access; [MapViewOfFile](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile) specifies read/write, read-only, copy-on-write and executable views subject to mapping-object protection. Its documentation also states that coherent views of the same backing file are generally visible across processes, with a remote-file exception, and that mapped writes are not guaranteed coherent with ordinary `ReadFile`/`WriteFile` operations.
4. Microsoft [MapViewOfFile](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile) documents `FILE_MAP_COPY` as a private copy-on-write view, distinct from a shared writable view. A Linux shared mapping with `F_SEAL_FUTURE_WRITE` is **not** equivalent to Windows copy-on-write merely because subsequent writable mappings are refused.

## Architectural hypotheses — not validated

- A memfd can plausibly hold shared bytes for **some** pagefile-backed guest sections, but guest section maximum protection, per-handle access rights, per-view protection and private-copy state should remain explicit in the NT compatibility layer.
- Applying irreversible Linux seals as a shortcut for guest `PAGE_READONLY`, `PAGE_WRITECOPY`, `VirtualProtect` or section handle access reduction risks permanent global restrictions that outlive a particular guest view. Avoid such translation until Windows reference tests prove the desired lifetime and transition semantics.
- A broker could maintain a writable backing descriptor while handing out narrower guest capabilities, but all host descriptor transfers and mappings must be mediated; a guest with unfiltered Linux syscalls could bypass the broker. This is a security hypothesis, not proof of containment (see R69/R90).
- A separate immutable-code backing policy may be valuable for integrity, but it must not be silently conflated with Windows image section protections or executable mapping behavior (R59/R117).

## Proposed falsification gates — NOT EXECUTED

1. **Linux capability matrix:** record kernel release, memfd flags, seals and error codes for `F_SEAL_WRITE` with/without existing `MAP_SHARED|PROT_WRITE` views, `F_SEAL_FUTURE_WRITE` with preexisting writable views, attempts to create later writable views, and descriptor transfer across processes. Test in disposable child processes; no host-global sysctl changes.
2. **Windows reference:** for pagefile-backed sections, create read/write and copy-on-write views across two processes; record `VirtualQuery`, permitted writes, visibility, `VirtualProtect` changes, handle closure and unmapping. Include access-denied negative controls and exact OS build. Do not assume `VirtualProtect` may exceed the section's maximum protection.
3. **State-model differential:** distinguish (a) backing bytes, (b) section identity and maximum protection, (c) guest handles and granted access, (d) view-specific permissions, (e) COW privatization, and (f) host memfd seals. Reject transitions where a host irreversible seal would make a Windows-permitted future operation impossible.
4. **Adversarial boundary:** attempt host-fd leakage, unmediated `mmap`, and stale-view use in a deliberately isolated harness. Passing normal application tests does not establish a security boundary.
5. **Resource controls:** exercise resizing, zero-fill and mapping lifetime under host memory/cgroup limits; integrate R105 accounting and R118 huge-page capability gates rather than equating a successful `ftruncate` with committed NT memory.

## Decision gate

Do not encode NT view permissions as Linux inode seals. Keep seals an **optional host integrity mechanism** until the versioned Windows differential and Linux negative tests establish a safe, narrowly specified mapping. No experiment, benchmark, Windows execution, or code change was performed for this note.

Primary documentation consulted 2026-11-16:
- https://man7.org/linux/man-pages/man2/memfd_create.2.html
- https://man7.org/linux/man-pages/man2/F_ADD_SEALS.2const.html
- https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-createfilemappingw
- https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile
