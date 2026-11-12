# LinuxNTAlpha

Research-first project investigating native execution of Windows PE applications on Linux through independently implemented NT-compatible interfaces.

## Goals
- Direct execution of x86-64 Windows machine code, without emulation or virtualization.
- No Wine or Proton runtime dependency in the eventual NT subsystem.
- Evidence-based performance, memory, input latency, security, and compatibility improvements.
- Linux driver ecosystem retained; desktop OS later, console variant only after desktop completion.

**Important:** Direct CPU execution does not eliminate the need to implement Windows APIs and NT semantics. This is not yet a functioning NT runtime.

## Current stage
Phase R0: research, architectural evaluation, licensing, and reproducible experiments. No kernel fork or implementation architecture has been selected.

See [research plan](research/PLAN.md), [initial findings](research/INITIAL_FINDINGS.md), and [decision log](research/DECISIONS.md).

For the full evidence trail, see [research index](research/INDEX.md). The [experiment suite](experiments/) has executable, intentionally narrow probes for syscall interception, MS/SysV ABI calls, ntsync UAPI/device capability, and **native execution of two custom-built PE32+ fixtures**, including one actual import table bound to a Linux-hosted MS-ABI function (R19/R20). On x86-64 Linux with GCC/Clang, LLVM clang and lld-link, Python 3 and Make, run **make -C experiments test**; read each result for **PASS** versus **SKIP**. The ntsync runtime test was unavailable in the initial container. The PE experiments are **not** general Windows application compatibility, a sandbox or a complete NT runtime. No ntsync kernel benchmark or full NT execution claim has been made.

## Principles
1. Measure before optimizing. Compare against Linux and Windows baselines on equivalent hardware.
2. Research existing code before writing replacements; respect licenses.
3. Keep kernel modifications minimal until experiments justify them.
4. Treat Windows anti-cheat support as dependent on vendor trust and authorization, not just API compatibility.
5. Never claim universal performance gains or compatibility without evidence.
