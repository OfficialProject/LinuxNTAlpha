# R1 — PE64 process startup and NT synchronization: primary-source audit
Date: 2026-11-12
Status: research findings; no implementation committed

## Sources reviewed
1. Microsoft PE/COFF format: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
2. Linux ntsync UAPI: https://www.kernel.org/doc/html/latest/userspace-api/ntsync.html
3. Linux ntsync upstream source documentation: https://github.com/torvalds/linux/blob/master/Documentation/userspace-api/ntsync.rst
4. ReactOS architecture overview: https://reactos.org/architecture/

## Verified findings

### PE64 loading is a multi-stage process
Microsoft documents the PE signature, COFF header, PE32+ optional header, section mapping, import address table, base relocation blocks, TLS directory and exception directory. A loader must validate bounds and sizes before mapping sections or following data directories. For x64 images the machine type is 0x8664. Base relocation handling is required if an image cannot load at its preferred base and relocations are available. Import binding updates the IAT with actual addresses. x64 exception metadata resides in .pdata and associated unwind information.

### Windows process startup requires more than executable mapping
Windows-specific TLS, process/thread environment, DLL initialization and exception handling remain unsatisfied by ordinary Linux ELF process startup. A PE parser that reads headers is not a Windows execution environment. We must distinguish file-format conformance, loader conformance, and application execution.

### ntsync is existing Linux kernel support for NT synchronization
The upstream ntsync user-space API implements NT-style semaphores, mutexes and events, including manual/auto reset event semantics and abandoned mutex handling. Objects are exposed as file descriptors. Its documentation explicitly describes it as support for user-space NT emulators, not a general-purpose replacement for futex/poll. This is an existing candidate for targeted kernel integration, not proof that all NT semantics belong in kernel space.

### ReactOS is a source of NT architectural decomposition
ReactOS documents kernel object, I/O, process/thread and other executive components separately from Win32 libraries, system services and shell. The existence of these layers reinforces that PE execution, NT semantics and Win32 compatibility are distinct engineering problems.

## Candidate architecture hypotheses (NOT decisions)
H1: A small trusted PE64 loader plus user-mode NT services on an upstream Linux kernel minimizes maintenance cost.
H2: ntsync may provide superior synchronization semantics/performance for some Windows workloads; quantify with controlled tests.
H3: A dedicated Windows ABI entry bridge and per-thread environment are necessary before nontrivial x64 PE applications can execute.
H4: Early kernel modifications will likely be unnecessary for a minimal console program; verify with a prototype.

## Critical engineering questions
- How to establish x64 Windows GS-based thread environment without conflicting with Linux TLS/runtime assumptions?
- How to handle x64 Windows unwind/SEH and translate Linux faults/signals while preserving semantics?
- What is the minimal ntdll import surface for an independently compiled Windows fixture?
- Which NT object/handle semantics can be implemented entirely in user space without unacceptable overhead?
- What are the attack surfaces of mapping untrusted PE sections and parsing relocation/import/TLS tables?
- What are the exact licensing and provenance constraints on any candidate reused code?

## Proposed next experiments
1. Build a corpus of deliberately minimal PE32+ x86-64 binaries, including malformed headers, imports, relocations, TLS and exception metadata.
2. Implement a memory-safe, bounds-checked read-only parser as a disposable experiment; compare fields to Microsoft's specification.
3. Implement an isolated image mapper with W^X protections and deterministic tests; no executing untrusted input.
4. Measure Linux eventfd/futex synchronization and ntsync where available, using identical test semantics and reporting variance.
5. Only then prototype direct machine-code entry with a documented Windows ABI and minimal runtime.

## Definition of evidence
VERIFIED = directly supported by linked primary specification/source; HYPOTHESIS = plausible design not measured; OPEN = unresolved. Do not convert hypotheses into requirements without tests.
