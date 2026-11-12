# R35 — Fibers, fiber-local storage, and thread identity

Date: 2026-11-12. **Status: verified Microsoft documentation review only. No Windows reference execution, Linux prototype, or performance measurement.**

## Why this is a distinct compatibility gate

R24/R25 test a synthetic per-*thread* GS/TIB pointer. R27 examines PE TLS. Neither demonstrates Win32 **fiber** switching or **fiber-local storage** (FLS). A fiber is application-scheduled execution within an OS thread, not another Linux thread. Treating a fiber as a pthread would alter thread identity, TLS, scheduling, and observable behavior.

## Verified from Microsoft primary sources

1. Microsoft's [Fibers overview](https://learn.microsoft.com/en-us/windows/win32/procthread/fibers) says fibers are cooperatively scheduled by the application, run in the context of the thread scheduling them, and retain a stack, a subset of registers normally preserved across calls, and creation-time fiber data. The OS still schedules the containing thread.
2. The same overview distinguishes ordinary **thread-local storage**, which follows the executing thread, from **fiber-local storage**, which changes with the selected fiber. The thread must first be converted to a fiber before switching; the overview describes `ConvertThreadToFiber`, `CreateFiber`, and `SwitchToFiber`.
3. The overview explicitly says a fiber created by another thread can be switched to, subject to proper synchronization. It also warns that deleting a fiber selected by another thread can lead to abnormal termination. Consequently, an implementation must not assume that fiber identity is permanently bound to one host thread.
4. [FlsAlloc](https://learn.microsoft.com/en-us/windows/win32/api/fibersapi/nf-fibersapi-flsalloc) allocates a process-scoped index, optionally with a callback. Each fiber has a distinct value for the index. The index is **not** portable across processes; allocation failure returns `FLS_OUT_OF_INDEXES`.
5. [FlsFree](https://learn.microsoft.com/en-us/windows/win32/api/fibersapi/nf-fibersapi-flsfree) frees an index across all FLS instances in the process and invokes the associated callback for each fiber whose slot holds a non-NULL value. It does not automatically free the memory pointed to by those values.
6. The [Fibers overview](https://learn.microsoft.com/en-us/windows/win32/procthread/fibers) states that returning from a fiber entry function terminates the thread running it; deleting the currently running fiber likewise terminates its thread. This is not equivalent to returning from an ordinary function.

These sources describe published Win32 behavior, **not** a version-pinned NT kernel ABI or a verified internal TEB/FLS memory layout.

## Architectural hypotheses, not implementation results

- **HYPOTHESIS:** Represent host thread identity and guest fiber identity as distinct entities. Maintain FLS indices at guest-process scope and FLS values at fiber scope; TLS must remain associated with the currently executing thread.
- **HYPOTHESIS:** Implement fiber context switching in userspace with an x64 Microsoft-ABI-aware context-save/restore routine. Audit all nonvolatile integer and vector registers, stack alignment, exception/unwind behavior, stack guard pages, and extended processor state before calling it compatible. A generic `setjmp/longjmp` pair is not automatically sufficient.
- **HYPOTHESIS:** If the guest fiber can migrate to another host thread, a per-thread TEB and fiber-specific FLS view must be reconciled without corrupting glibc FS TLS or the guest GS-based thread view tested in R24/R25.
- **OPEN:** Precise callback invocation order, reentrancy, fiber deletion and thread-exit cleanup across Windows versions; whether migration affects particular TEB fields; FLS index reuse under concurrent frees; interactions with DLL unload, exceptions and activation contexts.
- **OPEN:** Determine which observable behavior requires full emulation for the first targeted program cohort. This is a compatibility gate, **not** evidence that fiber support belongs in the Linux kernel.

## Proposed falsifiable conformance matrix — NOT RUN

| Windows reference case | Observation to record | LinuxNTAlpha gate |
| --- | --- | --- |
| Convert thread, create two fibers, switch A→B→A | Thread ID, fiber pointer/data, return order | Same identity and switching sequence |
| Set different TLS and FLS values on two fibers of one thread | TLS value, FLS value at each switch | Thread TLS stable, fiber FLS isolated |
| Switch a fiber created on another thread with synchronization | Thread ID, FLS, TLS and return paths | Matches reference or documents version-specific restriction |
| Free FLS index with NULL/non-NULL slots | Callback count, values and order | No lost or duplicate callbacks; exact reference behavior |
| Delete dormant fiber and return from fiber entry | Cleanup, thread termination, callbacks | Matches reference |
| Guard page and nested call stacks | Stack limits, faults, unwind paths | No silent stack corruption |
| Concurrent FLS index reuse and DLL teardown | Callback reentrancy, errors, lifetime | Race-safe and reference-conformant |

Reference harness requirements: record Windows edition/build, CPU architecture, compiler and optimization, FLS callback logs, thread IDs and exact call order. Run equivalent LinuxNTAlpha tests only after a real implementation exists. Do not label a skipped reference test as PASS.

## Implication for next work

A minimal PE64 loader that initializes GS:0x30 and handles one import (R25/R28) has not established fiber support. Before using a userspace context-switching library, audit its license, Microsoft x64 calling convention support, signal/unwind interactions and stack lifecycle. Prioritize reference tests for TLS-vs-FLS isolation and callback cleanup; then decide whether a custom context switch is justified.

## Sources inspected

- Microsoft Learn, *Fibers* (page updated 2025-07-14): https://learn.microsoft.com/en-us/windows/win32/procthread/fibers
- Microsoft Learn, *FlsAlloc* (page updated 2024-02-22): https://learn.microsoft.com/en-us/windows/win32/api/fibersapi/nf-fibersapi-flsalloc
- Microsoft Learn, *FlsFree* (page updated 2024-02-22): https://learn.microsoft.com/en-us/windows/win32/api/fibersapi/nf-fibersapi-flsfree
