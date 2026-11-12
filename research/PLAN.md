# Research plan — 16-week framework

This is a sequence of active-work stages, not an autonomous schedule or delivery promise.

## Weeks 1–4: semantics and feasibility
- Map PE/COFF, x86-64 Windows ABI, TEB/PEB, loader, TLS, SEH/unwind, ntdll, NT object and handle semantics.
- Map Linux process, VM, scheduling, signal, futex, filesystem and security interfaces.
- Identify mismatches and document candidate implementations.
- Begin driver, anti-cheat, licensing and security constraints immediately.

## Weeks 5–8: source audits
- Review ReactOS, Wine, Linux binfmt_misc, ntsync and relevant prior art.
- Capture upstream revision, file paths, licenses, subsystem boundaries and known limitations.
- Distinguish reference behavior from reusable code; no code copying until license review.

## Weeks 9–12: controlled experiments
- Compare executable launch and syscall/IPC overhead, thread synchronization, page faults, memory usage and scheduler wake-up latency.
- Measure with repetitions, variance, hardware/software configuration, and comparable baselines.
- Prototype PE loading and ABI handling in isolated test harnesses; do not confuse PE parsing with Windows compatibility.

## Weeks 13–16: architectural decision
- Evaluate user-space NT services, targeted kernel extensions, and deeper NT kernel integration.
- Score performance evidence, compatibility, security, driver ecosystem, maintenance and licensing.
- Deliver architecture decision records, component interfaces, prioritized backlog and reproducible acceptance tests.

## Research exit criteria
- Every major architecture claim backed by a primary source or experiment.
- Critical unknowns explicitly tracked.
- Licensing strategy reviewed.
- Native execution milestone precisely defined and testable.
- No assertion of anti-cheat compatibility without vendor support.

## Evidence policy
Tag claims as VERIFIED (source or reproduced measurement), HYPOTHESIS, or OPEN. Record source URL, retrieval date, upstream revision where applicable, and experiment environment.
