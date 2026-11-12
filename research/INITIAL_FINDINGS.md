# Initial findings — 2026-11-12

## VERIFIED: Linux executable dispatch is not NT compatibility
Linux binfmt_misc dispatches binaries by magic bytes or extension to a configured handler. It does not itself implement the PE loader, Windows ABI, DLL imports or NT services.
Source: https://www.kernel.org/doc/html/latest/admin-guide/binfmt-misc.html

## VERIFIED: ReactOS is relevant, but not a drop-in Linux subsystem
ReactOS targets Windows NT compatibility, identifies its OS as alpha-quality, and describes substantial Wine-derived user-mode code. Its repository lists GPL-2.0 and other licenses. Individual files require license review.
Sources: https://github.com/reactos/reactos and https://reactos.org/intellectual-property-guideline/

## VERIFIED: Wine is not CPU emulation by default
Wine provides a loader and implementations of Windows APIs on Unix-like systems. Removing Wine as a runtime dependency does not eliminate the Windows API compatibility problem.
Source: https://github.com/reactos/wine (Wine README mirror; verify against canonical upstream before implementation)

## HYPOTHESIS: kernel-level NT services may improve some workloads
Needs benchmarks against existing Linux/Wine implementations. More kernel code may worsen security and maintenance. No speedup established.

## OPEN
- Precise minimal x86-64 PE test binary and its required NT contracts.
- Latest kernel ntsync semantics and upstream source audit.
- Feasibility of NT process/exception semantics without intrusive kernel changes.
- Vendor-supported anti-cheat and attestation requirements.
- License compatibility of candidate components.
- Comparative benchmark harness and hardware access.

## First experiment
Construct a minimal PE64 executable fixture, verify parsing, relocations/imports and ABI setup in a controlled loader harness, then progressively add Windows-compatible runtime services. Parsing alone is not counted as execution success.
