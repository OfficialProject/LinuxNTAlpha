# R184 — seccomp user notification as an NT syscall-broker candidate

Status: **primary-documentation review and architecture hypothesis**, 2026-11-19. No experiment, benchmark, or implementation performed.

## Decision question

Could Linux seccomp user notification replace or complement Syscall User Dispatch (SUD) for an independently implemented NT syscall path, while preserving Linux-hosted runtime syscalls and process security? This is a *broker protocol* evaluation, not a claim that seccomp implements NT services.

## Verified upstream contracts

1. **VERIFIED (Linux kernel documentation):** A seccomp filter returning `SECCOMP_RET_USER_NOTIF` can delegate a syscall decision to a supervising listener. The listener obtains a notification file descriptor using `SECCOMP_FILTER_FLAG_NEW_LISTENER` and communicates with `SECCOMP_IOCTL_NOTIF_RECV` / `SECCOMP_IOCTL_NOTIF_SEND`. The kernel explicitly warns that user notification is **not intended for implementing security policy**. Source: [Linux seccomp user notification](https://docs.kernel.org/userspace-api/seccomp_filter.html#userspace-notification).
2. **VERIFIED (Linux manual-page contract):** A notification includes a syscall number, arguments, architecture and instruction pointer, but the supervisor must validate notification identity with `SECCOMP_IOCTL_NOTIF_ID_VALID` when relying on a pending request. Notifications and tasks have lifetimes; a task can disappear or a notification become invalid. Source: [seccomp_unotify(2)](https://man7.org/linux/man-pages/man2/seccomp_unotify.2.html).
3. **VERIFIED (Linux manual-page contract):** `SECCOMP_USER_NOTIF_FLAG_CONTINUE` allows the intercepted syscall to proceed; the documentation warns about time-of-check/time-of-use races and explicitly cautions against treating this mechanism as a security-policy engine. Source: [seccomp_unotify(2)](https://man7.org/linux/man-pages/man2/seccomp_unotify.2.html).
4. **VERIFIED (Linux manual-page contract):** `SECCOMP_IOCTL_NOTIF_ADDFD` supports installing a supervisor-provided file descriptor into the target, with documented flags and availability constraints. This is a Linux-fd operation, not an NT object-handle transfer. Source: [seccomp_unotify(2)](https://man7.org/linux/man-pages/man2/seccomp_unotify.2.html).
5. **VERIFIED (Linux kernel documentation):** Seccomp BPF observes syscall-entry data, cannot dereference guest pointers, and is not a complete sandbox by itself. Filter action precedence and thread synchronization must be considered. Source: [seccomp BPF](https://docs.kernel.org/userspace-api/seccomp_filter.html).
6. **VERIFIED (project-specific evidence):** R26 demonstrates one Windows-target PE raw syscall intercepted by SUD; it does **not** demonstrate seccomp notification, a broker, repeated dispatch, or NTSTATUS translation. See [R26](R26_PE64_RAW_SYSCALL_SUD.md), [R90](R90_GUEST_HOST_SYSCALL_PROVENANCE.md), and [R164](R164_SUD_SIGSYS_ALTSTACK_CONTINUATION_GATES.md).

These are documented API contracts, **not** confirmation of feature availability on any LinuxNTAlpha test host. Linux feature detection, UAPI version and exact notification behavior must be pinned before code is written.

## Architecture analysis — hypotheses, not verified behavior

**H1: broker isolation may simplify the host/guest syscall boundary.** A separate supervising process can receive selected Linux syscall-entry events from the guest process and synthesize return values. However, the kernel still interprets the trapped instruction using Linux syscall numbering and argument registers; it does not know NT service identity. A Windows-origin service number colliding with a permitted Linux number can bypass selective filtering unless a robust origin discriminator exists. Conversely, trapping all syscalls risks intercepting the runtime's own essential Linux operations.

**H2: instruction-pointer classification is not an authorization boundary.** Seccomp's instruction pointer is available to filters, but arbitrary native code could jump to permitted syscall sites. SUD has a similar provenance caveat (R90). Any guest code considered untrusted needs a separately justified security boundary, not an address-range convention.

**H3: a listener introduces asynchronous lifecycle complexity.** Notification receipt, task cancellation, listener exit, signal interruption and reply races need explicit state machines. A blocked guest thread cannot be assumed to remain alive or to have unchanged user memory. Any emulated NT service requiring pointer input must establish safe copy-in/copy-out and race semantics independently.

**H4: seccomp notification is unlikely to be a universal fast path.** A supervisor round trip requires cross-process scheduling and communication; direct in-process MS-ABI imports and an in-process SUD path have different cost profiles. This is a qualitative hypothesis only: no comparative latency measurements exist.

**H5: Linux fd injection is not NT handle semantics.** The project needs process-relative NT handles, access masks, object references and lifetime rules (R10/R67/R146). ADDFD may support a narrowly defined host resource transfer, but cannot replace the guest object manager.

## Falsifiable evaluation plan — NOT RUN

1. **Capability gate:** record `uname`, seccomp notification action availability, listener creation, kernel headers, notification structure sizes and ioctl support; SKIP unsupported combinations rather than interpreting failures as architectural proof.
2. **Collision gate:** run two isolated fixtures whose raw syscall numbers collide with real Linux services; verify neither unintentionally reaches the Linux kernel when guest-origin. Independently verify host libc/syscalls continue to work.
3. **Lifecycle gate:** test cancellation, target-thread death, listener death, pending-notification invalidation, signals, concurrent requests and supervisor restarts. Check identity before any response and record all error codes.
4. **Pointer gate:** emulate a service with guest memory arguments, mutate/unmap memory concurrently, and check that no unchecked pointer or stale notification is trusted.
5. **Return gate:** compare raw Linux negative errno conventions with the required Windows NTSTATUS and x64 register state; assert correct guest-visible results for success, unsupported service and cancellation.
6. **Performance gate:** on identical hardware measure direct MS-ABI import, SUD and user-notification dispatch using distributions (including p99), context switches, CPU usage and concurrent scaling. No performance ranking until these measurements exist.
7. **Security gate:** separately specify what constitutes trusted host code, which Linux syscalls guest code can issue, and how arbitrary jumps or filter manipulation are prevented. Do not call a compatibility broker a sandbox.

## Decision

**OPEN — do not adopt seccomp notification as the default NT service transport yet.** First demonstrate correct guest/host provenance under syscall-number collisions and nested transitions. Use notification as an explicitly benchmarked alternative or as a narrowly scoped privileged broker mechanism, not as a replacement for the NT object manager or a presumed containment guarantee.

**Nonduplication:** R90 sets general provenance/security invariants; R164 studies SUD signal-stack lifetime; this note isolates the *distinct seccomp notification listener protocol*, cancellation and identity validation, ADDFD limits, and comparative broker acceptance gates.
