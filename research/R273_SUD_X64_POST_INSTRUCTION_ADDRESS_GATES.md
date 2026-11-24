# R273 — SUD x86-64 post-instruction address and rollback contract

Reviewed 2026-11-24. Source audit only; no experiment, compilation or benchmark. Extends R234/R271/R272 with a version-specific kernel-level address oracle, rather than another selector state-machine proposal.

## Verified source findings (Linux v6.12 tag)

* [entry_64.S](https://github.com/torvalds/linux/blob/v6.12/arch/x86/entry/entry_64.S), blob 1b5be07f86698a3b634a0d83b6578775781d1739: entry_SYSCALL_64 constructs pt_regs with IP taken from RCX, which holds the return address following SYSCALL. The original accumulator is saved separately. The instruction does not push a guest stack frame.
* [syscall_user_dispatch.c](https://github.com/torvalds/linux/blob/v6.12/kernel/entry/syscall_user_dispatch.c), blob 5340c5aa89e7dd33f30a3543e4f73bfc08ea5a4e: syscall_user_dispatch first tests unsigned instruction_pointer(regs) - offset < len; then checks the architecture vDSO sigreturn exception; only then reads the selector. An unreadable selector forces SIGSEGV exit, an invalid selector value forces SIGSYS exit, and a blocked call sets on_dispatch, rolls back the syscall and constructs SIGSYS with SYS_USER_DISPATCH, KSTK_EIP(current), architecture and syscall number. No selector rearm is performed here.
* [x86 syscall.h](https://github.com/torvalds/linux/blob/v6.12/arch/x86/include/asm/syscall.h): syscall_rollback copies orig_ax into ax; syscall_get_nr returns an int derived from orig_ax. Native Linux argument extraction uses DI, SI, DX, R10, R8, R9; this is not a guest NT argument decoder.

## Derived implications (not runtime observations)

For the native x86-64 SYSCALL entry path, the region comparison operates on the saved continuation IP, not necessarily the address of the opcode. For a two-byte unprefixed SYSCALL at P, the expected comparison address is P+2. The region is half-open: a continuation exactly at offset+len is outside, while one exactly at offset is inside. Thus a host stub whose opcode is inside a region but whose continuation lies at the upper boundary can still trap. Conversely, an opcode immediately before the region may have a continuation inside it. Do not use a mapping-membership test of the opcode alone as proof of dispatch classification.

The rollback restores the syscall number, not a successful NT result; an emulator must explicitly publish its chosen return value. The source's vDSO exception is architecture-specific; do not infer that arbitrary libc signal restorers qualify.

## Proposed isolated fixture gates (all unrun)

1. Generate trusted assembly stubs with labeled opcode and continuation addresses. Record disassembly and runtime addresses; configure the legacy ON region with selector BLOCK. Use harmless getpid collision calls and a controlled signal-return path.
2. Sweep continuation addresses at offset-1, offset, offset+len-1 and offset+len using separate configurations around the same stub. Expected region classifications are outside, inside, inside, outside. Independently record actual Linux completion versus SIGSYS, not just the accumulator.
3. Record si_call_addr, saved RIP, si_arch, si_syscall, saved RAX and labeled continuation for each trap. Compare against the kernel source-derived oracle; do not silently subtract two from arbitrary signal addresses or apply this oracle to INT 0x80, SYSENTER or prefixed encodings.
4. Verify selector precedence by repeating an inside-region call with BLOCK and an outside-region call with ALLOW. Test invalid/unmapped selector only in disposable children; verify the expected fatal signal rather than attempting recovery.
5. For the R271 multi-trap fixture, place the trusted host syscall and signal restorer continuation strictly inside the configured region, and guest continuation sites strictly outside. Confirm the actual restorer address by disassembly, not by assuming libc uses the vDSO. Keep BLOCK throughout the proposed region-only path. This is a candidate implementation, not a proven fix.
6. Record exact kernel build/configuration, compiler, libc, binary hashes, address layout, ordered trap records and exit status. Repeat on newer dispatch modes only after auditing their corresponding source; v6.12 results cannot establish newer-mode semantics.

## Decision

OPEN. Region-based dispatch remains a candidate for eliminating the selector rearm gap in trusted fixtures. Its address-boundary contract must be verified before promoting it. It is not a containment mechanism: guest-controlled control flow may reach allowed syscall sites. No architecture selection or performance claim follows.
