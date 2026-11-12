# R4 — Native execution: definitions, risks and dependency map
Date: 2026-11-12.

## Terminology
- Direct machine-code execution: Windows x64 instructions run on an x86-64 CPU, without CPU emulation.
- Native OS execution: the OS directly implements the Windows execution contracts used by the program.
- Compatibility implementation: re-creating Windows API and NT semantics; unavoidable even if Wine/Proton are not dependencies.
- Driver compatibility: Windows kernel drivers depend on NT kernel ABI and trust policy, not merely user-mode PE loading.

## Minimal dependency graph
PE parser -> image mapper/relocations -> DLL resolver/imports -> ABI bridge -> PEB/TEB/TLS -> ntdll primitives -> process/thread and object services -> exceptions/unwind -> kernel32/Win32 -> GUI/graphics -> broad applications/games.

The arrows are approximate dependency order, not proof of a single universal startup path.

## Threat model
Untrusted PE input may exploit integer overflows, malicious directory offsets, overlapping sections, inconsistent image sizes, crafted relocations, import loops, and executable memory permissions. Parser and loader must enforce checked arithmetic, bounded allocations, W^X where feasible, and explicit trust boundaries. No executing arbitrary downloaded PE samples in the host development environment.

## Anti-cheat constraints
Windows kernel anti-cheat cannot be assumed to run on Linux by reimplementing Win32. Secure Boot/TPM alone do not imply vendor authorization. Vendor-supported Linux anti-cheat, trust attestation and privacy requirements must be investigated separately.

## Driver constraints
Prefer upstream Linux drivers for hardware support. Do not attempt generic execution of arbitrary Windows .sys drivers as an early milestone; kernel driver ABIs, DMA, interrupts and security create unacceptable complexity and risk.

## Candidate research architectures
A. Userspace loader + NT services, Linux kernel unmodified.
B. A plus selective kernel mechanisms (e.g. NTSYNC).
C. Deep NT executive inside Linux kernel.
Compare measured performance, compatibility, maintainability, safety and upstream integration before selecting.

## Unresolved high-priority issues
ABI bridge correctness; exception dispatch; Windows process/object semantics; filesystem case-insensitivity and path rules; Windows security tokens and access checks; registry; graphics stack; Windows anti-cheat vendor acceptance; licensing.
