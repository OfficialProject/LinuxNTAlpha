# R103 — Windows CPU Sets versus Linux per-thread affinity

Date: 2026-11-15. Status: **VERIFIED FROM PRIMARY DOCUMENTATION; NO EXPERIMENT**.

## Why this is separate from R77

R77 addresses priority and scheduler policy. This note addresses a different compatibility surface: Windows **CPU Sets** (process defaults, thread selections, processor-group relationships) and why Linux `sched_setaffinity` alone is not an equivalent API.

## Verified contracts

1. Microsoft [CPU Sets](https://learn.microsoft.com/en-us/windows/win32/procthread/cpu-sets) defines two assignment levels: process-default CPU Sets apply to threads lacking an explicit thread-selected assignment. A restrictive **affinity mask takes precedence** over conflicting CPU Set assignments. CPU Sets allocated exclusively to another process cannot be used as requested; Windows may schedule the thread elsewhere. Multi-group restrictions also matter.
2. Microsoft [SetProcessDefaultCpuSets](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessdefaultcpusets) says threads created without explicit selections inherit the process default. A NULL array clears the assignment; the process handle requires `PROCESS_SET_LIMITED_INFORMATION`. This is a guest-visible policy object, not simply a one-time update of all existing host threads.
3. Microsoft [SetProcessDefaultCpuSetMasks](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessdefaultcpusetmasks) offers processor-group-mask representation on Windows 11/Server 2022 and later. The specified groups select CPU Sets whose home processors fall in those masks. Do not assume that a Windows CPU Set ID is the same integer as a Linux CPU index.
4. Linux [sched_setaffinity(2)](https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html) sets a **per-thread** eligibility mask. The effective set is constrained by online/present CPUs and cpuset restrictions; the kernel may silently narrow it. The API can return `EINVAL` when no permitted processor remains, `EPERM` for insufficient rights, and `ESRCH` for an absent thread.
5. Linux [pthread_setaffinity_np(3)](https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html) documents that a new pthread inherits a copy of its creator's affinity mask. This differs from Windows' explicit process-default versus thread-selected CPU Set distinction. Linux also supports dynamically sized CPU masks beyond fixed `cpu_set_t` capacity, documented by `sched_setaffinity(2)`.

These are documentation-level facts, **not** proof of specific Windows scheduler internals, throughput or latency.

## Architectural implications — HYPOTHESES

Maintain guest CPU Set IDs, topology/processor-group mappings, per-process default assignments and optional per-thread selections **separately** from the Linux effective affinity mask. Translate to host affinity only after intersecting guest affinity restrictions with the guest CPU Set policy and host availability. The exact Windows fallback when a selected set is unavailable must be determined by reference testing, not guessed.

A Linux thread's inherited host mask is insufficient to implement Windows default-set changes and explicit per-thread overrides. A guest runtime should track the source of a selection so later process-default changes affect the appropriate threads without incorrectly overriding explicit selections. An implementation may need to recompute host masks after CPU hotplug or cpuset cgroup changes.

Treat the guest-visible CPU topology as a coherent virtual view. Do not advertise CPUs to a guest and then silently imply that a Linux affinity operation successfully pinned the thread to them. Host policy and guest policy are distinct; neither grants authority to escape cgroup restrictions.

## Conformance and falsification plan — NOT EXECUTED

1. **Default versus override:** on a Windows reference host, create two threads, explicitly select CPU Sets for one, change process defaults, and query both assignments. Record whether current and newly created threads behave as documented.
2. **Affinity precedence:** choose conflicting affinity masks and CPU Set selections; query selection and observe processor placement separately. Do not confuse a stored assignment with actual execution eligibility.
3. **Invalid IDs and cleared selection:** test nonexistent IDs, empty lists, NULL/zero clearing, duplicate IDs, and inaccessible CPU Sets. Record exact return values and `GetLastError`, not assumed error codes.
4. **Processor groups:** on hardware/VM configurations exposing multiple groups, compare ID-based and group-mask APIs; skip when unavailable rather than extrapolate.
5. **Linux host limits:** in a disposable unprivileged cpuset-controlled environment, attempt permitted and disallowed masks; compare `sched_getaffinity` and actual placement, recording kernel version and effective cpuset. Do not alter production cgroups or host scheduling configuration.
6. **Thread creation and churn:** compare guest policy inheritance with Linux pthread inheritance, then change guest defaults while threads run. Include CPU-hotplug simulation only on a controlled test host.
7. **Performance:** measure migration rate, wake-to-run latency and frame-time tails with and without policy translation, using equivalent workload and hardware. No speedup is currently established.

## Decision gate

**Do not implement CPU Sets as a thin alias for `sched_setaffinity`.** First establish an explicit guest policy model and differential Windows conformance for precedence, inheritance, identity and failure behavior. Reuse Linux affinity as a constrained execution mechanism, not as the complete Windows API state.

Sources inspected 2026-11-15:
- Microsoft CPU Sets: https://learn.microsoft.com/en-us/windows/win32/procthread/cpu-sets
- Microsoft SetProcessDefaultCpuSets: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessdefaultcpusets
- Microsoft SetProcessDefaultCpuSetMasks: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessdefaultcpusetmasks
- Linux man-pages sched_setaffinity(2): https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html
- Linux man-pages pthread_setaffinity_np(3): https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html
