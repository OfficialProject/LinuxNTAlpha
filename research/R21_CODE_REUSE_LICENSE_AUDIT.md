# R21 — Cross-project code reuse and licensing: primary-source audit

Date: 2026-11-12. This is technical compliance scoping, **not legal advice**. No third-party implementation code has been imported into LinuxNTAlpha.

## Source-verified upstream licenses

**Linux kernel:** https://github.com/torvalds/linux/blob/master/COPYING (blob SHA a635a38ef9405fdfcfe97f3a435393c1e9cae971) identifies **GPL-2.0 WITH Linux-syscall-note**. Official kernel documentation https://docs.kernel.org/process/license-rules.html describes the syscall exception and distinction for userspace API headers. The ntsync UAPI header at https://github.com/torvalds/linux/blob/v6.14/include/uapi/linux/ntsync.h carries the Linux-syscall-note SPDX expression. This permits using the documented userspace interface without assuming all userspace applications become GPL; copying **kernel driver implementation code** such as drivers/misc/ntsync.c is legally different and needs specific review.

**ReactOS:** https://github.com/reactos/reactos/blob/master/README.md (blob SHA db833c41a5eda29d1b1b7f90fa3472996add33cc) labels the ReactOS project GPL-2.0. https://github.com/reactos/reactos/blob/master/COPYING (blob SHA 513a9494538ceb111dad89bbd2752f0bbe127f39) contains GPL v2. https://github.com/reactos/reactos/blob/master/COPYING.LIB (blob SHA 67870f655ec189a8382cb0168b0d267de05d9e84) contains LGPL 2.1. ReactOS' official https://reactos.org/intellectual-property-guideline/ specifies nearly all ReactOS code is GPL but some code is LGPL/BSD/public domain and sets out its binary-linking policy for proprietary apps and device drivers. **That policy does not grant permission to copy any ReactOS source under arbitrary terms.** Determine per-file licensing and provenance, not just the root repository license.

**Wine:** canonical upstream https://gitlab.winehq.org/wine/wine was access-restricted during this research, so this audit inspected the public Wine GitHub mirror https://github.com/wine-mirror/wine/blob/master/LICENSE instead. Its LICENSE blob SHA 3909c6e050cd238630e2bb9baa7de3ea199bf61e states **LGPL-2.1-or-later**. Study is appropriate; copying or modifying implementation code creates distinct attribution, license and distribution obligations. No Wine/Proton **runtime dependency** does not mean Wine's code license ceases to apply if code is reused.

**LLVM:** https://github.com/llvm/llvm-project/blob/main/llvm/LICENSE.TXT blob SHA fa6ac540007032cbd0ec772a1c72e6cb5527a4fe describes **Apache-2.0 WITH LLVM-exception**. Using Clang and LLD to produce self-authored fixtures does not mean their tool implementation is copied into LinuxNTAlpha. Binary distribution/reuse must be assessed separately.

## Windows specifications are not a proprietary-code grant

Microsoft's public PE/COFF format and Windows API documentation support developing independently written interoperable code, but are not a blanket license to copy proprietary Windows ntoskrnl/ntdll implementation code. Our experiments build minimal synthetic fixtures from original code with LLVM; they don't redistribute Microsoft binaries.

## Repository and decisions

As verified from the root listing on 2026-11-12, the LinuxNTAlpha repository does **not currently contain a root LICENSE file**. Public visibility is not the same as an open-source license. The project owner should select the eventual license before inviting contributions, distributing releases or importing third-party source. **No license choice has been made on the user's behalf.**

Before any imported component, record: canonical repository and pinned commit; individual file path, notices and SPDX expression; provenance; study vs copy vs modification vs link; dependencies; intended target binary/library/kernel distribution; obligations for notices, source and modification; and separate legal review where ambiguity exists.

**Acceptance gate:** No upstream implementation code may be copied into the project until the relevant source-file license and compatible intended distribution are documented and approved. Independently written spec-driven experiments may continue.

This note is not a legal opinion, and does not attempt to settle complex compatibility or derivative-work questions by inference.
