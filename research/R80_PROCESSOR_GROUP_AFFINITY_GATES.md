# R80 — Windows processor groups versus Linux CPU affinity (documentation review)

Date: 2026-11-14. Status: **primary-source documentation verified; no code or Windows/Linux experiments executed**.

## Why this is a separate gate

R77 covers Windows scheduling priorities; it does not establish a translation of Windows *processor-group topology and affinity identity* to Linux CPU masks. An NT-compatible runtime must preserve guest-visible group-relative CPU numbers even when the Linux host has a different set of online or permitted CPUs. Mapping a Windows GROUP_AFFINITY bit mask directly to a Linux cpu_set_t is not generally correct.

## Verified API contracts

1. Microsoft defines each processor group as a static set of **at most 64 logical processors**, with a group number and group-relative processor number. Systems with fewer than 64 logical processors have group 0. See [Microsoft: Processor Groups](https://learn.microsoft.com/en-us/windows/win32/procthread/processor-groups).
2. **Version-dependent behavior matters:** before Windows 11 / Server 2022, the documented default is a process initially constrained to one group; beginning with those releases, default process/thread affinity can span groups, with a *primary group* retained for compatibility with legacy group-unaware APIs. This is not a universal single-group rule. Same Microsoft source.
3. [SetThreadGroupAffinity](https://learn.microsoft.com/en-us/windows/win32/api/processtopologyapi/nf-processtopologyapi-setthreadgroupaffinity) takes a thread handle requiring THREAD_SET_INFORMATION, a GROUP_AFFINITY and optional previous-group output. On Windows 11/Server 2022+, the operation restricts the thread to the specified **single group** and makes that its primary group. The API reports success/failure through BOOL and GetLastError.
4. Linux [sched_setaffinity(2)](https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html) sets a **per-thread host CPU mask**, not a Windows processor group. The effective mask is intersected with physically available CPUs and cpuset restrictions, which may be silently imposed. EINVAL covers masks with no permitted CPUs; EPERM applies to insufficient permission for another thread. A glibc fixed-size cpu_set_t represents CPU numbers only through 1023; CPU_ALLOC is available for larger masks. Linux affinity is inherited over fork and retained over execve.

## Architectural consequence (inference, not demonstrated)

A proposed guest topology service should own: (a) guest group IDs and group-relative logical CPU IDs; (b) a guest-to-host CPU mapping that can handle sparse Linux CPU IDs; (c) guest-visible allowed masks and primary groups; (d) the selected Windows-version profile; and (e) an explicit policy for cpuset/hotplug changes. The host backend may then translate eligible guest CPUs into sched_setaffinity masks, but **must not** treat host affinity success as proof that all guest API semantics were reproduced.

Potentially contradictory demands need a deliberate policy: a guest-visible CPU might become host-disallowed; exposing all host CPUs might change the guest's processor-group count; a Linux process can have threads with distinct affinity without representing a Windows process's primary group. These are unresolved design choices, not experimentally verified bugs.

## Proposed conformance and negative gates (NOT RUN)

- On Windows reference machines with <=64 and >64 logical processors, record group counts, primary group, process and thread affinity before/after explicit SetThreadGroupAffinity, including previous-group output and invalid group/mask failures. Repeat under Windows 10 and Windows 11/Server 2022 where available; do not infer old-version behavior from new-version runs.
- Record legacy GetProcessAffinityMask/SetThreadAffinityMask behavior and group-aware GetThreadGroupAffinity and GetProcessGroupAffinity on the same thread/process; compare group-relative numbering, not just bit counts.
- On Linux, test sparse host CPU numbering, cpuset restrictions, offline CPU changes, and masks exceeding fixed cpu_set_t capacity. Record effective sched_getaffinity after setting, not only syscall return.
- For runtime integration, test thread creation inheritance, concurrent affinity changes, access-denied handles, and restoration after failed calls. Verify guest error values independently of errno.
- Keep tests in isolated fixtures; no performance claim follows from API-contract review.

## Decision gate

Do **not** implement a global CPU-mask alias for GROUP_AFFINITY. First specify the guest OS-version contract and stable virtual CPU/group numbering, then build a reference test matrix and a host mapping policy. Performance/NUMA optimizations are later work and must not change guest-visible topology silently.

## Source provenance

- Microsoft, *Processor Groups*, updated 2026-11-02, accessed 2026-11-14: https://learn.microsoft.com/en-us/windows/win32/procthread/processor-groups
- Microsoft, *SetThreadGroupAffinity*, updated 2022-01-27, accessed 2026-11-14: https://learn.microsoft.com/en-us/windows/win32/api/processtopologyapi/nf-processtopologyapi-setthreadgroupaffinity
- Linux man-pages, *sched_setaffinity(2)*, man-pages 6.19, accessed 2026-11-14: https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html
