# R146 — NT handle reuse, close-versus-wait races, and object-reference ownership

Reviewed: 2026-11-17. **Primary API contract synthesis only. No experiment, benchmark, or Windows-reference run was performed.** This note is narrower than R67 (cross-process duplication), R68 (pseudo-handles), R75 (multi-object waits), and R87 (object names versus references): it identifies the *handle-slot reuse and concurrent wait* correctness boundary for an eventual Linux-hosted NT object broker.

## Source-grounded contracts (VERIFIED as documented; not experimentally validated here)

1. Microsoft explicitly warns that closing a handle while another thread waits on it with `WaitForSingleObject` results in **undefined behavior**. Therefore, a guest runtime must not invent a guaranteed Windows result for that race. [WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject).
2. `CloseHandle` invalidates the caller's handle, but a kernel object is deleted only after its last handle is closed (subject to object-specific references and lifetime). A handle value is not the object's permanent identity. [CloseHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle); [Handles and Objects](https://learn.microsoft.com/en-us/windows/win32/sysinfo/handles-and-objects).
3. `DuplicateHandle` creates a handle in the target process with specified access/inheritance properties; `DUPLICATE_CLOSE_SOURCE` closes the source handle even when duplication fails, according to the documented flag contract. This is a transaction with distinct source/target table effects. [DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle).
4. The Linux `pidfd_getfd(2)` interface duplicates a target *file descriptor* into the calling process subject to ptrace permission checks. It is not a general NT object-handle duplication primitive and does not encode NT granted-access masks. [pidfd_getfd(2)](https://man7.org/linux/man-pages/man2/pidfd_getfd.2.html).
5. Linux `close(2)` documents an important host-specific hazard: the descriptor number may be released early in close processing, and retrying a failed close can accidentally close a descriptor reused by another thread. This is not a Windows compatibility rule; it is a host backend design constraint. [close(2)](https://man7.org/linux/man-pages/man2/close.2.html).

## Architecture hypotheses (NOT IMPLEMENTED)

**H1 — Separate guest handle slots from host file descriptors.** A process-local guest table entry should carry a generation, granted-access mask, object type and a reference to a broker-owned object record. A Linux fd may be a *resource of* that object, but should never be used directly as its guest handle identity. The generation is an internal correctness aid; no claim is made that Windows exposes or guarantees generation-tagged handles.

**H2 — Pin object references before releasing table synchronization.** For a valid wait, resolve and authorize the handle under the guest-table lock, acquire a lifetime reference to the target object, and only then enter the blocking wait path. This avoids a host use-after-free if another thread closes the guest handle. It does **not** define the guest-visible result of the documented close-while-wait undefined-behavior case.

**H3 — Handle-slot ABA must not retarget an in-flight operation.** After a slot is closed and reused, an operation already bound to the old object must not silently act on the new object merely because the integer handle value matches. Retain an object reference or a checked generation token across deferred work, including I/O completion, wait registration and cancellation.

**H4 — Duplication and close-source must be explicitly ordered.** Treat `DUPLICATE_CLOSE_SOURCE` as a source-table mutation with its own failure and rollback matrix, not as a plain Linux `dup` followed by best-effort `close`. Guest access-right reduction and target-process authorization must be decided before publishing a destination handle. The exact Windows failure ordering needs differential tests.

## Falsifiable acceptance gates (PROPOSED; NOT RUN)

1. **Isolated handle-reuse stress:** repeatedly create, close and reallocate event handles while recording guest slot, generation and broker object identity. Assert no deferred wait or completion resolves through a newly reused slot. This is an internal safety invariant, **not** a claim about Windows integer reuse frequency.
2. **Reference lifetime:** duplicate an event handle, close one copy, signal through the other, and wait through a still-valid copy; compare Windows-reference results. Repeat with named and unnamed objects. Confirm that namespace lifetime and object lifetime are tracked separately (R87).
3. **Rights:** duplicate with reduced access, then attempt operations requiring rights not granted to that handle. Verify guest access checks are performed before host operations. Include cross-process target authorization (R67).
4. **Close-source failures:** on Windows reference, exercise invalid destination process, insufficient rights, and source-handle edge cases with `DUPLICATE_CLOSE_SOURCE`; record both source validity and target-table state. Do not infer behavior from a Linux fd prototype.
5. **Undefined race:** run close-while-wait only in isolated Windows-reference processes with watchdogs and mark outcomes **observations, not required semantics**. Independently require that the Linux-hosted implementation has no memory corruption or accidental operation on a reused object.
6. **Host fd reuse:** stress Linux fd allocation/closure while a broker operation holds an object reference. Record fd numbers, object generations, errors and whether any accidental close or access targets a newly allocated fd. Never retry `close` on the same numeric fd without proving ownership.

## Implementation gate

Before composing R115's M1 multi-thread lifecycle, specify one shared object-reference ownership protocol for waits, duplicate/close, async I/O and process exit. Separate three properties in test reports: **Windows-documented guest behavior**, **runtime-internal memory safety**, and **host fd correctness**. Passing one does not establish the others.

## Unresolved

- Windows-reference behavior for the defined duplicate/close-source failure matrix has not been measured.
- Broker locking, reclamation strategy (reference counts, epochs, or equivalent), and performance cost are not selected.
- The initial M1 fixture may avoid concurrent close/wait, but the runtime must not present such avoidance as proof of race correctness.

No source code was copied. All external links are primary vendor documentation or the Linux man-pages project; exact page revisions were not independently fetched during this run.
